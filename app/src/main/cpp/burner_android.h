// Header for the Android port of FinalBurn Neo (mirrors burner_sdl.h)
#pragma once
#include <unistd.h>
#include <stdarg.h>

typedef struct tagRECT { int left; int top; int right; int bottom; } RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;
typedef UINT32 DWORD;
typedef UINT8  BYTE;
typedef UINT16 WORD;
typedef UINT64 QWORD;

#ifndef MAX_PATH
#define MAX_PATH 511
#endif
#define _MAX_PATH MAX_PATH

#ifndef __cdecl
#define __cdecl
#endif

#define _getcwd getcwd
#define _chdir  chdir

#define ANSIToTCHAR(a, b, c) ((TCHAR*)(a))
#define TCHARToANSI(a, b, c) ((char*)(a))
#define _TtoA(a) TCHARToANSI(a, NULL, 0)
#define _AtoT(a) ANSIToTCHAR(a, NULL, 0)

bool AppProcessKeyboardInput();

int ConfigAppLoad();
int ConfigAppSave();

extern int  bDrvOkay;
extern char szAppRomPaths[DIRS_MAX][MAX_PATH];
int  DrvInit(int nDrvNum, bool bRestore);
int  DrvInitCallback();
int  DrvExit();
int  ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
int  AppError(TCHAR* szText, int bWarning);

int  RunReset();
int  Android_RunFrame();
extern bool bRunPause;
extern bool bAppDoFast;
extern int  nAppVirtualFps;

int MediaInit();
int MediaExit();

int InputInit();
int InputExit();
int InputMake(bool bCopy);

extern INT32  nReplayStatus;
extern INT32  bReplayReadOnly;
extern UINT32 nStartFrame;
INT32 StopReplay();
void  SetPauseMode(INT32 bPause);
void  EmulatorAppDoFast(bool dofast);
char* GetSavestateFilename(int nSlot);

extern TCHAR szAppListsPath[MAX_PATH];
extern TCHAR szAppDatListsPath[MAX_PATH];
extern TCHAR szAppArchivesPath[MAX_PATH];
void InitSupportPaths(const char* filesDir);
const char* Android_GetFilesDir();

void Android_PushConsoleLine(const char* line);
void Android_SetLastError(const char* e);
const char* Android_GetLastError();
