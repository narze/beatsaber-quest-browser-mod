#pragma once

#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "KeyboardPanel.hpp"
#include "ToolbarPanel.hpp"
#include "bookmarks_logic.hpp"
#include "browser_logic.hpp"
#include "resize_logic.hpp"
#include "songs_logic.hpp"
#include "tabs_logic.hpp"
#include "custom-types/shared/macros.hpp"

// The browser window: only the page, with panels (tabs, menu, history) that take
// the place of the page when open. The toolbar floats below (ToolbarPanel).
// Shows the page on a RawImage and turns laser pointer input into touches.
DECLARE_CLASS_CODEGEN(QuestBrowser, BrowserScreen, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::RawImage>, pageImage);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Texture2D>, texture);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::CanvasGroup>, canvasGroup);
    DECLARE_INSTANCE_FIELD(UnityW<KeyboardPanel>, keyboard);
    DECLARE_INSTANCE_FIELD(UnityW<ToolbarPanel>, toolbar);

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_DEFAULT_CTOR();

   public:
    // Builds the browser UI as a child of parent. parent must be under a Canvas.
    static BrowserScreen* CreateIn(UnityEngine::Transform* parent);

    // When false, the screen only shows the page: no clicks, no scroll, no buttons.
    // Used while a song plays.
    void SetInteractive(bool value);
    void SetDimensions(QuestBrowser::Resize::Dimensions size);
    // The window has no controls inside, so the minimum size does not grow with distance.
    float GetControlScale() const { return 1.0f; }
    float GetChromeHeight() const { return Resize::kChromeHeight; }
    void RefreshForPose() {}
    // Play mode: hide the toolbar and close panels, so only the page shows.
    void SetChromeHidden(bool hidden);
    // Applies the opacity of the current context (menu or song).
    void ApplyOpacity() { ChangeOpacity(0.0f); }
    bool IsChromeHidden() const { return chromeHidden; }
    QuestBrowser::Resize::Dimensions GetDimensions() const { return dimensions; }

   private:
    enum class Panel { None, Tabs, Menu, History, Bookmarks };

    void Setup();
    ToolbarActions MakeToolbarActions();
    void UpdateTexture();
    void UpdateInput();
    void UpdateLabels();
    std::string ToolbarLabel();
    void HideKeyboard();
    void OpenUrlEditing();
    void ToggleKeyboard();
    void ApplyViewport();
    void ChangeOpacity(float delta);
    void ChangeZoom(float delta);

    // Panels in the page area.
    UnityEngine::GameObject* MakePanel();
    void ShowPanel(Panel panel);
    void BuildMenu();
    void RefreshMenu();
    void BuildHistory();
    void RefreshHistory();
    void BuildBookmarks();
    void RefreshBookmarks();
    void BuildTabs();
    void RefreshTabs();
    Panel openPanel = Panel::None;
    std::vector<UnityW<UnityEngine::GameObject>> panels;  // indexed by Panel - 1
    std::vector<UnityW<UnityEngine::UI::LayoutElement>> panelSizes;
    // Content of each panel, designed at 144 x 81 and scaled to fill the page area.
    std::vector<UnityW<UnityEngine::RectTransform>> panelContents;

    bool chromeHidden = false;
    Resize::Dimensions dimensions;
    bool viewportDirty = true;
    float viewportTimer = 0;

    // Menu panel.
    UnityW<TMPro::TextMeshProUGUI> zoomText;
    UnityW<TMPro::TextMeshProUGUI> opacityText;
    UnityW<TMPro::TextMeshProUGUI> aspectText;

    // History panel.
    std::vector<std::string> history;
    std::vector<UnityW<TMPro::TextMeshProUGUI>> historyLabels;
    std::vector<UnityW<UnityEngine::UI::Button>> historyOpenButtons;
    std::vector<UnityW<UnityEngine::UI::Button>> historyDeleteButtons;
    UnityW<TMPro::TextMeshProUGUI> historyStatus;
    std::size_t historyPage = 0;
    std::string lastHistoryUrl;

    // Bookmarks panel.
    std::vector<Bookmarks::Bookmark> bookmarks;
    std::vector<UnityW<TMPro::TextMeshProUGUI>> bookmarkLabels;
    std::vector<UnityW<UnityEngine::UI::Button>> bookmarkOpenButtons;
    std::vector<UnityW<UnityEngine::UI::Button>> bookmarkDeleteButtons;
    UnityW<TMPro::TextMeshProUGUI> bookmarksStatus;
    std::size_t bookmarksPage = 0;

    // Tabs panel, and the tab list read from the Java side.
    void UpdateTabs();
    int tabsVersion = -1;
    std::vector<std::string> tabUrls;
    std::vector<std::string> tabTitles;
    int activeTab = 0;
    std::size_t tabsPage = 0;
    std::vector<UnityW<TMPro::TextMeshProUGUI>> tabLabels;
    std::vector<UnityW<UnityEngine::UI::Button>> tabOpenButtons;
    std::vector<UnityW<UnityEngine::UI::Button>> tabCloseButtons;
    UnityW<TMPro::TextMeshProUGUI> tabsStatus;
    UnityW<UnityEngine::UI::Button> newTabButton;
    void CloseTab(int index);

    // Map downloads: status for the toolbar ("" when none), and the SongCore reload.
    std::string songStatus;
    Songs::RefreshGate songRefresh;

    bool ready = false;
    bool interactive = true;
    bool counted = false;
    float labelTimer = 0.0f;
    Logic::PointerTracker tracker;
    bool triggerHeld = false;
    struct UploadStats {
        int frames = 0;
        int uploads = 0;
        float seconds = 0.0f;
        float uploadMs = 0.0f;
    } uploadStats;
};
