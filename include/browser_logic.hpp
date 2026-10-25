#pragma once

// Game-independent logic. Keep this file free of Unity and il2cpp headers
// so that it can be unit tested on the host (see tests/).

#include <optional>
#include <string>
#include <string_view>

namespace QuestBrowser::Logic {

constexpr std::string_view kHomeUrl = "https://duckduckgo.com/";
constexpr std::string_view kSearchPrefix = "https://duckduckgo.com/?q=";

struct Rect {
    float x;  // left edge, in local units
    float y;  // bottom edge, in local units
    float width;
    float height;
};

struct Pixel {
    float x;  // from the left edge of the page
    float y;  // from the top edge of the page
    bool operator==(Pixel const&) const = default;
};

enum class PointerAction { None, Down, Move, Up, Cancel };

inline std::string_view Trim(std::string_view s) {
    constexpr std::string_view ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string_view::npos) return {};
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

inline std::string UrlEncodeQuery(std::string_view s) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// Turns text from the address bar into a URL to load.
// Full URLs pass through, bare host names get "https://", other text becomes a search.
inline std::string NormalizeUrl(std::string_view input) {
    auto s = Trim(input);
    if (s.empty()) return std::string(kHomeUrl);

    // Has a scheme such as "https:" or "about:" (not "localhost:8080").
    auto colon = s.find(':');
    if (colon != std::string_view::npos && colon > 0) {
        auto scheme = s.substr(0, colon);
        bool alpha = scheme.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-.") == std::string_view::npos;
        auto rest = s.substr(colon + 1);
        bool portLike = !rest.empty() && rest.front() >= '0' && rest.front() <= '9';
        if (alpha && !portLike) return std::string(s);
    }

    bool hasSpace = s.find(' ') != std::string_view::npos;
    auto host = s.substr(0, s.find_first_of("/?#"));
    bool looksLikeHost = host.find('.') != std::string_view::npos || host.starts_with("localhost");
    if (!hasSpace && looksLikeHost) return "https://" + std::string(s);

    return std::string(kSearchPrefix) + UrlEncodeQuery(s);
}

// Maps a point in the local space of a UI rect (Y up) to page pixels (Y down).
// Returns nullopt when the point is outside the rect.
inline std::optional<Pixel> LocalToPixel(Rect const& rect, float localX, float localY, int texWidth, int texHeight) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) return std::nullopt;
    float u = (localX - rect.x) / rect.width;
    float v = (localY - rect.y) / rect.height;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return std::nullopt;
    return Pixel{u * static_cast<float>(texWidth), (1.0f - v) * static_cast<float>(texHeight)};
}

// Converts a thumbstick Y value into a mouse wheel value for one frame.
// Positive means scroll up, the same as Android AXIS_VSCROLL.
inline float ThumbstickToScroll(float stickY, float deadZone = 0.2f, float speed = 1.0f) {
    if (stickY > -deadZone && stickY < deadZone) return 0.0f;
    float sign = stickY > 0.0f ? 1.0f : -1.0f;
    float scaled = (stickY - sign * deadZone) / (1.0f - deadZone);
    return scaled * speed;
}

// Turns the trigger state and laser hit point of each frame into touch events.
class PointerTracker {
   public:
    PointerAction Update(bool pressed, std::optional<Pixel> hit) {
        if (!pressed) {
            blocked = false;
            if (!down) return PointerAction::None;
            down = false;
            return PointerAction::Up;
        }
        if (blocked) return PointerAction::None;
        if (!hit) {
            if (!down) {
                blocked = true;
                return PointerAction::None;
            }
            down = false;
            blocked = true;
            return PointerAction::Cancel;
        }
        if (!down) {
            down = true;
            last = *hit;
            return PointerAction::Down;
        }
        if (*hit == last) return PointerAction::None;
        last = *hit;
        return PointerAction::Move;
    }

    bool IsDown() const { return down; }
    Pixel LastPixel() const { return last; }

   private:
    bool down = false;
    // Set when the trigger is held but the touch can not start or continue.
    // Cleared when the trigger is released.
    bool blocked = false;
    Pixel last{};
};

// Counts the browser screens that are visible. The page runs while at least one is.
// Add() and Remove() return the new running state when it changes, else nullopt.
class ActiveCounter {
   public:
    std::optional<bool> Add() {
        if (++count == 1) return true;
        return std::nullopt;
    }
    std::optional<bool> Remove() {
        if (count == 0) return std::nullopt;
        if (--count == 0) return false;
        return std::nullopt;
    }
    bool IsActive() const { return count > 0; }

   private:
    int count = 0;
};

// Page frame rate. It is lower while a song plays, to keep the game smooth.
// Measured on Quest 3S: a 1280x720 frame costs under 2 ms (draw, copy, upload).
inline int FrameRateFor(bool interactive) { return interactive ? 60 : 30; }

enum class Context { Menu, SongPlaying, SongPaused };
enum class PoseSlot { Menu, Song };

struct ScreenState {
    bool visible;
    bool interactive;
    bool showHandle;
    PoseSlot pose;
};

// How the one browser window looks in each part of the game.
// userVisible: the menu button toggle. showInSong: the mod setting.
inline ScreenState ScreenStateFor(Context context, bool userVisible, bool showInSong) {
    bool inSong = context != Context::Menu;
    bool visible = userVisible && (!inSong || showInSong);
    bool interactive = context != Context::SongPlaying;
    return {visible, interactive, interactive, inSong ? PoseSlot::Song : PoseSlot::Menu};
}

// enabled: the "Enable browser" setting. A disabled browser is hidden everywhere.
inline ScreenState ScreenStateFor(Context context, bool enabled, bool userVisible, bool showInSong) {
    return ScreenStateFor(context, enabled && userVisible, showInSong);
}

}  // namespace QuestBrowser::Logic
