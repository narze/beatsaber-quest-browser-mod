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
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"
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

struct CurveSettingsControls {
    SafePtrUnity<HMUI::ViewController> view;
    SafePtrUnity<BSML::ToggleSetting> toggle;
    SafePtrUnity<BSML::IncrementSetting> level;
};
std::vector<CurveSettingsControls> curveSettingsControls;

void BuildSettings(HMUI::ViewController* view) {
    auto container = BSML::Lite::CreateScrollableSettingsContainer(view->transform);
    auto c = container->transform;
    BSML::Lite::CreateToggle(c, "Enable browser", Config::GetEnabled(), [](bool value) {
        if (value == Config::GetEnabled()) return;
        Config::SetEnabled(value);
        Window::Refresh();
    });
    BSML::Lite::CreateToggle(c, "Show during songs", Config::GetShowInSong(), [](bool value) {
        Config::SetShowInSong(value);
        Window::Refresh();
    });
    BSML::Lite::CreateToggle(c, "Hide scenery in front of the browser in songs", Config::GetHideBlockers(),
                             [](bool value) { Config::SetHideBlockers(value); });
    auto curvedToggle = BSML::Lite::CreateToggle(c, "Curved browser (experimental)", Config::GetCurved(), [](bool value) {
        Config::SetCurved(value);
        Window::Refresh();
    });
    auto curveLevel = BSML::Lite::CreateIncrementSetting(c, "Curve level", 0, 1.0f, Config::GetCurveLevel(),
                                       QuestBrowser::Logic::kMinCurveLevel, QuestBrowser::Logic::kMaxCurveLevel,
                                       [](float value) {
                                           Config::SetCurveLevel(static_cast<int>(value));
                                           Window::Refresh();
                                       });
    curveSettingsControls.push_back({view, curvedToggle, curveLevel});

    // Keep the names alive: the dropdown keeps the views.
    static std::vector<std::string_view> names;
    if (names.empty())
        for (auto const& preset : QuestBrowser::Keybind::Presets()) names.push_back(preset.name);
    BSML::Lite::CreateDropdown(c, "Reset keybind (hold)", Config::GetResetKeybind(), names,
                               [](StringW value) { Config::SetResetKeybind(static_cast<std::string>(value)); });

    BSML::Lite::CreateUIButton(c, "Reset menu position", []() { Window::ResetPosition(PoseSlot::Menu); });
    BSML::Lite::CreateUIButton(c, "Reset in-song position", []() { Window::ResetPosition(PoseSlot::Song); });
    BSML::Lite::CreateUIButton(c, "Reset keyboard position", []() { Config::ResetKeyboardPlacement(); });
}

void ShowSettings(HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
    if (firstActivation) BuildSettings(viewController);
    for (auto& controls : curveSettingsControls) {
        if (!controls.view || controls.view.ptr() != viewController) continue;
        if (controls.toggle && controls.toggle->toggle) {
            controls.toggle->currentValue = Config::GetCurved();
            controls.toggle->toggle->SetIsOnWithoutNotify(Config::GetCurved());
        }
        if (controls.level) controls.level->set_Value(Config::GetCurveLevel());
    }
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
