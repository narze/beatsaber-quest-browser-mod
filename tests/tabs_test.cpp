#include <doctest/doctest.h>

#include "tabs_logic.hpp"

using namespace QuestBrowser::Tabs;

TEST_CASE("Restore: saved tabs come back with the saved active tab") {
    auto s = Restore({"https://a.example/", "https://b.example/"}, 1, "https://home/");
    CHECK(s.urls == std::vector<std::string>{"https://a.example/", "https://b.example/"});
    CHECK(s.active == 1);
}

TEST_CASE("Restore: no saved tabs opens one home tab") {
    auto s = Restore({}, 0, "https://home/");
    CHECK(s.urls == std::vector<std::string>{"https://home/"});
    CHECK(s.active == 0);
}

TEST_CASE("Restore: drops empty URLs, keeps at most kMaxTabs, clamps the active tab") {
    std::vector<std::string> many;
    for (int i = 0; i < 20; i++) many.push_back("https://t" + std::to_string(i) + "/");
    many.insert(many.begin() + 1, "");
    auto s = Restore(many, 99, "https://home/");
    CHECK(static_cast<int>(s.urls.size()) == kMaxTabs);
    CHECK(s.urls[1] == "https://t1/");
    CHECK(s.active == kMaxTabs - 1);
    CHECK(Restore({"https://a/"}, -3, "h").active == 0);
}

TEST_CASE("ActiveAfterClose: closing the active tab selects the next one, or the previous at the end") {
    CHECK(ActiveAfterClose(3, 1, 1) == 1);  // [a, B, c] close B -> c (now index 1)
    CHECK(ActiveAfterClose(3, 2, 2) == 1);  // close the last, active -> previous
    CHECK(ActiveAfterClose(1, 0, 0) == 0);  // last tab closed: a new home tab at 0
}

TEST_CASE("ActiveAfterClose: closing another tab keeps the same page active") {
    CHECK(ActiveAfterClose(3, 2, 0) == 1);  // [x, b, C] close x -> C moves to index 1
    CHECK(ActiveAfterClose(3, 0, 2) == 0);
}

TEST_CASE("VisibleRange shows up to `slots` tabs and keeps the active one in view") {
    CHECK(VisibleRange(3, 0, 4) == Range{0, 3});
    CHECK(VisibleRange(8, 0, 4) == Range{0, 4});
    CHECK(VisibleRange(8, 3, 4) == Range{0, 4});
    CHECK(VisibleRange(8, 4, 4) == Range{1, 5});  // active at the right edge
    CHECK(VisibleRange(8, 7, 4) == Range{4, 8});
}

TEST_CASE("CanAddTab stops at kMaxTabs") {
    CHECK(CanAddTab(kMaxTabs - 1));
    CHECK_FALSE(CanAddTab(kMaxTabs));
}

TEST_CASE("Label shortens a tab title, and uses the host when there is no title") {
    CHECK(Label("Example Domain", "https://example.com/", 12) == "Example Do..");
    CHECK(Label("", "https://www.youtube.com/watch?v=1", 20) == "www.youtube.com");
    CHECK(Label("Short", "https://x/", 12) == "Short");
}
