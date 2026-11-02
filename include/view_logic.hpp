#pragma once

// Which scenery is between the player and the browser window during a song.
// Game-independent, unit tested on the host (see tests/view_test.cpp).

#include <algorithm>
#include <cmath>
#include <vector>

namespace QuestBrowser::View {

struct Vec3 {
    float x, y, z;
};

// True when the segment from a to b goes through the box [min, max] (slab test).
inline bool SegmentHitsBox(Vec3 a, Vec3 b, Vec3 min, Vec3 max) {
    float start = 0.0f, end = 1.0f;
    float from[3] = {a.x, a.y, a.z};
    float delta[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
    float lo[3] = {min.x, min.y, min.z};
    float hi[3] = {max.x, max.y, max.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(delta[i]) < 1e-6f) {
            if (from[i] < lo[i] || from[i] > hi[i]) return false;
            continue;
        }
        float t1 = (lo[i] - from[i]) / delta[i];
        float t2 = (hi[i] - from[i]) / delta[i];
        if (t1 > t2) std::swap(t1, t2);
        start = std::max(start, t1);
        end = std::min(end, t2);
        if (start > end) return false;
    }
    return true;
}

// An n x n grid of points across a window with this center and half extents.
inline std::vector<Vec3> WindowSamples(Vec3 center, Vec3 halfRight, Vec3 halfUp, int n) {
    std::vector<Vec3> points;
    n = std::max(n, 2);
    points.reserve(static_cast<std::size_t>(n * n));
    for (int row = 0; row < n; ++row) {
        float v = 2.0f * static_cast<float>(row) / static_cast<float>(n - 1) - 1.0f;
        for (int col = 0; col < n; ++col) {
            float u = 2.0f * static_cast<float>(col) / static_cast<float>(n - 1) - 1.0f;
            points.push_back({center.x + halfRight.x * u + halfUp.x * v, center.y + halfRight.y * u + halfUp.y * v,
                              center.z + halfRight.z * u + halfUp.z * v});
        }
    }
    return points;
}

// True when a box (the world bounds of an object) is in the line of sight from the
// eye to any point of the window. A box around the eye (a dome, a room) is the
// surroundings, not something in the way.
inline bool BlocksView(Vec3 eye, std::vector<Vec3> const& window, Vec3 min, Vec3 max) {
    bool around = eye.x >= min.x && eye.x <= max.x && eye.y >= min.y && eye.y <= max.y && eye.z >= min.z && eye.z <= max.z;
    if (around) return false;
    return std::any_of(window.begin(), window.end(), [&](Vec3 p) { return SegmentHitsBox(eye, p, min, max); });
}

// The depth occluder: a depth-only copy of the window shape, a little behind the page.
// It draws after the opaque scenery (still visible through a see-through window) and
// before the transparent objects, so that those behind the window fail the depth test.
constexpr int kOccluderQueue = 2499;
constexpr float kOccluderGapMeters = 0.02f;

inline float OccluderGapUnits(float worldScale) { return kOccluderGapMeters / std::max(worldScale, 0.0001f); }

}  // namespace QuestBrowser::View
