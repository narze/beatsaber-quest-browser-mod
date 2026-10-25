#pragma once

// Where "Reset menu position" and "Reset in-song position" put the browser window.
// Game-independent, unit tested on the host (see tests/placement_test.cpp).

#include <cmath>

namespace QuestBrowser::Placement {

struct Point {
    float x, y, z;
};

// Top of the song selection screens in the main menu, in meters (estimate).
constexpr float kSongSelectionTop = 2.6f;

// Centered in front of the player and high enough to clear the song selection.
// In songs, this is above the note track and the pause menu.
constexpr Point kResetPosition{0.0f, 3.5f, 3.8f};
// Tilted back toward the player, who looks up at it.
constexpr float kResetPitchDegrees = -25.0f;

// Height of the bottom edge of a window with its center at centerY, tilted by
// pitchDegrees around its horizontal axis.
inline float BottomEdgeY(float centerY, float heightMeters, float pitchDegrees) {
    float pitch = pitchDegrees * 3.14159265f / 180.0f;
    return centerY - heightMeters / 2.0f * std::cos(pitch);
}

}  // namespace QuestBrowser::Placement
