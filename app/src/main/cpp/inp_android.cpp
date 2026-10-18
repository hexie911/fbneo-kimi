// Android input module: Java injects logical pad states
#include "burner.h"
#include <string.h>

static UINT8 sState[512];

INT32 Android_MapInput(INT32 player, INT32 id)
{
    if (player < 1 || player > 4) return -1;
    INT32 base = (player - 1) * 64;
    if (id >= 0 && id <= 3)  return base + id;
    if (id >= 4 && id <= 19) return base + 16 + (id - 4);
    if (id == 20) return base + 60;
    if (id == 21) return base + 61;
    return -1;
}

extern "C" void Android_SetInputState(INT32 player, INT32 id, INT32 down)
{
    INT32 code = Android_MapInput(player, id);
    if (code >= 0 && code < 512) sState[code] = down ? 1 : 0;
}

void RemapGameInp()
{
    for (UINT32 i = 0; i < nGameInpCount; i++) {
        struct GameInp* pgi = &GameInp[i];
        if (pgi->nInput != GIT_SWITCH) continue;

        UINT32 player = (pgi->nIdent >> 28) & 0xF;
        UINT32 action = (pgi->nIdent >> 20) & 0xF;
        UINT32 dir    = (pgi->nIdent >>  4) & 0xF;
        UINT32 btn    = (pgi->nIdent >>  8) & 0x1F;

        INT32 code = -1;
        if (player >= 1 && player <= 4) {
            INT32 base = (player - 1) * 64;
            if (action == 1)      code = base + 60;
            else if (action == 2) code = base + 61;
            else if (dir >= 1 && dir <= 4) code = base + (dir - 1);
            else if (btn >= 1 && btn <= 16) code = base + 16 + (btn - 1);
        }
        if (code < 0) code = 500;
        pgi->Input.Switch.nCode = code;
    }
}

static INT32 InpAndroidInit()  { memset(sState, 0, sizeof(sState)); return 0; }
static INT32 InpAndroidExit()  { return 0; }
static INT32 InpAndroidNewFrame() { return 0; }

static INT32 InpAndroidReadSwitch(INT32 code)
{
    if (code >= 0 && code < 512) return sState[code];
    return 0;
}

static INT32 InpAndroidReadJoyAxis(INT32, INT32)   { return 0; }
static INT32 InpAndroidReadMouseAxis(INT32, INT32) { return 0; }
static INT32 InpAndroidFind(INT32)                 { return -1; }

static INT32 InpAndroidGetControlName(INT32, TCHAR* pszDeviceName, TCHAR* pszControlName)
{
    if (pszDeviceName)  pszDeviceName[0]  = 0;
    if (pszControlName) pszControlName[0] = 0;
    return 1;
}

struct InputInOut InputInOutAndroid;

void Android_RegisterInputInOut()
{
    InputInOutAndroid.szModuleName        = _T("Android virtual pads");
    InputInOutAndroid.SetCooperativeLevel = NULL;
    InputInOutAndroid.Init                = InpAndroidInit;
    InputInOutAndroid.Exit                = InpAndroidExit;
    InputInOutAndroid.NewFrame            = InpAndroidNewFrame;
    InputInOutAndroid.ReadSwitch          = InpAndroidReadSwitch;
    InputInOutAndroid.ReadJoyAxis         = InpAndroidReadJoyAxis;
    InputInOutAndroid.ReadMouseAxis       = InpAndroidReadMouseAxis;
    InputInOutAndroid.Find                = InpAndroidFind;
    InputInOutAndroid.GetControlName      = InpAndroidGetControlName;
    InputInOutAndroid.GetPluginSettings   = NULL;
}
