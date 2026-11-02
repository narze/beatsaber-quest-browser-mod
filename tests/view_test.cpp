#include <doctest/doctest.h>

#include "view_logic.hpp"

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
