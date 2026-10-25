#include <doctest/doctest.h>

#include <string>

#include "keybind_logic.hpp"

using namespace QuestBrowser::Keybind;

TEST_CASE("Presets include Off, X + Y, and single stick clicks") {
    CHECK(Find("Off").buttons == 0);
    CHECK(Find("X + Y").buttons == (kX | kY));
    CHECK(Find("Left stick click").buttons == kLeftStick);
    CHECK(Find("Both stick clicks").buttons == (kLeftStick | kRightStick));
}

TEST_CASE("Default is X + Y, and an unknown saved name falls back to it") {
    CHECK(std::string(kDefaultName) == "X + Y");
    CHECK(Find("no such binding").name == kDefaultName);
}

TEST_CASE("Names are unique and every preset can be found again by name") {
    for (auto const& preset : Presets()) {
        CAPTURE(preset.name);
        CHECK(Find(preset.name).buttons == preset.buttons);
    }
}

TEST_CASE("IsHeld needs every button of the combo, extra buttons are fine") {
    auto xy = Find("X + Y");
    CHECK_FALSE(IsHeld(xy, kX));
    CHECK_FALSE(IsHeld(xy, kY));
    CHECK(IsHeld(xy, kX | kY));
    CHECK(IsHeld(xy, kX | kY | kA));
}

TEST_CASE("IsHeld is never true for Off") {
    CHECK_FALSE(IsHeld(Find("Off"), kX | kY | kA | kB | kLeftStick | kRightStick));
}
