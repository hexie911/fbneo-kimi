// Android video output module: renders pVidImage into an RGBA front buffer
#include "burner.h"
#include <android/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FBNeo", __VA_ARGS__)

static UINT8* sFront = NULL;
static INT32  sW = 0, sH = 0;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

static UINT32 __cdecl HighCol32(INT32 r, INT32 g, INT32 b, INT32)
{
    return 0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

static INT32 VidAndroidInit()
{
    INT32 w = 0, h = 0;
    BurnDrvGetFullSize(&w, &h);
    if (w <= 0 || h <= 0) { w = 320; h = 240; }

    nVidImageWidth  = w;
    nVidImageHeight = h;
    nVidImageBPP    = 4;
    nVidImageDepth  = 32;
    nVidImagePitch  = w * 4;
    BurnDrvGetVisibleOffs(&nVidImageLeft, &nVidImageTop);

    free(pVidImage);
    pVidImage = (UINT8*)calloc(1, w * h * 4);
    pVidImageSize = w * h * 4;
    VidHighCol = HighCol32;

    pthread_mutex_lock(&sLock);
    free(sFront);
    sW = w - nVidImageLeft * 2; if (sW <= 0) sW = w;
    sH = h - nVidImageTop  * 2; if (sH <= 0) sH = h;
    sFront = (UINT8*)calloc(1, sW * sH * 4);
    pthread_mutex_unlock(&sLock);

    LOGI("vid init %dx%d (visible %dx%d)", w, h, sW, sH);
    return pVidImage ? 0 : 1;
}

static INT32 VidAndroidExit()
{
    free(pVidImage); pVidImage = NULL; pVidImageSize = 0;
    pthread_mutex_lock(&sLock);
    free(sFront); sFront = NULL;
    pthread_mutex_unlock(&sLock);
    return 0;
}

static void BlitToFront()
{
    INT32 vw = 0, vh = 0;
    BurnDrvGetVisibleSize(&vw, &vh);
    if (vw <= 0) vw = sW;
    if (vh <= 0) vh = sH;

    pthread_mutex_lock(&sLock);
    if (sFront) {
        for (INT32 y = 0; y < vh && y < sH; y++) {
            UINT8* src = pVidImage + (y + nVidImageTop) * nVidImagePitch + nVidImageLeft * 4;
            UINT8* dst = sFront + y * sW * 4;
            for (INT32 x = 0; x < vw && x < sW; x++, src += 4, dst += 4) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = 0xFF;
            }
        }
    }
    pthread_mutex_unlock(&sLock);
}

static INT32 VidAndroidFrame(INT32 bRedraw)
{
    INT32 nRet = VidFrameCallback(bRedraw);
    if (nRet == 0) BlitToFront();
    return nRet;
}

static INT32 VidAndroidPaint(INT32) { return 0; }

static INT32 VidAndroidImageSize(RECT* pRect, INT32 w, INT32 h)
{
    if (pRect) { pRect->left = 0; pRect->top = 0; pRect->right = w; pRect->bottom = h; }
    return 0;
}

struct VidOut VidOutAndroid;

void Android_RegisterVidOut()
{
    VidOutAndroid.szModuleName    = _T("Android RGBA");
    VidOutAndroid.Init            = VidAndroidInit;
    VidOutAndroid.Exit            = VidAndroidExit;
    VidOutAndroid.Frame           = VidAndroidFrame;
    VidOutAndroid.Paint           = VidAndroidPaint;
    VidOutAndroid.ImageSize       = VidAndroidImageSize;
    VidOutAndroid.GetPluginSettings = NULL;
}

extern "C" void Android_VideoCopyFrame(UINT8* dst)
{
    pthread_mutex_lock(&sLock);
    if (sFront && dst) memcpy(dst, sFront, sW * sH * 4);
    pthread_mutex_unlock(&sLock);
}

extern "C" void Android_VideoGetSize(INT32* w, INT32* h)
{
    *w = sW; *h = sH;
}

void Android_PushConsoleLine(const char* line);

INT32 VidSNewTinyMsg(TCHAR* pText, ...)
{
    if (pText) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", TCHARToANSI(pText, NULL, 0));
        LOGI("osd: %s", buf);
        Android_PushConsoleLine(buf);
    }
    return 0;
}

INT32 VidSNewShortMsg(TCHAR* pText, ...)
{
    return VidSNewTinyMsg(pText);
}
