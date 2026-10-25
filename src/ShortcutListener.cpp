#include "ShortcutListener.hpp"

#include "BrowserWindow.hpp"
#include "Config.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Time.hpp"
#include "keybind_logic.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, ShortcutListener);

using namespace GlobalNamespace;

namespace QuestBrowser {

namespace {

SafePtrUnity<ShortcutListener> instance;

// Pressed controller buttons as Keybind::Button bits.
uint32_t PressedButtons() {
    auto touch = OVRInput::Controller::Touch;
    uint32_t pressed = 0;
    if (OVRInput::Get(OVRInput::RawButton::X, touch)) pressed |= Keybind::kX;
    if (OVRInput::Get(OVRInput::RawButton::Y, touch)) pressed |= Keybind::kY;
    if (OVRInput::Get(OVRInput::RawButton::A, touch)) pressed |= Keybind::kA;
    if (OVRInput::Get(OVRInput::RawButton::B, touch)) pressed |= Keybind::kB;
    if (OVRInput::Get(OVRInput::RawButton::LThumbstick, touch)) pressed |= Keybind::kLeftStick;
    if (OVRInput::Get(OVRInput::RawButton::RThumbstick, touch)) pressed |= Keybind::kRightStick;
    return pressed;
}

}  // namespace

void ShortcutListener::EnsureCreated() {
    if (instance) return;
    auto go = UnityEngine::GameObject::New_ctor("QuestBrowserShortcut");
    UnityEngine::Object::DontDestroyOnLoad(go);
    instance = go->AddComponent<ShortcutListener*>();
    logger.info("Reset keybind ready: {}", Config::GetResetKeybind());
}

void ShortcutListener::Update() {
    auto preset = Keybind::Find(Config::GetResetKeybind());
    bool held = Keybind::IsHeld(preset, PressedButtons());
    bool allowed = Shortcut::ShortcutAllowed(Window::CurrentContext(), preset.buttons != 0);
    // Unscaled: the game can stop time while paused.
    if (!hold.Update(held, UnityEngine::Time::get_unscaledDeltaTime(), allowed)) return;
    logger.info("Reset keybind ({}): reset browser position", preset.name);
    Window::ResetCurrentPosition();
}

}  // namespace QuestBrowser
