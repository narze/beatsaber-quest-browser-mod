#include <doctest/doctest.h>

#include "browser_logic.hpp"
#include "songs_logic.hpp"

using namespace QuestBrowser::Songs;

TEST_CASE("RefreshGate: nothing installed, no reload") {
    RefreshGate gate;
    CHECK_FALSE(gate.Take(true, false));
}

TEST_CASE("RefreshGate: reloads once in the menu after an install") {
    RefreshGate gate;
    gate.Installed(1);
    CHECK(gate.Take(true, false));
    CHECK_FALSE(gate.Take(true, false));
}

TEST_CASE("RefreshGate: waits for the menu") {
    RefreshGate gate;
    gate.Installed(2);
    CHECK_FALSE(gate.Take(false, false));
    CHECK(gate.Take(true, false));
}

TEST_CASE("RefreshGate: waits while SongCore is already reloading") {
    RefreshGate gate;
    gate.Installed(1);
    CHECK_FALSE(gate.Take(true, true));
    CHECK(gate.Take(true, false));
}

TEST_CASE("RefreshGate: zero or negative counts do not cause a reload") {
    RefreshGate gate;
    gate.Installed(0);
    gate.Installed(-1);
    CHECK_FALSE(gate.Take(true, false));
}

TEST_CASE("MapSites: the sites in the menu, in order") {
    REQUIRE(kMapSites.size() == 3);
    CHECK(kMapSites[0].name == "BeatSaver");
    CHECK(kMapSites[1].name == "BeastSaber");
    CHECK(kMapSites[2].name == "BeatLeader");
}

TEST_CASE("MapSites: each URL is https and loads as is") {
    for (auto const& site : kMapSites) {
        CAPTURE(site.name);
        CHECK(site.url.starts_with("https://"));
        CHECK(QuestBrowser::Logic::NormalizeUrl(site.url) == site.url);
    }
}
