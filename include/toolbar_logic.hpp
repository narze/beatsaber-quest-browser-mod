#pragma once

// Fade rule of the floating toolbar under the browser window.
// Game-independent, unit tested on the host (see tests/ui_test.cpp).

#include <algorithm>

namespace QuestBrowser::Toolbar {

// The toolbar stays this long after the pointer leaves the window and the toolbar.
constexpr float kStaySeconds = 1.5f;
// Fade in or out in 0.2 s.
constexpr float kFadePerSecond = 5.0f;

class Fader {
   public:
    // hovered: the pointer is on the window or the toolbar.
    // pinned: the keyboard or a panel is open, so the toolbar must stay.
    void Update(bool hovered, bool pinned, float deltaSeconds) {
        if (hovered || pinned) idle = 0.0f;
        else idle += deltaSeconds;
        float target = idle < kStaySeconds ? 1.0f : 0.0f;
        float step = kFadePerSecond * deltaSeconds;
        alpha = target > alpha ? std::min(target, alpha + step) : std::max(target, alpha - step);
    }

    // Hide at once, for example when a song starts playing.
    void Hide() {
        alpha = 0.0f;
        idle = kStaySeconds;
    }

    float Alpha() const { return alpha; }
    // Only a mostly visible toolbar takes clicks.
    bool Clickable() const { return alpha > 0.5f; }

   private:
    float alpha = 0.0f;
    float idle = kStaySeconds;  // hidden until the user points at the window
};

}  // namespace QuestBrowser::Toolbar
