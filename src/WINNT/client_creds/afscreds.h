/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCREDS_H
#define AFSCREDS_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaLocale.h>
#undef REALLOC /* RT ticket 2120; So we can redefine it later */
#include <windows.h>
#include <commctrl.h>
#include <regstr.h>
#include <time.h>
#include <shellapi.h>
#include <WINNT/al_wizard.h>
#include <WINNT\afsreg.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <osithrdnt.h>
#include <osisleep.h>
#include <osibasel.h>
#include <rxkad.h>
#ifdef __cplusplus
}
#endif
#include "resource.h"
#include "checklist.h"
#include "window.h"
#include "shortcut.h"
#include "trayicon.h"
#include "creds.h"
#include "credstab.h"
#include "advtab.h"
#include "mounttab.h"
#include "afswiz.h"
#include "drivemap.h"
#include "help.hid"


/*
 * REG PATHS __________________________________________________________________
 *
 */

#define REGVAL_AFS_TITLE     TEXT("Title")
#define REGVAL_AFS_VERSION   TEXT("VersionString")
#define REGVAL_AFS_PATCH     TEXT("PatchLevel")
#define REGVAL_AFS_PATH      TEXT("PathName")

#define cszSHORTCUT_NAME     TEXT("AFS Credentials.lnk")

#define AFSCREDS_SHORTCUT_OPTIONS  TEXT("-A -M -N -Q")

/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szCell[ MAX_PATH ];
   TCHAR szUser[ MAX_PATH ];
   SYSTEMTIME stExpires;
   BOOL fRemind;
   } CREDS, *LPCREDS;

typedef struct
   {
   HWND hMain;
   CREDS *aCreds;
   size_t cCreds;
   DWORD tickLastRetest;
   LONG fShowingMessage;
   LPWIZARD pWizard;
   BOOL fStartup;
   BOOL fIsWinNT;
   TCHAR szHelpFile[ MAX_PATH ];
   osi_mutex_t expirationCheckLock;
   osi_mutex_t credsLock;
   TCHAR SmbName[ MAXRANDOMNAMELEN ];
   } GLOBALS;

extern GLOBALS g;


/*
 * TIMING _____________________________________________________________________
 *
 */

#define cminREMIND_TEST      1    // test every minute for expired creds
#define cminREMIND_WARN      15   // warn if creds expire in 15 minutes
#define cminRENEW            20   // renew creds when there are 20 minutes remaining
#define cminMINLIFE          30   // minimum life of Kerberos creds

#define cmsecMOUSEOVER       1000 // retest freq when mouse is over tray icon
#define cmsecSERVICE         2000 // retest freq when starting/stopping service

#define c100ns1SECOND        (LONGLONG)10000000
#define cmsec1SECOND         1000
#define cmsec1MINUTE         60000
#define csec1MINUTE          60


#define ID_REMIND_TIMER      1000
#define ID_SERVICE_TIMER     1001
#define ID_WIZARD_TIMER      1002


/*
 * MACROS _____________________________________________________________________
 *
 */

#ifndef FileExists
#define FileExists(_psz)  ((GetFileAttributes (_psz) == 0xFFFFFFFF) ? FALSE : TRUE)
#endif

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef iswhite
#define iswhite(_ch) ( ((_ch)==TEXT(' ')) || ((_ch)==TEXT('\t')) || ((_ch)==TEXT('\r')) || ((_ch)==TEXT('\n')) )
#endif

#ifndef cxRECT
#define cxRECT(_r) ((_r).right - (_r).left)
#endif

#ifndef cyRECT
#define cyRECT(_r) ((_r).bottom - (_r).top)
#endif

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) AfsCredsReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Quit (void);

BOOL AfsCredsReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc);

void LoadRemind (size_t iCreds);
void SaveRemind (size_t iCreds);

void TimeToSystemTime (SYSTEMTIME *pst, time_t TimeT);

LPARAM GetTabParam (HWND hTab, int iTab);
HWND GetTabChild (HWND hTab);


#endif

