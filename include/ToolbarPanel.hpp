#pragma once

#include <functional>
#include <string>

#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "KeyboardPanel.hpp"
#include "custom-types/shared/macros.hpp"
#include "toolbar_logic.hpp"

namespace QuestBrowser {

// What the toolbar shows and does. BrowserScreen fills it in.
struct ToolbarActions {
    std::function<void()> back, forward, reload, editUrl, toggleTabs, toggleMenu;
    std::function<std::string()> label;  // page title, URL, or the address being typed
    std::function<int()> tabCount;
    std::function<bool()> pinned;       // a panel or the keyboard is open: stay shown
    std::function<bool()> interactive;  // false while a song plays: stay hidden
};

}  // namespace QuestBrowser

// Slim toolbar that floats under the browser window: back, forward, reload, the
// address, tabs, and menu. It fades in when you point at the window or the toolbar.
// Own floating screen (the window clips everything outside its rectangle).
DECLARE_CLASS_CODEGEN(QuestBrowser, ToolbarPanel, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Transform>, window);
    DECLARE_INSTANCE_FIELD(UnityW<KeyboardPanel>, keyboard);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::CanvasGroup>, canvasGroup);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, urlText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, tabCountText);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
    DECLARE_DEFAULT_CTOR();

   public:
    static ToolbarPanel* Create(UnityEngine::Transform* window, KeyboardPanel* keyboard, ToolbarActions actions);
    // Window size in UI units, so the toolbar matches its width and sits under it.
    void SetWindowSize(float width, float height);
    void SetShown(bool shown);

   private:
    void Build();
    void Follow();
    bool PointerOnUs();

    ToolbarActions actions;
    Toolbar::Fader fader;
    float windowWidth = 150.0f;
    float windowHeight = 87.0f;
    float controlScale = 1.0f;
    float labelTimer = 0.0f;
};
