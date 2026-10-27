#include <doctest/doctest.h>

#include "keyboard_logic.hpp"

using namespace QuestBrowser::Keys;

namespace {

KeyDef const& Find(std::string_view normal) {
    for (auto const& row : Layout())
        for (auto const& key : row)
            if (key.normal == normal) return key;
    FAIL("key not in layout: " << normal);
    static KeyDef none{};
    return none;
}

}  // namespace

TEST_CASE("Layout has more keys than a basic keyboard") {
    std::size_t count = 0;
    for (auto const& row : Layout()) count += row.size();
    CHECK(Layout().size() == 5);
    CHECK(count >= 60);
    for (auto name : {"Tab", "Caps", "Shift", "Ctrl", "Esc", "Left", "Right", "Up", "Down", "Del", "Enter", ".com", "Hide"}) {
        CAPTURE(name);
        CHECK(Find(name).kind != Kind::Char);
    }
}

TEST_CASE("Letters type lowercase, shift makes one uppercase letter") {
    KeyboardState kb;
    CHECK(kb.Press(Find("a")) == Output::Text("a"));
    kb.Press(Find("Shift"));
    CHECK(kb.Label(Find("a")) == "A");
    CHECK(kb.Press(Find("a")) == Output::Text("A"));
    CHECK(kb.Press(Find("a")) == Output::Text("a"));
}

TEST_CASE("Shift gives the symbol on number and punctuation keys") {
    KeyboardState kb;
    CHECK(kb.Label(Find("1")) == "1");
    kb.Press(Find("Shift"));
    CHECK(kb.Label(Find("1")) == "!");
    CHECK(kb.Press(Find("1")) == Output::Text("!"));
    kb.Press(Find("Shift"));
    CHECK(kb.Press(Find("/")) == Output::Text("?"));
}

TEST_CASE("Caps lock makes letters uppercase but not symbols, until pressed again") {
    KeyboardState kb;
    kb.Press(Find("Caps"));
    CHECK(kb.Press(Find("q")) == Output::Text("Q"));
    CHECK(kb.Press(Find("q")) == Output::Text("Q"));
    CHECK(kb.Press(Find("1")) == Output::Text("1"));
    // Shift with caps lock gives a lowercase letter.
    kb.Press(Find("Shift"));
    CHECK(kb.Press(Find("q")) == Output::Text("q"));
    kb.Press(Find("Caps"));
    CHECK(kb.Press(Find("q")) == Output::Text("q"));
}

TEST_CASE("Pressing shift twice cancels it") {
    KeyboardState kb;
    kb.Press(Find("Shift"));
    kb.Press(Find("Shift"));
    CHECK_FALSE(kb.Modifiers().shift);
    CHECK(kb.Press(Find("a")) == Output::Text("a"));
}

TEST_CASE("Ctrl with a letter sends a key with the ctrl meta state, once") {
    KeyboardState kb;
    kb.Press(Find("Ctrl"));
    CHECK(kb.Modifiers().ctrl);
    CHECK(kb.Press(Find("a")) == Output::Key(kKeyCodeA, kMetaCtrl));
    CHECK(kb.Press(Find("v")) == Output::Text("v"));
}

TEST_CASE("Special keys send Android key codes with the active modifiers") {
    KeyboardState kb;
    CHECK(kb.Press(Find("Enter")) == Output::Key(66, 0));
    CHECK(kb.Press(Find("Del")) == Output::Key(67, 0));
    CHECK(kb.Press(Find("Tab")) == Output::Key(61, 0));
    CHECK(kb.Press(Find("Esc")) == Output::Key(111, 0));
    CHECK(kb.Press(Find("Left")) == Output::Key(21, 0));
    kb.Press(Find("Shift"));
    CHECK(kb.Press(Find("Right")) == Output::Key(22, kMetaShift));
    CHECK_FALSE(kb.Modifiers().shift);
}

TEST_CASE(".com types text and Hide asks to hide") {
    KeyboardState kb;
    CHECK(kb.Press(Find(".com")) == Output::Text(".com"));
    CHECK(kb.Press(Find("Hide")).type == OutputType::Hide);
}

TEST_CASE("Space types a space") {
    KeyboardState kb;
    CHECK(kb.Press(Find("Space")) == Output::Text(" "));
}

TEST_CASE("http:// and https:// type the URL start") {
    KeyboardState kb;
    CHECK(kb.Press(Find("http://")) == Output::Text("http://"));
    CHECK(kb.Press(Find("https://")) == Output::Text("https://"));
}

TEST_CASE("No row is wider than the widest letter row, so the panel does not grow") {
    auto width = [](Row const& row) {
        float w = 0.0f;
        for (auto const& key : row) w += key.width;
        return w;
    };
    float letters = std::max(width(Layout()[2]), width(Layout()[3]));
    for (auto const& row : Layout()) CHECK(width(row) <= letters + 0.001f);
}
