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
#include <afs/afskfw.h>
}
#include <WINNT\afsreg.h>

#include "afscreds.h"
#ifdef USE_KFW
#include "afskrb5.h"
#endif

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Advanced_OnInitDialog (HWND hDlg);
void Advanced_StartTimer (HWND hDlg);
void Advanced_OnServiceTimer (HWND hDlg);
void Advanced_OnOpenCPL (HWND hDlg);
void Advanced_OnChangeService (HWND hDlg, WORD wCmd);
void Advanced_OnStartup (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Advanced_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         SetWindowPos (hDlg, NULL, rTab.left, rTab.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

         Advanced_OnInitDialog (hDlg);
         break;

      case WM_TIMER:
         Advanced_OnServiceTimer (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_SERVICE_STOP:
            case IDC_SERVICE_START:
            case IDC_SERVICE_AUTO:
               Advanced_OnChangeService (hDlg, LOWORD(wp));
               break;

            case IDC_OPEN_CPL:
               Advanced_OnOpenCPL (hDlg);
               break;

            case IDC_STARTUP:
               Advanced_OnStartup (hDlg);
               break;

            case IDHELP:
               Advanced_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCREDS_TAB_ADVANCED);
         break;
      }
   return FALSE;
}


void Advanced_OnInitDialog (HWND hDlg)
{
   CheckDlgButton (hDlg, IDC_STARTUP, g.fStartup);
   Advanced_OnServiceTimer (hDlg);
   Advanced_StartTimer (hDlg);
}


void Advanced_StartTimer (HWND hDlg)
{
   KillTimer (hDlg, ID_SERVICE_TIMER);
   SetTimer (hDlg, ID_SERVICE_TIMER, (ULONG)cmsecSERVICE, NULL);
}


void Advanced_OnServiceTimer (HWND hDlg)
{
   BOOL fFinal = TRUE;
   BOOL fFound = FALSE;

   struct {
      QUERY_SERVICE_CONFIG Config;
      BYTE buf[1024];
   } Config;
   memset (&Config, 0x00, sizeof(Config));
   Config.Config.dwStartType = SERVICE_DEMAND_START;

   SERVICE_STATUS Status;
   memset (&Status, 0x00, sizeof(Status));
   Status.dwCurrentState = SERVICE_STOPPED;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         fFound = TRUE;
         DWORD dwSize = sizeof(Config);
         QueryServiceConfig (hService, (QUERY_SERVICE_CONFIG*)&Config, sizeof(Config), &dwSize);
         QueryServiceStatus (hService, &Status);
		 TestAndDoMapShare(Status.dwCurrentState);

         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   if (fFound)
      {
      if ((Status.dwCurrentState == SERVICE_STOP_PENDING) ||
          (Status.dwCurrentState == SERVICE_START_PENDING))
         {
         fFinal = FALSE;
         }
      }

   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_START), fFound && (Status.dwCurrentState == SERVICE_STOPPED));
   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_STOP), fFound && (Status.dwCurrentState == SERVICE_RUNNING));
   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_AUTO), fFound);
   CheckDlgButton (hDlg, IDC_SERVICE_AUTO, fFound && (Config.Config.dwStartType == SERVICE_AUTO_START));

   TCHAR szStatus[cchRESOURCE];
   if (!fFound)
      GetString (szStatus, IDS_SERVICE_BROKEN);
   else if (Status.dwCurrentState == SERVICE_RUNNING)
      GetString (szStatus, IDS_SERVICE_RUNNING);
   else if (Status.dwCurrentState == SERVICE_STOPPED)
      GetString (szStatus, IDS_SERVICE_STOPPED);
   else if (Status.dwCurrentState == SERVICE_STOP_PENDING)
      GetString (szStatus, IDS_SERVICE_STOPPING);
   else if (Status.dwCurrentState == SERVICE_START_PENDING)
      GetString (szStatus, IDS_SERVICE_STARTING);
   else
      GetString (szStatus, IDS_SERVICE_UNKNOWN);
   TestAndDoMapShare(Status.dwCurrentState);
   SetDlgItemText (hDlg, IDC_SERVICE_STATUS, szStatus);

   if (fFinal && GetWindowLong (hDlg, DWL_USER))
      {
      SetWindowLong (hDlg, DWL_USER, 0);
      Main_RepopulateTabs (FALSE);
      }

   if (fFinal)
      {
      KillTimer (hDlg, ID_SERVICE_TIMER);
      }
}


void Advanced_OnChangeService (HWND hDlg, WORD wCmd)
{
   BOOL fSuccess = FALSE;
   ULONG error = 0;
   SC_HANDLE hManager, hService;
   
    switch (wCmd)
    {
    case IDC_SERVICE_AUTO:
        DWORD StartType;
        if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT |
                                        SC_MANAGER_ENUMERATE_SERVICE |
                                        SC_MANAGER_QUERY_LOCK_STATUS)) != NULL)
        {
            if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), 
                                         SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG | 
                                         SERVICE_QUERY_STATUS)) != NULL)
            {
                StartType = (IsDlgButtonChecked (hDlg, wCmd)) ? SERVICE_AUTO_START : SERVICE_DEMAND_START;
                if (ChangeServiceConfig (hService, SERVICE_NO_CHANGE, StartType, 
                                         SERVICE_NO_CHANGE, 0, 0, 0, 0, 0, 0, 0))
                    fSuccess = TRUE;
                CloseServiceHandle (hService);
            }
            CloseServiceHandle (hManager);
        }
        break;

    case IDC_SERVICE_START:
        if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT |
                                        SC_MANAGER_ENUMERATE_SERVICE |
                                        SC_MANAGER_QUERY_LOCK_STATUS )) != NULL)
        {
            if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), 
                                         SERVICE_QUERY_STATUS | SERVICE_START)) != NULL)
            {
                if (StartService (hService, 0, 0))
                {
                    TestAndDoMapShare(SERVICE_START_PENDING);
                    if ( KFW_is_available() && KFW_AFS_wait_for_service_start() ) {
#ifdef USE_MS2MIT
                        KFW_import_windows_lsa();
#endif /* USE_MS2MIT */
                        KFW_AFS_renew_tokens_for_all_cells();
                    }
                    fSuccess = TRUE;
                }
                CloseServiceHandle (hService);
            }
            CloseServiceHandle (hManager);
        }
        break;

    case IDC_SERVICE_STOP:
        if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT |
                                        SC_MANAGER_ENUMERATE_SERVICE |
                                        SC_MANAGER_QUERY_LOCK_STATUS )) != NULL)
        {            
            if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), 
                                         SERVICE_QUERY_STATUS | SERVICE_STOP)) != NULL)
            {
                SERVICE_STATUS Status;
                TestAndDoUnMapShare();
                ControlService (hService, SERVICE_CONTROL_STOP, &Status);
                fSuccess = TRUE;
            }
            CloseServiceHandle (hService);
        }
        CloseServiceHandle (hManager);
        break;
    }

   if (fSuccess)
      {
      if (wCmd == IDC_SERVICE_STOP)
         SetWindowLong (hDlg, DWL_USER, TRUE);
      Advanced_OnServiceTimer (hDlg);
      Advanced_StartTimer (hDlg);
#ifdef USE_KFW
      if ( wCmd == IDC_SERVICE_START && KRB5_is_available() && KRB5_wait_for_service_start() )
          KRB5_AFS_renew_tokens_for_all_cells();
#endif /* USE_KFW */
      }
   else
      {
      if (!error)
         error = GetLastError();

      int ids;
      switch (wCmd)
         {
         case IDC_SERVICE_START:
            ids = IDS_SERVICE_FAIL_START;
            break;
         case IDC_SERVICE_STOP:
            ids = IDS_SERVICE_FAIL_STOP;
            break;
         default:
            ids = IDS_SERVICE_FAIL_CONFIG;
            break;
         }

      Message (MB_OK | MB_ICONHAND, IDS_SERVICE_ERROR, ids, TEXT("%08lX"), error);
      }
}


void Advanced_OnOpenCPL (HWND hDlg)
{
   WinExec ("afs_config.exe", SW_SHOW);
}


void Advanced_OnStartup (HWND hDlg)
{
   g.fStartup = IsDlgButtonChecked (hDlg, IDC_STARTUP);

   HKEY hk;
   if (RegCreateKey (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY), &hk) == 0)
      {
      DWORD dwSize = sizeof(g.fStartup);
      DWORD dwType = REG_DWORD;
      RegSetValueEx (hk, TEXT("ShowTrayIcon"), NULL, dwType, (PBYTE)&g.fStartup, dwSize);
      RegCloseKey (hk);
      }

   Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup);
}

