#pragma once

#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "browser_logic.hpp"
#include "custom-types/shared/macros.hpp"

// Shows the web page on a RawImage and turns laser pointer input into touches.
// It is also the BSML host for assets/browser.bsml.
DECLARE_CLASS_CODEGEN(QuestBrowser, BrowserScreen, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::RawImage>, pageImage);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, titleText);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Texture2D>, texture);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    // Bound to the value of the URL keyboard, so that it opens with the current URL.
    DECLARE_INSTANCE_FIELD(StringW, urlValue);

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_INSTANCE_METHOD(void, OnBack);
    DECLARE_INSTANCE_METHOD(void, OnForward);
    DECLARE_INSTANCE_METHOD(void, OnReload);
    DECLARE_INSTANCE_METHOD(void, OnHome);
    DECLARE_INSTANCE_METHOD(void, OnEnterKey);
    DECLARE_INSTANCE_METHOD(void, OnBackspace);
    DECLARE_INSTANCE_METHOD(void, OnUrlEnter, StringW value);
    DECLARE_INSTANCE_METHOD(void, OnTypeEnter, StringW value);

    DECLARE_DEFAULT_CTOR();

   public:
    // Builds the UI in the view controller. Call once, on first activation.
    static void CreateIn(HMUI::ViewController* viewController);

   private:
    void Setup();
    void UpdateTexture();
    void UpdateInput();
    void UpdateLabels();

    bool ready = false;
    float labelTimer = 0.0f;
    Logic::PointerTracker tracker;
    bool triggerHeld = false;
};
