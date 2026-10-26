#pragma once

// Game-independent logic of the controller shortcut that brings the browser
// in front of the player. Unit tested on the host (see tests/shortcut_test.cpp).

#include "browser_logic.hpp"

namespace QuestBrowser::Shortcut {

// Hold time of the shortcut button, in seconds.
constexpr float kHoldSeconds = 0.6f;

// Not while a song plays: the window would cover the notes.
inline bool ShortcutAllowed(Logic::Context context, bool enabledInSettings) {
    return enabledInSettings && context != Logic::Context::SongPlaying;
}

// browserEnabled: the "Enable browser" setting. Nothing to reset while it is off.
inline bool ShortcutAllowed(Logic::Context context, bool enabledInSettings, bool browserEnabled) {
    return browserEnabled && ShortcutAllowed(context, enabledInSettings);
}

// Fires once when a button is held for holdSeconds. It fires again only after a release.
// A hold that starts while not allowed must be released before it can fire.
class HoldDetector {
   public:
    explicit HoldDetector(float holdSeconds = kHoldSeconds) : holdSeconds(holdSeconds) {}

    bool Update(bool pressed, float deltaSeconds, bool allowed = true) {
        if (!pressed) {
            held = 0.0f;
            fired = false;
            blocked = false;
            return false;
        }
        if (!allowed) blocked = true;
        if (blocked || fired) return false;
        held += deltaSeconds;
        if (held < holdSeconds) return false;
        fired = true;
        return true;
    }

   private:
    float holdSeconds;
    float held = 0.0f;
    bool fired = false;
    bool blocked = false;
};

}  // namespace QuestBrowser::Shortcut
