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
#include "grp_rename.h"
#include "browse.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Group_Rename_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Group_Rename_OnInitDialog (HWND hDlg);
void Group_Rename_OnDestroy (HWND hDlg);
void Group_Rename_OnNewName (HWND hDlg);
void Group_Rename_OnChangeOwner (HWND hDlg);
void Group_Rename_OnOK (HWND hDlg);
void Group_Rename_UpdateDialog (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Group_ShowRename (HWND hParent, ASID idGroup)
{
   ModalDialogParam (IDD_GROUP_RENAME, hParent, (DLGPROC)Group_Rename_DlgProc, (LPARAM)idGroup);
}


BOOL CALLBACK Group_Rename_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_GROUP_RENAME, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLong (hDlg, DWL_USER, lp);
         Group_Rename_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         Group_Rename_OnDestroy (hDlg);
         break;

      case WM_ASC_NOTIFY_OBJECT:
         Group_Rename_UpdateDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Group_Rename_OnOK (hDlg);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_RENAME_NEWNAME:
               Group_Rename_OnNewName (hDlg);
               break;

            case IDC_RENAME_CHOWN:
               Group_Rename_OnChangeOwner (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Group_Rename_OnInitDialog (HWND hDlg)
{
   ASID idGroup = (ASID)GetWindowLong (hDlg, DWL_USER);

   // Indicate we want to know if anything changes with this group
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   asc_AsidListCreate (&pTask->pAsidList);
   asc_AsidListAddEntry (&pTask->pAsidList, idGroup, 0);
   StartTask (taskOBJECT_LISTEN, NULL, pTask);

   // Update the dialog's information
   //
   SetDlgItemText (hDlg, IDC_RENAME_NEWNAME, TEXT(""));
   Group_Rename_UpdateDialog (hDlg);
}


void Group_Rename_OnDestroy (HWND hDlg)
{
   // Indicate we no longer care if anything changes with this group
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   StartTask (taskOBJECT_LISTEN, NULL, pTask);
}


void Group_Rename_OnNewName (HWND hDlg)
{
   TCHAR szOldName[ cchNAME ];
   GetDlgItemText (hDlg, IDC_RENAME_OLDNAME, szOldName, cchNAME);

   TCHAR szNewName[ cchNAME ];
   GetDlgItemText (hDlg, IDC_RENAME_NEWNAME, szNewName, cchNAME);

   BOOL fEnable = TRUE;
   if (!szNewName[0])
      fEnable = FALSE;
   if (!lstrcmp (szOldName, szNewName))
      fEnable = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Group_Rename_OnChangeOwner (HWND hDlg)
{
   ASID idGroup = (ASID)GetWindowLong (hDlg, DWL_USER);

   BROWSE_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.hParent = hDlg;
   pp.iddForHelp = IDD_BROWSE_OWNER;
   pp.idsTitle = IDS_GROUP_CHANGEOWNER_TITLE;
   pp.idsPrompt = IDS_GROUP_CHANGEOWNER_PROMPT;
   pp.idsCheck = 0;
   pp.TypeToShow = (ASOBJTYPE)( (ULONG)TYPE_USER | (ULONG)TYPE_GROUP );
   pp.fAllowMultiple = FALSE;

   ULONG status;
   ASOBJPROP Properties;
   if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idGroup, &Properties, &status))
      {
      lstrcpy (pp.szName, Properties.u.GroupProperties.szOwner);

      if (ShowBrowseDialog (&pp))
         {
         LPGROUP_CHANGE_PARAMS pTask = New (GROUP_CHANGE_PARAMS);
         pTask->idGroup = idGroup;
         pTask->NewProperties.aaListStatus = Properties.u.GroupProperties.aaListStatus;
         pTask->NewProperties.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
         pTask->NewProperties.aaListMembers = Properties.u.GroupProperties.aaListMembers;
         pTask->NewProperties.aaAddMember = Properties.u.GroupProperties.aaAddMember;
         pTask->NewProperties.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;
         lstrcpy (pTask->NewProperties.szOwner, pp.szName);
         StartTask (taskGROUP_CHANGE, NULL, pTask);
         }
      }

   if (pp.pObjectsSelected)
      asc_AsidListFree (&pp.pObjectsSelected);
}


void Group_Rename_OnOK (HWND hDlg)
{
   ASID idGroup = (ASID)GetWindowLong (hDlg, DWL_USER);

   LPGROUP_RENAME_PARAMS lpp = New (GROUP_RENAME_PARAMS);
   memset (lpp, 0x00, sizeof(GROUP_RENAME_PARAMS));
   lpp->idGroup = idGroup;
   GetDlgItemText (hDlg, IDC_RENAME_NEWNAME, lpp->szNewName, cchNAME);

   StartTask (taskGROUP_RENAME, NULL, lpp);
}


void Group_Rename_UpdateDialog (HWND hDlg)
{
   ASID idGroup = (ASID)GetWindowLong (hDlg, DWL_USER);

   // Get the current properties for this group
   //
   ULONG status;
   TCHAR szName[ cchNAME ];
   asc_ObjectNameGet_Fast (g.idClient, g.idCell, idGroup, szName, &status);

   ASOBJPROP Properties;
   if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idGroup, &Properties, &status))
      GetString (Properties.u.GroupProperties.szOwner, IDS_UNKNOWN_NAME);
   SetDlgItemText (hDlg, IDC_RENAME_OWNER, Properties.u.GroupProperties.szOwner);

   // Fill in the text at the top of the dialog
   //
   LPTSTR pszText = FormatString (IDS_RENAME_TITLE, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_RENAME_TITLE, pszText);
   FreeString (pszText);

   // Prepare the New Name control and disable the OK button
   //
   SetDlgItemText (hDlg, IDC_RENAME_OLDNAME, szName);
   Group_Rename_OnNewName (hDlg);
}

