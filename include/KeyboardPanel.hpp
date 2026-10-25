#pragma once

#include <vector>

#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "custom-types/shared/macros.hpp"
#include "keyboard_logic.hpp"

// The browser keyboard. It opens near the player (in front, below the eyes), where it
// is large enough to type on; without a camera it falls back to below the window.
// Key presses go to the page, or to the address field while it is in use.
//
// It is on its own floating screen, not inside the window: the window screen has a
// RectMask2D that clips everything outside its rectangle. It follows the window.
DECLARE_CLASS_CODEGEN(QuestBrowser, KeyboardPanel, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Transform>, anchor);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
    DECLARE_DEFAULT_CTOR();

   public:
    // window: the transform of the browser window. windowHeight: its height in UI units.
    static KeyboardPanel* Create(UnityEngine::Transform* window, float windowHeight);

    void SetVisible(bool visible);
    bool IsVisible();
    void SetWindowHeight(float height) { windowHeight = height; FollowAnchor(); }
    // Extra space between the window and the keyboard, in window units (the toolbar).
    void SetExtraOffset(float offset) { extraOffset = offset; }

   private:
    struct KeyButton {
        Keys::KeyDef const* key;
        UnityW<UnityEngine::UI::Button> button;
        UnityW<TMPro::TextMeshProUGUI> text;
    };

    void Build(UnityEngine::Vector2 size);
    void FollowAnchor();
    bool PlaceNearUser();
    bool placedNearUser = false;
    void OnKey(Keys::KeyDef const& key);
    void RefreshLabels();

    Keys::KeyboardState state;
    float windowHeight = 0.0f;
    float extraOffset = 0.0f;
    UnityEngine::Vector2 panelSize{};
    std::vector<KeyButton> buttons;
};
