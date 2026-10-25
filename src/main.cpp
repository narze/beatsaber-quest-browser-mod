// Include our main header file, containing the logger
#include "main.hpp"

// Include dependency headers
#include "Config.hpp"
#include "keybind_logic.hpp"
#include "BrowserWindow.hpp"
#include "UnityEngine/Transform.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/Settings/ToggleSetting.hpp"
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

namespace {

using QuestBrowser::Logic::PoseSlot;
namespace Config = QuestBrowser::Config;
namespace Window = QuestBrowser::Window;

// The "Show browser" switch of each settings page. The controller shortcut and
// "Bring browser here" can show the window, so refresh it when a page opens.
std::vector<SafePtrUnity<BSML::ToggleSetting>> visibleToggles;

void BuildSettings(UnityEngine::Transform* parent) {
    auto container = BSML::Lite::CreateScrollableSettingsContainer(parent);
    auto c = container->transform;
    visibleToggles.emplace_back(BSML::Lite::CreateToggle(c, "Show browser", Config::GetVisible(), [](bool value) {
        if (value == Config::GetVisible()) return;
        Config::SetVisible(value);
        Window::Refresh();
    }));
    BSML::Lite::CreateToggle(c, "Show during songs", Config::GetShowInSong(), [](bool value) {
        Config::SetShowInSong(value);
        Window::Refresh();
    });

    // Keep the names alive: the dropdown keeps the views.
    static std::vector<std::string_view> names;
    if (names.empty())
        for (auto const& preset : QuestBrowser::Keybind::Presets()) names.push_back(preset.name);
    BSML::Lite::CreateDropdown(c, "Reset keybind (hold)", Config::GetResetKeybind(), names,
                               [](StringW value) { Config::SetResetKeybind(static_cast<std::string>(value)); });

    BSML::Lite::CreateUIButton(c, "Reset menu position", []() { Window::ResetPosition(PoseSlot::Menu); });
    BSML::Lite::CreateUIButton(c, "Reset in-song position", []() { Window::ResetPosition(PoseSlot::Song); });
}

void ShowSettings(HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
    if (firstActivation) {
        BuildSettings(viewController->transform);
        return;
    }
    for (auto& toggle : visibleToggles)
        if (toggle && toggle->get_Value() != Config::GetVisible()) toggle->set_Value(Config::GetVisible());
}

}  // namespace

// Called later on in the game loading, after all mods have been opened
// Often used to install hooks and use the APIs of other mods or libraries
extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    custom_types::Register::AutoRegister();
    BSML::Init();

    // "Browser" in the mods list on the left of the main menu opens the browser settings.
    BSML::Register::RegisterMainMenuViewControllerMethod("Browser", "Browser", "Browser settings", ShowSettings);

    // The same page in Mod settings.
    BSML::Register::RegisterSettingsMenu("Browser", ShowSettings, false);

    QuestBrowser::Window::InstallHooks();

    logger.info("Completed load!");
}
