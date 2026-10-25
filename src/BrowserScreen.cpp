#include "BrowserScreen.hpp"

#include "GlobalNamespace/VRController.hpp"
#include "JniBridge.hpp"
#include "Config.hpp"
#include "BrowserWindow.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "System/IntPtr.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Camera.hpp"

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
    <horizontal id="mainControls" spacing="1" pref-height="10" child-expand-width="false">
      <button text="&lt;" on-click="OnBack" pref-width="22" hover-hint="Back"/>
      <button text="&gt;" on-click="OnForward" pref-width="22" hover-hint="Forward"/>
      <button text="URL" on-click="OnUrl" pref-width="40" hover-hint="Type an address or a search"/>
      <button text="Menu" on-click="OnMenu" pref-width="40"/>
    </horizontal>
    <horizontal id="groupControls" spacing="1" pref-height="10" child-expand-width="false">
      <button text="Browse" on-click="OnBrowseGroup" pref-width="60"/>
      <button text="View" on-click="OnViewGroup" pref-width="60"/>
    </horizontal>
    <horizontal id="browseControls" spacing="1" pref-height="10" child-expand-width="false">
      <button text="Reload" on-click="OnReload" pref-width="22"/>
      <button text="Home" on-click="OnHome" pref-width="20"/>
      <button text="Keyboard" on-click="OnKeyboard" pref-width="28" hover-hint="Show or hide the keyboard"/>
      <button text="History" on-click="OnHistory" pref-width="32"/>
    </horizontal>
    <horizontal id="viewControls" spacing="1" pref-height="10" child-expand-width="false">
      <button text="Aspect" on-click="OnAspect" pref-width="28"/>
      <button text="Zoom -" on-click="OnZoomDown" pref-width="32"/>
      <button text="Zoom +" on-click="OnZoomUp" pref-width="32"/>
    </horizontal>
    <horizontal id="viewStatus" spacing="1" pref-height="10" child-expand-width="false">
      <button text="Fade -" on-click="OnOpacityDown" pref-width="28" hover-hint="Decrease opacity"/>
      <button text="Fade +" on-click="OnOpacityUp" pref-width="28" hover-hint="Increase opacity"/>
      <text id="opacityText" text="100%" font-size="3.5" pref-width="18"/>
      <text id="shapeText" text="Free / 100%" font-size="3.5" pref-width="62"/>
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
    screen->history = Config::GetHistory();
    screen->canvasGroup->alpha = Config::GetOpacity();
    screen->ChangeOpacity(0.0f);
    for (auto row : {screen->mainControls, screen->groupControls, screen->browseControls, screen->viewControls, screen->viewStatus}) {
        if (!row) continue;
        for (int i = 0; i < row->transform->childCount; ++i) {
            auto child = row->transform->GetChild(i);
            auto layout = child->GetComponent<UI::LayoutElement*>();
            if (layout) screen->controlSizes.push_back({layout, layout->preferredWidth});
            auto label = child->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
            if (label) screen->controlLabels.push_back(label);
        }
    }
    screen->RefreshControls();
    screen->BuildHistory();
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
    UpdateControlScale();
    UpdateTexture();
    UpdateInput();
    UpdateLabels();
    viewportTimer -= Time::get_unscaledDeltaTime();
    if (viewportDirty && viewportTimer <= 0) ApplyViewport();
}

void BrowserScreen::SetDimensions(Resize::Dimensions size) {
    if (dimensions.width == size.width && dimensions.height == size.height && !viewportDirty) return;
    dimensions = size;
    if (pageImage) {
        auto layout = pageImage->gameObject->GetComponent<UI::LayoutElement*>();
        if (layout) {
            layout->preferredWidth = size.width - Resize::kPaddingWidth;
            layout->preferredHeight = std::max(81.0f, size.height - GetChromeHeight());
            layout->minWidth = 0;
            layout->minHeight = 0;
        }
    }
    if (historyPanel) {
        auto layout = historyPanel->GetComponent<UI::LayoutElement*>();
        if (layout) {
            layout->preferredWidth = size.width - Resize::kPaddingWidth;
            layout->preferredHeight = std::max(81.0f, size.height - GetChromeHeight());
        }
    }
    if (keyboard) keyboard->SetWindowHeight(size.height);
    viewportDirty = true;
}

void BrowserScreen::ApplyViewport() {
    if (tracker.IsDown()) {
        auto p = tracker.LastPixel();
        Jni::Touch(3, p.x, p.y);
        tracker = {};
    }
    float w = (dimensions.width - Resize::kPaddingWidth) * (1280.0f / 144.0f) / Config::GetZoom();
    float h = std::max(81.0f, dimensions.height - GetChromeHeight()) * (1280.0f / 144.0f) / Config::GetZoom();
    float limit = std::min(1.0f, 4096.0f / std::max(w, h));
    Jni::SetViewport(static_cast<int>(w * limit + 0.5f), static_cast<int>(h * limit + 0.5f));
    viewportDirty = false;
    viewportTimer = 0.15f;
    constexpr char const* names[] = {"Free", "16:9", "4:3", "Portrait"};
    if (shapeText) shapeText->text = fmt::format("{} / {}%", names[Window::GetAspectPreset()], static_cast<int>(Config::GetZoom() * 100 + 0.5f));
}

void BrowserScreen::OnAspect() { Window::CycleAspectPreset(); viewportDirty = true; }
void BrowserScreen::ChangeZoom(float delta) { Config::SetZoom(Config::GetZoom() + delta); viewportDirty = true; }
void BrowserScreen::OnZoomDown() { ChangeZoom(-0.1f); }
void BrowserScreen::OnZoomUp() { ChangeZoom(0.1f); }

float BrowserScreen::GetChromeHeight() const {
    int rows = 1 + (menuOpen ? (viewOpen ? 3 : 2) : 0);
    return 12.0f + rows * (10.0f * controlScale + 1.0f);
}

void BrowserScreen::SetChromeHidden(bool hidden) {
    if (hidden == chromeHidden) return;
    chromeHidden = hidden;
    if (hidden && historyPanel && historyPanel->activeSelf) OnHistory();
    RefreshControls();
}

void BrowserScreen::RefreshControls() {
    bool show = !chromeHidden;
    if (mainControls) mainControls->gameObject->SetActive(show);
    if (titleText) titleText->gameObject->SetActive(show);
    if (groupControls) groupControls->gameObject->SetActive(show && menuOpen);
    if (browseControls) browseControls->gameObject->SetActive(show && menuOpen && !viewOpen);
    if (viewControls) viewControls->gameObject->SetActive(show && menuOpen && viewOpen);
    if (viewStatus) viewStatus->gameObject->SetActive(show && menuOpen && viewOpen);
    for (auto& size : controlSizes) {
        size.layout->preferredWidth = size.width * controlScale;
        size.layout->minWidth = size.width * controlScale;
        size.layout->preferredHeight = 10 * controlScale;
        size.layout->minHeight = 10 * controlScale;
    }
    for (auto label : controlLabels) if (label) {
        label->enableAutoSizing = false;
        label->fontSize = 3.5f * controlScale;
    }
    for (auto row : {mainControls, groupControls, browseControls, viewControls, viewStatus}) if (row) {
        auto layout = row->GetComponent<UI::LayoutElement*>();
        if (layout) {
            layout->preferredHeight = 10 * controlScale;
            layout->minHeight = 10 * controlScale;
        }
        row->spacing = controlScale;
    }
    viewportDirty = true;
    Window::FitControls();
}

void BrowserScreen::UpdateControlScale() {
    auto camera = Camera::get_main();
    if (!camera) return;
    auto offset = Vector3::op_Subtraction(transform->position, camera->transform->position);
    float next = Resize::ControlScale(offset.magnitude, transform->lossyScale.x);
    if (std::fabs(next - controlScale) < 0.05f) return;
    controlScale = next;
    RefreshControls();
}

void BrowserScreen::OnMenu() { menuOpen = !menuOpen; RefreshControls(); }
void BrowserScreen::OnBrowseGroup() { viewOpen = false; RefreshControls(); }
void BrowserScreen::OnViewGroup() { viewOpen = true; RefreshControls(); }

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
    if (pageImage->gameObject->activeInHierarchy && over && over.ptr() == pageImage->gameObject.ptr()) {
        auto rectTransform = pageImage->rectTransform;
        auto local = rectTransform->InverseTransformPoint(pointer->cursorPosition);
        auto r = rectTransform->rect;
        hit = Logic::LocalToPixel({r.m_XMin, r.m_YMin, r.m_Width, r.m_Height}, local.x, local.y, kPageWidth, kPageHeight);
    }

    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (!controller) return;
    float trigger = controller->triggerValue;
    bool wasHeld = triggerHeld;
    triggerHeld = triggerHeld ? trigger > kTriggerRelease : trigger > kTriggerPress;
    if (triggerHeld && !wasHeld && keyboard && keyboard->IsVisible()) {
        auto target = over ? over->transform : nullptr;
        bool inWindow = target && target->IsChildOf(transform);
        bool inKeyboard = target && target->IsChildOf(keyboard->transform);
        if (!inWindow && !inKeyboard) HideKeyboard();
    }

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

    if (Jni::GetProgress() == 100 && !url.empty() && url != lastHistoryUrl) {
        lastHistoryUrl = url;
        Logic::RememberUrl(history, url);
        Config::SetHistory(history);
        if (historyPanel && historyPanel->activeSelf) RefreshHistory();
    }

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

void BrowserScreen::BuildHistory() {
    if (!pageImage) return;
    auto panel = BSML::Lite::CreateVerticalLayoutGroup(pageImage->transform->parent);
    panel->spacing = 1.0f;
    panel->childForceExpandHeight = false;
    panel->childControlHeight = true;
    auto size = panel->gameObject->GetComponent<UI::LayoutElement*>();
    if (!size) size = panel->gameObject->AddComponent<UI::LayoutElement*>();
    size->preferredHeight = 81.0f;
    size->preferredWidth = 144.0f;
    historyPanel = panel->gameObject;
    historyStatus = BSML::Lite::CreateText(panel->transform, "History", 3.5f);
    for (std::size_t row = 0; row < 5; ++row) {
        auto line = BSML::Lite::CreateHorizontalLayoutGroup(panel->transform);
        line->childForceExpandWidth = false;
        auto lineSize = line->gameObject->GetComponent<UI::LayoutElement*>();
        if (!lineSize) lineSize = line->gameObject->AddComponent<UI::LayoutElement*>();
        lineSize->preferredHeight = 11.0f;
        lineSize->minHeight = 11.0f;
        auto open = BSML::Lite::CreateUIButton(line->transform, "", Vector2(0, 0), Vector2(116, 10), [this, row]() {
            auto index = historyPage * 5 + row;
            if (index >= history.size()) return;
            auto url = history[index];
            OnHistory();
            Jni::LoadUrl(url);
        });
        auto label = open->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if (label) { label->richText = false; label->fontSize = 2.7f; label->enableWordWrapping = false; }
        historyLabels.push_back(label);
        historyOpenButtons.push_back(open);
        historyDeleteButtons.push_back(BSML::Lite::CreateUIButton(line->transform, "Delete", Vector2(0, 0), Vector2(25, 10), [this, row]() {
            auto index = historyPage * 5 + row;
            if (index >= history.size()) return;
            history.erase(history.begin() + index);
            Config::SetHistory(history);
            RefreshHistory();
        }));
        // Layout groups use LayoutElement sizes rather than the button's rect size.
        for (auto button : {open, historyDeleteButtons.back().ptr()}) {
            auto layout = button->gameObject->GetComponent<UI::LayoutElement*>();
            if (!layout) layout = button->gameObject->AddComponent<UI::LayoutElement*>();
            layout->preferredWidth = button == open ? 116.0f : 25.0f;
            layout->minWidth = layout->preferredWidth;
            layout->flexibleWidth = 0.0f;
            layout->preferredHeight = 10.0f;
        }
    }
    auto controls = BSML::Lite::CreateHorizontalLayoutGroup(panel->transform);
    BSML::Lite::CreateUIButton(controls->transform, "Newer", [this]() {
        if (historyPage) --historyPage;
        RefreshHistory();
    });
    BSML::Lite::CreateUIButton(controls->transform, "Older", [this]() {
        if ((historyPage + 1) * 5 < history.size()) ++historyPage;
        RefreshHistory();
    });
    BSML::Lite::CreateUIButton(controls->transform, "Clear all", [this]() {
        history.clear();
        Config::SetHistory(history);
        RefreshHistory();
    });
    BSML::Lite::CreateUIButton(controls->transform, "Close", [this]() { OnHistory(); });
    historyPanel->SetActive(false);
}

void BrowserScreen::RefreshHistory() {
    historyPage = std::min(historyPage, history.empty() ? std::size_t(0) : (history.size() - 1) / 5);
    if (historyStatus) historyStatus->text = history.empty() ? "No browsing history" : fmt::format("History - page {}/{} ({} URLs)", historyPage + 1, (history.size() + 4) / 5, history.size());
    for (std::size_t row = 0; row < historyLabels.size(); ++row) {
        auto index = historyPage * 5 + row;
        bool exists = index < history.size();
        historyOpenButtons[row]->interactable = exists;
        historyDeleteButtons[row]->interactable = exists;
        if (historyLabels[row]) historyLabels[row]->text = exists ? history[index] : "";
    }
}

void BrowserScreen::OnHistory() {
    if (!historyPanel || !pageImage) return;
    HideKeyboard();
    bool show = !historyPanel->activeSelf;
    pageImage->gameObject->SetActive(!show);
    historyPanel->SetActive(show);
    if (show) RefreshHistory();
}

void BrowserScreen::ChangeOpacity(float delta) {
    if (delta != 0.0f) Config::SetOpacity(Config::GetOpacity() + delta);
    if (canvasGroup) canvasGroup->alpha = Config::GetOpacity();
    if (opacityText) opacityText->text = fmt::format("{}%", static_cast<int>(Config::GetOpacity() * 100.0f + 0.5f));
}
void BrowserScreen::OnOpacityDown() { ChangeOpacity(-0.1f); }
void BrowserScreen::OnOpacityUp() { ChangeOpacity(0.1f); }

}  // namespace QuestBrowser
