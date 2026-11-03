#include <doctest/doctest.h>

#include "scroll_logic.hpp"

#include <vector>

using namespace QuestBrowser::Scroll;

// The menu: a title of 9 and seven rows of 10, with 1 between them: 86 in total.
std::vector<float> const kMenu{9, 10, 10, 10, 10, 10, 10, 10};

TEST_CASE("ContentHeight: rows and the gaps between them") {
    CHECK(ContentHeight(kMenu, 1) == doctest::Approx(86));
    CHECK(ContentHeight({}, 1) == doctest::Approx(0));
    CHECK(ContentHeight({10}, 1) == doctest::Approx(10));
}

TEST_CASE("MaxOffset: how far the rows can scroll") {
    CHECK(MaxOffset(86, 81) == doctest::Approx(5));
    CHECK(MaxOffset(70, 81) == doctest::Approx(0));
}

TEST_CASE("StepOffset: the stick moves the rows smoothly, in proportion to time") {
    // Stick down shows the rows below.
    float down = StepOffset(0, -1, 0.1f, 50);
    CHECK(down == doctest::Approx(kRowsPerSecond * 10 * 0.1f));
    CHECK(StepOffset(0, -1, 0.05f, 50) == doctest::Approx(down / 2));
    // Stick up goes back.
    CHECK(StepOffset(20, 1, 0.1f, 50) == doctest::Approx(20 - down));
}

TEST_CASE("StepOffset: a small stick move does nothing, and the offset stays in range") {
    CHECK(StepOffset(3, -0.1f, 0.1f, 5) == doctest::Approx(3));
    CHECK(StepOffset(4, -1, 1, 5) == doctest::Approx(5));
    CHECK(StepOffset(1, 1, 1, 5) == doctest::Approx(0));
    CHECK(StepOffset(9, 0, 0.1f, 5) == doctest::Approx(5));
}

TEST_CASE("ContentShift: rows that fit stay in the middle") {
    CHECK(ContentShift(70, 81, 0) == doctest::Approx(0));
    CHECK(ContentShift(70, 81, 4) == doctest::Approx(0));
}

TEST_CASE("ContentShift: rows that do not fit start at the top and move up") {
    // Content center below the panel center, so the content top is at the panel top.
    CHECK(ContentShift(86, 81, 0) == doctest::Approx(-2.5f));
    CHECK(ContentShift(86, 81, 5) == doctest::Approx(2.5f));
}

TEST_CASE("RowShown: only rows fully in the view") {
    // Rows from the content top: 0..9, 10..20, ..., 76..86.
    CHECK(RowShown(0, 9, 0, 81));
    CHECK_FALSE(RowShown(76, 10, 0, 81));
    CHECK(RowShown(76, 10, 5, 81));
    CHECK_FALSE(RowShown(0, 9, 5, 81));
    CHECK(RowShown(10, 10, 5, 81));
}

TEST_CASE("Hint: tells where more rows are") {
    CHECK(Hint(0, 0) == "");
    CHECK(Hint(0, 5) == "stick down: more");
    CHECK(Hint(5, 5) == "stick up: more");
    CHECK(Hint(2, 5) == "stick: more");
}
