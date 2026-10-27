#include "Config.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "main.hpp"
#include "keybind_logic.hpp"
#include "placement_logic.hpp"

namespace QuestBrowser::Config {

namespace {

// Menu and songs: centered and raised above the song selection (and the note
// track), tilted toward the player. See placement_logic.hpp.
Pose ResetPoseValue() {
    auto p = Placement::kResetPosition;
    return {{p.x, p.y, p.z}, UnityEngine::Quaternion::Euler(Placement::kResetPitchDegrees, 0.0f, 0.0f), kDefaultScale};
}

// Song keys have no prefix, for compatibility with configs from 0.1.0.
char const* Prefix(Logic::PoseSlot slot) { return slot == Logic::PoseSlot::Menu ? "menu" : ""; }
Pose DefaultPose(Logic::PoseSlot) { return ResetPoseValue(); }

std::string Key(Logic::PoseSlot slot, char const* name) {
    std::string key = std::string(Prefix(slot)) + name;
    if (slot == Logic::PoseSlot::Menu) key[4] = static_cast<char>(key[4] - 'a' + 'A');  // menuPosX
    return key;
}

Configuration& Get() {
    static Configuration config({MOD_ID, VERSION, 0});
    return config;
}

rapidjson::Value& Doc() { return Get().config; }
auto& Alloc() { return Get().config.GetAllocator(); }

float GetFloat(std::string const& key, float fallback) {
    auto& doc = Doc();
    auto it = doc.FindMember(key.c_str());
    if (it != doc.MemberEnd() && it->value.IsNumber()) return it->value.GetFloat();
    return fallback;
}

void SetFloat(std::string const& key, float value) {
    auto& doc = Doc();
    auto it = doc.FindMember(key.c_str());
    if (it != doc.MemberEnd()) it->value.SetFloat(value);
    else doc.AddMember(rapidjson::Value(key.c_str(), Alloc()), rapidjson::Value(value), Alloc());
}

bool GetBool(char const* key, bool fallback) {
    auto& doc = Doc();
    auto it = doc.FindMember(key);
    if (it != doc.MemberEnd() && it->value.IsBool()) return it->value.GetBool();
    return fallback;
}

void SetBool(char const* key, bool value) {
    auto& doc = Doc();
    auto it = doc.FindMember(key);
    if (it != doc.MemberEnd()) it->value.SetBool(value);
    else doc.AddMember(rapidjson::Value(key, Alloc()), rapidjson::Value(value), Alloc());
    Get().Write();
}

}  // namespace

void Load() {
    Get().Load();
    if (!Doc().IsObject()) Doc().SetObject();
    // "Show browser" merged into "Enable browser": keep the browser off if either was off.
    if (auto it = Doc().FindMember("visible"); it != Doc().MemberEnd()) {
        std::optional<bool> enabled, visible;
        if (it->value.IsBool()) visible = it->value.GetBool();
        if (auto e = Doc().FindMember("enabled"); e != Doc().MemberEnd() && e->value.IsBool()) enabled = e->value.GetBool();
        Doc().RemoveMember("visible");
        SetBool("enabled", Logic::EnabledAfterMerge(enabled, visible));
    }
    // Layout 2: the toolbar left the window, so saved heights lose the old toolbar rows.
    if (GetFloat("layoutVersion", 1) < 2) {
        for (auto slot : {Logic::PoseSlot::Menu, Logic::PoseSlot::Song}) {
            auto key = Key(slot, "height");
            if (Doc().HasMember(key.c_str())) SetFloat(key, Resize::MigrateHeight(GetFloat(key, 0)));
        }
        SetFloat("layoutVersion", 2);
        Get().Write();
    }
}

bool GetEnabled() { return GetBool("enabled", true); }
void SetEnabled(bool value) { SetBool("enabled", value); }

bool GetShowInSong() { return GetBool("showInSong", true); }
void SetShowInSong(bool value) { SetBool("showInSong", value); }

std::string GetResetKeybind() {
    auto it = Doc().FindMember("resetKeybind");
    std::string name = it != Doc().MemberEnd() && it->value.IsString() ? it->value.GetString() : "";
    return std::string(Keybind::Find(name).name);
}

void SetResetKeybind(std::string_view name) {
    auto& doc = Doc();
    rapidjson::Value value(std::string(Keybind::Find(name).name).c_str(), Alloc());
    auto it = doc.FindMember("resetKeybind");
    if (it != doc.MemberEnd()) it->value = value;
    else doc.AddMember(rapidjson::Value("resetKeybind", Alloc()), value, Alloc());
    Get().Write();
}

Pose GetPose(Logic::PoseSlot slot) {
    auto const d = DefaultPose(slot);
    auto f = [&](char const* name, float fallback) { return GetFloat(Key(slot, name), fallback); };
    return {
        {f("posX", d.position.x), f("posY", d.position.y), f("posZ", d.position.z)},
        {f("rotX", d.rotation.x), f("rotY", d.rotation.y), f("rotZ", d.rotation.z), f("rotW", d.rotation.w)},
        f("scale", d.scale),
    };
}

void SetPose(Logic::PoseSlot slot, Pose const& pose) {
    auto f = [&](char const* name, float value) { SetFloat(Key(slot, name), value); };
    f("posX", pose.position.x);
    f("posY", pose.position.y);
    f("posZ", pose.position.z);
    f("rotX", pose.rotation.x);
    f("rotY", pose.rotation.y);
    f("rotZ", pose.rotation.z);
    f("rotW", pose.rotation.w);
    f("scale", pose.scale);
    Get().Write();
}

void ResetPose(Logic::PoseSlot slot) {
    SetPose(slot, DefaultPose(slot));
    SetDimensions(slot, {});
    SetAspectPreset(slot, 0);
}

void ResetPoses() {
    ResetPose(Logic::PoseSlot::Menu);
    ResetPose(Logic::PoseSlot::Song);
}

Resize::Dimensions GetDimensions(Logic::PoseSlot slot) {
    Resize::Dimensions d;
    return Resize::DimensionsFromDrag({GetFloat(Key(slot, "width"), d.width), GetFloat(Key(slot, "height"), d.height)}, 0, 0, 0, 0, 0);
}
void SetDimensions(Logic::PoseSlot slot, Resize::Dimensions size) {
    SetFloat(Key(slot, "width"), size.width);
    SetFloat(Key(slot, "height"), size.height);
    Get().Write();
}
int GetAspectPreset(Logic::PoseSlot slot) { return static_cast<int>(std::clamp(GetFloat(Key(slot, "aspect"), 0), 0.0f, 3.0f)); }
void SetAspectPreset(Logic::PoseSlot slot, int preset) { SetFloat(Key(slot, "aspect"), preset); Get().Write(); }
float GetZoom() { return std::clamp(GetFloat("zoom", 1), 0.5f, 2.0f); }
void SetZoom(float zoom) { SetFloat("zoom", std::clamp(zoom, 0.5f, 2.0f)); Get().Write(); }

// Songs keep the old shared "opacity" key; the menu starts from it until it has its own.
float GetOpacity(Logic::PoseSlot slot) {
    float shared = GetFloat("opacity", 1.0f);
    return std::clamp(slot == Logic::PoseSlot::Menu ? GetFloat(Key(slot, "opacity"), shared) : shared, 0.2f, 1.0f);
}
void SetOpacity(Logic::PoseSlot slot, float value) {
    SetFloat(Key(slot, "opacity"), std::clamp(value, 0.2f, 1.0f));
    Get().Write();
}

namespace {

std::vector<std::string> GetStrings(char const* key, std::size_t limit) {
    std::vector<std::string> out;
    auto it = Doc().FindMember(key);
    if (it != Doc().MemberEnd() && it->value.IsArray()) {
        for (auto const& entry : it->value.GetArray()) {
            if (entry.IsString() && out.size() < limit) out.emplace_back(entry.GetString(), entry.GetStringLength());
        }
    }
    return out;
}

void SetStrings(char const* key, std::vector<std::string> const& values) {
    rapidjson::Value entries(rapidjson::kArrayType);
    for (auto const& v : values) entries.PushBack(rapidjson::Value(v.c_str(), Alloc()), Alloc());
    auto it = Doc().FindMember(key);
    if (it == Doc().MemberEnd()) Doc().AddMember(rapidjson::Value(key, Alloc()), entries, Alloc());
    else it->value = entries;
}

}  // namespace

namespace {
// Tilted back so that it faces the eyes, like a laptop keyboard.
constexpr float kKeyboardTiltDegrees = 50.0f;
}  // namespace

KeyboardPlacement GetKeyboardPlacement() {
    auto tilt = UnityEngine::Quaternion::Euler(kKeyboardTiltDegrees, 0.0f, 0.0f);
    return {{GetFloat("kbX", 0), GetFloat("kbY", 0), GetFloat("kbZ", 0)},
            {GetFloat("kbRotX", tilt.x), GetFloat("kbRotY", tilt.y), GetFloat("kbRotZ", tilt.z), GetFloat("kbRotW", tilt.w)}};
}

void SetKeyboardPlacement(KeyboardPlacement const& p) {
    SetFloat("kbX", p.offset.x);
    SetFloat("kbY", p.offset.y);
    SetFloat("kbZ", p.offset.z);
    SetFloat("kbRotX", p.rotation.x);
    SetFloat("kbRotY", p.rotation.y);
    SetFloat("kbRotZ", p.rotation.z);
    SetFloat("kbRotW", p.rotation.w);
    Get().Write();
}

void ResetKeyboardPlacement() {
    SetKeyboardPlacement({{0, 0, 0}, UnityEngine::Quaternion::Euler(kKeyboardTiltDegrees, 0.0f, 0.0f)});
}

std::vector<std::string> GetTabs() { return GetStrings("tabs", 64); }
int GetActiveTab() { return static_cast<int>(GetFloat("activeTab", 0)); }
void SetTabs(std::vector<std::string> const& urls, int active) {
    SetStrings("tabs", urls);
    SetFloat("activeTab", static_cast<float>(active));
    Get().Write();
}

std::vector<std::string> GetHistory() {
    std::vector<std::string> urls;
    auto it = Doc().FindMember("history");
    if (it != Doc().MemberEnd() && it->value.IsArray()) {
        for (auto const& entry : it->value.GetArray()) {
            if (entry.IsString() && urls.size() < 100) urls.emplace_back(entry.GetString(), entry.GetStringLength());
        }
    }
    return urls;
}

void SetHistory(std::vector<std::string> const& urls) {
    rapidjson::Value entries(rapidjson::kArrayType);
    for (auto const& url : urls) entries.PushBack(rapidjson::Value(url.c_str(), Alloc()), Alloc());
    auto it = Doc().FindMember("history");
    if (it == Doc().MemberEnd()) Doc().AddMember(rapidjson::Value("history", Alloc()), entries, Alloc());
    else it->value = entries;
    Get().Write();
}

// [{"url": ..., "title": ...}, ...]
std::vector<Bookmarks::Bookmark> GetBookmarks() {
    std::vector<Bookmarks::Bookmark> out;
    auto it = Doc().FindMember("bookmarks");
    if (it == Doc().MemberEnd() || !it->value.IsArray()) return out;
    for (auto const& entry : it->value.GetArray()) {
        if (!entry.IsObject() || out.size() >= Bookmarks::kMax) continue;
        auto url = entry.FindMember("url");
        if (url == entry.MemberEnd() || !url->value.IsString()) continue;
        auto title = entry.FindMember("title");
        out.push_back({std::string(url->value.GetString(), url->value.GetStringLength()),
                       title != entry.MemberEnd() && title->value.IsString()
                           ? std::string(title->value.GetString(), title->value.GetStringLength())
                           : std::string()});
    }
    return out;
}

void SetBookmarks(std::vector<Bookmarks::Bookmark> const& bookmarks) {
    rapidjson::Value entries(rapidjson::kArrayType);
    for (auto const& b : bookmarks) {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember("url", rapidjson::Value(b.url.c_str(), Alloc()), Alloc());
        entry.AddMember("title", rapidjson::Value(b.title.c_str(), Alloc()), Alloc());
        entries.PushBack(entry, Alloc());
    }
    auto it = Doc().FindMember("bookmarks");
    if (it == Doc().MemberEnd()) Doc().AddMember(rapidjson::Value("bookmarks", Alloc()), entries, Alloc());
    else it->value = entries;
    Get().Write();
}

}  // namespace QuestBrowser::Config
