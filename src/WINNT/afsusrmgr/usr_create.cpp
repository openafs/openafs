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
#include "usr_prop.h"
#include "usr_create.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szPassword[ cchRESOURCE ];
   int uid;
   USERPROPINFO Advanced;
   } CREATEUSERDLG, *LPCREATEUSERDLG;

#define UID_AUTOSELECT  ((int)0)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK User_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void User_Create_OnInitDialog (HWND hDlg);
void User_Create_OnNames (HWND hDlg);
void User_Create_OnID (HWND hDlg);
void User_Create_OnAdvanced (HWND hDlg);
BOOL User_Create_OnOK (HWND hDlg);
void User_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void User_SetDefaultCreateParams (LPUSERPROPINFO lpp)
{
   memset (lpp, 0x00, sizeof(USERPROPINFO));
   lpp->fSeal = FALSE;
   lpp->fAdmin = FALSE;
   lpp->fGrantTickets = TRUE;
   lpp->csecLifetime = csecTICKETLIFETIME_DEFAULT;
   lpp->cGroupQuota = cGROUPQUOTA_DEFAULT;
   lpp->fExpires = FALSE;
   lpp->aaStatus = aaANYONE;
   lpp->aaOwned = aaANYONE;
   lpp->aaMember = aaANYONE;
   lpp->fCreateKAS = TRUE;
   lpp->fCreatePTS = TRUE;
   lpp->fCanChangePw = TRUE;
   lpp->fCanReusePw = FALSE;
   lpp->cdayPwExpires = 0;
   lpp->cFailLock = 0;
   lpp->csecFailLock = 0;
}


void User_ShowCreate (HWND hParent)
{
   LPCREATEUSERDLG lpp = New (CREATEUSERDLG);
   memset (lpp, 0x00, sizeof(CREATEUSERDLG));
   memcpy (&lpp->Advanced, &gr.CreateUser, sizeof(USERPROPINFO));
   lpp->uid = UID_AUTOSELECT;
   lpp->Advanced.pGroupsMember = NULL;
   lpp->Advanced.pGroupsOwner = NULL;

   (void)ModalDialogParam (IDD_NEWUSER, hParent, (DLGPROC)User_Create_DlgProc, (LPARAM)lpp);

   if (lpp->Advanced.pGroupsMember)
      asc_AsidListFree (&lpp->Advanced.pGroupsMember);
   if (lpp->Advanced.pGroupsOwner)
      asc_AsidListFree (&lpp->Advanced.pGroupsOwner);
   Delete (lpp);
}


BOOL CALLBACK User_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_NEWUSER, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         User_Create_OnInitDialog (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskOBJECT_GET)
               User_Create_OnEndTask_ObjectGet (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (User_Create_OnOK (hDlg))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_ADVANCED:
               User_Create_OnAdvanced (hDlg);
               break;

            case IDC_NEWUSER_NAME:
               User_Create_OnNames (hDlg);
               break;

            case IDC_NEWUSER_ID_AUTO:
            case IDC_NEWUSER_ID_MANUAL:
               User_Create_OnID (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void User_Create_OnInitDialog (HWND hDlg)
{
   LPCREATEUSERDLG lpp = (LPCREATEUSERDLG)GetWindowLongPtr (hDlg, DWLP_USER);

   // Fix the title of the dialog
   //
   ULONG status;
   TCHAR szName[ cchNAME ];
   asc_CellNameGet_Fast (g.idClient, g.idCell, szName, &status);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_NEWUSER_TITLE, szText, cchRESOURCE);

   LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_NEWUSER_TITLE, pszText);
   FreeString (pszText);

   // Attach a spinner to the ID control
   //
   CheckDlgButton (hDlg, IDC_NEWUSER_ID_AUTO, TRUE);
   CreateSpinner (GetDlgItem (hDlg, IDC_NEWUSER_ID), 10, FALSE, 1, 1, (int)-1);
   User_Create_OnID (hDlg);

   StartTask (taskOBJECT_GET, hDlg, (PVOID)(g.idCell));
}


void User_Create_OnNames (HWND hDlg)
{
   TCHAR szSeparators[ cchRESOURCE ];
   GetString (szSeparators, IDS_SEPARATORS);
   lstrcat (szSeparators, TEXT(" \t"));

   LPTSTR pszNames = GetEditText (GetDlgItem (hDlg, IDC_NEWUSER_NAME));
   EnableWindow (GetDlgItem (hDlg, IDOK), (pszNames && *pszNames));

   BOOL fMultiple = FALSE;
   for (LPTSTR psz = pszNames; !fMultiple && psz && *psz; ++psz)
      {
      if (lstrchr (szSeparators, *psz))
         fMultiple = TRUE;
      }
   FreeString (pszNames);

   EnableWindow (GetDlgItem (hDlg, IDC_NEWUSER_ID_AUTO), !fMultiple);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWUSER_ID_MANUAL), !fMultiple);
   if (fMultiple)
      {
      CheckDlgButton (hDlg, IDC_NEWUSER_ID_AUTO, TRUE);
      CheckDlgButton (hDlg, IDC_NEWUSER_ID_MANUAL, FALSE);
      User_Create_OnID (hDlg);
      }
}


void User_Create_OnID (HWND hDlg)
{
   BOOL fEnable = IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_MANUAL);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWUSER_ID), fEnable);
}


void User_Create_OnAdvanced (HWND hDlg)
{
   LPCREATEUSERDLG lpp = (LPCREATEUSERDLG)GetWindowLongPtr (hDlg, DWLP_USER);
   lpp->Advanced.pUserList = NULL;
   lpp->Advanced.fDeleteMeOnClose = FALSE;
   lpp->Advanced.fShowModal = TRUE;
   lpp->Advanced.hParent = hDlg;
   lpp->Advanced.fMachine = FALSE;
   lpp->Advanced.fApplyGeneral = FALSE;
   lpp->Advanced.fApplyAdvanced = FALSE;
   User_ShowProperties (&lpp->Advanced, uptMEMBERSHIP);
}


BOOL User_Create_OnOK (HWND hDlg)
{
   LPCREATEUSERDLG lpp = (LPCREATEUSERDLG)GetWindowLongPtr (hDlg, DWLP_USER);

   // First do a little validation of the dialog's entries
   //
   TCHAR szPw1[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_NEWUSER_PW1, szPw1, cchRESOURCE);
   TCHAR szPw2[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_NEWUSER_PW2, szPw2, cchRESOURCE);

   if (!szPw1[0])
      {
      ErrorDialog (ERROR_INVALID_PASSWORDNAME, IDS_ERROR_NO_PASSWORD_GIVEN);
      return FALSE;
      }
   if (lstrcmp (szPw1, szPw2))
      {
      ErrorDialog (ERROR_INVALID_PASSWORDNAME, IDS_ERROR_MISMATCH_PASSWORD_GIVEN);
      return FALSE;
      }

   // Then start a background task to do all this work.
   //
   LPUSER_CREATE_PARAMS pTask = New (USER_CREATE_PARAMS);
   memset (pTask, 0x00, sizeof(USER_CREATE_PARAMS));

   lstrcpy (pTask->szPassword, szPw1);

   if (IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_AUTO))
      pTask->idUser = UID_AUTOSELECT;
   else // (IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_MANUAL))
      pTask->idUser = SP_GetPos (GetDlgItem (hDlg, IDC_NEWUSER_ID));

   pTask->Properties.fIsAdmin = lpp->Advanced.fAdmin;
   pTask->Properties.fCanGetTickets = lpp->Advanced.fGrantTickets;
   pTask->Properties.fEncrypt = lpp->Advanced.fSeal;
   pTask->Properties.fCanChangePassword = lpp->Advanced.fCanChangePw;
   pTask->Properties.fCanReusePasswords = lpp->Advanced.fCanReusePw;
   memcpy (&pTask->Properties.timeExpires, &lpp->Advanced.stExpires, sizeof(SYSTEMTIME));
   pTask->Properties.cdayPwExpire = lpp->Advanced.cdayPwExpires;
   pTask->Properties.csecTicketLifetime = lpp->Advanced.csecLifetime;
   pTask->Properties.cFailLogin = lpp->Advanced.cFailLock;
   pTask->Properties.csecFailLoginLock = lpp->Advanced.csecFailLock;
   pTask->Properties.cgroupCreationQuota = lpp->Advanced.cGroupQuota;
   pTask->Properties.aaListStatus = lpp->Advanced.aaStatus;
   pTask->Properties.aaGroupsOwned = lpp->Advanced.aaOwned;
   pTask->Properties.aaMembership = lpp->Advanced.aaMember;
   pTask->fCreateKAS = lpp->Advanced.fCreateKAS;
   pTask->fCreatePTS = lpp->Advanced.fCreatePTS;

   if (lpp->Advanced.pGroupsMember)
      asc_AsidListCopy (&pTask->pGroupsMember, &lpp->Advanced.pGroupsMember);
   else
      pTask->pGroupsMember = NULL;

   if (lpp->Advanced.pGroupsOwner)
      asc_AsidListCopy (&pTask->pGroupsOwner, &lpp->Advanced.pGroupsOwner);
   else
      pTask->pGroupsOwner = NULL;

   // Crack the specified list of user names into a multi-string
   //
   TCHAR szSeparators[ cchRESOURCE ];
   GetString (szSeparators, IDS_SEPARATORS);
   lstrcat (szSeparators, TEXT(" \t"));

   LPTSTR pszNames = GetEditText (GetDlgItem (hDlg, IDC_NEWUSER_NAME));
   LPCTSTR pszStart = pszNames;
   while (lstrchr (szSeparators, *pszStart))
      ++pszStart;

   while (*pszStart)
      {
      // Find the first non-name character
      //
      LPCTSTR pszEnd = pszStart;
      while (*pszEnd && !lstrchr(szSeparators, *pszEnd))
         ++pszEnd;

      // Copy off this particular name
      //
      TCHAR szName[ cchNAME ];
      lstrcpy (szName, pszStart);
      szName[ pszEnd - pszStart ] = TEXT('\0');

      if (szName[0])
         FormatMultiString  (&pTask->mszNames, FALSE, TEXT("%1"), TEXT("%s"), szName);

      // Find the next valid-name character
      //
      pszStart = pszEnd;
      while (lstrchr(szSeparators, *pszStart))
         ++pszStart;
      }
   FreeString (pszNames);

   // Do the real work of creating the user(s)
   //
   StartTask (taskUSER_CREATE, NULL, pTask);

   // Store these creation parameters as the new defaults
   //
   memcpy (&gr.CreateUser, &lpp->Advanced, sizeof(USERPROPINFO));
   return TRUE;
}


void User_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp)
{
   if (ptp->rc)
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_NEWUSER_ID_AUTO, szText, cchRESOURCE);

      LPTSTR pszText = FormatString (TEXT("%1 (%2)"), TEXT("%s%ld"), szText, TASKDATA(ptp)->Properties.u.CellProperties.idUserMax+1);
      SetDlgItemText (hDlg, IDC_NEWUSER_ID_AUTO, pszText);
      FreeString (pszText);

      if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_NEWUSER_ID)))
         SP_SetPos (GetDlgItem (hDlg, IDC_NEWUSER_ID), TASKDATA(ptp)->Properties.u.CellProperties.idUserMax+1);
      }
}

