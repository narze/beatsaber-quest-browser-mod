#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

// Settings saved in /sdcard/ModData/com.beatgames.beatsaber/Configs/QuestBrowser.json
namespace QuestBrowser::Config {

struct InSongPose {
    UnityEngine::Vector3 position;
    UnityEngine::Quaternion rotation;
};

void Load();

bool GetShowInSong();
void SetShowInSong(bool value);

InSongPose GetInSongPose();
void SetInSongPose(InSongPose const& pose);
void ResetInSongPose();

}  // namespace QuestBrowser::Config
