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

constexpr std::string_view kLayout = R"xml(
<bg>
  <vertical spacing="1" child-expand-height="false" child-control-height="true" anchor-pos-y="2">
    <horizontal spacing="1" pref-height="8" child-expand-width="false">
      <button text="&lt;" on-click="OnBack" pref-width="10" hover-hint="Back"/>
      <button text="&gt;" on-click="OnForward" pref-width="10" hover-hint="Forward"/>
      <button text="Reload" on-click="OnReload" pref-width="17"/>
      <button text="Home" on-click="OnHome" pref-width="15"/>
      <button text="URL" click-event="show-url" pref-width="13" hover-hint="Type an address or a search"/>
      <button text="Type" click-event="show-type" pref-width="15" hover-hint="Type text into the selected field"/>
      <button text="Enter" on-click="OnEnterKey" pref-width="16"/>
      <button text="Del" on-click="OnBackspace" pref-width="13" hover-hint="Backspace"/>
    </horizontal>
    <text id="titleText" text="Loading..." font-size="3.5" pref-height="5" align="Left"/>
    <raw-image id="pageImage" pref-width="144" pref-height="81"/>
  </vertical>
  <modal-keyboard show-event="show-url" value="urlValue" on-enter="OnUrlEnter" clear-on-open="false" move-to-center="true" click-off-closes="true"/>
  <modal-keyboard show-event="show-type" on-enter="OnTypeEnter" clear-on-open="true" move-to-center="true" click-off-closes="true"/>
</bg>
)xml";

}  // namespace

void BrowserScreen::CreateIn(HMUI::ViewController* viewController) {
    auto screen = viewController->gameObject->AddComponent<BrowserScreen*>();
    BSML::parse_and_construct(kLayout, viewController->transform, screen);
    screen->Setup();
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

    urlValue = StringW(std::string(Logic::kHomeUrl));
    ready = Jni::Init(kPageWidth, kPageHeight, Logic::kHomeUrl);
    if (!ready) {
        if (titleText) titleText->text = "Browser failed to start. See the log for details.";
        return;
    }
    Jni::SetActive(isActiveAndEnabled);
}

void BrowserScreen::OnEnable() {
    if (ready) Jni::SetActive(true);
}

void BrowserScreen::OnDisable() {
    if (ready) Jni::SetActive(false);
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
    if (!pageImage) return;
    if (!pointer) {
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
    labelTimer = 0.25f;

    auto url = Jni::GetUrl();
    if (!url.empty()) urlValue = StringW(url);
    if (!titleText) return;

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

void BrowserScreen::OnUrlEnter(StringW value) {
    auto url = Logic::NormalizeUrl(static_cast<std::string>(value));
    logger.info("Loading {}", url);
    urlValue = StringW(url);
    Jni::LoadUrl(url);
}

void BrowserScreen::OnTypeEnter(StringW value) {
    auto text = static_cast<std::string>(value);
    if (!text.empty()) Jni::TypeText(text);
}

}  // namespace QuestBrowser
