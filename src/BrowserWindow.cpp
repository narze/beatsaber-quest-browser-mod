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
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/FindObjectsSortMode.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "System/Type.hpp"
#include "placement_logic.hpp"
#include "view_logic.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "bsml/shared/BSML-Lite.hpp"
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
// Whole objects are hidden: the game turns light renderers back on by itself.
std::vector<SafePtrUnity<GameObject>> hiddenObjects;
// Scenery renderers to check again while the song plays (lasers move and turn).
std::vector<SafePtrUnity<Renderer>> viewCandidates;
float viewTimer = 0.0f;
constexpr float kViewCheckSeconds = 0.25f;
// The current hold of the window (grip or move handle).
Controls::HoldSteer holdSteer;
GlobalNamespace::VRController* holder = nullptr;
bool handleHolding = false;
bool leftTriggerWasDown = false, rightTriggerWasDown = false, stickWasDown = false;
std::optional<Resize::Dimensions> heldSize;  // changed in resize mode during this hold
SafePtrUnity<BSML::FloatingScreen> holdLabel;
SafePtrUnity<TMPro::TextMeshProUGUI> holdText;

View::Vec3 ToVec(Vector3 v) { return {v.x, v.y, v.z}; }
Placement::Point ToVecP(Vector3 v) { return {v.x, v.y, v.z}; }

void RestoreView() {
    for (auto& object : hiddenObjects)
        if (object) object->SetActive(true);
    hiddenObjects.clear();
    viewCandidates.clear();
}

System::Type* TypeOf(Il2CppClass* klass) { return reinterpret_cast<System::Type*>(il2cpp_utils::GetSystemType(klass)); }

// Hide the candidates whose bounds are in the line of sight from the head to the
// window. Returns the names of the newly hidden objects.
std::string CheckView() {
    std::string names;
    if (!floatingScreen) return names;
    auto camera = Camera::get_main();
    if (!camera) return names;
    auto eye = ToVec(camera->transform->position);
    auto t = floatingScreen->transform;
    auto size = floatingScreen->get_ScreenSize();
    float scale = t->lossyScale.x;
    auto right = Vector3::op_Multiply(t->right, size.x / 2.0f * scale);
    auto up = Vector3::op_Multiply(t->up, size.y / 2.0f * scale);
    auto window = View::WindowSamples(ToVec(t->position), ToVec(right), ToVec(up), 7);
    for (auto& candidate : viewCandidates) {
        if (!candidate) continue;
        auto object = candidate->gameObject;
        if (!object->activeInHierarchy) continue;
        auto bounds = candidate->bounds;
        if (!View::BlocksView(eye, window, ToVec(bounds.min), ToVec(bounds.max))) continue;
        object->SetActive(false);
        hiddenObjects.emplace_back(object.ptr());
        candidate = nullptr;
        names += static_cast<std::string>(object->name) + ", ";
    }
    if (!names.empty()) {
        std::vector<SafePtrUnity<Renderer>> kept;
        for (auto& c : viewCandidates)
            if (c) kept.emplace_back(c.ptr());
        viewCandidates.swap(kept);
    }
    return names;
}

// When the song plays (start, or resume after a move): show what was hidden, find the
// scenery renderers, and hide those in front of the window. Renderers inside a canvas
// (the HUD) and in other scenes (notes, sabers, this window) stay.
void ClearView() {
    RestoreView();
    if (!Config::GetHideBlockers() || !floatingScreen || !floatingScreen->gameObject->activeSelf) return;
    auto canvasType = TypeOf(classof(Canvas*));
    for (auto object : Object::FindObjectsByType(TypeOf(classof(Renderer*)), FindObjectsSortMode::None)) {
        auto renderer = reinterpret_cast<Renderer*>(object.ptr());
        if (!renderer) continue;
        auto scene = renderer->gameObject->scene;
        if (static_cast<std::string>(scene.name).find("Environment") == std::string::npos) continue;
        if (renderer->GetComponentInParent(canvasType, true)) continue;
        viewCandidates.emplace_back(renderer);
    }
    viewTimer = kViewCheckSeconds;
    auto names = CheckView();
    logger.info("Scenery check: {} objects, hid {}: {}", viewCandidates.size() + hiddenObjects.size(), hiddenObjects.size(), names);
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
    int order = Logic::SortingOrderFor(context);
    if (auto canvas = floatingScreen->GetComponent<Canvas*>()) canvas->sortingOrder = order;
    if (browserScreen) browserScreen->SetSortingOrder(order);
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
    if (value == Logic::Context::Menu) {
        hiddenObjects.clear();
        viewCandidates.clear();
    }
}

}  // namespace

void Refresh() { Apply(); }

void BoostHandleDepth(Component* screenComponent) {
    if (!screenComponent) return;
    auto screen = screenComponent->GetComponent<BSML::FloatingScreen*>();
    if (!screen || !screen->handle || !screen->handle->m_CachedPtr.m_value) return;
    // The browser window: SteerHandle does this, by hold mode.
    if (floatingScreen && screen == floatingScreen.ptr()) return;
    auto handle = screen->handle->GetComponent<BSML::FloatingScreenHandle*>();
    if (!handle || !handle->_grabbingController) return;
    auto grab = handle->_grabPos;
    float stick = handle->_grabbingController->thumbstick.y;
    grab.z = Resize::HandleDepthStep(grab.z, stick, Time::get_unscaledDeltaTime());
    handle->_grabPos = grab;
}

namespace {

Quaternion FacingHead(Vector3 position, float turnDegrees, float tiltDegrees, Quaternion fallback) {
    auto camera = Camera::get_main();
    if (!camera) return fallback;
    auto a = Placement::FacingAngles(ToVecP(camera->transform->position), ToVecP(position));
    return Quaternion::Euler(a.pitch + tiltDegrees, a.yaw + turnDegrees, 0.0f);
}

bool TriggerDown(bool left) {
    using GlobalNamespace::OVRInput;
    return OVRInput::Get(left ? OVRInput::RawButton::LIndexTrigger : OVRInput::RawButton::RIndexTrigger, OVRInput::Controller::Touch);
}

bool StickDown(GlobalNamespace::VRController* controller) {
    using GlobalNamespace::OVRInput;
    bool left = controller->node == UnityEngine::XR::XRNode::LeftHand;
    return OVRInput::Get(left ? OVRInput::RawButton::LThumbstick : OVRInput::RawButton::RThumbstick, OVRInput::Controller::Touch);
}

// A small label above the holding hand: the mode and what the buttons do.
void ShowHoldLabel(GlobalNamespace::VRController* controller) {
    if (!holdLabel) {
        auto screen = BSML::FloatingScreen::CreateFloatingScreen({100.0f, 18.0f}, false, {}, Quaternion::get_identity(), 0.0f, false);
        screen->name = "QuestBrowserHoldLabel";
        if (auto canvas = screen->GetComponent<Canvas*>()) canvas->sortingOrder = Logic::SortingOrderFor(Logic::Context::Menu) + 2;
        Object::DontDestroyOnLoad(screen->gameObject);
        auto text = BSML::Lite::CreateText(screen->transform, "", TMPro::FontStyles::Normal, 5.0f, {0.0f, 0.0f}, {100.0f, 18.0f});
        text->alignment = TMPro::TextAlignmentOptions::Center;
        holdText = text;
        holdLabel = screen;
    }
    holdLabel->gameObject->SetActive(true);
    if (holdText)
        holdText->text = fmt::format("{}\n<size=70%>Trigger: next mode    Stick click: face me</size>",
                                     Controls::HoldModeLabel(holdSteer.Mode()));
    auto t = holdLabel->transform;
    t->localScale = Vector3(0.0025f, 0.0025f, 0.0025f);
    auto position = Vector3::op_Addition(controller->transform->position, Vector3(0.0f, 0.12f, 0.0f));
    t->SetPositionAndRotation(position, FacingHead(position, 0.0f, 0.0f, t->rotation));
}

}  // namespace

void BeginHold(GlobalNamespace::VRController* controller) {
    if (!controller || !floatingScreen) return;
    holder = controller;
    float turn = 0.0f, tilt = 0.0f;
    if (auto camera = Camera::get_main()) {
        auto t = floatingScreen->transform;
        auto facing = Placement::FacingAngles(ToVecP(camera->transform->position), ToVecP(t->position));
        auto euler = t->rotation.eulerAngles;
        turn = Placement::WrapDegrees(euler.y - facing.yaw);
        tilt = Placement::WrapDegrees(euler.x - facing.pitch);
    }
    holdSteer.Begin(turn, tilt);
    // Buttons already down when the hold starts (the trigger on the handle) do not count.
    leftTriggerWasDown = TriggerDown(true);
    rightTriggerWasDown = TriggerDown(false);
    stickWasDown = StickDown(controller);
    heldSize.reset();
    ShowHoldLabel(controller);
}

float UpdateHold(GlobalNamespace::VRController* controller) {
    if (!controller || !floatingScreen) return 0.0f;
    if (controller != holder) BeginHold(controller);
    bool left = TriggerDown(true), right = TriggerDown(false), stick = StickDown(controller);
    bool triggerPressed = (left && !leftTriggerWasDown) || (right && !rightTriggerWasDown);
    bool stickClicked = stick && !stickWasDown;
    leftTriggerWasDown = left;
    rightTriggerWasDown = right;
    stickWasDown = stick;
    auto stickAxes = controller->thumbstick;
    auto mode = holdSteer.Mode();
    auto out = holdSteer.Update({stickAxes.x, stickAxes.y, triggerPressed, stickClicked, Time::get_unscaledDeltaTime()});
    if (holdSteer.Mode() != mode) logger.info("Hold mode: {}", Controls::HoldModeLabel(holdSteer.Mode()));
    if (out.widthFactor != 1.0f || out.heightFactor != 1.0f) {
        auto size = floatingScreen->get_ScreenSize();
        // Each edge moves by half the change, so the window stays centered.
        float dx = size.x * (out.widthFactor - 1.0f) / 2.0f, dy = size.y * (out.heightFactor - 1.0f) / 2.0f;
        int axisX = out.widthFactor != 1.0f ? 1 : 0, axisY = out.heightFactor != 1.0f ? 1 : 0;
        auto next = Resize::DimensionsFromDrag({size.x, size.y}, dx, dy, axisX, axisY,
                                               Resize::AspectFor(GetAspectPreset()), GetControlScale(), GetChromeHeight());
        ApplyDimensions(next, false);
        heldSize = next;
    }
    ShowHoldLabel(controller);
    return out.depthStick;
}

Quaternion HoldRotation(Vector3 position, Quaternion fallback) {
    return FacingHead(position, holdSteer.Turn(), holdSteer.Tilt(), fallback);
}

void EndHold() {
    if (heldSize && placedFor) Config::SetDimensions(*placedFor, *heldSize);
    heldSize.reset();
    holder = nullptr;
    if (holdLabel) holdLabel->gameObject->SetActive(false);
}

bool IsHolding() { return holder != nullptr; }

void UpdateClearView() {
    if (context != Logic::Context::SongPlaying || viewCandidates.empty()) return;
    viewTimer -= Time::get_unscaledDeltaTime();
    if (viewTimer > 0.0f) return;
    viewTimer = kViewCheckSeconds;
    auto names = CheckView();
    if (!names.empty()) logger.info("Scenery check: hid {}", names);
}

// BSML moves the held window to the controller rotation times _grabRot (smoothly),
// and pushes and pulls it with stick Y at 1 m/s.
void SteerHandle(Component* screenComponent) {
    if (!screenComponent || !floatingScreen || screenComponent->gameObject != floatingScreen->gameObject) return;
    if (!floatingScreen->handle || !floatingScreen->handle->m_CachedPtr.m_value) return;
    auto handle = floatingScreen->handle->GetComponent<BSML::FloatingScreenHandle*>();
    if (!handle) return;
    auto controller = handle->_grabbingController;
    if (!controller) {
        if (handleHolding) {
            handleHolding = false;
            EndHold();
        }
        return;
    }
    if (!handleHolding) {
        handleHolding = true;
        BeginHold(controller.ptr());
    }
    UpdateHold(controller.ptr());
    float dt = Time::get_unscaledDeltaTime();
    float stickY = controller->thumbstick.y;
    auto grab = handle->_grabPos;
    // Zoom mode: faster push and pull. Other modes: undo the BSML push and pull.
    if (holdSteer.Mode() == Controls::HoldMode::Zoom) grab.z = Resize::HandleDepthStep(grab.z, stickY, dt);
    else grab.z -= stickY * dt;
    handle->_grabPos = grab;
    auto anchor = controller->_viewAnchorTransform;
    if (!anchor) return;
    auto target = anchor->TransformPoint(grab);
    handle->_grabRot = Quaternion::op_Multiply(Quaternion::Inverse(anchor->rotation),
                                               HoldRotation(target, floatingScreen->transform->rotation));
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
