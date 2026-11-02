#include <doctest/doctest.h>

#include "view_logic.hpp"

#include <optional>
#include <string>

using namespace QuestBrowser::View;

TEST_CASE("SegmentHitsBox: a box between the eye and the target blocks it") {
    CHECK(SegmentHitsBox({0, 1.7f, 0}, {0, 1.7f, 10}, {-1, 1, 4}, {1, 2, 5}));
    // To the side, below, and behind the target.
    CHECK_FALSE(SegmentHitsBox({0, 1.7f, 0}, {0, 1.7f, 10}, {2, 1, 4}, {3, 2, 5}));
    CHECK_FALSE(SegmentHitsBox({0, 1.7f, 0}, {0, 1.7f, 10}, {-1, -1, 4}, {1, 0, 5}));
    CHECK_FALSE(SegmentHitsBox({0, 1.7f, 0}, {0, 1.7f, 10}, {-1, 1, 11}, {1, 2, 12}));
}

TEST_CASE("SegmentHitsBox: works for slanted and axis-parallel segments") {
    // Looking down at a far, low window: the track in between blocks it.
    CHECK(SegmentHitsBox({0, 1.7f, 0}, {0, -2.5f, 16}, {-1, -0.5f, 2}, {1, 0.2f, 30}));
    // Parallel to the box faces, outside the box.
    CHECK_FALSE(SegmentHitsBox({0, 3, 0}, {0, 3, 10}, {-1, 0, 0}, {1, 2, 10}));
}

TEST_CASE("WindowSamples: a grid across the window, corners included") {
    auto points = WindowSamples({0, 2, 10}, {2, 0, 0}, {0, 1, 0}, 3);
    CHECK(points.size() == 9);
    CHECK(points.front().x == doctest::Approx(-2));
    CHECK(points.front().y == doctest::Approx(1));
    CHECK(points.back().x == doctest::Approx(2));
    CHECK(points.back().y == doctest::Approx(3));
    CHECK(points[4].x == doctest::Approx(0));
    CHECK(points[4].y == doctest::Approx(2));
}

TEST_CASE("BlocksView: an object that covers part of the window blocks it") {
    Vec3 eye{0, 1.7f, 0};
    auto window = WindowSamples({0, 2, 10}, {3, 0, 0}, {0, 2, 0}, 5);
    // A small box in front of the lower left part.
    CHECK(BlocksView(eye, window, {-1.5f, 0.5f, 4}, {-0.5f, 1.2f, 5}));
    // Behind the window, or far to the side: not in the way.
    CHECK_FALSE(BlocksView(eye, window, {-1, 0, 12}, {1, 4, 13}));
    CHECK_FALSE(BlocksView(eye, window, {20, 0, 4}, {21, 4, 5}));
}

TEST_CASE("BlocksView: surroundings that contain the eye stay visible") {
    // A dome or a room around the player is not in the way: hiding it would hide everything.
    auto window = WindowSamples({0, 2, 10}, {3, 0, 0}, {0, 2, 0}, 5);
    CHECK_FALSE(BlocksView({0, 1.7f, 0}, window, {-50, -5, -50}, {50, 50, 50}));
}

TEST_CASE("Depth occluder: after the opaque scenery, before transparent glows and the UI") {
    // Opaque scenery uses queues up to 2018; laser glows start at 2501; UI is 3000.
    CHECK(kOccluderQueue > 2018);
    CHECK(kOccluderQueue <= 2500);
}

TEST_CASE("Depth occluder: a small gap behind the page, in window units") {
    // At the default scale (1 unit = 1.2 cm) the gap is under 2 units.
    CHECK(OccluderGapUnits(0.012f) * 0.012f == doctest::Approx(kOccluderGapMeters));
    CHECK(OccluderGapUnits(0.0f) > 0.0f);  // no division by zero
}

TEST_CASE("IsScenery: environment scenes and track rings, but not the HUD or game objects") {
    CHECK(IsScenery("DefaultEnvironment", false, false));
    CHECK(IsScenery("GameCore", true, false));          // track lane rings are spawned into GameCore
    CHECK_FALSE(IsScenery("GameCore", false, false));   // notes, sabers, trails
    CHECK_FALSE(IsScenery("StandardGameplay", false, false));
    CHECK_FALSE(IsScenery("DefaultEnvironment", false, true));  // HUD canvases
}

TEST_CASE("SceneryMode: names for the setting, and back") {
    CHECK(std::string(SceneryModeName(SceneryMode::Off)) == "Off");
    CHECK(std::string(SceneryModeName(SceneryMode::InFront)) == "In front of browser");
    CHECK(std::string(SceneryModeName(SceneryMode::All)) == "All");
    for (auto mode : {SceneryMode::Off, SceneryMode::InFront, SceneryMode::All})
        CHECK(SceneryModeFromName(SceneryModeName(mode)) == mode);
    CHECK(SceneryModeFromName("something else") == SceneryMode::Off);
}

TEST_CASE("SceneryMode: from the config, with the old on/off switch as fallback") {
    CHECK(SceneryModeFromConfig(2, false) == SceneryMode::All);
    CHECK(SceneryModeFromConfig(1, false) == SceneryMode::InFront);
    CHECK(SceneryModeFromConfig(7, true) == SceneryMode::Off);  // unknown value
    CHECK(SceneryModeFromConfig(std::nullopt, true) == SceneryMode::InFront);
    CHECK(SceneryModeFromConfig(std::nullopt, false) == SceneryMode::Off);
}

TEST_CASE("IsSongEnvironmentScene: song environments only, not the menu or game scenes") {
    CHECK(IsSongEnvironmentScene("DefaultEnvironment"));
    CHECK(IsSongEnvironmentScene("WeaveEnvironment"));
    CHECK_FALSE(IsSongEnvironmentScene("DefaultMenuEnvironment"));
    CHECK_FALSE(IsSongEnvironmentScene("Environment"));
    CHECK_FALSE(IsSongEnvironmentScene("GameCore"));
}

TEST_CASE("CullingMaskFor: All removes the environment layers, keeps notes and sabers") {
    int game = (1 << 8) | (1 << 9) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 29);
    int hidden = CullingMaskFor(game, true);
    CHECK((hidden & (1 << 13)) == 0);
    CHECK((hidden & (1 << 14)) == 0);
    CHECK((hidden & (1 << 29)) == 0);
    CHECK((hidden & (1 << 8)) != 0);   // notes
    CHECK((hidden & (1 << 9)) != 0);   // debris
    CHECK((hidden & (1 << 12)) != 0);  // sabers
    CHECK(CullingMaskFor(game, false) == game);
}
