#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "browser_logic.hpp"

using namespace QuestBrowser::Logic;

TEST_CASE("Paused game selects browser targets ahead of overlapping game UI") {
    CHECK(BrowserRaycastPriority(Context::SongPaused, true, false) == -1);
    CHECK(BrowserRaycastPriority(Context::SongPaused, false, true) == 1);
    CHECK(BrowserRaycastPriority(Context::SongPaused, true, true) == 0);
    CHECK(BrowserRaycastPriority(Context::Menu, true, false) == 0);
    CHECK(BrowserRaycastPriority(Context::SongPlaying, true, false) == 0);
}

TEST_CASE("Curve levels control strength and scale with window width") {
    CHECK(CurveRadius(150.0f, 3) == doctest::Approx(225.0f));
    CHECK(CurveRadius(150.0f, 1) > CurveRadius(150.0f, 3));
    CHECK(CurveRadius(150.0f, 5) < CurveRadius(150.0f, 3));
    CHECK(CurveRadius(150.0f, 10) < CurveRadius(150.0f, 5));
    CHECK(CurveRadius(300.0f, 3) == doctest::Approx(450.0f));
    CHECK(CurveRadius(150.0f, -1) == doctest::Approx(CurveRadius(150.0f, 1)));
    CHECK(CurveRadius(150.0f, 100) == doctest::Approx(CurveRadius(150.0f, 10)));
}

TEST_CASE("Curved page coordinates recover the unwrapped horizontal position") {
    auto projected = [](float x) { return 200.0f * std::sin(x / 200.0f); };
    CHECK(UnwrapCurveX(projected(-70.0f), -75.0f, 75.0f, projected) == doctest::Approx(-70.0f).epsilon(0.0001));
    CHECK(UnwrapCurveX(projected(0.0f), -75.0f, 75.0f, projected) == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(UnwrapCurveX(projected(70.0f), -75.0f, 75.0f, projected) == doctest::Approx(70.0f).epsilon(0.0001));
}

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

TEST_CASE("ActiveCounter reports a change only on 0 <-> 1 transitions") {
    ActiveCounter counter;
    CHECK(counter.Add() == std::optional<bool>(true));
    CHECK(counter.Add() == std::nullopt);
    CHECK(counter.Remove() == std::nullopt);
    CHECK(counter.Remove() == std::optional<bool>(false));
    CHECK_FALSE(counter.IsActive());
}

TEST_CASE("ActiveCounter ignores extra removes") {
    ActiveCounter counter;
    CHECK(counter.Remove() == std::nullopt);
    CHECK(counter.Add() == std::optional<bool>(true));
    CHECK(counter.IsActive());
}

TEST_CASE("FrameRateFor: 60 fps when interactive, 30 fps while a song is playing") {
    // Measured on Quest 3S: a 1280x720 frame costs under 2 ms in total (draw, copy, upload).
    CHECK(FrameRateFor(true) == 60);
    CHECK(FrameRateFor(false) == 30);
}

TEST_CASE("ScreenStateFor: menu is interactive with a handle") {
    auto s = ScreenStateFor(Context::Menu, true, true);
    CHECK(s.visible);
    CHECK(s.interactive);
    CHECK(s.showHandle);
    CHECK(s.pose == PoseSlot::Menu);
}

TEST_CASE("ScreenStateFor: playing a song shows the page but locks it") {
    auto s = ScreenStateFor(Context::SongPlaying, true, true);
    CHECK(s.visible);
    CHECK_FALSE(s.interactive);
    CHECK_FALSE(s.showHandle);
    CHECK(s.pose == PoseSlot::Song);
}

TEST_CASE("ScreenStateFor: pause unlocks the song screen") {
    auto s = ScreenStateFor(Context::SongPaused, true, true);
    CHECK(s.visible);
    CHECK(s.interactive);
    CHECK(s.showHandle);
    CHECK(s.pose == PoseSlot::Song);
}

TEST_CASE("ScreenStateFor: hidden by the user means hidden everywhere") {
    CHECK_FALSE(ScreenStateFor(Context::Menu, false, true).visible);
    CHECK_FALSE(ScreenStateFor(Context::SongPlaying, false, true).visible);
    CHECK_FALSE(ScreenStateFor(Context::SongPaused, false, true).visible);
}

TEST_CASE("ScreenStateFor: 'show during songs' off hides it only in songs") {
    CHECK(ScreenStateFor(Context::Menu, true, false).visible);
    CHECK_FALSE(ScreenStateFor(Context::SongPlaying, true, false).visible);
    CHECK_FALSE(ScreenStateFor(Context::SongPaused, true, false).visible);
}
#include "history_logic.hpp"

TEST_CASE("history revisits move to front and oldest visits are bounded") {
    std::vector<std::string> urls;
    for (int i = 0; i < 105; ++i) QuestBrowser::Logic::RememberUrl(urls, "http://server/" + std::to_string(i));
    REQUIRE(urls.size() == 100);
    CHECK(urls.back() == "http://server/5");
    QuestBrowser::Logic::RememberUrl(urls, "http://server/50");
    CHECK(urls.front() == "http://server/50");
    CHECK(urls.size() == 100);
    QuestBrowser::Logic::RememberUrl(urls, "");
    CHECK(urls.size() == 100);
}

TEST_CASE("EnabledAfterMerge: one switch replaces Enable browser and Show browser") {
    // Nothing saved: on.
    CHECK(EnabledAfterMerge(std::nullopt, std::nullopt));
    // Someone who hid the window with the old Show browser switch stays hidden.
    CHECK_FALSE(EnabledAfterMerge(std::nullopt, false));
    CHECK_FALSE(EnabledAfterMerge(true, false));
    // A disabled browser stays disabled.
    CHECK_FALSE(EnabledAfterMerge(false, true));
    CHECK(EnabledAfterMerge(true, true));
}

TEST_CASE("SlotFor: the menu has its own settings, a song (playing or paused) has its own") {
    CHECK(SlotFor(Context::Menu) == PoseSlot::Menu);
    CHECK(SlotFor(Context::SongPlaying) == PoseSlot::Song);
    CHECK(SlotFor(Context::SongPaused) == PoseSlot::Song);
}

TEST_CASE("SortingOrderFor: in the menu the browser draws over the game UI, in songs under everything") {
    // The highest game canvas in the main menu (view controllers) uses order 10.
    CHECK(SortingOrderFor(Context::Menu) > 10);
    // In songs, walls, scores, trails and the HUD (lowest canvas: -2) draw after the
    // browser; the depth occluder hides those behind it.
    CHECK(SortingOrderFor(Context::SongPlaying) < -2);
    CHECK(SortingOrderFor(Context::SongPaused) < -2);
}
