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
#include "svr_prop.h"
#include "svr_address.h"
#include "svr_general.h"
#include "problems.h"
#include "propcache.h"



/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Server_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK Server_Scout_DlgProc   (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_General_OnInitDialog (HWND hDlg, LPIDENT lpiServer);
void Server_General_OnEndTask_InitDialog (HWND hDlg, LPIDENT lpiServer, LPTASKPACKET ptp);
void Server_General_OnAuth (HWND hDlg, LPIDENT lpiServer, BOOL fEnableAuth);
void Server_General_OnChangeAddr (HWND hDlg, LPIDENT lpiServer);

void Server_Scout_OnInitDialog (HWND hDlg, LPIDENT lpiServer);
void Server_Scout_OnApply (HWND hDlg, LPIDENT lpiServer);
void Server_Scout_OnAggWarn (HWND hDlg, LPIDENT lpiServer);
void Server_Scout_OnSetWarn (HWND hDlg, LPIDENT lpiServer);
void Server_Scout_OnAutoRefresh (HWND hDlg, LPIDENT lpiServer);
void Server_Scout_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp);
void Server_Scout_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_ShowProperties (LPIDENT lpiServer, size_t nAlerts)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_PROP, lpiServer)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      TCHAR szName[ cchNAME ];
      lpiServer->GetServerName (szName);
      LPTSTR pszTitle = FormatString (IDS_SVR_PROP_TITLE, TEXT("%s"), szName);

      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      psh->fMadeCaption = TRUE;

      if ( (PropSheet_AddProblemsTab (psh, IDD_SVR_PROBLEMS, lpiServer, nAlerts)) &&
           (PropSheet_AddTab (psh, IDS_SVR_GENERAL_TAB, IDD_SVR_GENERAL, (DLGPROC)Server_General_DlgProc, (LPARAM)lpiServer, TRUE)) &&
           (PropSheet_AddTab (psh, IDS_SVR_SCOUT_TAB,   IDD_SVR_SCOUT,   (DLGPROC)Server_Scout_DlgProc,   (LPARAM)lpiServer, TRUE)) )
         {
         PropSheet_ShowModeless (psh);
         }
      }
}


BOOL CALLBACK Server_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpiServer = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcSVR_PROP, (LPIDENT)lp, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Server_General_OnInitDialog (hDlg, lpiServer);
         StartTask (taskSVR_PROP_INIT, hDlg, lpiServer);
         break;

      case WM_DESTROY:
         SetWindowLongPtr (hDlg, DWLP_USER, 0);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_PROP_INIT)
               Server_General_OnEndTask_InitDialog (hDlg, lpiServer, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               break;

            case IDC_SVR_AUTH_YES:
               Server_General_OnAuth (hDlg, lpiServer, TRUE);
               break;

            case IDC_SVR_AUTH_NO:
               Server_General_OnAuth (hDlg, lpiServer, FALSE);
               break;

            case IDC_SVR_CHANGEADDR:
               Server_General_OnChangeAddr (hDlg, lpiServer);
               break;
            }
         break;

      case WM_CTLCOLORLISTBOX:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_SVR_ADDRESSES))
            {
            SetBkColor ((HDC)wp, GetSysColor (COLOR_BTNFACE));
            return CreateSolidBrush (GetSysColor (COLOR_BTNFACE))?TRUE:FALSE;
            }
         break;
      }

   return FALSE;
}


void Server_General_OnInitDialog (HWND hDlg, LPIDENT lpiServer)
{
   TCHAR szText[ cchRESOURCE ];
   lpiServer->GetServerName (szText);
   SetDlgItemText (hDlg, IDC_SVR_NAME, szText);

   EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTH_YES), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTH_NO), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_CHANGEADDR), FALSE);
}


void Server_General_OnEndTask_InitDialog (HWND hDlg, LPIDENT lpiServer, LPTASKPACKET ptp)
{
   TCHAR szText[ cchRESOURCE ];
   if (!ptp->rc)
      {
      GetString (szText, IDS_UNKNOWN);
      SetDlgItemText (hDlg, IDC_SVR_NUMAGGREGATES, szText);
      SetDlgItemText (hDlg, IDC_SVR_CAPACITY, szText);
      SetDlgItemText (hDlg, IDC_SVR_ALLOCATION, szText);

      LB_StartChange (GetDlgItem (hDlg, IDC_SVR_ADDRESSES), TRUE);
      LB_AddItem (GetDlgItem (hDlg, IDC_SVR_ADDRESSES), szText, (LPARAM)-1);
      LB_EndChange (GetDlgItem (hDlg, IDC_SVR_ADDRESSES), (LPARAM)-1);

      lpiServer->GetServerName (szText);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_SERVER_STATUS, TEXT("%s"), szText);
      }
   else
      {
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTH_YES), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTH_NO), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_CHANGEADDR), TRUE);

      wsprintf (szText, TEXT("%lu"), TASKDATA(ptp)->nAggr);
      SetDlgItemText (hDlg, IDC_SVR_NUMAGGREGATES, szText);

      LPTSTR pszText = FormatString (IDS_SVR_CAPACITY, TEXT("%.1B"), (double)TASKDATA(ptp)->ckCapacity * 1024.0);
      SetDlgItemText (hDlg, IDC_SVR_CAPACITY, pszText);
      FreeString (pszText);

      DWORD dwPercentUsed = 100;
      if (TASKDATA(ptp)->ckCapacity)
         dwPercentUsed = (DWORD)( (double)100 * TASKDATA(ptp)->ckAllocation / TASKDATA(ptp)->ckCapacity );
      pszText = FormatString (IDS_SVR_ALLOCATION, TEXT("%.1B%lu"), (double)TASKDATA(ptp)->ckAllocation * 1024.0, dwPercentUsed);
      SetDlgItemText (hDlg, IDC_SVR_ALLOCATION, pszText);
      FreeString (pszText);

      Server_FillAddrList (hDlg, &TASKDATA(ptp)->ss);
      }
}



void Server_General_OnAuth (HWND hDlg, LPIDENT lpiServer, BOOL fEnable)
{
   if (!fEnable)
      {
      if (Message (MB_YESNO | MB_ICONASTERISK, IDS_WARN_TITLE, IDS_WARN_DISABLE_AUTH) != IDYES)
         return;
      }

   LPSVR_SETAUTH_PARAMS lpp = New (SVR_SETAUTH_PARAMS);
   lpp->lpiServer = lpiServer;
   lpp->fEnableAuth = fEnable;
   StartTask (taskSVR_SETAUTH, NULL, lpp);
}


void Server_General_OnChangeAddr (HWND hDlg, LPIDENT lpiServer)
{
   LPSVR_CHANGEADDR_PARAMS lpp = New (SVR_CHANGEADDR_PARAMS);
   memset (lpp, 0x00, sizeof(SVR_CHANGEADDR_PARAMS));
   lpp->lpiServer = lpiServer;

   if (ModalDialogParam (IDD_SVR_ADDRESS, hDlg, (DLGPROC)ChangeAddr_DlgProc, (LPARAM)lpp) == IDOK)
      {
      StartTask (taskSVR_CHANGEADDR, NULL, lpp);
      }
   else
      {
      Delete (lpp);
      }
}


BOOL CALLBACK Server_Scout_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_SCOUT, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpiServer = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         Server_Scout_OnInitDialog (hDlg, lpiServer);
         StartTask (taskSVR_SCOUT_INIT, hDlg, lpiServer);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_SCOUT_INIT)
               Server_Scout_OnEndTask_InitDialog (hDlg, ptp);
            else if (ptp->idTask == taskSVR_SCOUT_APPLY)
               Server_Scout_OnEndTask_Apply (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               break;

            case IDAPPLY:
               Server_Scout_OnApply (hDlg, lpiServer);
               break;

            case IDC_SVR_WARN_AGGFULL:
               Server_Scout_OnAggWarn (hDlg, lpiServer);
               PropSheetChanged (hDlg);
               break;

            case IDC_SVR_WARN_SETFULL:
               Server_Scout_OnSetWarn (hDlg, lpiServer);
               PropSheetChanged (hDlg);
               break;

            case IDC_SVR_WARN_AGGALLOC:
            case IDC_SVR_WARN_SVCSTOP:
            case IDC_SVR_WARN_TIMEOUT:
            case IDC_SVR_WARN_SETNOVLDB:
            case IDC_SVR_WARN_SETNOSERV:
            case IDC_SVR_WARN_AGGFULL_PERCENT:
            case IDC_SVR_WARN_SETFULL_PERCENT:
            case IDC_SVR_AUTOREFRESH_MINUTES:
               PropSheetChanged (hDlg);
               break;

            case IDC_SVR_AUTOREFRESH:
               Server_Scout_OnAutoRefresh (hDlg, lpiServer);
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Server_Scout_OnInitDialog (HWND hDlg, LPIDENT lpiServer)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL_PERCENT), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL_PERCENT), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGALLOC), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SVCSTOP), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_TIMEOUT), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETNOVLDB), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETNOSERV), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGNOSERV), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH_MINUTES), FALSE);
}


void Server_Scout_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   if (!ptp->rc)
      {
      TCHAR szText[ cchNAME ];
      lpiServer->GetServerName (szText);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_SERVER_STATUS, TEXT("%s"), szText);
      }
   else
      {
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL_PERCENT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL_PERCENT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGALLOC), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SVCSTOP), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_TIMEOUT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETNOVLDB), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETNOSERV), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGNOSERV), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH_MINUTES), TRUE);

      CheckDlgButton (hDlg, IDC_SVR_WARN_AGGFULL, (TASKDATA(ptp)->lpsp->perWarnAggFull != 0));
      CheckDlgButton (hDlg, IDC_SVR_WARN_SETFULL, (TASKDATA(ptp)->lpsp->perWarnSetFull != 0));
      CheckDlgButton (hDlg, IDC_SVR_WARN_AGGALLOC, TASKDATA(ptp)->lpsp->fWarnAggAlloc);
      CheckDlgButton (hDlg, IDC_SVR_WARN_SVCSTOP, TASKDATA(ptp)->lpsp->fWarnSvcStop);
      CheckDlgButton (hDlg, IDC_SVR_WARN_TIMEOUT, TASKDATA(ptp)->lpsp->fWarnSvrTimeout);
      CheckDlgButton (hDlg, IDC_SVR_WARN_SETNOVLDB, TASKDATA(ptp)->lpsp->fWarnSetNoVLDB);
      CheckDlgButton (hDlg, IDC_SVR_WARN_SETNOSERV, TASKDATA(ptp)->lpsp->fWarnSetNoServ);
      CheckDlgButton (hDlg, IDC_SVR_WARN_AGGNOSERV, TASKDATA(ptp)->lpsp->fWarnAggNoServ);

      CheckDlgButton (hDlg, IDC_SVR_AUTOREFRESH, (TASKDATA(ptp)->lpsp->oa.cTickRefresh != 0));

      CreateSpinner (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH_MINUTES),
                     10, FALSE,  // base, signed
                     1,
                     TASKDATA(ptp)->lpsp->oa.cTickRefresh / cmsec1MINUTE,
                     60L * 24L);  // maximum refresh rate of one day

      Server_Scout_OnAutoRefresh (hDlg, lpiServer);


      CreateSpinner (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL_PERCENT),
                     10, FALSE,  // base, signed
                     1,
                     (TASKDATA(ptp)->lpsp->perWarnAggFull == 0) ? perDEFAULT_AGGFULL_WARNING : TASKDATA(ptp)->lpsp->perWarnAggFull,
                     100L);

      Server_Scout_OnAggWarn (hDlg, lpiServer);

      CreateSpinner (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL_PERCENT),
                     10, FALSE,  // base, signed
                     1,
                     (TASKDATA(ptp)->lpsp->perWarnSetFull == 0) ? perDEFAULT_SETFULL_WARNING : TASKDATA(ptp)->lpsp->perWarnSetFull,
                     100L);

      Server_Scout_OnSetWarn (hDlg, lpiServer);
      }
}


void Server_Scout_OnApply (HWND hDlg, LPIDENT lpiServer)
{
   LPSVR_SCOUT_APPLY_PACKET lpp;

   if ((lpp = New (SVR_SCOUT_APPLY_PACKET)) != NULL)
      {
      lpp->lpiServer = lpiServer;

      lpp->fIDC_SVR_WARN_AGGFULL = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_AGGFULL);
      lpp->wIDC_SVR_WARN_AGGFULL_PERCENT = (short)SP_GetPos (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL_PERCENT));

      lpp->fIDC_SVR_WARN_SETFULL = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_SETFULL);
      lpp->wIDC_SVR_WARN_SETFULL_PERCENT = (short)SP_GetPos (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL_PERCENT));

      lpp->fIDC_SVR_WARN_AGGALLOC = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_AGGALLOC);
      lpp->fIDC_SVR_WARN_SVCSTOP = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_SVCSTOP);
      lpp->fIDC_SVR_WARN_TIMEOUT = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_TIMEOUT);
      lpp->fIDC_SVR_WARN_SETNOVLDB = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_SETNOVLDB);
      lpp->fIDC_SVR_WARN_SETNOSERV = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_SETNOSERV);
      lpp->fIDC_SVR_WARN_AGGNOSERV = IsDlgButtonChecked (hDlg, IDC_SVR_WARN_AGGNOSERV);

      lpp->fIDC_SVR_AUTOREFRESH = IsDlgButtonChecked (hDlg, IDC_SVR_AUTOREFRESH);
      lpp->dwIDC_SVR_AUTOREFRESH_MINUTES = SP_GetPos (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH_MINUTES));

      StartTask (taskSVR_SCOUT_APPLY, hDlg, lpp);
      }
}


void Server_Scout_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      LPIDENT lpiServer = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

      TCHAR szText[ cchNAME ];
      lpiServer->GetServerName (szText);
      ErrorDialog (ptp->status, IDS_ERROR_CHANGE_SERVER_STATUS, TEXT("%s"), szText);
      }
}


void Server_Scout_OnAggWarn (HWND hDlg, LPIDENT lpiServer)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_AGGFULL_PERCENT),
                 IsDlgButtonChecked (hDlg, IDC_SVR_WARN_AGGFULL));
}


void Server_Scout_OnSetWarn (HWND hDlg, LPIDENT lpiServer)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_WARN_SETFULL_PERCENT),
                 IsDlgButtonChecked (hDlg, IDC_SVR_WARN_SETFULL));
}


void Server_Scout_OnAutoRefresh (HWND hDlg, LPIDENT lpiServer)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_AUTOREFRESH_MINUTES),
                 IsDlgButtonChecked (hDlg, IDC_SVR_AUTOREFRESH));
}

