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
#include "svr_install.h"
#include "propcache.h"

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL WINAPI Server_Install_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Install_OnInitDialog (HWND hDlg, LPSVR_INSTALL_PARAMS lpp);
void Server_Install_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_INSTALL_PARAMS lpp);
void Server_Install_OnBrowse (HWND hDlg);
void Server_Install_EnableOK (HWND hDlg);
void Server_Install_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Install (LPIDENT lpiServer)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVR_INSTALL, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_INSTALL_PARAMS lpp = New (SVR_INSTALL_PARAMS);
      lpp->lpiServer = lpiServer;
      lpp->szSource[0] = TEXT('\0');
      lpp->szTarget[0] = TEXT('\0');

      HWND hDlg = ModelessDialogParam (IDD_SVR_INSTALL, NULL, (DLGPROC)Server_Install_DlgProc, (LPARAM)lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL WINAPI Server_Install_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_INSTALL, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_INSTALL_PARAMS lpp;

   if (msg == WM_INITDIALOG)
       SetWindowLongPtr (hDlg, DWLP_USER, lp);

   if ((lpp = (LPSVR_INSTALL_PARAMS)GetWindowLongPtr(hDlg,DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_INSTALL, NULL, hDlg);
            Server_Install_OnInitDialog (hDlg, lpp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_Install_OnEndTask_InitDialog (hDlg, ptp, lpp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_Install_OnOK (hDlg);
                  // fall through

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_BROWSE:
                  Server_Install_OnBrowse (hDlg);
                  break;

               case IDC_FILENAME:
               case IDC_DIRECTORY:
               case IDC_SERVER:
                  Server_Install_EnableOK (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            SetWindowLongPtr (hDlg, DWLP_USER, 0);
            PropCache_Delete (pcSVR_INSTALL, NULL);
            Delete (lpp);
            break;
         }
      }

   return FALSE;
}


void Server_Install_OnInitDialog (HWND hDlg, LPSVR_INSTALL_PARAMS lpp)
{
   LPTSTR pszText = FormatString (IDS_INSTALL_DESC1);
   SetDlgItemText (hDlg, IDC_INSTALL_DESC, pszText);
   FreeString (pszText);

   LPSVR_ENUM_TO_COMBOBOX_PACKET lppEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lppEnum->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lppEnum->lpiSelect = lpp->lpiServer;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lppEnum);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK),       FALSE);
}


void Server_Install_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSVR_INSTALL_PARAMS lpp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);
   Server_Install_EnableOK (hDlg);
}


void Server_Install_EnableOK (HWND hDlg)
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

   if (fEnable)
      {
      TCHAR szName[ MAX_PATH ];
      GetDlgItemText (hDlg, IDC_DIRECTORY, szName, MAX_PATH);
      if (szName[0] == TEXT('\0'))
         fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Server_Install_OnOK (HWND hDlg)
{
   LPSVR_INSTALL_PARAMS lpp = New (SVR_INSTALL_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   GetDlgItemText (hDlg, IDC_FILENAME,  lpp->szSource, MAX_PATH);
   GetDlgItemText (hDlg, IDC_DIRECTORY, lpp->szTarget, MAX_PATH);

   StartTask (taskSVR_INSTALL, NULL, lpp);
}


void Server_Install_OnBrowse (HWND hDlg)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, IDS_FILTER_ALLFILES);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   TCHAR szFilename[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_FILENAME, szFilename, MAX_PATH);

   OPENFILENAME ofn;
   memset (&ofn, 0x00, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hDlg;
   ofn.hInstance = THIS_HINST;
   ofn.lpstrFilter = szFilter;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = szFilename;
   ofn.nMaxFile = MAX_PATH;
   ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   TCHAR szPath[ MAX_PATH ];
   GetCurrentDirectory (MAX_PATH, szPath);

   BOOL rc = GetOpenFileName (&ofn);

   SetCurrentDirectory (szPath);

   if (rc)
      SetDlgItemText (hDlg, IDC_FILENAME, szFilename);
}

