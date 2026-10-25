// Include our main header file, containing the logger
#include "main.hpp"

// Include dependency headers
#include "Config.hpp"
#include "BrowserWindow.hpp"
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
    // It shows or hides the browser window, which stays open in songs too.
    BSML::Register::RegisterMenuButton("Browser", "Show or hide the web browser", QuestBrowser::Window::ToggleVisible);
    BSML::Register::RegisterMenuButton("Bring browser here", "Recover the browser in front of you", QuestBrowser::Window::BringHere);

    // Mod settings: show the browser during songs, and reset its positions.
    BSML::Register::RegisterSettingsMenu(
        "Browser", [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
            if (!firstActivation) return;
            auto container = BSML::Lite::CreateScrollableSettingsContainer(viewController->transform);
            BSML::Lite::CreateToggle(container->transform, "Show browser during songs", QuestBrowser::Config::GetShowInSong(),
                                     [](bool value) {
                                         QuestBrowser::Config::SetShowInSong(value);
                                         QuestBrowser::Window::Refresh();
                                     });
            BSML::Lite::CreateText(container->transform,
                                   "Move the window with the handle below it, and resize it with the grip in its bottom-right corner. "
                                   "The menu and songs each keep their own position and size. "
                                   "In a song, pause to click or move it.",
                                   3.5f);
            BSML::Lite::CreateUIButton(container->transform, "Reset positions and sizes", QuestBrowser::Window::ResetPositions);
            BSML::Lite::CreateUIButton(container->transform, "Bring browser here", QuestBrowser::Window::BringHere);
        },
        false);

    QuestBrowser::Window::InstallHooks();

    logger.info("Completed load!");
}
