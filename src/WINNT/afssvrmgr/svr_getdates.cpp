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
#include "svr_getdates.h"
#include "propcache.h"

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL WINAPI Server_GetDates_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_GetDates_OnInitDialog (HWND hDlg, LPSVR_GETDATES_PARAMS lpp);
void Server_GetDates_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_GETDATES_PARAMS lpp);
void Server_GetDates_EnableOK (HWND hDlg);
void Server_GetDates_OnOK (HWND hDlg);

BOOL WINAPI Server_GetDates_Results_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_GetDates_Results_OnInitDialog (HWND hDlg, LPSVR_GETDATES_PARAMS lppIn);
void Server_GetDates_Results_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_GetDates (LPIDENT lpiServer)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_GETDATES, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_GETDATES_PARAMS lpp = New (SVR_GETDATES_PARAMS);
      lpp->lpiServer = lpiServer;
      lpp->szFilename[0] = TEXT('\0');

      HWND hDlg = ModelessDialogParam (IDD_SVR_GETDATES, NULL, (DLGPROC)Server_GetDates_DlgProc, (LPARAM)lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL WINAPI Server_GetDates_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_GETDATES, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_GETDATES_PARAMS lpp;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   if ((lpp = (LPSVR_GETDATES_PARAMS)GetWindowLongPtr(hDlg,DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_GETDATES, NULL, hDlg);
            Server_GetDates_OnInitDialog (hDlg, lpp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_GetDates_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_GetDates_OnOK (hDlg);
                  // fall through

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_SERVER:
               case IDC_FILENAME:
                  Server_GetDates_EnableOK (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            Delete (lpp);
            SetWindowLongPtr (hDlg, DWLP_USER, 0);
            PropCache_Delete (pcSVR_GETDATES, NULL);
            break;
         }
      }

   return FALSE;
}


void Server_GetDates_OnInitDialog (HWND hDlg, LPSVR_GETDATES_PARAMS lpp)
{
   LPSVR_ENUM_TO_COMBOBOX_PACKET lppEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lppEnum->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lppEnum->lpiSelect = lpp->lpiServer;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lppEnum);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK),       FALSE);
}


void Server_GetDates_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_GETDATES_PARAMS lpp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);
   Server_GetDates_EnableOK (hDlg);
}


void Server_GetDates_EnableOK (HWND hDlg)
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


void Server_GetDates_OnOK (HWND hDlg)
{
   LPSVR_GETDATES_PARAMS lpp = New (SVR_GETDATES_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   GetDlgItemText (hDlg, IDC_FILENAME, lpp->szFilename, MAX_PATH);

   ModelessDialogParam (IDD_SVR_GETDATES_RESULTS, NULL, (DLGPROC)Server_GetDates_Results_DlgProc, (LPARAM)lpp);
}


BOOL WINAPI Server_GetDates_Results_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_GETDATES_RESULTS, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         Server_GetDates_Results_OnInitDialog (hDlg, (LPSVR_GETDATES_PARAMS)lp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_GETDATES)
               Server_GetDates_Results_OnEndTask_InitDialog (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Server_GetDates_Results_OnInitDialog (HWND hDlg, LPSVR_GETDATES_PARAMS lppIn)
{
   LPSVR_GETDATES_PARAMS lpp = New (SVR_GETDATES_PARAMS);
   memcpy (lpp, lppIn, sizeof(SVR_GETDATES_PARAMS));
   StartTask (taskSVR_GETDATES, hDlg, lpp);

   TCHAR szServer[ cchNAME ];
   lppIn->lpiServer->GetServerName (szServer);
   SetDlgItemText (hDlg, IDC_SERVER, szServer);

   SetDlgItemText (hDlg, IDC_FILENAME, lppIn->szFilename);
}


void Server_GetDates_Results_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp)
{
   if (ptp->rc && TASKDATA(ptp)->pszText1)
      {
      LPTSTR pszText = FormatString (IDS_LASTMODIFIED, TEXT("%s"), TASKDATA(ptp)->pszText1);
      SetDlgItemText (hDlg, IDC_DATE_FILE, pszText);
      FreeString (pszText);
      }
   if (ptp->rc && TASKDATA(ptp)->pszText2)
      {
      LPTSTR pszText = FormatString (IDS_LASTMODIFIED, TEXT("%s"), TASKDATA(ptp)->pszText2);
      SetDlgItemText (hDlg, IDC_DATE_BAK, pszText);
      FreeString (pszText);
      }
   if (ptp->rc && TASKDATA(ptp)->pszText3)
      {
      LPTSTR pszText = FormatString (IDS_LASTMODIFIED, TEXT("%s"), TASKDATA(ptp)->pszText3);
      SetDlgItemText (hDlg, IDC_DATE_OLD, pszText);
      FreeString (pszText);
      }

   ShowWindow (hDlg, SW_SHOW);
}

