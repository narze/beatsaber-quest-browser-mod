#include <doctest/doctest.h>

#include "icons_logic.hpp"
#include "toolbar_logic.hpp"

using namespace QuestBrowser;

TEST_CASE("Fader: hidden at start, fades in while pointed at") {
    Toolbar::Fader f;
    CHECK(f.Alpha() == doctest::Approx(0));
    f.Update(true, false, 0.1f);
    CHECK(f.Alpha() > 0.0f);
    for (int i = 0; i < 10; i++) f.Update(true, false, 0.1f);
    CHECK(f.Alpha() == doctest::Approx(1));
    CHECK(f.Clickable());
}

TEST_CASE("Fader: stays a moment after the pointer leaves, then fades out") {
    Toolbar::Fader f;
    for (int i = 0; i < 10; i++) f.Update(true, false, 0.1f);
    f.Update(false, false, Toolbar::kStaySeconds - 0.1f);
    CHECK(f.Alpha() == doctest::Approx(1));
    for (int i = 0; i < 20; i++) f.Update(false, false, 0.1f);
    CHECK(f.Alpha() == doctest::Approx(0));
    CHECK_FALSE(f.Clickable());
}

TEST_CASE("Fader: pinned (keyboard or panel open) keeps it shown") {
    Toolbar::Fader f;
    for (int i = 0; i < 50; i++) f.Update(false, true, 0.1f);
    CHECK(f.Alpha() == doctest::Approx(1));
}

TEST_CASE("Fader: Hide() hides at once (song playing)") {
    Toolbar::Fader f;
    for (int i = 0; i < 10; i++) f.Update(true, false, 0.1f);
    f.Hide();
    CHECK(f.Alpha() == doctest::Approx(0));
}

TEST_CASE("Icons: each icon has ink, and transparent corners") {
    for (auto kind : {Icons::Kind::Back, Icons::Kind::Forward, Icons::Kind::Reload, Icons::Kind::Menu, Icons::Kind::Tabs}) {
        CAPTURE(static_cast<int>(kind));
        auto px = Icons::Draw(kind, 64);
        REQUIRE(px.size() == 64 * 64 * 4);
        int ink = 0;
        for (std::size_t i = 3; i < px.size(); i += 4) ink += px[i] > 128;
        CHECK(ink > 64);       // something is drawn
        CHECK(ink < 64 * 32);  // but not a filled square
        CHECK(px[3] == 0);     // corner (0, 0) is transparent
    }
}

TEST_CASE("Icons: Back points left, Forward points right") {
    // Texture rows start at the bottom; check the middle row.
    auto alphaAt = [](std::vector<uint8_t> const& px, int x, int y) { return px[(y * 64 + x) * 4 + 3]; };
    auto back = Icons::Draw(Icons::Kind::Back, 64);
    auto fwd = Icons::Draw(Icons::Kind::Forward, 64);
    int leftInkBack = 0, leftInkFwd = 0;
    for (int x = 0; x < 32; x++) {
        leftInkBack += alphaAt(back, x, 32) > 128;
        leftInkFwd += alphaAt(fwd, x, 32) > 128;
    }
    CHECK(leftInkBack > 0);  // the tip of < is on the left half of the middle row
    CHECK(leftInkFwd == 0);  // > has its tip on the right
}
