#include "Config.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "main.hpp"

namespace QuestBrowser::Config {

namespace {

// Songs: to the left of the track, turned to face the player (yaw -50 degrees).
constexpr Pose kDefaultSongPose{{-2.6f, 1.6f, 2.4f}, {0.0f, -0.4226f, 0.0f, 0.9063f}, kDefaultScale};
// Menu: above the main menu screens, tilted down to the player (pitch -25 degrees).
constexpr Pose kDefaultMenuPose{{0.0f, 3.4f, 3.6f}, {-0.2164f, 0.0f, 0.0f, 0.9763f}, kDefaultScale};

// Song keys have no prefix, for compatibility with configs from 0.1.0.
char const* Prefix(Logic::PoseSlot slot) { return slot == Logic::PoseSlot::Menu ? "menu" : ""; }
Pose const& DefaultPose(Logic::PoseSlot slot) { return slot == Logic::PoseSlot::Menu ? kDefaultMenuPose : kDefaultSongPose; }

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
}

bool GetVisible() { return GetBool("visible", true); }
void SetVisible(bool value) { SetBool("visible", value); }

bool GetShowInSong() { return GetBool("showInSong", true); }
void SetShowInSong(bool value) { SetBool("showInSong", value); }

bool GetShortcutEnabled() { return GetBool("shortcut", true); }
void SetShortcutEnabled(bool value) { SetBool("shortcut", value); }

Pose GetPose(Logic::PoseSlot slot) {
    auto const& d = DefaultPose(slot);
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

void ResetPoses() {
    SetPose(Logic::PoseSlot::Menu, kDefaultMenuPose);
    SetPose(Logic::PoseSlot::Song, kDefaultSongPose);
    SetDimensions(Logic::PoseSlot::Menu, {});
    SetDimensions(Logic::PoseSlot::Song, {});
    SetAspectPreset(Logic::PoseSlot::Menu, 0);
    SetAspectPreset(Logic::PoseSlot::Song, 0);
}

Resize::Dimensions GetDimensions(Logic::PoseSlot slot) {
    return Resize::DimensionsFromDrag({GetFloat(Key(slot, "width"), 150), GetFloat(Key(slot, "height"), 126)}, 0, 0, 0, 0, 0);
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

float GetOpacity() { return std::clamp(GetFloat("opacity", 1.0f), 0.2f, 1.0f); }
void SetOpacity(float value) {
    SetFloat("opacity", std::clamp(value, 0.2f, 1.0f));
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

}  // namespace QuestBrowser::Config
