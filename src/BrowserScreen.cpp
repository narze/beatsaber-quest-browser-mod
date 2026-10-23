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
#include "UnityEngine/Transform.hpp"
#include "bsml/shared/BSML.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, BrowserScreen);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr int kPageWidth = 1280;
constexpr int kPageHeight = 720;

// Android key codes.
constexpr int kKeyEnter = 66;
constexpr int kKeyDelete = 67;

// Trigger hysteresis, so a half-pulled trigger does not flicker.
constexpr float kTriggerPress = 0.75f;
constexpr float kTriggerRelease = 0.35f;
constexpr float kScrollSpeed = 0.25f;

// Shared by every BrowserScreen: the page runs while any screen is visible.
Logic::ActiveCounter activeScreens;

constexpr std::string_view kLayout = R"xml(
<bg>
  <vertical spacing="1" child-expand-height="false" child-control-height="true" anchor-pos-y="2">
    <horizontal spacing="1" pref-height="8" child-expand-width="false">
      <button text="&lt;" on-click="OnBack" pref-width="10" hover-hint="Back"/>
      <button text="&gt;" on-click="OnForward" pref-width="10" hover-hint="Forward"/>
      <button text="Reload" on-click="OnReload" pref-width="17"/>
      <button text="Home" on-click="OnHome" pref-width="15"/>
      <button text="URL" on-click="OnUrl" pref-width="13" hover-hint="Type an address or a search"/>
      <button text="Keys" on-click="OnKeyboard" pref-width="15" hover-hint="Open the keyboard for the selected field"/>
      <button text="Enter" on-click="OnEnterKey" pref-width="16"/>
      <button text="Del" on-click="OnBackspace" pref-width="13" hover-hint="Backspace"/>
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
        if (ready) Jni::HideKeyboard();
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

    ready = Jni::Init(kPageWidth, kPageHeight, Logic::kHomeUrl);
    if (!ready) {
        if (titleText) titleText->text = "Browser failed to start. See the log for details.";
        return;
    }
    OnEnable();
}

void BrowserScreen::OnEnable() {
    if (!ready || counted || !isActiveAndEnabled) return;
    counted = true;
    if (auto change = activeScreens.Add()) Jni::SetActive(*change);
    Jni::SetFrameRate(Logic::FrameRateFor(interactive));
}

void BrowserScreen::OnDisable() {
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
    bool updated = Jni::ConsumeFrame([this](void* pixels, std::size_t size) {
        std::size_t expected = static_cast<std::size_t>(kPageWidth) * kPageHeight * 4;
        if (size < expected) return;
        texture->LoadRawTextureData(System::IntPtr(pixels), static_cast<int>(expected));
    });
    if (updated) texture->Apply(false);
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
    }
    if (!titleText) return;

    // While the address is typed, show it here: the system keyboard does not.
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
void BrowserScreen::OnEnterKey() { Jni::PressKey(kKeyEnter); }
void BrowserScreen::OnBackspace() { Jni::PressKey(kKeyDelete); }

void BrowserScreen::OnUrl() { Jni::ShowUrlKeyboard(Jni::GetUrl()); }
void BrowserScreen::OnKeyboard() { Jni::ShowPageKeyboard(); }

}  // namespace QuestBrowser
