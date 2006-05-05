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
#include <rx/rxkad.h>
#include <afs/cm_config.h>
}

#include "afs_config.h"
#include "tab_general.h"
#include "tab_hosts.h"
#include "tab_advanced.h"

#include "drivemap.h"
#include <adssts.h>

/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct l
   {
   BOOL fWarnIfStopped;
   BOOL fWarnIfNotStopped;
   BOOL fRestartIfStopped;
   BOOL fServiceIsRunning;
   BOOL fStarting;
   HWND hStatus;
   } l;


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ID_TIMER                0

#define cmsecIDLE_REFRESH   10000
#define cmsecFAST_REFRESH    1000


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void GeneralTab_OnInitDialog (HWND hDlg);
void GeneralTab_OnTimer (HWND hDlg);
BOOL GeneralTab_OnApply (HWND hDlg, BOOL fForce, BOOL fComplainIfInvalid);
void GeneralTab_OnRefresh (HWND hDlg, BOOL fRequery);
void GeneralTab_OnStartStop (HWND hDlg, BOOL fStart);
void GeneralTab_OnConnect (HWND hDlg);
void GeneralTab_OnGateway (HWND hDlg);
void GeneralTab_OnCell (HWND hDlg);

void GeneralTab_DoStartStop (HWND hDlg, BOOL fStart, BOOL fRestart);
void GeneralTab_FixRefreshTimer (HWND hDlg, UINT cmsec = 0);
DWORD GeneralTab_GetDisplayState (HWND hDlg);
void GeneralTab_ShowCurrentState (HWND hDlg);
BOOL GeneralTab_AskIfStopped (HWND hDlg);

BOOL fIsCellInCellServDB (LPCTSTR pszCell);

BOOL CALLBACK Status_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Status_OnRefresh (HWND hDlg);

/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK GeneralTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Main_OnInitDialog (GetParent(hDlg));
         GeneralTab_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         GeneralTab_FixRefreshTimer (hDlg, 0);
         break;

      case WM_TIMER:
         GeneralTab_OnTimer (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               if (!GeneralTab_OnApply (hDlg, FALSE, TRUE))
                  SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
               else if (g.fIsWinNT && !GeneralTab_AskIfStopped (hDlg))
                  SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
               break;

            case IDC_REFRESH:
               GeneralTab_OnRefresh (hDlg, FALSE);
               break;

            case IDC_SERVICE_START:
               GeneralTab_OnStartStop (hDlg, TRUE);
               break;

            case IDC_SERVICE_STOP:
               GeneralTab_OnStartStop (hDlg, FALSE);
               break;

            case IDC_GATEWAY_CONN:
               GeneralTab_OnConnect (hDlg);
               break;

            case IDC_GATEWAY:
               GeneralTab_OnGateway (hDlg);
               break;

            case IDC_CELL:
               GeneralTab_OnCell (hDlg);
               break;

            case IDHELP:
               GeneralTab_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         if (g.fIsWinNT)
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_GENERAL_NT);
         else
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_GENERAL_95);
         break;
      }

   return FALSE;
}


void GeneralTab_OnInitDialog (HWND hDlg)
{
   DWORD CurrentState = Config_GetServiceState();
   BOOL fNeedFastRefresh = ((CurrentState == SERVICE_STOPPED) || (CurrentState == SERVICE_RUNNING)) ? FALSE : TRUE;
   GeneralTab_FixRefreshTimer (hDlg, ((fNeedFastRefresh) ? cmsecFAST_REFRESH : cmsecIDLE_REFRESH));
   GeneralTab_OnTimer (hDlg);
   GeneralTab_OnRefresh (hDlg, TRUE);
}


BOOL GeneralTab_VerifyCell (HWND hDlg, BOOL fComplainIfInvalid, LPCTSTR pszCell)
{
   TCHAR szNoCell[ cchRESOURCE ];
   GetString (szNoCell, IDS_CELL_UNKNOWN);

   TCHAR szCell[ cchRESOURCE ];
   if (pszCell)
      lstrcpy (szCell, pszCell);
   else
      GetDlgItemText (hDlg, IDC_CELL, szCell, cchRESOURCE);
   if ((!szCell[0]) || (!lstrcmpi (szNoCell, szCell)))
      {
      if (fComplainIfInvalid)
         {
         if (g.fIsWinNT)
            Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_NOCELL_DESC);
         else
            Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_BADGATEWAY_DESC);
         }
      return FALSE;
      }

   if (!fIsCellInCellServDB (szCell))
      {
      if (fComplainIfInvalid)
         {
         if (g.fIsWinNT)
            Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_BADCELL_DESC);
         else
            Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_BADGWCELL_DESC, TEXT("%s"), szCell);
         }
      return FALSE;
      }

   return TRUE;
}


BOOL GeneralTab_VerifyOK (HWND hDlg, BOOL fComplainIfInvalid)
{
   // If it's Windows 95, make sure there's a valid Gateway entry
   //
   if (!g.fIsWinNT)
      {
      TCHAR szGateway[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_GATEWAY, szGateway, cchRESOURCE);
      if (!szGateway[0])
         {
         if (fComplainIfInvalid)
            Message (MB_ICONASTERISK | MB_OK, IDS_NOGATEWAY_TITLE, IDS_NOGATEWAY_DESC);
         return FALSE;
         }
      }

   // Make sure the cell is in our CellServDB.
   //
   if (g.fIsWinNT)
      {
      if (!GeneralTab_VerifyCell (hDlg, fComplainIfInvalid, NULL))
         return FALSE;
      }

   return TRUE;
}

BOOL GeneralTab_OnApply (HWND hDlg, BOOL fForce, BOOL fComplainIfInvalid)
{
   if (!fForce)
      {
      // Don't try to do anything if we've already failed the apply
      if (GetWindowLongPtr (hDlg, DWLP_MSGRESULT))
         return FALSE;
      }

   // If the user has changed CellServDB, configuration parameters for
   // the driver or anything else, we want to commit those changes first.
   // We *won't* commit server prefs changes yet, because we haven't yet
   // checked to see if the service is running.
   //
   if (!HostsTab_CommitChanges (fForce))
      return FALSE;

   if (!AdvancedTab_CommitChanges (fForce))
      return FALSE;

   if (!GeneralTab_VerifyOK (hDlg, fComplainIfInvalid))
      return FALSE;

   TCHAR szText[ MAX_PATH ];

   if (g.fIsWinNT)
      {
      GetDlgItemText (hDlg, IDC_CELL, szText, MAX_PATH);
      if (lstrcmpi (szText, g.Configuration.szCell))
         {
         if (!Config_SetCellName (szText))
            return FALSE;
         lstrcpy (g.Configuration.szCell, szText);
         }
      }

   BOOL fLogonAuthent = IsDlgButtonChecked (hDlg, IDC_LOGON);
   if (fLogonAuthent != g.Configuration.fLogonAuthent)
      {
	   SetBitLogonOption(fLogonAuthent,LOGON_OPTION_INTEGRATED);
      g.Configuration.fLogonAuthent = fLogonAuthent;
      }

   Config_SetTrayIconFlag (IsDlgButtonChecked (hDlg, IDC_TRAYICON));

   if (g.fIsWinNT)
      {
      BOOL fBeGateway = IsDlgButtonChecked (hDlg, IDC_GATEWAY);
      if (fBeGateway != g.Configuration.fBeGateway)
         {
         if (!Config_SetGatewayFlag (fBeGateway))
            return FALSE;
         g.fNeedRestart = TRUE;
         g.Configuration.fBeGateway = fBeGateway;
         }
      }
   else // (!g.fIsWinNT)
      {
      GetDlgItemText (hDlg, IDC_GATEWAY, szText, MAX_PATH);
      if (lstrcmpi (szText, g.Configuration.szGateway))
         {
         TCHAR szNewCell[ MAX_PATH ];
         if (!Config_ContactGateway (szText, szNewCell))
            {
            Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_BADGATEWAY_DESC);
            return FALSE;
            }

         if (!GeneralTab_VerifyCell (hDlg, fComplainIfInvalid, szNewCell))
            return FALSE;

         if (!Config_SetGatewayName (szText))
            return FALSE;

         if (!Config_SetCellName (szNewCell))
            return FALSE;

         Config_FixGatewayDrives();

         SetDlgItemText (hDlg, IDC_CELL, szNewCell);
         lstrcpy (g.Configuration.szGateway, szText);
         lstrcpy (g.Configuration.szCell, szNewCell);

         GeneralTab_OnGateway (hDlg);
         }
      }

   return TRUE;
}


void GeneralTab_OnRefresh (HWND hDlg, BOOL fRequery)
{
   // If necessary, update any fields in g.Configuration that we care about
   //
   if (fRequery)
      {
      if (g.fIsWinNT)
         Config_GetGatewayFlag (&g.Configuration.fBeGateway);
      else
         Config_GetGatewayName (g.Configuration.szGateway);

      Config_GetCellName (g.Configuration.szCell);
      g.Configuration.fLogonAuthent=RWLogonOption(TRUE,LOGON_OPTION_INTEGRATED);
      Config_GetTrayIconFlag (&g.Configuration.fShowTrayIcon);

      if (!g.fIsWinNT)
         SetDlgItemText (hDlg, IDC_GATEWAY, g.Configuration.szGateway);
      else
         CheckDlgButton (hDlg, IDC_GATEWAY, g.Configuration.fBeGateway);

      SetDlgItemText (hDlg, IDC_CELL, g.Configuration.szCell);
      CheckDlgButton (hDlg, IDC_LOGON, g.Configuration.fLogonAuthent);
      CheckDlgButton (hDlg, IDC_TRAYICON, g.Configuration.fShowTrayIcon);
      }

   // Update our display of the service's status
   //
   DWORD CurrentState = Config_GetServiceState();
   BOOL fIfServiceStopped = !(g.fIsWinNT && !g.fIsAdmin);
   BOOL fIfServiceRunning = fIfServiceStopped && (CurrentState == SERVICE_RUNNING);

   GeneralTab_ShowCurrentState (hDlg);

   EnableWindow (GetDlgItem (hDlg, IDC_CELL), fIfServiceStopped && g.fIsWinNT);

   EnableWindow (GetDlgItem (hDlg, IDC_LOGON), fIfServiceStopped);
   EnableWindow (GetDlgItem (hDlg, IDC_GATEWAY), fIfServiceStopped);

   // Update our warning. Note that under WinNT, this tab doesn't have any
   // controls (other than Start Service) which disable just because the
   // service isn't running...so don't show that warning in that case.
   //
   TCHAR szText[ cchRESOURCE ];
   if ((!g.fIsWinNT) && (CurrentState != SERVICE_RUNNING))
      {
      GetString (szText, IDS_WARN_STOPPED);
      SetDlgItemText (hDlg, IDC_WARN, szText);
      ShowWindow (GetDlgItem (hDlg, IDC_WARN), SW_SHOW);
      }
   else if (g.fIsWinNT && !g.fIsAdmin)
      {
      GetString (szText, IDS_WARN_ADMIN);
      SetDlgItemText (hDlg, IDC_WARN, szText);
      ShowWindow (GetDlgItem (hDlg, IDC_WARN), SW_SHOW);
      }
   else // ((CurrentState == SERVICE_RUNNING) && (g.fIsAdmin))
      {
      ShowWindow (GetDlgItem (hDlg, IDC_WARN), SW_HIDE);
      }

   GeneralTab_OnGateway (hDlg);

   // If the service isn't running/stopped, we may need to complain
   //
   if ((CurrentState == SERVICE_RUNNING) && (l.fWarnIfNotStopped))
      {
      Message (MB_ICONHAND, GetErrorTitle(), IDS_SERVICE_FAIL_STOP, TEXT("%08lX"), ERROR_SERVICE_SPECIFIC_ERROR);
      }
   else if ((CurrentState == SERVICE_STOPPED) && (l.fWarnIfStopped))
      {
      Message (MB_ICONHAND, GetErrorTitle(), IDS_SERVICE_FAIL_START, TEXT("%08lX"), ERROR_SERVICE_SPECIFIC_ERROR);
      }

   if ((CurrentState == SERVICE_RUNNING) || (CurrentState == SERVICE_STOPPED))
      {
      BOOL fRestart = ((CurrentState == SERVICE_STOPPED) && (l.fRestartIfStopped));
      l.fWarnIfStopped = FALSE;
      l.fWarnIfNotStopped = FALSE;
      l.fRestartIfStopped = FALSE;
      l.fServiceIsRunning = (CurrentState == SERVICE_RUNNING);

      if (fRestart)
         {
         GeneralTab_DoStartStop (hDlg, TRUE, FALSE);
         }
      }
}


void GeneralTab_OnTimer (HWND hDlg)
{
   DWORD CurrentState = Config_GetServiceState();
   DWORD DisplayState = GeneralTab_GetDisplayState(hDlg);
   TestAndDoMapShare(CurrentState);		//Re map mounted drives if necessary

   BOOL fInEndState = ((CurrentState == SERVICE_RUNNING) || (CurrentState == SERVICE_STOPPED));
   if (fInEndState && l.hStatus)
      {
      if (IsWindow (l.hStatus))
         DestroyWindow (l.hStatus);
      l.hStatus = NULL;
      }
   else if (!fInEndState && !l.hStatus)
      {
      l.hStatus = ModelessDialog (IDD_STARTSTOP, GetParent (hDlg), (DLGPROC)Status_DlgProc);
      }

   if (CurrentState != DisplayState)
      {
      GeneralTab_OnRefresh (hDlg, FALSE);
      Main_RefreshAllTabs();

      if (l.hStatus && IsWindow (l.hStatus))
         PostMessage (l.hStatus, WM_COMMAND, IDINIT, 0);
      }

   BOOL fNeedFastRefresh = ((CurrentState == SERVICE_STOPPED) || (CurrentState == SERVICE_RUNNING)) ? FALSE : TRUE;
   BOOL fHaveFastRefresh = ((DisplayState == SERVICE_STOPPED) || (DisplayState == SERVICE_RUNNING)) ? FALSE : TRUE;

   if (fNeedFastRefresh != fHaveFastRefresh)
      {
      GeneralTab_FixRefreshTimer (hDlg, ((fNeedFastRefresh) ? cmsecFAST_REFRESH : cmsecIDLE_REFRESH));
      }
}


void GeneralTab_OnStartStop (HWND hDlg, BOOL fStart)
{
   BOOL fSuccess = FALSE;
   ULONG error = 0;

   // Don't let the user stop the service on a whim; warn him first
   //
   if (!fStart)
      {
      if (Message (MB_ICONEXCLAMATION | MB_OKCANCEL, GetCautionTitle(), IDS_STOP_DESC) != IDOK)
         return;
      }

   // To start the service, we'll need to successfully commit our new
   // configuration. To stop the service, we'll *try*, but it's not
   // fatal if something goes wrong.
   //
   if (!GeneralTab_OnApply (hDlg, TRUE, ((fStart) ? TRUE : FALSE)))
      {
      if (fStart)
         return;
      }

   // Okay, start the service
   //
   GeneralTab_DoStartStop (hDlg, fStart, FALSE);
}


void GeneralTab_OnConnect (HWND hDlg)
{
   if (!GeneralTab_OnApply (hDlg, TRUE, TRUE))
      return;
   GeneralTab_OnGateway (hDlg);
   GeneralTab_OnApply (hDlg, TRUE, TRUE);
}


void GeneralTab_OnGateway (HWND hDlg)
{
   if (!g.fIsWinNT)
      {
      TCHAR szGateway[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_GATEWAY, szGateway, cchRESOURCE);

      BOOL fEnable = TRUE;
      if (!szGateway[0])
         fEnable = FALSE;
      if (!lstrcmpi (szGateway, g.Configuration.szGateway))
         fEnable = FALSE;
      EnableWindow (GetDlgItem (hDlg, IDC_GATEWAY_CONN), fEnable);
      }
}


void GeneralTab_OnCell (HWND hDlg)
{
   if (g.fIsWinNT)
      {
      GeneralTab_ShowCurrentState (hDlg);
      }
}


void GeneralTab_FixRefreshTimer (HWND hDlg, UINT cmsec)
{
   static BOOL fTimerActive = FALSE;
   if (fTimerActive)
      {
      KillTimer (hDlg, ID_TIMER);
      fTimerActive = FALSE;
      }

   if (g.fIsWinNT && (cmsec != 0))
      {
      SetTimer (hDlg, ID_TIMER, cmsec, NULL);
      }
}


DWORD GeneralTab_GetDisplayState (HWND hDlg)
{
   TCHAR szText[ cchRESOURCE ];
   TCHAR szTextNow[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_STATUS, szTextNow, cchRESOURCE);

   GetString (szText, IDS_STATE_STOPPED);
   if (!lstrcmpi (szTextNow, szText))
      return SERVICE_STOPPED;

   GetString (szText, IDS_STATE_RUNNING);
   if (!lstrcmpi (szTextNow, szText))
      return SERVICE_RUNNING;

   GetString (szText, IDS_STATE_STARTING);
   if (!lstrcmpi (szTextNow, szText))
      return SERVICE_START_PENDING;

   GetString (szText, IDS_STATE_STOPPING);
   if (!lstrcmpi (szTextNow, szText))
      return SERVICE_STOP_PENDING;

   return 0;
}


void GeneralTab_ShowCurrentState (HWND hDlg)
{
   TCHAR szNoCell[ cchRESOURCE ];
   GetString (szNoCell, IDS_CELL_UNKNOWN);

   TCHAR szCell[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CELL, szCell, cchRESOURCE);

   BOOL fValidCell = TRUE;
   if (!szCell[0])
      fValidCell = FALSE;
   if (!lstrcmpi (szCell, szNoCell))
      fValidCell = FALSE;

   DWORD CurrentState = Config_GetServiceState();

   TCHAR szText[ cchRESOURCE ];
   switch (CurrentState)
      {
      case SERVICE_STOPPED:
         GetString (szText, (fValidCell) ? IDS_STATE_STOPPED : IDS_STOPPED_NOCELL);
         break;
      case SERVICE_RUNNING:
         GetString (szText, IDS_STATE_RUNNING);
         break;
      case SERVICE_START_PENDING:
         GetString (szText, IDS_STATE_STARTING);
         break;
      case SERVICE_STOP_PENDING:
         GetString (szText, IDS_STATE_STOPPING);
         break;
      default:
         GetString (szText, IDS_STATE_UNKNOWN);
         break;
      }
   SetDlgItemText (hDlg, IDC_STATUS, szText);

   // Enable or disable controls as necessary
   //
   BOOL fIfServiceStopped = !(g.fIsWinNT && !g.fIsAdmin);
   BOOL fIfServiceRunning = fIfServiceStopped && (CurrentState == SERVICE_RUNNING);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_START), ((CurrentState == SERVICE_STOPPED) && (g.fIsAdmin) && (fValidCell)));
   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_STOP),  ((CurrentState == SERVICE_RUNNING) && (g.fIsAdmin)));
}


BOOL GeneralTab_AskIfStopped (HWND hDlg)
{
   BOOL fStopService = FALSE;
   BOOL fStartService = FALSE;

   // If we changed things, ask if we should restart the service.
   // Otherwise, if it's stopped, ask the user if we should start the service.
   //
   DWORD CurrentState = Config_GetServiceState();
   if (g.fIsAdmin)
      {
      if ((CurrentState == SERVICE_RUNNING) && (g.fNeedRestart))
         {
         if (Message (MB_YESNO | MB_ICONQUESTION, IDS_RESTART_TITLE, IDS_RESTART_DESC) == IDYES)
            {
            fStopService = TRUE;
            fStartService = TRUE;
            }
         }
      if (CurrentState == SERVICE_STOPPED)
         {
         if (Message (MB_YESNO | MB_ICONQUESTION, GetCautionTitle(), IDS_OKSTOP_DESC) == IDYES)
            {
            fStartService = TRUE;
            }
         }
      }

   // If we need to, start or stop-n-restart the service
   //
   if (fStartService && fStopService)
      {
      GeneralTab_DoStartStop (hDlg, FALSE, TRUE); // Stop and restart the thing
      }
   else if (fStartService && !fStopService)
      {
      GeneralTab_DoStartStop (hDlg, TRUE, FALSE); // Just start it
      }

   if (fStartService)
      {
      while ( (l.fRestartIfStopped) ||
              (l.fWarnIfNotStopped) ||
              (l.fWarnIfStopped) )
         {
         MSG msg;
         if (!GetMessage (&msg, NULL, 0, 0))
            break;
         if (IsMemoryManagerMessage (&msg))
            continue;
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }

   if (fStartService && !l.fServiceIsRunning)
      return FALSE;

   return TRUE;
}


BOOL fIsCellInCellServDB (LPCTSTR pszCell)
{
   BOOL fFound = FALSE;
   CELLSERVDB CellServDB;

   if (CSDB_ReadFile (&CellServDB, NULL))
   {
       if (CSDB_FindCell (&CellServDB, pszCell))
           fFound = TRUE;
       CSDB_FreeFile (&CellServDB);
   }
#ifdef AFS_AFSDB_ENV
    if ( fFound == FALSE ) {
        int ttl;
        char cellname[128], i;

        /* we pray for all ascii cellnames */
        for ( i=0 ; pszCell[i] && i < (sizeof(cellname)-1) ; i++ )
            cellname[i] = pszCell[i];
        cellname[i] = '\0';

        fFound = !cm_SearchCellByDNS(cellname, NULL, &ttl, NULL, NULL);
    }
#endif
   return fFound;
}


void GeneralTab_DoStartStop (HWND hDlg, BOOL fStart, BOOL fRestart)
{
   BOOL fSuccess = FALSE;
   ULONG error = 0;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), SERVICE_ALL_ACCESS)) != NULL)
         {
         if (fStart)
            {
            g.fNeedRestart = FALSE;
            if (StartService (hService, 0, 0))
				TestAndDoMapShare(SERVICE_START_PENDING);
               fSuccess = TRUE;
            }
         else // (!fStart)
            {
            SERVICE_STATUS Status;
            if (ControlService (hService, SERVICE_CONTROL_STOP, &Status))
               fSuccess = TRUE;
			   if (g.Configuration.fLogonAuthent)
				   DoUnMapShare(FALSE);
            }

         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   if (fSuccess)
      {
      l.fWarnIfStopped = fStart;
      l.fWarnIfNotStopped = !fStart;
      l.fRestartIfStopped = fRestart && !fStart;
      l.fStarting = fStart;
      GeneralTab_OnTimer (hDlg);
      }
   else
      {
      l.fWarnIfStopped = FALSE;
      l.fWarnIfNotStopped = FALSE;
      l.fRestartIfStopped = FALSE;
      GeneralTab_OnTimer (hDlg);

      if (!error)
         error = GetLastError();
      Message (MB_OK | MB_ICONHAND, GetErrorTitle(), ((fStart) ? IDS_SERVICE_FAIL_START : IDS_SERVICE_FAIL_STOP), TEXT("%08lX"), error);
      }
}


BOOL CALLBACK Status_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         l.hStatus = hDlg;
         ShowWindow (l.hStatus, SW_SHOW);
         Status_OnRefresh (hDlg);
         break;

      case WM_DESTROY:
         l.hStatus = NULL;
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;

            case IDINIT:
               Status_OnRefresh (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Status_OnRefresh (HWND hDlg)
{
   DWORD CurrentState = Config_GetServiceState();
   if (CurrentState == SERVICE_START_PENDING)
      l.fStarting = TRUE;
   else if (CurrentState == SERVICE_STOP_PENDING)
      l.fStarting = FALSE;

   ShowWindow (GetDlgItem (l.hStatus, IDC_STARTING), l.fStarting);
   ShowWindow (GetDlgItem (l.hStatus, IDC_STOPPING), !l.fStarting);
}

