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
#include "set_release.h"
#include "propcache.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Release_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Release_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Filesets_Release_OnOK (HWND hDlg, LPIDENT lpi);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Release (LPIDENT lpi)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSET_RELEASE, lpi)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      HWND hDlg = ModelessDialogParam (IDD_SET_RELEASE, NULL, (DLGPROC)Filesets_Release_DlgProc, (LPARAM)lpi);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL CALLBACK Filesets_Release_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_RELEASE, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPIDENT lpi;
   if ((lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSET_RELEASE, lpi, hDlg);
            Filesets_Release_OnInitDialog (hDlg, lpi);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Filesets_Release_OnOK (hDlg, lpi);
                  DestroyWindow (hDlg);
                  break;
               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            PropCache_Delete (pcSET_RELEASE, lpi);
            break;
         }
      }

   return FALSE;
}


void Filesets_Release_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   lpi->GetServerName (szServer);
   lpi->GetAggregateName (szAggregate);
   lpi->GetFilesetName (szFileset);

   TCHAR szOld[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_RELSET_DESC, szOld, cchRESOURCE);

   LPTSTR pszText = FormatString (szOld, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_RELSET_DESC, pszText);
   FreeString (pszText);

   CheckDlgButton (hDlg, IDC_RELSET_NORMAL, TRUE);
   CheckDlgButton (hDlg, IDC_RELSET_FORCE, FALSE);
}


void Filesets_Release_OnOK (HWND hDlg, LPIDENT lpi)
{
   LPSET_RELEASE_PARAMS lpp = New (SET_RELEASE_PARAMS);
   memset (lpp, 0x00, sizeof(SET_RELEASE_PARAMS));
   lpp->lpiRW = lpi;
   lpp->fForce = IsDlgButtonChecked (hDlg, IDC_RELSET_FORCE);
   StartTask (taskSET_RELEASE, NULL, lpp);
}

