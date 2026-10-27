#include "KeyboardPanel.hpp"

#include <cmath>

#include "JniBridge.hpp"
#include "TMPro/FontStyles.hpp"
#include "UnityEngine/Color.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "BrowserWindow.hpp"
#include "Config.hpp"
#include "UnityEngine/Camera.hpp"
#include "main.hpp"
#include "placement_logic.hpp"

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
// Near the player, at 0.45 cm per UI unit (about 68 cm wide).
constexpr float kNearScale = 0.0045f;

Placement::Point ToPoint(Vector3 v) { return {v.x, v.y, v.z}; }

// The player moved the keyboard with its handle: keep that spot for the next time,
// relative to the direction the player faces now.
void OnHandleReleased(BSML::FloatingScreen* screen, BSML::FloatingScreenHandleEventArgs const& args) {
    auto camera = Camera::get_main();
    if (!camera || !screen) return;
    auto head = camera->transform;
    auto offset = Placement::ToHeadFrame(ToPoint(head->position), ToPoint(head->forward), ToPoint(args.position));
    float yaw = Placement::KeyboardNearHead(ToPoint(head->position), ToPoint(head->forward)).yawDegrees;
    auto relative = Quaternion::op_Multiply(Quaternion::Inverse(Quaternion::Euler(0.0f, yaw, 0.0f)), args.rotation);
    Config::SetKeyboardPlacement({{offset.x, offset.y, offset.z}, relative});
    logger.info("Saved keyboard position: right {:.2f} m, up {:.2f} m, away {:.2f} m", offset.x, offset.y, offset.z);
}

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

Vector2 PanelSize() {
    auto& rows = Keys::Layout();
    float width = 0.0f;
    for (auto const& row : rows) {
        float w = 0.0f;
        for (auto const& key : row) w += key.width * kKeyUnit + kSpacing;
        width = std::max(width, w);
    }
    return {width + 2 * kPadding, rows.size() * (kKeyHeight + kSpacing) + 2 * kPadding};
}

KeyboardPanel* KeyboardPanel::Create(Transform* window, float windowHeight) {
    auto size = PanelSize();
    // With a handle on top, so the player can move it.
    auto screen = BSML::FloatingScreen::CreateFloatingScreen(size, true, window->position, window->rotation, 0.0f, false);
    screen->name = "QuestBrowserKeyboard";
    screen->set_HandleSide(BSML::Side::Top);
    screen->HandleReleased.addCallback(&OnHandleReleased);
    Object::DontDestroyOnLoad(screen->gameObject);

    auto panel = screen->gameObject->AddComponent<KeyboardPanel*>();
    panel->anchor = window;
    panel->windowHeight = windowHeight;
    panel->panelSize = size;
    panel->Build(size);
    panel->FollowAnchor();
    screen->gameObject->SetActive(false);
    return panel;
}

void KeyboardPanel::LateUpdate() {
    Window::BoostHandleDepth(this);
    // Near the player it stays where it opened; it only follows the window as a fallback.
    if (!placedNearUser) FollowAnchor();
}

// In front of the player, below the eyes, where it is large enough to type on.
bool KeyboardPanel::PlaceNearUser() {
    auto camera = Camera::get_main();
    if (!camera) return false;
    auto head = camera->transform;
    auto p = head->position;
    auto f = head->forward;
    auto saved = Config::GetKeyboardPlacement();
    auto pose = Placement::KeyboardNearHead({p.x, p.y, p.z}, {f.x, f.y, f.z}, ToPoint(saved.offset));
    transform->position = Vector3(pose.position.x, pose.position.y, pose.position.z);
    transform->rotation = Quaternion::op_Multiply(Quaternion::Euler(0.0f, pose.yawDegrees, 0.0f), saved.rotation);
    transform->localScale = Vector3(kNearScale, kNearScale, kNearScale);
    return true;
}

// Below the window, tilted toward the player, in the window's units and scale.
void KeyboardPanel::FollowAnchor() {
    if (!anchor) return;
    float tilt = kTiltDegrees * 3.14159265f / 180.0f;
    float top = -(windowHeight / 2.0f + kGap + extraOffset);
    float half = panelSize.y / 2.0f;
    // Rotating by +tilt around X moves the lower edge toward the player (-Z).
    Vector3 centerInWindow{0.0f, top - half * std::cos(tilt), -half * std::sin(tilt)};
    transform->position = anchor->TransformPoint(centerInWindow);
    transform->rotation = Quaternion::op_Multiply(anchor->rotation, Quaternion::Euler(kTiltDegrees, 0.0f, 0.0f));
    transform->localScale = anchor->lossyScale;
}

void KeyboardPanel::Build(Vector2 size) {
    auto& rows = Keys::Layout();
    float width = size.x;

    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(transform);
    vertical->spacing = kSpacing;
    vertical->padding = RectOffset::New_ctor(kPadding, kPadding, kPadding, kPadding);
    vertical->childForceExpandHeight = false;
    vertical->childForceExpandWidth = false;
    vertical->childControlHeight = true;
    vertical->childControlWidth = true;
    vertical->childAlignment = TextAnchor::UpperCenter;
    if (auto bg = vertical->GetComponent<BSML::Backgroundable*>()) {
        bg->ApplyBackground("round-rect-panel");
        // Solid to the laser between the keys: else a click in a gap hits nothing, which counts
        // as a click outside, and the keyboard closes.
        if (bg->background) bg->background->raycastTarget = true;
    }

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

void KeyboardPanel::SetVisible(bool visible) {
    bool wasVisible = gameObject->activeSelf;
    gameObject->SetActive(visible);
    if (!visible) return;
    // Place it when it opens, not on every key: it must not move while typing.
    if (!wasVisible) placedNearUser = PlaceNearUser();
    auto rect = gameObject->GetComponent<RectTransform*>();
    auto pos = transform->position;
    auto scale = transform->lossyScale;
    auto size = rect ? rect->rect.size : Vector2{};
    logger.info("Keyboard shown: activeInHierarchy={} layer={} pos=({:.2f},{:.2f},{:.2f}) scale={:.4f} size=({:.1f},{:.1f}) keys={}",
                gameObject->activeInHierarchy, gameObject->layer, pos.x, pos.y, pos.z, scale.x, size.x, size.y, buttons.size());
}

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
