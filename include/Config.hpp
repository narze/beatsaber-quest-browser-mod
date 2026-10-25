#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "browser_logic.hpp"
#include "history_logic.hpp"
#include "resize_logic.hpp"

// Settings saved in /sdcard/ModData/com.beatgames.beatsaber/Configs/QuestBrowser.json
namespace QuestBrowser::Config {

// Default window scale: 1 UI unit = 1.2 cm, so the window is about 1.8 m wide.
constexpr float kDefaultScale = 0.012f;

struct Pose {
    UnityEngine::Vector3 position;
    UnityEngine::Quaternion rotation;
    float scale = kDefaultScale;
};

void Load();

// The menu button shows or hides the browser window.
bool GetVisible();
void SetVisible(bool value);

bool GetShowInSong();
void SetShowInSong(bool value);

// Name of the controller combo (keybind_logic.hpp) that resets the window position when held.
std::string GetResetKeybind();
void SetResetKeybind(std::string_view name);

// One saved position and size for the menu and one for songs.
Pose GetPose(Logic::PoseSlot slot);
void SetPose(Logic::PoseSlot slot, Pose const& pose);
// Back to the default position, size and aspect: centered, above the song selection.
void ResetPose(Logic::PoseSlot slot);
void ResetPoses();
Resize::Dimensions GetDimensions(Logic::PoseSlot slot);
void SetDimensions(Logic::PoseSlot slot, Resize::Dimensions size);
int GetAspectPreset(Logic::PoseSlot slot);
void SetAspectPreset(Logic::PoseSlot slot, int preset);
float GetZoom();
void SetZoom(float zoom);
float GetOpacity();
void SetOpacity(float value);
// Where the keyboard opens, relative to the direction the player faces: offset in
// meters (x right, y up, z away) and turn relative to facing the player.
struct KeyboardPlacement {
    UnityEngine::Vector3 offset;
    UnityEngine::Quaternion rotation;
};
KeyboardPlacement GetKeyboardPlacement();
void SetKeyboardPlacement(KeyboardPlacement const& placement);
void ResetKeyboardPlacement();

// Open tabs, restored at the next start.
std::vector<std::string> GetTabs();
int GetActiveTab();
void SetTabs(std::vector<std::string> const& urls, int active);
std::vector<std::string> GetHistory();
void SetHistory(std::vector<std::string> const& urls);

}  // namespace QuestBrowser::Config
