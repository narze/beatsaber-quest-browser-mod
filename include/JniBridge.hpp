#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Native side of java/src/com/narze/questbrowser/BrowserBridge.java.
// Call Init() from the Unity main thread. The other functions are safe to call
// from any thread after Init() returns true.
namespace QuestBrowser::Jni {

// Starts the browser with one tab per URL and opens tab activeTab.
bool Init(int width, int height, std::vector<std::string> const& tabUrls, int activeTab);
bool IsReady();

void SetActive(bool active);
void SetFrameRate(int fps);
void SetViewport(int width, int height);
void LoadUrl(std::string_view url);
void GoBack();
void GoForward();
void Reload();

// action: 0 down, 1 up, 2 move, 3 cancel (Android MotionEvent values).
void Touch(int action, float x, float y);
void Scroll(float x, float y, float amount);
// Tabs. The Java side follows these calls; the rules are in tabs_logic.hpp.
void NewTab(std::string_view url);
void SelectTab(int index);
// newActive: the active tab afterwards, in the list without the closed tab.
void CloseTab(int index, int newActive);
// Changes whenever a tab opens, closes, or changes its URL or title.
int GetTabsVersion();
std::vector<std::string> GetTabUrls();
std::vector<std::string> GetTabTitles();
int GetActiveTab();

// In-game keyboard. Text and keys go to the address field while it is in use, else to the page.
void SendText(std::string_view text);
void SendKey(int androidKeyCode, int metaState);
void BeginUrlEditing(std::string_view currentUrl);
void CancelUrlEditing();
// True once after the user tapped a text field in the page.
bool TakeKeyboardRequest();
// The address being typed, or nullopt when the address field is not in use.
std::optional<std::string> GetUrlDraft();
// The address submitted with Enter, once.
std::optional<std::string> TakeSubmittedUrl();

// Maps from BeatSaver One-Click links and downloads install into dir (SongCore custom levels).
void SetSongsDir(std::string_view dir);
// Playlists from BeatSaver and BeastSaber playlist links are saved into dir (PlaylistManager).
void SetPlaylistsDir(std::string_view dir);
// Map download status for the toolbar, or "" when there is nothing to show.
std::string GetSongStatus();
// Maps and playlists installed since the last call.
int TakeSongsInstalled();

std::string GetUrl();
std::string GetTitle();
int GetProgress();

// If there is a new frame, calls fn with RGBA pixels (top row first) and returns true.
bool ConsumeFrame(std::function<void(void* pixels, std::size_t size)> const& fn);

}  // namespace QuestBrowser::Jni
