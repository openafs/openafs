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

#include "svrmgr.h"
#include "svc_prop.h"
#include "svc_general.h"
#include "svc_startstop.h"
#include "svc_viewlog.h"
#include "propcache.h"
#include "problems.h"



/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Services_General_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Services_General_OnApply (HWND hDlg, LPIDENT lpi);
void Services_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp);
void Services_General_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp);

BOOL CALLBACK Services_BOS_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Services_BOS_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Services_BOS_OnEnable (HWND hDlg, LPIDENT lpi);
void Services_BOS_OnApply (HWND hDlg, LPIDENT lpi);
void Services_BOS_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPIDENT lpi);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL PropSheet_AddBOSTab (LPPROPSHEET psh, LPIDENT lpiService)
{
   TCHAR szSvcName[ cchNAME ];
   lpiService->GetServiceName (szSvcName);

   if (lstrcmp (szSvcName, TEXT("BOS")))
      return TRUE;

   return PropSheet_AddTab (psh, IDS_SVC_BOS_TAB, IDD_SVC_BOS, (DLGPROC)Services_BOS_DlgProc, (LPARAM)lpiService, TRUE);
}


void Services_ShowProperties (LPIDENT lpiService, size_t nAlerts)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVC_PROP, lpiService)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      TCHAR szSvrName[ cchNAME ];
      TCHAR szSvcName[ cchNAME ];
      lpiService->GetServerName (szSvrName);
      lpiService->GetServiceName (szSvcName);
      LPTSTR pszTitle = FormatString (IDS_SVC_PROP_TITLE, TEXT("%s%s"), szSvrName, szSvcName);

      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      psh->fMadeCaption = TRUE;

      if (PropSheet_AddProblemsTab (psh, IDD_SVC_PROBLEMS, lpiService, nAlerts) &&
          PropSheet_AddTab (psh, IDS_SVC_GENERAL_TAB, IDD_SVC_GENERAL, (DLGPROC)Services_General_DlgProc, (LPARAM)lpiService, TRUE) &&
          PropSheet_AddBOSTab (psh, lpiService))
         {
         PropSheet_ShowModeless (psh);
         }
      }
}


BOOL CALLBACK Services_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcSVC_PROP, (LPIDENT)lp, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         NotifyMe (WHEN_OBJECT_CHANGES, lpi, hDlg, 0);
         Services_General_OnInitDialog (hDlg, lpi);
         StartTask (taskSVC_PROP_INIT, hDlg, lpi);
         break;

      case WM_DESTROY:
         DontNotifyMeEver (hDlg);
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         if (((LPNOTIFYSTRUCT)lp)->evt == evtRefreshStatusEnd)
            {
            if (((LPNOTIFYSTRUCT)lp)->Params.lpi1 == lpi)
               {
               StartTask (taskSVC_PROP_INIT, hDlg, lpi);
               }
            }
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVC_PROP_INIT)
               Services_General_OnEndTask_InitDialog (hDlg, ptp);
            else if (ptp->idTask == taskSVC_PROP_APPLY)
               Services_General_OnEndTask_Apply (hDlg, ptp);
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
               Services_General_OnApply (hDlg, lpi);
               break;

            case IDC_SVC_WARNSTOP:
               PropSheetChanged (hDlg);
               break;

            case IDC_SVC_START:
               TCHAR szSvcName[ cchNAME ];
               lpi->GetServiceName (szSvcName);
               if (!lstrcmpi (szSvcName, TEXT("BOS")))
                  Services_Restart (lpi);
               else
                  Services_Start (lpi);
               break;

            case IDC_SVC_STOP:
               Services_Stop (lpi);
               break;

            case IDC_SVC_VIEWLOG:
               Services_ShowServiceLog (lpi);
               break;
            }
         break;
      }

   return FALSE;
}


void Services_General_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szText[ cchRESOURCE ];
   LPTSTR pszText;

   TCHAR szSvrName[ cchNAME ];
   TCHAR szSvcName[ cchNAME ];
   lpi->GetServerName (szSvrName);
   lpi->GetServiceName (szSvcName);

   GetDlgItemText (hDlg, IDC_SVC_NAME, szText, cchRESOURCE);
   pszText = FormatString (szText, TEXT("%s%s"), szSvrName, szSvcName);
   SetDlgItemText (hDlg, IDC_SVC_NAME, pszText);
   FreeString (pszText);

   EnableWindow (GetDlgItem (hDlg, IDC_SVC_WARNSTOP), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVC_VIEWLOG), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVC_STOP),  FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SVC_START), FALSE);

   if (!lstrcmpi (szSvcName, TEXT("BOS")))
      {
      RECT rStart;
      RECT rStop;
      GetRectInParent (GetDlgItem (hDlg, IDC_SVC_START), &rStart);
      GetRectInParent (GetDlgItem (hDlg, IDC_SVC_STOP),  &rStop);
      DestroyWindow (GetDlgItem (hDlg, IDC_SVC_STOP));
      SetWindowPos (GetDlgItem (hDlg, IDC_SVC_START), NULL,
                    0, 0, rStop.right-rStart.left, rStart.bottom-rStart.top,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
      GetString (szText, IDS_SVC_RESTART_BUTTON);
      SetDlgItemText (hDlg, IDC_SVC_START, szText);
      }
}


void Services_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   TCHAR szUnknown[ cchRESOURCE ];
   GetString (szUnknown, IDS_UNKNOWN);

   if (!ptp->rc)
      {
      SetDlgItemText (hDlg, IDC_SVC_STARTDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SVC_STOPDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SVC_LASTERROR, szUnknown);
      GetString (szUnknown, IDS_SERVICESTATUS_UNKNOWN);
      SetDlgItemText (hDlg, IDC_SVC_STATUS, szUnknown);
      SetDlgItemText (hDlg, IDC_SVC_PARAMS, szUnknown);
      SetDlgItemText (hDlg, IDC_SVC_NOTIFIER, szUnknown);

      TCHAR szSvrName[ cchNAME ];
      TCHAR szSvcName[ cchNAME ];
      lpi->GetServerName (szSvrName);
      lpi->GetServiceName (szSvcName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_SERVICE_STATUS, TEXT("%s%s"), szSvrName, szSvcName);
      }
   else
      {
      TCHAR szText[ cchRESOURCE ];

      EnableWindow (GetDlgItem (hDlg, IDC_SVC_WARNSTOP), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVC_VIEWLOG), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVC_STOP),  TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVC_START), TRUE);

      if (TASKDATA(ptp)->cs.type == SERVICETYPE_FS)
         GetString (szText, IDS_SERVICETYPE_FS_LONG);
      else if (TASKDATA(ptp)->cs.type == SERVICETYPE_CRON)
         GetString (szText, IDS_SERVICETYPE_CRON_LONG);
      else // (TASKDATA(ptp)->cs.type == SERVICETYPE_SIMPLE)
         GetString (szText, IDS_SERVICETYPE_SIMPLE_LONG);
      SetDlgItemText (hDlg, IDC_SVC_TYPE, szText);

      LPTSTR pszParams = CloneString (TASKDATA(ptp)->cs.szParams);
      LPTSTR pch;
      for (pch = pszParams; pch && *pch; ++pch)
         {
         if (*pch == TEXT('\r') || *pch == TEXT('\t') || *pch == TEXT('\n'))
            *pch = TEXT(' ');
         }
      SetDlgItemText (hDlg, IDC_SVC_PARAMS, pszParams ? pszParams : TEXT(""));
      FreeString (pszParams);

      LPTSTR pszNotifier = CloneString (TASKDATA(ptp)->cs.szNotifier);
      for (pch = pszNotifier; pch && *pch; ++pch)
         {
         if (*pch == TEXT('\r') || *pch == TEXT('\t') || *pch == TEXT('\n'))
            *pch = TEXT(' ');
         }
      GetString (szText, IDS_SVC_NONOTIFIER);
      SetDlgItemText (hDlg, IDC_SVC_NOTIFIER, (pszNotifier && *pszNotifier) ? pszNotifier : szText);
      FreeString (pszNotifier);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->cs.timeLastStart))
         SetDlgItemText (hDlg, IDC_SVC_STARTDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SVC_STARTDATE, szText);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->cs.timeLastStop))
         SetDlgItemText (hDlg, IDC_SVC_STOPDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SVC_STOPDATE, szText);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->cs.timeLastFail))
         SetDlgItemText (hDlg, IDC_SVC_LASTERROR, szUnknown);
      else
         {
         LPTSTR pszText = FormatString (IDS_SERVICE_LASTERRORDATE, TEXT("%s%ld"), szText, TASKDATA(ptp)->cs.dwErrLast);
         SetDlgItemText (hDlg, IDC_SVC_LASTERROR, pszText);
         FreeString (pszText);
         }

      if (TASKDATA(ptp)->cs.state == SERVICESTATE_RUNNING)
         {
         GetString (szText, IDS_SERVICESTATUS_RUNNING);
         }
      else if (TASKDATA(ptp)->cs.state == SERVICESTATE_STARTING)
         {
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_STOP), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_START), FALSE);
         GetString (szText, IDS_SERVICESTATUS_STARTING);
         }
      else if (TASKDATA(ptp)->cs.state == SERVICESTATE_STOPPING)
         {
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_STOP), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_START), FALSE);
         GetString (szText, IDS_SERVICESTATUS_STOPPING);
         }
      else // (TASKDATA(ptp)->cs.state == SERVICESTATE_STOPPED)
         {
         GetString (szText, IDS_SERVICESTATUS_STOPPED);
         }
      SetDlgItemText (hDlg, IDC_SVC_STATUS, szText);

      CheckDlgButton (hDlg, IDC_SVC_WARNSTOP, (TASKDATA(ptp)->lpsp->fWarnSvcStop && TASKDATA(ptp)->lpcp->fWarnSvcStop));
      }
}


void Services_General_OnApply (HWND hDlg, LPIDENT lpi)
{
   LPSVC_PROP_APPLY_PACKET lpp;
   if ((lpp = New (SVC_PROP_APPLY_PACKET)) != NULL)
      {
      lpp->lpi = lpi;
      lpp->fIDC_SVC_WARNSTOP = IsDlgButtonChecked (hDlg, IDC_SVC_WARNSTOP);

      StartTask (taskSVC_PROP_APPLY, hDlg, lpp);
      }
}


void Services_General_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      LPIDENT lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

      TCHAR szSvrName[ cchNAME ];
      TCHAR szSvcName[ cchNAME ];
      lpi->GetServerName (szSvrName);
      lpi->GetServiceName (szSvcName);
      ErrorDialog (ptp->status, IDS_ERROR_CHANGE_SERVICE_STATUS, TEXT("%s%s"), szSvrName, szSvcName);
      }
}


BOOL CALLBACK Services_BOS_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_BOS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         Services_BOS_OnInitDialog (hDlg, lpi);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVC_GETRESTARTTIMES)
               Services_BOS_OnEndTask_InitDialog (hDlg, ptp, lpi);
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
               Services_BOS_OnApply (hDlg, lpi);
               break;

            case IDC_BOS_GENRES:
            case IDC_BOS_BINRES:
               Services_BOS_OnEnable (hDlg, lpi);
               PropSheetChanged (hDlg);
               break;

            case IDC_BOS_GENRES_DATE:
            case IDC_BOS_GENRES_TIME:
            case IDC_BOS_BINRES_DATE:
            case IDC_BOS_BINRES_TIME:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Services_BOS_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szText[ cchRESOURCE ];
   LPTSTR pszText;

   TCHAR szSvrName[ cchNAME ];
   TCHAR szSvcName[ cchNAME ];
   lpi->GetServerName (szSvrName);
   lpi->GetServiceName (szSvcName);

   GetDlgItemText (hDlg, IDC_SVC_NAME, szText, cchRESOURCE);
   pszText = FormatString (szText, TEXT("%s%s"), szSvrName, szSvcName);
   SetDlgItemText (hDlg, IDC_SVC_NAME, pszText);
   FreeString (pszText);

   HWND hDay = GetDlgItem (hDlg, IDC_BOS_GENRES_DATE);
   CB_StartChange (hDay, TRUE);
   CB_AddItem (hDay, IDS_RECUR_DAILY,    (WORD)-1);
   CB_AddItem (hDay, IDS_RECUR_SUNDAY,    0);
   CB_AddItem (hDay, IDS_RECUR_MONDAY,    1);
   CB_AddItem (hDay, IDS_RECUR_TUESDAY,   2);
   CB_AddItem (hDay, IDS_RECUR_WEDNESDAY, 3);
   CB_AddItem (hDay, IDS_RECUR_THURSDAY,  4);
   CB_AddItem (hDay, IDS_RECUR_FRIDAY,    5);
   CB_AddItem (hDay, IDS_RECUR_SATURDAY,  6);
   CB_EndChange (hDay, (WORD)-1);

   hDay = GetDlgItem (hDlg, IDC_BOS_BINRES_DATE);
   CB_StartChange (hDay, TRUE);
   CB_AddItem (hDay, IDS_RECUR_DAILY,    (WORD)-1);
   CB_AddItem (hDay, IDS_RECUR_SUNDAY,    0);
   CB_AddItem (hDay, IDS_RECUR_MONDAY,    1);
   CB_AddItem (hDay, IDS_RECUR_TUESDAY,   2);
   CB_AddItem (hDay, IDS_RECUR_WEDNESDAY, 3);
   CB_AddItem (hDay, IDS_RECUR_THURSDAY,  4);
   CB_AddItem (hDay, IDS_RECUR_FRIDAY,    5);
   CB_AddItem (hDay, IDS_RECUR_SATURDAY,  6);
   CB_EndChange (hDay, (WORD)-1);

   EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES), FALSE);
   Services_BOS_OnEnable (hDlg, lpi);

   LPSVC_RESTARTTIMES_PARAMS lpp = New (SVC_RESTARTTIMES_PARAMS);
   memset (lpp, 0x00, sizeof(SVC_RESTARTTIMES_PARAMS));
   lpp->lpi = lpi;
   StartTask (taskSVC_GETRESTARTTIMES, hDlg, lpp);
}


void Services_BOS_OnEnable (HWND hDlg, LPIDENT lpi)
{
   BOOL fEnable;

   fEnable = TRUE;
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_BOS_GENRES)))
      fEnable = FALSE;
   else if (!IsDlgButtonChecked (hDlg, IDC_BOS_GENRES))
      fEnable = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES_DESC1), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES_DATE),  fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES_DESC2), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES_TIME),  fEnable);

   fEnable = TRUE;
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_BOS_BINRES)))
      fEnable = FALSE;
   else if (!IsDlgButtonChecked (hDlg, IDC_BOS_BINRES))
      fEnable = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES_DESC1), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES_DATE),  fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES_DESC2), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES_TIME),  fEnable);
}


void Services_BOS_OnApply (HWND hDlg, LPIDENT lpi)
{
   LPSVC_RESTARTTIMES_PARAMS lpp = New (SVC_RESTARTTIMES_PARAMS);
   lpp->lpi = lpi;

   lpp->fGeneral = IsDlgButtonChecked (hDlg, IDC_BOS_GENRES);
   TI_GetTime (GetDlgItem (hDlg, IDC_BOS_GENRES_TIME), &lpp->stGeneral);
   lpp->stGeneral.wDayOfWeek = (WORD)CB_GetSelectedData (GetDlgItem (hDlg, IDC_BOS_GENRES_DATE));

   lpp->fNewBinary = IsDlgButtonChecked (hDlg, IDC_BOS_BINRES);
   TI_GetTime (GetDlgItem (hDlg, IDC_BOS_BINRES_TIME), &lpp->stNewBinary);
   lpp->stNewBinary.wDayOfWeek = (WORD)CB_GetSelectedData (GetDlgItem (hDlg, IDC_BOS_BINRES_DATE));

   StartTask (taskSVC_SETRESTARTTIMES, NULL, lpp);
}


void Services_BOS_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPIDENT lpi)
{
   LPSVC_RESTARTTIMES_PARAMS lpp = (LPSVC_RESTARTTIMES_PARAMS)(ptp->lpUser);

   if (ptp->rc)
      {
      EnableWindow (GetDlgItem (hDlg, IDC_BOS_GENRES), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_BOS_BINRES), TRUE);

      CheckDlgButton (hDlg, IDC_BOS_GENRES, lpp->fGeneral);
      if (!lpp->fGeneral)
         {
         lpp->stGeneral.wDay = 0;
         lpp->stGeneral.wHour = 5;
         lpp->stGeneral.wMinute = 0;
         }

      TI_SetTime (GetDlgItem (hDlg, IDC_BOS_GENRES_TIME), &lpp->stGeneral);
      CB_SetSelectedByData (GetDlgItem (hDlg, IDC_BOS_GENRES_DATE), (LPARAM)lpp->stGeneral.wDayOfWeek);

      CheckDlgButton (hDlg, IDC_BOS_BINRES, lpp->fNewBinary);
      if (!lpp->fNewBinary)
         {
         lpp->stNewBinary.wDay = 0;
         lpp->stNewBinary.wHour = 5;
         lpp->stNewBinary.wMinute = 0;
         }

      TI_SetTime (GetDlgItem (hDlg, IDC_BOS_BINRES_TIME), &lpp->stNewBinary);
      CB_SetSelectedByData (GetDlgItem (hDlg, IDC_BOS_BINRES_DATE), (LPARAM)lpp->stNewBinary.wDayOfWeek);

      Services_BOS_OnEnable (hDlg, lpi);
      }

   Delete (lpp);
}

