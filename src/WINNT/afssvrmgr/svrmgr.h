/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVRMGR_H
#define SVRMGR_H


/*
 * COMMON HEADERS _____________________________________________________________
 *
 */

#include <windows.h>
#include <WINNT/AfsAppLib.h>
#include <WINNT/AfsClass.h>


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

#define cszENDING_REPLICA  TEXT(".readonly")
#define cszENDING_CLONE    TEXT(".clone")

#define REGSTR_SETTINGS_BASE  HKCU
#define REGSTR_SETTINGS_PATH  TEXT("Software\\OpenAFS\\AFS Server Manager")
#define REGVAL_SETTINGS       TEXT("Settings")
#define REGSTR_SETTINGS_PREFS TEXT("Software\\OpenAFS\\AFS Server Manager\\Preferences")
#define REGSTR_SETTINGS_CELLS REGSTR_SETTINGS_PREFS


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include "resource.h"
#include "window.h"
#include "dispatch.h"
#include "alert.h"
#include "general.h"
#include "subset.h"
#include "prefs.h"

#include "help.hid"
#define cszHELPFILENAME  TEXT("TaAfsSvrMgr.hlp")


/*
 * STRUCTURES _________________________________________________________________
 *
 */

typedef enum // CHILDTAB
   {
   tabINVALID = -1,
   tabFILESETS = 0,
   tabAGGREGATES,
   tabSERVICES
   } CHILDTAB;

typedef struct // DISPLAYINFO
   {
   LONG cSplitter;	// splitter between servers and tabs
   VIEWINFO viewSvr;
   } DISPLAYINFO;

typedef struct // SERVER_PREF
   {
   OBJECTALERTS oa;

   short perWarnAggFull;	// ==0 to disable warning
   short perWarnSetFull;	// ==0 to disable warning
   BOOL fWarnSvcStop;	// ==FALSE to disable warning
   BOOL fWarnSvrTimeout;	// ==FALSE to disable warning
   BOOL fWarnSetNoVLDB;	// ==FALSE to disable warning
   BOOL fWarnSetNoServ;	// ==FALSE to disable warning
   BOOL fWarnAggNoServ;	// ==FALSE to disable warning
   BOOL fWarnAggAlloc;	// ==FALSE to disable warning

   RECT rLast;
   BOOL fOpen;
   BOOL fIsMonitored;
   BOOL fExpandTree;
   SERVERSTATUS ssLast;
   } SERVER_PREF, *LPSERVER_PREF;

#define wVerSERVER_PREF  MAKEVERSION(2,1)


typedef struct // SERVICE_PREF
   {
   OBJECTALERTS oa;

   BOOL fWarnSvcStop;
   TCHAR szLogFile[ MAX_PATH ];

   SERVICESTATUS ssLast;
   } SERVICE_PREF, *LPSERVICE_PREF;

#define wVerSERVICE_PREF  MAKEVERSION(1,0)


typedef struct // AGGREGATE_PREF
   {
   OBJECTALERTS oa;
   short perWarnAggFull;	// ==0 to disable, ==-1 for svr default
   BOOL fWarnAggAlloc;	// ==FALSE to disable warning

   TCHAR szDevice[ cchNAME ];
   AGGREGATESTATUS asLast;
   BOOL fExpandTree;
   } AGGREGATE_PREF, *LPAGGREGATE_PREF;

#define wVerAGGREGATE_PREF  MAKEVERSION(2,1)


typedef struct // FILESET_PREF
   {
   OBJECTALERTS oa;
   short perWarnSetFull;	// ==0 to disable, ==-1 for svr default

   FILESETSTATUS fsLast;
   LPIDENT lpiRW;
   } FILESET_PREF, *LPFILESET_PREF;

#define wVerFILESET_PREF  MAKEVERSION(1,1)


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
   LPIDENT lpiCell;

   LPSUBSET sub;
   UINT_PTR hCreds;
   } GLOBALS;

typedef struct
   {
   // Window placement
   //
   RECT rMain;
   RECT rMainPreview;
   RECT rServerLast;
   RECT rActions;
   BOOL fPreview;
   BOOL fVert;
   BOOL fActions;

   // How is information viewed?
   //
   CHILDTAB tabLast;
   VIEWINFO viewSvc;
   VIEWINFO viewAgg;
   VIEWINFO viewSet;
   VIEWINFO viewRep;
   VIEWINFO viewAggMove;
   VIEWINFO viewAggCreate;
   VIEWINFO viewAggRestore;
   VIEWINFO viewAct;
   VIEWINFO viewKey;
   DISPLAYINFO diHorz;
   DISPLAYINFO diVert;
   RECT rViewLog;
   size_t cbQuotaUnits;
   BOOL fOpenMonitors;
   BOOL fCloseUnmonitors;
   BOOL fServerLongNames;
   BOOL fDoubleClickOpens;  // 0=Properties, 1=Open, 2=Depends (PreviewPane)
   BOOL fWarnBadCreds;
   ICONVIEW ivSvr;
   ICONVIEW ivAgg;
   ICONVIEW ivSet;
   ICONVIEW ivSvc;
   } GLOBALS_RESTORED;

#define wVerGLOBALS_RESTORED  MAKEVERSION(2,3)


extern GLOBALS g;
extern GLOBALS_RESTORED gr;

/*
 * OTHER INCLUSIONS ___________________________________________________________
 *
 */

#include "task.h"
#include "helpfunc.h"


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

