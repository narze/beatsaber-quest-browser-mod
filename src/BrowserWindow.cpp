#include "BrowserWindow.hpp"

#include "BrowserScreen.hpp"
#include "Config.hpp"
#include "ResizeGrip.hpp"
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
// At the default scale, about 1.8 m x 1.3 m; the corner grip changes the scale.
constexpr Vector2 kScreenSize{150.0f, 106.0f};

Logic::Context context = Logic::Context::Menu;
std::optional<Logic::PoseSlot> placedFor;
SafePtrUnity<BSML::FloatingScreen> floatingScreen;
SafePtrUnity<BrowserScreen> browserScreen;
SafePtrUnity<ResizeGrip> resizeGrip;

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
    resizeGrip = ResizeGrip::Create(screen->transform, [](float scale) {
        if (!placedFor) return;
        auto pose = Config::GetPose(*placedFor);
        pose.scale = scale;
        Config::SetPose(*placedFor, pose);
    });
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
    }
    floatingScreen->gameObject->SetActive(true);
    floatingScreen->set_ShowHandle(state.showHandle);
    if (resizeGrip) resizeGrip->gameObject->SetActive(state.showHandle);
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

void OnSongEnded() { SetContext(Logic::Context::Menu); }

void ResetPositions() {
    Config::ResetPoses();
    placedFor.reset();
    Apply();
}

MAKE_HOOK_MATCH(MainMenuViewController_DidActivate, &MainMenuViewController::DidActivate, void, MainMenuViewController* self,
                bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
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
