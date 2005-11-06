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

#include <WINNT/TaAfsAdmSvrClient.h>
#include <WINNT/AfsAppLib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct
   {
   BOOL fUseAdminServer;
   UINT_PTR idAdminServerClient;
   } l = {0, 0};


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL AfsAppLib_OpenAdminServer (LPTSTR pszAddress, ULONG *pStatus)
{
   AfsAppLib_CloseAdminServer();

   ULONG status;
   if (!asc_AdminServerOpen (pszAddress, &l.idAdminServerClient, &status))
      {
      if (pStatus)
         *pStatus = status;
      return FALSE;
      }

   l.fUseAdminServer = TRUE;
   return TRUE;
}


void AfsAppLib_CloseAdminServer (void)
{
   if (l.fUseAdminServer)
      {
      ULONG status;
      asc_AdminServerClose (l.idAdminServerClient, &status);
      l.idAdminServerClient = 0;
      l.fUseAdminServer = FALSE;
      }
}


UINT_PTR AfsAppLib_GetAdminServerClientID (void)
{
   return (l.fUseAdminServer) ? l.idAdminServerClient : 0;
}


/*
 * WRAPPERS ___________________________________________________________________
 *
 * This really ugly hack allows other processes to call the asc_* routines
 * in the context of the AfsAppLib library.
 *
 */

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListCreate (LPASIDLIST *ppList)
{
   return asc_AsidListCreate (ppList);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListCopy (LPASIDLIST *ppListTarget, LPASIDLIST *ppListSource)
{
   return asc_AsidListCopy (ppListTarget, ppListSource);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListAddEntry (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   return asc_AsidListAddEntry (ppList, idObject, lp);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListRemoveEntry (LPASIDLIST *ppList, ASID idObject)
{
   return asc_AsidListRemoveEntry (ppList, idObject);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListRemoveEntryByIndex (LPASIDLIST *ppList, size_t iIndex)
{
   return asc_AsidListRemoveEntryByIndex (ppList, iIndex);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListSetEntryParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   return asc_AsidListSetEntryParam (ppList, idObject, lp);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListSetEntryParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp)
{
   return asc_AsidListSetEntryParamByIndex (ppList, iIndex, lp);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListTest (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam)
{
   return asc_AsidListTest (ppList, idObject, pParam);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListFree (LPASIDLIST *ppList)
{
   return asc_AsidListFree (ppList);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListCreate (LPASOBJPROPLIST *ppList)
{
   return asc_ObjPropListCreate (ppList);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListCopy (LPASOBJPROPLIST *ppListTarget, LPASOBJPROPLIST *ppListSource)
{
   return asc_ObjPropListCopy (ppListTarget, ppListSource);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListAddEntry (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp)
{
   return asc_ObjPropListAddEntry (ppList, pProperties, lp);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListRemoveEntry (LPASOBJPROPLIST *ppList, ASID idObject)
{
   return asc_ObjPropListRemoveEntry (ppList, idObject);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListTest (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties, LPARAM *pParam)
{
   return asc_ObjPropListTest (ppList, idObject, pProperties, pParam);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListFree (LPASOBJPROPLIST *ppList)
{
   return asc_ObjPropListFree (ppList);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListCreate (LPASACTIONLIST *ppList)
{
   return asc_ActionListCreate (ppList);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListCopy (LPASACTIONLIST *ppListTarget, LPASACTIONLIST *ppListSource)
{
   return asc_ActionListCopy (ppListTarget, ppListSource);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListAddEntry (LPASACTIONLIST *ppList, LPASACTION pAction)
{
   return asc_ActionListAddEntry (ppList, pAction);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListRemoveEntry (LPASACTIONLIST *ppList, DWORD idAction)
{
   return asc_ActionListRemoveEntry (ppList, idAction);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListTest (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction)
{
   return asc_ActionListTest (ppList, idAction, pAction);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListFree (LPASACTIONLIST *ppList)
{
   return asc_ActionListFree (ppList);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_AdminServerOpen (LPCTSTR pszAddress, UINT_PTR *pidClient, ULONG *pStatus)
{
   return asc_AdminServerOpen (pszAddress, pidClient, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AdminServerClose (UINT_PTR idClient, ULONG *pStatus)
{
   return asc_AdminServerClose (idClient, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_CredentialsCrack (UINT_PTR idClient, PVOID hCreds, LPTSTR pszCell, LPTSTR pszUser, SYSTEMTIME *pstExpiration, ULONG *pStatus)
{
   return asc_CredentialsCrack (idClient, hCreds, pszCell, pszUser, pstExpiration, pStatus);
}

EXPORTED UINT_PTR ADMINAPI AfsAppLib_asc_CredentialsGet (UINT_PTR idClient, LPCTSTR pszCell, ULONG *pStatus)
{
   return asc_CredentialsGet (idClient, pszCell, pStatus);
}

EXPORTED UINT_PTR ADMINAPI AfsAppLib_asc_CredentialsSet (UINT_PTR idClient, LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus)
{
   return asc_CredentialsSet (idClient, pszCell, pszUser, pszPassword, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CredentialsPush (UINT_PTR idClient, PVOID hCreds, ASID idCell, ULONG *pStatus)
{
   return asc_CredentialsPush (idClient, hCreds, idCell, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_LocalCellGet (UINT_PTR idClient, LPTSTR pszCell, ULONG *pStatus)
{
   return asc_LocalCellGet (idClient, pszCell, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ErrorCodeTranslate (UINT_PTR idClient, ULONG code, LANGID idLanguage, STRING pszErrorText, ULONG *pStatus)
{
   return asc_ErrorCodeTranslate (idClient, code, idLanguage, pszErrorText, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionGet (UINT_PTR idClient, DWORD idAction, LPASACTION pAction, ULONG *pStatus)
{
   return asc_ActionGet (idClient, idAction, pAction, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionGetMultiple (UINT_PTR idClient, UINT_PTR idClientSearch, ASID idCellSearch, LPASACTIONLIST *ppList, ULONG *pStatus)
{
   return asc_ActionGetMultiple (idClient, idClientSearch, idCellSearch, ppList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListen (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   return asc_ActionListen (idClient, hNotify, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   return asc_ActionListenClear (idClient, hNotify, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellOpen (UINT_PTR idClient, PVOID hCreds, LPCTSTR pszCell, DWORD dwScope, ASID *pidCell, ULONG *pStatus)
{
   return asc_CellOpen (idClient, hCreds, pszCell, dwScope, pidCell, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellClose (UINT_PTR idClient, ASID idCell, ULONG *pStatus)
{
   return asc_CellClose (idClient, idCell, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellChange (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CHANGECELL_PARAMS pChange, ULONG *pStatus)
{
   return asc_CellChange (idClient, idCell, pChange, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellRefreshRateSet (UINT_PTR idClient, ASID idCell, LONG cminRefreshRate, ULONG *pStatus)
{
   return asc_CellRefreshRateSet (idClient, idCell, cminRefreshRate, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectFind (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszName, ASID *pidObject, ULONG *pStatus)
{
   return asc_ObjectFind (idClient, idSearchScope, ObjectType, pszName, pidObject, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectFindMultiple (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszPattern, LPAFSADMSVR_SEARCH_PARAMS pSearchParams, LPASIDLIST *ppList, ULONG *pStatus)
{
   return asc_ObjectFindMultiple (idClient, idSearchScope, ObjectType, pszPattern, pSearchParams, ppList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGet (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus)
{
   return asc_ObjectPropertiesGet (idClient, GetLevel, idCell, idObject, pProperties, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGetMultiple (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, LPASIDLIST pAsidList, LPASOBJPROPLIST *ppPropertiesList, ULONG *pStatus)
{
   return asc_ObjectPropertiesGetMultiple (idClient, GetLevel, idCell, pAsidList, ppPropertiesList, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListen (UINT_PTR idClient, ASID idCell, ASID idObject, HWND hNotify, ULONG *pStatus)
{
   return asc_ObjectListen (idClient, idCell, idObject, hNotify, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   return asc_ObjectListenClear (idClient, hNotify, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListenMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, HWND hNotify, ULONG *pStatus)
{
   return asc_ObjectListenMultiple (idClient, idCell, pAsidList, hNotify, pStatus);
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectRefresh (UINT_PTR idClient, ASID idCell, ASID idObject, ULONG *pStatus)
{
   return asc_ObjectRefresh (idClient, idCell, idObject, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectRefreshMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, ULONG *pStatus)
{
   return asc_ObjectRefreshMultiple (idClient, idCell, pAsidList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_RandomKeyGet (UINT_PTR idClient, ASID idCell, PBYTE pkey, ULONG *pStatus)
{
   return asc_RandomKeyGet (idClient, idCell, pkey, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellNameGet_Fast (UINT_PTR idClient, ASID idCell, LPTSTR pszCell, ULONG *pStatus)
{
   return asc_CellNameGet_Fast (idClient, idCell, pszCell, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectNameGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPTSTR pszObject, ULONG *pStatus)
{
   return asc_ObjectNameGet_Fast (idClient, idCell, idObject, pszObject, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectTypeGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, ASOBJTYPE *pObjectType, ULONG *pStatus)
{
   return asc_ObjectTypeGet_Fast (idClient, idCell, idObject, pObjectType, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus)
{
   return asc_ObjectPropertiesGet_Fast (idClient, idCell, idObject, pProperties, pStatus);
}


EXPORTED void ADMINAPI AfsAppLib_asc_Enter (void)
{
   asc_Enter();
}

EXPORTED void ADMINAPI AfsAppLib_asc_Leave (void)
{
   asc_Leave();
}

EXPORTED LPCRITICAL_SECTION ADMINAPI AfsAppLib_asc_GetCriticalSection (void)
{
   return asc_GetCriticalSection();
}


EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserChange (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_CHANGEUSER_PARAMS pChange, ULONG *pStatus)
{
   return asc_UserChange (idClient, idCell, idUser, pChange, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserPasswordSet (UINT_PTR idClient, ASID idCell, ASID idUser, int keyVersion, LPCTSTR pkeyString, PBYTE pkeyData, ULONG *pStatus)
{
   return asc_UserPasswordSet (idClient, idCell, idUser, keyVersion, pkeyString, pkeyData, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserUnlock (UINT_PTR idClient, ASID idCell, ASID idUser, ULONG *pStatus)
{
   return asc_UserUnlock (idClient, idCell, idUser, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEUSER_PARAMS pCreate, ASID *pidUser, ULONG *pStatus)
{
   return asc_UserCreate (idClient, idCell, pCreate, pidUser, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserDelete (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_DELETEUSER_PARAMS pDelete, ULONG *pStatus)
{
   return asc_UserDelete (idClient, idCell, idUser, pDelete, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupChange (UINT_PTR idClient, ASID idCell, ASID idGroup, LPAFSADMSVR_CHANGEGROUP_PARAMS pChange, ULONG *pStatus)
{
   return asc_GroupChange (idClient, idCell, idGroup, pChange, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMembersGet (UINT_PTR idClient, ASID idCell, ASID idGroup, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   return asc_GroupMembersGet (idClient, idCell, idGroup, ppAsidList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMemberAdd (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   return asc_GroupMemberAdd (idClient, idCell, idGroup, idMember, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMemberRemove (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus)
{
   return asc_GroupMemberRemove (idClient, idCell, idGroup, idMember, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupRename (UINT_PTR idClient, ASID idCell, ASID idGroup, LPCTSTR pszNewName, ULONG *pStatus)
{
   return asc_GroupRename (idClient, idCell, idGroup, pszNewName, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupOwnershipGet (UINT_PTR idClient, ASID idCell, ASID idMember, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   return asc_GroupOwnershipGet (idClient, idCell, idMember, ppAsidList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMembershipGet (UINT_PTR idClient, ASID idCell, ASID idOwner, LPASIDLIST *ppAsidList, ULONG *pStatus)
{
   return asc_GroupMembershipGet (idClient, idCell, idOwner, ppAsidList, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEGROUP_PARAMS pCreate, ASID *pidGroup, ULONG *pStatus)
{
   return asc_GroupCreate (idClient, idCell, pCreate, pidGroup, pStatus);
}

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupDelete (UINT_PTR idClient, ASID idCell, ASID idGroup, ULONG *pStatus)
{
   return asc_GroupDelete (idClient, idCell, idGroup, pStatus);
}

