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
constexpr float kMaxGrabDepth = 30.0f;
constexpr float kStickDeadZone = 0.2f;

inline float GrabDepth(float distance, float stickY, float deltaSeconds) {
    if (std::fabs(stickY) < kStickDeadZone) return distance;
    return std::clamp(distance + kGrabDepthSpeed * stickY * deltaSeconds, kMinGrabDepth, kMaxGrabDepth);
}

// While the window is grabbed, thumbstick X turns it around its own center (degrees).
constexpr float kTurnSpeed = 45.0f;  // degrees per second at full stick

inline float TurnStep(float yawDegrees, float stickX, float deltaSeconds) {
    if (std::fabs(stickX) < kStickDeadZone) return yawDegrees;
    return yawDegrees + kTurnSpeed * stickX * deltaSeconds;
}

// While the window is held, a trigger press changes what stick Y does.
enum class HoldMode { Zoom, Tilt, Resize };

inline char const* HoldModeLabel(HoldMode mode) {
    switch (mode) {
        case HoldMode::Tilt: return "Tilt: stick up / down tilts";
        case HoldMode::Resize: return "Resize: stick up / down = bigger / smaller";
        default: return "Zoom: stick up / down = farther / closer";
    }
}

constexpr float kTiltSpeed = 45.0f;   // degrees per second at full stick
constexpr float kMaxTilt = 80.0f;     // degrees from facing the player
constexpr float kResizeRate = 0.7f;   // about 2x per second at full stick

struct HoldInput {
    float stickX, stickY;
    bool triggerPressed;  // a trigger that does not hold the window went down
    bool stickClicked;    // the stick of the holding hand went down
    float deltaSeconds;
};

struct HoldOutput {
    float depthStick = 0.0f;  // zoom mode: push (+) and pull (-)
    float sizeFactor = 1.0f;  // resize mode: multiply the window size
};

// Turn and tilt are offsets from facing the player, in degrees. Tilt is added to
// the pitch: negative tilts the top away.
class HoldSteer {
   public:
    void Begin(float turn, float tilt) {
        mode = HoldMode::Zoom;
        turnDegrees = turn;
        tiltDegrees = std::clamp(tilt, -kMaxTilt, kMaxTilt);
    }

    HoldOutput Update(HoldInput in) {
        if (in.triggerPressed) mode = mode == HoldMode::Zoom ? HoldMode::Tilt : mode == HoldMode::Tilt ? HoldMode::Resize : HoldMode::Zoom;
        if (in.stickClicked) turnDegrees = tiltDegrees = 0.0f;
        turnDegrees = TurnStep(turnDegrees, in.stickX, in.deltaSeconds);
        HoldOutput out;
        bool moved = std::fabs(in.stickY) >= kStickDeadZone;
        switch (mode) {
            case HoldMode::Zoom:
                out.depthStick = in.stickY;
                break;
            case HoldMode::Tilt:
                if (moved) tiltDegrees = std::clamp(tiltDegrees - kTiltSpeed * in.stickY * in.deltaSeconds, -kMaxTilt, kMaxTilt);
                break;
            case HoldMode::Resize:
                if (moved) out.sizeFactor = std::exp(kResizeRate * in.stickY * in.deltaSeconds);
                break;
        }
        return out;
    }

    HoldMode Mode() const { return mode; }
    float Turn() const { return turnDegrees; }
    float Tilt() const { return tiltDegrees; }

   private:
    HoldMode mode = HoldMode::Zoom;
    float turnDegrees = 0.0f;
    float tiltDegrees = 0.0f;
};

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
