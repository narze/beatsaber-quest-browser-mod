#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "browser_logic.hpp"

// Smooth scrolling of a panel whose rows do not all fit in the window. Units are the
// panel content units (a row is 10). The offset is how far the rows moved up.
namespace QuestBrowser::Scroll {

// Rows per second at full stick.
constexpr float kRowsPerSecond = 4.0f;
constexpr float kRowHeight = 10.0f;
constexpr float kDeadZone = 0.2f;

inline float ContentHeight(std::vector<float> const& heights, float spacing) {
    if (heights.empty()) return 0.0f;
    float total = spacing * static_cast<float>(heights.size() - 1);
    for (float h : heights) total += h;
    return total;
}

inline float MaxOffset(float contentHeight, float available) { return std::max(0.0f, contentHeight - available); }

// Stick down shows the rows below.
inline float StepOffset(float offset, float stickY, float deltaSeconds, float maxOffset) {
    float speed = Logic::ThumbstickToScroll(stickY, kDeadZone, kRowsPerSecond * kRowHeight);
    return std::clamp(offset - speed * deltaSeconds, 0.0f, maxOffset);
}

// Shift of the content center from the panel center: rows that fit stay in the
// middle; rows that do not fit start at the top and move up by the offset.
inline float ContentShift(float contentHeight, float available, float offset) {
    if (contentHeight <= available) return 0.0f;
    return (available - contentHeight) * 0.5f + std::clamp(offset, 0.0f, contentHeight - available);
}

// True when a row (top measured from the content top) is fully in the view.
inline bool RowShown(float top, float height, float offset, float available) {
    constexpr float kSlack = 0.01f;
    return top >= offset - kSlack && top + height <= offset + available + kSlack;
}

inline std::string Hint(float offset, float maxOffset) {
    constexpr float kSlack = 0.01f;
    if (maxOffset <= kSlack) return "";
    bool above = offset > kSlack;
    bool below = offset < maxOffset - kSlack;
    if (above && below) return "stick: more";
    return below ? "stick down: more" : "stick up: more";
}

}  // namespace QuestBrowser::Scroll
