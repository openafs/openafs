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
#include "svr_prune.h"
#include "propcache.h"

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL WINAPI Server_Prune_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Prune_OnInitDialog (HWND hDlg, LPSVR_PRUNE_PARAMS lpp);
void Server_Prune_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_PRUNE_PARAMS lpp);
void Server_Prune_EnableOK (HWND hDlg);
void Server_Prune_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Prune (LPIDENT lpiServer, BOOL fBAK, BOOL fOLD, BOOL fCore)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_PRUNE, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_PRUNE_PARAMS lpp = New (SVR_PRUNE_PARAMS);
      lpp->lpiServer = lpiServer;
      lpp->fBAK = fBAK;
      lpp->fOLD = fOLD;
      lpp->fCore = fCore;

      HWND hDlg = ModelessDialogParam (IDD_SVR_PRUNE, NULL, (DLGPROC)Server_Prune_DlgProc, (LPARAM)lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL WINAPI Server_Prune_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_PRUNE, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_PRUNE_PARAMS lpp;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   if ((lpp = (LPSVR_PRUNE_PARAMS)GetWindowLong(hDlg,DWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_PRUNE, NULL, hDlg);
            Server_Prune_OnInitDialog (hDlg, lpp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_Prune_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_Prune_OnOK (hDlg);
                  // fall through

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_OP_DELETE_BAK:
               case IDC_OP_DELETE_OLD:
               case IDC_OP_DELETE_CORE:
                  Server_Prune_EnableOK (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            SetWindowLong (hDlg, DWL_USER, 0);
            PropCache_Delete (pcSVR_PRUNE, NULL);
            Delete (lpp);
            break;
         }
      }

   return FALSE;
}


void Server_Prune_OnInitDialog (HWND hDlg, LPSVR_PRUNE_PARAMS lpp)
{
   CheckDlgButton (hDlg, IDC_OP_DELETE_BAK,  lpp->fBAK);
   CheckDlgButton (hDlg, IDC_OP_DELETE_OLD,  lpp->fOLD);
   CheckDlgButton (hDlg, IDC_OP_DELETE_CORE, lpp->fCore);

   LPSVR_ENUM_TO_COMBOBOX_PACKET lppEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lppEnum->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lppEnum->lpiSelect = lpp->lpiServer;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lppEnum);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK),       FALSE);
}


void Server_Prune_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_PRUNE_PARAMS lpp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);

   Server_Prune_EnableOK (hDlg);
}


void Server_Prune_EnableOK (HWND hDlg)
{
   LPIDENT lpiServer = NULL;

   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_SERVER)))
      {
      lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
      }

   BOOL fEnable = (lpiServer != NULL) ? TRUE : FALSE;

   if ( !IsDlgButtonChecked (hDlg, IDC_OP_DELETE_BAK) && 
        !IsDlgButtonChecked (hDlg, IDC_OP_DELETE_OLD) && 
        !IsDlgButtonChecked (hDlg, IDC_OP_DELETE_CORE) )
      {
      fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Server_Prune_OnOK (HWND hDlg)
{
   LPSVR_PRUNE_PARAMS lpp = New (SVR_PRUNE_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   lpp->fBAK = IsDlgButtonChecked (hDlg, IDC_OP_DELETE_BAK);
   lpp->fOLD = IsDlgButtonChecked (hDlg, IDC_OP_DELETE_OLD);
   lpp->fCore = IsDlgButtonChecked (hDlg, IDC_OP_DELETE_CORE);

   StartTask (taskSVR_PRUNE, NULL, lpp);
}

