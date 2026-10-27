#pragma once

// Saved pages. Game-independent, unit tested on the host (see tests/bookmarks_test.cpp).

#include <algorithm>
#include <string>
#include <vector>

namespace QuestBrowser::Bookmarks {

constexpr std::size_t kMax = 100;

struct Bookmark {
    std::string url;
    std::string title;
    bool operator==(Bookmark const&) const = default;
};

// Puts the page on top. A page that is already saved moves to the top with the new title.
inline void Add(std::vector<Bookmark>& list, std::string const& url, std::string const& title) {
    if (url.empty() || url == "about:blank") return;
    std::erase_if(list, [&](Bookmark const& b) { return b.url == url; });
    list.insert(list.begin(), {url, title});
    if (list.size() > kMax) list.resize(kMax);
}

inline std::string const& Label(Bookmark const& b) { return b.title.empty() ? b.url : b.title; }

}  // namespace QuestBrowser::Bookmarks
