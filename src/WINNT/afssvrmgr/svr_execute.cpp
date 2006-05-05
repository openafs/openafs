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
#include "svr_execute.h"
#include "propcache.h"

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL WINAPI Server_Execute_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Execute_OnInitDialog (HWND hDlg, LPSVR_EXECUTE_PARAMS lpp);
void Server_Execute_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_EXECUTE_PARAMS lpp);
void Server_Execute_EnableOK (HWND hDlg);
void Server_Execute_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Execute (LPIDENT lpiServer)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_EXECUTE, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_EXECUTE_PARAMS lpp = New (SVR_EXECUTE_PARAMS);
      lpp->lpiServer = lpiServer;
      lpp->szCommand[0] = TEXT('\0');

      HWND hDlg = ModelessDialogParam (IDD_SVR_EXECUTE, NULL, (DLGPROC)Server_Execute_DlgProc, (LPARAM)lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL WINAPI Server_Execute_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_EXECUTE, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_EXECUTE_PARAMS lpp;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   if ((lpp = (LPSVR_EXECUTE_PARAMS)GetWindowLongPtr(hDlg,DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_EXECUTE, NULL, hDlg);
            Server_Execute_OnInitDialog (hDlg, lpp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_Execute_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_Execute_OnOK (hDlg);
                  DestroyWindow (hDlg);
                  break;

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_SERVER:
               case IDC_COMMAND:
                  Server_Execute_EnableOK (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            Delete (lpp);
            SetWindowLongPtr (hDlg, DWLP_USER, 0);
            PropCache_Delete (pcSVR_EXECUTE, NULL);
            break;
         }
      }

   return FALSE;
}


void Server_Execute_OnInitDialog (HWND hDlg, LPSVR_EXECUTE_PARAMS lpp)
{
   LPSVR_ENUM_TO_COMBOBOX_PACKET lppEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lppEnum->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lppEnum->lpiSelect = lpp->lpiServer;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lppEnum);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
}


void Server_Execute_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_EXECUTE_PARAMS lpp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);
   Server_Execute_EnableOK (hDlg);
}


void Server_Execute_EnableOK (HWND hDlg)
{
   LPIDENT lpiServer = NULL;

   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_SERVER)))
      lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));

   BOOL fEnable = (lpiServer != NULL) ? TRUE : FALSE;

   if (fEnable)
      {
      TCHAR szCommand[ MAX_PATH ];
      GetDlgItemText (hDlg, IDC_COMMAND, szCommand, MAX_PATH);
      if (szCommand[0] == TEXT('\0'))
         fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Server_Execute_OnOK (HWND hDlg)
{
   LPSVR_EXECUTE_PARAMS lpp = New (SVR_EXECUTE_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   GetDlgItemText (hDlg, IDC_COMMAND, lpp->szCommand, MAX_PATH);

   StartTask (taskSVR_EXECUTE, NULL, lpp);
}

