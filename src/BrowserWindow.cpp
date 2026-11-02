#include "BrowserWindow.hpp"

#include "BrowserScreen.hpp"
#include "Config.hpp"
#include "ResizeGrip.hpp"
#include "ShortcutListener.hpp"
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "HMUI/CurvedCanvasSettings.hpp"
#include "UnityEngine/BoxCollider.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshCollider.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/EventSystems/PointerEventData.hpp"
#include "UnityEngine/EventSystems/RaycastResult.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"
#include "VRUIControls/VRInputModule.hpp"
#include "UnityEngine/Transform.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreenHandle.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Bounds.hpp"
#include "UnityEngine/FindObjectsSortMode.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "System/Type.hpp"
#include "placement_logic.hpp"
#include "view_logic.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, SongScope);

using namespace GlobalNamespace;
using namespace UnityEngine;

namespace QuestBrowser::Window {

namespace {

// Fits the layout in BrowserScreen.cpp: 144 x 81 page, toolbar, title, padding.
// At the default scale, about 1.8 m wide. Grips change dimensions, not scale.
constexpr Vector2 kScreenSize{150.0f, 87.0f};

Logic::Context context = Logic::Context::Menu;
std::optional<Logic::PoseSlot> placedFor;
SafePtrUnity<BSML::FloatingScreen> floatingScreen;
SafePtrUnity<BrowserScreen> browserScreen;
std::vector<SafePtrUnity<ResizeGrip>> resizeGrips;
SafePtrUnity<GameObject> shapedHandle;
SafePtrUnity<Mesh> flatHandleMesh;
SafePtrUnity<Mesh> curvedHandleMesh;
float handleRadius = -1.0f, handleWidth = -1.0f, handleHeight = -1.0f;
// Scenery hidden because it was between the player and the window (this song only).
std::vector<SafePtrUnity<Renderer>> hiddenRenderers;
// The controller that holds the move handle, and its thumbstick turn for this grab.
GlobalNamespace::VRController* steeredController = nullptr;
float handleTurn = 0.0f;

View::Vec3 ToVec(Vector3 v) { return {v.x, v.y, v.z}; }
Placement::Point ToVecP(Vector3 v) { return {v.x, v.y, v.z}; }

void RestoreView() {
    for (auto& renderer : hiddenRenderers)
        if (renderer) renderer->enabled = true;
    hiddenRenderers.clear();
}

// Hide the environment renderers whose bounds are in the line of sight from the
// head to the window. Objects in other scenes (notes, sabers, HUD, this window) stay.
void ClearView() {
    RestoreView();
    if (!Config::GetHideBlockers() || !floatingScreen || !floatingScreen->gameObject->activeSelf) return;
    auto camera = Camera::get_main();
    if (!camera) return;
    auto eye = ToVec(camera->transform->position);
    auto t = floatingScreen->transform;
    auto size = floatingScreen->get_ScreenSize();
    float scale = t->lossyScale.x;
    auto right = Vector3::op_Multiply(t->right, size.x / 2.0f * scale);
    auto up = Vector3::op_Multiply(t->up, size.y / 2.0f * scale);
    auto window = View::WindowSamples(ToVec(t->position), ToVec(right), ToVec(up), 7);

    auto type = reinterpret_cast<System::Type*>(il2cpp_utils::GetSystemType(classof(Renderer*)));
    auto objects = Object::FindObjectsByType(type, FindObjectsSortMode::None);
    std::string names;
    for (auto object : objects) {
        auto renderer = reinterpret_cast<Renderer*>(object.ptr());
        if (!renderer || !renderer->enabled) continue;
        auto scene = renderer->gameObject->scene;
        std::string sceneName = static_cast<std::string>(scene.name);
        if (sceneName.find("Environment") == std::string::npos) continue;
        auto bounds = renderer->bounds;
        if (!View::BlocksView(eye, window, ToVec(bounds.min), ToVec(bounds.max))) continue;
        renderer->enabled = false;
        hiddenRenderers.emplace_back(renderer);
        if (hiddenRenderers.size() <= 12) names += static_cast<std::string>(renderer->gameObject->name) + ", ";
    }
    logger.info("Hid {} scenery objects in front of the browser: {}", hiddenRenderers.size(), names);
}

// The stock handle is a cube with a box collider. Bend its mesh and collider
// together so grabbing works across the full visible arc.
void ShapeHandle(GameObject* handle, Vector2 size) {
    auto settings = floatingScreen->GetComponent<HMUI::CurvedCanvasSettings*>();
    auto filter = handle->GetComponent<MeshFilter*>();
    auto box = handle->GetComponent<BoxCollider*>();
    if (!settings || !filter || !box) return;
    if (!shapedHandle || shapedHandle.ptr() != handle) {
        shapedHandle = handle;
        flatHandleMesh = filter->sharedMesh.ptr();
        curvedHandleMesh = nullptr;
        handleRadius = -1.0f;
    }
    float radius = settings->get_radius();
    if (handleRadius == radius && handleWidth == size.x && handleHeight == size.y) return;
    handleRadius = radius;
    handleWidth = size.x;
    handleHeight = size.y;
    auto collider = handle->GetComponent<MeshCollider*>();
    if (radius <= 0.0f) {
        if (flatHandleMesh) filter->sharedMesh = flatHandleMesh.ptr();
        if (collider) collider->enabled = false;
        box->enabled = true;
        return;
    }
    constexpr int segments = 32;
    ArrayW<Vector3> vertices(static_cast<il2cpp_array_size_t>((segments + 1) * 4));
    ArrayW<int32_t> triangles(static_cast<il2cpp_array_size_t>(segments * 24 + 12));
    auto handleSize = Resize::HandleSizeFor({size.x, size.y});
    float width = handleSize.width;
    float thickness = handleSize.thickness;
    float y = size.y / 2.0f + thickness / 2.0f + 1.0f;
    for (int i = 0; i <= segments; ++i) {
        float x = width * (static_cast<float>(i) / segments - 0.5f);
        auto point = settings->TransformPointFromCanvasTo3D({x, y});
        float px = point.x / width;
        float pz = point.z / thickness;
        vertices[i * 4] = {px, -0.5f, pz - 0.5f};
        vertices[i * 4 + 1] = {px, 0.5f, pz - 0.5f};
        vertices[i * 4 + 2] = {px, -0.5f, pz + 0.5f};
        vertices[i * 4 + 3] = {px, 0.5f, pz + 0.5f};
    }
    int n = 0;
    auto quad = [&](int a, int b, int c, int d) {
        triangles[n++] = a; triangles[n++] = b; triangles[n++] = c;
        triangles[n++] = a; triangles[n++] = c; triangles[n++] = d;
    };
    for (int i = 0; i < segments; ++i) {
        int a = i * 4, b = a + 4;
        quad(a, a + 1, b + 1, b);
        quad(a + 2, b + 2, b + 3, a + 3);
        quad(a + 1, a + 3, b + 3, b + 1);
        quad(a, b, b + 2, a + 2);
    }
    quad(0, 2, 3, 1);
    int end = segments * 4;
    quad(end, end + 1, end + 3, end + 2);
    if (!curvedHandleMesh) curvedHandleMesh = Mesh::New_ctor();
    auto mesh = curvedHandleMesh.ptr();
    if (collider) collider->sharedMesh = nullptr;
    mesh->Clear();
    mesh->vertices = vertices;
    mesh->triangles = triangles;
    mesh->RecalculateNormals();
    mesh->RecalculateBounds();
    filter->sharedMesh = mesh;
    if (!collider) collider = handle->AddComponent<MeshCollider*>();
    collider->sharedMesh = mesh;
    collider->enabled = true;
    box->enabled = false;
}

// The move handle sits above the window: at the bottom it sank into the floor
// and could not be reached. BSML puts a top handle across the top edge, where it
// would cover the toolbar, so raise it just above the edge. BSML sets its own,
// larger handle scale when the size changes; replace it here.
void PlaceHandle() {
    if (!floatingScreen) return;
    auto handle = floatingScreen->handle;
    if (!handle || !handle->m_CachedPtr.m_value) return;
    auto size = floatingScreen->get_ScreenSize();
    auto handleSize = Resize::HandleSizeFor({size.x, size.y});
    float thickness = handleSize.thickness;
    handle->transform->localScale = Vector3(handleSize.width, thickness, thickness);
    handle->transform->localPosition = Vector3(0.0f, size.y / 2.0f + thickness / 2.0f + 1.0f, 0.0f);
    ShapeHandle(handle, size);
}

void ApplyDimensions(Resize::Dimensions size, bool save) {
    if (!floatingScreen) return;
    floatingScreen->set_ScreenSize({size.width, size.height});
    PlaceHandle();
    if (browserScreen) browserScreen->SetDimensions(size);
    PlaceHandle();
    if (save && placedFor) Config::SetDimensions(*placedFor, size);
}

void OnHandleReleased(BSML::FloatingScreen*, BSML::FloatingScreenHandleEventArgs const& args) {
    if (!placedFor) return;
    float scale = floatingScreen ? floatingScreen->transform->localScale.x : Config::kDefaultScale;
    Config::SetPose(*placedFor, {args.position, args.rotation, scale});
    logger.info("Saved browser position for {}", *placedFor == Logic::PoseSlot::Menu ? "menu" : "songs");
}

void EnsureCreated() {
    if (floatingScreen) return;
    auto screen = BSML::FloatingScreen::CreateFloatingScreen(kScreenSize, true, {}, Quaternion::get_identity(), 0.0f, true);
    screen->name = "QuestBrowserWindow";
    screen->transform->localScale = Vector3(Config::kDefaultScale, Config::kDefaultScale, Config::kDefaultScale);
    // Keep the window, and so the page, when the game changes scenes.
    Object::DontDestroyOnLoad(screen->gameObject);
    screen->HandleReleased.addCallback(&OnHandleReleased);
    screen->set_HandleSide(BSML::Side::Top);

    floatingScreen = screen;
    placedFor.reset();
    browserScreen = BrowserScreen::CreateIn(screen->transform);
    for (auto axes : {Vector2(1, -1), Vector2(-1, -1), Vector2(1, 1), Vector2(-1, 1),
                      Vector2(-1, 0), Vector2(1, 0), Vector2(0, 1), Vector2(0, -1)})
        resizeGrips.emplace_back(ResizeGrip::Create(screen->transform, static_cast<int>(axes.x), static_cast<int>(axes.y), ApplyDimensions));
    logger.info("Created browser window");
}

void Apply() {
    auto state = Logic::ScreenStateFor(context, Config::GetEnabled(), Config::GetShowInSong());
    if (!state.visible) {
        if (floatingScreen) floatingScreen->gameObject->SetActive(false);
        return;
    }
    EnsureCreated();
    if (placedFor != state.pose) {
        auto pose = Config::GetPose(state.pose);
#ifdef QB_DEBUG_SHOW_KEYBOARD
        // Debug builds: put the window 2.5 m in front of the headset, so a screenshot shows it.
        if (auto cam = Camera::get_main()) {
            auto t = cam->transform;
            auto fwd = t->forward;
            fwd.y = 0.0f;
            auto dir = Vector3::Normalize(fwd);
            auto p = t->position;
            pose.position = Vector3(p.x + dir.x * 2.5f, p.y + 0.4f, p.z + dir.z * 2.5f);
            pose.rotation = Quaternion::LookRotation(fwd, Vector3::get_up());
        }
#endif
        floatingScreen->transform->SetPositionAndRotation(pose.position, pose.rotation);
        floatingScreen->transform->localScale = Vector3(pose.scale, pose.scale, pose.scale);
        placedFor = state.pose;
        ApplyDimensions(Config::GetDimensions(state.pose), false);
    }
    floatingScreen->gameObject->SetActive(true);
    floatingScreen->set_ShowHandle(state.showHandle);
    PlaceHandle();
    for (auto& grip : resizeGrips) if (grip) grip->gameObject->SetActive(state.showHandle);
    if (browserScreen) {
        browserScreen->SetCurved(Config::GetCurved());
        PlaceHandle();
        // While a song plays only the page shows; the toolbar comes back on pause.
        browserScreen->SetChromeHidden(!state.interactive);
        browserScreen->SetInteractive(state.interactive);
        // The menu and songs each have their own opacity.
        browserScreen->ApplyOpacity();
    }
}

void SetContext(Logic::Context value) {
    context = value;
    Apply();
    // Each time the song plays (start, resume after a move), check the view again.
    if (value == Logic::Context::SongPlaying) ClearView();
    // The song scene is gone: nothing to restore.
    if (value == Logic::Context::Menu) hiddenRenderers.clear();
}

}  // namespace

void Refresh() { Apply(); }

void BoostHandleDepth(Component* screenComponent) {
    if (!screenComponent) return;
    auto screen = screenComponent->GetComponent<BSML::FloatingScreen*>();
    if (!screen || !screen->handle || !screen->handle->m_CachedPtr.m_value) return;
    auto handle = screen->handle->GetComponent<BSML::FloatingScreenHandle*>();
    if (!handle || !handle->_grabbingController) return;
    auto grab = handle->_grabPos;
    float stick = handle->_grabbingController->thumbstick.y;
    grab.z = Resize::HandleDepthStep(grab.z, stick, Time::get_unscaledDeltaTime());
    handle->_grabPos = grab;
}

Quaternion FacingHead(Vector3 position, float turnDegrees, Quaternion fallback) {
    auto camera = Camera::get_main();
    if (!camera) return fallback;
    auto a = Placement::FacingAngles(ToVecP(camera->transform->position), ToVecP(position));
    return Quaternion::Euler(a.pitch, a.yaw + turnDegrees, 0.0f);
}

// BSML moves the held window to the controller rotation times _grabRot (smoothly).
// Set _grabRot so that the result faces the head.
void SteerHandle(Component* screenComponent) {
    if (!screenComponent || !floatingScreen || screenComponent->gameObject != floatingScreen->gameObject) return;
    if (!floatingScreen->handle || !floatingScreen->handle->m_CachedPtr.m_value) return;
    auto handle = floatingScreen->handle->GetComponent<BSML::FloatingScreenHandle*>();
    if (!handle) return;
    auto controller = handle->_grabbingController;
    if (!controller) {
        steeredController = nullptr;
        return;
    }
    if (steeredController != controller.ptr()) {
        steeredController = controller.ptr();
        handleTurn = 0.0f;
    }
    handleTurn = Controls::TurnStep(handleTurn, controller->thumbstick.x, Time::get_unscaledDeltaTime());
    auto anchor = controller->_viewAnchorTransform;
    if (!anchor) return;
    auto target = anchor->TransformPoint(handle->_grabPos);
    auto facing = FacingHead(target, handleTurn, floatingScreen->transform->rotation);
    handle->_grabRot = Quaternion::op_Multiply(Quaternion::Inverse(anchor->rotation), facing);
}

Logic::Context CurrentContext() { return context; }

float GetControlScale() { return browserScreen ? browserScreen->GetControlScale() : 1.0f; }
float GetChromeHeight() { return browserScreen ? browserScreen->GetChromeHeight() : Resize::kChromeHeight; }
void FitControls() {
    if (!floatingScreen || !browserScreen) return;
    auto rect = floatingScreen->GetComponent<RectTransform*>()->rect;
    ApplyDimensions(Resize::DimensionsFromDrag({rect.width, rect.height}, 0, 0, 0, 0,
                    Resize::AspectFor(GetAspectPreset()), GetControlScale(), GetChromeHeight()), false);
}

int GetAspectPreset() { return placedFor ? Config::GetAspectPreset(*placedFor) : 0; }
void CycleAspectPreset() {
    if (!placedFor) return;
    int preset = (GetAspectPreset() + 1) % 4;
    Config::SetAspectPreset(*placedFor, preset);
    auto size = Config::GetDimensions(*placedFor);
    ApplyDimensions(Resize::DimensionsFromDrag(size, 0, 0, 1, 0, Resize::AspectFor(preset), GetControlScale(), GetChromeHeight()), true);
}

void OnSongEnded() { SetContext(Logic::Context::Menu); }

void ResetPosition(Logic::PoseSlot slot) {
    if (placedFor == slot) {
        if (browserScreen) browserScreen->CancelInteraction();
        if (floatingScreen && floatingScreen->handle) {
            if (auto handle = floatingScreen->handle->GetComponent<BSML::FloatingScreenHandle*>())
                handle->_grabbingController = nullptr;
        }
    }
    Config::ResetPose(slot);
    // Place the window again now if it shows this slot.
    if (placedFor == slot) placedFor.reset();
    Apply();
    if (placedFor == slot && browserScreen) {
        browserScreen->RefreshForPose();
        FitControls();
    }
    logger.info("Reset browser position for {}", slot == Logic::PoseSlot::Menu ? "menu" : "songs");
}

bool IsBrowserTarget(GameObject* target) {
    return target && browserScreen && browserScreen->ContainsTarget(target->transform);
}

// While paused, allow the browser's own canvases to be selected through game
// scene colliders. Do not change the game or other mods' raycasters.
MAKE_HOOK_MATCH(VRGraphicRaycaster_Raycast, &VRUIControls::VRGraphicRaycaster::Raycast, void,
                VRUIControls::VRGraphicRaycaster* self, UnityEngine::EventSystems::PointerEventData* eventData,
                System::Collections::Generic::List_1<UnityEngine::EventSystems::RaycastResult>* results) {
    if (context != Logic::Context::SongPaused || !IsBrowserTarget(self->gameObject)) {
        VRGraphicRaycaster_Raycast(self, eventData, results);
        return;
    }
    auto mask = self->_blockingMask;
    self->_blockingMask = UnityEngine::LayerMask(0);
    VRGraphicRaycaster_Raycast(self, eventData, results);
    self->_blockingMask = mask;
}

// Competing game UI can otherwise win by being nearer than the browser.
MAKE_HOOK_MATCH(VRInputModule_RaycastComparer, &VRUIControls::VRInputModule::RaycastComparer, int,
                UnityEngine::EventSystems::RaycastResult lhs, UnityEngine::EventSystems::RaycastResult rhs) {
    if (context == Logic::Context::SongPaused) {
        bool left = IsBrowserTarget(lhs.get_gameObject());
        bool right = IsBrowserTarget(rhs.get_gameObject());
        int priority = Logic::BrowserRaycastPriority(context, left, right);
        if (priority != 0) return priority;
    }
    return VRInputModule_RaycastComparer(lhs, rhs);
}

void SavePosition() {
    if (!placedFor || !floatingScreen) return;
    auto t = floatingScreen->transform;
    Config::SetPose(*placedFor, {t->position, t->rotation, t->localScale.x});
    logger.info("Saved browser position for {}", *placedFor == Logic::PoseSlot::Menu ? "menu" : "songs");
}

void ResetCurrentPosition() {
    ResetPosition(CurrentSlot());
}

void ResetPositions() {
    Config::ResetPoses();
    placedFor.reset();
    Apply();
}

MAKE_HOOK_MATCH(MainMenuViewController_DidActivate, &MainMenuViewController::DidActivate, void, MainMenuViewController* self,
                bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    ShortcutListener::EnsureCreated();
    SetContext(Logic::Context::Menu);
}

MAKE_HOOK_MATCH(PauseController_Start, &PauseController::Start, void, PauseController* self) {
    PauseController_Start(self);
    self->gameObject->AddComponent<SongScope*>();
    // A level can start paused (for example after the game lost focus).
    bool paused = self->_paused == PauseController::PauseState::Paused;
    SetContext(paused ? Logic::Context::SongPaused : Logic::Context::SongPlaying);
}

MAKE_HOOK_MATCH(PauseController_Pause, &PauseController::Pause, void, PauseController* self) {
    PauseController_Pause(self);
    if (self->_paused == PauseController::PauseState::Paused) SetContext(Logic::Context::SongPaused);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidPressContinueButton,
                &PauseController::HandlePauseMenuManagerDidPressContinueButton, void, PauseController* self) {
    // Lock the window before the resume countdown, so a click can not reach the page during play.
    SetContext(Logic::Context::SongPlaying);
    PauseController_HandlePauseMenuManagerDidPressContinueButton(self);
}

void InstallHooks() {
    INSTALL_HOOK(logger, VRGraphicRaycaster_Raycast);
    INSTALL_HOOK(logger, VRInputModule_RaycastComparer);
    INSTALL_HOOK(logger, MainMenuViewController_DidActivate);
    INSTALL_HOOK(logger, PauseController_Start);
    INSTALL_HOOK(logger, PauseController_Pause);
    INSTALL_HOOK(logger, PauseController_HandlePauseMenuManagerDidPressContinueButton);
}

}  // namespace QuestBrowser::Window

void QuestBrowser::SongScope::OnDestroy() { Window::OnSongEnded(); }
