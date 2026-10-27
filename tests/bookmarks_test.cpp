#include <doctest/doctest.h>

#include "bookmarks_logic.hpp"

using namespace QuestBrowser::Bookmarks;

TEST_CASE("Add: the new bookmark goes on top") {
    std::vector<Bookmark> list;
    Add(list, "https://a.com/", "A");
    Add(list, "https://b.com/", "B");
    REQUIRE(list.size() == 2);
    CHECK(list[0] == Bookmark{"https://b.com/", "B"});
    CHECK(list[1] == Bookmark{"https://a.com/", "A"});
}

TEST_CASE("Add: the same page again moves it to the top with the new title, no copy") {
    std::vector<Bookmark> list{{"https://b.com/", "B"}, {"https://a.com/", "A"}};
    Add(list, "https://a.com/", "A new");
    REQUIRE(list.size() == 2);
    CHECK(list[0] == Bookmark{"https://a.com/", "A new"});
}

TEST_CASE("Add: no bookmark for an empty or blank page") {
    std::vector<Bookmark> list;
    Add(list, "", "x");
    Add(list, "about:blank", "x");
    CHECK(list.empty());
}

TEST_CASE("Add: keeps at most kMax bookmarks, the oldest go") {
    std::vector<Bookmark> list;
    for (std::size_t i = 0; i <= kMax; ++i) Add(list, "https://a.com/" + std::to_string(i), "");
    CHECK(list.size() == kMax);
    CHECK(list.front().url == "https://a.com/" + std::to_string(kMax));
    CHECK(list.back().url == "https://a.com/1");
}

TEST_CASE("Label: the title, or the address when there is no title") {
    CHECK(Label({"https://a.com/", "A"}) == "A");
    CHECK(Label({"https://a.com/", ""}) == "https://a.com/");
}
