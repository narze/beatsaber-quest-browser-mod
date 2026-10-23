#include "Config.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "main.hpp"

namespace QuestBrowser::Config {

namespace {

// Songs: to the left of the track, turned to face the player (yaw -50 degrees).
constexpr Pose kDefaultSongPose{{-2.6f, 1.6f, 2.4f}, {0.0f, -0.4226f, 0.0f, 0.9063f}};
// Menu: above the main menu screens, tilted down to the player (pitch -25 degrees).
constexpr Pose kDefaultMenuPose{{0.0f, 3.4f, 3.6f}, {-0.2164f, 0.0f, 0.0f, 0.9763f}};

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

Pose GetPose(Logic::PoseSlot slot) {
    auto const& d = DefaultPose(slot);
    auto f = [&](char const* name, float fallback) { return GetFloat(Key(slot, name), fallback); };
    return {
        {f("posX", d.position.x), f("posY", d.position.y), f("posZ", d.position.z)},
        {f("rotX", d.rotation.x), f("rotY", d.rotation.y), f("rotZ", d.rotation.z), f("rotW", d.rotation.w)},
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
    Get().Write();
}

void ResetPoses() {
    SetPose(Logic::PoseSlot::Menu, kDefaultMenuPose);
    SetPose(Logic::PoseSlot::Song, kDefaultSongPose);
}

}  // namespace QuestBrowser::Config
