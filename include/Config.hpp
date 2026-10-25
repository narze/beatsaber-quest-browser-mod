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

// Hold the left thumbstick click to bring the browser in front of you.
bool GetShortcutEnabled();
void SetShortcutEnabled(bool value);

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
std::vector<std::string> GetHistory();
void SetHistory(std::vector<std::string> const& urls);

}  // namespace QuestBrowser::Config
