#pragma once

// Controller keybind presets for "reset browser position".
// Game-independent, unit tested on the host (see tests/keybind_test.cpp).

#include <cstdint>
#include <string_view>
#include <vector>

namespace QuestBrowser::Keybind {

// One bit per controller button.
enum Button : uint32_t {
    kX = 1u << 0,
    kY = 1u << 1,
    kA = 1u << 2,
    kB = 1u << 3,
    kLeftStick = 1u << 4,   // left thumbstick click
    kRightStick = 1u << 5,  // right thumbstick click
};

struct Preset {
    std::string_view name;  // shown in the settings and saved in the config
    uint32_t buttons;       // 0 = off
};

constexpr std::string_view kDefaultName = "X + Y";

inline std::vector<Preset> const& Presets() {
    static std::vector<Preset> const presets = {
        {"Off", 0},
        {"X + Y", kX | kY},
        {"A + B", kA | kB},
        {"X + A", kX | kA},
        {"Y + B", kY | kB},
        {"Left stick click", kLeftStick},
        {"Right stick click", kRightStick},
        {"Both stick clicks", kLeftStick | kRightStick},
    };
    return presets;
}

// The preset with this name, else the default.
inline Preset Find(std::string_view name) {
    for (auto const& p : Presets())
        if (p.name == name) return p;
    for (auto const& p : Presets())
        if (p.name == kDefaultName) return p;
    return {};
}

// True when every button of the preset is pressed. Extra buttons do not matter.
inline bool IsHeld(Preset const& preset, uint32_t pressed) {
    return preset.buttons != 0 && (pressed & preset.buttons) == preset.buttons;
}

}  // namespace QuestBrowser::Keybind
