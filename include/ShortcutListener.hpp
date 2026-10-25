#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"
#include "shortcut_logic.hpp"

// Controller keybind: hold the combo from the settings (default X + Y) to show the
// browser and reset its position (Window::ResetCurrentPosition). Lives on its own
// persistent object, so it also works while the browser window is hidden.
DECLARE_CLASS_CODEGEN(QuestBrowser, ShortcutListener, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_DEFAULT_CTOR();

   public:
    // Creates the listener once. Call from the main menu.
    static void EnsureCreated();

   private:
    Shortcut::HoldDetector hold;
};
