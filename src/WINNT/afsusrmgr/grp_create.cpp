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
#include "grp_prop.h"
#include "grp_create.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   GROUPPROPINFO Advanced;
   } CREATEGROUPDLG, *LPCREATEGROUPDLG;

#define UID_AUTOSELECT  ((int)0)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Group_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Group_Create_OnInitDialog (HWND hDlg);
void Group_Create_OnNames (HWND hDlg);
void Group_Create_OnID (HWND hDlg);
void Group_Create_OnAdvanced (HWND hDlg);
BOOL Group_Create_OnOK (HWND hDlg);
void Group_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Group_SetDefaultCreateParams (LPGROUPPROPINFO lpp)
{
   memset (lpp, 0x00, sizeof(GROUPPROPINFO));
   lpp->aaStatus = aaANYONE;
   lpp->fStatus_Mixed = FALSE;
   lpp->aaGroups = aaANYONE;
   lpp->fGroups_Mixed = FALSE;
   lpp->aaMembers = aaANYONE;
   lpp->fMembers_Mixed = FALSE;
   lpp->aaAdd = aaGROUP_ONLY;
   lpp->fAdd_Mixed = FALSE;
   lpp->aaRemove = aaGROUP_ONLY;
   lpp->fRemove_Mixed = FALSE;
   lstrcpy (lpp->szOwner, TEXT(""));
   lpp->fOwner_Mixed = FALSE;
   lstrcpy (lpp->szCreator, TEXT(""));
   lpp->fCreator_Mixed = FALSE;
}


void Group_ShowCreate (HWND hParent)
{
   LPCREATEGROUPDLG lpp = New (CREATEGROUPDLG);
   memset (lpp, 0x00, sizeof(CREATEGROUPDLG));
   memcpy (&lpp->Advanced, &gr.CreateGroup, sizeof(GROUPPROPINFO));
   lpp->Advanced.pMembers = NULL;
   lpp->Advanced.pGroupsOwner = NULL;

   (void)ModalDialogParam (IDD_NEWGROUP, hParent, (DLGPROC)Group_Create_DlgProc, (LPARAM)lpp);

   if (lpp->Advanced.pMembers)
      asc_AsidListFree (&lpp->Advanced.pMembers);
   if (lpp->Advanced.pGroupsOwner)
      asc_AsidListFree (&lpp->Advanced.pGroupsOwner);
   Delete (lpp);
}


BOOL CALLBACK Group_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_NEWGROUP, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         Group_Create_OnInitDialog (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskOBJECT_GET)
               Group_Create_OnEndTask_ObjectGet (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (Group_Create_OnOK (hDlg))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_ADVANCED:
               Group_Create_OnAdvanced (hDlg);
               break;

            case IDC_NEWGROUP_NAME:
               Group_Create_OnNames (hDlg);
               break;

            case IDC_NEWGROUP_ID_AUTO:
            case IDC_NEWGROUP_ID_MANUAL:
               Group_Create_OnID (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Group_Create_OnInitDialog (HWND hDlg)
{
   LPCREATEGROUPDLG lpp = (LPCREATEGROUPDLG)GetWindowLongPtr (hDlg, DWLP_USER);

   // Fix the title of the dialog
   //
   ULONG status;
   TCHAR szName[ cchNAME ];
   asc_CellNameGet_Fast (g.idClient, g.idCell, szName, &status);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_NEWGROUP_TITLE, szText, cchRESOURCE);

   LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_NEWGROUP_TITLE, pszText);
   FreeString (pszText);

   // Attach a spinner to the ID control
   //
   CheckDlgButton (hDlg, IDC_NEWGROUP_ID_AUTO, TRUE);
   CreateSpinner (GetDlgItem (hDlg, IDC_NEWGROUP_ID), 10, TRUE, -0x7FFFFFFF, -1, -1);
   Group_Create_OnID (hDlg);

   StartTask (taskOBJECT_GET, hDlg, (PVOID)(g.idCell));
}


void Group_Create_OnNames (HWND hDlg)
{
   TCHAR szSeparators[ cchRESOURCE ];
   GetString (szSeparators, IDS_SEPARATORS);
   lstrcat (szSeparators, TEXT(" \t"));

   LPTSTR pszNames = GetEditText (GetDlgItem (hDlg, IDC_NEWGROUP_NAME));
   EnableWindow (GetDlgItem (hDlg, IDOK), (pszNames && *pszNames));

   BOOL fMultiple = FALSE;
   for (LPTSTR psz = pszNames; !fMultiple && psz && *psz; ++psz)
      {
      if (lstrchr (szSeparators, *psz))
         fMultiple = TRUE;
      }
   FreeString (pszNames);

   EnableWindow (GetDlgItem (hDlg, IDC_NEWGROUP_ID_AUTO), !fMultiple);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWGROUP_ID_MANUAL), !fMultiple);
   if (fMultiple)
      {
      CheckDlgButton (hDlg, IDC_NEWGROUP_ID_AUTO, TRUE);
      CheckDlgButton (hDlg, IDC_NEWGROUP_ID_MANUAL, FALSE);
      Group_Create_OnID (hDlg);
      }
}


void Group_Create_OnID (HWND hDlg)
{
   BOOL fEnable = IsDlgButtonChecked (hDlg, IDC_NEWGROUP_ID_MANUAL);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWGROUP_ID), fEnable);
}


void Group_Create_OnAdvanced (HWND hDlg)
{
   LPCREATEGROUPDLG lpp = (LPCREATEGROUPDLG)GetWindowLongPtr (hDlg, DWLP_USER);
   lpp->Advanced.pGroupList = NULL;
   lpp->Advanced.fDeleteMeOnClose = FALSE;
   lpp->Advanced.fShowModal = TRUE;
   lpp->Advanced.hParent = hDlg;
   lpp->Advanced.fApplyGeneral = FALSE;
   Group_ShowProperties (&lpp->Advanced, gptMEMBERS);
}


BOOL Group_Create_OnOK (HWND hDlg)
{
   LPCREATEGROUPDLG lpp = (LPCREATEGROUPDLG)GetWindowLongPtr (hDlg, DWLP_USER);

   // Start a background task to do all the work.
   //
   LPGROUP_CREATE_PARAMS pTask = New (GROUP_CREATE_PARAMS);
   memset (pTask, 0x00, sizeof(GROUP_CREATE_PARAMS));

   if (IsDlgButtonChecked (hDlg, IDC_NEWGROUP_ID_AUTO))
      pTask->idGroup = UID_AUTOSELECT;
   else // (IsDlgButtonChecked (hDlg, IDC_NEWGROUP_ID_MANUAL))
      pTask->idGroup = SP_GetPos (GetDlgItem (hDlg, IDC_NEWGROUP_ID));

   lstrcpy (pTask->Properties.szOwner, lpp->Advanced.szOwner);

   pTask->Properties.aaListStatus = lpp->Advanced.aaStatus;
   pTask->Properties.aaListGroupsOwned = lpp->Advanced.aaGroups;
   pTask->Properties.aaListMembers = lpp->Advanced.aaMembers;
   pTask->Properties.aaAddMember = lpp->Advanced.aaAdd;
   pTask->Properties.aaDeleteMember = lpp->Advanced.aaRemove;

   if (lpp->Advanced.pMembers)
      asc_AsidListCopy (&pTask->pMembers, &lpp->Advanced.pMembers);
   else
      pTask->pMembers = NULL;

   if (lpp->Advanced.pGroupsOwner)
      asc_AsidListCopy (&pTask->pGroupsOwner, &lpp->Advanced.pGroupsOwner);
   else
      pTask->pGroupsOwner = NULL;

   // Crack the specified list of user names into a multi-string
   //
   TCHAR szSeparators[ cchRESOURCE ];
   GetString (szSeparators, IDS_SEPARATORS);
   lstrcat (szSeparators, TEXT(" \t"));

   LPTSTR pszNames = GetEditText (GetDlgItem (hDlg, IDC_NEWGROUP_NAME));
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
   StartTask (taskGROUP_CREATE, NULL, pTask);

   // Store these creation parameters as the new defaults
   //
   memcpy (&gr.CreateGroup, &lpp->Advanced, sizeof(GROUPPROPINFO));
   return TRUE;
}


void Group_Create_OnEndTask_ObjectGet (HWND hDlg, LPTASKPACKET ptp)
{
   if (ptp->rc)
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_NEWGROUP_ID_AUTO, szText, cchRESOURCE);

      LPTSTR pszText = FormatString (TEXT("%1 (%2)"), TEXT("%s%ld"), szText, TASKDATA(ptp)->Properties.u.CellProperties.idGroupMax-1);
      SetDlgItemText (hDlg, IDC_NEWGROUP_ID_AUTO, pszText);
      FreeString (pszText);

      if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_NEWGROUP_ID)))
         SP_SetPos (GetDlgItem (hDlg, IDC_NEWGROUP_ID), TASKDATA(ptp)->Properties.u.CellProperties.idGroupMax-1);
      }
}

