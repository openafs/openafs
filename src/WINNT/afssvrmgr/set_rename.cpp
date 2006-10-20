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
#include "set_rename.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Rename_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Rename_OnInitDialog (HWND hDlg, LPSET_RENAME_APPLY_PARAMS psrp);
void Filesets_Rename_EnableOK (HWND hDlg, LPSET_RENAME_APPLY_PARAMS psrp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_ShowRename (LPIDENT lpiFileset)
{
   LPSET_RENAME_INIT_PARAMS lpp = New (SET_RENAME_INIT_PARAMS);
   memset (lpp, 0x00, sizeof(SET_RENAME_INIT_PARAMS));
   lpp->lpiReq = lpiFileset;

   StartTask (taskSET_RENAME_INIT, g.hMain, lpp);
}


void Filesets_OnEndTask_ShowRename (LPTASKPACKET ptp)
{
   LPSET_RENAME_INIT_PARAMS lpp = (LPSET_RENAME_INIT_PARAMS)(ptp->lpUser);

   TCHAR szSvrName[ cchNAME ];
   TCHAR szAggName[ cchNAME ];
   TCHAR szSetName[ cchNAME ];
   lpp->lpiReq->GetServerName (szSvrName);
   lpp->lpiReq->GetAggregateName (szAggName);
   lpp->lpiReq->GetFilesetName (szSetName);

   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szSvrName, szAggName, szSetName);
      }
   else if (!lpp->lpiRW) // couldn't find RW fileset entry?
      {
      ErrorDialog (ptp->status, IDS_ERROR_NOT_REPLICATED, TEXT("%s"), szSetName);
      }
   else
      {
      LPSET_RENAME_APPLY_PARAMS psrp = New (SET_RENAME_APPLY_PARAMS);
      memset (psrp, 0x00, sizeof(SET_RENAME_APPLY_PARAMS));
      psrp->lpiFileset = lpp->lpiRW;

      int rc = ModalDialogParam (IDD_SET_RENAME, GetActiveWindow(), (DLGPROC)Filesets_Rename_DlgProc, (LPARAM)psrp);
      if (rc != IDOK)
         {
         Delete (psrp);
         }
      else
         {
         StartTask (taskSET_RENAME_APPLY, NULL, psrp);
         }
      }

   Delete (lpp);
}


BOOL CALLBACK Filesets_Rename_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_RENAME, hDlg, msg, wp, lp))
      return TRUE;

   static LPSET_RENAME_APPLY_PARAMS psrp = NULL;
   if (msg == WM_INITDIALOG)
      psrp = (LPSET_RENAME_APPLY_PARAMS)lp;

   if (psrp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Filesets_Rename_OnInitDialog (hDlg, psrp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_RENSET_NEW:
                  Filesets_Rename_EnableOK (hDlg, psrp);
                  break;

               case IDHELP:
                  WinHelp (hDlg, cszHELPFILENAME, HELP_CONTEXT, IDH_SVRMGR_RENAMEFILESET_OVERVIEW);
                  break;
               }
            break;

         case WM_DESTROY:
            psrp = NULL;
            break;
         }
      }

   return FALSE;
}


void Filesets_Rename_OnInitDialog (HWND hDlg, LPSET_RENAME_APPLY_PARAMS psrp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];

   psrp->lpiFileset->GetServerName (szServer);
   psrp->lpiFileset->GetAggregateName (szAggregate);
   psrp->lpiFileset->GetFilesetName (szFileset);

   TCHAR szOld[ cchRESOURCE ];

   GetDlgItemText (hDlg, IDC_RENSET_DESC, szOld, cchRESOURCE);
   LPTSTR pszNew = FormatString (szOld, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_RENSET_DESC, pszNew);
   FreeString (pszNew);

   SetDlgItemText (hDlg, IDC_RENSET_OLD, szFileset);
   SetDlgItemText (hDlg, IDC_RENSET_NEW, szFileset);

   PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg,IDC_RENSET_NEW), TRUE);

   Filesets_Rename_EnableOK (hDlg, psrp);
}


void Filesets_Rename_EnableOK (HWND hDlg, LPSET_RENAME_APPLY_PARAMS psrp)
{
   TCHAR szOld[ cchNAME ];

   GetDlgItemText (hDlg, IDC_RENSET_OLD, szOld, cchNAME);
   GetDlgItemText (hDlg, IDC_RENSET_NEW, psrp->szNewName, cchNAME);

   BOOL fEnable = TRUE;

   if (!psrp->szNewName[0])
      {
      fEnable = FALSE;
      }
   else if (!lstrcmpi (szOld, psrp->szNewName))
      {
      fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}

