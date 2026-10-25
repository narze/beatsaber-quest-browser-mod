#include "ShortcutListener.hpp"

#include "BrowserWindow.hpp"
#include "Config.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Time.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, ShortcutListener);

using namespace GlobalNamespace;

namespace QuestBrowser {

namespace {
SafePtrUnity<ShortcutListener> instance;
}  // namespace

void ShortcutListener::EnsureCreated() {
    if (instance) return;
    auto go = UnityEngine::GameObject::New_ctor("QuestBrowserShortcut");
    UnityEngine::Object::DontDestroyOnLoad(go);
    instance = go->AddComponent<ShortcutListener*>();
    logger.info("Controller shortcut ready: hold the left thumbstick click to bring the browser here");
}

void ShortcutListener::Update() {
    bool pressed = OVRInput::Get(OVRInput::RawButton::LThumbstick, OVRInput::Controller::LTouch);
    bool allowed = Shortcut::ShortcutAllowed(Window::CurrentContext(), Config::GetShortcutEnabled());
    // Unscaled: the game can stop time while paused.
    if (!hold.Update(pressed, UnityEngine::Time::get_unscaledDeltaTime(), allowed)) return;
    logger.info("Controller shortcut: bring browser here");
    Window::BringHere();
}

}  // namespace QuestBrowser
