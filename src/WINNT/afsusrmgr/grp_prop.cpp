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
#include "usr_col.h"
#include "winlist.h"
#include "browse.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define GWD_ASIDLIST_MEMBER   TEXT("AsidList - Members")
#define GWD_ASIDLIST_OWNER    TEXT("AsidList - Owned")


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK GroupProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void GroupProp_General_OnInitDialog (HWND hDlg);
void GroupProp_General_OnDestroy (HWND hDlg);
void GroupProp_General_OnApply (HWND hDlg);
void GroupProp_General_OnBrowse (HWND hDlg);
void GroupProp_General_UpdateDialog (HWND hDlg);
void GroupProp_General_UpdateDialog_Perm (HWND hDlg, int idc, ACCOUNTACCESS aa, BOOL fMixed);

BOOL CALLBACK GroupProp_Member_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void GroupProp_Member_OnInitDialog (HWND hDlg);
void GroupProp_Member_OnDestroy (HWND hDlg);
void GroupProp_Member_OnShowType (HWND hDlg);
void GroupProp_Member_OnSelect (HWND hDlg);
void GroupProp_Member_OnApply (HWND hDlg);
void GroupProp_Member_OnAdd (HWND hDlg);
void GroupProp_Member_OnRemove (HWND hDlg);
void GroupProp_Member_OnEndTask_GetMembers (HWND hDlg, LPTASKPACKET ptp);
void GroupProp_Member_OnEndTask_GetOwned (HWND hDlg, LPTASKPACKET ptp);
void GroupProp_Member_PopulateList (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Group_FreeProperties (LPGROUPPROPINFO lpp)
{
   if (lpp)
      {
      if (lpp->fApplyGeneral && lpp->pGroupList)
         {
         for (size_t ii = 0; ii < lpp->pGroupList->cEntries; ++ii)
            {
            ULONG status;
            ASOBJPROP Properties;
            if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pGroupList->aEntries[ ii ].idObject, &Properties, &status))
               continue;

            LPGROUP_CHANGE_PARAMS pTask = New (GROUP_CHANGE_PARAMS);
            memset (pTask, 0x00, sizeof(GROUP_CHANGE_PARAMS));
            pTask->idGroup = lpp->pGroupList->aEntries[ ii ].idObject;

            // From General tab:

            if (!lpp->fApplyGeneral || lpp->fStatus_Mixed)
               pTask->NewProperties.aaListStatus = Properties.u.GroupProperties.aaListStatus;
            else
               pTask->NewProperties.aaListStatus = lpp->aaStatus;

            if (!lpp->fApplyGeneral || lpp->fGroups_Mixed)
               pTask->NewProperties.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
            else
               pTask->NewProperties.aaListGroupsOwned = lpp->aaGroups;

            if (!lpp->fApplyGeneral || lpp->fMembers_Mixed)
               pTask->NewProperties.aaListMembers = Properties.u.GroupProperties.aaListMembers;
            else
               pTask->NewProperties.aaListMembers = lpp->aaMembers;

            if (!lpp->fApplyGeneral || lpp->fAdd_Mixed)
               pTask->NewProperties.aaAddMember = Properties.u.GroupProperties.aaAddMember;
            else
               pTask->NewProperties.aaAddMember = lpp->aaAdd;

            if (!lpp->fApplyGeneral || lpp->fRemove_Mixed)
               pTask->NewProperties.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;
            else
               pTask->NewProperties.aaDeleteMember = lpp->aaRemove;

            if (!lpp->fApplyGeneral || lpp->fOwner_Mixed)
               lstrcpy (pTask->NewProperties.szOwner, Properties.u.GroupProperties.szOwner);
            else
               lstrcpy (pTask->NewProperties.szOwner, lpp->szOwner);

            StartTask (taskGROUP_CHANGE, NULL, pTask);
            }
         }

      if (lpp->pMembers)
         asc_AsidListFree (&lpp->pMembers);
      if (lpp->pGroupsOwner)
         asc_AsidListFree (&lpp->pGroupsOwner);
      if (lpp->pGroupList)
         asc_AsidListFree (&lpp->pGroupList);
      memset (lpp, 0x00, sizeof(GROUPPROPINFO));
      Delete (lpp);
      }
}


void Group_ShowProperties (LPASIDLIST pGroupList, GROUPPROPTAB gptTarget)
{
   LPGROUPPROPINFO lpp = New (GROUPPROPINFO);
   memset (lpp, 0x00, sizeof(GROUPPROPINFO));
   lpp->pGroupList = pGroupList;
   lpp->fDeleteMeOnClose = TRUE;
   lpp->fShowModal = FALSE;
   Group_ShowProperties (lpp, gptTarget);
}


void Group_ShowProperties (LPGROUPPROPINFO lpp, GROUPPROPTAB gptTarget)
{
   HWND hSheet = NULL;

   // If we've been asked to view properties for only one group, and there is
   // already a window open for that group, switch to it.
   //
   if (lpp->pGroupList)
      {
      if (lpp->pGroupList->cEntries == 1)
         {
         ASID idGroup = lpp->pGroupList->aEntries[0].idObject;

         if ((hSheet = WindowList_Search (wltGROUP_PROPERTIES, idGroup)) != NULL)
            {
            SetForegroundWindow (hSheet);
            if (gptTarget != gptANY)
               {
               HWND hTab = GetDlgItem (hSheet, IDC_PROPSHEET_TABCTRL);
               int nTabs = TabCtrl_GetItemCount (hTab);
               if (nTabs < nGROUPPROPTAB_MAX)
                  gptTarget = (GROUPPROPTAB)( ((int)gptTarget-1) );
               TabCtrl_SwitchToTab (hTab, gptTarget);
               }
            return;
            }
         }

      // Otherwise, make sure there are no Properties windows open for any
      // of the selected groups
      //
      for (size_t iGroup = 0; iGroup < lpp->pGroupList->cEntries; ++iGroup)
         {
         ASID idGroup = lpp->pGroupList->aEntries[ iGroup ].idObject;

         // See if there's a Properties window open that is dedicated to
         // this group
         //
         if ((hSheet = WindowList_Search (wltGROUP_PROPERTIES, idGroup)) != NULL)
            {
            ErrorDialog (0, IDS_ERROR_GROUP_MULTIPROP);
            return;
            }

         // See if there is a multiple-group properties window open; if so,
         // test it to see if it covers this group
         //
         if ((hSheet = WindowList_Search (wltGROUP_PROPERTIES, 0)) != NULL)
            {
            LPGROUPPROPINFO pInfo = (LPGROUPPROPINFO)PropSheet_FindTabParam (hSheet);
            if (pInfo && pInfo->pGroupList && asc_AsidListTest (&pInfo->pGroupList, idGroup))
               {
               ErrorDialog (0, IDS_ERROR_GROUP_MULTIPROP);
               return;
               }
            }
         }
      }

   // Okay, we're clear to create the new properties window.
   //
   LPTSTR pszTitle;
   if (!lpp->pGroupList)
      {
      pszTitle = FormatString (IDS_NEWGROUP_PROPERTIES_TITLE);
      }
   else if (lpp->pGroupList->cEntries > 1)
      {
      pszTitle = FormatString (IDS_GROUP_PROPERTIES_TITLE_MULTIPLE);
      }
   else
      {
      TCHAR szGroup[ cchRESOURCE ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->pGroupList->aEntries[0].idObject, szGroup);
      pszTitle = FormatString (IDS_GROUP_PROPERTIES_TITLE, TEXT("%s"), szGroup);
      }

   LPPROPSHEET psh = PropSheet_Create (pszTitle, TRUE, NULL, (LPARAM)lpp);
   PropSheet_AddTab (psh, 0, (lpp->pGroupList) ? IDD_GROUP_GENERAL : IDD_NEWGROUP_GENERAL, (DLGPROC)GroupProp_General_DlgProc, (LPARAM)lpp, TRUE, (gptTarget == gptGENERAL) ? TRUE : FALSE);
   PropSheet_AddTab (psh, 0, (lpp->pGroupList) ? IDD_GROUP_MEMBER : IDD_NEWGROUP_MEMBER, (DLGPROC)GroupProp_Member_DlgProc, (LPARAM)lpp, TRUE, (gptTarget == gptMEMBERS) ? TRUE : FALSE);

   if (lpp->fShowModal)
      PropSheet_ShowModal (psh, PumpMessage);
   else
      PropSheet_ShowModeless (psh, SW_SHOW);
}


BOOL CALLBACK GroupProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_GROUP_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         if (lpp && lpp->pGroupList && !lpp->fShowModal)
            {
            if (lpp->pGroupList->cEntries == 1)
               WindowList_Add (hDlg, wltGROUP_PROPERTIES, lpp->pGroupList->aEntries[0].idObject);
            else // (lpp->pGroupList->cEntries > 1)
               WindowList_Add (hDlg, wltGROUP_PROPERTIES, 0);
            }
         break;

      case WM_DESTROY_SHEET:
         WindowList_Remove (hDlg);
         if (lpp && lpp->fDeleteMeOnClose)
            Group_FreeProperties (lpp);
         break;

      case WM_INITDIALOG:
         GroupProp_General_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         GroupProp_General_OnDestroy (hDlg);
         break;

      case WM_ASC_NOTIFY_OBJECT:
         GroupProp_General_UpdateDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               GroupProp_General_OnApply (hDlg);
               break;

            case IDC_GROUP_CHANGEOWNER:
               GroupProp_General_OnBrowse (hDlg);
               break;

            case IDC_GROUP_PERM_STATUS:
            case IDC_GROUP_PERM_GROUPS:
            case IDC_GROUP_PERM_MEMBERS:
            case IDC_GROUP_PERM_ADD:
            case IDC_GROUP_PERM_REMOVE:
               if (HIWORD(wp) == CBN_SELCHANGE)
                  PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void GroupProp_General_OnInitDialog (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // Indicate we want to know if anything changes with these groups
   //
   if (lpp->pGroupList)
      {
      LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
      memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
      pTask->hNotify = hDlg;
      asc_AsidListCopy (&pTask->pAsidList, &lpp->pGroupList);
      StartTask (taskOBJECT_LISTEN, NULL, pTask);
      }

   // Fill in the information about the selected groups
   //
   GroupProp_General_UpdateDialog (hDlg);
}


void GroupProp_General_OnDestroy (HWND hDlg)
{
   // Indicate we no longer care if anything changes with these groups
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   StartTask (taskOBJECT_LISTEN, NULL, pTask);
}


void GroupProp_General_UpdateDialog (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // Fix the name shown on the dialog
   //
   if (!lpp->pGroupList || (lpp->pGroupList->cEntries == 1))
      {
      ULONG status;
      TCHAR szName[ cchRESOURCE ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, (lpp->pGroupList) ? (lpp->pGroupList->aEntries[ 0 ].idObject) : g.idCell, szName, &status);

      if (lpp->pGroupList)
         {
         ASOBJPROP Properties;
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pGroupList->aEntries[ 0 ].idObject, &Properties, &status))
            AppendUID (szName, Properties.u.GroupProperties.uidName);
         }

      LPTSTR pszText = FormatString (IDS_GROUP_TITLE, TEXT("%s"), szName);
      SetDlgItemText (hDlg, IDC_GROUP_NAME, pszText);
      FreeString (pszText);
      }
   else  // More than one name?
      {
      LPTSTR pszText = CreateNameList (lpp->pGroupList, IDS_GROUP_NAME_MULTIPLE);
      SetDlgItemText (hDlg, IDC_GROUP_NAME, pszText);
      FreeString (pszText);
      }

   // Fill in the rest of the dialog
   //
   BOOL fGotAnyData = FALSE;

   for (size_t ii = 0; lpp->pGroupList && (ii < lpp->pGroupList->cEntries); ++ii)
      {
      ULONG status;
      ASOBJPROP Properties;
      if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pGroupList->aEntries[ ii ].idObject, &Properties, &status))
         continue;

      if (!fGotAnyData)
         {
         lpp->aaStatus = Properties.u.GroupProperties.aaListStatus;
         lpp->aaGroups = Properties.u.GroupProperties.aaListGroupsOwned;
         lpp->aaMembers = Properties.u.GroupProperties.aaListMembers;
         lpp->aaAdd = Properties.u.GroupProperties.aaAddMember;
         lpp->aaRemove = Properties.u.GroupProperties.aaDeleteMember;
         lstrcpy (lpp->szOwner, Properties.u.GroupProperties.szOwner);
         AppendUID (lpp->szOwner, Properties.u.GroupProperties.uidOwner);
         lstrcpy (lpp->szCreator, Properties.u.GroupProperties.szCreator);
         AppendUID (lpp->szCreator, Properties.u.GroupProperties.uidCreator);
         fGotAnyData = TRUE;
         }
      else // (fGotAnyData)
         {
         if (lpp->aaStatus != Properties.u.GroupProperties.aaListStatus)
            lpp->fStatus_Mixed = TRUE;
         if (lpp->aaGroups != Properties.u.GroupProperties.aaListGroupsOwned)
            lpp->fGroups_Mixed = TRUE;
         if (lpp->aaMembers != Properties.u.GroupProperties.aaListMembers)
            lpp->fMembers_Mixed = TRUE;
         if (lpp->aaAdd != Properties.u.GroupProperties.aaAddMember)
            lpp->fAdd_Mixed = TRUE;
         if (lpp->aaRemove != Properties.u.GroupProperties.aaDeleteMember)
            lpp->fRemove_Mixed = TRUE;

         TCHAR szThisOwner[ cchNAME ];
         lstrcpy (szThisOwner, Properties.u.GroupProperties.szOwner);
         AppendUID (szThisOwner, Properties.u.GroupProperties.uidOwner);
         if (lstrcmpi (lpp->szOwner, szThisOwner))
            lpp->fOwner_Mixed = TRUE;

         TCHAR szThisCreator[ cchNAME ];
         lstrcpy (szThisCreator, Properties.u.GroupProperties.szCreator);
         AppendUID (szThisCreator, Properties.u.GroupProperties.uidCreator);
         if (lstrcmpi (lpp->szCreator, szThisCreator))
            lpp->fCreator_Mixed = TRUE;
         }
      }

   // Fill in the dialog's controls
   //
   GroupProp_General_UpdateDialog_Perm (hDlg, IDC_GROUP_PERM_STATUS, lpp->aaStatus, lpp->fStatus_Mixed);
   GroupProp_General_UpdateDialog_Perm (hDlg, IDC_GROUP_PERM_GROUPS, lpp->aaGroups, lpp->fGroups_Mixed);
   GroupProp_General_UpdateDialog_Perm (hDlg, IDC_GROUP_PERM_MEMBERS, lpp->aaMembers, lpp->fMembers_Mixed);
   GroupProp_General_UpdateDialog_Perm (hDlg, IDC_GROUP_PERM_ADD, lpp->aaAdd, lpp->fAdd_Mixed);
   GroupProp_General_UpdateDialog_Perm (hDlg, IDC_GROUP_PERM_REMOVE, lpp->aaRemove, lpp->fRemove_Mixed);

   if (lpp->fOwner_Mixed)
      GetString (lpp->szOwner, IDS_OWNER_MIXED);
   if (lpp->fCreator_Mixed)
      GetString (lpp->szCreator, IDS_CREATOR_MIXED);
   SetDlgItemText (hDlg, IDC_GROUP_OWNER, lpp->szOwner);
   SetDlgItemText (hDlg, IDC_GROUP_CREATOR, lpp->szCreator);
}


void GroupProp_General_UpdateDialog_Perm (HWND hDlg, int idc, ACCOUNTACCESS aa, BOOL fMixed)
{
   CB_StartChange (GetDlgItem (hDlg, idc), TRUE);
   CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_OWNGROUP, aaOWNER_ONLY);
   if (idc != IDC_GROUP_PERM_GROUPS)
      CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_GROUP, aaGROUP_ONLY);
   if (idc != IDC_GROUP_PERM_REMOVE)
      CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_ANYONE, aaANYONE);
   if (fMixed)
      CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_MIXED, (LPARAM)-1);
   CB_EndChange (GetDlgItem (hDlg, idc), TRUE);

   LPARAM lpSelect = (fMixed) ? ((LPARAM)-1) : (LPARAM)aa;
   CB_SetSelectedByData (GetDlgItem (hDlg, idc), lpSelect);
}


void GroupProp_General_OnApply (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   lpp->fApplyGeneral = TRUE;

   GetDlgItemText (hDlg, IDC_GROUP_OWNER, lpp->szOwner, cchNAME);

   TCHAR szOwnerMixed[ cchNAME ];
   GetString (szOwnerMixed, IDS_OWNER_MIXED);
   if (!lstrcmpi (lpp->szOwner, szOwnerMixed))
      {
      lpp->szOwner[0] = TEXT('\0');
      lpp->fOwner_Mixed = TRUE;
      }
   else
      {
      LPTSTR pch;
      if ((pch = (LPTSTR)lstrchr (lpp->szOwner, TEXT(' '))) != NULL)
         *pch = TEXT('\0'); // strip off any displayed UID
      lpp->fOwner_Mixed = FALSE;
      }

   lpp->aaStatus = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_GROUP_PERM_STATUS));
   lpp->fStatus_Mixed = (lpp->aaStatus == (ACCOUNTACCESS)-1);
   lpp->aaGroups = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_GROUP_PERM_GROUPS));
   lpp->fGroups_Mixed = (lpp->aaGroups == (ACCOUNTACCESS)-1);
   lpp->aaMembers = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_GROUP_PERM_MEMBERS));
   lpp->fMembers_Mixed = (lpp->aaMembers == (ACCOUNTACCESS)-1);
   lpp->aaAdd = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_GROUP_PERM_ADD));
   lpp->fAdd_Mixed = (lpp->aaAdd == (ACCOUNTACCESS)-1);
   lpp->aaRemove = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_GROUP_PERM_REMOVE));
   lpp->fRemove_Mixed = (lpp->aaRemove == (ACCOUNTACCESS)-1);
}


void GroupProp_General_OnBrowse (HWND hDlg)
{
   BROWSE_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.hParent = hDlg;
   pp.iddForHelp = IDD_BROWSE_OWNER;
   pp.idsTitle = IDS_GROUP_CHANGEOWNER_TITLE;
   pp.idsPrompt = IDS_GROUP_CHANGEOWNER_PROMPT;
   pp.idsCheck = 0;
   pp.TypeToShow = (ASOBJTYPE)( (ULONG)TYPE_USER | (ULONG)TYPE_GROUP );
   pp.fAllowMultiple = FALSE;
   GetDlgItemText (hDlg, IDC_GROUP_OWNER, pp.szName, cchNAME);

   TCHAR szOwnerMixed[ cchNAME ];
   GetString (szOwnerMixed, IDS_OWNER_MIXED);
   LPTSTR pch;
   if (!lstrcmpi (pp.szName, szOwnerMixed))
      pp.szName[0] = TEXT('\0');
   else if ((pch = (LPTSTR)lstrchr (pp.szName, TEXT(' '))) != NULL)
      *pch = TEXT('\0');

   if (ShowBrowseDialog (&pp))
      {
      TCHAR szName[ cchNAME ];
      lstrcpy (szName, pp.szName);

      if (pp.pObjectsSelected && (pp.pObjectsSelected->cEntries == 1))
         {
         ULONG status;
         ASOBJPROP Properties;
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pp.pObjectsSelected->aEntries[0].idObject, &Properties, &status))
            {
            if (Properties.Type == TYPE_USER)
               AppendUID (szName, Properties.u.UserProperties.PTSINFO.uidName);
            else if (Properties.Type == TYPE_GROUP)
               AppendUID (szName, Properties.u.GroupProperties.uidName);
            }
         }

      SetDlgItemText (hDlg, IDC_GROUP_OWNER, szName);
      }

   if (pp.pObjectsSelected)
      asc_AsidListFree (&pp.pObjectsSelected);
}


BOOL CALLBACK GroupProp_Member_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_GROUP_MEMBER, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         GroupProp_Member_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         GroupProp_Member_OnDestroy (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskGROUP_MEMBERS_GET)
               GroupProp_Member_OnEndTask_GetMembers (hDlg, ptp);
            else if (ptp->idTask == taskGROUP_OWNED_GET)
               GroupProp_Member_OnEndTask_GetOwned (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               GroupProp_Member_OnApply (hDlg);
               break;

            case IDC_GROUP_SHOW_MEMBER:
            case IDC_GROUP_SHOW_OWNER:
               GroupProp_Member_OnShowType (hDlg);
               break;

            case IDC_MEMBER_ADD:
               GroupProp_Member_OnAdd (hDlg);
               break;

            case IDC_MEMBER_REMOVE:
               GroupProp_Member_OnRemove (hDlg);
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               GroupProp_Member_OnSelect (hDlg);
               break;
            }
         break;
      }
   return FALSE;
}


void GroupProp_Member_OnInitDialog (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // If we've come in here with a valid set of groups to display,
   // copy those. We'll need the copies if the user cancels the dialog.
   //
   SetWindowData (hDlg, GWD_ASIDLIST_MEMBER, (UINT_PTR)(lpp->pMembers));
   SetWindowData (hDlg, GWD_ASIDLIST_OWNER,  (UINT_PTR)(lpp->pGroupsOwner));

   LPASIDLIST pList;
   if ((pList = lpp->pMembers) != NULL)
      asc_AsidListCopy (&lpp->pMembers, &pList);
   if ((pList = lpp->pGroupsOwner) != NULL)
      asc_AsidListCopy (&lpp->pGroupsOwner, &pList);

   // If this prop sheet reflects more than one group, disable the
   // Ownership button (we do this primarily because the Add/Remove
   // buttons have no straight-forward semantic for that case).
   // Actually, instead of disabling the thing, we'll hide the buttons
   // entirely and resize the list/move the title.
   //
   if (lpp->pGroupList && (lpp->pGroupList->cEntries > 1))
      {
      ShowWindow (GetDlgItem (hDlg, IDC_GROUP_SHOW_MEMBER), SW_HIDE);
      ShowWindow (GetDlgItem (hDlg, IDC_GROUP_SHOW_OWNER), SW_HIDE);

      RECT rButton;
      GetRectInParent (GetDlgItem (hDlg, IDC_GROUP_SHOW_MEMBER), &rButton);

      RECT rTitle;
      GetRectInParent (GetDlgItem (hDlg, IDC_USERS_TITLE), &rTitle);

      RECT rList;
      GetRectInParent (GetDlgItem (hDlg, IDC_USERS_LIST), &rList);

      LONG cyDelta = rTitle.top - rButton.top;

      SetWindowPos (GetDlgItem (hDlg, IDC_USERS_TITLE), NULL,
                    rTitle.left, rTitle.top-cyDelta,
                    rTitle.right-rTitle.left, rTitle.bottom-rTitle.top,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

      SetWindowPos (GetDlgItem (hDlg, IDC_USERS_LIST), NULL,
                    rList.left, rList.top-cyDelta,
                    rList.right-rList.left, rList.bottom-rList.top+cyDelta,
                    SWP_NOZORDER | SWP_NOACTIVATE);
      }

   // Apply an imagelist to the dialog's fastlist
   //
   FastList_SetImageLists (GetDlgItem (hDlg, IDC_USERS_LIST), AfsAppLib_CreateImageList (FALSE), NULL);

   // Select a checkbox and pretend that the user clicked it; that will
   // make the dialog automatically re-populate the list of groups
   //
   CheckDlgButton (hDlg, IDC_GROUP_SHOW_MEMBER, TRUE);
   GroupProp_Member_OnShowType (hDlg);
}


void GroupProp_Member_OnDestroy (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   LPASIDLIST pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_MEMBER)) != NULL)
      lpp->pMembers = pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_OWNER)) != NULL)
      lpp->pGroupsOwner = pList;
}


void GroupProp_Member_OnShowType (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // If we've already obtained the list of users we should be displaying,
   // just go show it.
   //
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      {
      if (lpp->pMembers)
         {
         GroupProp_Member_PopulateList (hDlg);
         return;
         }
      }
   else // (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      {
      if (lpp->pGroupsOwner)
         {
         GroupProp_Member_PopulateList (hDlg);
         return;
         }
      }

   // Otherwise, we'll have to start a background task to do the querying.
   // Change the text above the list to "Querying...", disable the buttons,
   // and remove any items in the list
   //
   if (!lpp->pGroupList)
      {
      // Generate a few empty ASID list; this is a new group account we're
      // displaying, and the caller hasn't yet added any members to it.
      //
      if (!lpp->pMembers)
         {
         if (!asc_AsidListCreate (&lpp->pMembers))
            return;
         }
      if (!lpp->pGroupsOwner)
         {
         if (!asc_AsidListCreate (&lpp->pGroupsOwner))
            return;
         }
      GroupProp_Member_OnShowType (hDlg);
      }
   else // (lpp->pUserList)
      {
      TCHAR szTitle[ cchRESOURCE ];
      GetString (szTitle, IDS_QUERYING_LONG);
      SetDlgItemText (hDlg, IDC_USERS_TITLE, szTitle);

      EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_ADD), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_REMOVE), FALSE);

      FastList_RemoveAll (GetDlgItem (hDlg, IDC_USERS_LIST));

      // Then start a background task to grab an ASIDLIST of users which
      // match the specified search criteria.
      //
      if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
         {
         LPASIDLIST pGroups;
         asc_AsidListCopy (&pGroups, &lpp->pGroupList);
         StartTask (taskGROUP_MEMBERS_GET, hDlg, pGroups);
         }
      else // (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
         {
         StartTask (taskGROUP_OWNED_GET, hDlg, (PVOID)(lpp->pGroupList->aEntries[0].idObject));
         }
      }
}


void GroupProp_Member_OnEndTask_GetMembers (HWND hDlg, LPTASKPACKET ptp)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // The search is complete. If we've just obtained an ASIDLIST successfully,
   // associate it with the dialog and repopulate the display.
   //
   if (ptp->rc && TASKDATA(ptp)->pAsidList)
      {
      if (!lpp->pMembers)
         {
         asc_AsidListCopy (&lpp->pMembers, &TASKDATA(ptp)->pAsidList);
         GroupProp_Member_PopulateList (hDlg);
         }
      }
}


void GroupProp_Member_OnEndTask_GetOwned (HWND hDlg, LPTASKPACKET ptp)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // The search is complete. If we've just obtained an ASIDLIST successfully,
   // associate it with the dialog and repopulate the display.
   //
   if (ptp->rc && TASKDATA(ptp)->pAsidList)
      {
      if (!lpp->pGroupsOwner)
         {
         asc_AsidListCopy (&lpp->pGroupsOwner, &TASKDATA(ptp)->pAsidList);
         GroupProp_Member_PopulateList (hDlg);
         }
      }
}


void GroupProp_Member_PopulateList (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // Clear the list of members/owned groups
   //
   HWND hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   FastList_Begin (hList);
   FastList_RemoveAll (hList);

   // We should have an ASIDLIST associated with the dialog to reflect
   // which objects to display. Find it, and repopulate the list on
   // the display.
   //
   LPASIDLIST pDisplay;
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      pDisplay = lpp->pMembers;
   else // (!IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      pDisplay = lpp->pGroupsOwner;

   if (pDisplay)
      {
      TCHAR szMixed[ cchRESOURCE ];
      GetString (szMixed, IDS_MEMBER_MIXED);

      for (size_t ii = 0; ii < pDisplay->cEntries; ++ii)
         {
         ULONG status;
         ASOBJPROP Properties;
         if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pDisplay->aEntries[ ii ].idObject, &Properties, &status))
            continue;

         TCHAR szName[ cchNAME ];
         if (Properties.Type == TYPE_USER)
            User_GetDisplayName (szName, &Properties);
         else
            lstrcpy (szName, Properties.szName);

         if (!pDisplay->aEntries[ ii ].lParam)
            lstrcat (szName, szMixed);

         FASTLISTADDITEM flai;
         memset (&flai, 0x00, sizeof(flai));
         flai.iFirstImage = (Properties.Type == TYPE_USER) ? imageUSER : imageGROUP;
         flai.iSecondImage = IMAGE_NOIMAGE;
         flai.pszText = szName;
         flai.lParam = (LPARAM)(pDisplay->aEntries[ ii ].idObject);
         FastList_AddItem (hList, &flai);
         }
      }

   FastList_End (hList);

   // Fix the buttons, and the text at the top of the list
   //
   TCHAR szTitle[ cchRESOURCE ];
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      GetString (szTitle, (!lpp->pGroupList) ? IDS_NEWGROUP_SHOW_MEMBER_TITLE : (lpp->pGroupList->cEntries == 1) ? IDS_GROUP_SHOW_MEMBER_TITLE : IDS_GROUP_SHOW_MEMBER_TITLE_MULTIPLE);
   else
      GetString (szTitle, (!lpp->pGroupList) ? IDS_NEWGROUP_SHOW_OWNED_TITLE : IDS_GROUP_SHOW_OWNED_TITLE);

   SetDlgItemText (hDlg, IDC_USERS_TITLE, szTitle);

   EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_ADD), TRUE);
   GroupProp_Member_OnSelect (hDlg);
}


void GroupProp_Member_OnSelect (HWND hDlg)
{
   BOOL fEnable = IsWindowEnabled (GetDlgItem (hDlg, IDC_MEMBER_ADD));
   if (fEnable && !FastList_FindFirstSelected (GetDlgItem (hDlg, IDC_USERS_LIST)))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_REMOVE), fEnable);
}


void GroupProp_Member_OnAdd (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   LPBROWSE_PARAMS pParams = New (BROWSE_PARAMS);
   memset (pParams, 0x00, sizeof(BROWSE_PARAMS));
   pParams->hParent = GetParent(hDlg);
   pParams->iddForHelp = IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER) ? IDD_BROWSE_MEMBER : IDD_BROWSE_OWNED;
   pParams->idsTitle = IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER) ? IDS_BROWSE_TITLE_MEMBER : IDS_BROWSE_TITLE_OWNED;
   pParams->idsPrompt = IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER) ? IDS_BROWSE_PROMPT_MEMBER : IDS_BROWSE_PROMPT_OWNED;
   pParams->idsCheck = IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER) ? IDS_BROWSE_CHECK_MEMBER : IDS_BROWSE_CHECK_OWNED;
   pParams->fAllowMultiple = TRUE;
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      pParams->TypeToShow = (ASOBJTYPE)( (ULONG)TYPE_USER | (ULONG)TYPE_GROUP );
   else // (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      pParams->TypeToShow = TYPE_GROUP;

   // First prepare an ASIDLIST which reflects only the groups which
   // all selected users own/are members in.
   //
   LPASIDLIST pDisplay;
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      pDisplay = lpp->pMembers;
   else // (!IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      pDisplay = lpp->pGroupsOwner;

   if (pDisplay)
      {
      asc_AsidListCreate (&pParams->pObjectsToSkip);
      for (size_t ii = 0; ii < pDisplay->cEntries; ++ii)
         {
         if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER) || pDisplay->aEntries[ ii ].lParam) // all groups have this member?
            asc_AsidListAddEntry (&pParams->pObjectsToSkip, pDisplay->aEntries[ ii ].idObject, 0);
         }
      }

   if (ShowBrowseDialog (pParams))
      {
      // The user added some entries; modify pDisplay (and the dialog,
      // at the same time).
      //
      HWND hList = GetDlgItem (hDlg, IDC_USERS_LIST);
      FastList_Begin (hList);

      for (size_t ii = 0; ii < pParams->pObjectsSelected->cEntries; ++ii)
         {
         ASID idMember = pParams->pObjectsSelected->aEntries[ ii ].idObject;

         // The user has chosen to add member {idMember}. If it's not in our
         // list at all, add it (and a new entry for the display). If it
         // *is* in our list, make sure its lParam is 1--indicating all users
         // get it--and modify the display's entry to remove the "(some)"
         // marker.
         //
         LPARAM lParam;
         if (!asc_AsidListTest (&pDisplay, idMember, &lParam))
            {
            ULONG status;
            ASOBJPROP Properties;
            if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idMember, &Properties, &status))
               continue;

            asc_AsidListAddEntry (&pDisplay, idMember, TRUE);

            FASTLISTADDITEM flai;
            memset (&flai, 0x00, sizeof(flai));
            flai.iFirstImage = (Properties.Type == TYPE_USER) ? imageUSER : imageGROUP;
            flai.iSecondImage = IMAGE_NOIMAGE;
            flai.pszText = Properties.szName;
            flai.lParam = (LPARAM)idMember;
            FastList_AddItem (hList, &flai);
            PropSheetChanged (hDlg);
            }
         else if (!lParam)
            {
            TCHAR szName[ cchNAME ];
            if (!User_GetDisplayName (szName, idMember))
               continue;

            asc_AsidListSetEntryParam (&pDisplay, idMember, TRUE);

            HLISTITEM hItem;
            if ((hItem = FastList_FindItem (hList, (LPARAM)idMember)) != NULL)
               FastList_SetItemText (hList, hItem, 0, szName);
            PropSheetChanged (hDlg);
            }
         }

      if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
         lpp->pMembers = pDisplay;
      else // (!IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
         lpp->pGroupsOwner = pDisplay;

      FastList_End (hList);
      }

   if (pParams->pObjectsToSkip)
      asc_AsidListFree (&pParams->pObjectsToSkip);
   if (pParams->pObjectsSelected)
      asc_AsidListFree (&pParams->pObjectsSelected);
   Delete (pParams);
}


void GroupProp_Member_OnRemove (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // Find which list-of-members is currently being displayed.
   //
   LPASIDLIST pDisplay;
   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      pDisplay = lpp->pMembers;
   else // (!IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      pDisplay = lpp->pGroupsOwner;

   // The user wants to remove some members/owned groups; modify pDisplay
   // (and the dialog, at the same time).
   //
   HWND hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   FastList_Begin (hList);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      ASID idMember = (ASID)FastList_GetItemParam (hList, hItem);
      FastList_RemoveItem (hList, hItem);
      asc_AsidListRemoveEntry (&pDisplay, idMember);
      PropSheetChanged (hDlg);
      }

   if (IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_MEMBER))
      lpp->pMembers = pDisplay;
   else // (!IsDlgButtonChecked (hDlg, IDC_GROUP_SHOW_OWNER))
      lpp->pGroupsOwner = pDisplay;

   FastList_End (hList);
}


void GroupProp_Member_OnApply (HWND hDlg)
{
   LPGROUPPROPINFO lpp = (LPGROUPPROPINFO)PropSheet_FindTabParam (hDlg);

   // Free the old backup ASIDLIST copies we attached to the dialog during
   // WM_INITDIALOG processing.
   //
   LPASIDLIST pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_MEMBER)) != NULL)
      asc_AsidListFree (&pList);
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_OWNER)) != NULL)
      asc_AsidListFree (&pList);
   SetWindowData (hDlg, GWD_ASIDLIST_MEMBER, 0);
   SetWindowData (hDlg, GWD_ASIDLIST_OWNER, 0);

   if (lpp->pGroupList)
      {
      // Start a background task to update the member-list etc
      //
      if (lpp->pMembers)
         {
         LPGROUP_MEMBERS_SET_PARAMS pTask = New (GROUP_MEMBERS_SET_PARAMS);
         memset (pTask, 0x00, sizeof(GROUP_MEMBERS_SET_PARAMS));
         asc_AsidListCopy (&pTask->pGroups, &lpp->pGroupList);
         asc_AsidListCopy (&pTask->pMembers, &lpp->pMembers);
         StartTask (taskGROUP_MEMBERS_SET, NULL, pTask);
         }
      if (lpp->pGroupsOwner)
         {
         LPGROUP_OWNED_SET_PARAMS pTask = New (GROUP_OWNED_SET_PARAMS);
         memset (pTask, 0x00, sizeof(GROUP_OWNED_SET_PARAMS));
         pTask->idGroup = lpp->pGroupList->aEntries[0].idObject;
         asc_AsidListCopy (&pTask->pOwnedGroups, &lpp->pGroupsOwner);
         StartTask (taskGROUP_OWNED_SET, NULL, pTask);
         }
      }
}

