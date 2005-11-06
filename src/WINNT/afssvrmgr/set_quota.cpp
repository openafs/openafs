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
#include "set_quota.h"
#include "agg_prop.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_SetQuota_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Filesets_SetQuota_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Filesets_SetQuota_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSET_SETQUOTA_APPLY_PARAMS lpp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Filesets_SetQuota (LPIDENT lpiFileset, size_t ckQuota)
{
   if (ckQuota == 0)
      {
      if ((ckQuota = Filesets_PickQuota (lpiFileset)) == 0)
         return FALSE;
      }

   LPSET_SETQUOTA_APPLY_PARAMS lpp = New (SET_SETQUOTA_APPLY_PARAMS);
   lpp->lpiFileset = lpiFileset;
   lpp->ckQuota = ckQuota;
   StartTask (taskSET_SETQUOTA_APPLY, NULL, lpp);
   return TRUE;
}


size_t Filesets_PickQuota (LPIDENT lpiFileset)
{
   SET_SETQUOTA_APPLY_PARAMS ssp;
   memset (&ssp, 0x00, sizeof(ssp));
   ssp.lpiFileset = lpiFileset;

   INT_PTR rc = ModalDialogParam (IDD_SET_SETQUOTA, GetActiveWindow(), (DLGPROC)Filesets_SetQuota_DlgProc, (LPARAM)&ssp);
   if (rc != IDOK)
      return 0;
   else
      return ssp.ckQuota;
}


BOOL CALLBACK Filesets_SetQuota_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSET_SETQUOTA_APPLY_PARAMS lpp = NULL;
   if (msg == WM_INITDIALOG)
      lpp = (LPSET_SETQUOTA_APPLY_PARAMS)lp;

   if (AfsAppLib_HandleHelp (IDD_SET_SETQUOTA, hDlg, msg, wp, lp))
      return TRUE;

   if (lpp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Filesets_SetQuota_OnInitDialog (hDlg, lpp->lpiFileset);
            StartTask (taskSET_SETQUOTA_INIT, hDlg, lpp->lpiFileset);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSET_SETQUOTA_INIT)
                  Filesets_SetQuota_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_SET_QUOTA:
                  if (HIWORD(wp) == SPN_UPDATE)
                     {
                     lpp->ckQuota = SP_GetPos (GetDlgItem (hDlg, IDC_SET_QUOTA));
                     if (gr.cbQuotaUnits == cb1MB)
                        lpp->ckQuota *= ck1MB;
                     }
                  break;

               case IDC_SET_QUOTA_UNITS:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     {
                     lpp->ckQuota = SP_GetPos (GetDlgItem (hDlg, IDC_SET_QUOTA));
                     if (gr.cbQuotaUnits == cb1MB)
                        lpp->ckQuota *= ck1MB;

                     gr.cbQuotaUnits = (size_t)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS));
                     StartTask (taskSET_SETQUOTA_INIT, hDlg, lpp->lpiFileset);
                     }
                  break;

               case IDC_AGG_PROPERTIES:
                  size_t nAlerts;
                  nAlerts = Alert_GetCount (lpp->lpiFileset->GetAggregate());
                  Aggregates_ShowProperties (lpp->lpiFileset->GetAggregate(), nAlerts, TRUE, hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            lpp = NULL;
            break;
         }
      }

   return FALSE;
}


void Filesets_SetQuota_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   lpi->GetServerName (szServer);
   lpi->GetAggregateName (szAggregate);
   lpi->GetFilesetName (szFileset);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_SET_NAME, szText, cchRESOURCE);
   LPTSTR pszNew = FormatString (szText, TEXT("%s"), szFileset);
   SetDlgItemText (hDlg, IDC_SET_NAME, pszNew);
   FreeString (pszNew);

   GetDlgItemText (hDlg, IDC_SET_AGGREGATE, szText, cchRESOURCE);
   pszNew = FormatString (szText, TEXT("%s%s"), szServer, szAggregate);
   SetDlgItemText (hDlg, IDC_SET_AGGREGATE, pszNew);
   FreeString (pszNew);

   EnableWindow (GetDlgItem (hDlg, IDC_SET_QUOTA), FALSE);

   CB_StartChange (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), TRUE);
   CB_AddItem (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), IDS_QUOTAUNITS_KB, (LPARAM)cb1KB);
   CB_AddItem (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), IDS_QUOTAUNITS_MB, (LPARAM)cb1MB);
   CB_EndChange (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), (LPARAM)gr.cbQuotaUnits);
}


void Filesets_SetQuota_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSET_SETQUOTA_APPLY_PARAMS lpp)
{
   if (!ptp->rc)
      {
      TCHAR szSvrName[ cchNAME ];
      TCHAR szAggName[ cchNAME ];
      TCHAR szSetName[ cchNAME ];
      lpp->lpiFileset->GetServerName (szSvrName);
      lpp->lpiFileset->GetAggregateName (szAggName);
      lpp->lpiFileset->GetFilesetName (szSetName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szSvrName, szAggName, szSetName);
      EndDialog (hDlg, IDCANCEL);
      }
   else
      {
      BOOL fBeenHereBefore = fHasSpinner (GetDlgItem (hDlg, IDC_SET_QUOTA));

      size_t cMin = TASKDATA(ptp)->ckMin;
      size_t cMax = TASKDATA(ptp)->ckMax;
      size_t cNow = (fBeenHereBefore) ? lpp->ckQuota : TASKDATA(ptp)->fs.ckQuota;

      if (gr.cbQuotaUnits == cb1MB)
         {
         cMin /= ck1MB;
         cMax /= ck1MB;
         cNow /= ck1MB;
         }

      EnableWindow (GetDlgItem (hDlg, IDC_SET_QUOTA), TRUE);

      if (!fBeenHereBefore)
         CreateSpinner (GetDlgItem (hDlg, IDC_SET_QUOTA), 10, FALSE, (int)cMin, (int)cNow, (int)cMax);
      else
         {
         SP_SetRange (GetDlgItem (hDlg, IDC_SET_QUOTA), cMin, cMax);
         SP_SetPos (GetDlgItem (hDlg, IDC_SET_QUOTA), cNow);
         }
      Filesets_DisplayQuota (hDlg, &TASKDATA(ptp)->fs);
      }
}


void Filesets_DisplayQuota (HWND hDlg, LPFILESETSTATUS lpfs)
{
   double dUsed  = 1024.0 * lpfs->ckUsed;
   double dTotal = 1024.0 * lpfs->ckQuota;
   DWORD dwPer = 100;
   if (lpfs->ckQuota != 0)
      {
      dwPer = (DWORD)( 100.0 * lpfs->ckUsed / lpfs->ckQuota );
      dwPer = limit( 0, dwPer, 100 );
      }

   LPTSTR pszUsage = FormatString (IDS_USAGE_FILESET, TEXT("%.1B%.1B%lu"), dUsed, dTotal, dwPer);
   SetDlgItemText (hDlg, IDC_SET_USAGE, pszUsage);
   FreeString (pszUsage);

   SendDlgItemMessage (hDlg, IDC_SET_USAGEBAR, PBM_SETRANGE, 0, MAKELPARAM(0,100));
   SendDlgItemMessage (hDlg, IDC_SET_USAGEBAR, PBM_SETPOS, (WPARAM)dwPer, 0);
}

