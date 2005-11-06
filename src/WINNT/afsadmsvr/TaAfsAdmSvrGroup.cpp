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

#include "TaAfsAdmSvrInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */


      // AfsAdmSvr_ChangeGroup
      // ...changes a group account's properties.
      //
extern "C" int AfsAdmSvr_ChangeGroup (UINT_PTR idClient, ASID idCell, ASID idGroup, LPAFSADMSVR_CHANGEGROUP_PARAMS pChange, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_CHANGE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.Group_Change.idGroup = idGroup;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeGroup (idGroup=0x%08lX)"), idClient, idGroup);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Find this group's current properties
   //
   LPASOBJPROP pCurrentProperties;
   if ((pCurrentProperties = AfsAdmSvr_GetCurrentProperties (idGroup, pStatus)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: ChangeGroup failed; no properties"), idClient);
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   // Build an AFSCLASS-style GROUPPROPERTIES structure that reflects the
   // new properties for the user; mark the structure's dwMask bit to indicate
   // what we're changing.
   //
   GROUPPROPERTIES NewProperties;
   memset (&NewProperties, 0x00, sizeof(NewProperties));

   if (!pChange->szOwner[0])
      lstrcpy (NewProperties.szOwner, pCurrentProperties->u.GroupProperties.szOwner);
   else
      {
      lstrcpy (NewProperties.szOwner, pChange->szOwner);
      if (lstrcmpi (NewProperties.szOwner, pCurrentProperties->u.GroupProperties.szOwner))
         NewProperties.dwMask |= MASK_GROUPPROP_szOwner;
      }

   if ((NewProperties.aaListStatus = pChange->aaListStatus) != pCurrentProperties->u.GroupProperties.aaListStatus)
      NewProperties.dwMask |= MASK_GROUPPROP_aaListStatus;
   if ((NewProperties.aaListGroupsOwned = pChange->aaListGroupsOwned) != pCurrentProperties->u.GroupProperties.aaListGroupsOwned)
      NewProperties.dwMask |= MASK_GROUPPROP_aaListGroupsOwned;
   if ((NewProperties.aaListMembers = pChange->aaListMembers) != pCurrentProperties->u.GroupProperties.aaListMembers)
      NewProperties.dwMask |= MASK_GROUPPROP_aaListMembers;
   if ((NewProperties.aaAddMember = pChange->aaAddMember) != pCurrentProperties->u.GroupProperties.aaAddMember)
      NewProperties.dwMask |= MASK_GROUPPROP_aaAddMember;
   if ((NewProperties.aaDeleteMember = pChange->aaDeleteMember) != pCurrentProperties->u.GroupProperties.aaDeleteMember)
      NewProperties.dwMask |= MASK_GROUPPROP_aaDeleteMember;

   // If we've decided to change anything, call AfsClass to actually do it
   //
   if (NewProperties.dwMask == 0)
      {
      Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeGroup succeeded (nothing to do)"), idClient);
      }
   else
      {
      ULONG status;
      if (!AfsClass_SetGroupProperties ((LPIDENT)idGroup, &NewProperties, &status))
         {
         Print (dlERROR, TEXT("Client 0x%08lX: ChangeGroup failed; error 0x%08lX"), idClient, status);
         return FALSE_(status,pStatus,iOp);
         }

      Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeGroup succeeded"), idClient);
      }

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_GetGroupMembers
      // ...retrieves the list of users which belong to a group
      //
extern "C" int AfsAdmSvr_GetGroupMembers (UINT_PTR idClient, ASID idCell, ASID idGroup, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupMembers (idGroup=0x%08lX)"), idClient, idGroup);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Use AfsClass to get the list of group members
   //
   if (GetAsidType (idGroup) != itGROUP)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   ULONG status;
   LPPTSGROUP lpGroup;
   if ((lpGroup = ((LPIDENT)idGroup)->OpenGroup (&status)) == NULL)
      return FALSE_(status,pStatus,iOp);

   LPTSTR pmszUsers = NULL;
   lpGroup->GetMembers (&pmszUsers);
   lpGroup->Close();

   // Then translate those user names into an ASID list
   //
   if ((*ppAsidList = AfsAdmSvr_CreateAsidList()) == NULL)
      {
      if (pmszUsers)
         FreeString (pmszUsers);
      return FALSE_(ERROR_NOT_ENOUGH_MEMORY,pStatus,iOp);
      }

   if (pmszUsers)
      {
      for (LPTSTR psz = pmszUsers; psz && *psz; psz += 1+lstrlen(psz))
         {
         LPIDENT lpi;
         if ((lpi = IDENT::FindGroup ((LPIDENT)idCell, psz)) == NULL)
            {
            TCHAR szName[ cchNAME ];
            TCHAR szInstance[ cchNAME ];
            USER::SplitUserName (psz, szName, szInstance);

            if ((lpi = IDENT::FindUser ((LPIDENT)idCell, szName, szInstance)) == NULL)
               {
               continue;
               }
            }

         AfsAdmSvr_AddToAsidList (ppAsidList, (ASID)lpi, 0);
         }
      FreeString (pmszUsers);
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupMembers succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_GetGroupMembership
      // ...retrieves the list of groups to which a user or group belongs
      //
extern "C" int AfsAdmSvr_GetGroupMembership (UINT_PTR idClient, ASID idCell, ASID idMember, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupMembership (idMember=0x%08lX)"), idClient, idMember);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Use AfsClass to get the appropriate list of groups
   //
   ULONG status;
   LPTSTR pmszGroups = NULL;

   if (GetAsidType (idMember) == itUSER)
      {
      LPUSER lpUser;
      if ((lpUser = ((LPIDENT)idMember)->OpenUser (&status)) == NULL)
         return FALSE_(status,pStatus,iOp);
      lpUser->GetMemberOf (&pmszGroups);
      lpUser->Close();
      }
   else if (GetAsidType (idMember) == itGROUP)
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = ((LPIDENT)idMember)->OpenGroup (&status)) == NULL)
         return FALSE_(status,pStatus,iOp);
      lpGroup->GetMemberOf (&pmszGroups);
      lpGroup->Close();
      }
   else // bogus type
      {
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);
      }

   // Then translate those group names into an ASID list
   //
   if ((*ppAsidList = AfsAdmSvr_CreateAsidList()) == NULL)
      {
      if (pmszGroups)
         FreeString (pmszGroups);
      return FALSE_(ERROR_NOT_ENOUGH_MEMORY,pStatus,iOp);
      }

   if (pmszGroups)
      {
      for (LPTSTR psz = pmszGroups; psz && *psz; psz += 1+lstrlen(psz))
         {
         LPIDENT lpi;
         if ((lpi = IDENT::FindGroup ((LPIDENT)idCell, psz)) == NULL)
            continue;
         AfsAdmSvr_AddToAsidList (ppAsidList, (ASID)lpi, 0);
         }
      FreeString (pmszGroups);
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupMembership succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_GetGroupOwnership
      // ...retrieves the list of groups which a user owns
      //
extern "C" int AfsAdmSvr_GetGroupOwnership (UINT_PTR idClient, ASID idCell, ASID idOwner, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupOwnership (idOwner=0x%08lX)"), idClient, idOwner);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Use AfsClass to get the appropriate list of groups
   //
   ULONG status;
   LPTSTR pmszGroups = NULL;

   if (GetAsidType (idOwner) == itUSER)
      {
      LPUSER lpUser;
      if ((lpUser = ((LPIDENT)idOwner)->OpenUser (&status)) == NULL)
         return FALSE_(status,pStatus,iOp);
      lpUser->GetOwnerOf (&pmszGroups);
      lpUser->Close();
      }
   else if (GetAsidType (idOwner) == itGROUP)
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = ((LPIDENT)idOwner)->OpenGroup (&status)) == NULL)
         return FALSE_(status,pStatus,iOp);
      lpGroup->GetOwnerOf (&pmszGroups);
      lpGroup->Close();
      }
   else // bogus type
      {
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);
      }

   // Then translate those group names into an ASID list
   //
   if ((*ppAsidList = AfsAdmSvr_CreateAsidList()) == NULL)
      {
      if (pmszGroups)
         FreeString (pmszGroups);
      return FALSE_(ERROR_NOT_ENOUGH_MEMORY,pStatus,iOp);
      }

   if (pmszGroups)
      {
      for (LPTSTR psz = pmszGroups; psz && *psz; psz += 1+lstrlen(psz))
         {
         LPIDENT lpi;
         if ((lpi = IDENT::FindGroup ((LPIDENT)idCell, psz)) == NULL)
            continue;
         AfsAdmSvr_AddToAsidList (ppAsidList, (ASID)lpi, 0);
         }
      FreeString (pmszGroups);
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetGroupOwnership succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_AddGroupMember
      // ...adds a member to the specified group
      //
extern "C" int AfsAdmSvr_AddGroupMember (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_MEMBER_ADD;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.Group_Member_Add.idGroup = idGroup;
   Action.u.Group_Member_Add.idUser = idMember;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: AddGroupMember (idGroup=0x%08lX, idUser=0x%08lX)"), idClient, idGroup, idMember);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Modify the group as requested
   //
   ULONG status;
   if (!AfsClass_AddUserToGroup ((LPIDENT)idGroup, (LPIDENT)idMember, &status))
      return FALSE_(status,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: AddGroupMember succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_RemoveGroupMember
      // ...removes a member from the specified group
      //
extern "C" int AfsAdmSvr_RemoveGroupMember (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_MEMBER_REMOVE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.Group_Member_Remove.idGroup = idGroup;
   Action.u.Group_Member_Remove.idUser = idMember;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RemoveGroupMember (idGroup=0x%08lX, idUser=0x%08lX)"), idClient, idGroup, idMember);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Modify the group as requested
   //
   ULONG status;
   if (!AfsClass_RemoveUserFromGroup ((LPIDENT)idGroup, (LPIDENT)idMember, &status))
      return FALSE_(status,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RemoveGroupMember succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_RenameGroup
      // ...changes a group's name
      //
extern "C" int AfsAdmSvr_RenameGroup (UINT_PTR idClient, ASID idCell, ASID idGroup, STRING szNewGroupName, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_RENAME;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.Group_Rename.idGroup = idGroup;
   lstrcpy (Action.u.Group_Rename.szNewName, szNewGroupName);
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RenameGroup (idGroup=0x%08lX, szNewName=%s)"), idClient, idGroup, szNewGroupName);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Modify the group as requested
   //
   ULONG status;
   if (!AfsClass_RenameGroup ((LPIDENT)idGroup, szNewGroupName, &status))
      return FALSE_(status,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RenameGroup succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_CreateGroup
      // ...creates a new PTS group
      //
extern "C" int AfsAdmSvr_CreateGroup (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEGROUP_PARAMS pCreate, ASID *pidGroup, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_CREATE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   lstrcpy (Action.u.Group_Create.szGroup, pCreate->szName);
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CreateGroup (szGroup=%s)"), idClient, pCreate->szName);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Find the owner (if we can)
   //
   LPIDENT lpiOwner;
   if ((lpiOwner = IDENT::FindUser ((LPIDENT)idCell, pCreate->szOwner)) == NULL)
      lpiOwner = IDENT::FindGroup ((LPIDENT)idCell, pCreate->szOwner);

   // Create the group account
   //
   ULONG status;
   LPIDENT lpiGroup;
   if ((lpiGroup = AfsClass_CreateGroup ((LPIDENT)idCell, pCreate->szName, lpiOwner, pCreate->idGroup, &status)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: CreateGroup failed; error 0x%08lX"), idClient, status);
      return FALSE_(status,pStatus,iOp);
      }

   if (pidGroup)
      *pidGroup = (ASID)lpiGroup;

   // Creating a group account may change the max group ID
   AfsAdmSvr_TestProperties (idCell);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CreateGroup succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_DeleteGroup
      // ...deletes a PTS group
      //
extern "C" int AfsAdmSvr_DeleteGroup (UINT_PTR idClient, ASID idCell, ASID idGroup, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_GROUP_DELETE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.Group_Delete.idGroup = idGroup;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: DeleteGroup (idGroup=0x%08lX)"), idClient, idGroup);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Delete the group
   //
   ULONG status;
   if (!AfsClass_DeleteGroup ((LPIDENT)idGroup, &status))
      {
      Print (dlERROR, TEXT("Client 0x%08lX: DeleteGroup failed; error 0x%08lX"), idClient, status);
      return FALSE_(status,pStatus,iOp);
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: DeleteGroup succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}

