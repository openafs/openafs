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
#include "set_prop.h"
#include "set_quota.h"
#include "svr_general.h"
#include "propcache.h"
#include "problems.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Filesets_General_OnInitDialog (HWND hDlg, LPIDENT lpiFileset);
void Filesets_General_OnApply (HWND hDlg, LPIDENT lpiFileset);
void Filesets_General_OnWarnings (HWND hDlg, LPIDENT lpiFileset);

void Filesets_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPIDENT lpi);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_ShowProperties (LPIDENT lpiFileset, size_t nAlerts, BOOL fJumpToThreshold)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSET_PROP, lpiFileset)) != NULL)
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
      TCHAR szCell[ cchNAME ];
      lpiFileset->GetCellName (szCell);

      TCHAR szFileset[ cchNAME ];
      lpiFileset->GetFilesetName (szFileset);

      LPTSTR pszTitle = FormatString (IDS_SET_PROP_TITLE, TEXT("%s"), szFileset);
      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      psh->fMadeCaption = TRUE;

      if (PropSheet_AddProblemsTab (psh, IDD_SET_PROBLEMS, lpiFileset, nAlerts) &&
          PropSheet_AddTab (psh, IDS_SET_GENERAL_TAB, IDD_SET_GENERAL, (DLGPROC)Filesets_General_DlgProc, (LPARAM)lpiFileset, TRUE))
         {
         if (fJumpToThreshold)
            psh->sh.nStartPage = psh->sh.nPages-1;
         PropSheet_ShowModeless (psh);
         }
      }
}


BOOL CALLBACK Filesets_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpi = (LPIDENT)GetWindowLong (hDlg, DWL_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcSET_PROP, (LPIDENT)lp, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Filesets_General_OnInitDialog (hDlg, lpi);
         StartTask (taskSET_PROP_INIT, hDlg, lpi);
         NotifyMe (WHEN_OBJECT_CHANGES, lpi, hDlg, 0);
         break;

      case WM_DESTROY:
         DontNotifyMeEver (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSET_PROP_INIT)
               Filesets_General_OnEndTask_InitDialog (hDlg, ptp, lpi);
            FreeTaskPacket (ptp);
            }
         break; 

      case WM_NOTIFY_FROM_DISPATCH:
         StartTask (taskSET_PROP_INIT, hDlg, lpi);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               break;

            case IDAPPLY:
               Filesets_General_OnApply (hDlg, lpi);
               break;

            case IDC_SET_WARN:
            case IDC_SET_WARN_SETFULL:
            case IDC_SET_WARN_SETFULL_DEF:
               Filesets_General_OnWarnings (hDlg, lpi);
               PropSheetChanged (hDlg);
               break;

            case IDC_SET_WARN_SETFULL_PERCENT:
               PropSheetChanged (hDlg);
               break;

            case IDC_SET_QUOTA:
               if (Filesets_SetQuota (lpi))
                  {
                  // this new task will block until the setquota req is done
                  StartTask (taskSET_PROP_INIT, hDlg, lpi);
                  }
               break;

            case IDC_SET_LOCK:
               StartTask (taskSET_LOCK, NULL, lpi);
               break;

            case IDC_SET_UNLOCK:
               StartTask (taskSET_UNLOCK, NULL, lpi);
               break;
            }
         break;
      }

   return FALSE;
}


void Filesets_General_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szText[ cchRESOURCE ];
   TCHAR szSvrName[ cchNAME ];
   TCHAR szAggName[ cchNAME ];
   TCHAR szSetName[ cchNAME ];
   VOLUMEID vidFileset;
   lpi->GetServerName (szSvrName);
   lpi->GetAggregateName (szAggName);
   lpi->GetFilesetName (szSetName);
   lpi->GetFilesetID (&vidFileset);

   GetDlgItemText (hDlg, IDC_SET_NAME, szText, cchRESOURCE);
   LPTSTR pszText = FormatString (szText, TEXT("%s%s%s"), szSvrName, szAggName, szSetName);
   SetDlgItemText (hDlg, IDC_SET_NAME, pszText);
   FreeString (pszText);

   pszText = FormatString (TEXT("%1"), TEXT("%lu"), vidFileset);
   SetDlgItemText (hDlg, IDC_SET_ID, pszText);
   FreeString (pszText);

   EnableWindow (GetDlgItem (hDlg, IDC_SET_LOCK), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_UNLOCK), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_QUOTA), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), FALSE);
}


void Filesets_General_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPIDENT lpi)
{
   LPTSTR pszText;

   TCHAR szUnknown[ cchRESOURCE ];
   GetString (szUnknown, IDS_UNKNOWN);

   if (!ptp->rc)
      {
      SetDlgItemText (hDlg, IDC_SET_CREATEDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_UPDATEDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_ACCESSDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_BACKUPDATE, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_USAGE, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_STATUS, szUnknown);
      SetDlgItemText (hDlg, IDC_SET_FILES, szUnknown);

      TCHAR szSvrName[ cchNAME ];
      TCHAR szAggName[ cchNAME ];
      TCHAR szSetName[ cchNAME ];
      lpi->GetServerName (szSvrName);
      lpi->GetAggregateName (szAggName);
      lpi->GetFilesetName (szSetName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szSvrName, szAggName, szSetName);
      }
   else
      {
      TCHAR szText[ cchRESOURCE ];

      EnableWindow (GetDlgItem (hDlg, IDC_SET_LOCK), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_UNLOCK), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_QUOTA), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), TRUE);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->fs.timeCreation))
         SetDlgItemText (hDlg, IDC_SET_CREATEDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SET_CREATEDATE, szText);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->fs.timeLastUpdate))
         SetDlgItemText (hDlg, IDC_SET_UPDATEDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SET_UPDATEDATE, szText);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->fs.timeLastAccess))
         SetDlgItemText (hDlg, IDC_SET_ACCESSDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SET_ACCESSDATE, szText);

      if (!FormatTime (szText, TEXT("%s"), &TASKDATA(ptp)->fs.timeLastBackup))
         SetDlgItemText (hDlg, IDC_SET_BACKUPDATE, szUnknown);
      else
         SetDlgItemText (hDlg, IDC_SET_BACKUPDATE, szText);

      wsprintf (szText, TEXT("%lu"), TASKDATA(ptp)->fs.nFiles);
      SetDlgItemText (hDlg, IDC_SET_FILES, szText);

      size_t nAlerts = Alert_GetCount (lpi);
      if (nAlerts == 1)
         {
         GetString (szText, IDS_SETSTATUS_1ALERT);
         SetDlgItemText (hDlg, IDC_SET_STATUS, szText);
         }
      else if (nAlerts > 1)
         {
         pszText = FormatString (IDS_SETSTATUS_2ALERT, TEXT("%lu"), nAlerts);
         SetDlgItemText (hDlg, IDC_SET_STATUS, pszText);
         FreeString (pszText);
         }
      else // (nAlerts == 0)
         {
         if (TASKDATA(ptp)->fs.State & fsBUSY)
            GetString (szText, IDS_SETSTATUS_BUSY);
         else if (TASKDATA(ptp)->fs.State & fsSALVAGE)
            GetString (szText, IDS_SETSTATUS_SALVAGE);
         else if (TASKDATA(ptp)->fs.State & fsMOVED)
            GetString (szText, IDS_SETSTATUS_MOVED);
         else if (TASKDATA(ptp)->fs.State & fsLOCKED)
            GetString (szText, IDS_SETSTATUS_LOCKED);
         else if (TASKDATA(ptp)->fs.State & fsNO_VOL)
            GetString (szText, IDS_SETSTATUS_NO_VOL);
         else
            GetString (szText, IDS_STATUS_NOALERTS);
         SetDlgItemText (hDlg, IDC_SET_STATUS, szText);
         }

      if (TASKDATA(ptp)->fs.Type == ftREADWRITE)
         {
         Filesets_DisplayQuota (hDlg, &TASKDATA(ptp)->fs);
         }
      else
         {
         if (TASKDATA(ptp)->fs.Type == ftREPLICA)
            GetString (szText, IDS_USAGE_REPLICA);
         else // (TASKDATA(ptp)->fs.Type == ftCLONE)
            GetString (szText, IDS_USAGE_CLONE);

         pszText = FormatString (szText, TEXT("%.1B"), (double)TASKDATA(ptp)->fs.ckUsed * cb1KB);
         SetDlgItemText (hDlg, IDC_SET_USAGE, pszText);
         FreeString (pszText);

         if (GetDlgItem (hDlg, IDC_SET_USAGEBAR))
            {
            RECT rUsagebar;
            GetRectInParent (GetDlgItem (hDlg, IDC_SET_USAGEBAR), &rUsagebar);

            HFONT hf = (HFONT)SendMessage (GetDlgItem (hDlg, IDC_SET_USAGEBAR), WM_GETFONT, 0, 0);
            DestroyWindow (GetDlgItem (hDlg, IDC_SET_USAGEBAR));

            if (TASKDATA(ptp)->fs.Type == ftREPLICA)
               GetString (szText, IDS_NO_QUOTA_REPLICA);
            else // (TASKDATA(ptp)->fs.Type == ftCLONE)
               GetString (szText, IDS_NO_QUOTA_CLONE);

            HWND hDesc = CreateWindow (TEXT("static"),
                          szText,
                          WS_CHILD | SS_SIMPLE,
                          rUsagebar.left, rUsagebar.top,
                          cxRECT(rUsagebar), cyRECT(rUsagebar),
                          hDlg,
                          (HMENU)-1,
                          THIS_HINST,
                          0);
            SendMessage (hDesc, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
            ShowWindow (hDesc, SW_SHOW);
            }

         EnableWindow (GetDlgItem (hDlg, IDC_SET_QUOTA), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), FALSE);
         }

      if (TASKDATA(ptp)->lpsp->perWarnSetFull == 0)
         {
         GetString (szText, IDS_SETFULL_WARN_OFF);
         SetDlgItemText (hDlg, IDC_SET_WARN_SETFULL_DEF, szText);
         }
      else
         {
         GetString (szText, IDS_SETFULL_WARN_ON);
         pszText = FormatString (szText, TEXT("%lu"), TASKDATA(ptp)->lpsp->perWarnSetFull);
         SetDlgItemText (hDlg, IDC_SET_WARN_SETFULL_DEF, pszText);
         FreeString (pszText);
         }

      CheckDlgButton (hDlg, IDC_SET_WARN, (TASKDATA(ptp)->lpfp->perWarnSetFull != 0));
      if (TASKDATA(ptp)->lpfp->perWarnSetFull != 0)
         {
         if (TASKDATA(ptp)->lpfp->perWarnSetFull != -1)
            CheckDlgButton (hDlg, IDC_SET_WARN_SETFULL, TRUE);
         else
            CheckDlgButton (hDlg, IDC_SET_WARN_SETFULL_DEF, TRUE);
         }

      CreateSpinner (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT),
                     10, FALSE,   // base, signed
                     1,
                     (TASKDATA(ptp)->lpfp->perWarnSetFull == 0 || TASKDATA(ptp)->lpfp->perWarnSetFull == -1) ? perDEFAULT_SETFULL_WARNING : TASKDATA(ptp)->lpfp->perWarnSetFull,
                     100); // min, default, max

      Filesets_General_OnWarnings (hDlg, lpi);
      }
}


void Filesets_General_OnApply (HWND hDlg, LPIDENT lpi)
{
   LPSET_PROP_APPLY_PARAMS lpp = New (SET_PROP_APPLY_PARAMS);

   lpp->lpi = lpi;
   lpp->fIDC_SET_WARN = IsDlgButtonChecked (hDlg, IDC_SET_WARN);
   lpp->fIDC_SET_WARN_SETFULL_DEF = IsDlgButtonChecked (hDlg, IDC_SET_WARN_SETFULL_DEF);
   lpp->wIDC_SET_WARN_SETFULL_PERCENT = (WORD)SP_GetPos (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT));

   StartTask (taskSET_PROP_APPLY, hDlg, lpp);
}


void Filesets_General_OnWarnings (HWND hDlg, LPIDENT lpi)
{
   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_SET_WARN)))
      {
      if (!IsDlgButtonChecked (hDlg, IDC_SET_WARN))
         {
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), FALSE);
         }
      else if (IsDlgButtonChecked (hDlg, IDC_SET_WARN_SETFULL))
         {
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), TRUE);
         }
      else // (IsDlgButtonChecked (hDlg, IDC_SET_WARN_SETFULL_DEF))
         {
         CheckDlgButton (hDlg, IDC_SET_WARN_SETFULL_DEF, TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DEF), TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL), TRUE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_PERCENT), FALSE);
         EnableWindow (GetDlgItem (hDlg, IDC_SET_WARN_SETFULL_DESC), FALSE);
         }
      }
}

