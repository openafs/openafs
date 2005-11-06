/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AL_ADMSVR_H
#define AL_ADMSVR_H

/*
 * ADMIN SERVER INTERFACE _____________________________________________________
 *
 * The AfsAppLib .DLL links with the admin server client library,
 * TaAfsAdmSvrClient.lib. Moreover, when you call AfsAppLib_OpenAdminServer(),
 * it's AfsAppLib's instance of that .LIB which gets initialized and used
 * by AfsAppLib.
 *
 * Ah, but the problem is: if you want your app to access the asc_* functions
 * as well (aside from just indirectly using them by calling AfsAppLib_*
 * functions), you'll have to magically access the .LIB inside of AfsAppLib--
 * if you just link your app with the .LIB, you'll find your copy of the .LIB
 * never is initialized.
 *
 * So for those of you who want to use the asc_* routines directly AND want
 * to the AfsAppLib library, including this header file remaps all asc_*
 * routines to ensure they'll call within al_admsvr.cpp.
 *
 * First, note that AfsAppLib exposes its own wrappers for its .LIB:
 *
 */

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListCreate (LPASIDLIST *ppList);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListCopy (LPASIDLIST *ppListTarget, LPASIDLIST *ppListSource);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListAddEntry (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListRemoveEntry (LPASIDLIST *ppList, ASID idObject);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListRemoveEntryByIndex (LPASIDLIST *ppList, size_t iIndex);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListSetEntryParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListSetEntryParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListTest (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AsidListFree (LPASIDLIST *ppList);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListCreate (LPASOBJPROPLIST *ppList);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListCopy (LPASOBJPROPLIST *ppListTarget, LPASOBJPROPLIST *ppListSource);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListAddEntry (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListRemoveEntry (LPASOBJPROPLIST *ppList, ASID idObject);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListTest (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties = NULL, LPARAM *pParam = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjPropListFree (LPASOBJPROPLIST *ppList);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListCreate (LPASACTIONLIST *ppList);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListCopy (LPASACTIONLIST *ppListTarget, LPASACTIONLIST *ppListSource);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListAddEntry (LPASACTIONLIST *ppList, LPASACTION pAction);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListRemoveEntry (LPASACTIONLIST *ppList, DWORD idAction);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListTest (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListFree (LPASACTIONLIST *ppList);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_AdminServerOpen (LPCTSTR pszAddress, UINT_PTR *pidClient, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_AdminServerClose (UINT_PTR idClient, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CredentialsCrack (UINT_PTR idClient, PVOID hCreds, LPTSTR pszCell, LPTSTR pszUser, SYSTEMTIME *pstExpiration, ULONG *pStatus);
EXPORTED UINT_PTR ADMINAPI AfsAppLib_asc_CredentialsGet (UINT_PTR idClient, LPCTSTR pszCell, ULONG *pStatus);
EXPORTED UINT_PTR ADMINAPI AfsAppLib_asc_CredentialsSet (UINT_PTR idClient, LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_CredentialsPush (UINT_PTR idClient, PVOID hCreds, ASID idCell, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_LocalCellGet (UINT_PTR idClient, LPTSTR pszCell, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ErrorCodeTranslate (UINT_PTR idClient, ULONG code, LANGID idLanguage, STRING pszErrorText, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionGet (UINT_PTR idClient, DWORD idAction, LPASACTION pAction, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionGetMultiple (UINT_PTR idClient, UINT_PTR idClientSearch, ASID idCellSearch, LPASACTIONLIST *ppList, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListen (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ActionListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellOpen (UINT_PTR idClient, PVOID hCreds, LPCTSTR pszCell, DWORD dwScope, ASID *pidCell, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellClose (UINT_PTR idClient, ASID idCell, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellChange (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CHANGECELL_PARAMS pChange, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellRefreshRateSet (UINT_PTR idClient, ASID idCell, LONG cminRefreshRate, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectFind (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszName, ASID *pidObject, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectFindMultiple (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszPattern, LPAFSADMSVR_SEARCH_PARAMS pSearchParams, LPASIDLIST *ppList, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGet (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGetMultiple (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, LPASIDLIST pAsidList, LPASOBJPROPLIST *ppPropertiesList, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListen (UINT_PTR idClient, ASID idCell, ASID idObject, HWND hNotify, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectListenMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, HWND hNotify, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectRefresh (UINT_PTR idClient, ASID idCell, ASID idObject, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectRefreshMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_RandomKeyGet (UINT_PTR idClient, ASID idCell, PBYTE pkey, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_CellNameGet_Fast (UINT_PTR idClient, ASID idCell, LPTSTR pszCell, ULONG *pStatus = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectNameGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPTSTR pszObject, ULONG *pStatus = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectTypeGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, ASOBJTYPE *pObjectType, ULONG *pStatus = NULL);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_ObjectPropertiesGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus = NULL);

EXPORTED void ADMINAPI AfsAppLib_asc_Enter (void);
EXPORTED void ADMINAPI AfsAppLib_asc_Leave (void);
EXPORTED LPCRITICAL_SECTION ADMINAPI AfsAppLib_asc_GetCriticalSection (void);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserChange (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_CHANGEUSER_PARAMS pChange, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserPasswordSet (UINT_PTR idClient, ASID idCell, ASID idUser, int keyVersion, LPCTSTR pkeyString, PBYTE pkeyData, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserUnlock (UINT_PTR idClient, ASID idCell, ASID idUser, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEUSER_PARAMS pCreate, ASID *pidUser, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_UserDelete (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_DELETEUSER_PARAMS pDelete, ULONG *pStatus);

EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupChange (UINT_PTR idClient, ASID idCell, ASID idGroup, LPAFSADMSVR_CHANGEGROUP_PARAMS pChange, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMembersGet (UINT_PTR idClient, ASID idCell, ASID idGroup, LPASIDLIST *ppAsidList, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMemberAdd (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMemberRemove (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupRename (UINT_PTR idClient, ASID idCell, ASID idGroup, LPCTSTR pszNewName, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupMembershipGet (UINT_PTR idClient, ASID idCell, ASID idMember, LPASIDLIST *ppAsidList, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupOwnershipGet (UINT_PTR idClient, ASID idCell, ASID idOwner, LPASIDLIST *ppAsidList, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEGROUP_PARAMS pCreate, ASID *pidGroup, ULONG *pStatus);
EXPORTED BOOL ADMINAPI AfsAppLib_asc_GroupDelete (UINT_PTR idClient, ASID idCell, ASID idGroup, ULONG *pStatus);

/*
 * Then, note that AfsAppLib remaps all the asc_* routines to call their
 * AfsAppLib_ equivalents:
 *
 */

#ifndef EXPORT_AFSAPPLIB

#define asc_AsidListCreate AfsAppLib_asc_AsidListCreate
#define asc_AsidListCopy AfsAppLib_asc_AsidListCopy
#define asc_AsidListAddEntry AfsAppLib_asc_AsidListAddEntry
#define asc_AsidListRemoveEntry AfsAppLib_asc_AsidListRemoveEntry
#define asc_AsidListRemoveEntryByIndex AfsAppLib_asc_AsidListRemoveEntryByIndex
#define asc_AsidListSetEntryParam AfsAppLib_asc_AsidListSetEntryParam
#define asc_AsidListSetEntryParamByIndex AfsAppLib_asc_AsidListSetEntryParamByIndex
#define asc_AsidListTest AfsAppLib_asc_AsidListTest
#define asc_AsidListFree AfsAppLib_asc_AsidListFree

#define asc_ObjPropListCreate AfsAppLib_asc_ObjPropListCreate
#define asc_ObjPropListCopy AfsAppLib_asc_ObjPropListCopy
#define asc_ObjPropListAddEntry AfsAppLib_asc_ObjPropListAddEntry
#define asc_ObjPropListRemoveEntry AfsAppLib_asc_ObjPropListRemoveEntry
#define asc_ObjPropListTest AfsAppLib_asc_ObjPropListTest
#define asc_ObjPropListFree AfsAppLib_asc_ObjPropListFree

#define asc_ActionListCreate AfsAppLib_asc_ActionListCreate
#define asc_ActionListCopy AfsAppLib_asc_ActionListCopy
#define asc_ActionListAddEntry AfsAppLib_asc_ActionListAddEntry
#define asc_ActionListRemoveEntry AfsAppLib_asc_ActionListRemoveEntry
#define asc_ActionListTest AfsAppLib_asc_ActionListTest
#define asc_ActionListFree AfsAppLib_asc_ActionListFree

#define asc_AdminServerOpen AfsAppLib_asc_AdminServerOpen
#define asc_AdminServerClose AfsAppLib_asc_AdminServerClose

#define asc_CredentialsGet AfsAppLib_asc_CredentialsGet
#define asc_CredentialsSet AfsAppLib_asc_CredentialsSet
#define asc_CredentialsPush AfsAppLib_asc_CredentialsPush

#define asc_LocalCellGet AfsAppLib_asc_LocalCellGet
#define asc_ErrorCodeTranslate AfsAppLib_asc_ErrorCodeTranslate

#define asc_ActionGet AfsAppLib_asc_ActionGet
#define asc_ActionGetMultiple AfsAppLib_asc_ActionGetMultiple
#define asc_ActionListen AfsAppLib_asc_ActionListen
#define asc_ActionListenClear AfsAppLib_asc_ActionListenClear

#define asc_CellOpen AfsAppLib_asc_CellOpen
#define asc_CellClose AfsAppLib_asc_CellClose
#define asc_CellChange AfsAppLib_asc_CellChange
#define asc_CellRefreshRateSet AfsAppLib_asc_CellRefreshRateSet

#define asc_ObjectFind AfsAppLib_asc_ObjectFind
#define asc_ObjectFindMultiple AfsAppLib_asc_ObjectFindMultiple
#define asc_ObjectPropertiesGet AfsAppLib_asc_ObjectPropertiesGet
#define asc_ObjectPropertiesGetMultiple AfsAppLib_asc_ObjectPropertiesGetMultiple

#define asc_ObjectListen AfsAppLib_asc_ObjectListen
#define asc_ObjectListenClear AfsAppLib_asc_ObjectListenClear
#define asc_ObjectListenMultiple AfsAppLib_asc_ObjectListenMultiple

#define asc_ObjectRefresh AfsAppLib_asc_ObjectRefresh
#define asc_ObjectRefreshMultiple AfsAppLib_asc_ObjectRefreshMultiple

#define asc_RandomKeyGet AfsAppLib_asc_RandomKeyGet

#define asc_CellNameGet_Fast AfsAppLib_asc_CellNameGet_Fast
#define asc_ObjectNameGet_Fast AfsAppLib_asc_ObjectNameGet_Fast
#define asc_ObjectTypeGet_Fast AfsAppLib_asc_ObjectTypeGet_Fast
#define asc_ObjectPropertiesGet_Fast AfsAppLib_asc_ObjectPropertiesGet_Fast

#define asc_Enter AfsAppLib_asc_Enter
#define asc_Leave AfsAppLib_asc_Leave
#define asc_GetCriticalSection AfsAppLib_asc_GetCriticalSection

#define asc_UserChange AfsAppLib_asc_UserChange
#define asc_UserPasswordSet AfsAppLib_asc_UserPasswordSet
#define asc_UserUnlock AfsAppLib_asc_UserUnlock
#define asc_UserCreate AfsAppLib_asc_UserCreate
#define asc_UserDelete AfsAppLib_asc_UserDelete

#define asc_GroupChange AfsAppLib_asc_GroupChange
#define asc_GroupMembersGet AfsAppLib_asc_GroupMembersGet
#define asc_GroupMemberAdd AfsAppLib_asc_GroupMemberAdd
#define asc_GroupMemberRemove AfsAppLib_asc_GroupMemberRemove
#define asc_GroupRename AfsAppLib_asc_GroupRename
#define asc_GroupOwnershipGet AfsAppLib_asc_GroupOwnershipGet
#define asc_GroupMembershipGet AfsAppLib_asc_GroupMembershipGet
#define asc_GroupCreate AfsAppLib_asc_GroupCreate
#define asc_GroupDelete AfsAppLib_asc_GroupDelete

#endif // EXPORT_AFSAPPLIB

#endif // AL_ADMSVR_H

