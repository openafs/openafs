/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_CONFIG_H
#define AFS_CONFIG_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaLocale.h>
#include <windows.h>
#include <commctrl.h>
#include <regstr.h>
#include <ctl_sockaddr.h>
#include <ctl_spinner.h>
#include <checklist.h>
#include <dialog.h>
#include <fastlist.h>
#include <tab_hosts.h>
#include "cellservdb.h"
#include "drivemap.h"
#include "resource.h"
#include "config.h"
#include "help.hid"
#include <WINNT\afsreg.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   HWND hMain;
   BOOL fIsWinNT;
   BOOL fIsAdmin;
   BOOL fIsCCenter;
   LPPROPSHEET psh;

   struct
      {
      TCHAR szGateway[ MAX_PATH ];
      TCHAR szCell[ MAX_PATH ];
      TCHAR szSysName[ MAX_PATH ];
      TCHAR szRootVolume[ MAX_PATH ];
      TCHAR szMountDir[ MAX_PATH ];
      TCHAR szCachePath[ MAX_PATH ];
      BOOL fBeGateway;
      BOOL fShowTrayIcon;
      BOOL fLogonAuthent;
      BOOL fTrapOnPanic;
      BOOL fFailLoginsSilently;
      BOOL fReportSessionStartups;
      DWORD ckCache;
      DWORD ckChunk;
      DWORD cStatEntries;
      DWORD csecProbe;
      DWORD nThreads;
      DWORD nDaemons;
      DWORD nLanAdapter;
      DWORD nTraceBufSize;
      DWORD nLoginRetryInterval;
      PSERVERPREFS pFServers;
      PSERVERPREFS pVLServers;
      CELLSERVDB CellServDB;
      DRIVEMAPLIST NetDrives;
      DRIVEMAPLIST GlobalDrives;
      BOOL fChangedPrefs;
      } Configuration;

   BOOL fNeedRestart;
   TCHAR szHelpFile[ MAX_PATH ];
   } GLOBALS;

extern GLOBALS g;

/*
 * MACROS _____________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) AfsConfigReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
#endif

#ifndef iswhite
#define iswhite(_ch) (((_ch)==TEXT(' ')) || ((_ch)==TEXT('\t')))
#endif
#ifndef iseol
#define iseol(_ch) (((_ch)==TEXT('\r')) || ((_ch)==TEXT('\n')))
#endif
#ifndef iswhiteeol
#define iswhiteeol(_ch) (iswhite(_ch) || iseol(_ch))
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Quit (void);

BOOL AfsConfigReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc);

void Main_OnInitDialog (HWND hMain);

void Main_RefreshAllTabs (void);

LPCTSTR GetCautionTitle (void);
LPCTSTR GetErrorTitle (void);


#endif

