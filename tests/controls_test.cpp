#include <doctest/doctest.h>

#include "controls_logic.hpp"

using namespace QuestBrowser::Controls;

TEST_CASE("HeldButton: press above the press level, release below the release level") {
    HeldButton grip;
    CHECK_FALSE(grip.Update(0.5f));
    CHECK(grip.Update(0.8f));
    CHECK(grip.Update(0.5f));  // half released: still held
    CHECK_FALSE(grip.Update(0.2f));
}

TEST_CASE("GrabDepth: the stick pushes the window away and pulls it closer") {
    CHECK(GrabDepth(2.0f, 1.0f, 0.5f) == doctest::Approx(2.0f + kGrabDepthSpeed * 0.5f));
    CHECK(GrabDepth(2.0f, -1.0f, 0.1f) == doctest::Approx(2.0f - kGrabDepthSpeed * 0.1f));
}

TEST_CASE("GrabDepth: no change in the dead zone, and limits near and far") {
    CHECK(GrabDepth(2.0f, 0.15f, 1.0f) == 2.0f);
    CHECK(GrabDepth(0.4f, -1.0f, 1.0f) == kMinGrabDepth);
    CHECK(GrabDepth(7.9f, 1.0f, 1.0f) == kMaxGrabDepth);
}

TEST_CASE("StickStepper: one step at once, then repeats while held") {
    StickStepper zoom;
    CHECK(zoom.Update(0.1f, 0.016f) == 0);
    CHECK(zoom.Update(0.9f, 0.016f) == 1);
    CHECK(zoom.Update(0.9f, kRepeatSeconds / 2) == 0);
    CHECK(zoom.Update(0.9f, kRepeatSeconds / 2 + 0.001f) == 1);
}

TEST_CASE("StickStepper: down steps down; back to the middle stops and resets") {
    StickStepper zoom;
    CHECK(zoom.Update(-0.9f, 0.016f) == -1);
    CHECK(zoom.Update(0.0f, 0.016f) == 0);
    CHECK(zoom.Update(-0.9f, 0.016f) == -1);
}

TEST_CASE("StickStepper: a flip to the other side steps at once") {
    StickStepper zoom;
    CHECK(zoom.Update(0.9f, 0.016f) == 1);
    CHECK(zoom.Update(-0.9f, 0.016f) == -1);
}
