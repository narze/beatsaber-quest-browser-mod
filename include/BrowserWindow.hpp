#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "browser_logic.hpp"
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
// Settings: move the menu or the in-song window back to its default position and size.
void ResetPosition(Logic::PoseSlot slot);
// Keybind: show the window and reset the position of the current context (menu or song).
void ResetCurrentPosition();
// Menu, song playing, or song paused.
Logic::Context CurrentContext();
void FitControls();
float GetControlScale();
float GetChromeHeight();
// Called by SongScope when the game scene goes away.
void OnSongEnded();
void CycleAspectPreset();
int GetAspectPreset();

}  // namespace QuestBrowser::Window

// Added to the game scene. When the level ends, the scene goes away and so does
// this component, which tells the window that we are back in the menu.
DECLARE_CLASS_CODEGEN(QuestBrowser, SongScope, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_DEFAULT_CTOR();
};
