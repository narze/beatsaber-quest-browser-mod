#include <doctest/doctest.h>

#include "resize_logic.hpp"

using namespace QuestBrowser::Resize;

TEST_CASE("RayPlaneHit finds where a ray meets a plane") {
    // Plane z = 2, facing the player at the origin.
    auto hit = RayPlaneHit({0, 1, 0}, {0, 0, 1}, {0, 0, 2}, {0, 0, 1});
    REQUIRE(hit.has_value());
    CHECK(hit->x == doctest::Approx(0.0f));
    CHECK(hit->y == doctest::Approx(1.0f));
    CHECK(hit->z == doctest::Approx(2.0f));

    // At 45 degrees to the right.
    auto diag = RayPlaneHit({0, 0, 0}, {1, 0, 1}, {0, 0, 2}, {0, 0, 1});
    REQUIRE(diag.has_value());
    CHECK(diag->x == doctest::Approx(2.0f));
    CHECK(diag->z == doctest::Approx(2.0f));
}

TEST_CASE("RayPlaneHit misses parallel rays and planes behind the ray") {
    CHECK_FALSE(RayPlaneHit({0, 0, 0}, {1, 0, 0}, {0, 0, 2}, {0, 0, 1}).has_value());
    CHECK_FALSE(RayPlaneHit({0, 0, 0}, {0, 0, -1}, {0, 0, 2}, {0, 0, 1}).has_value());
}

TEST_CASE("ScaleFromDrag scales with the distance from the window center") {
    // Corner dragged from 1 m to 1.5 m from the center: 1.5 times bigger.
    CHECK(ScaleFromDrag(0.012f, 1.0f, 1.5f, 0.006f, 0.03f) == doctest::Approx(0.018f));
    // Dragged in to 0.75 m: smaller.
    CHECK(ScaleFromDrag(0.012f, 1.0f, 0.75f, 0.006f, 0.03f) == doctest::Approx(0.009f));
}

TEST_CASE("ScaleFromDrag stays inside the limits") {
    CHECK(ScaleFromDrag(0.012f, 1.0f, 10.0f, 0.006f, 0.03f) == doctest::Approx(0.03f));
    CHECK(ScaleFromDrag(0.012f, 1.0f, 0.01f, 0.006f, 0.03f) == doctest::Approx(0.006f));
}

TEST_CASE("ScaleFromDrag keeps the scale when the start distance is zero") {
    CHECK(ScaleFromDrag(0.012f, 0.0f, 1.0f, 0.006f, 0.03f) == doctest::Approx(0.012f));
}

TEST_CASE("TriggerLatch has hysteresis and reports the press and release edges") {
    TriggerLatch t;
    CHECK(t.Update(0.5f) == Edge::None);  // not yet over the press level
    CHECK(t.Update(0.8f) == Edge::Pressed);
    CHECK(t.Update(0.5f) == Edge::None);  // still held above the release level
    CHECK(t.IsHeld());
    CHECK(t.Update(0.2f) == Edge::Released);
    CHECK_FALSE(t.IsHeld());
}
