// Native part of the test harness: the same hidden API exemption as the mod
// (src/hidden_api.cpp), done when the harness loads this library.
#include <jni.h>

#include "hidden_api.hpp"

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    QuestBrowser::HiddenApi::Exempt(vm, {"Llibcore/net/NetworkSecurityPolicy;"});
    return JNI_VERSION_1_6;
}
