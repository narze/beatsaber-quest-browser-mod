#include "KeyboardPanel.hpp"

#include "JniBridge.hpp"
#include "TMPro/FontStyles.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, KeyboardPanel);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr float kKeyUnit = 9.4f;  // width of a letter key, in UI units
constexpr float kKeyHeight = 9.0f;
constexpr float kSpacing = 0.6f;
constexpr float kPadding = 1.5f;
constexpr float kFontSize = 3.6f;
// Below the window, tilted toward the player like a laptop keyboard.
constexpr float kGap = 9.0f;  // clears the move handle at the bottom edge of the window
constexpr float kTiltDegrees = 30.0f;

Color const kNormalText{1.0f, 1.0f, 1.0f, 1.0f};
Color const kActiveText{0.25f, 0.85f, 1.0f, 1.0f};

// Short labels for keys whose names are long; the game font has no arrow glyphs.
std::string_view DisplayName(Keys::KeyDef const& key) {
    if (key.normal == "Left") return "<-";
    if (key.normal == "Right") return "->";
    if (key.normal == "Up") return "Up";
    if (key.normal == "Down") return "Dn";
    return key.normal;
}

void SetSize(Component* c, float width, float height) {
    auto go = c->gameObject;
    auto le = go->GetComponent<UI::LayoutElement*>();
    if (!le) le = go->AddComponent<UI::LayoutElement*>();
    le->minWidth = width;
    le->preferredWidth = width;
    le->flexibleWidth = 0.0f;
    le->minHeight = height;
    le->preferredHeight = height;
    le->flexibleHeight = 0.0f;
}

}  // namespace

KeyboardPanel* KeyboardPanel::Create(Transform* parent, float windowHeight) {
    auto go = GameObject::New_ctor("QuestBrowserKeyboard");
    go->transform->SetParent(parent, false);
    auto panel = go->AddComponent<KeyboardPanel*>();
    panel->Build(windowHeight);
    go->SetActive(false);
    return panel;
}

void KeyboardPanel::Build(float windowHeight) {
    auto& rows = Keys::Layout();
    float width = 0.0f;
    for (auto const& row : rows) {
        float w = 0.0f;
        for (auto const& key : row) w += key.width * kKeyUnit + kSpacing;
        width = std::max(width, w);
    }
    width += 2 * kPadding;
    float height = rows.size() * (kKeyHeight + kSpacing) + 2 * kPadding;

    auto rect = gameObject->AddComponent<RectTransform*>();
    if (!rect) rect = gameObject->GetComponent<RectTransform*>();
    rect->anchorMin = {0.5f, 0.5f};
    rect->anchorMax = {0.5f, 0.5f};
    rect->pivot = {0.5f, 1.0f};
    rect->sizeDelta = {width, height};
    rect->anchoredPosition = {0.0f, -(windowHeight / 2.0f + kGap)};
    rect->localRotation = Quaternion::Euler(kTiltDegrees, 0.0f, 0.0f);

    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(transform);
    vertical->spacing = kSpacing;
    vertical->padding = RectOffset::New_ctor(kPadding, kPadding, kPadding, kPadding);
    vertical->childForceExpandHeight = false;
    vertical->childForceExpandWidth = false;
    vertical->childControlHeight = true;
    vertical->childControlWidth = true;
    vertical->childAlignment = TextAnchor::UpperCenter;
    if (auto bg = vertical->GetComponent<BSML::Backgroundable*>()) bg->ApplyBackground("round-rect-panel");

    for (auto const& row : rows) {
        auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(vertical->transform);
        horizontal->spacing = kSpacing;
        horizontal->childForceExpandWidth = false;
        horizontal->childForceExpandHeight = false;
        horizontal->childControlWidth = true;
        horizontal->childControlHeight = true;
        horizontal->childAlignment = TextAnchor::MiddleCenter;
        SetSize(horizontal, width - 2 * kPadding, kKeyHeight);

        for (auto const& key : row) {
            auto const* keyPtr = &key;
            auto button = BSML::Lite::CreateUIButton(horizontal->transform, std::string(DisplayName(key)),
                                                     [this, keyPtr]() { OnKey(*keyPtr); });
            SetSize(button, key.width * kKeyUnit, kKeyHeight);
            auto text = button->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
            if (text) {
                // The button font is upper case and parses tags by default; keys need neither.
                text->fontStyle = TMPro::FontStyles::Normal;
                text->richText = false;
                text->enableWordWrapping = false;
                text->fontSize = kFontSize;
            }
            buttons.push_back({keyPtr, button, text});
        }
    }
    RefreshLabels();
}

void KeyboardPanel::SetVisible(bool visible) { gameObject->SetActive(visible); }

bool KeyboardPanel::IsVisible() { return gameObject->activeSelf; }

void KeyboardPanel::OnKey(Keys::KeyDef const& key) {
    auto out = state.Press(key);
    switch (out.type) {
        case Keys::OutputType::Text:
            Jni::SendText(out.text);
            break;
        case Keys::OutputType::Key:
            Jni::SendKey(out.keyCode, out.meta);
            break;
        case Keys::OutputType::Hide:
            Jni::CancelUrlEditing();
            SetVisible(false);
            break;
        case Keys::OutputType::None:
            break;
    }
    RefreshLabels();
}

void KeyboardPanel::RefreshLabels() {
    auto const& mods = state.Modifiers();
    for (auto& kb : buttons) {
        if (!kb.text) continue;
        auto const& key = *kb.key;
        if (key.kind == Keys::Kind::Char) kb.text->text = state.Label(key);
        bool active = (key.kind == Keys::Kind::Shift && mods.shift) || (key.kind == Keys::Kind::Caps && mods.caps) ||
                      (key.kind == Keys::Kind::Ctrl && mods.ctrl);
        kb.text->color = active ? kActiveText : kNormalText;
    }
}

}  // namespace QuestBrowser
