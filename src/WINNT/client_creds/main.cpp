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
#include <osilog.h>
#include <afs/fs_utils.h>
}

#include "afscreds.h"
#include "..\afsreg\afsreg.h" // So we can see if the server's installed
#include "drivemap.h"
#include <stdlib.h>
#include <stdio.h>
#include "rxkad.h"

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

const TCHAR cszCLASSNAME[] = TEXT("AfsCreds");


/*
 * VARIABLES __________________________________________________________________
 *
 */

GLOBALS g;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL InitApp (LPSTR pszCmdLineA);
void ExitApp (void);
void Quit (void);
void PumpMessage (MSG *pmsg);
BOOL IsServerInstalled (void);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR pCmdLine, int nCmdShow)
{
   Shortcut_Init();
   TaLocale_LoadCorrespondingModule (hInst);

   osi_InitTraceOption();
   osi_LogEvent0("AFSCreds Main command line",pCmdLine);
   fs_utils_InitMountRoot();

   if (InitApp (pCmdLine))
      {
      MSG msg;
      while (GetMessage (&msg, NULL, 0, 0) > 0)
         {
         PumpMessage (&msg);
         }

      ExitApp();
      }

   Shortcut_Exit();
   return 0;
}

#define ISHIGHSECURITY(v) ( ((v) & LOGON_OPTION_HIGHSECURITY)==LOGON_OPTION_HIGHSECURITY)
#define REG_CLIENT_PROVIDER_KEY "SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider"

BOOL InitApp (LPSTR pszCmdLineA)
{
   BOOL fShow = FALSE;
   BOOL fQuiet = FALSE;
   BOOL fExit = FALSE;
   BOOL fInstall = FALSE;
   BOOL fUninstall = FALSE;

   // Parse the command-line
   //
   while (pszCmdLineA && *pszCmdLineA)
      {
      if ((*pszCmdLineA != '-') && (*pszCmdLineA != '/'))
         break;

      switch (*(++pszCmdLineA))
         {
         case 's':
         case 'S':
            fShow = TRUE;
            break;

         case 'q':
         case 'Q':
            fQuiet = TRUE;
            break;

         case 'e':
         case 'E':
            fExit = TRUE;
            break;

         case 'i':
         case 'I':
            fInstall = TRUE;
            break;

         case 'u':
         case 'U':
            fUninstall = TRUE;
            break;
		 case ':':
			 MapShareName(pszCmdLineA);
			 break;
         case 'x':
         case 'X':
             TestAndDoMapShare(SERVICE_START_PENDING);
             TestAndDoMapShare(SERVICE_RUNNING);
	     return 0;
         }

      while (*pszCmdLineA && (*pszCmdLineA != ' '))
         ++pszCmdLineA;
	  if (*pszCmdLineA==' ') ++pszCmdLineA;
      }

   if (fInstall)
      Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup = TRUE);
   else if (fUninstall)
      Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup = FALSE);

   if (fInstall)
      {
      HKEY hk;
      if (RegCreateKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"), &hk) == 0)
         {
         DWORD dwSize = sizeof(g.fStartup);
         DWORD dwType = REG_DWORD;
         RegSetValueEx (hk, TEXT("ShowTrayIcon"), NULL, dwType, (PBYTE)&g.fStartup, dwSize);
         RegCloseKey (hk);
         }
      }

   // Only show up if there's not another version of this app around already.
   //
   for (HWND hSearch = GetWindow (GetDesktopWindow(), GW_CHILD);
        hSearch && IsWindow(hSearch);
        hSearch = GetWindow (hSearch, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];
      if (GetClassName (hSearch, szClassName, cchRESOURCE))
         {
         if (!lstrcmpi (szClassName, cszCLASSNAME))
            {
            if (fShow)
               PostMessage (hSearch, WM_COMMAND, M_ACTIVATE, 0);
            else if (fExit)
               PostMessage (hSearch, WM_COMMAND, M_TERMINATE_NOW, 0);
            else if (fUninstall)
               PostMessage (hSearch, WM_COMMAND, M_TERMINATE_NOW, 0);
            return FALSE;
            }
         }
      }

   if (fExit || fUninstall || fInstall)
      return FALSE;

   // Initialize our global variables and window classes
   //
   memset (&g, 0x00, sizeof(g));
   g.fStartup = TRUE;

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"), &hk) == 0)
      {
      DWORD dwSize = sizeof(g.fStartup);
      DWORD dwType = REG_DWORD;
      RegQueryValueEx (hk, TEXT("ShowTrayIcon"), NULL, &dwType, (PBYTE)&g.fStartup, &dwSize);
      RegCloseKey (hk);
      }

   Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup);

   // Is this Windows NT?
   //
   OSVERSIONINFO Version;
   memset (&Version, 0x00, sizeof(Version));
   Version.dwOSVersionInfoSize = sizeof(Version);
   if (GetVersionEx (&Version))
      g.fIsWinNT = (Version.dwPlatformId == VER_PLATFORM_WIN32_NT) ? TRUE : FALSE;

   if (!g.fIsWinNT)
      lstrcpy (g.szHelpFile, TEXT("afs-light.hlp"));
   else
      lstrcpy (g.szHelpFile, TEXT("afs-nt.hlp"));

   // Initialize winsock etc
   //
   WSADATA Data;
   WSAStartup (0x0101, &Data);

   InitCommonControls();
   RegisterCheckListClass();
   osi_Init();
   lock_InitializeMutex(&g.expirationCheckLock, "expiration check lock");
   lock_InitializeMutex(&g.credsLock, "global creds lock");

   if ( IsDebuggerPresent() ) {
       if ( !g.fIsWinNT )
           OutputDebugString("No Service Present on non-NT systems\n");
       else {
           if ( IsServiceRunning() )
               OutputDebugString("AFSD Service started\n");
           else {
               OutputDebugString("AFSD Service stopped\n");
               if ( !IsServiceConfigured() )
                   OutputDebugString("AFSD Service not configured\n");
           }   
       }
   }

   // Create a main window. All further initialization will be done during
   // processing of WM_INITDIALOG.
   //
   WNDCLASS wc;
   if (!GetClassInfo (NULL, WC_DIALOG, &wc))   // Get dialog class info
      return FALSE;
   wc.hInstance = THIS_HINST;
   wc.hIcon = TaLocale_LoadIcon (IDI_MAIN);
   wc.lpszClassName = cszCLASSNAME;
   wc.style |= CS_GLOBALCLASS;
   RegisterClass(&wc);

   g.hMain = ModelessDialog (IDD_MAIN, NULL, (DLGPROC)Main_DlgProc);
   if (g.hMain == NULL)
      return FALSE;

   // If the service isn't started yet, show our startup wizard.
   //
   if (!IsServiceRunning() && !fQuiet)
      {
      if (!g.fIsWinNT)
         Message (MB_ICONHAND, IDS_UNCONFIG_TITLE_95, IDS_UNCONFIG_DESC_95);
      else if (IsServiceConfigured())
         ShowStartupWizard();
      else if (!IsServerInstalled())
         Message (MB_ICONHAND, IDS_UNCONFIG_TITLE, IDS_UNCONFIG_DESC);
      }
   if (IsServiceRunning()) { 
      if (fShow)
      {
      if ( IsDebuggerPresent() )
          OutputDebugString("Displaying Main window\n");
      Main_Show (TRUE);
      }
   } else if ( IsDebuggerPresent() )
      OutputDebugString("Displaying Main window\n");
    return TRUE;
}


void ExitApp (void)
{
   g.hMain = NULL;
}


void PumpMessage (MSG *pmsg)
{
   if (!IsMemoryManagerMessage (pmsg))
      {
      if (!IsDialogMessage (g.hMain, pmsg))
         {
         TranslateMessage (pmsg);
         DispatchMessage (pmsg);
         }
      }
}


void Quit (void)
{
   if (IsWindow (g.hMain))
      {
      ChangeTrayIcon (NIM_DELETE);
      DestroyWindow (g.hMain);
      }
   PostQuitMessage (0);
}


BOOL IsServerInstalled (void)
{
   BOOL fInstalled = FALSE;

   TCHAR szKey[] = AFSREG_SVR_SVC_KEY;
   LPCTSTR pch = lstrchr (szKey, TEXT('\\'));

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, &pch[1], &hk) == 0)
      {
      fInstalled = TRUE;
      RegCloseKey (hk);
      }

   return fInstalled;
}

