#include <doctest/doctest.h>

#include "placement_logic.hpp"

using namespace QuestBrowser::Placement;

TEST_CASE("BottomEdgeY: a window tilted back has its bottom edge closer to the center") {
    // 1 m tall, upright: the bottom edge is 0.5 m below the center.
    CHECK(BottomEdgeY(3.0f, 1.0f, 0.0f) == doctest::Approx(2.5f));
    // Tilted 60 degrees: only half of the half height is vertical.
    CHECK(BottomEdgeY(3.0f, 1.0f, -60.0f) == doctest::Approx(2.75f));
}

TEST_CASE("Reset pose is centered in front of the player") {
    CHECK(kResetPosition.x == doctest::Approx(0.0f));
    CHECK(kResetPosition.z > 2.0f);
}

TEST_CASE("Reset pose clears the song selection screens at the default window size") {
    // Default window: 87 UI units tall at 1.2 cm per unit (only the page), and the
    // toolbar below it: 2 units of gap and 12 units of bar.
    float height = 87.0f * 0.012f;
    float toolbar = (2.0f + 12.0f) * 0.012f;
    float bottom = BottomEdgeY(kResetPosition.y, height, kResetPitchDegrees) - toolbar;
    CHECK(bottom >= kSongSelectionTop);
}

TEST_CASE("KeyboardNearHead: in front of the player, below the eyes, level with the floor") {
    // Head at 1.7 m looking along +z.
    auto pose = KeyboardNearHead({0, 1.7f, 0}, {0, 0, 1});
    CHECK(pose.position.x == doctest::Approx(0));
    CHECK(pose.position.z == doctest::Approx(kKeyboardDistance));
    CHECK(pose.position.y == doctest::Approx(1.7f - kKeyboardDrop));
    CHECK(pose.yawDegrees == doctest::Approx(0));
}

TEST_CASE("KeyboardNearHead: looking down or up does not move the keyboard into the floor") {
    auto down = KeyboardNearHead({0, 1.7f, 0}, {0, -0.8f, 0.6f});
    CHECK(down.position.y == doctest::Approx(1.7f - kKeyboardDrop));
    CHECK(down.position.z == doctest::Approx(kKeyboardDistance));
}

TEST_CASE("KeyboardNearHead: follows the direction the player faces") {
    auto right = KeyboardNearHead({1, 1.5f, 2}, {1, 0, 0});
    CHECK(right.position.x == doctest::Approx(1 + kKeyboardDistance));
    CHECK(right.position.z == doctest::Approx(2));
    CHECK(right.yawDegrees == doctest::Approx(90));
}

TEST_CASE("KeyboardNearHead: straight up or down keeps a forward direction") {
    auto up = KeyboardNearHead({0, 1.7f, 0}, {0, 1, 0});
    CHECK(std::isfinite(up.position.x));
    CHECK(std::isfinite(up.position.z));
}
