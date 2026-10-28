#include "BrowserScreen.hpp"

#include <chrono>

#include "BrowserWindow.hpp"
#include "Config.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/XR/XRNode.hpp"
#include "JniBridge.hpp"
#include "songcore/shared/SongCore.hpp"
#include "System/IntPtr.hpp"
#include "TMPro/FontStyles.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "TMPro/TextOverflowModes.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
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

// Rows per page in the tabs and history panels.
constexpr std::size_t kRows = 5;

Color const kAccent{0.25f, 0.85f, 1.0f, 1.0f};
Color const kWhite{1.0f, 1.0f, 1.0f, 1.0f};

// Shared by every BrowserScreen: the page runs while any screen is visible.
Logic::ActiveCounter activeScreens;

// Only the page. Panels are added next to it in code; the toolbar floats below.
constexpr std::string_view kLayout = R"xml(
<bg>
  <vertical pad="3" child-align="MiddleCenter" child-expand-height="false" child-control-height="true">
    <raw-image id="pageImage" pref-width="144" pref-height="81"/>
  </vertical>
</bg>
)xml";

void SetSize(Component* c, float width, float height) {
    auto go = c->gameObject;
    auto le = go->GetComponent<UI::LayoutElement*>();
    if (!le) le = go->AddComponent<UI::LayoutElement*>();
    le->preferredWidth = width;
    le->minWidth = width;
    le->flexibleWidth = 0.0f;
    le->preferredHeight = height;
    le->minHeight = height;
}

UI::Button* Button(Transform* parent, std::string_view text, float width, std::function<void()> onClick) {
    auto button = BSML::Lite::CreateUIButton(parent, std::string(text), Vector2(0, 0), Vector2(width, 9), std::move(onClick));
    SetSize(button, width, 9.0f);
    if (auto label = button->GetComponentInChildren<TMPro::TextMeshProUGUI*>()) {
        label->richText = false;
        label->fontStyle = TMPro::FontStyles::Normal;
        label->enableWordWrapping = false;
        label->overflowMode = TMPro::TextOverflowModes::Ellipsis;
        label->fontSize = 3.4f;
    }
    return button;
}

UI::HorizontalLayoutGroup* Row(Transform* parent) {
    auto row = BSML::Lite::CreateHorizontalLayoutGroup(parent);
    row->spacing = 1.0f;
    row->childForceExpandWidth = false;
    row->childControlWidth = true;
    row->childAlignment = TextAnchor::MiddleCenter;
    SetSize(row, 136.0f, 10.0f);
    return row;
}

TMPro::TextMeshProUGUI* Label(Transform* parent, std::string_view text, float width) {
    auto label = BSML::Lite::CreateText(parent, std::string(text), 3.4f);
    label->fontStyle = TMPro::FontStyles::Normal;
    label->alignment = TMPro::TextAlignmentOptions::Center;
    label->enableWordWrapping = false;
    SetSize(label, width, 9.0f);
    return label;
}

}  // namespace

BrowserScreen* BrowserScreen::CreateIn(Transform* parent) {
    auto go = parent->gameObject;
    auto screen = go->AddComponent<BrowserScreen*>();
    screen->canvasGroup = go->GetComponent<CanvasGroup*>();
    if (!screen->canvasGroup) screen->canvasGroup = go->AddComponent<CanvasGroup*>();
    BSML::parse_and_construct(kLayout, parent, screen);
    screen->history = Config::GetHistory();
    screen->bookmarks = Config::GetBookmarks();
    screen->canvasGroup->alpha = Config::GetOpacity(Window::CurrentSlot());
    // Order of creation follows the Panel enum: Tabs, Menu, History, Bookmarks.
    screen->BuildTabs();
    screen->BuildMenu();
    screen->BuildHistory();
    screen->BuildBookmarks();
    auto parentRect = parent->GetComponent<RectTransform*>();
    screen->keyboard = KeyboardPanel::Create(parent, parentRect ? parentRect->rect.height : 87.0f);
    screen->toolbar = ToolbarPanel::Create(parent, screen->keyboard.ptr(), screen->MakeToolbarActions());
    screen->Setup();
    return screen;
}

ToolbarActions BrowserScreen::MakeToolbarActions() {
    ToolbarActions a;
    a.back = []() { Jni::GoBack(); };
    a.forward = []() { Jni::GoForward(); };
    a.reload = []() { Jni::Reload(); };
    a.editUrl = [this]() { OpenUrlEditing(); };
    a.toggleTabs = [this]() { ShowPanel(openPanel == Panel::Tabs ? Panel::None : Panel::Tabs); };
    a.toggleMenu = [this]() { ShowPanel(openPanel == Panel::Menu ? Panel::None : Panel::Menu); };
    a.label = [this]() { return ToolbarLabel(); };
    a.tabCount = [this]() { return static_cast<int>(std::max<std::size_t>(1, tabUrls.size())); };
    // Also stay while a map downloads, so that its progress shows.
    a.pinned = [this]() { return openPanel != Panel::None || (keyboard && keyboard->IsVisible()) || !songStatus.empty() || zoomLabelSeconds > 0.0f; };
    a.interactive = [this]() { return interactive && !chromeHidden; };
    return a;
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
        EndGrab();
    }
    if (ready && isActiveAndEnabled) Jni::SetFrameRate(Logic::FrameRateFor(value));
}

void BrowserScreen::SetChromeHidden(bool hidden) {
    if (hidden == chromeHidden) return;
    chromeHidden = hidden;
    if (hidden) ShowPanel(Panel::None);
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
    ready = Jni::Init(kPageWidth, kPageHeight, {"https://www.youtube.com/embed/" QB_STR(QB_DEBUG_YOUTUBE_ID) "?autoplay=1"}, 0);
#else
    // Restore the tabs of the last session.
    auto tabs = Tabs::Restore(Config::GetTabs(), Config::GetActiveTab(), Logic::kHomeUrl);
    ready = Jni::Init(kPageWidth, kPageHeight, tabs.urls, tabs.active);
    logger.info("Restored {} tabs, tab {} open", tabs.urls.size(), tabs.active + 1);
#endif
    if (!ready) {
        logger.error("Browser failed to start");
        return;
    }
    Jni::SetSongsDir(SongCore::API::Loading::GetPreferredCustomLevelPath().string());
    // The folder that PlaylistCore (PlaylistManager) loads playlists from. It reloads them when SongCore reloads.
    Jni::SetPlaylistsDir((std::filesystem::path(getDataDir("PlaylistManager")) / "Playlists").string());
#ifdef QB_DEBUG_SHOW_KEYBOARD
    // Debug builds (qpm s build -D QB_DEBUG_SHOW_KEYBOARD=1): open the keyboard at start, for screenshots.
    if (keyboard) keyboard->SetVisible(true);
#endif
    OnEnable();
}

void BrowserScreen::OnEnable() {
    // The toolbar is not a child of the window, so show it with the window.
    if (toolbar) toolbar->SetShown(true);
    if (!ready || counted || !isActiveAndEnabled) return;
    counted = true;
    if (auto change = activeScreens.Add()) Jni::SetActive(*change);
    Jni::SetFrameRate(Logic::FrameRateFor(interactive));
}

void BrowserScreen::OnDisable() {
    // The keyboard and the toolbar are not children of the window, so hide them with it.
    if (keyboard) keyboard->SetVisible(false);
    if (toolbar) toolbar->SetShown(false);
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
    EndGrab();
}

void BrowserScreen::Update() {
    // The window: faster push and pull while its handle is held.
    Window::BoostHandleDepth(this);
    if (!ready) return;
    UpdateTexture();
    UpdateInput();
    UpdateLabels();
    viewportTimer -= Time::get_unscaledDeltaTime();
    zoomLabelSeconds -= Time::get_unscaledDeltaTime();
    if (viewportDirty && viewportTimer <= 0) ApplyViewport();
}

void BrowserScreen::SetDimensions(Resize::Dimensions size) {
    if (dimensions.width == size.width && dimensions.height == size.height && !viewportDirty) return;
    dimensions = size;
    float pageWidth = size.width - Resize::kPaddingWidth;
    float pageHeight = std::max(81.0f, size.height - GetChromeHeight());
    if (pageImage) {
        auto layout = pageImage->gameObject->GetComponent<UI::LayoutElement*>();
        if (layout) {
            layout->preferredWidth = pageWidth;
            layout->preferredHeight = pageHeight;
            layout->minWidth = 0;
            layout->minHeight = 0;
        }
    }
    for (auto& panel : panelSizes) {
        if (!panel) continue;
        panel->preferredWidth = pageWidth;
        panel->preferredHeight = pageHeight;
    }
    // Panel text and buttons grow with the window.
    float panelScale = Resize::PanelScale(pageWidth, pageHeight);
    for (auto& content : panelContents)
        if (content) content->localScale = Vector3(panelScale, panelScale, 1.0f);
    if (keyboard) keyboard->SetWindowHeight(size.height);
    if (toolbar) toolbar->SetWindowSize(size.width, size.height);
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
    RefreshMenu();
}

void BrowserScreen::ChangeZoom(float delta) {
    Config::SetZoom(Config::GetZoom() + delta);
    viewportDirty = true;
    RefreshMenu();
}

void BrowserScreen::ChangeOpacity(float delta) {
    auto slot = Window::CurrentSlot();
    if (delta != 0.0f) Config::SetOpacity(slot, Config::GetOpacity(slot) + delta);
    // While a panel is open the window stays opaque; the new opacity applies when it closes.
    if (canvasGroup) canvasGroup->alpha = openPanel == Panel::None ? Config::GetOpacity(slot) : 1.0f;
    RefreshMenu();
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
    if (pageImage->gameObject->activeInHierarchy && over && over.ptr() == pageImage->gameObject.ptr()) {
        auto rectTransform = pageImage->rectTransform;
        auto local = rectTransform->InverseTransformPoint(pointer->cursorPosition);
        auto r = rectTransform->rect;
        hit = Logic::LocalToPixel({r.m_XMin, r.m_YMin, r.m_Width, r.m_Height}, local.x, local.y, kPageWidth, kPageHeight);
    }

    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (!controller) return;
    auto overTarget = over ? over->transform : nullptr;
    bool overWindow = overTarget && (overTarget->IsChildOf(transform) || (toolbar && overTarget->IsChildOf(toolbar->transform)));
    if (UpdateGrab(controller.ptr(), overWindow)) return;
    float trigger = controller->triggerValue;
    bool wasHeld = triggerHeld;
    triggerHeld = triggerHeld ? trigger > kTriggerRelease : trigger > kTriggerPress;
    // A click outside the window, the toolbar and the keyboard closes the keyboard.
    if (triggerHeld && !wasHeld && keyboard && keyboard->IsVisible()) {
        auto target = over ? over->transform : nullptr;
        bool inside = target && (target->IsChildOf(transform) || target->IsChildOf(keyboard->transform) ||
                                 (toolbar && target->IsChildOf(toolbar->transform)));
        if (!inside) HideKeyboard();
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

    // While the laser is on the page, unless a drag is active: the right thumbstick scrolls
    // the page under the pointer, the left thumbstick zooms.
    auto touch = GlobalNamespace::OVRInput::Controller::Touch;
    bool onPage = hit && !tracker.IsDown();
    int zoomStep = zoomStepper.Update(
        onPage ? GlobalNamespace::OVRInput::Get(GlobalNamespace::OVRInput::RawAxis2D::LThumbstick, touch).y : 0.0f,
        Time::get_unscaledDeltaTime());
    if (zoomStep != 0) {
        ChangeZoom(0.1f * zoomStep);
        zoomLabelSeconds = 1.5f;
    }
    if (onPage) {
        float stick = GlobalNamespace::OVRInput::Get(GlobalNamespace::OVRInput::RawAxis2D::RThumbstick, touch).y;
        float amount = Logic::ThumbstickToScroll(stick, 0.2f, kScrollSpeed);
        if (amount != 0.0f) Jni::Scroll(hit->x, hit->y, amount);
    }
}

namespace {
float GripValue(GlobalNamespace::VRController* controller) {
    using GlobalNamespace::OVRInput;
    bool left = controller->node == UnityEngine::XR::XRNode::LeftHand;
    return OVRInput::Get(left ? OVRInput::RawAxis1D::LHandTrigger : OVRInput::RawAxis1D::RHandTrigger, OVRInput::Controller::Touch);
}
}  // namespace

// Press the grip while the laser is on the window or the toolbar: the window follows the
// controller until the grip is released, and the thumbstick of that hand pushes it away
// or pulls it closer.
bool BrowserScreen::UpdateGrab(GlobalNamespace::VRController* controller, bool overWindow) {
    if (grabController) {
        if (!grip.Update(GripValue(grabController.ptr()))) {
            EndGrab();
            return false;
        }
        auto hand = grabController->transform;
        grabPosition.z = Controls::GrabDepth(grabPosition.z, grabController->thumbstick.y, Time::get_unscaledDeltaTime());
        transform->SetPositionAndRotation(hand->TransformPoint(grabPosition), Quaternion::op_Multiply(hand->rotation, grabRotation));
        return true;
    }
    bool wasHeld = grip.Held();
    if (!grip.Update(GripValue(controller)) || wasHeld || !overWindow) return false;
    // Start: cancel a touch on the page, and keep the window pose relative to the controller.
    if (tracker.IsDown()) {
        auto p = tracker.LastPixel();
        Jni::Touch(3, p.x, p.y);
    }
    tracker = {};
    auto hand = controller->transform;
    grabController = controller;
    grabPosition = hand->InverseTransformPoint(transform->position);
    grabRotation = Quaternion::op_Multiply(Quaternion::Inverse(hand->rotation), transform->rotation);
    logger.info("Grabbed the window with the {} grip", controller->node == UnityEngine::XR::XRNode::LeftHand ? "left" : "right");
    return true;
}

void BrowserScreen::EndGrab() {
    if (!grabController) return;
    grabController = nullptr;
    Window::SavePosition();
}

void BrowserScreen::UpdateLabels() {
    labelTimer -= Time::get_deltaTime();
    if (labelTimer > 0.0f) return;
    labelTimer = 0.1f;
    UpdateTabs();

    if (auto submitted = Jni::TakeSubmittedUrl()) {
        auto url = Logic::NormalizeUrl(*submitted);
        logger.info("Loading {}", url);
        Jni::LoadUrl(url);
        if (keyboard) keyboard->SetVisible(false);
    }
    // The user tapped a text field in the page.
    if (Jni::TakeKeyboardRequest() && interactive && keyboard) keyboard->SetVisible(true);

    // Maps installed from the browser show up in the song list after SongCore reloads.
    songStatus = Jni::GetSongStatus();
    songRefresh.Installed(Jni::TakeSongsInstalled());
    bool inMenu = Window::CurrentContext() == Logic::Context::Menu;
    if (songRefresh.Take(inMenu, SongCore::API::Loading::AreSongsRefreshing())) {
        logger.info("Reloading songs for new maps");
        SongCore::API::Loading::RefreshSongs(false);
    }

    auto url = Jni::GetUrl();
    if (Jni::GetProgress() == 100 && !url.empty() && url != lastHistoryUrl) {
        lastHistoryUrl = url;
        Logic::RememberUrl(history, url);
        Config::SetHistory(history);
        if (openPanel == Panel::History) RefreshHistory();
    }
}

// Text in the toolbar: the address being typed, else the map download status, else the
// page title (or address).
std::string BrowserScreen::ToolbarLabel() {
    if (auto draft = Jni::GetUrlDraft()) return fmt::format("{}_", *draft);
    if (zoomLabelSeconds > 0.0f) return fmt::format("Zoom {}%", static_cast<int>(Config::GetZoom() * 100 + 0.5f));
    if (!songStatus.empty()) return songStatus;
    auto title = Jni::GetTitle();
    std::string label = title.empty() ? Jni::GetUrl() : title;
    int progress = Jni::GetProgress();
    if (progress < 100) label = fmt::format("{}%  {}", progress, label);
    return label;
}

void BrowserScreen::OpenUrlEditing() {
    ShowPanel(Panel::None);
    Jni::BeginUrlEditing(Jni::GetUrl());
    if (keyboard) keyboard->SetVisible(true);
}

void BrowserScreen::ToggleKeyboard() {
    if (keyboard && keyboard->IsVisible()) HideKeyboard();
    else if (keyboard) keyboard->SetVisible(true);
}

void BrowserScreen::HideKeyboard() {
    if (ready) Jni::CancelUrlEditing();
    if (keyboard) keyboard->SetVisible(false);
}

// ---- Panels ----------------------------------------------------------------

// A panel is a box the size of the page area, holding content designed at 144 x 81
// that is scaled to fill the box (SetDimensions). Returns the content.
GameObject* BrowserScreen::MakePanel() {
    auto root = GameObject::New_ctor("QuestBrowserPanel");
    root->transform->SetParent(pageImage->transform->parent, false);
    root->AddComponent<RectTransform*>();
    auto size = root->AddComponent<UI::LayoutElement*>();
    size->preferredWidth = 144.0f;
    size->preferredHeight = 81.0f;

    auto content = BSML::Lite::CreateVerticalLayoutGroup(root->transform);
    content->spacing = 1.0f;
    content->childForceExpandHeight = false;
    content->childControlHeight = true;
    content->childAlignment = TextAnchor::MiddleCenter;
    if (auto fitter = content->GetComponent<UI::ContentSizeFitter*>()) fitter->enabled = false;
    auto rect = content->rectTransform;
    rect->anchorMin = {0.5f, 0.5f};
    rect->anchorMax = {0.5f, 0.5f};
    rect->pivot = {0.5f, 0.5f};
    rect->anchoredPosition = {0.0f, 0.0f};
    rect->sizeDelta = {144.0f, 81.0f};

    panels.push_back(root);
    panelSizes.push_back(size);
    panelContents.push_back(rect);
    root->SetActive(false);
    return content->gameObject;
}

void BrowserScreen::ShowPanel(Panel panel) {
    if (panel != Panel::None) HideKeyboard();
    openPanel = panel;
    for (std::size_t i = 0; i < panels.size(); ++i)
        if (panels[i]) panels[i]->SetActive(static_cast<int>(panel) == static_cast<int>(i) + 1);
    if (pageImage) pageImage->gameObject->SetActive(panel == Panel::None);
    // Panels are fully opaque, so they are easy to read; the page uses the opacity setting.
    if (canvasGroup) canvasGroup->alpha = panel == Panel::None ? Config::GetOpacity(Window::CurrentSlot()) : 1.0f;
    if (panel == Panel::Tabs) RefreshTabs();
    if (panel == Panel::Menu) RefreshMenu();
    if (panel == Panel::History) RefreshHistory();
    if (panel == Panel::Bookmarks) RefreshBookmarks();
}

void BrowserScreen::BuildMenu() {
    auto panel = MakePanel()->transform;
    Label(panel, "Menu", 60.0f);

    auto nav = Row(panel);
    Button(nav->transform, "Home", 33.0f, [this]() {
        ShowPanel(Panel::None);
        Jni::LoadUrl(Logic::kHomeUrl);
    });
    Button(nav->transform, "History", 33.0f, [this]() { ShowPanel(Panel::History); });
    Button(nav->transform, "Bookmarks", 33.0f, [this]() { ShowPanel(Panel::Bookmarks); });
    Button(nav->transform, "Keyboard", 33.0f, [this]() {
        ShowPanel(Panel::None);
        ToggleKeyboard();
    });

    auto sites = Row(panel);
    for (auto const& site : Songs::kMapSites) {
        Button(sites->transform, site.name, 33.0f, [this, url = site.url]() {
            ShowPanel(Panel::None);
            Jni::LoadUrl(url);
        });
    }

    auto zoom = Row(panel);
    Button(zoom->transform, "-", 14.0f, [this]() { ChangeZoom(-0.1f); });
    zoomText = Label(zoom->transform, "", 44.0f);
    Button(zoom->transform, "+", 14.0f, [this]() { ChangeZoom(0.1f); });

    auto fade = Row(panel);
    Button(fade->transform, "-", 14.0f, [this]() { ChangeOpacity(-0.1f); });
    opacityText = Label(fade->transform, "", 44.0f);
    Button(fade->transform, "+", 14.0f, [this]() { ChangeOpacity(0.1f); });

    auto shape = Row(panel);
    aspectText = Label(shape->transform, "", 44.0f);
    Button(shape->transform, "Change", 28.0f, [this]() {
        Window::CycleAspectPreset();
        viewportDirty = true;
        RefreshMenu();
    });
    Button(shape->transform, "Close", 28.0f, [this]() { ShowPanel(Panel::None); });
}

void BrowserScreen::RefreshMenu() {
    constexpr char const* names[] = {"Free", "16:9", "4:3", "Portrait"};
    if (zoomText) zoomText->text = fmt::format("Zoom {}%", static_cast<int>(Config::GetZoom() * 100 + 0.5f));
    if (opacityText) opacityText->text = fmt::format("Opacity {}%", static_cast<int>(Config::GetOpacity(Window::CurrentSlot()) * 100 + 0.5f));
    if (aspectText) aspectText->text = fmt::format("Shape: {}", names[std::clamp(Window::GetAspectPreset(), 0, 3)]);
}

void BrowserScreen::BuildHistory() {
    auto panel = MakePanel()->transform;
    historyStatus = Label(panel, "History", 120.0f);
    for (std::size_t row = 0; row < kRows; ++row) {
        auto line = Row(panel);
        auto open = Button(line->transform, "", 108.0f, [this, row]() {
            auto index = historyPage * kRows + row;
            if (index >= history.size()) return;
            auto url = history[index];
            ShowPanel(Panel::None);
            Jni::LoadUrl(url);
        });
        historyLabels.push_back(open->GetComponentInChildren<TMPro::TextMeshProUGUI*>());
        historyOpenButtons.push_back(open);
        historyDeleteButtons.push_back(Button(line->transform, "Delete", 26.0f, [this, row]() {
            auto index = historyPage * kRows + row;
            if (index >= history.size()) return;
            history.erase(history.begin() + index);
            Config::SetHistory(history);
            RefreshHistory();
        }));
    }
    auto controls = Row(panel);
    Button(controls->transform, "Newer", 30.0f, [this]() {
        if (historyPage) --historyPage;
        RefreshHistory();
    });
    Button(controls->transform, "Older", 30.0f, [this]() {
        if ((historyPage + 1) * kRows < history.size()) ++historyPage;
        RefreshHistory();
    });
    Button(controls->transform, "Clear all", 34.0f, [this]() {
        history.clear();
        Config::SetHistory(history);
        RefreshHistory();
    });
    Button(controls->transform, "Close", 30.0f, [this]() { ShowPanel(Panel::None); });
}

void BrowserScreen::RefreshHistory() {
    historyPage = std::min(historyPage, history.empty() ? std::size_t(0) : (history.size() - 1) / kRows);
    if (historyStatus)
        historyStatus->text = history.empty() ? "No history"
                                              : fmt::format("History  {}/{}", historyPage + 1, (history.size() + kRows - 1) / kRows);
    for (std::size_t row = 0; row < historyLabels.size(); ++row) {
        auto index = historyPage * kRows + row;
        bool exists = index < history.size();
        historyOpenButtons[row]->gameObject->SetActive(exists);
        historyDeleteButtons[row]->gameObject->SetActive(exists);
        if (historyLabels[row]) historyLabels[row]->text = exists ? history[index] : "";
    }
}

// ---- Bookmarks -------------------------------------------------------------

void BrowserScreen::BuildBookmarks() {
    auto panel = MakePanel()->transform;
    bookmarksStatus = Label(panel, "Bookmarks", 120.0f);
    for (std::size_t row = 0; row < kRows; ++row) {
        auto line = Row(panel);
        auto open = Button(line->transform, "", 108.0f, [this, row]() {
            auto index = bookmarksPage * kRows + row;
            if (index >= bookmarks.size()) return;
            auto url = bookmarks[index].url;
            ShowPanel(Panel::None);
            Jni::LoadUrl(url);
        });
        bookmarkLabels.push_back(open->GetComponentInChildren<TMPro::TextMeshProUGUI*>());
        bookmarkOpenButtons.push_back(open);
        bookmarkDeleteButtons.push_back(Button(line->transform, "Delete", 26.0f, [this, row]() {
            auto index = bookmarksPage * kRows + row;
            if (index >= bookmarks.size()) return;
            bookmarks.erase(bookmarks.begin() + index);
            Config::SetBookmarks(bookmarks);
            RefreshBookmarks();
        }));
    }
    auto controls = Row(panel);
    Button(controls->transform, "Add this page", 34.0f, [this]() {
        Bookmarks::Add(bookmarks, Jni::GetUrl(), Jni::GetTitle());
        Config::SetBookmarks(bookmarks);
        bookmarksPage = 0;
        RefreshBookmarks();
    });
    Button(controls->transform, "Newer", 30.0f, [this]() {
        if (bookmarksPage) --bookmarksPage;
        RefreshBookmarks();
    });
    Button(controls->transform, "Older", 30.0f, [this]() {
        if ((bookmarksPage + 1) * kRows < bookmarks.size()) ++bookmarksPage;
        RefreshBookmarks();
    });
    Button(controls->transform, "Close", 30.0f, [this]() { ShowPanel(Panel::None); });
}

void BrowserScreen::RefreshBookmarks() {
    bookmarksPage = std::min(bookmarksPage, bookmarks.empty() ? std::size_t(0) : (bookmarks.size() - 1) / kRows);
    if (bookmarksStatus)
        bookmarksStatus->text = bookmarks.empty() ? "No bookmarks"
                                                  : fmt::format("Bookmarks  {}/{}", bookmarksPage + 1, (bookmarks.size() + kRows - 1) / kRows);
    for (std::size_t row = 0; row < bookmarkLabels.size(); ++row) {
        auto index = bookmarksPage * kRows + row;
        bool exists = index < bookmarks.size();
        bookmarkOpenButtons[row]->gameObject->SetActive(exists);
        bookmarkDeleteButtons[row]->gameObject->SetActive(exists);
        if (bookmarkLabels[row]) bookmarkLabels[row]->text = exists ? Bookmarks::Label(bookmarks[index]) : "";
    }
}

// ---- Tabs ------------------------------------------------------------------

void BrowserScreen::BuildTabs() {
    auto panel = MakePanel()->transform;
    tabsStatus = Label(panel, "Tabs", 120.0f);
    for (std::size_t row = 0; row < kRows; ++row) {
        auto line = Row(panel);
        auto open = Button(line->transform, "", 112.0f, [this, row]() {
            int index = static_cast<int>(tabsPage * kRows + row);
            if (index >= static_cast<int>(tabUrls.size())) return;
            ShowPanel(Panel::None);
            if (index != activeTab) Jni::SelectTab(index);
        });
        tabLabels.push_back(open->GetComponentInChildren<TMPro::TextMeshProUGUI*>());
        tabOpenButtons.push_back(open);
        tabCloseButtons.push_back(Button(line->transform, "x", 14.0f, [this, row]() {
            CloseTab(static_cast<int>(tabsPage * kRows + row));
        }));
    }
    auto controls = Row(panel);
    newTabButton = Button(controls->transform, "+ New tab", 40.0f, [this]() {
        if (!Tabs::CanAddTab(static_cast<int>(tabUrls.size()))) return;
        ShowPanel(Panel::None);
        Jni::NewTab(Logic::kHomeUrl);
        // Ready to type where to go.
        Jni::BeginUrlEditing("");
        if (keyboard) keyboard->SetVisible(true);
    });
    Button(controls->transform, "<", 14.0f, [this]() {
        if (tabsPage) --tabsPage;
        RefreshTabs();
    });
    Button(controls->transform, ">", 14.0f, [this]() {
        if ((tabsPage + 1) * kRows < tabUrls.size()) ++tabsPage;
        RefreshTabs();
    });
    Button(controls->transform, "Close", 28.0f, [this]() { ShowPanel(Panel::None); });
}

void BrowserScreen::RefreshTabs() {
    std::size_t count = tabUrls.size();
    tabsPage = std::min(tabsPage, count == 0 ? std::size_t(0) : (count - 1) / kRows);
    if (tabsStatus) tabsStatus->text = fmt::format("Tabs  {}/{}", count, Tabs::kMaxTabs);
    for (std::size_t row = 0; row < tabLabels.size(); ++row) {
        auto index = tabsPage * kRows + row;
        bool exists = index < count;
        tabOpenButtons[row]->gameObject->SetActive(exists);
        tabCloseButtons[row]->gameObject->SetActive(exists);
        if (!exists || !tabLabels[row]) continue;
        std::string title = index < tabTitles.size() ? tabTitles[index] : "";
        tabLabels[row]->text = Tabs::Label(title, tabUrls[index], 60);
        tabLabels[row]->color = static_cast<int>(index) == activeTab ? kAccent : kWhite;
    }
    if (newTabButton) newTabButton->interactable = Tabs::CanAddTab(static_cast<int>(count));
}

void BrowserScreen::UpdateTabs() {
    int version = Jni::GetTabsVersion();
    if (version == tabsVersion) return;
    tabsVersion = version;
    auto urls = Jni::GetTabUrls();
    int active = Jni::GetActiveTab();
    tabTitles = Jni::GetTabTitles();
    // Save for the next session when the tab list changes (not for title changes).
    if (!urls.empty() && (urls != tabUrls || active != activeTab)) Config::SetTabs(urls, active);
    tabUrls = std::move(urls);
    activeTab = active;
    if (openPanel == Panel::Tabs) RefreshTabs();
}

void BrowserScreen::CloseTab(int index) {
    int count = static_cast<int>(tabUrls.size());
    if (index < 0 || index >= count) return;
    if (count <= 1) {
        // Keep one tab: closing the last one goes back to the home page.
        Jni::LoadUrl(Logic::kHomeUrl);
        return;
    }
    Jni::CloseTab(index, Tabs::ActiveAfterClose(count, activeTab, index));
}

}  // namespace QuestBrowser
