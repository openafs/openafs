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
#include "svr_uninstall.h"
#include "propcache.h"

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL WINAPI Server_Uninstall_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Uninstall_OnInitDialog (HWND hDlg, LPSVR_UNINSTALL_PARAMS lpp);
void Server_Uninstall_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_UNINSTALL_PARAMS lpp);
void Server_Uninstall_EnableOK (HWND hDlg);
void Server_Uninstall_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Uninstall (LPIDENT lpiServer)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_UNINSTALL, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_UNINSTALL_PARAMS lpp = New (SVR_UNINSTALL_PARAMS);
      lpp->lpiServer = lpiServer;
      lpp->szUninstall[0] = TEXT('\0');

      HWND hDlg = ModelessDialogParam (IDD_SVR_UNINSTALL, NULL, (DLGPROC)Server_Uninstall_DlgProc, (LPARAM)lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL WINAPI Server_Uninstall_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_UNINSTALL, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_UNINSTALL_PARAMS lpp;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   if ((lpp = (LPSVR_UNINSTALL_PARAMS)GetWindowLong(hDlg,DWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_UNINSTALL, NULL, hDlg);
            Server_Uninstall_OnInitDialog (hDlg, lpp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_Uninstall_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_Uninstall_OnOK (hDlg);
                  DestroyWindow (hDlg);
                  break;

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_SERVER:
               case IDC_FILENAME:
                  Server_Uninstall_EnableOK (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            SetWindowLong (hDlg, DWL_USER, 0);
            PropCache_Delete (pcSVR_UNINSTALL, NULL);
            Delete (lpp);
            break;
         }
      }

   return FALSE;
}


void Server_Uninstall_OnInitDialog (HWND hDlg, LPSVR_UNINSTALL_PARAMS lpp)
{
   LPSVR_ENUM_TO_COMBOBOX_PACKET lppEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lppEnum->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lppEnum->lpiSelect = lpp->lpiServer;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lppEnum);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK),       FALSE);
}


void Server_Uninstall_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_UNINSTALL_PARAMS lpp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);
   Server_Uninstall_EnableOK (hDlg);
}


void Server_Uninstall_EnableOK (HWND hDlg)
{
   LPIDENT lpiServer = NULL;

   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_SERVER)))
      lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));

   BOOL fEnable = (lpiServer != NULL) ? TRUE : FALSE;

   if (fEnable)
      {
      TCHAR szName[ MAX_PATH ];
      GetDlgItemText (hDlg, IDC_FILENAME, szName, MAX_PATH);
      if (szName[0] == TEXT('\0'))
         fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Server_Uninstall_OnOK (HWND hDlg)
{
   LPSVR_UNINSTALL_PARAMS lpp = New (SVR_UNINSTALL_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   GetDlgItemText (hDlg, IDC_FILENAME, lpp->szUninstall, MAX_PATH);

   StartTask (taskSVR_UNINSTALL, NULL, lpp);
}

