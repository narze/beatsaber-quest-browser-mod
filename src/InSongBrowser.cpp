#include "InSongBrowser.hpp"

#include "BrowserScreen.hpp"
#include "Config.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Transform.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "main.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

namespace QuestBrowser::InSong {

namespace {

// Same layout size as the menu screen. Scaled to about 1.8 m x 1.2 m.
constexpr Vector2 kScreenSize{150.0f, 100.0f};
constexpr float kScale = 0.012f;

SafePtrUnity<BSML::FloatingScreen> floatingScreen;
SafePtrUnity<BrowserScreen> browserScreen;

void OnHandleReleased(BSML::FloatingScreen*, BSML::FloatingScreenHandleEventArgs const& args) {
    Config::SetInSongPose({args.position, args.rotation});
    logger.info("Saved in-song browser position");
}

void Create(PauseController* pauseController) {
    auto pose = Config::GetInSongPose();
    auto screen = BSML::FloatingScreen::CreateFloatingScreen(kScreenSize, true, pose.position, pose.rotation, 0.0f, true);
    screen->transform->localScale = Vector3(kScale, kScale, kScale);
    screen->name = "QuestBrowserInSong";
    // Put it in the game scene, so that it goes away with the level.
    SceneManagement::SceneManager::MoveGameObjectToScene(screen->gameObject, pauseController->gameObject->scene);
    screen->HandleReleased.addCallback(&OnHandleReleased);
    screen->set_HandleSide(BSML::Side::Bottom);
    screen->set_ShowHandle(false);

    floatingScreen = screen;
    browserScreen = BrowserScreen::CreateIn(screen->transform);
    browserScreen->SetInteractive(false);
    logger.info("Created in-song browser");
}

void SetPaused(bool paused) {
    if (!floatingScreen || !browserScreen) return;
    floatingScreen->set_ShowHandle(paused);
    browserScreen->SetInteractive(paused);
}

}  // namespace

MAKE_HOOK_MATCH(PauseController_Start, &PauseController::Start, void, PauseController* self) {
    PauseController_Start(self);
    if (!Config::GetShowInSong()) return;
    Create(self);
    // A level can start paused (for example after the game lost focus).
    SetPaused(self->_paused == PauseController::PauseState::Paused);
}

MAKE_HOOK_MATCH(PauseController_Pause, &PauseController::Pause, void, PauseController* self) {
    PauseController_Pause(self);
    if (self->_paused == PauseController::PauseState::Paused) SetPaused(true);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidPressContinueButton,
                &PauseController::HandlePauseMenuManagerDidPressContinueButton, void, PauseController* self) {
    // Lock the screen before the resume countdown, so a click can not reach the page during play.
    SetPaused(false);
    PauseController_HandlePauseMenuManagerDidPressContinueButton(self);
}

void InstallHooks() {
    INSTALL_HOOK(logger, PauseController_Start);
    INSTALL_HOOK(logger, PauseController_Pause);
    INSTALL_HOOK(logger, PauseController_HandlePauseMenuManagerDidPressContinueButton);
}

}  // namespace QuestBrowser::InSong
