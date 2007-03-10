/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSUSRMGR_H
#define TAAFSUSRMGR_H

#include <WINNT/TaLocale.h>
#include <WINNT/TaAfsAdmSvrClient.h>
#include <WINNT/AfsAppLib.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif

#ifndef cb1KB
#define cb1KB  1024L
#endif

#ifndef ck1MB
#define ck1MB  1024L
#endif

#ifndef cb1MB
#define cb1MB  1048576L
#endif

#ifndef ck1GB
#define ck1GB  1048576L
#endif

#ifndef ck1TB
#define ck1TB  (unsigned long)0x40000000  // 1073741824L == 1024^3
#endif

#define REGSTR_SETTINGS_BASE  HKCU
#define REGSTR_SETTINGS_PATH  TEXT("Software\\OpenAFS\\AFS Account Manager")
#define REGVAL_SETTINGS       TEXT("Settings")
#define REGSTR_SETTINGS_PREFS TEXT("Software\\OpenAFS\\AFS Account Manager\\Preferences")
#define REGSTR_SETTINGS_CELLS REGSTR_SETTINGS_PREFS


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include "resource.h"

#include "help.hid"
#define cszHELPFILENAME  TEXT("TaAfsUsrMgr.hlp")

#include "usr_prop.h"
#include "grp_prop.h"


/*
 * STRUCTURES _________________________________________________________________
 *
 */

typedef enum // ICONVIEW
   {
   ivTWOICONS,
   ivONEICON,
   ivSTATUS
   } ICONVIEW, *LPICONVIEW;

typedef struct
   {
   HINSTANCE hInst;
   HWND hMain;
   HWND hAction;
   HACCEL hAccel;
   int rc;

   UINT_PTR idClient;
   ASID idCell;
   UINT_PTR hCreds;

   TCHAR szPatternUsers[ cchNAME ];
   TCHAR szPatternGroups[ cchNAME ];
   TCHAR szPatternMachines[ cchNAME ];
   } GLOBALS;

typedef struct
   {
   // Window placement
   //
   RECT rMain;
   RECT rActions;

   // How is information viewed?
   //
   VIEWINFO viewAct;
   VIEWINFO viewUsr;
   VIEWINFO viewGrp;
   VIEWINFO viewMch;
   ICONVIEW ivUsr;
   ICONVIEW ivGrp;
   ICONVIEW ivMch;

   BOOL fWarnBadCreds;
   BOOL fShowActions;
   BOOL fWindowsRegexp;
   DWORD cminRefreshRate;

   size_t iTabLast;

   // What user preferences have been chosen?
   //
   USERPROPINFO CreateUser;
   GROUPPROPINFO CreateGroup;
   USERPROPINFO CreateMachine;

   AFSADMSVR_SEARCH_PARAMS SearchUsers;

   } GLOBALS_RESTORED;

#define wVerGLOBALS_RESTORED  MAKEVERSION(1,0)


extern GLOBALS g;
extern GLOBALS_RESTORED gr;

/*
 * OTHER INCLUSIONS ___________________________________________________________
 *
 */

#include "task.h"
#include "helpfunc.h"
#include "display.h"
#include "general.h"
#include "errdata.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Quit (int rc);

void PumpMessage (MSG *lpm);

// StartThread() accepts any 32-bit quantity as its second parameter;
// it uses '...' so you won't have to cast the thing regardless of what it
// is--an HWND fits through just as easily as an LPIDENT.
//
BOOL cdecl StartThread (DWORD (WINAPI *lpfnStart)(PVOID lp), ...);


#endif

