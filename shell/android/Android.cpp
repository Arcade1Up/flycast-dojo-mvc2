#include "emulator.h"
#include "emulator.hpp"
#include "emulator_api.h"
#include "types.h"
#include "wsi/context.h"
#include "network/ggpo.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <csignal>
#include <jni.h>


JavaVM* g_jvm;

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed.
class JVMAttacher {
 public:
  JVMAttacher() : _env(NULL), _detach_thread(false) {
  }
  JNIEnv *getEnv() {
    if (_env == NULL) {
      if (g_jvm == NULL) {
        die("g_jvm == NULL");
        return NULL;
      }
      int rc = g_jvm->GetEnv((void **)&_env, JNI_VERSION_1_6);
      if (rc == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&_env, NULL) != 0) {
          die("AttachCurrentThread failed");
          return NULL;
        }
        _detach_thread = true;
      } else if (rc == JNI_EVERSION) {
        die("JNI version error");
        return NULL;
      }
    }
    return _env;
  }

  ~JVMAttacher() {
    if (_detach_thread)
      g_jvm->DetachCurrentThread();
  }

 private:
  JNIEnv *_env;
  bool _detach_thread;
};
static thread_local JVMAttacher jvm_attacher;

void os_DoEvents() {
}

static ANativeWindow *g_window = nullptr;
void os_CreateWindow() {
  initRenderApi(g_window);
}

void UpdateInputState() {
}

void common_linux_setup();

void os_SetupInput() {
}
void os_TermInput() {
}

void os_SetWindowText(char const *Text) {
}

static void updateOption(int name, int value, const char *arg1, const char *arg2, int arg3, int arg4, const char *arg5, const char *arg6, int arg7);

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env, jobject obj) { sen::emulator.pause(); }
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env, jobject obj) { sen::emulator.resume(); }

std::vector<char *> args;
std::vector<jstring> args_jstr;
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_start(JNIEnv *env, jobject obj, jobject surface, jint length, jobjectArray arg) {
  common_linux_setup();
  g_window = ANativeWindow_fromSurface(env, surface);

  args_jstr.reserve(length);
  for (int i = 0; i < length; ++i)
    args_jstr.push_back((jstring)env->GetObjectArrayElement(arg, i));

  args.reserve(length);
  for (int i = 0; i < length; i++) {
    args.push_back((char *)env->GetStringUTFChars(args_jstr[i], nullptr));
  }

  flycast::start(length, args.data());
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_quit(JNIEnv *env, jobject obj) {
  int length = args.size();
  for (int i = 0; i < length; i++) {
    env->ReleaseStringUTFChars(args_jstr[i], args[i]);
  }
  args_jstr.clear();
  args.clear();

  flycast::quit();
  termRenderApi();
  ANativeWindow_release(g_window);
  g_window = nullptr;
}
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_term(JNIEnv *env, jobject obj) {
  emu.term();
}
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_nativeSetOption(JNIEnv *env, jobject obj, jstring key, jstring value) {
  char *c_key = (char*) env->GetStringUTFChars(key, nullptr);
  char *c_value = (char*) env->GetStringUTFChars(value, nullptr);

  ggpo::setOption(c_key, c_value);
}

static jclass jni_class = nullptr;
static jmethodID updateOptionMID = nullptr;
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_register(JNIEnv *env, jobject obj, jobject activity) {
  env->GetJavaVM(&g_jvm);

  jni_class = env->FindClass("com/reicast/emulator/emu/JNIdc");
  updateOptionMID = env->GetStaticMethodID(jni_class, "updateOption", "(IILjava/lang/String;Ljava/lang/String;II)V");

  Emulator_NotificationCallback(updateOption);
}
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_unregister(JNIEnv *env, jobject obj) {
  Emulator_NotificationCallback(nullptr);

  updateOptionMID = nullptr;
  jni_class = nullptr;
  g_jvm = nullptr;
}

static void updateOption(int name, int value, const char *arg1, const char *arg2, int arg3, int arg4, const char *arg5, const char *arg6, int arg7) {
  JNIEnv *env = jvm_attacher.getEnv();

  jstring jarg1 = env->NewStringUTF(arg1);
  jstring jarg2 = env->NewStringUTF(arg2);

  // ZGS_LOG("name %d, value %d, %s %s %d %d", name, value, arg1, arg2, arg3, arg4);
  env->CallStaticVoidMethod(jni_class, updateOptionMID, name, value, jarg1, jarg2, arg3, arg4);

  if (jarg1 != nullptr) env->DeleteLocalRef(jarg1);
  if (jarg2 != nullptr) env->DeleteLocalRef(jarg2);
}

void os_DebugBreak() {
  // TODO: notify the parent thread about it ...

  raise(SIGABRT);
  //pthread_exit(NULL);

  // Attach debugger here to figure out what went wrong
  for (;;)
    ;
}
void enableNetworkBroadcast(bool enable) {}
