#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

// One floating browser window that lives for the whole session, in the menu and in songs.
// Menu and paused: clickable and movable. Playing: view only.
namespace QuestBrowser::Window {

void InstallHooks();
// Menu button: show or hide the window.
void ToggleVisible();
// Settings: apply changed settings, for example "show during songs".
void Refresh();
// Settings: move the window back to the default positions.
void ResetPositions();
// Called by SongScope when the game scene goes away.
void OnSongEnded();

}  // namespace QuestBrowser::Window

// Added to the game scene. When the level ends, the scene goes away and so does
// this component, which tells the window that we are back in the menu.
DECLARE_CLASS_CODEGEN(QuestBrowser, SongScope, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_DEFAULT_CTOR();
};
