// Android driver/burner glue, adapted from FBNeo's SDL port
#include "burner.h"
#include <android/log.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "FBNeo", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FBNeo", __VA_ARGS__)

int   bDrvOkay = 0;
int   nAppVirtualFps = 0;
bool  bRunPause = false;
bool  bAppDoFast = false;
bool  bAppFullscreen = false;
bool  bAlwaysProcessKeyboardInput = false;
TCHAR szAppBurnVer[16] = _T("1.0.0.3");
INT32 bCheatsAllowed = 1;
INT32 bDrvSaveAll = 0;
INT32 EnableHiscores = 1;
INT32 nReplayStatus = 0;
INT32 bReplayReadOnly = 0;
UINT32 nStartFrame = 0;
bool  bResetDrv = false;

INT32 nAnalogSpeed = 0x0100;
INT32 nFireButtons = 0;

char szAppRomPaths[DIRS_MAX][MAX_PATH] = { { "" }, };

TCHAR szAppHiscorePath[MAX_PATH];   TCHAR szAppSamplesPath[MAX_PATH];
TCHAR szAppHDDPath[MAX_PATH];       TCHAR szAppBlendPath[MAX_PATH];
TCHAR szAppEEPROMPath[MAX_PATH];    TCHAR szAppSnesMsu1Path[MAX_PATH];
TCHAR szAppPreviewsPath[MAX_PATH];  TCHAR szAppTitlesPath[MAX_PATH];
TCHAR szAppCheatsPath[MAX_PATH];    TCHAR szAppIpsPath[MAX_PATH];
TCHAR szAppRomdataPath[MAX_PATH];   TCHAR szAppIconsPath[MAX_PATH];
TCHAR szAppSelectPath[MAX_PATH];    TCHAR szAppVersusPath[MAX_PATH];
TCHAR szAppHowtoPath[MAX_PATH];     TCHAR szAppScoresPath[MAX_PATH];
TCHAR szAppBossesPath[MAX_PATH];    TCHAR szAppGameoverPath[MAX_PATH];
TCHAR szAppFlyersPath[MAX_PATH];    TCHAR szAppMarqueesPath[MAX_PATH];
TCHAR szAppControlsPath[MAX_PATH];  TCHAR szAppCabinetsPath[MAX_PATH];
TCHAR szAppPCBsPath[MAX_PATH];      TCHAR szAppHistoryPath[MAX_PATH];
TCHAR szAppCommandPath[MAX_PATH];
TCHAR szAppListsPath[MAX_PATH];     TCHAR szAppDatListsPath[MAX_PATH];
TCHAR szAppArchivesPath[MAX_PATH];

TCHAR szAppSavestatePath[MAX_PATH];
TCHAR szAppConfigPath[MAX_PATH];
TCHAR szAppScreenshotPath[MAX_PATH];

static char gFilesDir[MAX_PATH] = { 0 };
static char gLastError[1024] = { 0 };

const char* Android_GetFilesDir() { return gFilesDir; }
void Android_SetLastError(const char* e) { snprintf(gLastError, sizeof(gLastError), "%s", e ? e : ""); }
const char* Android_GetLastError() { return gLastError; }

static void mk(const char* path) { mkdir(path, 0755); }

void InitSupportPaths(const char* filesDir)
{
    snprintf(gFilesDir, sizeof(gFilesDir), "%s", filesDir);
    #define P(var, sub) snprintf(var, MAX_PATH, "%s/" sub, gFilesDir); mk(var)
    P(szAppHiscorePath,   "hiscore");
    P(szAppSamplesPath,   "samples");
    P(szAppHDDPath,       "hdd");
    P(szAppBlendPath,     "blend");
    P(szAppEEPROMPath,    "eeprom");
    P(szAppSnesMsu1Path,  "snesmsu1");
    P(szAppPreviewsPath,  "previews");
    P(szAppTitlesPath,    "titles");
    P(szAppCheatsPath,    "cheats");
    P(szAppIpsPath,       "ips");
    P(szAppRomdataPath,   "romdata");
    P(szAppIconsPath,     "icons");
    P(szAppSelectPath,    "select");
    P(szAppVersusPath,    "versus");
    P(szAppHowtoPath,     "howto");
    P(szAppScoresPath,    "scores");
    P(szAppBossesPath,    "bosses");
    P(szAppGameoverPath,  "gameover");
    P(szAppFlyersPath,    "flyers");
    P(szAppMarqueesPath,  "marquees");
    P(szAppControlsPath,  "controls");
    P(szAppCabinetsPath,  "cabinets");
    P(szAppPCBsPath,      "pcbs");
    P(szAppHistoryPath,   "history");
    P(szAppCommandPath,   "command");
    P(szAppListsPath,     "lists");
    P(szAppDatListsPath,  "dats");
    P(szAppArchivesPath,  "archives");
    P(szAppSavestatePath, "savestates");
    P(szAppScreenshotPath,"screenshots");
    P(szAppConfigPath,    "config");
    #undef P
    snprintf(szAppRomPaths[0], MAX_PATH, "%s/roms", gFilesDir); mk(szAppRomPaths[0]);
    snprintf(szAppRomPaths[1], MAX_PATH, "%s",      gFilesDir);
}

INT32 StopReplay() { return 0; }
void SetPauseMode(INT32 bPause) { bRunPause = (bPause != 0); }
void EmulatorAppDoFast(bool dofast) { bAppDoFast = dofast; }

char* GetSavestateFilename(int nSlot)
{
    static char buf[MAX_PATH];
    snprintf(buf, sizeof(buf), "%s/%s slot %02x.fs",
             szAppSavestatePath, BurnDrvGetTextA(DRV_NAME), nSlot);
    return buf;
}

bool AppProcessKeyboardInput() { return true; }
int ConfigAppLoad() { return 1; }
int ConfigAppSave() { return 0; }

int ProgressUpdateBurner(double, const TCHAR*, bool) { return 0; }

int AppError(TCHAR* szText, int)
{
    char msg[1024];
    snprintf(msg, sizeof(msg), "FBNeo error: %s", szText ? TCHARToANSI(szText, NULL, 0) : "?");
    LOGE("%s", msg);
    Android_SetLastError(msg);
    Android_PushConsoleLine(msg);
    return 0;
}

static int AppDebugPrintf(int, TCHAR* pszFormat, ...)
{
    char buf[2048];
    va_list args; va_start(args, pszFormat);
    vsnprintf(buf, sizeof(buf), pszFormat, args); va_end(args);
    LOGI("%s", buf);
    return 0;
}

void Android_RegisterPrintf() { bprintf = AppDebugPrintf; }

extern UINT32 nFramesEmulated;
static struct timespec ts_start;

int RunReset()
{
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    return 0;
}

extern "C" INT16* Android_AudioBeginWrite(INT32 frames);
extern "C" void   Android_AudioCommitWrite(INT32 frames, INT32 wrote);

int Android_RunFrame()
{
    if (!bDrvOkay) return 1;

    CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);

    nFramesEmulated++;
    nCurrentFrame++;

    InputMake(true);

    INT16* pSnd = Android_AudioBeginWrite(nBurnSoundLen);
    pBurnSoundOut = pSnd;

    if (VidFrame()) {
        pBurnDraw = NULL;
        BurnDrvFrame();
        AudBlankSound();
    }
    Android_AudioCommitWrite(nBurnSoundLen, pSnd ? nBurnSoundLen : 0);
    pBurnSoundOut = NULL;

    VidPaint(0);

    CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
    FBA_LuaFrameBoundary();
    return 0;
}

int MediaInit()
{
    if (!bInputOkay) InputInit();

    nAppVirtualFps = nBurnFPS;

    if (!bAudOkay) AudSoundInit();

    nBurnSoundRate = 0;
    pBurnSoundOut = NULL;
    if (bAudOkay) {
        nBurnSoundRate = nAudSampleRate[nAudSelect];
        nBurnSoundLen = nAudSegLen;
    }

    if (!bVidOkay) VidInit();

    return 0;
}

int MediaExit()
{
    nBurnSoundRate = 0;
    pBurnSoundOut = NULL;
    AudSoundExit();
    VidExit();
    InputExit();
    return 0;
}

static int DoLibInit()
{
    BzipOpen(false);
    int nRet = BurnDrvInit();
    BzipClose();
    return nRet;
}

static int DrvLoadRom(unsigned char* Dest, int* pnWrote, int i)
{
    BzipOpen(false);
    int nRet = BurnExtLoadRom(Dest, pnWrote, i);
    BzipClose();
    BurnExtLoadRom = DrvLoadRom;
    return nRet;
}

extern void RemapGameInp();

int DrvInit(int nDrvNum, bool)
{
    DrvExit();
    MediaExit();

    nBurnDrvActive = nDrvNum;

    {
        bVidOkay = 1; bAudOkay = 1;
        MediaInit();
        bVidOkay = 0; bAudOkay = 0;
    }

    nMaxPlayers = BurnDrvGetMaxPlayers();
    GameInpInit();

    bSaveInputs = ConfigGameLoad(true);
    if (bSaveInputs) ConfigGameLoadHardwareDefaults();

    InputMake(true);
    GameInpDefault();
    RemapGameInp();

    if (DoLibInit()) {
        BurnDrvExit();
        return 1;
    }

    BurnExtLoadRom = DrvLoadRom;

    bDrvOkay = 1;
    nBurnLayer = 0xFF;
    ConfigCheatLoad();
    RunReset();
    return 0;
}

int DrvInitCallback() { return DrvInit(nBurnDrvActive, false); }

int DrvExit()
{
    if (bDrvOkay) {
        if (nBurnDrvActive < nBurnDrvCount) {
            ConfigGameSave(bSaveInputs);
            GameInpExit();
            BurnDrvExit();
        }
    }
    BurnExtLoadRom = NULL;
    bDrvOkay = 0;
    return 0;
}

int StatedAuto(int bSave)
{
    TCHAR szName[MAX_PATH];
    _stprintf(szName, _T("%s/%s.fs"), szAppSavestatePath, BurnDrvGetText(DRV_NAME));
    if (bSave == 0) return BurnStateLoad(szName, bDrvSaveAll, NULL);
    return BurnStateSave(szName, bDrvSaveAll);
}
