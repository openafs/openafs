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
#include "usr_delete.h"
#include "usr_col.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK User_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void User_Delete_OnInitDialog (HWND hDlg);
void User_Delete_OnDestroy (HWND hDlg);
void User_Delete_OnCheck (HWND hDlg);
void User_Delete_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void User_ShowDelete (LPASIDLIST pUserList)
{
   ModalDialogParam (IDD_USER_DELETE, g.hMain, (DLGPROC)User_Delete_DlgProc, (LPARAM)pUserList);
}


BOOL CALLBACK User_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_USER_DELETE, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         User_Delete_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         User_Delete_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               User_Delete_OnOK (hDlg);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_DELETE_KAS:
            case IDC_DELETE_PTS:
               User_Delete_OnCheck (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void User_Delete_OnInitDialog (HWND hDlg)
{
   LPASIDLIST pUserList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);

   // Fix the title of the dialog
   //
   if (pUserList->cEntries == 1)
      {
      TCHAR szName[ cchNAME ];
      User_GetDisplayName (szName, pUserList->aEntries[0].idObject);

      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_DELETE_TITLE, szText, cchRESOURCE);

      LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);
      }
   else
      {
      LPTSTR pszNames = CreateNameList (pUserList);

      LPTSTR pszText = FormatString (IDS_USER_DELETE_MULTIPLE, TEXT("%s"), pszNames);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);

      FreeString (pszNames);
      }

   // Check the checkboxes
   //
   CheckDlgButton (hDlg, IDC_DELETE_KAS, BST_CHECKED);
   CheckDlgButton (hDlg, IDC_DELETE_PTS, BST_CHECKED);
}


void User_Delete_OnDestroy (HWND hDlg)
{
   LPASIDLIST pUserList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);
   asc_AsidListFree (&pUserList);
}


void User_Delete_OnCheck (HWND hDlg)
{
   BOOL fEnable = FALSE;
   if (IsDlgButtonChecked (hDlg, IDC_DELETE_KAS))
      fEnable = TRUE;
   if (IsDlgButtonChecked (hDlg, IDC_DELETE_PTS))
      fEnable = TRUE;
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void User_Delete_OnOK (HWND hDlg)
{
   LPASIDLIST pUserList = (LPASIDLIST)GetWindowLongPtr (hDlg, DWLP_USER);

   // Start a background task to do all the work.
   //
   LPUSER_DELETE_PARAMS pTask = New (USER_DELETE_PARAMS);
   memset (pTask, 0x00, sizeof(USER_DELETE_PARAMS));
   pTask->fDeleteKAS = IsDlgButtonChecked (hDlg, IDC_DELETE_KAS);
   pTask->fDeletePTS = IsDlgButtonChecked (hDlg, IDC_DELETE_PTS);
   asc_AsidListCopy (&pTask->pUserList, &pUserList);
   StartTask (taskUSER_DELETE, NULL, pTask);
}

