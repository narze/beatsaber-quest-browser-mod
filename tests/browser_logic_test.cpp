#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "browser_logic.hpp"

using namespace QuestBrowser::Logic;

TEST_CASE("NormalizeUrl keeps full URLs") {
    CHECK(NormalizeUrl("https://example.com/a?b=c") == "https://example.com/a?b=c");
    CHECK(NormalizeUrl("http://example.com") == "http://example.com");
    CHECK(NormalizeUrl("about:blank") == "about:blank");
    CHECK(NormalizeUrl("file:///sdcard/index.html") == "file:///sdcard/index.html");
}

TEST_CASE("NormalizeUrl adds https to bare domains") {
    CHECK(NormalizeUrl("example.com") == "https://example.com");
    CHECK(NormalizeUrl("www.youtube.com/watch?v=1") == "https://www.youtube.com/watch?v=1");
    CHECK(NormalizeUrl("localhost:8080") == "https://localhost:8080");
    CHECK(NormalizeUrl("192.168.1.10") == "https://192.168.1.10");
}

TEST_CASE("NormalizeUrl trims whitespace") {
    CHECK(NormalizeUrl("  example.com \n") == "https://example.com");
}

TEST_CASE("NormalizeUrl turns other text into a search") {
    CHECK(NormalizeUrl("beat saber mods") == "https://duckduckgo.com/?q=beat+saber+mods");
    CHECK(NormalizeUrl("c++ & you") == "https://duckduckgo.com/?q=c%2B%2B+%26+you");
    CHECK(NormalizeUrl("hello") == "https://duckduckgo.com/?q=hello");
}

TEST_CASE("NormalizeUrl gives the home page for empty input") {
    CHECK(NormalizeUrl("") == kHomeUrl);
    CHECK(NormalizeUrl("   ") == kHomeUrl);
}

TEST_CASE("LocalToPixel maps a centered rect to top-left pixel origin") {
    // Rect is 100 x 50 units centered on its pivot. Texture is 1000 x 500 px.
    Rect rect{-50.0f, -25.0f, 100.0f, 50.0f};

    auto topLeft = LocalToPixel(rect, -50.0f, 25.0f, 1000, 500);
    REQUIRE(topLeft.has_value());
    CHECK(topLeft->x == doctest::Approx(0.0f));
    CHECK(topLeft->y == doctest::Approx(0.0f));

    auto center = LocalToPixel(rect, 0.0f, 0.0f, 1000, 500);
    REQUIRE(center.has_value());
    CHECK(center->x == doctest::Approx(500.0f));
    CHECK(center->y == doctest::Approx(250.0f));

    auto bottomRight = LocalToPixel(rect, 50.0f, -25.0f, 1000, 500);
    REQUIRE(bottomRight.has_value());
    CHECK(bottomRight->x == doctest::Approx(1000.0f));
    CHECK(bottomRight->y == doctest::Approx(500.0f));
}

TEST_CASE("LocalToPixel rejects points outside the rect") {
    Rect rect{-50.0f, -25.0f, 100.0f, 50.0f};
    CHECK_FALSE(LocalToPixel(rect, -51.0f, 0.0f, 1000, 500).has_value());
    CHECK_FALSE(LocalToPixel(rect, 0.0f, 26.0f, 1000, 500).has_value());
}

TEST_CASE("LocalToPixel rejects a degenerate rect") {
    Rect rect{0.0f, 0.0f, 0.0f, 10.0f};
    CHECK_FALSE(LocalToPixel(rect, 0.0f, 5.0f, 100, 100).has_value());
}

TEST_CASE("ThumbstickToScroll applies a dead zone and inverts the Y axis") {
    CHECK(ThumbstickToScroll(0.1f) == doctest::Approx(0.0f));
    CHECK(ThumbstickToScroll(-0.15f) == doctest::Approx(0.0f));
    // Push up (positive) scrolls the page up, which is a positive wheel value.
    CHECK(ThumbstickToScroll(1.0f) > 0.0f);
    CHECK(ThumbstickToScroll(-1.0f) < 0.0f);
    CHECK(ThumbstickToScroll(1.0f) == doctest::Approx(-ThumbstickToScroll(-1.0f)));
}

TEST_CASE("PointerTracker emits down, move, and up events") {
    PointerTracker tracker;
    Pixel p{10.0f, 20.0f};

    CHECK(tracker.Update(false, p) == PointerAction::None);
    CHECK(tracker.Update(true, p) == PointerAction::Down);
    CHECK(tracker.Update(true, p) == PointerAction::None);
    CHECK(tracker.Update(true, Pixel{12.0f, 20.0f}) == PointerAction::Move);
    CHECK(tracker.Update(false, Pixel{12.0f, 20.0f}) == PointerAction::Up);
    CHECK(tracker.Update(false, p) == PointerAction::None);
}

TEST_CASE("PointerTracker cancels a press when the pointer leaves") {
    PointerTracker tracker;
    CHECK(tracker.Update(true, Pixel{1.0f, 1.0f}) == PointerAction::Down);
    CHECK(tracker.Update(true, std::nullopt) == PointerAction::Cancel);
    // A press that starts outside the page does not become a touch.
    CHECK(tracker.Update(true, Pixel{1.0f, 1.0f}) == PointerAction::None);
    CHECK(tracker.Update(false, Pixel{1.0f, 1.0f}) == PointerAction::None);
    CHECK(tracker.Update(true, Pixel{1.0f, 1.0f}) == PointerAction::Down);
}
