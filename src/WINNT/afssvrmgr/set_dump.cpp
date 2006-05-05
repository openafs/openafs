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
#include "set_dump.h"
#include "columns.h"
#include "svr_window.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Dump_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Dump_OnInitDialog (HWND hDlg, LPSET_DUMP_PARAMS psdp);
void Filesets_Dump_OnSelect (HWND hDlg);
void Filesets_Dump_EnableOK (HWND hDlg);
void Filesets_Dump_OnOK (HWND hDlg, LPSET_DUMP_PARAMS psdp);
void Filesets_Dump_OnBrowse (HWND hDlg, LPSET_DUMP_PARAMS psdp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Dump (LPIDENT lpi)
{
   LPSET_DUMP_PARAMS psdp = New (SET_DUMP_PARAMS);
   memset (psdp, 0x00, sizeof(SET_DUMP_PARAMS));
   psdp->lpi = lpi;

   INT_PTR rc = ModalDialogParam (IDD_SET_DUMP, NULL, (DLGPROC)Filesets_Dump_DlgProc, (LPARAM)psdp);

   if (rc != IDOK)
      {
      Delete (psdp);
      }
   else
      {
      StartTask (taskSET_DUMP, NULL, psdp);
      }
}


BOOL CALLBACK Filesets_Dump_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_DUMP, hDlg, msg, wp, lp))
      return TRUE;

   static LPSET_DUMP_PARAMS psdp = NULL;
   if (msg == WM_INITDIALOG)
      psdp = (LPSET_DUMP_PARAMS)lp;

   if (psdp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Filesets_Dump_OnInitDialog (hDlg, psdp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Filesets_Dump_OnOK (hDlg, psdp);
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_DUMP_FULL:
               case IDC_DUMP_LIMIT_TIME:
                  Filesets_Dump_OnSelect (hDlg);
                  break;

               case IDC_DUMP_FILENAME:
                  Filesets_Dump_EnableOK (hDlg);
                  break;

               case IDC_DUMP_BROWSE:
                  Filesets_Dump_OnBrowse (hDlg, psdp);
                  break;
               }
            break;

         case WM_DESTROY:
            psdp = NULL;
            break;
         }
      }

   return FALSE;
}


void Filesets_Dump_OnInitDialog (HWND hDlg, LPSET_DUMP_PARAMS psdp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   psdp->lpi->GetServerName (szServer);
   psdp->lpi->GetAggregateName (szAggregate);
   psdp->lpi->GetFilesetName (szFileset);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_DUMP_FULL, szText, cchRESOURCE);

   LPTSTR pszText = FormatString (szText, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_DUMP_FULL, pszText);
   FreeString (pszText);

   pszText = FormatString (IDS_SET_DUMP_NAME, TEXT("%s"), szFileset);
   SetDlgItemText (hDlg, IDC_DUMP_FILENAME, pszText);
   FreeString (pszText);

   // Get the local system time
   SYSTEMTIME st;
   GetSystemTime (&st);
   FILETIME ft;
   SystemTimeToFileTime (&st, &ft);
   FILETIME lft;
   FileTimeToLocalFileTime (&ft, &lft);
   FileTimeToSystemTime (&lft, &st);

   DA_SetDate (GetDlgItem (hDlg, IDC_DUMP_DATE), &st);
   TI_SetTime (GetDlgItem (hDlg, IDC_DUMP_TIME), &st);

   CheckDlgButton (hDlg, IDC_DUMP_FULL, TRUE);
   Filesets_Dump_OnSelect (hDlg);
   Filesets_Dump_EnableOK (hDlg);
}


void Filesets_Dump_OnSelect (HWND hDlg)
{
   BOOL fEnable;

   fEnable = IsDlgButtonChecked (hDlg, IDC_DUMP_LIMIT_TIME);
   EnableWindow (GetDlgItem (hDlg, IDC_DUMP_DATE), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_DUMP_TIME), fEnable);
}


void Filesets_Dump_EnableOK (HWND hDlg)
{
   TCHAR szText[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_DUMP_FILENAME, szText, MAX_PATH);

   EnableWindow (GetDlgItem (hDlg, IDOK), (szText[0] == TEXT('\0')) ? FALSE : TRUE);
}


void Filesets_Dump_OnOK (HWND hDlg, LPSET_DUMP_PARAMS psdp)
{
   GetDlgItemText (hDlg, IDC_DUMP_FILENAME, psdp->szFilename, MAX_PATH);

   psdp->fDumpByDate = IsDlgButtonChecked (hDlg, IDC_DUMP_LIMIT_TIME);
   memset (&psdp->stDump, 0x00, sizeof(SYSTEMTIME));
   DA_GetDate (GetDlgItem (hDlg, IDC_DUMP_DATE), &psdp->stDump);
   TI_GetTime (GetDlgItem (hDlg, IDC_DUMP_TIME), &psdp->stDump);
}


void Filesets_Dump_OnBrowse (HWND hDlg, LPSET_DUMP_PARAMS psdp)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, IDS_RESTORE_FILTER);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   TCHAR szFilename[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_DUMP_FILENAME, szFilename, MAX_PATH);

   OPENFILENAME sfn;
   memset (&sfn, 0x00, sizeof(sfn));
   sfn.lStructSize = sizeof(sfn);
   sfn.hwndOwner = hDlg;
   sfn.hInstance = THIS_HINST;
   sfn.lpstrFilter = szFilter;
   sfn.nFilterIndex = 1;
   sfn.lpstrFile = szFilename;
   sfn.nMaxFile = MAX_PATH;
   sfn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
   sfn.lpstrDefExt = TEXT("dmp");

   if (GetSaveFileName (&sfn))
      {
      SetDlgItemText (hDlg, IDC_DUMP_FILENAME, szFilename);
      Filesets_Dump_EnableOK (hDlg);
      }
}

