// JNI bridge between the Kotlin UI and the FBNeo Android port
#include "burner.h"
#include "luaengine.h"
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "FBNeo", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FBNeo", __VA_ARGS__)

extern "C" {
    void Android_RegisterPrintf();
    void Android_RegisterVidOut();
    void Android_RegisterAudOut();
    void Android_RegisterInputInOut();
    INT32 StatedAuto(INT32);
    INT16* Android_AudioBeginWrite(INT32);
    void   Android_AudioCommitWrite(INT32, INT32);
    INT32  Android_AudioPull(INT16*, INT32);
    void   Android_AudioReset();
    void   Android_VideoCopyFrame(UINT8*);
    void   Android_VideoGetSize(INT32*, INT32*);
    void   Android_SetInputState(INT32, INT32, INT32);
    const char* Android_GetLastError();
    void   InitSupportPaths(const char*);
}

static JavaVM* gVm = NULL;
static jclass  gConsoleClass = NULL;
static jmethodID gConsoleMethod = NULL;
static int gPipe[2] = { -1, -1 };

void Android_PushConsoleLine(const char* line)
{
    if (!gVm || !gConsoleClass || !gConsoleMethod || !line) return;
    JNIEnv* env = NULL;
    bool attached = false;
    if (gVm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (gVm->AttachCurrentThread(&env, NULL) != JNI_OK) return;
        attached = true;
    }
    jstring s = env->NewStringUTF(line);
    env->CallStaticVoidMethod(gConsoleClass, gConsoleMethod, s);
    env->DeleteLocalRef(s);
    if (attached) gVm->DetachCurrentThread();
}

static void* ConsolePump(void*)
{
    char buf[1024]; int n = 0;
    while (true) {
        char c;
        ssize_t r = read(gPipe[0], &c, 1);
        if (r <= 0) break;
        if (c == '\n' || n >= (int)sizeof(buf) - 1) {
            buf[n] = 0;
            if (n > 0) Android_PushConsoleLine(buf);
            n = 0;
        } else if (c != '\r') {
            buf[n++] = c;
        }
    }
    return NULL;
}

static void StartConsoleRedirect()
{
    if (pipe(gPipe) != 0) return;
    dup2(gPipe[1], STDOUT_FILENO);
    dup2(gPipe[1], STDERR_FILENO);
    setvbuf(stdout, NULL, _IONBF, 0);
    pthread_t t; pthread_create(&t, NULL, ConsolePump, NULL);
    pthread_detach(t);
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeInit(JNIEnv* env, jclass, jstring filesDir)
{
    env->GetJavaVM(&gVm);
    const char* dir = env->GetStringUTFChars(filesDir, NULL);
    InitSupportPaths(dir);
    chdir(dir);
    env->ReleaseStringUTFChars(filesDir, dir);

    StartConsoleRedirect();

    Android_RegisterPrintf();
    Android_RegisterVidOut();
    Android_RegisterAudOut();
    Android_RegisterInputInOut();

    ComputeGammaLUT();
    BurnLibInit();

    LOGI("FBNeo lib initialized, %u drivers", nBurnDrvCount);
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeSetConsoleHandler(JNIEnv* env, jclass, jclass handlerClass, jstring methodName)
{
    if (gConsoleClass) env->DeleteGlobalRef(gConsoleClass);
    const char* mn = env->GetStringUTFChars(methodName, NULL);
    gConsoleClass = (jclass)env->NewGlobalRef(handlerClass);
    gConsoleMethod = env->GetStaticMethodID(handlerClass, mn, "(Ljava/lang/String;)V");
    env->ReleaseStringUTFChars(methodName, mn);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeGetDriverCount(JNIEnv*, jclass) { return (jint)nBurnDrvCount; }

extern "C" JNIEXPORT jstring JNICALL
Java_com_fbneo_android_Fbneo_nativeGetDriverName(JNIEnv* env, jclass, jint i)
{
    nBurnDrvActive = (UINT32)i;
    return env->NewStringUTF(BurnDrvGetTextA(DRV_FULLNAME));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_fbneo_android_Fbneo_nativeGetDriverZip(JNIEnv* env, jclass, jint i)
{
    char* name = NULL;
    nBurnDrvActive = (UINT32)i;
    BurnDrvGetZipName(&name, 0);
    return env->NewStringUTF(name ? name : "");
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeLoadGame(JNIEnv*, jclass, jint idx)
{
    Android_SetLastError("");
    if (DrvInit((int)idx, false)) {
        LOGE("DrvInit failed for driver %d", (int)idx);
        return 1;
    }
    if (MediaInit()) { return 2; }
    AudSoundPlay();
    StatedAuto(0);
    Android_AudioReset();
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeUnloadGame(JNIEnv*, jclass)
{
    if (bDrvOkay) StatedAuto(1);
    DrvExit();
    MediaExit();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeGetFps100(JNIEnv*, jclass) { return nAppVirtualFps; }

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeRunFrame(JNIEnv*, jclass) { Android_RunFrame(); }

extern "C" JNIEXPORT jstring JNICALL
Java_com_fbneo_android_Fbneo_nativeGetLastError(JNIEnv* env, jclass)
{
    return env->NewStringUTF(Android_GetLastError());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeVideoWidth(JNIEnv*, jclass)
{
    INT32 w, h; Android_VideoGetSize(&w, &h); return w;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeVideoHeight(JNIEnv*, jclass)
{
    INT32 w, h; Android_VideoGetSize(&w, &h); return h;
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeCopyFrame(JNIEnv* env, jclass, jobject byteBuffer)
{
    void* dst = env->GetDirectBufferAddress(byteBuffer);
    if (dst) Android_VideoCopyFrame((UINT8*)dst);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeAudioRate(JNIEnv*, jclass) { return nAudSampleRate[nAudSelect]; }

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeAudioFrames(JNIEnv*, jclass) { return nAudSegLen; }

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeAudioPull(JNIEnv* env, jclass, jobject byteBuffer, jint frames)
{
    void* dst = env->GetDirectBufferAddress(byteBuffer);
    return dst ? Android_AudioPull((INT16*)dst, frames) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeSetInput(JNIEnv*, jclass, jint player, jint id, jint down)
{
    Android_SetInputState(player, id, down);
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeReset(JNIEnv*, jclass) { extern bool bResetDrv; bResetDrv = true; }

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeSaveState(JNIEnv* env, jclass, jstring path)
{
    const char* p = env->GetStringUTFChars(path, NULL);
    TCHAR t[MAX_PATH]; snprintf(t, MAX_PATH, "%s", p);
    INT32 r = BurnStateSave(t, 1);
    if (r == 0) luasav_save(p);
    env->ReleaseStringUTFChars(path, p);
    return r;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeLoadState(JNIEnv* env, jclass, jstring path)
{
    const char* p = env->GetStringUTFChars(path, NULL);
    TCHAR t[MAX_PATH]; snprintf(t, MAX_PATH, "%s", p);
    INT32 r = BurnStateLoad(t, 1, &DrvInitCallback);
    if (r == 0) luasav_load(p);
    env->ReleaseStringUTFChars(path, p);
    return r;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeLuaLoad(JNIEnv* env, jclass, jstring path)
{
    const char* p = env->GetStringUTFChars(path, NULL);
    INT32 r = FBA_LoadLuaCode(p);
    env->ReleaseStringUTFChars(path, p);
    return r;
}

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeLuaStop(JNIEnv*, jclass) { FBA_LuaStop(); }

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeLuaReload(JNIEnv*, jclass) { FBA_ReloadLuaCode(); }

extern "C" JNIEXPORT jint JNICALL
Java_com_fbneo_android_Fbneo_nativeLuaRunning(JNIEnv*, jclass) { return FBA_LuaRunning(); }

extern "C" JNIEXPORT void JNICALL
Java_com_fbneo_android_Fbneo_nativeSetFast(JNIEnv*, jclass, jint on) { bAppDoFast = (on != 0); }
