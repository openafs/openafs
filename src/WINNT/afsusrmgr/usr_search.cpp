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
#include "usr_search.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Users_Search_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Users_Search_OnInitDialog (HWND hDlg);
void Users_Search_OnCheck (HWND hDlg);
void Users_Search_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Users_SetDefaultSearchParams (LPAFSADMSVR_SEARCH_PARAMS pSearchParams)
{
   memset (pSearchParams, 0x00, sizeof(AFSADMSVR_SEARCH_PARAMS));
   pSearchParams->SearchType = SEARCH_NO_LIMITATIONS;
}


void Users_ShowAdvancedSearch (HWND hParent)
{
   ModalDialog (IDD_SEARCH_USERS, hParent, (DLGPROC)Users_Search_DlgProc);
}


BOOL CALLBACK Users_Search_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SEARCH_USERS, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         Users_Search_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Users_Search_OnOK (hDlg);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_SEARCH_ALL:
            case IDC_SEARCH_EXPIRE:
            case IDC_SEARCH_PWEXPIRE:
               Users_Search_OnCheck (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Users_Search_OnInitDialog (HWND hDlg)
{
   CheckDlgButton (hDlg, IDC_SEARCH_ALL, (gr.SearchUsers.SearchType == SEARCH_NO_LIMITATIONS));
   CheckDlgButton (hDlg, IDC_SEARCH_EXPIRE, (gr.SearchUsers.SearchType == SEARCH_EXPIRES_BEFORE));
   CheckDlgButton (hDlg, IDC_SEARCH_PWEXPIRE, (gr.SearchUsers.SearchType == SEARCH_PASSWORD_EXPIRES_BEFORE));

   SYSTEMTIME stNow;
   GetSystemTime (&stNow);

   SYSTEMTIME stShow;
   memcpy (&stShow, ((gr.SearchUsers.SearchType == SEARCH_EXPIRES_BEFORE) ? &gr.SearchUsers.SearchTime : &stNow), sizeof(SYSTEMTIME));
   DA_SetDate (GetDlgItem (hDlg, IDC_SEARCH_EXPIRE_DATE), &stShow);

   memcpy (&stShow, ((gr.SearchUsers.SearchType == SEARCH_PASSWORD_EXPIRES_BEFORE) ? &gr.SearchUsers.SearchTime : &stNow), sizeof(SYSTEMTIME));
   DA_SetDate (GetDlgItem (hDlg, IDC_SEARCH_PWEXPIRE_DATE), &stShow);

   Users_Search_OnCheck (hDlg);
}


void Users_Search_OnOK (HWND hDlg)
{
   AFSADMSVR_SEARCH_PARAMS OldSearchType;
   memcpy (&OldSearchType, &gr.SearchUsers, sizeof(AFSADMSVR_SEARCH_PARAMS));

   if (IsDlgButtonChecked (hDlg, IDC_SEARCH_EXPIRE))
      {
      gr.SearchUsers.SearchType = SEARCH_EXPIRES_BEFORE;
      DA_GetDate (GetDlgItem (hDlg, IDC_SEARCH_EXPIRE_DATE), &gr.SearchUsers.SearchTime);
      }
   else if (IsDlgButtonChecked (hDlg, IDC_SEARCH_PWEXPIRE))
      {
      gr.SearchUsers.SearchType = SEARCH_PASSWORD_EXPIRES_BEFORE;
      DA_GetDate (GetDlgItem (hDlg, IDC_SEARCH_PWEXPIRE_DATE), &gr.SearchUsers.SearchTime);
      }
   else // (IsDlgButtonChecked (hDlg, IDC_SEARCH_ALL))
      {
      gr.SearchUsers.SearchType = SEARCH_NO_LIMITATIONS;
      }

   // If the user changed any search parameters, refresh the display
   //
   if (memcmp (&OldSearchType, &gr.SearchUsers, sizeof(AFSADMSVR_SEARCH_PARAMS)))
      {
      Display_PopulateList();
      }
}


void Users_Search_OnCheck (HWND hDlg)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SEARCH_EXPIRE_DATE), IsDlgButtonChecked (hDlg, IDC_SEARCH_EXPIRE));
   EnableWindow (GetDlgItem (hDlg, IDC_SEARCH_PWEXPIRE_DATE), IsDlgButtonChecked (hDlg, IDC_SEARCH_PWEXPIRE));
}

