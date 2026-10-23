// Include our main header file, containing the logger
#include "main.hpp"

// Include dependency headers
#include "BrowserScreen.hpp"
#include "Config.hpp"
#include "InSongBrowser.hpp"
#include "UnityEngine/Transform.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

// Store the mod ID and version, so it can be sent to the modloader at startup
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

// Called in the early stages of game loading
// (see https://github.com/sc2ad/scotland2?tab=readme-ov-file#installationusage)
// Often used to initialize and load configs, in addition to its contents here
extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);
    QuestBrowser::Config::Load();

    logger.info("Completed setup!");
}

// Called later on in the game loading, after all mods have been opened
// Often used to install hooks and use the APIs of other mods or libraries
extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    custom_types::Register::AutoRegister();
    BSML::Init();

    // Adds a "Browser" button to the mods list on the left of the main menu.
    BSML::Register::RegisterMainMenuViewControllerMethod(
        "Browser", "Browser", "Open a web browser", [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
            if (firstActivation) QuestBrowser::BrowserScreen::CreateIn(viewController->transform);
        });

    // Mod settings: show the browser during songs, and reset its position.
    BSML::Register::RegisterSettingsMenu(
        "Browser", [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
            if (!firstActivation) return;
            auto container = BSML::Lite::CreateScrollableSettingsContainer(viewController->transform);
            BSML::Lite::CreateToggle(container->transform, "Show browser during songs", QuestBrowser::Config::GetShowInSong(),
                                     [](bool value) { QuestBrowser::Config::SetShowInSong(value); });
            BSML::Lite::CreateText(container->transform, "Pause the song to click the page or to move it with the handle below it.", 3.5f);
            BSML::Lite::CreateUIButton(container->transform, "Reset in-song position", []() { QuestBrowser::Config::ResetInSongPose(); });
        },
        false);

    QuestBrowser::InSong::InstallHooks();

    logger.info("Completed load!");
}
