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
#include "agg_prop.h"
#include "agg_general.h"
#include "svr_general.h"
#include "propcache.h"
#include "problems.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Aggregates_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Aggregates_General_OnInitDialog (HWND hDlg);
void Aggregates_General_OnApply (HWND hDlg);
void Aggregates_General_OnWarnings (HWND hDlg);
void Aggregates_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp);
void Aggregates_General_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Aggregates_ShowProperties (LPIDENT lpiAggregate, size_t nAlerts, BOOL fJumpToThreshold, HWND hParentModal)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcAGG_PROP, lpiAggregate)) != NULL)
      {
      SetFocus (hCurrent);

      if (fJumpToThreshold)
         {
         HWND hTab;
         if ((hTab = GetDlgItem (hCurrent, IDC_PROPSHEET_TABCTRL)) != NULL)
            {
            int nTabs = TabCtrl_GetItemCount (hTab);
            TabCtrl_SetCurSel (hTab, nTabs-1);

            NMHDR nm;
            nm.hwndFrom = hTab;
            nm.idFrom = IDC_PROPSHEET_TABCTRL;
            nm.code = TCN_SELCHANGE;
            SendMessage (hCurrent, WM_NOTIFY, 0, (LPARAM)&nm);
            }
         }
      }
   else
      {
      TCHAR szSvrName[ cchNAME ];
      TCHAR szAggName[ cchNAME ];
      lpiAggregate->GetServerName (szSvrName);
      lpiAggregate->GetAggregateName (szAggName);
      LPTSTR pszTitle = FormatString (IDS_AGG_PROP_TITLE, TEXT("%s%s"), szSvrName, szAggName);

      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      psh->fMadeCaption = TRUE;
      psh->sh.hwndParent = hParentModal;

      if (PropSheet_AddProblemsTab (psh, IDD_AGG_PROBLEMS, lpiAggregate, nAlerts) &&
          PropSheet_AddTab (psh, IDS_AGG_GENERAL_TAB, IDD_AGG_GENERAL, (DLGPROC)Aggregates_General_DlgProc, (LPARAM)lpiAggregate, TRUE, FALSE))
         {
         if (fJumpToThreshold)
            psh->sh.nStartPage = psh->sh.nPages-1;

         if (hParentModal)
            PropSheet_ShowModal (psh, PumpMessage);
         else
            PropSheet_ShowModeless (psh);
         }
      }
}


BOOL CALLBACK Aggregates_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_AGG_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcAGG_PROP, (LPIDENT)lp, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Aggregates_General_OnInitDialog (hDlg);
         StartTask (taskAGG_PROP_INIT, hDlg, lpi);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskAGG_PROP_INIT)
               Aggregates_General_OnEndTask_InitDialog (hDlg, ptp);
            else if (ptp->idTask == taskAGG_PROP_APPLY)
               Aggregates_General_OnEndTask_Apply (hDlg, ptp);
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
               Aggregates_General_OnApply (hDlg);
               break;

            case IDC_AGG_WARNALLOC:
               PropSheetChanged (hDlg);
               break;

            case IDC_AGG_WARN:
            case IDC_AGG_WARN_AGGFULL:
            case IDC_AGG_WARN_AGGFULL_DEF:
               Aggregates_General_OnWarnings (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_AGG_WARN_AGGFULL_PERCENT:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Aggregates_General_OnInitDialog (HWND hDlg)
{
   LPIDENT lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);

   TCHAR szSvrName[ cchNAME ];
   TCHAR szAggName[ cchNAME ];
   lpi->GetServerName (szSvrName);
   lpi->GetAggregateName (szAggName);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_AGG_NAME, szText, cchRESOURCE);
   LPTSTR pszText = FormatString (szText, TEXT("%s%s"), szSvrName, szAggName);
   SetDlgItemText (hDlg, IDC_AGG_NAME, pszText);
   FreeString (pszText);

   EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARNALLOC), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DEF), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT), FALSE);
}


void Aggregates_General_OnApply (HWND hDlg)
{
   LPAGG_PROP_APPLY_PACKET lpp;
   if ((lpp = New (AGG_PROP_APPLY_PACKET)) != NULL)
      {
      lpp->lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);
      lpp->fIDC_AGG_WARNALLOC = IsDlgButtonChecked (hDlg, IDC_AGG_WARNALLOC);
      lpp->fIDC_AGG_WARN = IsDlgButtonChecked (hDlg, IDC_AGG_WARN);
      lpp->fIDC_AGG_WARN_AGGFULL_DEF = IsDlgButtonChecked (hDlg, IDC_AGG_WARN_AGGFULL_DEF);
      lpp->wIDC_AGG_WARN_AGGFULL_PERCENT = (short)SP_GetPos (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT));

      StartTask (taskAGG_PROP_APPLY, hDlg, lpp);
      }
}


void Aggregates_General_OnWarnings (HWND hDlg)
{
   if (!IsDlgButtonChecked (hDlg, IDC_AGG_WARN))
      {
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DEF), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DESC), FALSE);
      }
   else if (IsDlgButtonChecked (hDlg, IDC_AGG_WARN_AGGFULL))
      {
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DEF), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DESC), TRUE);
      }
   else // (IsDlgButtonChecked (hDlg, IDC_AGG_WARN_AGGFULL_DEF))
      {
      CheckDlgButton (hDlg, IDC_AGG_WARN_AGGFULL_DEF, TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DEF), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_DESC), FALSE);
      }
}


void Aggregates_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);

   if (!ptp->rc)
      {
      TCHAR szUnknown[ cchRESOURCE ];
      GetString (szUnknown, IDS_UNKNOWN);
      SetDlgItemText (hDlg, IDC_AGG_ID,       szUnknown);
      SetDlgItemText (hDlg, IDC_AGG_DEVICE,   szUnknown);
      SetDlgItemText (hDlg, IDC_AGG_FILESETS, szUnknown);
      SetDlgItemText (hDlg, IDC_AGG_USAGE,    szUnknown);

      TCHAR szSvrName[ cchNAME ];
      TCHAR szAggName[ cchNAME ];
      lpi->GetServerName (szSvrName);
      lpi->GetAggregateName (szAggName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_AGGREGATE_STATUS, TEXT("%s%s"), szSvrName, szAggName);
      }
   else
      {
      TCHAR szText[ cchRESOURCE ];
      wsprintf (szText, TEXT("%lu"), TASKDATA(ptp)->as.dwID);
      SetDlgItemText (hDlg, IDC_AGG_ID, szText);

      SetDlgItemText (hDlg, IDC_AGG_DEVICE, TASKDATA(ptp)->pszText1);

      LPTSTR pszText = FormatString (IDS_AGG_FILESETS, TEXT("%lu%.1B"), TASKDATA(ptp)->nFilesets, (double)cb1KB * (double)(TASKDATA(ptp)->as.ckStorageAllocated));
      SetDlgItemText (hDlg, IDC_AGG_FILESETS, pszText);
      FreeString (pszText);

      double dUsed  = 1024.0 * (TASKDATA(ptp)->as.ckStorageTotal - TASKDATA(ptp)->as.ckStorageFree);
      double dTotal = 1024.0 * TASKDATA(ptp)->as.ckStorageTotal;
      DWORD dwPer   = 100;
      if (TASKDATA(ptp)->as.ckStorageTotal != 0)
         {
         dwPer = (DWORD)( 100.0 * (TASKDATA(ptp)->as.ckStorageTotal - TASKDATA(ptp)->as.ckStorageFree) / TASKDATA(ptp)->as.ckStorageTotal );
         dwPer = limit( 0, dwPer, 100 );
         }

      LPTSTR pszUsage = FormatString (IDS_USAGE_AGGREGATE, TEXT("%.1B%.1B%lu"), dUsed, dTotal, dwPer);
      SetDlgItemText (hDlg, IDC_AGG_USAGE, pszUsage);
      FreeString (pszUsage);

      SendDlgItemMessage (hDlg, IDC_AGG_USAGEBAR, PBM_SETRANGE, 0, MAKELPARAM(0,100));
      SendDlgItemMessage (hDlg, IDC_AGG_USAGEBAR, PBM_SETPOS, (WPARAM)dwPer, 0);

      if (TASKDATA(ptp)->lpsp->fWarnAggAlloc)
         {
         EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARNALLOC), TRUE);
         CheckDlgButton (hDlg, IDC_AGG_WARNALLOC, TASKDATA(ptp)->lpap->fWarnAggAlloc);
         }

      EnableWindow (GetDlgItem (hDlg, IDC_AGG_WARN), TRUE);

      if (TASKDATA(ptp)->lpsp->perWarnAggFull == 0)
         {
         GetString (szText, IDS_AGGFULL_WARN_OFF);
         SetDlgItemText (hDlg, IDC_AGG_WARN_AGGFULL_DEF, szText);
         }
      else
         {
         pszText = FormatString (IDS_AGGFULL_WARN_ON, TEXT("%lu"), TASKDATA(ptp)->lpsp->perWarnAggFull);
         SetDlgItemText (hDlg, IDC_AGG_WARN_AGGFULL_DEF, pszText);
         FreeString (pszText);
         }

      CheckDlgButton (hDlg, IDC_AGG_WARN, (TASKDATA(ptp)->lpap->perWarnAggFull != 0));
      if (TASKDATA(ptp)->lpap->perWarnAggFull != 0)
         {
         if (TASKDATA(ptp)->lpap->perWarnAggFull != -1)
            CheckDlgButton (hDlg, IDC_AGG_WARN_AGGFULL, TRUE);
         else
            CheckDlgButton (hDlg, IDC_AGG_WARN_AGGFULL_DEF, TRUE);
         }

      CreateSpinner (GetDlgItem (hDlg, IDC_AGG_WARN_AGGFULL_PERCENT),
                     10, FALSE,   // base, signed
                     1,
                     (TASKDATA(ptp)->lpap->perWarnAggFull == 0 || TASKDATA(ptp)->lpap->perWarnAggFull == -1) ? perDEFAULT_AGGFULL_WARNING : TASKDATA(ptp)->lpap->perWarnAggFull,
                     100); // min, default, max

      Aggregates_General_OnWarnings (hDlg);
      }
}


void Aggregates_General_OnEndTask_Apply (HWND hDlg, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      LPIDENT lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);

      TCHAR szSvrName[ cchNAME ];
      TCHAR szAggName[ cchNAME ];
      lpi->GetServerName (szSvrName);
      lpi->GetAggregateName (szAggName);
      ErrorDialog (ptp->status, IDS_ERROR_CHANGE_AGGREGATE_STATUS, TEXT("%s%s"), szSvrName, szAggName);
      }
}

