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

#include "TaAfsAdmSvrClientInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL ADMINAPI asc_GroupChange (DWORD idClient, ASID idCell, ASID idGroup, LPAFSADMSVR_CHANGEGROUP_PARAMS pChange, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_ChangeGroup (idClient, idCell, idGroup, pChange, &status)) != FALSE)
         {
         // If we succeeded in changing this group's properties, get the
         // newest values for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idGroup, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupMembersGet (DWORD idClient, ASID idCell, ASID idGroup, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_GetGroupMembers (idClient, idCell, idGroup, ppAsidList, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupMemberAdd (DWORD idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_AddGroupMember (idClient, idCell, idGroup, idMember, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupMemberRemove (DWORD idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_RemoveGroupMember (idClient, idCell, idGroup, idMember, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupRename (DWORD idClient, ASID idCell, ASID idGroup, LPCTSTR pszNewName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szNewName = TEXT("");
      if (pszNewName)
         lstrcpy (szNewName, pszNewName);
      if ((rc = AfsAdmSvr_RenameGroup (idClient, idCell, idGroup, szNewName, &status)) == TRUE)
         {
         // If we succeeded in changing this group's name, get the
         // newest group properties for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idGroup, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupMembershipGet (DWORD idClient, ASID idCell, ASID idMember, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_GetGroupMembership (idClient, idCell, idMember, ppAsidList, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupOwnershipGet (DWORD idClient, ASID idCell, ASID idOwner, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_GetGroupOwnership (idClient, idCell, idOwner, ppAsidList, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupCreate (DWORD idClient, ASID idCell, LPAFSADMSVR_CREATEGROUP_PARAMS pCreate, ASID *pidGroup, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_CreateGroup (idClient, idCell, pCreate, pidGroup, &status)) == TRUE)
         {
         // If we succeeded in creating this group, get the
         // initial group properties for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, *pidGroup, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_GroupDelete (DWORD idClient, ASID idCell, ASID idGroup, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_DeleteGroup (idClient, idCell, idGroup, &status)) == TRUE)
         {
         // If we succeeded in deleting this group, clean up our cache.
         // Expect this call to fail (the group's deleted, right?)
         //
         ASOBJPROP Properties;
         ULONG dummy;
         (void)asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idGroup, &Properties, &dummy);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}

