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

#include "TaAfsUsrMgr.h"
#include "grp_delete.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Group_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Group_Delete_OnInitDialog (HWND hDlg);
void Group_Delete_OnDestroy (HWND hDlg);
void Group_Delete_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Group_ShowDelete (LPASIDLIST pGroupList)
{
   ModalDialogParam (IDD_GROUP_DELETE, g.hMain, (DLGPROC)Group_Delete_DlgProc, (LPARAM)pGroupList);
}


BOOL CALLBACK Group_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_GROUP_DELETE, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         Group_Delete_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         Group_Delete_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Group_Delete_OnOK (hDlg);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;
            }
         break;
      }

   return FALSE;
}


void Group_Delete_OnInitDialog (HWND hDlg)
{
   LPASIDLIST pGroupList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);

   // Fix the title of the dialog
   //
   if (pGroupList->cEntries == 1)
      {
      ULONG status;
      TCHAR szName[ cchNAME ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, pGroupList->aEntries[0].idObject, szName, &status);

      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_DELETE_TITLE, szText, cchRESOURCE);

      LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);
      }
   else
      {
      LPTSTR pszNames = CreateNameList (pGroupList);

      LPTSTR pszText = FormatString (IDS_GROUP_DELETE_MULTIPLE, TEXT("%s"), pszNames);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);

      FreeString (pszNames);
      }
}


void Group_Delete_OnDestroy (HWND hDlg)
{
   LPASIDLIST pGroupList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);
   asc_AsidListFree (&pGroupList);
}


void Group_Delete_OnOK (HWND hDlg)
{
   LPASIDLIST pGroupList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);

   // Start a background task to do all the work.
   //
   LPASIDLIST pTask;
   asc_AsidListCopy (&pTask, &pGroupList);
   StartTask (taskGROUP_DELETE, NULL, pTask);
}

