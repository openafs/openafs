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
#include "usr_cpw.h"
#include "usr_col.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK User_Password_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void User_Password_OnInitDialog (HWND hDlg);
BOOL User_Password_OnOK (HWND hDlg);
void User_Password_OnSelectVer (HWND hDlg);
void User_Password_OnSelectType (HWND hDlg);
void User_Password_OnType (HWND hDlg);
void User_Password_OnRandom (HWND hDlg);
void User_Password_OnEndTask_Random (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void User_ShowChangePassword (HWND hParent, ASID idUser)
{
   ModalDialogParam (IDD_USER_PASSWORD, hParent, (DLGPROC)User_Password_DlgProc, (LPARAM)idUser);
}


BOOL CALLBACK User_Password_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_USER_PASSWORD, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLong (hDlg, DWL_USER, lp);
         User_Password_OnInitDialog (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskGET_RANDOM_KEY)
               User_Password_OnEndTask_Random (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (User_Password_OnOK (hDlg))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_CPW_VERSION_AUTO:
            case IDC_CPW_VERSION_MANUAL:
               User_Password_OnSelectVer (hDlg);
               break;

            case IDC_CPW_BYSTRING:
            case IDC_CPW_BYDATA:
               User_Password_OnSelectType (hDlg);
               break;

            case IDC_CPW_STRING:
            case IDC_CPW_DATA:
               User_Password_OnType (hDlg);
               break;

            case IDC_CPW_RANDOM:
               User_Password_OnRandom (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void User_Password_OnInitDialog (HWND hDlg)
{
   ASID idUser = (ASID)GetWindowLong (hDlg, DWL_USER);

   // Get the current properties for this user
   //
   ULONG status;
   TCHAR szName[ cchNAME ];
   User_GetDisplayName (szName, idUser);

   ASOBJPROP Properties;
   if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idUser, &Properties, &status))
      {
      ErrorDialog (status, IDS_ERROR_CANT_GET_USERPROP, TEXT("%s"), szName);
      EndDialog (hDlg, IDCANCEL);
      return;
      }

   // Fill in the text at the top of the dialog
   //
   TCHAR szOld[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CPW_TITLE, szOld, cchRESOURCE);

   LPTSTR pszText = FormatString (szOld, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_CPW_TITLE, pszText);
   FreeString (pszText);

   // Check the appropriate radio buttons, etc
   //
   CheckDlgButton (hDlg, IDC_CPW_VERSION_AUTO, TRUE);
   CheckDlgButton (hDlg, IDC_CPW_BYSTRING, TRUE);
   CreateSpinner (GetDlgItem (hDlg, IDC_CPW_VERSION), 10, FALSE, 1, 1+Properties.u.UserProperties.KASINFO.keyVersion, 255);
   User_Password_OnSelectType (hDlg);
   User_Password_OnSelectVer (hDlg);
}


void User_Password_OnSelectVer (HWND hDlg)
{
   EnableWindow (GetDlgItem (hDlg, IDC_CPW_VERSION), IsDlgButtonChecked (hDlg, IDC_CPW_VERSION_MANUAL));
}


void User_Password_OnSelectType (HWND hDlg)
{
   EnableWindow (GetDlgItem (hDlg, IDC_CPW_STRING), IsDlgButtonChecked (hDlg, IDC_CPW_BYSTRING));
   EnableWindow (GetDlgItem (hDlg, IDC_CPW_DATA),   IsDlgButtonChecked (hDlg, IDC_CPW_BYDATA));
   EnableWindow (GetDlgItem (hDlg, IDC_CPW_RANDOM), IsDlgButtonChecked (hDlg, IDC_CPW_BYDATA));
   User_Password_OnType (hDlg);
}


void User_Password_OnType (HWND hDlg)
{
   BOOL fEnable = FALSE;

   if (IsDlgButtonChecked (hDlg, IDC_CPW_BYSTRING))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_CPW_STRING, szKey, cchRESOURCE);
      if (szKey[0] != TEXT('\0'))
         fEnable = TRUE;
      }
   else // (IsDlgButtonChecked (hDlg, IDC_CPW_BYDATA))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_CPW_DATA, szKey, cchRESOURCE);

      BYTE key[ ENCRYPTIONKEYLENGTH ];
      if (ScanServerKey (key, szKey))
         fEnable = TRUE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void User_Password_OnRandom (HWND hDlg)
{
   StartTask (taskGET_RANDOM_KEY, hDlg, 0);
}


BOOL User_Password_OnOK (HWND hDlg)
{
   ASID idUser = (ASID)GetWindowLong (hDlg, DWL_USER);

   LPUSER_CPW_PARAMS lpp = New (USER_CPW_PARAMS);
   memset (lpp, 0x00, sizeof(USER_CPW_PARAMS));
   lpp->idUser = idUser;

   if (IsDlgButtonChecked (hDlg, IDC_CPW_VERSION_MANUAL))
      lpp->keyVersion = SP_GetPos (GetDlgItem (hDlg, IDC_CPW_VERSION));
   else // (IsDlgButtonChecked (hDlg, IDC_CPW_VERSION_AUTO))
      lpp->keyVersion = 0;

   if (IsDlgButtonChecked (hDlg, IDC_CPW_BYSTRING))
      {
      GetDlgItemText (hDlg, IDC_CPW_STRING, lpp->keyString, cchRESOURCE);
      }
   else // (IsDlgButtonChecked (hDlg, IDC_CPW_BYDATA))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_CPW_DATA, szKey, cchRESOURCE);

      if (!ScanServerKey (lpp->keyData, szKey))
         {
         Delete (lpp);
         return FALSE;
         }
      }

   StartTask (taskUSER_CPW, NULL, lpp);
   return TRUE;
}


void User_Password_OnEndTask_Random (HWND hDlg, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_GET_RANDOM_KEY);
      EnableWindow (GetDlgItem (hDlg, IDC_CPW_RANDOM), FALSE);
      }
   else
      {
      TCHAR szKey[ cchRESOURCE ];
      FormatServerKey (szKey, TASKDATA(ptp)->key);
      SetDlgItemText (hDlg, IDC_CPW_DATA, szKey);
      }
}

