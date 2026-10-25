#include "ToolbarPanel.hpp"

#include "BrowserWindow.hpp"
#include "HMUI/ImageView.hpp"
#include "TMPro/FontStyles.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "TMPro/TextOverflowModes.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "bsml/shared/BSML/Components/ClickableImage.hpp"
#include "bsml/shared/BSML/Components/ClickableText.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "icons_logic.hpp"
#include "main.hpp"
#include "resize_logic.hpp"

DEFINE_TYPE(QuestBrowser, ToolbarPanel);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr float kHeight = 12.0f;  // toolbar units
constexpr float kIcon = 9.0f;
constexpr float kGap = 2.0f;       // window units between the window and the toolbar
constexpr float kMinWidth = 110.0f;

Color const kIconColor{1.0f, 1.0f, 1.0f, 0.85f};
Color const kAccent{0.25f, 0.85f, 1.0f, 1.0f};
Color const kBarColor{0.07f, 0.08f, 0.11f, 0.94f};

Sprite* IconSprite(Icons::Kind kind) {
    constexpr int n = 64;
    auto px = Icons::Draw(kind, n);
    ArrayW<uint8_t> data(static_cast<il2cpp_array_size_t>(px.size()));
    std::copy(px.begin(), px.end(), data.begin());
    auto tex = Texture2D::New_ctor(n, n, TextureFormat::RGBA32, false);
    tex->LoadRawTextureData(data);
    tex->Apply(false);
    return BSML::Lite::TextureToSprite(tex);
}

void Size(Component* c, float width, float height, float flexible = 0.0f) {
    auto go = c->gameObject;
    auto le = go->GetComponent<UI::LayoutElement*>();
    if (!le) le = go->AddComponent<UI::LayoutElement*>();
    le->preferredWidth = width;
    le->minWidth = flexible > 0 ? 10.0f : width;
    le->flexibleWidth = flexible;
    le->preferredHeight = height;
    le->minHeight = height;
}

}  // namespace

ToolbarPanel* ToolbarPanel::Create(Transform* window, KeyboardPanel* keyboard, ToolbarActions actions) {
    auto screen = BSML::FloatingScreen::CreateFloatingScreen({150.0f, kHeight}, false, window->position, window->rotation, 0.0f, false);
    screen->name = "QuestBrowserToolbar";
    Object::DontDestroyOnLoad(screen->gameObject);
    auto bar = screen->gameObject->AddComponent<ToolbarPanel*>();
    bar->window = window;
    bar->keyboard = keyboard;
    bar->actions = std::move(actions);
    bar->canvasGroup = screen->gameObject->AddComponent<CanvasGroup*>();
    bar->Build();
    bar->canvasGroup->alpha = 0.0f;
    bar->canvasGroup->blocksRaycasts = false;
    bar->Follow();
    return bar;
}

void ToolbarPanel::Build() {
    auto row = BSML::Lite::CreateHorizontalLayoutGroup(transform);
    auto rect = row->rectTransform;
    rect->anchorMin = {0.0f, 0.0f};
    rect->anchorMax = {1.0f, 1.0f};
    rect->sizeDelta = {0.0f, 0.0f};
    rect->anchoredPosition = {0.0f, 0.0f};
    row->padding = RectOffset::New_ctor(4, 4, 1, 1);
    row->spacing = 2.0f;
    row->childForceExpandWidth = false;
    row->childForceExpandHeight = false;
    row->childControlWidth = true;
    row->childControlHeight = true;
    row->childAlignment = TextAnchor::MiddleCenter;
    if (auto bg = row->GetComponent<BSML::Backgroundable*>()) {
        bg->ApplyBackground("round-rect-panel");
        bg->ApplyColor(kBarColor);
    }

    auto icon = [&](Icons::Kind kind, std::function<void()>* action) {
        auto image = BSML::Lite::CreateClickableImage(row->transform, IconSprite(kind), [action]() {
            if (*action) (*action)();
        });
        image->preserveAspect = true;
        image->set_defaultColor(kIconColor);
        image->set_highlightColor(kAccent);
        Size(image, kIcon, kIcon);
        return image;
    };
    icon(Icons::Kind::Back, &actions.back);
    icon(Icons::Kind::Forward, &actions.forward);
    icon(Icons::Kind::Reload, &actions.reload);

    auto url = BSML::Lite::CreateClickableText(row->transform, "", 3.6f, {0, 0}, {0, 0}, [this]() {
        if (actions.editUrl) actions.editUrl();
    });
    url->richText = false;
    url->enableWordWrapping = false;
    url->overflowMode = TMPro::TextOverflowModes::Ellipsis;
    url->alignment = TMPro::TextAlignmentOptions::MidlineLeft;
    url->fontStyle = TMPro::FontStyles::Normal;
    Size(url, 60.0f, kIcon, 1.0f);
    urlText = url;

    auto tabs = icon(Icons::Kind::Tabs, &actions.toggleTabs);
    auto count = BSML::Lite::CreateText(tabs->transform, "1", 3.2f);
    count->alignment = TMPro::TextAlignmentOptions::Center;
    count->fontStyle = TMPro::FontStyles::Normal;
    count->raycastTarget = false;
    auto countRect = count->rectTransform;
    countRect->anchorMin = {0.0f, 0.0f};
    countRect->anchorMax = {1.0f, 1.0f};
    countRect->sizeDelta = {0.0f, 0.0f};
    countRect->anchoredPosition = {0.0f, 0.0f};
    tabCountText = count;

    icon(Icons::Kind::Menu, &actions.toggleMenu);
}

void ToolbarPanel::SetWindowSize(float width, float height) {
    windowWidth = width;
    windowHeight = height;
}

void ToolbarPanel::SetShown(bool shown) { gameObject->SetActive(shown); }

bool ToolbarPanel::PointerOnUs() {
    if (!pointer || !pointer->isActiveAndEnabled) {
        pointer = Object::FindAnyObjectByType<VRUIControls::VRPointer*>();
        if (!pointer) return false;
    }
    UnityW<GameObject> over = pointer->pointingOver;
    if (!over) return false;
    auto t = over->transform;
    return (window && t->IsChildOf(window)) || t->IsChildOf(transform) || (keyboard && t->IsChildOf(keyboard->transform));
}

// Under the window, as wide as the window, scaled up when the window is far away.
void ToolbarPanel::Follow() {
    if (!window) return;
    if (auto camera = Camera::get_main()) {
        auto offset = Vector3::op_Subtraction(window->position, camera->transform->position);
        controlScale = Resize::ControlScale(offset.magnitude, window->lossyScale.x);
    }
    float width = std::max(kMinWidth, windowWidth / controlScale);
    if (auto screen = GetComponent<BSML::FloatingScreen*>()) {
        auto size = screen->get_ScreenSize();
        if (std::fabs(size.x - width) > 0.5f) screen->set_ScreenSize({width, kHeight});
    }
    float barHeight = kHeight * controlScale;  // in window units
    Vector3 center{0.0f, -(windowHeight / 2.0f + kGap + barHeight / 2.0f), 0.0f};
    transform->position = window->TransformPoint(center);
    transform->rotation = window->rotation;
    float s = window->lossyScale.x * controlScale;
    transform->localScale = Vector3(s, s, s);
    if (keyboard) keyboard->SetExtraOffset(kGap + barHeight);
}

void ToolbarPanel::LateUpdate() {
    Follow();
    float dt = Time::get_unscaledDeltaTime();
    bool interactive = !actions.interactive || actions.interactive();
    if (!interactive) fader.Hide();
    else fader.Update(PointerOnUs(), actions.pinned && actions.pinned(), dt);
    canvasGroup->alpha = fader.Alpha();
    canvasGroup->blocksRaycasts = fader.Clickable();

    labelTimer -= dt;
    if (labelTimer > 0.0f || fader.Alpha() <= 0.0f) return;
    labelTimer = 0.1f;
    if (urlText && actions.label) urlText->text = actions.label();
    if (tabCountText && actions.tabCount) tabCountText->text = std::to_string(actions.tabCount());
}

}  // namespace QuestBrowser
