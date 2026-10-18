// Android audio module: ring buffer drained by the Java AudioTrack thread
#include "burner.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define RING_FRAMES (1 << 15)

static INT16 sRing[RING_FRAMES * 2];
static UINT32 sWr = 0, sRd = 0;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

extern "C" INT16* Android_AudioBeginWrite(INT32 frames)
{
    pthread_mutex_lock(&sLock);
    UINT32 avail = (sRd - sWr - 1) & (RING_FRAMES - 1);
    pthread_mutex_unlock(&sLock);
    if (avail < (UINT32)frames) return NULL;
    return sRing + sWr * 2;
}

extern "C" void Android_AudioCommitWrite(INT32 frames, INT32 wrote)
{
    pthread_mutex_lock(&sLock);
    if (wrote > 0) sWr = (sWr + frames) & (RING_FRAMES - 1);
    else           sRd = sWr = 0;
    pthread_mutex_unlock(&sLock);
}

extern "C" INT32 Android_AudioPull(INT16* dst, INT32 frames)
{
    pthread_mutex_lock(&sLock);
    UINT32 avail = (sWr - sRd) & (RING_FRAMES - 1);
    INT32 n = frames < (INT32)avail ? frames : (INT32)avail;
    if (n > 0) {
        if (sRd + n <= RING_FRAMES) {
            memcpy(dst, sRing + sRd * 2, n * 4);
        } else {
            INT32 p1 = RING_FRAMES - sRd;
            memcpy(dst, sRing + sRd * 2, p1 * 4);
            memcpy(dst + p1 * 2, sRing, (n - p1) * 4);
        }
        sRd = (sRd + n) & (RING_FRAMES - 1);
    }
    pthread_mutex_unlock(&sLock);
    if (n < frames) memset(dst + n * 2, 0, (frames - n) * 4);
    return n;
}

extern "C" void Android_AudioReset()
{
    pthread_mutex_lock(&sLock); sRd = sWr = 0; pthread_mutex_unlock(&sLock);
}

static INT32 AudAndroidBlankSound() { return 0; }
static INT32 AudAndroidSoundCheck() { return 0; }

static INT32 AudAndroidSoundInit()
{
    if (nAppVirtualFps <= 0) nAppVirtualFps = 6000;
    nAudSegLen = (nAudSampleRate[nAudSelect] * 100 + (nAppVirtualFps >> 1)) / nAppVirtualFps;
    if (nAudSegLen < 32) nAudSegLen = 735;
    nAudAllocSegLen = nAudSegLen << 2;
    nAudNextSound = (INT16*)malloc(nAudAllocSegLen);
    Android_AudioReset();
    return 0;
}

static INT32 AudAndroidSetCallback(INT32 (*)(INT32)) { return 0; }
static INT32 AudAndroidSoundPlay()  { return 0; }
static INT32 AudAndroidSoundStop()  { return 0; }

static INT32 AudAndroidSoundExit()
{
    free(nAudNextSound); nAudNextSound = NULL;
    return 0;
}

static INT32 AudAndroidSoundSetVolume() { return 0; }

struct AudOut AudOutAndroid;

void Android_RegisterAudOut()
{
    AudOutAndroid.szModuleName      = _T("Android AudioTrack");
    AudOutAndroid.BlankSound        = AudAndroidBlankSound;
    AudOutAndroid.SoundCheck        = AudAndroidSoundCheck;
    AudOutAndroid.SoundInit         = AudAndroidSoundInit;
    AudOutAndroid.SetCallback       = AudAndroidSetCallback;
    AudOutAndroid.SoundPlay         = AudAndroidSoundPlay;
    AudOutAndroid.SoundStop         = AudAndroidSoundStop;
    AudOutAndroid.SoundExit         = AudAndroidSoundExit;
    AudOutAndroid.SoundSetVolume    = AudAndroidSoundSetVolume;
    AudOutAndroid.GetPluginSettings = NULL;
}
