/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "svr_col.h"
#include "svc_col.h"
#include "agg_col.h"
#include "set_col.h"
#include "svr_security.h"
#include "creds.h"
#include "propcache.h"
#include "action.h"
#include "subset.h"
#include "messages.h"
#include "cmdline.h"

extern "C" {
#include <afs/afs_AdminErrors.h>
} // extern "C"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * VARIABLES __________________________________________________________________
 *
 */

GLOBALS g;
GLOBALS_RESTORED gr;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL InitApplication (HINSTANCE hInst, LPTSTR pszCmdLine, int nCmdShow);
void ExitApplication (void);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

extern "C" int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR pszCmdLineA, int nCmdShow)
{
   LPTSTR pszCmdLine = AnsiToString (pszCmdLineA);

   if (InitApplication (hInst, pszCmdLine, nCmdShow))
      {
      AfsAppLib_MainPump();

      ExitApplication();
      }

   FreeString (pszCmdLine, pszCmdLineA);
   return g.rc;
}


BOOL InitApplication (HINSTANCE hInst, LPTSTR pszCmdLine, int nCmdShow)
{
   TaLocale_LoadCorrespondingModule (hInst);

   memset (&g, 0x00, sizeof(g));
   g.hInst = hInst;
   g.hAccel = TaLocale_LoadAccelerators (ACCEL_MAIN);

   HWND hPrevious;
   TCHAR szTitle[ cchRESOURCE ];
   GetString (szTitle, IDS_APP_TITLE);
   if ((hPrevious = FindWindow (TEXT("AFSManagerClass"), szTitle)) != NULL)
      {
      SetFocus (hPrevious);
      SendMessage (hPrevious, WM_SHOW_YOURSELF, 0, 0);
      return FALSE;
      }

   AfsAppLib_SetAppName(szTitle);
   AfsAppLib_SetPumpRoutine(PumpMessage);

   TASKQUEUE_PARAMS tqp;
   memset (&tqp, 0x00, sizeof(tqp));
   tqp.nThreadsMax = 10;
   tqp.fnCreateTaskPacket = CreateTaskPacket;
   tqp.fnPerformTask = PerformTask;
   tqp.fnFreeTaskPacket = FreeTaskPacket;
   AfsAppLib_InitTaskQueue (&tqp);

   Main_ConfigureHelp();

   // Determine how the app is supposed to look--that is, remember what it
   // looked like last time, and if there was no "last time", pick some
   // decent defaults.
   //
   if (!RestoreSettings (REGSTR_SETTINGS_BASE, REGSTR_SETTINGS_PATH, REGVAL_SETTINGS, &gr, sizeof(gr), wVerGLOBALS_RESTORED))
      {
      memset (&gr, 0x00, sizeof(gr));
      SetRectEmpty (&gr.rMain);
      SetRectEmpty (&gr.rMainPreview);
      SetRectEmpty (&gr.rServerLast);
      SetRectEmpty (&gr.rViewLog);
      SetRectEmpty (&gr.rActions);

      gr.fPreview = TRUE;
      gr.fVert = TRUE;
      gr.fActions = FALSE;

      gr.tabLast = tabFILESETS;

      Server_SetDefaultView_Horz (&gr.diHorz.viewSvr);
      Server_SetDefaultView_Vert (&gr.diVert.viewSvr);
      Services_SetDefaultView (&gr.viewSvc);
      Aggregates_SetDefaultView (&gr.viewAgg);
      Filesets_SetDefaultView (&gr.viewSet);
      Replicas_SetDefaultView (&gr.viewRep);
      Action_SetDefaultView (&gr.viewAct);
      Server_Key_SetDefaultView (&gr.viewKey);

      gr.diHorz.cSplitter = -100;
      gr.diVert.cSplitter = -89;

      gr.cbQuotaUnits = cb1KB;

      gr.fOpenMonitors = TRUE;
      gr.fCloseUnmonitors = TRUE;
      gr.fServerLongNames = FALSE;
      gr.fDoubleClickOpens = 2;
      gr.fWarnBadCreds = TRUE;

      gr.ivSvr = ivSTATUS;
      gr.ivAgg = ivSTATUS;
      gr.ivSet = ivSTATUS;
      gr.ivSvc = ivSTATUS;
      }

   ULONG status;
   if (!AfsClass_Initialize (&status))
      {
      if (status == ADMCLIENTCANTINITAFSLOCATION)
         ImmediateErrorDialog (status, IDS_ERROR_CANT_INIT_AFSCLASS_INSTALL);
      else
         ImmediateErrorDialog (status, IDS_ERROR_CANT_INIT_AFSCLASS_UNKNOWN);
      return FALSE;
      }

   AfsClass_RequestLongServerNames (gr.fServerLongNames);
   AfsClass_SpecifyRefreshDomain (AFSCLASS_WANT_VOLUMES);

   // Create a notification object for the AFSClass library, so that it can
   // let us know when anything changes.  The notification handler we'll
   // install will take requests from the rest of the SVRMGR package and
   // forward notifications around to whichever windows are actually
   // interested.
   //
   CreateNotificationDispatch();

   // Create a few variations on WC_DIALOG, so we get appropriate icons on
   // our windows.
   //
   WNDCLASS wc;
   GetClassInfo (THIS_HINST, MAKEINTRESOURCE( WC_DIALOG ), &wc);
   wc.hInstance = THIS_HINST;
   wc.hIcon = TaLocale_LoadIcon (IDI_MAIN);
   wc.lpszClassName = TEXT("AFSManagerClass");
   wc.style |= CS_GLOBALCLASS;
   RegisterClass (&wc);

   GetClassInfo (THIS_HINST, MAKEINTRESOURCE( WC_DIALOG ), &wc);
   wc.hInstance = THIS_HINST;
   wc.hIcon = TaLocale_LoadIcon (IDI_SERVER);
   wc.lpszClassName = TEXT("ServerWindowClass");
   wc.style |= CS_GLOBALCLASS;
   RegisterClass (&wc);

   // Okay, the big step: create the main window (ie, the servers list).
   // Note that it doesn't get shown yet!
   //
   CMDLINEOP op = ParseCommandLine (pszCmdLine);
   if (op == opCLOSEAPP)
      return FALSE;

   if (op == opLOOKUPERRORCODE)
      {
      Help_FindError();
      return FALSE;
      }

   g.hMain = ModelessDialog (IDD_MAIN, NULL, (DLGPROC)Main_DialogProc);
   if (g.hMain == NULL)
      return FALSE;

   if (op != opNOCELLDIALOG)
      {
      if (OpenCellDialog() != IDOK)
         return FALSE;
      }

   return TRUE;
}


void ExitApplication (void)
{
}


void Quit (int rc)
{
   if (g.hMain && IsWindow(g.hMain))
      {
      if (gr.fPreview && !gr.fVert)
         FL_StoreView (GetDlgItem (g.hMain, IDC_SERVERS), &gr.diHorz.viewSvr);
      else
         FL_StoreView (GetDlgItem (g.hMain, IDC_SERVERS), &gr.diVert.viewSvr);

      WINDOWPLACEMENT wpl;
      wpl.length = sizeof(wpl);
      if (GetWindowPlacement (g.hMain, &wpl))
         {
         if (gr.fPreview)
            gr.rMainPreview = wpl.rcNormalPosition;
         else
            gr.rMain = wpl.rcNormalPosition;
         }
      }

   StoreSettings (REGSTR_SETTINGS_BASE, REGSTR_SETTINGS_PATH, REGVAL_SETTINGS, &gr, sizeof(gr), wVerGLOBALS_RESTORED);

   if (Subsets_SaveIfDirty (g.sub))
      {
      if (Action_fAnyActive())  // just *pretend* to close the app
         {
         Action_WindowToTop (FALSE);
         ShowWindow (g.hMain, SW_HIDE);
         }
      else
         {
         g.rc = rc;
         PostQuitMessage (g.rc);
         }
      }
}


void PumpMessage (MSG *lpm)
{
   if (g.hMain && IsWindow (g.hMain))
      {
      if (GetActiveWindow())
         {
         if (TranslateAccelerator (GetActiveWindow(), g.hAccel, lpm))
            return;
         }
      }

   if (!IsMemoryManagerMessage (lpm))
      {
      TranslateMessage (lpm);
      DispatchMessage (lpm);
      }
}


BOOL cdecl StartThread (DWORD (WINAPI *lpfnStart)(PVOID lp), ...)
{
   va_list   arg;
   va_start (arg, lpfnStart);
   PVOID     lp = va_arg (arg, PVOID);

   DWORD dwThreadID;
   HANDLE hThread;

   if ((hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)lpfnStart, lp, 0, &dwThreadID)) == NULL)
      return FALSE;

   SetThreadPriority (hThread, THREAD_PRIORITY_BELOW_NORMAL);
   return TRUE;
}

