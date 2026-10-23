#include "hidden_api.hpp"

#include <thread>

namespace QuestBrowser::HiddenApi {

namespace {

bool ExemptOnThisThread(JNIEnv* env, std::vector<std::string> const& prefixes) {
    jclass runtimeCls = env->FindClass("dalvik/system/VMRuntime");
    if (!runtimeCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    jmethodID getRuntime = env->GetStaticMethodID(runtimeCls, "getRuntime", "()Ldalvik/system/VMRuntime;");
    jmethodID setExemptions = env->GetMethodID(runtimeCls, "setHiddenApiExemptions", "([Ljava/lang/String;)V");
    if (!getRuntime || !setExemptions || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    jobject runtime = env->CallStaticObjectMethod(runtimeCls, getRuntime);
    jclass stringCls = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(prefixes.size()), stringCls, nullptr);
    for (jsize i = 0; i < static_cast<jsize>(prefixes.size()); i++) {
        jstring s = env->NewStringUTF(prefixes[i].c_str());
        env->SetObjectArrayElement(array, i, s);
        env->DeleteLocalRef(s);
    }
    env->CallVoidMethod(runtime, setExemptions, array);
    bool ok = !env->ExceptionCheck();
    env->ExceptionClear();
    env->DeleteLocalRef(array);
    env->DeleteLocalRef(stringCls);
    env->DeleteLocalRef(runtime);
    env->DeleteLocalRef(runtimeCls);
    return ok;
}

}  // namespace

bool Exempt(JavaVM* vm, std::vector<std::string> const& signaturePrefixes) {
    if (!vm) return false;
    bool ok = false;
    // ART trusts JNI calls from a thread that has no Java frames on its stack
    // (art/runtime/jni/jni_internal.cc, GetJniAccessContext), so make the call
    // from a new native thread.
    std::thread worker([&] {
        JNIEnv* env = nullptr;
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
        ok = ExemptOnThisThread(env, signaturePrefixes);
        vm->DetachCurrentThread();
    });
    worker.join();
    return ok;
}

}  // namespace QuestBrowser::HiddenApi
