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
#include "set_delete.h"
#include "propcache.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Delete_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Filesets_Delete_OnCheckBoxes (HWND hDlg);
void Filesets_Delete_OnEndTask_FindGhost (HWND hDlg, LPTASKPACKET ptp, LPSET_DELETE_PARAMS psdp);
void Filesets_Delete_ShrinkWindow (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Delete (LPIDENT lpiFileset)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSET_DELETE, lpiFileset)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSET_DELETE_PARAMS psdp = New (SET_DELETE_PARAMS);
      memset (psdp, 0x00, sizeof(SET_DELETE_PARAMS));
      psdp->lpiFileset = lpiFileset;
      psdp->iddHelp = IDD_SET_DELETE;

      ModelessDialogParam (IDD_SET_DELETE, NULL, (DLGPROC)Filesets_Delete_DlgProc, (LPARAM)psdp);
      // NOTE: Don't show window!
      }
}



BOOL CALLBACK Filesets_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPSET_DELETE_PARAMS psdp;
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);
   if ((psdp = (LPSET_DELETE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (AfsAppLib_HandleHelp (psdp->iddHelp, hDlg, msg, wp, lp))
         return TRUE;

      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSET_DELETE, psdp->lpiFileset, hDlg);
            Filesets_Delete_OnInitDialog (hDlg, psdp->lpiFileset);
            StartTask (taskSET_FIND_GHOST, hDlg, psdp->lpiFileset);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSET_FIND_GHOST)
                  Filesets_Delete_OnEndTask_FindGhost (hDlg, ptp, psdp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  psdp->fVLDB   = IsDlgButtonChecked (hDlg, IDC_DELSET_VLDB);
                  psdp->fServer = IsDlgButtonChecked (hDlg, IDC_DELSET_SERVER);

                  if (psdp->fVLDB || psdp->fServer)
                     StartTask (taskSET_DELETE, NULL, psdp);
                  else
                     Delete (psdp);
                  DestroyWindow (hDlg);
                  break;

               case IDCANCEL:
                  Delete (psdp);
                  DestroyWindow (hDlg);
                  break;

               case IDC_DELSET_VLDB:
               case IDC_DELSET_SERVER:
                  Filesets_Delete_OnCheckBoxes (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            PropCache_Delete (hDlg);
            SetWindowLongPtr (hDlg, DWLP_USER, 0);
            psdp = NULL;
            break;
         }
      }

   return FALSE;
}


void Filesets_Delete_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];

   lpi->GetServerName (szServer);
   lpi->GetAggregateName (szAggregate);
   lpi->GetFilesetName (szFileset);

   TCHAR szOld[ cchRESOURCE ];

   GetDlgItemText (hDlg, IDC_DELSET_DESC, szOld, cchRESOURCE);
   LPTSTR pszNew = FormatString (szOld, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_DELSET_DESC, pszNew);
   FreeString (pszNew);

   GetDlgItemText (hDlg, IDC_DELSET_SERVER, szOld, cchRESOURCE);
   pszNew = FormatString (szOld, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_DELSET_SERVER, pszNew);
   FreeString (pszNew);

   GetDlgItemText (hDlg, IDC_DELSET_VLDB, szOld, cchRESOURCE);
   pszNew = FormatString (szOld, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_DELSET_VLDB, pszNew);
   FreeString (pszNew);

   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
}


void Filesets_Delete_OnEndTask_FindGhost (HWND hDlg, LPTASKPACKET ptp, LPSET_DELETE_PARAMS psdp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   psdp->lpiFileset->GetServerName (szServer);
   psdp->lpiFileset->GetAggregateName (szAggregate);
   psdp->lpiFileset->GetFilesetName (szFileset);

   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      DestroyWindow (hDlg);
      }
   else if ( (TASKDATA(ptp)->fs.Type == ftREADWRITE) && (TASKDATA(ptp)->fHasReplicas) )
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_DELETE_REPLICATED_FILESET, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      DestroyWindow (hDlg);
      }
   else
      {
      if (TASKDATA(ptp)->fs.Type == ftREPLICA)
         psdp->wGhost  = GHOST_HAS_SERVER_ENTRY;
      else if (TASKDATA(ptp)->fs.Type == ftCLONE)
         psdp->wGhost  = GHOST_HAS_SERVER_ENTRY;
      else
         psdp->wGhost  = TASKDATA(ptp)->wGhost;

      psdp->fVLDB   = (psdp->wGhost & GHOST_HAS_VLDB_ENTRY)   ? TRUE : FALSE;
      psdp->fServer = (psdp->wGhost & GHOST_HAS_SERVER_ENTRY) ? TRUE : FALSE;

      CheckDlgButton (hDlg, IDC_DELSET_VLDB,   psdp->fVLDB);
      CheckDlgButton (hDlg, IDC_DELSET_SERVER, psdp->fServer);

      if (TASKDATA(ptp)->fs.Type == ftREPLICA)
         {
         psdp->iddHelp = IDD_SET_DELREP;

         LPTSTR pszNew = FormatString (IDS_DELSET_REPLICA_DESC, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
         SetDlgItemText (hDlg, IDC_DELSET_DESC, pszNew);
         FreeString (pszNew);

         Filesets_Delete_ShrinkWindow (hDlg);
         }
      else if (TASKDATA(ptp)->fs.Type == ftCLONE)
         {
         psdp->iddHelp = IDD_SET_DELCLONE;

         LPTSTR pszNew = FormatString (IDS_DELSET_CLONE_DESC, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
         SetDlgItemText (hDlg, IDC_DELSET_DESC, pszNew);
         FreeString (pszNew);

         Filesets_Delete_ShrinkWindow (hDlg);
         }

      if (!(psdp->wGhost & GHOST_HAS_SERVER_ENTRY))
         EnableWindow (GetDlgItem (hDlg, IDC_DELSET_SERVER), FALSE);

      if (!(psdp->wGhost & GHOST_HAS_VLDB_ENTRY))
         EnableWindow (GetDlgItem (hDlg, IDC_DELSET_VLDB), FALSE);

      Filesets_Delete_OnCheckBoxes (hDlg);

      ShowWindow (hDlg, SW_SHOW);
      }
}


void Filesets_Delete_OnCheckBoxes (HWND hDlg)
{
   BOOL fEnableOK = TRUE;
   if ( !IsDlgButtonChecked (hDlg, IDC_DELSET_VLDB) &&
        !IsDlgButtonChecked (hDlg, IDC_DELSET_SERVER) )
      fEnableOK = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnableOK);
}


void Filesets_Delete_ShrinkWindow (HWND hDlg)
{
   RECT rDialog;
   GetWindowRect (hDlg, &rDialog);

   RECT rCheckbox;
   GetRectInParent (GetDlgItem (hDlg, IDC_DELSET_VLDB), &rCheckbox);

   RECT rButton;
   GetRectInParent (GetDlgItem (hDlg, IDOK), &rButton);

   LONG cy = rButton.top -rCheckbox.top; // shrink window by this much

   EnableWindow (GetDlgItem (hDlg, IDC_DELSET_SERVER), FALSE);
   ShowWindow (GetDlgItem (hDlg, IDC_DELSET_SERVER), SW_HIDE);

   EnableWindow (GetDlgItem (hDlg, IDC_DELSET_VLDB), FALSE);
   ShowWindow (GetDlgItem (hDlg, IDC_DELSET_VLDB), SW_HIDE);

   GetRectInParent (GetDlgItem (hDlg, IDOK), &rButton);
   SetWindowPos (GetDlgItem (hDlg, IDOK), NULL,
                 rButton.left, rButton.top -cy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

   GetRectInParent (GetDlgItem (hDlg, IDCANCEL), &rButton);
   SetWindowPos (GetDlgItem (hDlg, IDCANCEL), NULL,
                 rButton.left, rButton.top -cy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

   GetRectInParent (GetDlgItem (hDlg, IDHELP), &rButton);
   SetWindowPos (GetDlgItem (hDlg, IDHELP), NULL,
                 rButton.left, rButton.top -cy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

   SetWindowPos (hDlg, NULL,
                 0, 0, cxRECT(rDialog), cyRECT(rDialog) -cy,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

