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
#include "mch_create.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   int uid;
   USERPROPINFO Advanced;
   } CREATEMACHINEDLG, *LPCREATEMACHINEDLG;

#define UID_AUTOSELECT  ((int)0)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Machine_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Machine_Create_OnInitDialog (HWND hDlg);
void Machine_Create_OnNames (HWND hDlg);
void Machine_Create_OnID (HWND hDlg);
void Machine_Create_OnAdvanced (HWND hDlg);
BOOL Machine_Create_OnOK (HWND hDlg);
void Machine_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Machine_SetDefaultCreateParams (LPUSERPROPINFO lpp)
{
   memset (lpp, 0x00, sizeof(USERPROPINFO));
   lpp->fSeal = FALSE;
   lpp->fAdmin = FALSE;
   lpp->fGrantTickets = TRUE;
   lpp->fMachine = TRUE;
   lpp->cGroupQuota = cGROUPQUOTA_DEFAULT;
   lpp->aaStatus = aaANYONE;
   lpp->aaOwned = aaANYONE;
   lpp->aaMember = aaANYONE;
   lpp->fCreateKAS = FALSE;
   lpp->fCreatePTS = TRUE;

   // All the KAS stuff is zero
   //
   lpp->csecLifetime = 0;
   lpp->fExpires = FALSE;
   lpp->fCanChangePw = 0;
   lpp->fCanReusePw = FALSE;
   lpp->cdayPwExpires = 0;
   lpp->cFailLock = 0;
   lpp->csecFailLock = 0;
}


void Machine_ShowCreate (HWND hParent)
{
   LPCREATEMACHINEDLG lpp = New (CREATEMACHINEDLG);
   memset (lpp, 0x00, sizeof(CREATEMACHINEDLG));
   memcpy (&lpp->Advanced, &gr.CreateMachine, sizeof(USERPROPINFO));
   lpp->uid = UID_AUTOSELECT;
   lpp->Advanced.pGroupsMember = NULL;
   lpp->Advanced.pGroupsOwner = NULL;

   (void)ModalDialogParam (IDD_NEWMACHINE, hParent, (DLGPROC)Machine_Create_DlgProc, (LPARAM)lpp);

   if (lpp->Advanced.pGroupsMember)
      asc_AsidListFree (&lpp->Advanced.pGroupsMember);
   if (lpp->Advanced.pGroupsOwner)
      asc_AsidListFree (&lpp->Advanced.pGroupsOwner);
   Delete (lpp);
}


BOOL CALLBACK Machine_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_NEWMACHINE, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         Machine_Create_OnInitDialog (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskOBJECT_GET)
               Machine_Create_OnEndTask_ObjectGet (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (Machine_Create_OnOK (hDlg))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_ADVANCED:
               Machine_Create_OnAdvanced (hDlg);
               break;

            case IDC_NEWUSER_NAME:
               Machine_Create_OnNames (hDlg);
               break;

            case IDC_NEWUSER_ID_AUTO:
            case IDC_NEWUSER_ID_MANUAL:
               Machine_Create_OnID (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Machine_Create_OnInitDialog (HWND hDlg)
{
   LPCREATEMACHINEDLG lpp = (LPCREATEMACHINEDLG)GetWindowLongPtr (hDlg, DWLP_USER);

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
   Machine_Create_OnID (hDlg);

   StartTask (taskOBJECT_GET, hDlg, (PVOID)(g.idCell));
}


void Machine_Create_OnNames (HWND hDlg)
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
      Machine_Create_OnID (hDlg);
      }
}


void Machine_Create_OnID (HWND hDlg)
{
   BOOL fEnable = IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_MANUAL);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWUSER_ID), fEnable);
}


void Machine_Create_OnAdvanced (HWND hDlg)
{
   LPCREATEMACHINEDLG lpp = (LPCREATEMACHINEDLG)GetWindowLongPtr (hDlg, DWLP_USER);
   lpp->Advanced.pUserList = NULL;
   lpp->Advanced.fDeleteMeOnClose = FALSE;
   lpp->Advanced.fShowModal = TRUE;
   lpp->Advanced.hParent = hDlg;
   lpp->Advanced.fMachine = TRUE;
   lpp->Advanced.fApplyGeneral = FALSE;
   lpp->Advanced.fApplyAdvanced = FALSE;
   User_ShowProperties (&lpp->Advanced, uptMEMBERSHIP);
}


BOOL Machine_Create_OnOK (HWND hDlg)
{
   LPCREATEMACHINEDLG lpp = (LPCREATEMACHINEDLG)GetWindowLongPtr (hDlg, DWLP_USER);

   // Start a background task to do all this work.
   //
   LPUSER_CREATE_PARAMS pTask = New (USER_CREATE_PARAMS);
   memset (pTask, 0x00, sizeof(USER_CREATE_PARAMS));

   lstrcpy (pTask->szPassword, TEXT(""));

   if (IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_AUTO))
      pTask->idUser = UID_AUTOSELECT;
   else // (IsDlgButtonChecked (hDlg, IDC_NEWUSER_ID_MANUAL))
      pTask->idUser = SP_GetPos (GetDlgItem (hDlg, IDC_NEWUSER_ID));

   pTask->Properties.cgroupCreationQuota = lpp->Advanced.cGroupQuota;
   pTask->Properties.aaListStatus = lpp->Advanced.aaStatus;
   pTask->Properties.aaGroupsOwned = lpp->Advanced.aaOwned;
   pTask->Properties.aaMembership = lpp->Advanced.aaMember;

   // All the KAS stuff is zero
   //
   pTask->Properties.fIsAdmin = FALSE;
   pTask->Properties.fCanGetTickets = FALSE;
   pTask->Properties.fEncrypt = FALSE;
   pTask->Properties.fCanChangePassword = FALSE;
   pTask->Properties.fCanReusePasswords = FALSE;
   pTask->Properties.cdayPwExpire = 0;
   pTask->Properties.csecTicketLifetime = 0;
   pTask->Properties.cFailLogin = 0;
   pTask->Properties.csecFailLoginLock = 0;
   memset (&pTask->Properties.timeExpires, 0x00, sizeof(SYSTEMTIME));

   // Copy over any lists-of-groups
   //
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
   pTask->fCreateKAS = FALSE;
   pTask->fCreatePTS = TRUE;
   StartTask (taskUSER_CREATE, NULL, pTask);

   // Store these creation parameters as the new defaults
   //
   memcpy (&gr.CreateMachine, &lpp->Advanced, sizeof(USERPROPINFO));
   return TRUE;
}


void Machine_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp)
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

