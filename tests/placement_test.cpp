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
