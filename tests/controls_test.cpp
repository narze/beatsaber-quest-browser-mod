#include <doctest/doctest.h>

#include "controls_logic.hpp"

#include <string>

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
    CHECK(GrabDepth(8.0f, 1.0f, 1.0f) > 8.0f);
    CHECK(GrabDepth(20.0f, 1.0f, 1.0f) > 20.0f);
    CHECK(GrabDepth(29.9f, 1.0f, 1.0f) == 30.0f);
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

TEST_CASE("TurnStep: thumbstick X turns the grabbed window, a resting stick does not") {
    CHECK(TurnStep(10.0f, 0.1f, 1.0f) == doctest::Approx(10.0f));  // dead zone
    CHECK(TurnStep(0.0f, 1.0f, 0.5f) == doctest::Approx(kTurnSpeed * 0.5f));
    CHECK(TurnStep(0.0f, -1.0f, 0.5f) == doctest::Approx(-kTurnSpeed * 0.5f));
}

TEST_CASE("HoldSteer: starts in zoom mode with the given turn and tilt") {
    HoldSteer steer;
    steer.Begin(10.0f, -5.0f);
    CHECK(steer.Mode() == HoldMode::Zoom);
    CHECK(steer.Turn() == doctest::Approx(10.0f));
    CHECK(steer.Tilt() == doctest::Approx(-5.0f));
}

TEST_CASE("HoldSteer: a trigger press goes to the next mode, and back to zoom after resize") {
    HoldSteer steer;
    steer.Begin(0, 0);
    steer.Update({0, 0, true, false, 0.01f});
    CHECK(steer.Mode() == HoldMode::Tilt);
    steer.Update({0, 0, true, false, 0.01f});
    CHECK(steer.Mode() == HoldMode::Resize);
    steer.Update({0, 0, true, false, 0.01f});
    CHECK(steer.Mode() == HoldMode::Zoom);
}

TEST_CASE("HoldSteer: zoom mode gives stick Y to push and pull, nothing else") {
    HoldSteer steer;
    steer.Begin(0, 0);
    auto out = steer.Update({0, 0.8f, false, false, 0.5f});
    CHECK(out.depthStick == doctest::Approx(0.8f));
    CHECK(out.sizeFactor == doctest::Approx(1.0f));
    CHECK(steer.Tilt() == doctest::Approx(0));
}

TEST_CASE("HoldSteer: tilt mode tilts with stick Y, up pushes the top away, within limits") {
    HoldSteer steer;
    steer.Begin(0, 0);
    steer.Update({0, 0, true, false, 0.0f});
    auto out = steer.Update({0, 1.0f, false, false, 0.5f});
    CHECK(out.depthStick == doctest::Approx(0));
    CHECK(steer.Tilt() == doctest::Approx(-kTiltSpeed * 0.5f));
    for (int i = 0; i < 100; ++i) steer.Update({0, 1.0f, false, false, 0.5f});
    CHECK(steer.Tilt() == doctest::Approx(-kMaxTilt));
}

TEST_CASE("HoldSteer: resize mode grows with stick up and shrinks with stick down") {
    HoldSteer steer;
    steer.Begin(0, 0);
    steer.Update({0, 0, true, false, 0.0f});
    steer.Update({0, 0, true, false, 0.0f});
    CHECK(steer.Update({0, 1.0f, false, false, 0.1f}).sizeFactor > 1.0f);
    CHECK(steer.Update({0, -1.0f, false, false, 0.1f}).sizeFactor < 1.0f);
    CHECK(steer.Update({0, 0.1f, false, false, 0.1f}).sizeFactor == doctest::Approx(1.0f));  // dead zone
    CHECK(steer.Update({0, 1.0f, false, false, 0.1f}).depthStick == doctest::Approx(0));
}

TEST_CASE("HoldSteer: stick X turns in every mode; a stick click faces the player again") {
    HoldSteer steer;
    steer.Begin(0, 20.0f);
    steer.Update({1.0f, 0, false, false, 0.5f});
    CHECK(steer.Turn() == doctest::Approx(kTurnSpeed * 0.5f));
    steer.Update({0, 0, true, false, 0.0f});
    steer.Update({1.0f, 0, false, false, 0.5f});
    CHECK(steer.Turn() == doctest::Approx(kTurnSpeed));
    steer.Update({0, 0, false, true, 0.0f});
    CHECK(steer.Turn() == doctest::Approx(0));
    CHECK(steer.Tilt() == doctest::Approx(0));
}

TEST_CASE("HoldModeLabel: names the mode") {
    CHECK(std::string(HoldModeLabel(HoldMode::Zoom)).find("Zoom") != std::string::npos);
    CHECK(std::string(HoldModeLabel(HoldMode::Tilt)).find("Tilt") != std::string::npos);
    CHECK(std::string(HoldModeLabel(HoldMode::Resize)).find("Resize") != std::string::npos);
}
