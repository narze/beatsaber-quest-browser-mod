#pragma once

// Game-independent math for resizing the browser window by dragging its corner.
// Unit tested on the host (see tests/resize_test.cpp).

#include <algorithm>
#include <cmath>
#include <optional>

namespace QuestBrowser::Resize {

struct Vec3 {
    float x, y, z;
};

inline Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

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
