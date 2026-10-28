#pragma once

// Game-independent model of the browser keyboard: layout, modifiers, and what
// each key press sends. Unit tested on the host (see tests/keyboard_test.cpp).

#include <string>
#include <string_view>
#include <vector>

namespace QuestBrowser::Keys {

// Android KeyEvent values.
constexpr int kKeyCodeA = 29;
constexpr int kMetaShift = 0x1;
constexpr int kMetaCtrl = 0x1000;

enum class Kind { Char, Key, Text, Shift, Caps, Ctrl, Hide };

struct KeyDef {
    Kind kind;
    std::string_view normal;   // label and name; for Char, the typed character
    std::string_view shifted;  // Char only: character with shift
    std::string_view text;     // Text only: text to type
    int keyCode;               // Key only: Android key code
    float width;               // relative width, 1 = one letter key
};

enum class OutputType { None, Text, Key, Hide };

struct Output {
    OutputType type = OutputType::None;
    std::string text;
    int keyCode = 0;
    int meta = 0;

    static Output Text(std::string_view t) { return {OutputType::Text, std::string(t), 0, 0}; }
    static Output Key(int code, int meta) { return {OutputType::Key, {}, code, meta}; }
    bool operator==(Output const&) const = default;
};

struct ModifierState {
    bool shift = false;  // one key press
    bool caps = false;   // until pressed again
    bool ctrl = false;   // one key press
};

using Row = std::vector<KeyDef>;

inline KeyDef Char(std::string_view normal, std::string_view shifted) { return {Kind::Char, normal, shifted, {}, 0, 1.0f}; }
inline KeyDef Special(Kind kind, std::string_view name, float width, int keyCode = 0, std::string_view text = {}) {
    return {kind, name, {}, text, keyCode, width};
}

inline std::vector<Row> const& Layout() {
    static std::vector<Row> const rows = {
        {Char("`", "~"), Char("1", "!"), Char("2", "@"), Char("3", "#"), Char("4", "$"), Char("5", "%"), Char("6", "^"),
         Char("7", "&"), Char("8", "*"), Char("9", "("), Char("0", ")"), Char("-", "_"), Char("=", "+"),
         Special(Kind::Key, "Del", 1.6f, 67)},
        {Special(Kind::Key, "Tab", 1.6f, 61), Char("q", "Q"), Char("w", "W"), Char("e", "E"), Char("r", "R"), Char("t", "T"),
         Char("y", "Y"), Char("u", "U"), Char("i", "I"), Char("o", "O"), Char("p", "P"), Char("[", "{"), Char("]", "}"),
         Char("\\", "|")},
        {Special(Kind::Caps, "Caps", 1.9f), Char("a", "A"), Char("s", "S"), Char("d", "D"), Char("f", "F"), Char("g", "G"),
         Char("h", "H"), Char("j", "J"), Char("k", "K"), Char("l", "L"), Char(";", ":"), Char("'", "\""),
         Special(Kind::Key, "Enter", 2.1f, 66)},
        {Special(Kind::Shift, "Shift", 2.3f), Char("z", "Z"), Char("x", "X"), Char("c", "C"), Char("v", "V"), Char("b", "B"),
         Char("n", "N"), Char("m", "M"), Char(",", "<"), Char(".", ">"), Char("/", "?"), Special(Kind::Key, "Up", 1.0f, 19),
         Special(Kind::Text, ".com", 1.7f, 0, ".com")},
        {Special(Kind::Ctrl, "Ctrl", 1.6f), Special(Kind::Key, "Esc", 1.4f, 111), Special(Kind::Text, "http://", 1.8f, 0, "http://"),
         Special(Kind::Text, "Space", 3.6f, 0, " "), Special(Kind::Text, "https://", 1.9f, 0, "https://"),
         Special(Kind::Key, "Left", 1.0f, 21), Special(Kind::Key, "Down", 1.0f, 20), Special(Kind::Key, "Right", 1.0f, 22),
         Special(Kind::Hide, "Hide", 1.6f)},
    };
    return rows;
}

class KeyboardState {
   public:
    ModifierState const& Modifiers() const { return mods; }

    // Text shown on the key for the current modifiers.
    std::string Label(KeyDef const& key) const {
        if (key.kind != Kind::Char) return std::string(key.normal);
        return std::string(UseShifted(key) ? key.shifted : key.normal);
    }

    Output Press(KeyDef const& key) {
        switch (key.kind) {
            case Kind::Shift:
                mods.shift = !mods.shift;
                return {};
            case Kind::Caps:
                mods.caps = !mods.caps;
                return {};
            case Kind::Ctrl:
                mods.ctrl = !mods.ctrl;
                return {};
            case Kind::Hide:
                return {OutputType::Hide, {}, 0, 0};
            case Kind::Text:
                ReleaseOneShot();
                return Output::Text(key.text);
            case Kind::Key: {
                int meta = (mods.shift ? kMetaShift : 0) | (mods.ctrl ? kMetaCtrl : 0);
                ReleaseOneShot();
                return Output::Key(key.keyCode, meta);
            }
            case Kind::Char: {
                if (mods.ctrl && IsLetter(key)) {
                    int code = kKeyCodeA + (key.normal[0] - 'a');
                    ReleaseOneShot();
                    return Output::Key(code, kMetaCtrl);
                }
                auto out = Output::Text(UseShifted(key) ? key.shifted : key.normal);
                ReleaseOneShot();
                return out;
            }
        }
        return {};
    }

   private:
    static bool IsLetter(KeyDef const& key) { return key.normal.size() == 1 && key.normal[0] >= 'a' && key.normal[0] <= 'z'; }

    bool UseShifted(KeyDef const& key) const {
        // Caps lock changes letters only; shift then gives the opposite case.
        bool caps = mods.caps && IsLetter(key);
        return mods.shift != caps;
    }

    void ReleaseOneShot() {
        mods.shift = false;
        mods.ctrl = false;
    }

    ModifierState mods;
};

}  // namespace QuestBrowser::Keys
