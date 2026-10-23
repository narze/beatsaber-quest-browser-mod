#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "browser_logic.hpp"

// Settings saved in /sdcard/ModData/com.beatgames.beatsaber/Configs/QuestBrowser.json
namespace QuestBrowser::Config {

struct Pose {
    UnityEngine::Vector3 position;
    UnityEngine::Quaternion rotation;
};

void Load();

// The menu button shows or hides the browser window.
bool GetVisible();
void SetVisible(bool value);

bool GetShowInSong();
void SetShowInSong(bool value);

// One saved position for the menu and one for songs.
Pose GetPose(Logic::PoseSlot slot);
void SetPose(Logic::PoseSlot slot, Pose const& pose);
void ResetPoses();

}  // namespace QuestBrowser::Config
