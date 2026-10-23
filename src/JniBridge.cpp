#include "JniBridge.hpp"

#include <jni.h>

#include "main.hpp"
#include "scotland2/shared/modloader.h"

namespace QuestBrowser::Jni {

namespace {

// The Java helper, compiled to dex by CMake (see cmake/dex.cmake).
// QB_DEX_PATH is the absolute path of the dex file, set by CMake.
// .incbin works on every NDK clang; #embed needs clang 19+.
__asm__(
    ".pushsection .rodata\n"
    ".balign 16\n"
    "qb_dex_begin:\n"
    ".incbin \"" QB_DEX_PATH "\"\n"
    "qb_dex_end:\n"
    ".popsection\n");
extern "C" unsigned char const qb_dex_begin[] __asm__("qb_dex_begin");
extern "C" unsigned char const qb_dex_end[] __asm__("qb_dex_end");

constexpr char const* kBridgeClass = "com.narze.questbrowser.BrowserBridge";

struct Methods {
    jclass cls = nullptr;
    jmethodID init, setActive, setFrameRate, loadUrl, goBack, goForward, reload;
    jmethodID touch, scroll;
    jmethodID sendText, sendKey, beginUrlEditing, cancelUrlEditing, takeKeyboardRequest, getUrlDraft, takeSubmittedUrl;
    jmethodID getUrl, getTitle, getProgress;
    jmethodID acquireFrame, releaseFrame;
};

Methods m;
bool ready = false;

JNIEnv* GetEnv() {
    JNIEnv* env = nullptr;
    if (!modloader_jvm) return nullptr;
    if (modloader_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        // Threads attached here stay attached; that is fine for the few game threads we use.
        if (modloader_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    }
    return env;
}

// Logs and clears a pending Java exception. Returns true if there was one.
bool CheckException(JNIEnv* env, std::string_view where) {
    if (!env->ExceptionCheck()) return false;
    jthrowable ex = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string message = "unknown";
    if (ex) {
        jclass throwable = env->FindClass("java/lang/Throwable");
        jmethodID toString = env->GetMethodID(throwable, "toString", "()Ljava/lang/String;");
        auto str = static_cast<jstring>(env->CallObjectMethod(ex, toString));
        if (!env->ExceptionCheck() && str) {
            char const* chars = env->GetStringUTFChars(str, nullptr);
            message = chars;
            env->ReleaseStringUTFChars(str, chars);
        }
        env->ExceptionClear();
        env->DeleteLocalRef(throwable);
        env->DeleteLocalRef(ex);
    }
    logger.error("Java exception in {}: {}", where, message);
    return true;
}

std::string ToStdString(JNIEnv* env, jstring str) {
    if (!str) return {};
    char const* chars = env->GetStringUTFChars(str, nullptr);
    std::string out = chars ? chars : "";
    env->ReleaseStringUTFChars(str, chars);
    env->DeleteLocalRef(str);
    return out;
}

// Unity keeps the current Activity in a static field. This only resolves on a
// thread whose Java stack has app classes, such as the Unity main thread.
jobject FindActivity(JNIEnv* env) {
    jclass player = env->FindClass("com/unity3d/player/UnityPlayer");
    if (CheckException(env, "FindClass UnityPlayer") || !player) return nullptr;
    jfieldID field = env->GetStaticFieldID(player, "currentActivity", "Landroid/app/Activity;");
    if (CheckException(env, "UnityPlayer.currentActivity") || !field) {
        env->DeleteLocalRef(player);
        return nullptr;
    }
    jobject activity = env->GetStaticObjectField(player, field);
    env->DeleteLocalRef(player);
    return activity;
}

jclass LoadBridgeClass(JNIEnv* env, jobject activity) {
    // Parent class loader: the app loader when we have an Activity, else the system loader.
    jobject parent = nullptr;
    if (activity) {
        jclass objCls = env->GetObjectClass(activity);
        jclass classCls = env->FindClass("java/lang/Class");
        jmethodID getClassLoader = env->GetMethodID(classCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
        parent = env->CallObjectMethod(objCls, getClassLoader);
        env->DeleteLocalRef(classCls);
        env->DeleteLocalRef(objCls);
        if (CheckException(env, "getClassLoader")) parent = nullptr;
    }
    if (!parent) {
        jclass loaderCls = env->FindClass("java/lang/ClassLoader");
        jmethodID getSystem = env->GetStaticMethodID(loaderCls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        parent = env->CallStaticObjectMethod(loaderCls, getSystem);
        env->DeleteLocalRef(loaderCls);
        if (CheckException(env, "getSystemClassLoader")) return nullptr;
    }

    // The dex bytes live in our .so for the life of the process, so a direct buffer is safe.
    jobject buffer = env->NewDirectByteBuffer(const_cast<unsigned char*>(qb_dex_begin), qb_dex_end - qb_dex_begin);
    jclass dexLoaderCls = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    if (CheckException(env, "FindClass InMemoryDexClassLoader")) return nullptr;
    jmethodID ctor = env->GetMethodID(dexLoaderCls, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    jobject loader = env->NewObject(dexLoaderCls, ctor, buffer, parent);
    env->DeleteLocalRef(buffer);
    env->DeleteLocalRef(parent);
    if (CheckException(env, "new InMemoryDexClassLoader")) return nullptr;

    jmethodID loadClass = env->GetMethodID(dexLoaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = env->NewStringUTF(kBridgeClass);
    auto cls = static_cast<jclass>(env->CallObjectMethod(loader, loadClass, name));
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    env->DeleteLocalRef(dexLoaderCls);
    if (CheckException(env, "loadClass BrowserBridge") || !cls) return nullptr;

    auto global = static_cast<jclass>(env->NewGlobalRef(cls));
    env->DeleteLocalRef(cls);
    return global;
}

bool LookupMethods(JNIEnv* env) {
    auto get = [&](jmethodID& out, char const* name, char const* sig) {
        out = env->GetStaticMethodID(m.cls, name, sig);
        return !CheckException(env, name) && out;
    };
    return get(m.init, "init", "(Ljava/lang/Object;IILjava/lang/String;)V") &&
           get(m.setActive, "setActive", "(Z)V") &&
           get(m.setFrameRate, "setFrameRate", "(I)V") &&
           get(m.loadUrl, "loadUrl", "(Ljava/lang/String;)V") &&
           get(m.goBack, "goBack", "()V") &&
           get(m.goForward, "goForward", "()V") &&
           get(m.reload, "reload", "()V") &&
           get(m.touch, "touch", "(IFF)V") &&
           get(m.scroll, "scroll", "(FFF)V") &&
           get(m.sendText, "sendText", "(Ljava/lang/String;)V") &&
           get(m.sendKey, "sendKey", "(II)V") &&
           get(m.beginUrlEditing, "beginUrlEditing", "(Ljava/lang/String;)V") &&
           get(m.cancelUrlEditing, "cancelUrlEditing", "()V") &&
           get(m.takeKeyboardRequest, "takeKeyboardRequest", "()Z") &&
           get(m.getUrlDraft, "getUrlDraft", "()Ljava/lang/String;") &&
           get(m.takeSubmittedUrl, "takeSubmittedUrl", "()Ljava/lang/String;") &&
           get(m.getUrl, "getUrl", "()Ljava/lang/String;") &&
           get(m.getTitle, "getTitle", "()Ljava/lang/String;") &&
           get(m.getProgress, "getProgress", "()I") &&
           get(m.acquireFrame, "acquireFrame", "()Ljava/nio/ByteBuffer;") &&
           get(m.releaseFrame, "releaseFrame", "()V");
}

template <typename... Args>
void CallVoid(char const* where, jmethodID method, Args... args) {
    if (!ready) return;
    JNIEnv* env = GetEnv();
    if (!env) return;
    env->CallStaticVoidMethod(m.cls, method, args...);
    CheckException(env, where);
}

void CallWithString(char const* where, jmethodID method, std::string_view text) {
    if (!ready) return;
    JNIEnv* env = GetEnv();
    if (!env) return;
    jstring str = env->NewStringUTF(std::string(text).c_str());
    env->CallStaticVoidMethod(m.cls, method, str);
    env->DeleteLocalRef(str);
    CheckException(env, where);
}

std::optional<std::string> CallOptionalString(char const* where, jmethodID method) {
    if (!ready) return std::nullopt;
    JNIEnv* env = GetEnv();
    if (!env) return std::nullopt;
    auto str = static_cast<jstring>(env->CallStaticObjectMethod(m.cls, method));
    if (CheckException(env, where) || !str) return std::nullopt;
    return ToStdString(env, str);
}

std::string CallString(char const* where, jmethodID method) { return CallOptionalString(where, method).value_or(""); }

}  // namespace

bool Init(int width, int height, std::string_view startUrl) {
    if (ready) return true;
    JNIEnv* env = GetEnv();
    if (!env) {
        logger.error("No JNIEnv, modloader_jvm is {}", fmt::ptr(modloader_jvm));
        return false;
    }

    jobject activity = FindActivity(env);
    logger.info("Unity activity: {}", activity ? "found" : "not found, using Application context");

    m.cls = LoadBridgeClass(env, activity);
    if (!m.cls || !LookupMethods(env)) {
        logger.error("Could not load the Java browser bridge");
        if (activity) env->DeleteLocalRef(activity);
        return false;
    }

    jstring url = env->NewStringUTF(std::string(startUrl).c_str());
    env->CallStaticVoidMethod(m.cls, m.init, activity, width, height, url);
    env->DeleteLocalRef(url);
    if (activity) env->DeleteLocalRef(activity);
    if (CheckException(env, "BrowserBridge.init")) return false;

    ready = true;
    logger.info("Browser bridge ready ({}x{}, dex {} bytes)", width, height, static_cast<long>(qb_dex_end - qb_dex_begin));
    return true;
}

bool IsReady() { return ready; }

void SetActive(bool active) { CallVoid("setActive", m.setActive, static_cast<jboolean>(active)); }
void SetFrameRate(int fps) { CallVoid("setFrameRate", m.setFrameRate, static_cast<jint>(fps)); }
void LoadUrl(std::string_view url) { CallWithString("loadUrl", m.loadUrl, url); }
void GoBack() { CallVoid("goBack", m.goBack); }
void GoForward() { CallVoid("goForward", m.goForward); }
void Reload() { CallVoid("reload", m.reload); }

void Touch(int action, float x, float y) {
    CallVoid("touch", m.touch, static_cast<jint>(action), static_cast<jfloat>(x), static_cast<jfloat>(y));
}

void Scroll(float x, float y, float amount) {
    CallVoid("scroll", m.scroll, static_cast<jfloat>(x), static_cast<jfloat>(y), static_cast<jfloat>(amount));
}


void SendText(std::string_view text) { CallWithString("sendText", m.sendText, text); }
void SendKey(int androidKeyCode, int metaState) {
    CallVoid("sendKey", m.sendKey, static_cast<jint>(androidKeyCode), static_cast<jint>(metaState));
}
void BeginUrlEditing(std::string_view currentUrl) { CallWithString("beginUrlEditing", m.beginUrlEditing, currentUrl); }
void CancelUrlEditing() { CallVoid("cancelUrlEditing", m.cancelUrlEditing); }

bool TakeKeyboardRequest() {
    if (!ready) return false;
    JNIEnv* env = GetEnv();
    if (!env) return false;
    jboolean requested = env->CallStaticBooleanMethod(m.cls, m.takeKeyboardRequest);
    return !CheckException(env, "takeKeyboardRequest") && requested;
}
std::optional<std::string> GetUrlDraft() { return CallOptionalString("getUrlDraft", m.getUrlDraft); }
std::optional<std::string> TakeSubmittedUrl() { return CallOptionalString("takeSubmittedUrl", m.takeSubmittedUrl); }

std::string GetUrl() { return CallString("getUrl", m.getUrl); }
std::string GetTitle() { return CallString("getTitle", m.getTitle); }

int GetProgress() {
    if (!ready) return 0;
    JNIEnv* env = GetEnv();
    if (!env) return 0;
    jint p = env->CallStaticIntMethod(m.cls, m.getProgress);
    return CheckException(env, "getProgress") ? 0 : p;
}

bool ConsumeFrame(std::function<void(void*, std::size_t)> const& fn) {
    if (!ready) return false;
    JNIEnv* env = GetEnv();
    if (!env) return false;
    jobject buffer = env->CallStaticObjectMethod(m.cls, m.acquireFrame);
    if (CheckException(env, "acquireFrame") || !buffer) return false;

    void* pixels = env->GetDirectBufferAddress(buffer);
    jlong size = env->GetDirectBufferCapacity(buffer);
    if (pixels && size > 0) fn(pixels, static_cast<std::size_t>(size));
    env->DeleteLocalRef(buffer);

    env->CallStaticVoidMethod(m.cls, m.releaseFrame);
    CheckException(env, "releaseFrame");
    return pixels != nullptr;
}

}  // namespace QuestBrowser::Jni
