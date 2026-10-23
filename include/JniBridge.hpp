#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// Native side of java/src/com/narze/questbrowser/BrowserBridge.java.
// Call Init() from the Unity main thread. The other functions are safe to call
// from any thread after Init() returns true.
namespace QuestBrowser::Jni {

bool Init(int width, int height, std::string_view startUrl);
bool IsReady();

void SetActive(bool active);
void SetFrameRate(int fps);
void LoadUrl(std::string_view url);
void GoBack();
void GoForward();
void Reload();

// action: 0 down, 1 up, 2 move, 3 cancel (Android MotionEvent values).
void Touch(int action, float x, float y);
void Scroll(float x, float y, float amount);
void TypeText(std::string_view text);
void PressKey(int androidKeyCode);

std::string GetUrl();
std::string GetTitle();
int GetProgress();

// If there is a new frame, calls fn with RGBA pixels (top row first) and returns true.
bool ConsumeFrame(std::function<void(void* pixels, std::size_t size)> const& fn);

}  // namespace QuestBrowser::Jni
