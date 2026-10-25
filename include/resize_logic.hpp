#pragma once

// Game-independent math for resizing the browser window by dragging its corner.
// Unit tested on the host (see tests/resize_test.cpp).

#include <algorithm>
#include <cmath>
#include <optional>

namespace QuestBrowser::Resize {

// The window is only the page; the toolbar floats below it. Chrome is the padding.
// Default page: 144 x 81 (16:9).
struct Dimensions { float width = 150, height = 87; };
constexpr float kChromeHeight = 6;
constexpr float kPaddingWidth = 6;
inline float AspectFor(int preset) {
    return preset == 1 ? 16.0f / 9.0f : preset == 2 ? 4.0f / 3.0f : preset == 3 ? 9.0f / 16.0f : 0.0f;
}

// The window stays centered. Each dragged edge moves by the pointer delta.
// Grow physical controls with distance, accounting for the window's world scale.
inline float ControlScale(float distance, float worldScale) {
    return std::max(1.0f, distance * 0.012f / (2.5f * std::max(worldScale, 0.0001f)));
}

// Saved heights from before the redesign include the old in-window toolbar rows
// (34 units at the default size); the page keeps its size without them.
inline float MigrateHeight(float height) { return std::max(81.0f + kChromeHeight, height - 34.0f); }

// Panels (tabs, menu, history) are designed for a 144 x 81 page and scaled up to
// fill the page area, so their text grows with the window.
inline float PanelScale(float pageWidth, float pageHeight) {
    return std::min(pageWidth / 144.0f, pageHeight / 81.0f);
}

inline Dimensions DimensionsFromDrag(Dimensions start, float dx, float dy, int axisX, int axisY, float aspect,
                                     float controls = 1, float chrome = kChromeHeight) {
    float minWidth = kPaddingWidth + 144 * controls;
    float minHeight = chrome + 81;
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

// Push and pull while holding a move handle. BSML moves the held screen 1 m/s along
// the controller with the thumbstick; this extra step makes the total kHandleDepthGain.
constexpr float kHandleDepthGain = 4.0f;

// New grab distance (meters along the controller) after one frame of extra push.
inline float HandleDepthStep(float grabDistance, float stickY, float deltaSeconds) {
    if (stickY > -0.2f && stickY < 0.2f) return grabDistance;
    return std::clamp(grabDistance + (kHandleDepthGain - 1.0f) * stickY * deltaSeconds, 0.3f, 8.0f);
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
