#include "BrowserWindow.hpp"

#include "BrowserScreen.hpp"
#include "Config.hpp"
#include "ResizeGrip.hpp"
#include "ShortcutListener.hpp"
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, SongScope);

using namespace GlobalNamespace;
using namespace UnityEngine;

namespace QuestBrowser::Window {

namespace {

// Fits the layout in BrowserScreen.cpp: 144 x 81 page, toolbar, title, padding.
// At the default scale, about 1.8 m wide. Grips change dimensions, not scale.
constexpr Vector2 kScreenSize{150.0f, 126.0f};

Logic::Context context = Logic::Context::Menu;
std::optional<Logic::PoseSlot> placedFor;
SafePtrUnity<BSML::FloatingScreen> floatingScreen;
SafePtrUnity<BrowserScreen> browserScreen;
std::vector<SafePtrUnity<ResizeGrip>> resizeGrips;

void ApplyDimensions(Resize::Dimensions size, bool save) {
    if (!floatingScreen) return;
    floatingScreen->set_ScreenSize({size.width, size.height});
    if (browserScreen) browserScreen->SetDimensions(size);
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
    screen->set_HandleSide(BSML::Side::Bottom);

    floatingScreen = screen;
    placedFor.reset();
    browserScreen = BrowserScreen::CreateIn(screen->transform);
    for (auto axes : {Vector2(1, -1), Vector2(-1, -1), Vector2(1, 1), Vector2(-1, 1),
                      Vector2(-1, 0), Vector2(1, 0), Vector2(0, 1), Vector2(0, -1)})
        resizeGrips.emplace_back(ResizeGrip::Create(screen->transform, static_cast<int>(axes.x), static_cast<int>(axes.y), ApplyDimensions));
    resizeGrips.emplace_back(ResizeGrip::CreateDepth(screen->transform, []() {
        if (!placedFor || !floatingScreen) return;
        auto t = floatingScreen->transform;
        Config::SetPose(*placedFor, {t->position, t->rotation, t->localScale.x});
    }));
    logger.info("Created browser window");
}

void Apply() {
    auto state = Logic::ScreenStateFor(context, Config::GetVisible(), Config::GetShowInSong());
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
    for (auto& grip : resizeGrips) if (grip) grip->gameObject->SetActive(state.showHandle);
    if (browserScreen) browserScreen->SetInteractive(state.interactive);
}

void SetContext(Logic::Context value) {
    context = value;
    Apply();
}

}  // namespace

void ToggleVisible() {
    Config::SetVisible(!Config::GetVisible());
    Apply();
}

void Refresh() { Apply(); }

Logic::Context CurrentContext() { return context; }

float GetControlScale() { return browserScreen ? browserScreen->GetControlScale() : 1.0f; }
float GetChromeHeight() { return browserScreen ? browserScreen->GetChromeHeight() : Resize::kChromeHeight; }
void FitControls() {
    if (!floatingScreen || !browserScreen) return;
    auto rect = floatingScreen->GetComponent<RectTransform*>()->rect;
    ApplyDimensions(Resize::DimensionsFromDrag({rect.width, rect.height}, 0, 0, 0, 0,
                    Resize::AspectFor(GetAspectPreset()), GetControlScale(), GetChromeHeight()), false);
}

void BringHere() {
    auto camera = Camera::get_main();
    if (!camera) return;
    Config::SetVisible(true);
    Apply();
    if (!floatingScreen || !placedFor) return;
    auto head = camera->transform;
    auto forward = head->forward;
    auto position = head->position;
    Config::Pose pose{{position.x + forward.x * 2.5f, position.y + forward.y * 2.5f,
                       position.z + forward.z * 2.5f},
                      Quaternion::LookRotation(forward, head->up), Config::kDefaultScale};
    floatingScreen->transform->SetPositionAndRotation(pose.position, pose.rotation);
    floatingScreen->transform->localScale = {pose.scale, pose.scale, pose.scale};
    Config::SetPose(*placedFor, pose);
    Config::SetAspectPreset(*placedFor, 0);
    if (browserScreen) browserScreen->RefreshForPose();
    ApplyDimensions({}, false);
    FitControls();
    auto rect = floatingScreen->GetComponent<RectTransform*>()->rect;
    Config::SetDimensions(*placedFor, {rect.width, rect.height});
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
    INSTALL_HOOK(logger, MainMenuViewController_DidActivate);
    INSTALL_HOOK(logger, PauseController_Start);
    INSTALL_HOOK(logger, PauseController_Pause);
    INSTALL_HOOK(logger, PauseController_HandlePauseMenuManagerDidPressContinueButton);
}

}  // namespace QuestBrowser::Window

void QuestBrowser::SongScope::OnDestroy() { Window::OnSongEnded(); }
