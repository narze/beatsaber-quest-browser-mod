#pragma once

// Where "Reset menu position" and "Reset in-song position" put the browser window.
// Game-independent, unit tested on the host (see tests/placement_test.cpp).

#include <cmath>

#include "resize_logic.hpp"

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

// The menu has more free space than a song, so its reset window is larger: a
// 240 x 135 page (16:9), about 2.9 m wide. It is a little higher, to clear the
// song selection.
constexpr Resize::Dimensions kMenuResetDimensions{240.0f + Resize::kPaddingWidth, 135.0f + Resize::kChromeHeight};
constexpr Point kMenuResetPosition{0.0f, 3.7f, 3.8f};

// The keyboard opens near the player, where it is large enough to type on.
constexpr float kKeyboardDistance = 0.50f;  // in front of the head, meters
constexpr float kKeyboardDrop = 0.45f;      // below the eyes, meters

struct KeyboardPose {
    Point position;
    float yawDegrees;  // turn around the vertical axis; 0 faces +z
};

namespace detail {

// The direction the player faces on the floor (unit length).
inline void Facing(Point forward, float& fx, float& fz) {
    fx = forward.x;
    fz = forward.z;
    float len = std::sqrt(fx * fx + fz * fz);
    if (len < 1e-3f) {
        fx = 0.0f;
        fz = 1.0f;
    } else {
        fx /= len;
        fz /= len;
    }
}

}  // namespace detail

// Keyboard pose for a head at `head` looking along `forward`, moved by `offset`
// (x right, y up, z away from the player, in meters). Only the direction on the
// floor counts, so looking up or down does not move the keyboard.
inline KeyboardPose KeyboardNearHead(Point head, Point forward, Point offset = {0, 0, 0}) {
    float fx, fz;
    detail::Facing(forward, fx, fz);
    // Right of the facing direction: (fz, 0, -fx).
    float away = kKeyboardDistance + offset.z;
    return {{head.x + fx * away + fz * offset.x, head.y - kKeyboardDrop + offset.y, head.z + fz * away - fx * offset.x},
            std::atan2(fx, fz) * 180.0f / 3.14159265f};
}

// The offset of a keyboard at `position` from its default pose, in the facing
// direction of the player (x right, y up, z away). Saved when the player moves it.
inline Point ToHeadFrame(Point head, Point forward, Point position) {
    float fx, fz;
    detail::Facing(forward, fx, fz);
    auto base = KeyboardNearHead(head, forward);
    float dx = position.x - base.position.x, dy = position.y - base.position.y, dz = position.z - base.position.z;
    return {dx * fz - dz * fx, dy, dx * fx + dz * fz};
}

// Turn of a window at `window` that faces a head at `head`, in degrees, like Unity
// Euler angles: pitch around x (negative tilts the top back, for a window above the
// eyes), yaw around y (0 faces +z). No roll.
struct Angles {
    float pitch, yaw;
};
inline Angles FacingAngles(Point head, Point window) {
    float dx = window.x - head.x, dy = window.y - head.y, dz = window.z - head.z;
    float flat = std::sqrt(dx * dx + dz * dz);
    constexpr float toDegrees = 180.0f / 3.14159265f;
    float yaw = flat < 1e-4f ? 0.0f : std::atan2(dx, dz) * toDegrees;
    return {-std::atan2(dy, flat) * toDegrees, yaw};
}

// Height of the bottom edge of a window with its center at centerY, tilted by
// pitchDegrees around its horizontal axis.
inline float BottomEdgeY(float centerY, float heightMeters, float pitchDegrees) {
    float pitch = pitchDegrees * 3.14159265f / 180.0f;
    return centerY - heightMeters / 2.0f * std::cos(pitch);
}

}  // namespace QuestBrowser::Placement
