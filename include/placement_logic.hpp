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

// The keyboard opens near the player, where it is large enough to type on.
constexpr float kKeyboardDistance = 0.55f;  // in front of the head, meters
constexpr float kKeyboardDrop = 0.30f;      // below the eyes, meters

struct KeyboardPose {
    Point position;
    float yawDegrees;  // turn around the vertical axis; 0 faces +z
};

// Keyboard pose for a head at `head` looking along `forward`. Only the direction
// on the floor counts, so looking up or down does not move the keyboard.
inline KeyboardPose KeyboardNearHead(Point head, Point forward) {
    float fx = forward.x, fz = forward.z;
    float len = std::sqrt(fx * fx + fz * fz);
    if (len < 1e-3f) {
        fx = 0.0f;
        fz = 1.0f;
    } else {
        fx /= len;
        fz /= len;
    }
    return {{head.x + fx * kKeyboardDistance, head.y - kKeyboardDrop, head.z + fz * kKeyboardDistance},
            std::atan2(fx, fz) * 180.0f / 3.14159265f};
}

// Height of the bottom edge of a window with its center at centerY, tilted by
// pitchDegrees around its horizontal axis.
inline float BottomEdgeY(float centerY, float heightMeters, float pitchDegrees) {
    float pitch = pitchDegrees * 3.14159265f / 180.0f;
    return centerY - heightMeters / 2.0f * std::cos(pitch);
}

}  // namespace QuestBrowser::Placement
