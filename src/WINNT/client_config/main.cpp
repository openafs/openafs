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
#include <afs/fs_utils.h>
}

#include "afs_config.h"
#include "isadmin.h"
#include "tab_general.h"
#include "tab_prefs.h"
#include "tab_hosts.h"
#include "tab_drives.h"
#include "tab_advanced.h"

/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * VARIABLES __________________________________________________________________
 *
 */

GLOBALS g;

/*
 * ROUTINES ___________________________________________________________________
 *
 */

extern "C" int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR pCmdLine, int nCmdShow)
{
   TaLocale_LoadCorrespondingModule (hInst);

   // Initialize winsock etc
   //
   WSADATA Data;
   WSAStartup (0x0101, &Data);

   InitCommonControls();
   RegisterCheckListClass();
   RegisterFastListClass();
   RegisterSockAddrClass();
   RegisterSpinnerClass();
   fs_utils_InitMountRoot();

   // Initialize our global variables and window classes
   //
   memset (&g, 0x00, sizeof(g));
   g.fIsWinNT = IsWindowsNT();
   g.fIsAdmin = IsAdmin();

   // Check our command-line options
   //
   while (pCmdLine && (*pCmdLine=='/' || *pCmdLine=='-'))
      {
      switch (*(++pCmdLine))
         {
         case 'c':
         case 'C':
            g.fIsCCenter = TRUE;
            break;
         }

      while (*pCmdLine == ' ')
         ++pCmdLine;
      }

   // Select an appropriate help file
   //
   if (g.fIsCCenter)
      lstrcpy (g.szHelpFile, TEXT("afs-cc.hlp>dialog"));
   else if (g.fIsWinNT)
      lstrcpy (g.szHelpFile, TEXT("afs-nt.hlp>dialog"));
   else
      lstrcpy (g.szHelpFile, TEXT("afs-light.hlp>dialog"));

   // Our main window is actually a tabbed dialog.
   //
   if ((g.psh = PropSheet_Create (((g.fIsCCenter) ? IDS_TITLE_CCENTER : (g.fIsWinNT) ? IDS_TITLE_NT : IDS_TITLE_95), FALSE, NULL)) == NULL)
      return FALSE;

   g.psh->sh.dwFlags |= PSH_NOAPPLYNOW;  // Remove the Apply button
   g.psh->sh.dwFlags |= PSH_HASHELP;     // Add a Help button instead

   if (g.fIsCCenter)
      {
      PropSheet_AddTab (g.psh, 0, IDD_HOSTS_CCENTER, (DLGPROC)HostsTab_DlgProc, 0, TRUE);
      }
   else
      {
      PropSheet_AddTab (g.psh, 0, ((g.fIsWinNT) ? IDD_GENERAL_NT : IDD_GENERAL_95), (DLGPROC)GeneralTab_DlgProc, 0, TRUE);

      PropSheet_AddTab (g.psh, 0, ((g.fIsWinNT) ? IDD_DRIVES_NT : IDD_DRIVES_95), (DLGPROC)DrivesTab_DlgProc, 0, TRUE);

      if (g.fIsWinNT)
         PropSheet_AddTab (g.psh, 0, IDD_PREFS_NT, (DLGPROC)PrefsTab_DlgProc, 0, TRUE);

      PropSheet_AddTab (g.psh, 0, ((g.fIsWinNT) ? IDD_HOSTS_NT : IDD_HOSTS_95), (DLGPROC)HostsTab_DlgProc, 0, TRUE);

      if (g.fIsWinNT)
         PropSheet_AddTab (g.psh, 0, IDD_ADVANCED_NT, (DLGPROC)AdvancedTab_DlgProc, 0, TRUE);
      }

   PropSheet_ShowModal (g.psh);

   return 0;
}


void Main_OnInitDialog (HWND hMain)
{
   g.hMain = hMain;

   // Center the window in the display
   //
   RECT rWindow;
   GetWindowRect (g.hMain, &rWindow);

   RECT rDesktop;
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rDesktop, 0);

   SetWindowPos (g.hMain, NULL,
                 rDesktop.left + ((rDesktop.right - rDesktop.left) - (rWindow.right - rWindow.left)) / 2,
                 rDesktop.top + ((rDesktop.bottom - rDesktop.top) - (rWindow.bottom - rWindow.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

   // Remove the Context Help [?] thing from the title bar
   //
   DWORD dwStyle = GetWindowLong (g.hMain, GWL_STYLE);
   dwStyle &= ~DS_CONTEXTHELP;
   SetWindowLong (g.hMain, GWL_STYLE, dwStyle);

   dwStyle = GetWindowLong (hMain, GWL_EXSTYLE);
   dwStyle &= ~WS_EX_CONTEXTHELP;
   SetWindowLong (g.hMain, GWL_EXSTYLE, dwStyle);
}


void Main_RefreshAllTabs (void)
{
   for (size_t ii = 0; ii < g.psh->cTabs; ++ii)
      {
      if (!g.psh->aTabs[ii].dlgproc)
         continue;
      if (!IsWindow (g.psh->aTabs[ii].hDlg))
         continue;

      CallWindowProc ((WNDPROC)(g.psh->aTabs[ii].dlgproc), g.psh->aTabs[ii].hDlg, WM_COMMAND, IDC_REFRESH, 0);
      }
}


void Quit (void)
{
   if (IsWindow (g.hMain))
      {
      DestroyWindow (g.hMain);
      }
   PostQuitMessage (0);
}


LPCTSTR GetCautionTitle (void)
{
   static TCHAR szTitle[ cchRESOURCE ] = TEXT("");
   if (!szTitle[0])
      GetString (szTitle, (g.fIsCCenter) ? IDS_TITLE_CAUTION_CCENTER : (g.fIsWinNT) ? IDS_TITLE_CAUTION_NT : IDS_TITLE_CAUTION_95);
   return szTitle;
}


LPCTSTR GetErrorTitle (void)
{
   static TCHAR szTitle[ cchRESOURCE ] = TEXT("");
   if (!szTitle[0])
      GetString (szTitle, (g.fIsCCenter) ? IDS_TITLE_ERROR_CCENTER : (g.fIsWinNT) ? IDS_TITLE_ERROR_NT : IDS_TITLE_ERROR_95);
   return szTitle;
}

