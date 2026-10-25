#include <doctest/doctest.h>

#include "resize_logic.hpp"

using namespace QuestBrowser::Resize;

TEST_CASE("Large windows can grow beyond the old ceiling and retain their saved dimensions") {
    auto size = DimensionsFromDrag({350, 390}, 200, 300, 1, 1, 0);
    CHECK(size.width == doctest::Approx(750));
    CHECK(size.height == doctest::Approx(990));
    auto restored = DimensionsFromDrag(size, 0, 0, 0, 0, 0);
    CHECK(restored.width == doctest::Approx(750));
    CHECK(restored.height == doctest::Approx(990));
    auto wide = DimensionsFromDrag({350, 390}, 400, 0, 1, 0, 16.0f / 9.0f);
    CHECK(wide.width == doctest::Approx(1150));
    CHECK((wide.width - 6) / (wide.height - kChromeHeight) == doctest::Approx(16.0f / 9.0f));
}

TEST_CASE("Distant controls keep their angular size and the window fits them") {
    CHECK(ControlScale(1, 0.012f) == doctest::Approx(1));
    CHECK(ControlScale(5, 0.012f) == doctest::Approx(2));
    CHECK(ControlScale(5, 0.024f) == doctest::Approx(1));
    auto size = DimensionsFromDrag({150, 126}, -100, -100, 1, 1, 0, 2, 70);
    CHECK(size.width == doctest::Approx(294));
    CHECK(size.height == doctest::Approx(151));
    auto portrait = DimensionsFromDrag({150, 126}, -100, -100, 1, 1, 9.0f / 16.0f, 2, 70);
    CHECK(portrait.width >= 294);
    CHECK((portrait.width - 6) / (portrait.height - 70) == doctest::Approx(9.0f / 16.0f));
}

TEST_CASE("Depth handle projects movement along viewing depth and clamps distance") {
    CHECK(DepthFromDrag(3, {0, 0, 0.5f}, {0, 0, 1}) == doctest::Approx(3.5f));
    CHECK(DepthFromDrag(3, {1, 2, 0}, {0, 0, 1}) == doctest::Approx(3));
    CHECK(DepthFromDrag(3, {0, 0, -10}, {0, 0, 1}) == doctest::Approx(0.6f));
    CHECK(DepthFromDrag(3, {0, 0, 10}, {0, 0, 1}) == doctest::Approx(8));
}

TEST_CASE("Free corner resize changes width and height independently") {
    auto size = DimensionsFromDrag({150, 126}, 25, -10, 1, -1, 0);
    CHECK(size.width == doctest::Approx(200));
    CHECK(size.height == doctest::Approx(146));
}

TEST_CASE("Edge resizing changes only its axis and enforces usable bounds") {
    auto width = DimensionsFromDrag({180, 160}, 15, 99, 1, 0, 0);
    CHECK(width.width == doctest::Approx(210));
    CHECK(width.height == doctest::Approx(160));
    auto height = DimensionsFromDrag({180, 160}, 99, 15, 0, 1, 0);
    CHECK(height.width == doctest::Approx(180));
    CHECK(height.height == doctest::Approx(190));
    auto minimum = DimensionsFromDrag({180, 160}, -999, 999, 1, -1, 0);
    CHECK(minimum.width == 150);
    CHECK(minimum.height == 87);  // page-only window: 81 of page plus 6 of padding
}

TEST_CASE("Aspect presets constrain the page area rather than toolbar area") {
    auto wide = DimensionsFromDrag({150, 126}, 30, 0, 1, 0, 16.0f / 9.0f);
    CHECK((wide.width - 6) / (wide.height - kChromeHeight) == doctest::Approx(16.0f / 9.0f));
    auto portrait = DimensionsFromDrag({150, 301}, 0, 20, 0, -1, 9.0f / 16.0f);
    CHECK((portrait.width - 6) / (portrait.height - kChromeHeight) == doctest::Approx(9.0f / 16.0f));
}

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

TEST_CASE("DepthFromDrag: the gain makes the window move faster than the hand") {
    // Hand 0.25 m forward with gain 4: the window moves 1 m.
    CHECK(DepthFromDrag(3, {0, 0, 0.25f}, {0, 0, 1}, kDepthGain) == doctest::Approx(3 + 0.25f * kDepthGain));
    CHECK(kDepthGain >= 3.0f);
    // Limits still apply.
    CHECK(DepthFromDrag(3, {0, 0, 5}, {0, 0, 1}, kDepthGain) == doctest::Approx(8));
}

TEST_CASE("The window is only the page: chrome is the padding, the default page is 16:9") {
    CHECK(kChromeHeight == doctest::Approx(6));
    Dimensions d;
    CHECK((d.width - kPaddingWidth) / (d.height - kChromeHeight) == doctest::Approx(16.0f / 9.0f));
}

TEST_CASE("MigrateHeight removes the old in-window toolbar from saved heights") {
    CHECK(MigrateHeight(126) == doctest::Approx(92));  // old default: 34 of toolbar rows
    CHECK(MigrateHeight(300) == doctest::Approx(266));
    CHECK(MigrateHeight(100) == doctest::Approx(87));  // never below the minimum
}
