#pragma once

// Controller rules for the window: grab it with the grip, push and pull it with the
// thumbstick, zoom with the left thumbstick. Game-independent, unit tested on the host
// (see tests/controls_test.cpp).

#include <algorithm>
#include <cmath>

namespace QuestBrowser::Controls {

// A button with an analog value (trigger, grip). Two levels, so that a half press does not flicker.
class HeldButton {
   public:
    bool Update(float value) {
        held = held ? value > kRelease : value > kPress;
        return held;
    }
    bool Held() const { return held; }

   private:
    static constexpr float kPress = 0.75f;
    static constexpr float kRelease = 0.35f;
    bool held = false;
};

// Grab distance along the controller, in meters.
constexpr float kGrabDepthSpeed = 3.0f;  // meters per second at full stick
constexpr float kMinGrabDepth = 0.3f;
constexpr float kMaxGrabDepth = 8.0f;
constexpr float kStickDeadZone = 0.2f;

inline float GrabDepth(float distance, float stickY, float deltaSeconds) {
    if (std::fabs(stickY) < kStickDeadZone) return distance;
    return std::clamp(distance + kGrabDepthSpeed * stickY * deltaSeconds, kMinGrabDepth, kMaxGrabDepth);
}

// Turns a held thumbstick into steps (+1 up, -1 down): one at once, then one every kRepeatSeconds.
constexpr float kRepeatSeconds = 0.3f;

class StickStepper {
   public:
    int Update(float stickY, float deltaSeconds) {
        int side = stickY > kOn ? 1 : stickY < -kOn ? -1 : 0;
        if (side == 0) {
            if (std::fabs(stickY) < kOff) held = 0;
            return 0;
        }
        if (side != held) {
            held = side;
            timer = kRepeatSeconds;
            return side;
        }
        timer -= deltaSeconds;
        if (timer > 0.0f) return 0;
        timer += kRepeatSeconds;
        return side;
    }

   private:
    static constexpr float kOn = 0.6f;
    static constexpr float kOff = 0.3f;
    int held = 0;
    float timer = 0.0f;
};

}  // namespace QuestBrowser::Controls
