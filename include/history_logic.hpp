#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace QuestBrowser::Logic {
inline void RememberUrl(std::vector<std::string>& urls, std::string const& url) {
    if (url.empty() || url == "about:blank") return;
    std::erase(urls, url);
    urls.insert(urls.begin(), url);
    if (urls.size() > 100) urls.resize(100);
}
}
