#pragma once

// Game-independent math for resizing the browser window by dragging its corner.
// Unit tested on the host (see tests/resize_test.cpp).

#include <algorithm>
#include <cmath>
#include <optional>

namespace QuestBrowser::Resize {

struct Dimensions { float width = 150, height = 126; };
constexpr float kChromeHeight = 45;
constexpr float kPaddingWidth = 6;
inline float AspectFor(int preset) {
    return preset == 1 ? 16.0f / 9.0f : preset == 2 ? 4.0f / 3.0f : preset == 3 ? 9.0f / 16.0f : 0.0f;
}

// The window stays centered. Each dragged edge moves by the pointer delta.
// Grow physical controls with distance, accounting for the window's world scale.
inline float ControlScale(float distance, float worldScale) {
    return std::max(1.0f, distance * 0.012f / (2.5f * std::max(worldScale, 0.0001f)));
}

// Window height while a song plays: the controls are hidden, so only the page and
// its padding remain. The page keeps the size it has with the controls shown.
inline float PlayModeHeight(Dimensions size, float chrome) {
    return std::max(81.0f, size.height - chrome) + kPaddingWidth;
}

inline Dimensions DimensionsFromDrag(Dimensions start, float dx, float dy, int axisX, int axisY, float aspect,
                                     float controls = 1, float chrome = kChromeHeight) {
    float minWidth = kPaddingWidth + 144 * controls;
    float minHeight = std::max(126.0f, chrome + 81);
    float w = std::max(start.width + 2 * dx * axisX, minWidth);
    float h = std::max(start.height + 2 * dy * axisY, minHeight);
    if (aspect > 0) {
        float pageW = axisX ? w - kPaddingWidth : (h - chrome) * aspect;
        float minW = std::max(minWidth - kPaddingWidth, (minHeight - chrome) * aspect);
        pageW = std::max(pageW, minW);
        w = pageW + kPaddingWidth;
        h = pageW / aspect + chrome;
    }
    return {w, h};
}

struct Vec3 {
    float x, y, z;
};

inline Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

// The window moves this many times as far as the hand, so a short arm movement
// covers the 0.6 m to 8 m range.
constexpr float kDepthGain = 4.0f;

// Controller motion sideways/upwards does not affect depth. Axis must be normalized.
inline float DepthFromDrag(float startDistance, Vec3 controllerDelta, Vec3 axis, float gain = 1.0f) {
    return std::clamp(startDistance + gain * Dot(controllerDelta, axis), 0.6f, 8.0f);
}

// Where the ray (origin, direction) meets the plane (point, normal).
// nullopt when the ray is parallel to the plane or the plane is behind the ray.
inline std::optional<Vec3> RayPlaneHit(Vec3 origin, Vec3 direction, Vec3 planePoint, Vec3 planeNormal) {
    float denom = Dot(direction, planeNormal);
    if (std::fabs(denom) < 1e-6f) return std::nullopt;
    float t = Dot(Sub(planePoint, origin), planeNormal) / denom;
    if (t < 0.0f) return std::nullopt;
    return Vec3{origin.x + direction.x * t, origin.y + direction.y * t, origin.z + direction.z * t};
}

// New window scale while dragging: the scale grows with the distance of the
// pointer from the window center, compared to the distance when the drag started.
inline float ScaleFromDrag(float startScale, float startDistance, float distance, float minScale, float maxScale) {
    if (startDistance <= 1e-4f) return startScale;
    return std::clamp(startScale * distance / startDistance, minScale, maxScale);
}

enum class Edge { None, Pressed, Released };

// Trigger with hysteresis, so a half-pulled trigger does not flicker.
class TriggerLatch {
   public:
    Edge Update(float value) {
        bool next = held ? value > kRelease : value > kPress;
        Edge edge = next == held ? Edge::None : (next ? Edge::Pressed : Edge::Released);
        held = next;
        return edge;
    }
    bool IsHeld() const { return held; }

   private:
    static constexpr float kPress = 0.75f;
    static constexpr float kRelease = 0.35f;
    bool held = false;
};

}  // namespace QuestBrowser::Resize
