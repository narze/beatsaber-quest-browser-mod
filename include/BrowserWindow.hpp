#pragma once

#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/Component.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "browser_logic.hpp"
#include "custom-types/shared/macros.hpp"

// One floating browser window that lives for the whole session, in the menu and in songs.
// Menu and paused: clickable and movable. Playing: view only.
namespace QuestBrowser::Window {

void InstallHooks();
// Settings: apply changed settings, for example "show during songs".
void Refresh();
// Settings: move the window back to the default positions.
void ResetPositions();
// Settings: move the menu or the in-song window back to its default position and size.
void ResetPosition(Logic::PoseSlot slot);
// Grip grab: save the current position for the current context (menu or song).
void SavePosition();
// Keybind: reset the position of the current context (menu or song).
void ResetCurrentPosition();
// While the player holds the move handle of `screen`, the thumbstick pushes or pulls
// it faster than BSML alone. Call every frame.
void BoostHandleDepth(UnityEngine::Component* screen);
// While the player holds the move handle of the browser window: the window faces the
// head, and the stick and triggers steer it (see BeginHold). Call every frame.
void SteerHandle(UnityEngine::Component* screen);
// The player holds the window (grip or move handle). It keeps its turn and tilt.
// While held: a trigger press changes the mode (zoom, tilt, resize), stick X turns,
// stick Y zooms, tilts or resizes, and a stick click faces the player again. A label
// near the hand shows the mode.
void BeginHold(GlobalNamespace::VRController* holder);
// Call every frame while held. Returns stick Y for push and pull (0 when not zooming).
float UpdateHold(GlobalNamespace::VRController* holder);
// The rotation of the held window at `position`: faces the head, plus the turn and tilt.
UnityEngine::Quaternion HoldRotation(UnityEngine::Vector3 position, UnityEngine::Quaternion fallback);
void EndHold();
bool IsHolding();
// "Hide scenery in front of the browser": check moving scenery again. Call every frame.
void UpdateClearView();
// Menu, song playing, or song paused.
Logic::Context CurrentContext();
// The settings of the current context: menu or song.
inline Logic::PoseSlot CurrentSlot() { return Logic::SlotFor(CurrentContext()); }
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
