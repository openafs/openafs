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
#include "messages.h"
#include "creds.h"
#include "action.h"
#include "usr_col.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef iswhite
#define iswhite(_ch) ( ((_ch) == TEXT(' ')) || ((_ch) == TEXT('\t')) )
#endif

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Task_OpenCell (LPTASKPACKET ptp);
void Task_UpdCreds (LPTASKPACKET ptp);
void Task_UpdUsers (LPTASKPACKET ptp);
void Task_UpdGroups (LPTASKPACKET ptp);
void Task_UpdMachines (LPTASKPACKET ptp);
void Task_Refresh (LPTASKPACKET ptp);
void Task_RefreshMult (LPTASKPACKET ptp);
void Task_Get_Actions (LPTASKPACKET ptp);
void Task_Get_Random_Key (LPTASKPACKET ptp);
void Task_User_Change (LPTASKPACKET ptp);
void Task_User_Find (LPTASKPACKET ptp);
void Task_User_Enum (LPTASKPACKET ptp);
void Task_User_GroupList_Set (LPTASKPACKET ptp);
BOOL Task_User_GroupList_Set_Do (LPUSER_GROUPLIST_SET_PARAMS lpp, ULONG *pStatus);
void Task_User_CPW (LPTASKPACKET ptp);
void Task_User_Unlock (LPTASKPACKET ptp);
void Task_User_Create (LPTASKPACKET ptp);
void Task_User_Delete (LPTASKPACKET ptp);
void Task_Group_Change (LPTASKPACKET ptp);
void Task_Group_Search (LPTASKPACKET ptp);
void Task_Group_Members_Get (LPTASKPACKET ptp);
void Task_Group_Members_Set (LPTASKPACKET ptp);
BOOL Task_Group_Members_Set_Do (LPGROUP_MEMBERS_SET_PARAMS lpp, ULONG *pStatus);
void Task_Group_Enum (LPTASKPACKET ptp);
void Task_Group_Rename (LPTASKPACKET ptp);
void Task_Group_Owned_Get (LPTASKPACKET ptp);
void Task_Group_Owned_Set (LPTASKPACKET ptp);
BOOL Task_Group_Owned_Set_Do (LPGROUP_OWNED_SET_PARAMS lpp, ULONG *pStatus);
void Task_Group_Create (LPTASKPACKET ptp);
void Task_Group_Delete (LPTASKPACKET ptp);
void Task_Cell_Change (LPTASKPACKET ptp);
void Task_List_Translate (LPTASKPACKET ptp);
void Task_Object_Listen (LPTASKPACKET ptp);
void Task_Object_Get (LPTASKPACKET ptp);
void Task_Set_Refresh (LPTASKPACKET ptp);
void Task_Expired_Creds (LPTASKPACKET ptp);

void WeedAsidList (LPASIDLIST *ppList, BOOL fWantMachines);
void TranslateRegExp (LPTSTR pszTarget, LPCTSTR pszSource);
BOOL PerformRefresh (LPTASKPACKET ptp, ASID idScope, ULONG *pStatus);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPTASKPACKET CreateTaskPacket (int idTask, HWND hReply, PVOID lpUser)
{
   LPTASKPACKET ptp;

   if ((ptp = New (TASKPACKET)) != NULL)
      {
      memset (ptp, 0x00, sizeof(TASKPACKET));

      ptp->idTask = idTask;
      ptp->hReply = hReply;
      ptp->lpUser = lpUser;
      ptp->rc = TRUE;
      ptp->status = 0;

      if ((ptp->pReturn = New (TASKPACKETDATA)) != NULL)
         {
         memset (ptp->pReturn, 0x00, sizeof(TASKPACKETDATA));
         }
      }

   return ptp;
}


void FreeTaskPacket (LPTASKPACKET ptp)
{
   if (ptp)
      {
      if (ptp->pReturn)
         {
         if (TASKDATA(ptp)->pAsidList)
            asc_AsidListFree (&TASKDATA(ptp)->pAsidList);
         if (TASKDATA(ptp)->pActionList)
            asc_ActionListFree (&TASKDATA(ptp)->pActionList);
         Delete (ptp->pReturn);
         }
      Delete (ptp);
      }
}


void PerformTask (LPTASKPACKET ptp)
{
   switch (ptp->idTask)
      {
      case taskOPENCELL:
         Task_OpenCell (ptp);
         break;

      case taskUPD_CREDS:
         Task_UpdCreds (ptp);
         break;

      case taskUPD_USERS:
         Task_UpdUsers (ptp);
         break;

      case taskUPD_GROUPS:
         Task_UpdGroups (ptp);
         break;

      case taskUPD_MACHINES:
         Task_UpdMachines (ptp);
         break;

      case taskREFRESH:
         Task_Refresh (ptp);
         break;

      case taskREFRESHMULT:
         Task_RefreshMult (ptp);
         break;

      case taskGET_ACTIONS:
         Task_Get_Actions (ptp);
         break;

      case taskGET_RANDOM_KEY:
         Task_Get_Random_Key (ptp);
         break;

      case taskUSER_CHANGE:
         Task_User_Change (ptp);
         break;

      case taskUSER_FIND:
         Task_User_Find (ptp);
         break;

      case taskUSER_ENUM:
         Task_User_Enum (ptp);
         break;

      case taskUSER_GROUPLIST_SET:
         Task_User_GroupList_Set (ptp);
         break;

      case taskUSER_CPW:
         Task_User_CPW (ptp);
         break;

      case taskUSER_UNLOCK:
         Task_User_Unlock (ptp);
         break;

      case taskUSER_CREATE:
         Task_User_Create (ptp);
         break;

      case taskUSER_DELETE:
         Task_User_Delete (ptp);
         break;

      case taskGROUP_CHANGE:
         Task_Group_Change (ptp);
         break;

      case taskGROUP_SEARCH:
         Task_Group_Search (ptp);
         break;

      case taskGROUP_MEMBERS_GET:
         Task_Group_Members_Get (ptp);
         break;

      case taskGROUP_MEMBERS_SET:
         Task_Group_Members_Set (ptp);
         break;

      case taskGROUP_ENUM:
         Task_Group_Enum (ptp);
         break;

      case taskGROUP_RENAME:
         Task_Group_Rename (ptp);
         break;

      case taskGROUP_OWNED_GET:
         Task_Group_Owned_Get (ptp);
         break;

      case taskGROUP_OWNED_SET:
         Task_Group_Owned_Set (ptp);
         break;

      case taskGROUP_CREATE:
         Task_Group_Create (ptp);
         break;

      case taskGROUP_DELETE:
         Task_Group_Delete (ptp);
         break;

      case taskCELL_CHANGE:
         Task_Cell_Change (ptp);
         break;

      case taskLIST_TRANSLATE:
         Task_List_Translate (ptp);
         break;

      case taskOBJECT_LISTEN:
         Task_Object_Listen (ptp);
         break;

      case taskOBJECT_GET:
         Task_Object_Get (ptp);
         break;

      case taskSET_REFRESH:
         Task_Set_Refresh (ptp);
         break;

      case taskEXPIRED_CREDS:
         Task_Expired_Creds (ptp);
         break;

      default:
         ptp->rc = FALSE;
         ptp->status = ERROR_INVALID_FUNCTION;
         break;
      }
}


/*
 * KEYS _______________________________________________________________________
 *
 */

HASHVALUE CALLBACK Key_Asid_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return *(ASID*)pData;
}

HASHVALUE CALLBACK Key_Asid_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return Key_Asid_HashData (pKey, (ASID*)pObject);
}

BOOL CALLBACK Key_Asid_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return ((*(ASID*)pObject) == (*(ASID*)pData));
}


/*
 * TASKS ______________________________________________________________________
 *
 */

void Task_OpenCell (LPTASKPACKET ptp)
{
   LPOPENCELL_PARAMS lpp = (LPOPENCELL_PARAMS)( ptp->lpUser );

   Display_StartWorking();

   // Try to open the cell for administration
   //
   ptp->rc = asc_CellOpen (g.idClient, (PVOID)lpp->hCreds, lpp->szCell, AFSADMSVR_SCOPE_USERS, &TASKDATA(ptp)->idCell, &ptp->status);

   if (ptp->rc)
      {
      PostMessage (g.hMain, WM_SHOW_YOURSELF, 0, 1);
      }
   else if ((!ptp->rc) && (!IsWindow(ptp->hReply)))
      {
      if (lpp->fCloseAppOnFail)
         FatalErrorDialog (ptp->status, IDS_ERROR_CANT_OPEN_CELL, TEXT("%s"), lpp->szCell);
      else
         ErrorDialog (ptp->status, IDS_ERROR_CANT_OPEN_CELL, TEXT("%s"), lpp->szCell);
      }

   // If we previously had another cell open, close it.
   //
   if (ptp->rc)
      {
      asc_Enter();

      if (g.idCell)
         {
         ULONG status;
         (void)asc_CellClose (g.idClient, g.idCell, &status);
         }
      g.idCell = TASKDATA(ptp)->idCell;

      asc_Leave();
      }

   // Update the "Selected Cell:" text on the main window
   //
   TCHAR szCell[ cchNAME ];
   if (!g.idCell)
      GetString (szCell, IDS_CELL_NONE);
   else if (!asc_CellNameGet_Fast (g.idClient, g.idCell, szCell))
      GetString (szCell, IDS_CELL_NONE);
   SetDlgItemText (g.hMain, IDC_CELL, szCell);
   ShowCurrentCredentials();

   // Oh--also, set the refresh rate for the newly-opened cell
   //
   ULONG dummy;
   asc_CellRefreshRateSet (g.idClient, g.idCell, gr.cminRefreshRate, &dummy);

   // Start re-populating the Users or Groups tab (whichever is showing)
   //
   Display_PopulateList();

   Display_StopWorking();

   // When we've opened a new cell, it's time to open the Actions window.
   //
   if (gr.fShowActions)
      PostMessage (g.hMain, WM_SHOW_ACTIONS, 0, 0);

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_UpdCreds (LPTASKPACKET ptp)
{
   // Update the display to report our new credentials
   //
   ShowCurrentCredentials();

   // Tell the admin server to use our new credentials, and refresh everything
   //
   if (!asc_CredentialsPush (g.idClient, (PVOID)g.hCreds, g.idCell, &ptp->status))
      ptp->rc = FALSE;
   else
      ptp->rc = PerformRefresh (ptp, g.idCell, &ptp->status);
}


void Task_UpdUsers (LPTASKPACKET ptp)
{
   // First we'll query the admin server to find a list of all users which
   // match our search pattern.
   //
   lstrcpy (TASKDATA(ptp)->szPattern, g.szPatternUsers);

   TCHAR szRegExp[ cchNAME ];
   TranslateRegExp (szRegExp, TASKDATA(ptp)->szPattern);
   if (!asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_USER, szRegExp, &gr.SearchUsers, &TASKDATA(ptp)->pAsidList, &ptp->status))
      ptp->rc = FALSE;

   // If we got a result back, weed out any entries that look like machines.
   //
   if (ptp->rc)
      {
      WeedAsidList (&TASKDATA(ptp)->pAsidList, FALSE);
      }

   // Wow, that was easy. Okay, next step: ensure that we have properties
   // for all these guys in the local cache--to do that, we'll query their
   // properties, then free the result.
   //
   if (ptp->rc)
      {
      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_ALL_DATA, g.idCell, TASKDATA(ptp)->pAsidList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }
}


void Task_UpdGroups (LPTASKPACKET ptp)
{
   // First we'll query the admin server to find a list of all groups which
   // match our search pattern.
   //
   lstrcpy (TASKDATA(ptp)->szPattern, g.szPatternGroups);

   TCHAR szRegExp[ cchNAME ];
   TranslateRegExp (szRegExp, TASKDATA(ptp)->szPattern);
   if (!asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_GROUP, szRegExp, NULL, &TASKDATA(ptp)->pAsidList, &ptp->status))
      ptp->rc = FALSE;

   // Wow, that was easy. Okay, next step: ensure that we have properties
   // for all these guys in the local cache--to do that, we'll query their
   // properties, then free the result.
   //
   if (ptp->rc)
      {
      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_ALL_DATA, g.idCell, TASKDATA(ptp)->pAsidList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }
}


void Task_UpdMachines (LPTASKPACKET ptp)
{
   // First we'll query the admin server to find a list of all users which
   // match our search pattern.
   //
   TCHAR szRegExp[ cchNAME ];
   if (g.szPatternMachines[0])
      TranslateRegExp (szRegExp, g.szPatternMachines);
   else
      lstrcpy (szRegExp, TEXT("^[0-9.]*$"));

   if (!asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_USER, szRegExp, NULL, &TASKDATA(ptp)->pAsidList, &ptp->status))
      ptp->rc = FALSE;

   lstrcpy (TASKDATA(ptp)->szPattern, g.szPatternMachines);

   // If we got a result back, weed out any entries that don't look
   // like machines.
   //
   if (ptp->rc)
      {
      WeedAsidList (&TASKDATA(ptp)->pAsidList, TRUE);
      }

   // Wow, that was easy. Okay, next step: ensure that we have properties
   // for all these guys in the local cache--to do that, we'll query their
   // properties, then free the result.
   //
   if (ptp->rc)
      {
      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_ALL_DATA, g.idCell, TASKDATA(ptp)->pAsidList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }
}



void Task_Refresh (LPTASKPACKET ptp)
{
   ASID idScope = (ASID)( ptp->lpUser );

   ptp->rc = PerformRefresh (ptp, idScope, &ptp->status);
}


void Task_RefreshMult (LPTASKPACKET ptp)
{
   LPASIDLIST pAsidList = (LPASIDLIST)( ptp->lpUser );

   // Invalidate the admin server's cached information about the specified
   // object. Remember that this is recursive hierarchically: if you pass
   // in a cell's ID, for instance, information about all users, groups,
   // servers, services, partitions and volumes anywhere in that cell will
   // be discarded.
   //
   ptp->rc = asc_ObjectRefreshMultiple (g.idClient, g.idCell, pAsidList, &ptp->status);

   // The Refresh call above just made us invalidate the status for one or
   // more objects; to get the display to reflect the changes, we'll have to
   // query the server for the latest properties for those objects. Once that's
   // done, we'll just redraw the main window and it will pick up the changes.
   //
   if (ptp->rc)
      {
      ULONG status;
      LPASOBJPROPLIST pPropList = NULL;
      if (asc_ObjectPropertiesGetMultiple (g.idClient, GET_ALL_DATA, g.idCell, pAsidList, &pPropList, &status))
         {
         // That call returned properties for the objects; we don't need
         // the properties here--we just wanted to get them in the cache.
         // Now that they're in the cache, redrawing the main window will
         // cause the latest data to be displayed.
         //
         if (pPropList)
            asc_ObjPropListFree (&pPropList);
         Display_RefreshView_Fast();
         }
      }

   asc_AsidListFree (&pAsidList);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Get_Actions (LPTASKPACKET ptp)
{
   // Query the admin server to get a current list of operations-in-progress.
   // We'll limit our search to operations being performed on this cell.
   //
   ptp->rc = asc_ActionGetMultiple (g.idClient, 0, g.idCell, &TASKDATA(ptp)->pActionList, &ptp->status);
}


void Task_Get_Random_Key (LPTASKPACKET ptp)
{
   ptp->rc = asc_RandomKeyGet (g.idClient, g.idCell, TASKDATA(ptp)->key, &ptp->status);

   if (!ptp->rc && !IsWindow(ptp->hReply))
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_GET_RANDOM_KEY);
      }
}


void Task_User_Change (LPTASKPACKET ptp)
{
   LPUSER_CHANGE_PARAMS lpp = (LPUSER_CHANGE_PARAMS)( ptp->lpUser );

   if ((ptp->rc = asc_UserChange (g.idClient, g.idCell, lpp->idUser, &lpp->NewProperties, &ptp->status)) == TRUE)
      {
      Display_RefreshView_Fast();
      }

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      TCHAR szUser[ cchNAME ];
      User_GetDisplayName (szUser, lpp->idUser);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_USER, TEXT("%s"), szUser);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_Find (LPTASKPACKET ptp)
{
   LPTSTR pszName = (LPTSTR)( ptp->lpUser );

   if ((ptp->rc = asc_ObjectFind (g.idClient, g.idCell, TYPE_USER, pszName, &TASKDATA(ptp)->idObject, &ptp->status)) == TRUE)
      {
      ptp->rc = asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, TASKDATA(ptp)->idObject, &TASKDATA(ptp)->Properties, &ptp->status);
      }

   FreeString (pszName);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_Enum (LPTASKPACKET ptp)
{
   LPCTSTR pszPattern = (LPCTSTR)( ptp->lpUser );

   TCHAR szRegExp[ cchNAME ];
   TranslateRegExp (szRegExp, pszPattern);
   if ((ptp->rc = asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_USER, szRegExp, NULL, &TASKDATA(ptp)->pAsidList, &ptp->status)) == TRUE)
      {
      LPASOBJPROPLIST pPropList = NULL;

      ptp->rc = asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, TASKDATA(ptp)->pAsidList, &pPropList, &ptp->status);

      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }

   FreeString (pszPattern);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_GroupList_Set (LPTASKPACKET ptp)
{
   LPUSER_GROUPLIST_SET_PARAMS lpp = (LPUSER_GROUPLIST_SET_PARAMS)( ptp->lpUser );

   ptp->rc = Task_User_GroupList_Set_Do (lpp, &ptp->status);

   asc_AsidListFree (&lpp->pUsers);
   asc_AsidListFree (&lpp->pGroups);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


BOOL Task_User_GroupList_Set_Do (LPUSER_GROUPLIST_SET_PARAMS lpp, ULONG *pStatus)
{
   BOOL rc = TRUE;

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_SET_GROUPS, IDS_ERROR_CANT_SET_GROUPS_MULTIPLE);

   // We'll need the supplied group-list in a hashlist, so we can quickly
   // test it for inclusion of a particular group.
   //
   LPHASHLIST pGroupsAllow = New (HASHLIST);
   size_t iGroup;
   for (iGroup = 0; iGroup < lpp->pGroups->cEntries; ++iGroup)
      pGroupsAllow->AddUnique ((PVOID)(lpp->pGroups->aEntries[ iGroup ].idObject));

   // We'll have to do this next bit for each user in the supplied user-list
   //
   for (size_t iUser = 0; iUser < lpp->pUsers->cEntries; ++iUser)
      {
      ULONG status;

      // Obtain the appropriate current list of groups for this user
      //
      LPASIDLIST pGroupsOld = NULL;
      if (lpp->fMembership)
         {
         if (!asc_GroupMembershipGet (g.idClient, g.idCell, lpp->pUsers->aEntries[ iUser ].idObject, &pGroupsOld, &status))
            {
            ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
            continue;
            }
         }
      else // (!lpp->fMembership)
         {
         if (!asc_GroupOwnershipGet (g.idClient, g.idCell, lpp->pUsers->aEntries[ iUser ].idObject, &pGroupsOld, &status))
            {
            ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
            continue;
            }
         }
      if (!pGroupsOld)
         continue;

      // Test each group in that current list to see if it's also on our
      // pGroupsAllow list. If not, remove it.
      //
      for (iGroup = 0; iGroup < pGroupsOld->cEntries; ++iGroup)
         {
         if (pGroupsAllow->fIsInList ((PVOID)(pGroupsOld->aEntries[iGroup].idObject)))
            continue;

         if (lpp->fMembership)
            {
            if (!asc_GroupMemberRemove (g.idClient, g.idCell, pGroupsOld->aEntries[iGroup].idObject, lpp->pUsers->aEntries[iUser].idObject, &status))
               ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
            }
         else // (!lpp->fMembership)
            {
            ASOBJPROP Properties;
            if (asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, pGroupsOld->aEntries[iGroup].idObject, &Properties, &status))
               {
               AFSADMSVR_CHANGEGROUP_PARAMS pp;
               memset (&pp, 0x00, sizeof(pp));
               lstrcpy (pp.szOwner, Properties.szName); // make group self-owned
               pp.aaListStatus = Properties.u.GroupProperties.aaListStatus;
               pp.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
               pp.aaListMembers = Properties.u.GroupProperties.aaListMembers;
               pp.aaAddMember = Properties.u.GroupProperties.aaAddMember;
               pp.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;
               if (!asc_GroupChange (g.idClient, g.idCell, pGroupsOld->aEntries[iGroup].idObject, &pp, &status))
                  ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
               }
            }
         }

      // Now the more complex part: see if there are any groups in the
      // supplied group-list which are marked as mandatory, but which
      // aren't in our pGroupsOld list. We'll need to put the latter in a
      // hashlist for this...
      //
      LPHASHLIST pGroupsOldList = New (HASHLIST);
      for (iGroup = 0; iGroup < pGroupsOld->cEntries; ++iGroup)
         pGroupsOldList->AddUnique ((PVOID)(pGroupsOld->aEntries[ iGroup ].idObject));

      for (iGroup = 0; iGroup < lpp->pGroups->cEntries; ++iGroup)
         {
         if (!lpp->pGroups->aEntries[ iGroup ].lParam)
            continue; // group not mandatory
         if (pGroupsOldList->fIsInList ((PVOID)(lpp->pGroups->aEntries[ iGroup ].idObject)))
            continue; // already a member

         if (lpp->fMembership)
            {
            if (!asc_GroupMemberAdd (g.idClient, g.idCell, lpp->pGroups->aEntries[iGroup].idObject, lpp->pUsers->aEntries[iUser].idObject, &status))
               ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
            }
         else // (!lpp->fMembership)
            {
            ASOBJPROP Properties;
            if (asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, lpp->pGroups->aEntries[iGroup].idObject, &Properties, &status))
               {
               AFSADMSVR_CHANGEGROUP_PARAMS pp;
               memset (&pp, 0x00, sizeof(pp));
               pp.aaListStatus = Properties.u.GroupProperties.aaListStatus;
               pp.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
               pp.aaListMembers = Properties.u.GroupProperties.aaListMembers;
               pp.aaAddMember = Properties.u.GroupProperties.aaAddMember;
               pp.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;

               if (asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->pUsers->aEntries[iUser].idObject, pp.szOwner, &status))
                  {
                  if (!asc_GroupChange (g.idClient, g.idCell, lpp->pGroups->aEntries[iGroup].idObject, &pp, &status))
                     ED_RegisterStatus (ped, lpp->pUsers->aEntries[ iUser ].idObject, FALSE, status);
                  }
               }
            }
         }

      Delete (pGroupsOldList);
      asc_AsidListFree (&pGroupsOld);
      }

   // If there were any errors, report them.
   //
   if ((*pStatus = ED_GetFinalStatus(ped)) != 0)
      {
      rc = FALSE;
      ED_ShowErrorDialog(ped);
      }
   ED_Free(ped);

   // Done; clean up
   //
   Delete (pGroupsAllow);
   return rc;
}


void Task_User_CPW (LPTASKPACKET ptp)
{
   LPUSER_CPW_PARAMS lpp = (LPUSER_CPW_PARAMS)( ptp->lpUser );

   ptp->rc = asc_UserPasswordSet (g.idClient, g.idCell, lpp->idUser, lpp->keyVersion, lpp->keyString, lpp->keyData, &ptp->status);

   if ((!ptp->rc) && (!IsWindow(ptp->hReply)))
      {
      TCHAR szName[ cchNAME ];
      User_GetDisplayName (szName, lpp->idUser);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_PASSWORD, TEXT("%s"), szName);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_Unlock (LPTASKPACKET ptp)
{
   LPASIDLIST pUserList = (LPASIDLIST)( ptp->lpUser );

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_UNLOCK, IDS_ERROR_CANT_UNLOCK_MULTIPLE);

   // Try to unlock the users' accounts...
   //
   for (size_t iUser = 0; iUser < pUserList->cEntries; ++iUser)
      {
      if (!asc_UserUnlock (g.idClient, g.idCell, pUserList->aEntries[ iUser ].idObject, &ptp->status))
         ED_RegisterStatus (ped, pUserList->aEntries[ iUser ].idObject, FALSE, ptp->status);
      }

   // If there were any errors, report them.
   //
   if (ED_GetFinalStatus(ped) && !IsWindow(ptp->hReply))
      ED_ShowErrorDialog(ped);
   ED_Free(ped);

   asc_AsidListFree (&pUserList);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_Create (LPTASKPACKET ptp)
{
   LPUSER_CREATE_PARAMS lpp = (LPUSER_CREATE_PARAMS)(ptp->lpUser);

   // We may actually have been asked to create more than one user here;
   // the {lpp->mszNames} parameter is a multi-string. So everything we
   // do, we'll do for each new user-name...
   //
   for (LPTSTR pszName = lpp->mszNames; pszName && *pszName; pszName += 1+lstrlen(pszName))
      {
      // First create this new user account
      //
      ASID idUser;

      AFSADMSVR_CREATEUSER_PARAMS pp;
      memset (&pp, 0x00, sizeof(AFSADMSVR_CREATEUSER_PARAMS));
      User_SplitDisplayName (pszName, pp.szName, pp.szInstance);
      lstrcpy (pp.szPassword, lpp->szPassword);
      pp.idUser = lpp->idUser;
      pp.fCreateKAS = lpp->fCreateKAS;
      pp.fCreatePTS = lpp->fCreatePTS;
      if ((ptp->rc = asc_UserCreate (g.idClient, g.idCell, &pp, &idUser, &ptp->status)) == FALSE)
         {
         if (!IsWindow (ptp->hReply))
            ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_USER, TEXT("%s"), pszName);
         continue;
         }

      // Then change its properties to be what we want
      //
      if ((ptp->rc = asc_UserChange (g.idClient, g.idCell, idUser, &lpp->Properties, &ptp->status)) == FALSE)
         {
         if (!IsWindow (ptp->hReply))
            ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_USER, TEXT("%s"), pszName);
         }
      if (!ptp->rc)
         continue;

      // Finally update its lists of groups
      //
      if (lpp->pGroupsMember)
         {
         USER_GROUPLIST_SET_PARAMS pp;
         memset (&pp, 0x00, sizeof(USER_GROUPLIST_SET_PARAMS));
         asc_AsidListCreate (&pp.pUsers);
         asc_AsidListAddEntry (&pp.pUsers, idUser, 0);
         asc_AsidListCopy (&pp.pGroups, &lpp->pGroupsMember);
         pp.fMembership = TRUE;
         ptp->rc = Task_User_GroupList_Set_Do (&pp, &ptp->status);
         asc_AsidListFree (&pp.pUsers);
         asc_AsidListFree (&pp.pGroups);
         }
      if (lpp->pGroupsOwner)
         {
         USER_GROUPLIST_SET_PARAMS pp;
         memset (&pp, 0x00, sizeof(USER_GROUPLIST_SET_PARAMS));
         asc_AsidListCreate (&pp.pUsers);
         asc_AsidListAddEntry (&pp.pUsers, idUser, 0);
         asc_AsidListCopy (&pp.pGroups, &lpp->pGroupsOwner);
         pp.fMembership = FALSE;
         ptp->rc = Task_User_GroupList_Set_Do (&pp, &ptp->status);
         asc_AsidListFree (&pp.pUsers);
         asc_AsidListFree (&pp.pGroups);
         }
      }

   // And we're done!
   //
   Display_PopulateList();

   if (lpp->pGroupsOwner)
      asc_AsidListFree (&lpp->pGroupsOwner);
   if (lpp->pGroupsMember)
      asc_AsidListFree (&lpp->pGroupsMember);
   if (lpp->mszNames)
      FreeString (lpp->mszNames);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_User_Delete (LPTASKPACKET ptp)
{
   LPUSER_DELETE_PARAMS lpp = (LPUSER_DELETE_PARAMS)(ptp->lpUser);

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_DELETE_USER, IDS_ERROR_CANT_DELETE_USER_MULTIPLE);

   // Go through and delete these users
   //
   for (size_t iUser = 0; iUser < lpp->pUserList->cEntries; ++iUser)
      {
      AFSADMSVR_DELETEUSER_PARAMS pp;
      memset (&pp, 0x00, sizeof(pp));
      pp.fDeleteKAS = lpp->fDeleteKAS;
      pp.fDeletePTS = lpp->fDeletePTS;

      ULONG status;
      if (!asc_UserDelete (g.idClient, g.idCell, lpp->pUserList->aEntries[iUser].idObject, &pp, &status))
         ED_RegisterStatus (ped, lpp->pUserList->aEntries[iUser].idObject, FALSE, status);
      }

   // If there were any errors, report them.
   //
   if (ED_GetFinalStatus(ped) && !IsWindow(ptp->hReply))
      ED_ShowErrorDialog(ped);
   ED_Free(ped);

   // And we're done!
   //
   Display_PopulateList();

   if (lpp->pUserList)
      asc_AsidListFree (&lpp->pUserList);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Change (LPTASKPACKET ptp)
{
   LPGROUP_CHANGE_PARAMS lpp = (LPGROUP_CHANGE_PARAMS)( ptp->lpUser );

   if ((ptp->rc = asc_GroupChange (g.idClient, g.idCell, lpp->idGroup, &lpp->NewProperties, &ptp->status)) == TRUE)
      {
      Display_RefreshView_Fast();
      }

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      TCHAR szGroup[ cchNAME ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->idGroup, szGroup);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_GROUP, TEXT("%s"), szGroup);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Search (LPTASKPACKET ptp)
{
   LPGROUP_SEARCH_PARAMS lpp = (LPGROUP_SEARCH_PARAMS)( ptp->lpUser );

   // Prepare an intermediate place for us to put the results of our search.
   // We'll be doing lots of work in this intermediate list, so it'll be
   // implemented as a hashlist with a key over the objects' ASIDs.
   //
   typedef struct
      {
      ASID idGroup;
      size_t cUsers;
      } GROUP_SEARCH_FOUND, *LPGROUP_SEARCH_FOUND;

   LPHASHLIST pListGroups = New (HASHLIST);
   LPHASHLISTKEY pListKeyAsid = pListGroups->CreateKey ("ASID", Key_Asid_Compare, Key_Asid_HashObject, Key_Asid_HashData);

   // Search through the appropriate groups for each user in the list
   //
   for (size_t iUser = 0; iUser < lpp->pUserList->cEntries; ++iUser)
      {
      LPASIDLIST pGroups = NULL;
      ULONG status;

      if (lpp->fMembership)
         {
         if (!asc_GroupMembershipGet (g.idClient, g.idCell, lpp->pUserList->aEntries[ iUser ].idObject, &pGroups, &status))
            continue;
         }
      else // (!lpp->fMembership)
         {
         if (!asc_GroupOwnershipGet (g.idClient, g.idCell, lpp->pUserList->aEntries[ iUser ].idObject, &pGroups, &status))
            continue;
         }

      if (pGroups)
         {
         // For each group we found, make sure the group is in the big
         // list we'll be returning. Use the {lParam} field of the
         // list's entry as a counter, to remember how many users have
         // this group
         //
         for (size_t iGroup = 0; iGroup < pGroups->cEntries; ++iGroup)
            {
            // Is it in the list already? If not, add it.
            //
            LPGROUP_SEARCH_FOUND pFind;
            if ((pFind = (LPGROUP_SEARCH_FOUND)pListKeyAsid->GetFirstObject (&pGroups->aEntries[ iGroup ].idObject)) != NULL)
               {
               pFind->cUsers ++;
               }
            else
               {
               pFind = New (GROUP_SEARCH_FOUND);
               pFind->idGroup = pGroups->aEntries[ iGroup ].idObject;
               pFind->cUsers = 1;
               pListGroups->Add (pFind);
               }
            }

         asc_AsidListFree (&pGroups);
         }
      }

   // Now that we have a list of groups that match our search criteria,
   // stick it in an ASID list. The lParam field for each ASID will be set
   // to 1 if all users have that group, or 0 if only some have it.
   //
   if (!asc_AsidListCreate (&TASKDATA(ptp)->pAsidList))
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }

   if (ptp->rc)
      {
      for (LPENUM pEnum = pListGroups->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPGROUP_SEARCH_FOUND pFind = (LPGROUP_SEARCH_FOUND)( pEnum->GetObject() );

         asc_AsidListAddEntry (&TASKDATA(ptp)->pAsidList, pFind->idGroup, (LPARAM)( (pFind->cUsers == lpp->pUserList->cEntries) ? 1 : 0 ));

         pListGroups->Remove (pFind);
         Delete (pFind);
         }

      TASKDATA(ptp)->fMembership = lpp->fMembership;
      }

   // Now that we have an ASID list full of groups to return, make sure we
   // have rudimentary properties for all those groups.
   //
   if (ptp->rc)
      {
      LPASIDLIST pList;
      asc_AsidListCopy (&pList, &TASKDATA(ptp)->pAsidList);

      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, pList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);

      asc_AsidListFree (&pList);
      }

   if (pListGroups)
      Delete (pListGroups);
   asc_AsidListFree (&lpp->pUserList);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Members_Get (LPTASKPACKET ptp)
{
   LPASIDLIST pGroups = (LPASIDLIST)( ptp->lpUser );

   // Prepare an intermediate place for us to put the results of our search.
   // We'll be doing lots of work in this intermediate list, so it'll be
   // implemented as a hashlist with a key over the objects' ASIDs.
   //
   typedef struct
      {
      ASID idUser;
      size_t cGroups;
      } GROUP_MEMBER_FOUND, *LPGROUP_MEMBER_FOUND;

   LPHASHLIST pListUsers = New (HASHLIST);
   LPHASHLISTKEY pListKeyAsid = pListUsers->CreateKey ("ASID", Key_Asid_Compare, Key_Asid_HashObject, Key_Asid_HashData);

   // For each group in the list, find all of that group's members
   //
   for (size_t iGroup = 0; iGroup < pGroups->cEntries; ++iGroup)
      {
      LPASIDLIST pMembers = NULL;
      ULONG status;

      if (!asc_GroupMembersGet (g.idClient, g.idCell, pGroups->aEntries[ iGroup ].idObject, &pMembers, &status))
         continue;

      if (pMembers)
         {
         // For each member we found, make sure the member is in the big
         // list we'll be returning. Use the {lParam} field of the
         // list's entry as a counter, to remember how many groups have
         // this member.
         //
         for (size_t iMember = 0; iMember < pMembers->cEntries; ++iMember)
            {
            // Is it in the list already? If not, add it.
            //
            LPGROUP_MEMBER_FOUND pFind;
            if ((pFind = (LPGROUP_MEMBER_FOUND)pListKeyAsid->GetFirstObject (&pMembers->aEntries[ iMember ].idObject)) != NULL)
               {
               pFind->cGroups ++;
               }
            else
               {
               pFind = New (GROUP_MEMBER_FOUND);
               pFind->idUser = pMembers->aEntries[ iMember ].idObject;
               pFind->cGroups = 1;
               pListUsers->Add (pFind);
               }
            }

         asc_AsidListFree (&pMembers);
         }
      }

   // Now that we have a list of users that match our search criteria,
   // stick it in an ASID list. The lParam field for each ASID will be set
   // to 1 if all groups have that member, or 0 if only some have it.
   //
   if (!asc_AsidListCreate (&TASKDATA(ptp)->pAsidList))
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }

   if (ptp->rc)
      {
      for (LPENUM pEnum = pListUsers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPGROUP_MEMBER_FOUND pFind = (LPGROUP_MEMBER_FOUND)( pEnum->GetObject() );

         asc_AsidListAddEntry (&TASKDATA(ptp)->pAsidList, pFind->idUser, (LPARAM)( (pFind->cGroups == pGroups->cEntries) ? 1 : 0 ));

         pListUsers->Remove (pFind);
         Delete (pFind);
         }
      }

   // Now that we have an ASID list full of users to return, make sure we
   // have rudimentary properties for all those users.
   //
   if (ptp->rc)
      {
      LPASIDLIST pList;
      asc_AsidListCopy (&pList, &TASKDATA(ptp)->pAsidList);

      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, pList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);

      asc_AsidListFree (&pList);
      }

   if (pListUsers)
      Delete (pListUsers);
   asc_AsidListFree (&pGroups);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Members_Set (LPTASKPACKET ptp)
{
   LPGROUP_MEMBERS_SET_PARAMS lpp = (LPGROUP_MEMBERS_SET_PARAMS)( ptp->lpUser );

   ptp->rc = Task_Group_Members_Set_Do (lpp, &ptp->status);

   asc_AsidListFree (&lpp->pGroups);
   asc_AsidListFree (&lpp->pMembers);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


BOOL Task_Group_Members_Set_Do (LPGROUP_MEMBERS_SET_PARAMS lpp, ULONG *pStatus)
{
   BOOL rc = TRUE;

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_SET_MEMBERS, IDS_ERROR_CANT_SET_MEMBERS_MULTIPLE);

   // We'll need the supplied member-list in a hashlist, so we can quickly
   // test it to see if a particular member should remain in a group.
   //
   LPHASHLIST pMembersAllow = New (HASHLIST);
   size_t iMember;
   for (iMember = 0; iMember < lpp->pMembers->cEntries; ++iMember)
      pMembersAllow->AddUnique ((PVOID)(lpp->pMembers->aEntries[ iMember ].idObject));

   // We'll have to do this next bit for each group in the supplied group-list
   //
   for (size_t iGroup = 0; iGroup < lpp->pGroups->cEntries; ++iGroup)
      {
      ULONG status;

      // Obtain the current list of members for this group
      //
      LPASIDLIST pMembersOld = NULL;
      if (!asc_GroupMembersGet (g.idClient, g.idCell, lpp->pGroups->aEntries[ iGroup ].idObject, &pMembersOld, &status))
         {
         ED_RegisterStatus (ped, lpp->pGroups->aEntries[ iGroup ].idObject, FALSE, status);
         continue;
         }
      if (!pMembersOld)
         continue;

      // Test each member in that current list to see if it's also on our
      // pMembersAllow list. If not, remove it.
      //
      for (iMember = 0; iMember < pMembersOld->cEntries; ++iMember)
         {
         if (pMembersAllow->fIsInList ((PVOID)(pMembersOld->aEntries[iMember].idObject)))
            continue;

         if (!asc_GroupMemberRemove (g.idClient, g.idCell, lpp->pGroups->aEntries[iGroup].idObject, pMembersOld->aEntries[iMember].idObject, &status))
            ED_RegisterStatus (ped, lpp->pGroups->aEntries[ iGroup ].idObject, FALSE, status);
         }

      // Now the more complex part: see if there are any members in the
      // supplied member-list which are marked as mandatory, but which
      // aren't in our pMembersOld list. We'll need to put the latter in a
      // hashlist for this...
      //
      LPHASHLIST pMembersOldList = New (HASHLIST);
      for (iMember = 0; iMember < pMembersOld->cEntries; ++iMember)
         pMembersOldList->AddUnique ((PVOID)(pMembersOld->aEntries[ iMember ].idObject));

      for (iMember = 0; iMember < lpp->pMembers->cEntries; ++iMember)
         {
         if (!lpp->pMembers->aEntries[ iMember ].lParam)
            continue; // member not mandatory
         if (pMembersOldList->fIsInList ((PVOID)(lpp->pMembers->aEntries[ iMember ].idObject)))
            continue; // already a member

         if (!asc_GroupMemberAdd (g.idClient, g.idCell, lpp->pGroups->aEntries[iGroup].idObject, lpp->pMembers->aEntries[iMember].idObject, &status))
            ED_RegisterStatus (ped, lpp->pGroups->aEntries[ iGroup ].idObject, FALSE, status);
         }

      Delete (pMembersOldList);

      asc_AsidListFree (&pMembersOld);
      }

   // If there were any errors, report them.
   //
   if ((*pStatus = ED_GetFinalStatus(ped)) != 0)
      {
      rc = FALSE;
      ED_ShowErrorDialog(ped);
      }
   ED_Free(ped);

   // Done; clean up
   //
   Delete (pMembersAllow);
   return rc;
}


void Task_Group_Enum (LPTASKPACKET ptp)
{
   LPCTSTR pszPattern = (LPCTSTR)( ptp->lpUser );

   TCHAR szRegExp[ cchNAME ];
   TranslateRegExp (szRegExp, pszPattern);
   if ((ptp->rc = asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_GROUP, szRegExp, NULL, &TASKDATA(ptp)->pAsidList, &ptp->status)) == TRUE)
      {
      LPASOBJPROPLIST pPropList = NULL;

      ptp->rc = asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, TASKDATA(ptp)->pAsidList, &pPropList, &ptp->status);

      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }

   FreeString (pszPattern);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Rename (LPTASKPACKET ptp)
{
   LPGROUP_RENAME_PARAMS lpp = (LPGROUP_RENAME_PARAMS)( ptp->lpUser );

   if ((ptp->rc = asc_GroupRename (g.idClient, g.idCell, lpp->idGroup, lpp->szNewName, &ptp->status)) != FALSE)
      {
      Display_RefreshView_Fast();
      }

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      TCHAR szOldName[ cchNAME ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->idGroup, szOldName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_RENAME_GROUP, TEXT("%s%s"), szOldName, lpp->szNewName);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Owned_Get (LPTASKPACKET ptp)
{
   ASID idGroup = (ASID)( ptp->lpUser );

   ptp->rc = asc_GroupOwnershipGet (g.idClient, g.idCell, idGroup, &TASKDATA(ptp)->pAsidList, &ptp->status);

   // Modify the ASID list to have lParams of 1. (This indicates that all
   // our supplied groups own this thing; silly but consistent with the
   // other such interfaces.)
   //
   if (TASKDATA(ptp)->pAsidList)
      {
      for (size_t ii = 0; ii < TASKDATA(ptp)->pAsidList->cEntries; ++ii)
         TASKDATA(ptp)->pAsidList->aEntries[ ii ].lParam = 1;
      }

   // Now that we have an ASID list full of groups to return, make sure we
   // have rudimentary properties for all those groups.
   //
   if (ptp->rc)
      {
      LPASIDLIST pList;
      asc_AsidListCopy (&pList, &TASKDATA(ptp)->pAsidList);

      LPASOBJPROPLIST pPropList = NULL;
      if (!asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, pList, &pPropList, &ptp->status))
         ptp->rc = FALSE;
      if (pPropList)
         asc_ObjPropListFree (&pPropList);

      asc_AsidListFree (&pList);
      }
}


void Task_Group_Owned_Set (LPTASKPACKET ptp)
{
   LPGROUP_OWNED_SET_PARAMS lpp = (LPGROUP_OWNED_SET_PARAMS)( ptp->lpUser );

   ptp->rc = Task_Group_Owned_Set_Do (lpp, &ptp->status);

   asc_AsidListFree (&lpp->pOwnedGroups);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


BOOL Task_Group_Owned_Set_Do (LPGROUP_OWNED_SET_PARAMS lpp, ULONG *pStatus)
{
   BOOL rc = TRUE;

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_CHANGE_OWNER, IDS_ERROR_CANT_CHANGE_OWNER_MULTIPLE);

   // We'll need the supplied groups-to-own list in a hashlist, so we can
   // quickly test it for inclusion of a particular group.
   //
   LPHASHLIST pGroupsAllow = New (HASHLIST);
   size_t iGroup;
   for (iGroup = 0; iGroup < lpp->pOwnedGroups->cEntries; ++iGroup)
      pGroupsAllow->AddUnique ((PVOID)(lpp->pOwnedGroups->aEntries[ iGroup ].idObject));

   // Obtain the current list-of-groups-owned for this group
   //
   LPASIDLIST pGroupsOld = NULL;
   if ((rc = asc_GroupOwnershipGet (g.idClient, g.idCell, lpp->idGroup, &pGroupsOld, pStatus)) == FALSE)
      pGroupsOld = NULL;

   if (!pGroupsOld)
      {
      for (size_t ii = 0; ii < lpp->pOwnedGroups->cEntries; ++ii)
         ED_RegisterStatus (ped, lpp->pOwnedGroups->aEntries[ii].idObject, FALSE, *pStatus);
      }
   else
      {
      // Test each group in that current list to see if it's also on our
      // pGroupsAllow list. If not, remove it.
      //
      for (iGroup = 0; iGroup < pGroupsOld->cEntries; ++iGroup)
         {
         if (pGroupsAllow->fIsInList ((PVOID)(pGroupsOld->aEntries[iGroup].idObject)))
            continue;

         ULONG status;
         ASOBJPROP Properties;
         if (!asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, pGroupsOld->aEntries[iGroup].idObject, &Properties, &status))
            ED_RegisterStatus (ped, pGroupsOld->aEntries[iGroup].idObject, FALSE, status);
         else
            {
            AFSADMSVR_CHANGEGROUP_PARAMS pp;
            memset (&pp, 0x00, sizeof(pp));
            lstrcpy (pp.szOwner, Properties.szName); // make group self-owned
            pp.aaListStatus = Properties.u.GroupProperties.aaListStatus;
            pp.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
            pp.aaListMembers = Properties.u.GroupProperties.aaListMembers;
            pp.aaAddMember = Properties.u.GroupProperties.aaAddMember;
            pp.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;

            if (!asc_GroupChange (g.idClient, g.idCell, pGroupsOld->aEntries[iGroup].idObject, &pp, &status))
               ED_RegisterStatus (ped, pGroupsOld->aEntries[iGroup].idObject, FALSE, status);
            }
         }

      // Now the more complex part: see if there are any groups in the
      // supplied group-list which are marked as mandatory, but which
      // aren't in our pGroupsOld list. We'll need to put the latter in a
      // hashlist for this...
      //
      LPHASHLIST pGroupsOldList = New (HASHLIST);
      for (iGroup = 0; iGroup < pGroupsOld->cEntries; ++iGroup)
         pGroupsOldList->AddUnique ((PVOID)(pGroupsOld->aEntries[ iGroup ].idObject));

      for (iGroup = 0; iGroup < lpp->pOwnedGroups->cEntries; ++iGroup)
         {
         if (!lpp->pOwnedGroups->aEntries[ iGroup ].lParam)
            continue; // group not mandatory
         if (pGroupsOldList->fIsInList ((PVOID)(lpp->pOwnedGroups->aEntries[ iGroup ].idObject)))
            continue; // already a member

         ULONG status;
         ASOBJPROP Properties;
         if (!asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, lpp->pOwnedGroups->aEntries[iGroup].idObject, &Properties, &status))
            ED_RegisterStatus (ped, lpp->pOwnedGroups->aEntries[iGroup].idObject, FALSE, status);
         else
            {
            AFSADMSVR_CHANGEGROUP_PARAMS pp;
            memset (&pp, 0x00, sizeof(pp));
            pp.aaListStatus = Properties.u.GroupProperties.aaListStatus;
            pp.aaListGroupsOwned = Properties.u.GroupProperties.aaListGroupsOwned;
            pp.aaListMembers = Properties.u.GroupProperties.aaListMembers;
            pp.aaAddMember = Properties.u.GroupProperties.aaAddMember;
            pp.aaDeleteMember = Properties.u.GroupProperties.aaDeleteMember;

            if (asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->idGroup, pp.szOwner, &status))
               {
               if (!asc_GroupChange (g.idClient, g.idCell, lpp->pOwnedGroups->aEntries[iGroup].idObject, &pp, &status))
                  ED_RegisterStatus (ped, lpp->pOwnedGroups->aEntries[iGroup].idObject, FALSE, status);
               }
            }
         }

      Delete (pGroupsOldList);
      asc_AsidListFree (&pGroupsOld);
      }

   // If there were any errors, report them.
   //
   if ((*pStatus = ED_GetFinalStatus(ped)) != 0)
      {
      rc = FALSE;
      ED_ShowErrorDialog(ped);
      }
   ED_Free(ped);

   // Done; clean up
   //
   Delete (pGroupsAllow);
   return rc;
}


void Task_Group_Create (LPTASKPACKET ptp)
{
   LPGROUP_CREATE_PARAMS lpp = (LPGROUP_CREATE_PARAMS)(ptp->lpUser);

   // If no owner was specified, use our current credentials' ID. If
   // we can't get that, we'll make each group self-owned.
   //
   if (!lpp->szOwner[0])
      {
      TCHAR szCell[ cchNAME ];
      if (!AfsAppLib_CrackCredentials (g.hCreds, szCell, lpp->szOwner))
         lpp->szOwner[0] = TEXT('\0');
      }

   // We may actually have been asked to create more than one group here;
   // the {lpp->mszNames} parameter is a multi-string. So everything we
   // do, we'll do for each new group-name...
   //
   for (LPTSTR pszName = lpp->mszNames; pszName && *pszName; pszName += 1+lstrlen(pszName))
      {
      // First create this new group account
      //
      ASID idGroup;

      AFSADMSVR_CREATEGROUP_PARAMS pp;
      memset (&pp, 0x00, sizeof(AFSADMSVR_CREATEGROUP_PARAMS));
      pp.idGroup = (int) lpp->idGroup;
      lstrcpy (pp.szName, pszName);
      lstrcpy (pp.szOwner, lpp->szOwner);
      if (!pp.szOwner[0])
         lstrcpy (pp.szOwner, pszName);

      if ((ptp->rc = asc_GroupCreate (g.idClient, g.idCell, &pp, &idGroup, &ptp->status)) == FALSE)
         {
         if (!IsWindow (ptp->hReply))
            ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_GROUP, TEXT("%s"), pszName);
         continue;
         }

      // Then change its properties to be what we want
      //
      if ((ptp->rc = asc_GroupChange (g.idClient, g.idCell, idGroup, &lpp->Properties, &ptp->status)) == FALSE)
         {
         if (!IsWindow (ptp->hReply))
            ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_GROUP, TEXT("%s"), pszName);
         }
      if (!ptp->rc)
         continue;

      // Finally update its lists of groups
      //
      if (lpp->pMembers)
         {
         GROUP_MEMBERS_SET_PARAMS pp;
         memset (&pp, 0x00, sizeof(GROUP_MEMBERS_SET_PARAMS));
         asc_AsidListCreate (&pp.pGroups);
         asc_AsidListAddEntry (&pp.pGroups, idGroup, 0);
         asc_AsidListCopy (&pp.pMembers, &lpp->pMembers);
         ptp->rc = Task_Group_Members_Set_Do (&pp, &ptp->status);
         asc_AsidListFree (&pp.pGroups);
         asc_AsidListFree (&pp.pMembers);
         }
      if (lpp->pGroupsOwner)
         {
         GROUP_OWNED_SET_PARAMS pp;
         memset (&pp, 0x00, sizeof(GROUP_OWNED_SET_PARAMS));
         pp.idGroup = idGroup;
         asc_AsidListCopy (&pp.pOwnedGroups, &lpp->pGroupsOwner);
         ptp->rc = Task_Group_Owned_Set_Do (&pp, &ptp->status);
         asc_AsidListFree (&pp.pOwnedGroups);
         }
      }

   // And we're done!
   //
   Display_PopulateList();

   if (lpp->pGroupsOwner)
      asc_AsidListFree (&lpp->pGroupsOwner);
   if (lpp->pMembers)
      asc_AsidListFree (&lpp->pMembers);
   if (lpp->mszNames)
      FreeString (lpp->mszNames);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Group_Delete (LPTASKPACKET ptp)
{
   LPASIDLIST pGroupList = (LPASIDLIST)(ptp->lpUser);

   // Maintain a structure describing any errors we encounter, so we can give
   // a reasonable error dialog if need be.
   //
   LPERRORDATA ped = ED_Create (IDS_ERROR_CANT_DELETE_GROUP, IDS_ERROR_CANT_DELETE_GROUP_MULTIPLE);

   // Go through and delete these users
   //
   for (size_t iGroup = 0; iGroup < pGroupList->cEntries; ++iGroup)
      {
      ULONG status;
      if (!asc_GroupDelete (g.idClient, g.idCell, pGroupList->aEntries[iGroup].idObject, &status))
         ED_RegisterStatus (ped, pGroupList->aEntries[iGroup].idObject, FALSE, status);
      }

   // If there were any errors, report them.
   //
   if (ED_GetFinalStatus(ped) && !IsWindow(ptp->hReply))
      ED_ShowErrorDialog(ped);
   ED_Free(ped);

   // And we're done!
   //
   Display_PopulateList();

   if (pGroupList)
      asc_AsidListFree (&pGroupList);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Cell_Change (LPTASKPACKET ptp)
{
   LPCELL_CHANGE_PARAMS lpp = (LPCELL_CHANGE_PARAMS)(ptp->lpUser);

   AFSADMSVR_CHANGECELL_PARAMS Change;
   memset (&Change, 0x00, sizeof(Change));
   Change.idUserMax = (DWORD)(lpp->idUserMax);
   Change.idGroupMax = (DWORD)(lpp->idGroupMax);
   ptp->rc = asc_CellChange (g.idClient, lpp->idCell, &Change, &ptp->status);

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      TCHAR szCell[ cchNAME ];
      asc_CellNameGet_Fast (g.idClient, lpp->idCell, szCell);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGE_CELL, TEXT("%s"), szCell);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_List_Translate (LPTASKPACKET ptp)
{
   LPLIST_TRANSLATE_PARAMS lpp = (LPLIST_TRANSLATE_PARAMS)( ptp->lpUser );
   TASKDATA(ptp)->Type = lpp->Type;

   // We'll need a hashlist into which to dump our results as we build
   // them (we use a hashlist so we can quickly detect and ignore duplicates).
   //
   LPHASHLIST pList = New (HASHLIST);

   // Now split the input string up into a series of names. To do that,
   // we'll valid separator characters are; since names can't have whitespace,
   // we'll include that.
   //
   TCHAR szSeparators[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SLIST, szSeparators, cchRESOURCE))
      szSeparators[0] = TEXT(',');
   GetString (&szSeparators[1], IDS_SEPARATORS);

   if (lpp->pszNames)
      {
      LPCTSTR pszStart = lpp->pszNames;
      while (iswhite(*pszStart) || lstrchr (szSeparators, *pszStart))
         ++pszStart;

      while (*pszStart)
         {
         // Find the first non-name character
         //
         LPCTSTR pszEnd = pszStart;
         while (*pszEnd && !iswhite(*pszEnd) && !lstrchr(szSeparators, *pszEnd))
            ++pszEnd;

         // Copy off this particular name
         //
         TCHAR szName[ cchNAME ];
         lstrcpy (szName, pszStart);
         szName[ pszEnd - pszStart ] = TEXT('\0');

         // Find the next valid-name character
         //
         pszStart = pszEnd;
         while (iswhite(*pszStart) || lstrchr(szSeparators, *pszStart))
            ++pszStart;

         // Translate this particular name. We'll also handle wildcards
         // here: if we don't get any wildcard characters, treat it as
         // a direct lookup; if we do, treat it as a regexp match.
         //
         if (lstrchr (szName, TEXT('*')) || lstrchr (szName, TEXT('?')) ||
             lstrchr (szName, TEXT('^')) || lstrchr (szName, TEXT('!')))
            {
            TCHAR szRegExp[ cchNAME ];
            TranslateRegExp (szRegExp, szName);

            LPASIDLIST pAsidList;
            if (asc_ObjectFindMultiple (g.idClient, g.idCell, lpp->Type, szRegExp, NULL, &pAsidList, &ptp->status))
               {
               if (pAsidList)
                  {
                  for (size_t ii = 0; ii < pAsidList->cEntries; ++ii)
                     pList->AddUnique ((PVOID)(pAsidList->aEntries[ii].idObject));
                  asc_AsidListFree (&pAsidList);
                  }
               }
            }
         else // No wildcards; just look up the name directly
            {
            ASID idObject;
            if (asc_ObjectFind (g.idClient, g.idCell, lpp->Type, szName, &idObject, &ptp->status))
               pList->AddUnique ((PVOID)idObject);
            }
         }
      }

   // Finally, build an ASIDLIST from our hashlist's contents.
   //
   if (!asc_AsidListCreate (&TASKDATA(ptp)->pAsidList))
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }
   else
      {
      for (LPENUM pEnum = pList->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         ASID idObject = (ASID)(pEnum->GetObject());
         asc_AsidListAddEntry (&TASKDATA(ptp)->pAsidList, idObject, 0);
         }
      }

   // Done! Clean up.
   //
   Delete (pList);
   FreeString (lpp->pszNames);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Object_Listen (LPTASKPACKET ptp)
{
   LPOBJECT_LISTEN_PARAMS lpp = (LPOBJECT_LISTEN_PARAMS)( ptp->lpUser );

   if (IsWindow(lpp->hNotify) && (lpp->pAsidList))
      ptp->rc = asc_ObjectListenMultiple (g.idClient, g.idCell, lpp->pAsidList, lpp->hNotify, &ptp->status);
   else
      ptp->rc = asc_ObjectListenClear (g.idClient, lpp->hNotify, &ptp->status);

   if (lpp->pAsidList)
      asc_AsidListFree (&lpp->pAsidList);
   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_Object_Get (LPTASKPACKET ptp)
{
   ASID idObject = (ASID)(ptp->lpUser);
   ptp->rc = asc_ObjectPropertiesGet (g.idClient, GET_ALL_DATA, g.idCell, idObject, &TASKDATA(ptp)->Properties, &ptp->status);
}


void Task_Set_Refresh (LPTASKPACKET ptp)
{
   if (g.idCell)
      {
      ptp->rc = asc_CellRefreshRateSet (g.idClient, g.idCell, gr.cminRefreshRate, &ptp->status);
      }
}


void Task_Expired_Creds (LPTASKPACKET ptp)
{
   if (g.idCell)
      {
      CheckForExpiredCredentials();
      }
}


void WeedAsidList (LPASIDLIST *ppList, BOOL fWantMachines)
{
   ULONG status;

   // First off, we can't do anything unless we have these guys' names.
   //
   if (ppList && (*ppList) && (*ppList)->cEntries)
      {
      LPASOBJPROPLIST pPropList = NULL;
      asc_ObjectPropertiesGetMultiple (g.idClient, GET_RUDIMENTARY_DATA, g.idCell, *ppList, &pPropList, &status);
      if (pPropList)
         asc_ObjPropListFree (&pPropList);
      }

   for (size_t ii = 0; ppList && (*ppList) && (ii < (*ppList)->cEntries); )
      {
      TCHAR szName[ cchRESOURCE ];
      if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, (*ppList)->aEntries[ ii ].idObject, szName, &status))
         {
         ++ii;
         continue;
         }

      if (fIsMachineAccount(szName) == fWantMachines)
         ++ii;
      else
         asc_AsidListRemoveEntryByIndex (ppList, ii);
      }
}


void TranslateRegExp (LPTSTR pszTarget, LPCTSTR pszSource)
{
   if (pszSource)
      {
      if (!gr.fWindowsRegexp)
         {
         lstrcpy (pszTarget, pszSource);
         }
      else
         {
         for ( ; *pszSource; pszSource++)
            {
            switch (*pszSource)
               {
               case TEXT('['):
               case TEXT(']'):
               case TEXT('.'):
                  *pszTarget++ = TEXT('\\');
                  *pszTarget++ = *pszSource;
                  break;

               case TEXT('?'):
                  *pszTarget++ = TEXT('.');
                  break;

               case TEXT('*'):
                  *pszTarget++ = TEXT('.');
                  *pszTarget++ = TEXT('*');
                  break;

               default:
                  *pszTarget++ = *pszSource;
                  break;
               }
            }
         }
      }

   *pszTarget++ = TEXT('\0');
}


BOOL PerformRefresh (LPTASKPACKET ptp, ASID idScope, ULONG *pStatus)
{
   // Invalidate the admin server's cached information about the specified
   // object. Remember that this is recursive hierarchically: if you pass
   // in a cell's ID, for instance, information about all users, groups,
   // servers, services, partitions and volumes anywhere in that cell will
   // be discarded.
   //
   if (!idScope)
      idScope = g.idCell;

   if (!asc_ObjectRefresh (g.idClient, g.idCell, idScope, pStatus))
      return FALSE;

   // The Refresh call above is a misnomer; it's really an Invalidate request,
   // in that it only causes the admin server to dump its cached information
   // over the specified scope. At this point we need to do something to
   // trigger the server to re-query that information, so that it will send
   // an ACTION_REFRESH notification (indicating it's doing that requery),
   // which we'll pick up on and use as a trigger to refresh our display.
   // A convenient way to get the server to do that re-query is to ask it
   // to perform some simple search among the users in the cell--we'll ask
   // it to find, oh, say, "JoeBobUser", and in order to do that, it will
   // have to re-fill its cache.
   //
   LPASIDLIST pListDummy;
   ULONG statusDummy;
   (void)asc_ObjectFindMultiple (g.idClient, g.idCell, TYPE_USER, TEXT("JoeBobUser"), NULL, &pListDummy, &statusDummy);
   if (pListDummy != NULL)
      asc_AsidListFree (&pListDummy);

   return TRUE;
}

