#pragma once

// "Hide scenery during songs: All". Hides the whole song environment while the browser
// shows in a song, the same way as narze/beatsaber-quest-passthrough-mod:
// - disables the renderers of the song environment scenes,
// - removes the environment layers from the gameplay camera (light scripts turn
//   renderers on again, and track rings are spawned into GameCore),
// - gives shaders a black light (bloom pre-pass) texture, so notes, bombs and fog
//   do not glow in the light colors.
// Notes, bombs, walls, sabers, scores and the HUD stay.
namespace QuestBrowser::Scenery {

void InstallHooks();
// A song starts. hideAll: the mode is All and the browser shows in this song.
void BeginSong(bool hideAll);
// The mode changed during a song: hide the environment, or show it again.
void SetHideAll(bool hideAll);
// The song scene is gone.
void EndSong();

}  // namespace QuestBrowser::Scenery
