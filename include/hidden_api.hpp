#pragma once

// Exempts a few Android internal (hidden) APIs from the app's hidden API policy.
// Depends only on jni.h, so the Android test harness builds it too.

#include <jni.h>

#include <string>
#include <vector>

namespace QuestBrowser::HiddenApi {

// signaturePrefixes: JNI type signatures, for example "Llibcore/net/NetworkSecurityPolicy;".
// Returns true when the exemption is in place.
bool Exempt(JavaVM* vm, std::vector<std::string> const& signaturePrefixes);

}  // namespace QuestBrowser::HiddenApi
