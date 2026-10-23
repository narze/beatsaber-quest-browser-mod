#pragma once

#include <vector>

#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "custom-types/shared/macros.hpp"
#include "keyboard_logic.hpp"

// The browser keyboard, drawn in the game below the browser window.
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

   private:
    struct KeyButton {
        Keys::KeyDef const* key;
        UnityW<UnityEngine::UI::Button> button;
        UnityW<TMPro::TextMeshProUGUI> text;
    };

    void Build(UnityEngine::Vector2 size);
    void FollowAnchor();
    void OnKey(Keys::KeyDef const& key);
    void RefreshLabels();

    Keys::KeyboardState state;
    float windowHeight = 0.0f;
    UnityEngine::Vector2 panelSize{};
    std::vector<KeyButton> buttons;
};
