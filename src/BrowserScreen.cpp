#include "BrowserScreen.hpp"

#include "GlobalNamespace/VRController.hpp"
#include "JniBridge.hpp"
#include "System/IntPtr.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"

#include <chrono>
#include "UnityEngine/Transform.hpp"
#include "bsml/shared/BSML.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, BrowserScreen);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr int kPageWidth = 1280;
constexpr int kPageHeight = 720;

// Trigger hysteresis, so a half-pulled trigger does not flicker.
constexpr float kTriggerPress = 0.75f;
constexpr float kTriggerRelease = 0.35f;
constexpr float kScrollSpeed = 0.25f;

// Shared by every BrowserScreen: the page runs while any screen is visible.
Logic::ActiveCounter activeScreens;

constexpr std::string_view kLayout = R"xml(
<bg>
  <vertical spacing="1" pad="3" child-align="UpperCenter" child-expand-height="false" child-control-height="true">
    <horizontal spacing="1" pref-height="8" child-expand-width="false">
      <button text="&lt;" on-click="OnBack" pref-width="14" hover-hint="Back"/>
      <button text="&gt;" on-click="OnForward" pref-width="14" hover-hint="Forward"/>
      <button text="Reload" on-click="OnReload" pref-width="22"/>
      <button text="Home" on-click="OnHome" pref-width="20"/>
      <button text="URL" on-click="OnUrl" pref-width="20" hover-hint="Type an address or a search"/>
      <button text="Keyboard" on-click="OnKeyboard" pref-width="28" hover-hint="Show or hide the keyboard"/>
    </horizontal>
    <text id="titleText" text="Loading..." font-size="3.5" pref-height="5" align="Left"/>
    <raw-image id="pageImage" pref-width="144" pref-height="81"/>
  </vertical>
</bg>
)xml";

}  // namespace

BrowserScreen* BrowserScreen::CreateIn(Transform* parent) {
    auto go = parent->gameObject;
    auto screen = go->AddComponent<BrowserScreen*>();
    screen->canvasGroup = go->GetComponent<CanvasGroup*>();
    if (!screen->canvasGroup) screen->canvasGroup = go->AddComponent<CanvasGroup*>();
    BSML::parse_and_construct(kLayout, parent, screen);
    auto parentRect = parent->GetComponent<RectTransform*>();
    screen->keyboard = KeyboardPanel::Create(parent, parentRect ? parentRect->rect.height : 100.0f);
    screen->Setup();
    return screen;
}

void BrowserScreen::SetInteractive(bool value) {
    interactive = value;
    if (canvasGroup) canvasGroup->blocksRaycasts = value;
    if (!value && tracker.IsDown()) {
        auto p = tracker.LastPixel();
        Jni::Touch(3, p.x, p.y);
    }
    if (!value) {
        HideKeyboard();
        tracker = {};
        triggerHeld = false;
    }
    if (ready && isActiveAndEnabled) Jni::SetFrameRate(Logic::FrameRateFor(value));
}

void BrowserScreen::Setup() {
    texture = Texture2D::New_ctor(kPageWidth, kPageHeight, TextureFormat::RGBA32, false);
    texture->filterMode = FilterMode::Bilinear;
    texture->wrapMode = TextureWrapMode::Clamp;

    if (pageImage) {
        pageImage->texture = texture.ptr();
        // Android bitmaps start at the top row, Unity textures at the bottom row.
        pageImage->uvRect = Rect(0.0f, 1.0f, 1.0f, -1.0f);
    } else {
        logger.error("BSML did not bind pageImage");
    }

#ifdef QB_DEBUG_YOUTUBE_ID
    // Debug builds (qpm s build -D QB_DEBUG_YOUTUBE_ID=<video id>): start on that video, for
    // performance measurements.
#define QB_STR2(x) #x
#define QB_STR(x) QB_STR2(x)
    ready = Jni::Init(kPageWidth, kPageHeight, "https://www.youtube.com/embed/" QB_STR(QB_DEBUG_YOUTUBE_ID) "?autoplay=1");
#else
    ready = Jni::Init(kPageWidth, kPageHeight, Logic::kHomeUrl);
#endif
    if (!ready) {
        if (titleText) titleText->text = "Browser failed to start. See the log for details.";
        return;
    }
#ifdef QB_DEBUG_SHOW_KEYBOARD
    // Debug builds (qpm s build -D QB_DEBUG_SHOW_KEYBOARD=1): open the keyboard at start, for screenshots.
    if (keyboard) keyboard->SetVisible(true);
#endif
    OnEnable();
}

void BrowserScreen::OnEnable() {
    if (!ready || counted || !isActiveAndEnabled) return;
    counted = true;
    if (auto change = activeScreens.Add()) Jni::SetActive(*change);
    Jni::SetFrameRate(Logic::FrameRateFor(interactive));
}

void BrowserScreen::OnDisable() {
    // The keyboard is not a child of the window, so hide it with the window.
    if (keyboard) keyboard->SetVisible(false);
    if (counted) {
        counted = false;
        if (auto change = activeScreens.Remove()) Jni::SetActive(*change);
    }
    // Do not leave a touch held down when the menu closes.
    if (tracker.IsDown()) {
        auto p = tracker.LastPixel();
        Jni::Touch(3, p.x, p.y);
    }
    tracker = {};
    triggerHeld = false;
}

void BrowserScreen::Update() {
    if (!ready) return;
    UpdateTexture();
    UpdateInput();
    UpdateLabels();
}

void BrowserScreen::UpdateTexture() {
    if (!texture) return;
    auto start = std::chrono::steady_clock::now();
    bool updated = Jni::ConsumeFrame([this](void* pixels, std::size_t size) {
        std::size_t expected = static_cast<std::size_t>(kPageWidth) * kPageHeight * 4;
        if (size < expected) return;
        texture->LoadRawTextureData(System::IntPtr(pixels), static_cast<int>(expected));
    });
    if (updated) texture->Apply(false);

    // Performance counters, logged every 30 seconds.
    auto& st = uploadStats;
    st.frames++;
    st.seconds += Time::get_unscaledDeltaTime();
    if (updated) {
        st.uploads++;
        st.uploadMs += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    if (st.seconds >= 30.0f) {
        logger.info("Stats: game {:.1f} fps, uploads {:.1f}/s avg {:.2f} ms", st.frames / st.seconds, st.uploads / st.seconds,
                    st.uploads ? st.uploadMs / st.uploads : 0.0f);
        st = {};
    }
}

void BrowserScreen::UpdateInput() {
    if (!pageImage || !interactive) return;
    // Each scene has its own pointer; find it again when the old one is gone or disabled.
    if (!pointer || !pointer->isActiveAndEnabled) {
        pointer = Object::FindAnyObjectByType<VRUIControls::VRPointer*>();
        if (!pointer) return;
    }

    std::optional<Logic::Pixel> hit;
    UnityW<GameObject> over = pointer->pointingOver;
    if (over && over.ptr() == pageImage->gameObject.ptr()) {
        auto rectTransform = pageImage->rectTransform;
        auto local = rectTransform->InverseTransformPoint(pointer->cursorPosition);
        auto r = rectTransform->rect;
        hit = Logic::LocalToPixel({r.m_XMin, r.m_YMin, r.m_Width, r.m_Height}, local.x, local.y, kPageWidth, kPageHeight);
    }

    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (!controller) return;
    float trigger = controller->triggerValue;
    triggerHeld = triggerHeld ? trigger > kTriggerRelease : trigger > kTriggerPress;

    switch (tracker.Update(triggerHeld, hit)) {
        case Logic::PointerAction::Down:
            Jni::Touch(0, hit->x, hit->y);
            break;
        case Logic::PointerAction::Move:
            Jni::Touch(2, hit->x, hit->y);
            break;
        case Logic::PointerAction::Up: {
            auto p = tracker.LastPixel();
            Jni::Touch(1, p.x, p.y);
            break;
        }
        case Logic::PointerAction::Cancel: {
            auto p = tracker.LastPixel();
            Jni::Touch(3, p.x, p.y);
            break;
        }
        case Logic::PointerAction::None:
            break;
    }

    // Thumbstick scrolls the page under the pointer, unless a drag is active.
    if (hit && !tracker.IsDown()) {
        float amount = Logic::ThumbstickToScroll(controller->thumbstick.y, 0.2f, kScrollSpeed);
        if (amount != 0.0f) Jni::Scroll(hit->x, hit->y, amount);
    }
}

void BrowserScreen::UpdateLabels() {
    labelTimer -= Time::get_deltaTime();
    if (labelTimer > 0.0f) return;
    labelTimer = 0.1f;

    if (auto submitted = Jni::TakeSubmittedUrl()) {
        auto url = Logic::NormalizeUrl(*submitted);
        logger.info("Loading {}", url);
        Jni::LoadUrl(url);
        if (keyboard) keyboard->SetVisible(false);
    }
    // The user tapped a text field in the page.
    if (Jni::TakeKeyboardRequest() && interactive && keyboard) keyboard->SetVisible(true);
    if (!titleText) return;

    // While the address is typed, show it here: the address field is not drawn.
    if (auto draft = Jni::GetUrlDraft()) {
        titleText->text = fmt::format("URL: {}_", *draft);
        return;
    }

    auto url = Jni::GetUrl();

    auto title = Jni::GetTitle();
    int progress = Jni::GetProgress();
    std::string label = title.empty() ? url : title;
    if (progress < 100) label = fmt::format("[{}%] {}", progress, label);
    if (label.size() > 120) label = label.substr(0, 117) + "...";
    titleText->text = label;
}

void BrowserScreen::OnBack() { Jni::GoBack(); }
void BrowserScreen::OnForward() { Jni::GoForward(); }
void BrowserScreen::OnReload() { Jni::Reload(); }
void BrowserScreen::OnHome() { Jni::LoadUrl(Logic::kHomeUrl); }

void BrowserScreen::OnUrl() {
    Jni::BeginUrlEditing(Jni::GetUrl());
    if (keyboard) keyboard->SetVisible(true);
}

void BrowserScreen::OnKeyboard() {
    logger.info("Keyboard button pressed (keyboard {})", keyboard ? (keyboard->IsVisible() ? "visible" : "hidden") : "missing");
    if (keyboard && keyboard->IsVisible()) HideKeyboard();
    else if (keyboard) keyboard->SetVisible(true);
}

void BrowserScreen::HideKeyboard() {
    if (ready) Jni::CancelUrlEditing();
    if (keyboard) keyboard->SetVisible(false);
}

}  // namespace QuestBrowser
