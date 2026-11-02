#pragma once

// Which scenery is between the player and the browser window during a song.
// Game-independent, unit tested on the host (see tests/view_test.cpp).

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
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

// Renderers that "Hide scenery" may hide: those in an environment scene, and the track
// lane rings (the game spawns them into GameCore, with the notes and sabers). Not the
// ones inside a canvas (the HUD).
inline bool IsScenery(std::string_view sceneName, bool inTrackRing, bool inCanvas) {
    if (inCanvas) return false;
    return inTrackRing || sceneName.find("Environment") != std::string_view::npos;
}

// "Hide scenery during songs" (only while the browser shows in songs).
// Off; InFront: scenery between the player and the window; All: the whole song
// environment, as the passthrough mod does (narze/beatsaber-quest-passthrough-mod).
enum class SceneryMode { Off = 0, InFront = 1, All = 2 };

inline char const* SceneryModeName(SceneryMode mode) {
    switch (mode) {
        case SceneryMode::InFront: return "In front of browser";
        case SceneryMode::All: return "All";
        default: return "Off";
    }
}

inline SceneryMode SceneryModeFromName(std::string_view name) {
    for (auto mode : {SceneryMode::InFront, SceneryMode::All})
        if (name == SceneryModeName(mode)) return mode;
    return SceneryMode::Off;
}

inline char const* SceneryModeShortName(SceneryMode mode) {
    return mode == SceneryMode::InFront ? "In front" : SceneryModeName(mode);
}

// Menu > View > Scenery "Change".
inline SceneryMode NextSceneryMode(SceneryMode mode) {
    return mode == SceneryMode::Off ? SceneryMode::InFront : mode == SceneryMode::InFront ? SceneryMode::All : SceneryMode::Off;
}

// The saved mode, or the old on/off switch ("hideBlockers") when there is none.
inline SceneryMode SceneryModeFromConfig(std::optional<int> saved, bool legacyOn) {
    if (!saved) return legacyOn ? SceneryMode::InFront : SceneryMode::Off;
    return *saved == 1 ? SceneryMode::InFront : *saved == 2 ? SceneryMode::All : SceneryMode::Off;
}

// Beat Saber loads each song environment as its own scene, "<Name>Environment".
// The menu environments ("...MenuEnvironment") do not count.
inline bool IsSongEnvironmentScene(std::string_view name) {
    constexpr std::string_view suffix = "Environment";
    return name.size() > suffix.size() && name.ends_with(suffix) && name.find("Menu") == std::string_view::npos;
}

// Unity layers of the song environment (from the passthrough mod, Beat Saber 1.40.8):
// 13 = lights (BoxLight, BakedBloom), 14 = geometry (rings, ...), 29 = BloomSkyboxQuad
// (the glowing background). Notes (8), debris (9) and sabers (12) are on other layers.
// Light scripts turn renderers on again, so All also removes these layers from the camera.
constexpr int kEnvironmentLayers = (1 << 13) | (1 << 14) | (1 << 29);

constexpr int CullingMaskFor(int original, bool hideAll) { return hideAll ? (original & ~kEnvironmentLayers) : original; }

// The depth occluder: a depth-only copy of the window shape, a little behind the page.
// It draws after the opaque scenery (still visible through a see-through window) and
// before the transparent objects, so that those behind the window fail the depth test.
constexpr int kOccluderQueue = 2499;
constexpr float kOccluderGapMeters = 0.02f;

inline float OccluderGapUnits(float worldScale) { return kOccluderGapMeters / std::max(worldScale, 0.0001f); }

}  // namespace QuestBrowser::View
