#include <doctest/doctest.h>

#include "shortcut_logic.hpp"

using namespace QuestBrowser::Shortcut;

TEST_CASE("HoldDetector fires once after the hold time") {
    HoldDetector hold(0.6f);
    CHECK_FALSE(hold.Update(true, 0.3f));
    CHECK_FALSE(hold.Update(true, 0.2f));  // 0.5 s
    CHECK(hold.Update(true, 0.2f));        // 0.7 s: fire
    CHECK_FALSE(hold.Update(true, 1.0f));  // still held: no repeat
    CHECK_FALSE(hold.Update(false, 0.1f));
    CHECK_FALSE(hold.Update(true, 0.5f));
    CHECK(hold.Update(true, 0.2f));        // a new hold fires again
}

TEST_CASE("HoldDetector: a short press does nothing") {
    HoldDetector hold(0.6f);
    CHECK_FALSE(hold.Update(true, 0.4f));
    CHECK_FALSE(hold.Update(false, 0.1f));
    CHECK_FALSE(hold.Update(true, 0.4f));  // timer started again from zero
    CHECK_FALSE(hold.Update(false, 0.1f));
}

TEST_CASE("HoldDetector: a hold that starts while not allowed does not fire") {
    HoldDetector hold(0.6f);
    CHECK_FALSE(hold.Update(true, 0.5f, false));  // song playing
    CHECK_FALSE(hold.Update(true, 0.5f, true));   // pause while still holding: must release first
    CHECK_FALSE(hold.Update(true, 0.5f, true));
    CHECK_FALSE(hold.Update(false, 0.1f, true));
    CHECK_FALSE(hold.Update(true, 0.4f, true));
    CHECK(hold.Update(true, 0.3f, true));
}

TEST_CASE("ShortcutAllowed: menu and pause yes, playing no, off in settings no") {
    using QuestBrowser::Logic::Context;
    CHECK(ShortcutAllowed(Context::Menu, true));
    CHECK(ShortcutAllowed(Context::SongPaused, true));
    CHECK_FALSE(ShortcutAllowed(Context::SongPlaying, true));
    CHECK_FALSE(ShortcutAllowed(Context::Menu, false));
}

TEST_CASE("ShortcutAllowed: never while the browser is disabled") {
    using QuestBrowser::Logic::Context;
    CHECK_FALSE(ShortcutAllowed(Context::Menu, true, false));
    CHECK(ShortcutAllowed(Context::Menu, true, true));
}
