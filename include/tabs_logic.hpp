#pragma once

// Game-independent rules for browser tabs: restore, close, and the tab strip.
// Unit tested on the host (see tests/tabs_test.cpp).

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace QuestBrowser::Tabs {

// Each tab has its own WebView, which costs memory on the Quest.
constexpr int kMaxTabs = 10;
// Tab buttons in the tab strip.
constexpr int kStripSlots = 4;

struct State {
    std::vector<std::string> urls;
    int active = 0;
};

// Tabs saved last session: drops empty URLs, keeps at most kMaxTabs, clamps the
// active tab. With nothing saved, one tab with the home page.
inline State Restore(std::vector<std::string> const& saved, int active, std::string_view home) {
    State s;
    for (auto const& url : saved) {
        if (url.empty()) continue;
        if (static_cast<int>(s.urls.size()) == kMaxTabs) break;
        s.urls.push_back(url);
    }
    if (s.urls.empty()) s.urls.emplace_back(home);
    s.active = std::clamp(active, 0, static_cast<int>(s.urls.size()) - 1);
    return s;
}

// The active tab after closing tab `closed` of `count` tabs. Closing the active tab
// selects the next tab, or the previous one if it was the last. If the last tab
// closes, a new home tab opens at index 0.
inline int ActiveAfterClose(int count, int active, int closed) {
    int left = count - 1;
    if (left <= 0) return 0;
    if (closed < active) return active - 1;
    if (closed > active) return active;
    return std::min(closed, left - 1);
}

inline bool CanAddTab(int count) { return count < kMaxTabs; }

struct Range {
    int begin;
    int end;
    bool operator==(Range const&) const = default;
};

// Tabs shown in the strip: up to `slots`, scrolled so that the active tab shows.
inline Range VisibleRange(int count, int active, int slots) {
    if (count <= slots) return {0, count};
    int begin = std::clamp(active - slots + 1, 0, count - slots);
    if (active < begin) begin = active;
    return {begin, begin + slots};
}

// Text for a tab button: the page title, or the host when there is no title yet.
inline std::string Label(std::string_view title, std::string_view url, std::size_t maxLength) {
    std::string text(title);
    if (text.empty()) {
        auto start = url.find("://");
        auto host = start == std::string_view::npos ? url : url.substr(start + 3);
        text = std::string(host.substr(0, host.find_first_of("/?#")));
    }
    if (text.size() > maxLength && maxLength > 2) text = text.substr(0, maxLength - 2) + "..";
    return text;
}

}  // namespace QuestBrowser::Tabs
