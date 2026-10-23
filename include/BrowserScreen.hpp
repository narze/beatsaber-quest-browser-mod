#pragma once

#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "KeyboardPanel.hpp"
#include "browser_logic.hpp"
#include "custom-types/shared/macros.hpp"

// Shows the web page on a RawImage and turns laser pointer input into touches.
// It is also the BSML host for the layout in BrowserScreen.cpp.
DECLARE_CLASS_CODEGEN(QuestBrowser, BrowserScreen, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::RawImage>, pageImage);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, titleText);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Texture2D>, texture);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::CanvasGroup>, canvasGroup);
    DECLARE_INSTANCE_FIELD(UnityW<KeyboardPanel>, keyboard);

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_INSTANCE_METHOD(void, OnBack);
    DECLARE_INSTANCE_METHOD(void, OnForward);
    DECLARE_INSTANCE_METHOD(void, OnReload);
    DECLARE_INSTANCE_METHOD(void, OnHome);
    DECLARE_INSTANCE_METHOD(void, OnUrl);
    DECLARE_INSTANCE_METHOD(void, OnKeyboard);

    DECLARE_DEFAULT_CTOR();

   public:
    // Builds the browser UI as a child of parent. parent must be under a Canvas.
    static BrowserScreen* CreateIn(UnityEngine::Transform* parent);

    // When false, the screen only shows the page: no clicks, no scroll, no buttons.
    // Used while a song plays.
    void SetInteractive(bool value);

   private:
    void Setup();
    void UpdateTexture();
    void UpdateInput();
    void UpdateLabels();
    void HideKeyboard();

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
