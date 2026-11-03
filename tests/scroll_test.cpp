#include <doctest/doctest.h>

#include "scroll_logic.hpp"

#include <vector>

using namespace QuestBrowser::Scroll;

// The menu: a title of 9 and seven rows of 10, with 1 between them.
std::vector<float> const kMenu{9, 10, 10, 10, 10, 10, 10, 10};

TEST_CASE("Visible: all rows show when they fit") {
    auto v = Visible(kMenu, 1, 1, 100, 0);
    CHECK(v.first == 1);
    CHECK(v.end == 8);
    CHECK_FALSE(v.moreAbove);
    CHECK_FALSE(v.moreBelow);
}

TEST_CASE("Visible: rows that do not fit are cut from the bottom") {
    // 9 + 1 + 6 * 10 + 5 = 75 fits in 81; one more row needs 86.
    auto v = Visible(kMenu, 1, 1, 81, 0);
    CHECK(v.first == 1);
    CHECK(v.end == 7);
    CHECK_FALSE(v.moreAbove);
    CHECK(v.moreBelow);
}

TEST_CASE("Visible: a scrolled list shows the last rows") {
    auto v = Visible(kMenu, 1, 1, 81, 1);
    CHECK(v.first == 2);
    CHECK(v.end == 8);
    CHECK(v.moreAbove);
    CHECK_FALSE(v.moreBelow);
}

TEST_CASE("Visible: the offset stops when the last row shows") {
    auto v = Visible(kMenu, 1, 1, 81, 5);
    CHECK(v.first == 2);
    CHECK(v.end == 8);
}

TEST_CASE("Visible: at least one row shows, even when nothing fits") {
    auto v = Visible(kMenu, 1, 1, 5, 0);
    CHECK(v.first == 1);
    CHECK(v.end == 2);
    CHECK(v.moreBelow);
}

TEST_CASE("Visible: no pinned rows") {
    std::vector<float> rows{10, 10, 10};
    auto v = Visible(rows, 0, 0, 20, 0);
    CHECK(v.first == 0);
    CHECK(v.end == 2);
    CHECK(v.moreBelow);
}

TEST_CASE("Visible: an empty list") {
    auto v = Visible({}, 0, 1, 81, 3);
    CHECK(v.first == 0);
    CHECK(v.end == 0);
    CHECK_FALSE(v.moreAbove);
    CHECK_FALSE(v.moreBelow);
}

TEST_CASE("Offset: the scroll offset is the first row after the pinned rows") {
    CHECK(Offset(Visible(kMenu, 1, 1, 81, 0), 1) == 0);
    CHECK(Offset(Visible(kMenu, 1, 1, 81, 9), 1) == 1);
}

TEST_CASE("Hint: tells where more rows are") {
    CHECK(Hint(Visible(kMenu, 1, 1, 100, 0)) == "");
    CHECK(Hint(Visible(kMenu, 1, 1, 81, 0)) == "stick down: more");
    CHECK(Hint(Visible(kMenu, 1, 1, 81, 1)) == "stick up: more");
    CHECK(Hint(Visible(kMenu, 1, 1, 50, 1)) == "stick: more");
}
