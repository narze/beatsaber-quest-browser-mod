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
DECLARE_CLASS_CODEGEN(QuestBrowser, KeyboardPanel, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

   public:
    // parent: the transform of the browser window. windowHeight: its height in UI units.
    static KeyboardPanel* Create(UnityEngine::Transform* parent, float windowHeight);

    void SetVisible(bool visible);
    bool IsVisible();

   private:
    struct KeyButton {
        Keys::KeyDef const* key;
        UnityW<UnityEngine::UI::Button> button;
        UnityW<TMPro::TextMeshProUGUI> text;
    };

    void Build(float windowHeight);
    void OnKey(Keys::KeyDef const& key);
    void RefreshLabels();

    Keys::KeyboardState state;
    std::vector<KeyButton> buttons;
};
