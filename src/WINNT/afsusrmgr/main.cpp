/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsUsrMgr.h"
#include "messages.h"
#include "window.h"
#include "cmdline.h"
#include "usr_col.h"
#include "usr_create.h"
#include "usr_search.h"
#include "grp_col.h"
#include "grp_create.h"
#include "mch_col.h"
#include "mch_create.h"
#include "creds.h"
#include "action.h"


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
      }
   ExitApplication();

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
   if ((hPrevious = FindWindow (TEXT("AFSAccountManagerClass"), szTitle)) != NULL)
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
      gr.cminRefreshRate = 60; // 1 hour default refresh rate

      User_SetDefaultCreateParams (&gr.CreateUser);
      Group_SetDefaultCreateParams (&gr.CreateGroup);
      Machine_SetDefaultCreateParams (&gr.CreateMachine);
      Actions_SetDefaultView (&gr.viewAct);
      User_SetDefaultView (&gr.viewUsr, &gr.ivUsr);
      Group_SetDefaultView (&gr.viewGrp, &gr.ivGrp);
      Machine_SetDefaultView (&gr.viewMch, &gr.ivMch);
      Users_SetDefaultSearchParams (&gr.SearchUsers);
      }

   // Create a variation on WC_DIALOG, so we get appropriate icons on
   // our windows.
   //
   WNDCLASS wc;
   GetClassInfo (THIS_HINST, MAKEINTRESOURCE( WC_DIALOG ), &wc);
   wc.hInstance = THIS_HINST;
   wc.hIcon = TaLocale_LoadIcon (IDI_MAIN);
   wc.lpszClassName = TEXT("AFSAccountManagerClass");
   wc.style |= CS_GLOBALCLASS;
   RegisterClass (&wc);

   // Okay, the big step: create the main window.  Note that it doesn't
   // get shown yet!
   //
   CMDLINEOP op = ParseCommandLine (pszCmdLine);
   if (op == opCLOSEAPP)
      return FALSE;

   // Okay, the big step: create the main window.
   // Note that it doesn't get shown yet!
   //
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
   if (g.idClient)
      {
      ULONG status;
      if (g.idCell)
         asc_CellClose (g.idClient, g.idCell, &status);
      AfsAppLib_CloseAdminServer();
      }
}


void Quit (int rc)
{
   if (g.hMain && IsWindow(g.hMain))
      {
      WINDOWPLACEMENT wpl;
      wpl.length = sizeof(wpl);
      if (GetWindowPlacement (g.hMain, &wpl))
         gr.rMain = wpl.rcNormalPosition;
      }

   StoreSettings (REGSTR_SETTINGS_BASE, REGSTR_SETTINGS_PATH, REGVAL_SETTINGS, &gr, sizeof(gr), wVerGLOBALS_RESTORED);
   PostQuitMessage (0);
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

