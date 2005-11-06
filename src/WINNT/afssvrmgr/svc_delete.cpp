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
#include "svc_delete.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Services_Delete_OnInitDialog (HWND hDlg, LPIDENT lpi);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Services_Delete (LPIDENT lpiService)
{
   INT_PTR rc = ModalDialogParam (IDD_SVC_DELETE, GetActiveWindow(), (DLGPROC)Services_Delete_DlgProc, (LPARAM)lpiService);
   if (rc == IDOK)
      {
      StartTask (taskSVC_DELETE, NULL, lpiService);
      }
}



BOOL CALLBACK Services_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_DELETE, hDlg, msg, wp, lp))
      return TRUE;

   static LPIDENT lpi = NULL;
   if (msg == WM_INITDIALOG)
      lpi = (LPIDENT)lp;

   if (lpi != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Services_Delete_OnInitDialog (hDlg, lpi);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;
               }
            break;

         case WM_DESTROY:
            lpi = NULL;
            break;
         }
      }

   return FALSE;
}


void Services_Delete_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szService[ cchNAME ];
   lpi->GetServerName (szServer);
   lpi->GetServiceName (szService);

   TCHAR szOld[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_DELSVC_DESC, szOld, cchRESOURCE);

   LPTSTR pszNew = FormatString (szOld, TEXT("%s%s"), szServer, szService);
   SetDlgItemText (hDlg, IDC_DELSVC_DESC, pszNew);
   FreeString (pszNew);
}

