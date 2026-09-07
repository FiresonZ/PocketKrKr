/*
 * KrKr2 Engine - Android JNI glue for the engine_api shared library.
 *
 * Bridges the ANativeWindow (SurfaceTexture/SurfaceProducer) from the Flutter
 * plugin's Kotlin code into the engine runtime, which renders into it via an
 * EGL WindowSurface (ANGLE) and eglSwapBuffers.
 *
 * Aligned with upstream reAAAq/KrKr2-Next (krkr2_android.cpp): also stores the
 * JavaVM (for JNI calls from native threads), the Application Context (used by
 * environ/android/AndroidUtils.cpp as a KR2Activity fallback in Flutter mode),
 * and provides a JNI_OnLoad that hands the VM to krkr::JniHelper.
 *
 * Symbols provided:
 *   - krkr_GetJavaVM() / krkr_GetJNIEnv():
 *     JavaVM/JNIEnv for the current thread (attaches if needed).
 *   - krkr_GetApplicationContext(): global Application Context (Flutter mode),
 *     returned WITHOUT extra ref (caller must not free).
 *   - krkr_GetNativeWindow() / krkr_GetSurfaceDimensions(): consumed by
 *     engine_api.cpp (Android) for auto-attaching the Surface render target.
 *     krkr_GetNativeWindow returns an ADDITIONAL reference that the caller must
 *     release with ANativeWindow_release().
 *   - JNI entry points called by FlutterEngineBridgePlugin (Kotlin):
 *     nativeSetSurface(window, width, height) / nativeDetachSurface() /
 *     nativeSetApplicationContext(context).
 *
 * Thread-safety: stored values are guarded by mutexes.
 */

#if defined(__ANDROID__) || defined(ANDROID)

#include <jni.h>
#include <android/log.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <mutex>

#include "android/KrkrJniHelper.h"

#define LOG_TAG "krkr2"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {

// ---------------------------------------------------------------------------
// JavaVM global storage (for JNI calls from any native thread)
// ---------------------------------------------------------------------------
std::mutex g_jvm_mutex;
JavaVM* g_javaVM = nullptr;

// ---------------------------------------------------------------------------
// ANativeWindow global storage for the Surface bridge
// ---------------------------------------------------------------------------
std::mutex g_surface_mutex;
ANativeWindow* g_native_window = nullptr;  // retained reference
uint32_t g_surface_width = 0;
uint32_t g_surface_height = 0;

// ---------------------------------------------------------------------------
// Application Context global storage (Flutter mode; KR2Activity may not run)
// ---------------------------------------------------------------------------
std::mutex g_context_mutex;
jobject g_app_context = nullptr;  // global ref

}  // namespace

// ---------------------------------------------------------------------------
// JavaVM / JNIEnv accessors
// ---------------------------------------------------------------------------

extern "C" JavaVM* krkr_GetJavaVM() {
  std::lock_guard<std::mutex> lock(g_jvm_mutex);
  return g_javaVM;
}

extern "C" JNIEnv* krkr_GetJNIEnv() {
  JavaVM* vm = krkr_GetJavaVM();
  if (!vm) return nullptr;

  JNIEnv* env = nullptr;
  jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (status == JNI_EDETACHED) {
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      LOGE("Failed to attach current thread to JVM");
      return nullptr;
    }
  }
  return env;
}

// ---------------------------------------------------------------------------
// Application Context accessor
// ---------------------------------------------------------------------------

extern "C" jobject krkr_GetApplicationContext() {
  std::lock_guard<std::mutex> lock(g_context_mutex);
  return g_app_context;
}

// ---------------------------------------------------------------------------
// ANativeWindow accessors
// ---------------------------------------------------------------------------

extern "C" ANativeWindow* krkr_GetNativeWindow() {
  std::lock_guard<std::mutex> lock(g_surface_mutex);
  if (g_native_window) {
    ANativeWindow_acquire(g_native_window);
  }
  return g_native_window;
}

extern "C" void krkr_GetSurfaceDimensions(uint32_t* out_width,
                                          uint32_t* out_height) {
  std::lock_guard<std::mutex> lock(g_surface_mutex);
  if (out_width) {
    *out_width = g_surface_width;
  }
  if (out_height) {
    *out_height = g_surface_height;
  }
}

// ---------------------------------------------------------------------------
// JNI_OnLoad: store JavaVM and hand it to krkr::JniHelper (used by AndroidUtils)
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  {
    std::lock_guard<std::mutex> lock(g_jvm_mutex);
    g_javaVM = vm;
  }
  krkr::JniHelper::setJavaVM(vm);
  LOGI("krkr2 JNI_OnLoad: JavaVM stored");
  return JNI_VERSION_1_6;
}

/*
 * Java package: dev.pocketkrkr.flutter_engine_bridge
 * JNI symbol:   Java_dev_pocketkrkr_flutter_1engine_1bridge_FlutterEngineBridgePlugin_nativeSetSurface
 * Pass null surface to detach.
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_pocketkrkr_flutter_1engine_1bridge_FlutterEngineBridgePlugin_nativeSetSurface(
    JNIEnv* env, jobject /*thiz*/, jobject surface, jint width, jint height) {
  std::lock_guard<std::mutex> lock(g_surface_mutex);
  if (g_native_window) {
    ANativeWindow_release(g_native_window);
    g_native_window = nullptr;
    g_surface_width = 0;
    g_surface_height = 0;
  }
  if (surface) {
    g_native_window = ANativeWindow_fromSurface(env, surface);
    if (g_native_window) {
      g_surface_width = width > 0 ? static_cast<uint32_t>(width) : 0;
      g_surface_height = height > 0 ? static_cast<uint32_t>(height) : 0;
      LOGI("nativeSetSurface: ANativeWindow acquired (%dx%d)", width, height);
    } else {
      LOGE("nativeSetSurface: ANativeWindow_fromSurface failed");
    }
  } else {
    LOGI("nativeSetSurface: Surface detached (null)");
  }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_pocketkrkr_flutter_1engine_1bridge_FlutterEngineBridgePlugin_nativeDetachSurface(
    JNIEnv* /*env*/, jobject /*thiz*/) {
  std::lock_guard<std::mutex> lock(g_surface_mutex);
  if (g_native_window) {
    ANativeWindow_release(g_native_window);
    g_native_window = nullptr;
    g_surface_width = 0;
    g_surface_height = 0;
    LOGI("nativeDetachSurface: ANativeWindow released");
  }
}

/*
 * JNI bridge: Flutter Kotlin plugin -> C++ engine.
 * Passes the Android Application Context so engine code (AndroidUtils.cpp)
 * can call Context methods (getExternalFilesDirs / getFilesDir, ...).
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_pocketkrkr_flutter_1engine_1bridge_FlutterEngineBridgePlugin_nativeSetApplicationContext(
    JNIEnv* env, jobject /*thiz*/, jobject context) {
  std::lock_guard<std::mutex> lock(g_context_mutex);
  if (g_app_context) {
    env->DeleteGlobalRef(g_app_context);
    g_app_context = nullptr;
  }
  if (context) {
    g_app_context = env->NewGlobalRef(context);
    LOGI("nativeSetApplicationContext: Application Context stored");
  } else {
    LOGW("nativeSetApplicationContext: null context passed");
  }
}

#endif  // __ANDROID__