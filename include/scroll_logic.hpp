#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

// Scrolling of a panel whose rows do not all fit in the window. The first `pinned` rows
// (the title) always show; the other rows show from an offset, as many as fit.
namespace QuestBrowser::Scroll {

struct Range {
    std::size_t first = 0;  // first scrolled row that shows
    std::size_t end = 0;    // one past the last row that shows
    bool moreAbove = false;
    bool moreBelow = false;
};

// `heights` are the row heights, `spacing` is the gap between two rows, `available` is
// the height of the panel. `offset` is the number of scrolled rows above the view; it
// stops when the last row shows.
inline Range Visible(std::vector<float> const& heights, std::size_t pinned, float spacing, float available,
                     std::size_t offset) {
    std::size_t n = heights.size();
    pinned = std::min(pinned, n);
    float used = 0.0f;
    for (std::size_t i = 0; i < pinned; ++i) used += heights[i] + spacing;
    float room = available - used + spacing;  // no gap after the last row

    // The smallest first row such that all rows below it fit.
    std::size_t lowest = n;
    float tail = 0.0f;
    while (lowest > pinned && tail + heights[lowest - 1] + spacing <= room + 1e-4f) {
        tail += heights[lowest - 1] + spacing;
        --lowest;
    }
    if (lowest == n && n > pinned) lowest = n - 1;  // at least one row shows

    Range r;
    r.first = std::min(pinned + offset, lowest);
    r.end = r.first;
    float rows = 0.0f;
    while (r.end < n && (r.end == r.first || rows + heights[r.end] + spacing <= room + 1e-4f)) {
        rows += heights[r.end] + spacing;
        ++r.end;
    }
    r.moreAbove = r.first > pinned;
    r.moreBelow = r.end < n;
    return r;
}

inline std::size_t Offset(Range const& r, std::size_t pinned) { return r.first > pinned ? r.first - pinned : 0; }

inline std::string Hint(Range const& r) {
    if (r.moreAbove && r.moreBelow) return "stick: more";
    if (r.moreBelow) return "stick down: more";
    if (r.moreAbove) return "stick up: more";
    return "";
}

}  // namespace QuestBrowser::Scroll
