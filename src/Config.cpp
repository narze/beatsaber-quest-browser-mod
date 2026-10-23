#include "Config.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "main.hpp"

namespace QuestBrowser::Config {

namespace {

// To the left of the track, turned to face the player.
constexpr InSongPose kDefaultPose{{-2.6f, 1.6f, 2.4f}, {0.0f, -0.4226f, 0.0f, 0.9063f}};  // yaw -50 degrees

Configuration& Get() {
    static Configuration config({MOD_ID, VERSION, 0});
    return config;
}

rapidjson::Value& Doc() { return Get().config; }
auto& Alloc() { return Get().config.GetAllocator(); }

float GetFloat(char const* key, float fallback) {
    auto& doc = Doc();
    if (doc.HasMember(key) && doc[key].IsNumber()) return doc[key].GetFloat();
    return fallback;
}

void SetFloat(char const* key, float value) {
    auto& doc = Doc();
    if (doc.HasMember(key)) doc[key].SetFloat(value);
    else doc.AddMember(rapidjson::Value(key, Alloc()), rapidjson::Value(value), Alloc());
}

}  // namespace

void Load() {
    Get().Load();
    if (!Doc().IsObject()) Doc().SetObject();
}

bool GetShowInSong() {
    auto& doc = Doc();
    if (doc.HasMember("showInSong") && doc["showInSong"].IsBool()) return doc["showInSong"].GetBool();
    return true;
}

void SetShowInSong(bool value) {
    auto& doc = Doc();
    if (doc.HasMember("showInSong")) doc["showInSong"].SetBool(value);
    else doc.AddMember("showInSong", value, Alloc());
    Get().Write();
}

InSongPose GetInSongPose() {
    auto const& d = kDefaultPose;
    return {
        {GetFloat("posX", d.position.x), GetFloat("posY", d.position.y), GetFloat("posZ", d.position.z)},
        {GetFloat("rotX", d.rotation.x), GetFloat("rotY", d.rotation.y), GetFloat("rotZ", d.rotation.z), GetFloat("rotW", d.rotation.w)},
    };
}

void SetInSongPose(InSongPose const& pose) {
    SetFloat("posX", pose.position.x);
    SetFloat("posY", pose.position.y);
    SetFloat("posZ", pose.position.z);
    SetFloat("rotX", pose.rotation.x);
    SetFloat("rotY", pose.rotation.y);
    SetFloat("rotZ", pose.rotation.z);
    SetFloat("rotW", pose.rotation.w);
    Get().Write();
}

void ResetInSongPose() { SetInSongPose(kDefaultPose); }

}  // namespace QuestBrowser::Config
