#pragma once

// Toolbar icons, drawn in code: the game font has no arrow or menu glyphs.
// Game-independent, unit tested on the host (see tests/ui_test.cpp).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace QuestBrowser::Icons {

enum class Kind { Back, Forward, Reload, Menu, Tabs };

namespace detail {

struct Seg {
    float x0, y0, x1, y1;
};

inline float DistToSeg(float px, float py, Seg s) {
    float dx = s.x1 - s.x0, dy = s.y1 - s.y0;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? std::clamp(((px - s.x0) * dx + (py - s.y0) * dy) / len2, 0.0f, 1.0f) : 0.0f;
    float cx = s.x0 + t * dx - px, cy = s.y0 + t * dy - py;
    return std::sqrt(cx * cx + cy * cy);
}

// Distance from (x, y) to the strokes of the icon, in units of the icon size (0..1).
inline float Distance(Kind kind, float x, float y) {
    constexpr float kPi = 3.14159265f;
    auto minSegs = [&](std::initializer_list<Seg> segs) {
        float d = 1e9f;
        for (auto s : segs) d = std::min(d, DistToSeg(x, y, s));
        return d;
    };
    switch (kind) {
        case Kind::Back:
            return minSegs({{0.62f, 0.82f, 0.36f, 0.5f}, {0.36f, 0.5f, 0.62f, 0.18f}});
        case Kind::Forward:
            return minSegs({{0.38f, 0.82f, 0.64f, 0.5f}, {0.64f, 0.5f, 0.38f, 0.18f}});
        case Kind::Menu:
            return minSegs({{0.24f, 0.3f, 0.76f, 0.3f}, {0.24f, 0.5f, 0.76f, 0.5f}, {0.24f, 0.7f, 0.76f, 0.7f}});
        case Kind::Tabs:
            // A square outline; the toolbar writes the tab count inside it.
            return minSegs({{0.2f, 0.2f, 0.8f, 0.2f}, {0.8f, 0.2f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.2f, 0.8f}, {0.2f, 0.8f, 0.2f, 0.2f}});
        case Kind::Reload: {
            // A circle with a gap at the top right, and an arrow head at the gap.
            float dx = x - 0.5f, dy = y - 0.5f;
            float ring = std::fabs(std::sqrt(dx * dx + dy * dy) - 0.28f);
            float angle = std::atan2(dy, dx);  // -pi..pi, 0 = right
            bool inGap = angle > 0.15f && angle < 1.1f;
            float arc = inGap ? 1e9f : ring;
            // Arrow head at the end of the arc (angle 0.15), pointing up.
            float ex = 0.5f + 0.28f * std::cos(0.15f), ey = 0.5f + 0.28f * std::sin(0.15f);
            float head = minSegs({{ex - 0.13f, ey + 0.02f, ex, ey + 0.12f}, {ex, ey + 0.12f, ex + 0.11f, ey - 0.03f}});
            (void)kPi;
            return std::min(arc, head);
        }
    }
    return 1e9f;
}

}  // namespace detail

// RGBA pixels (white, the shape in alpha), size x size, texture row 0 at the bottom.
inline std::vector<uint8_t> Draw(Kind kind, int size) {
    constexpr float kHalfStroke = 0.055f;
    std::vector<uint8_t> px(static_cast<std::size_t>(size) * size * 4, 0);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = (x + 0.5f) / size, fy = (y + 0.5f) / size;
            float d = detail::Distance(kind, fx, fy);
            // Soft edge about one pixel wide.
            float a = std::clamp((kHalfStroke - d) * size + 0.5f, 0.0f, 1.0f);
            auto i = (static_cast<std::size_t>(y) * size + x) * 4;
            px[i] = px[i + 1] = px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
        }
    }
    return px;
}

}  // namespace QuestBrowser::Icons
