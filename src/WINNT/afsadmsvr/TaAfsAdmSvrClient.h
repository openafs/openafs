/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCLIENT_H
#define TAAFSADMSVRCLIENT_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#include <WINNT/TaAfsAdmSvr.h>

#ifndef ADMINAPI
#ifdef WIN32
#define ADMINAPI __cdecl
#else
#define ADMINAPI
#endif
#endif

         // You can use asc_ObjectListen() to specify that you want a
         // particular window to receive a message whenever there is a
         // change to a given object's properties. Changes will automatically
         // be detected when the server performs its periodic refresh,
         // or will be detected whenever an explicit asc_ObjectRefresh()
         // call is made.
         //
#define WM_ASC_NOTIFY_OBJECT   (WM_USER + 0x300)    // lp=object ASID

         // You can use asc_ActionListen() to specify that you want a
         // particular window to receive a message whenever an action
         // is initiated or completed on the server.
         //
#define WM_ASC_NOTIFY_ACTION   (WM_USER + 0x301)    // wp=fEnd, lp=action ASID


/*
 * DATA STRUCTURES ____________________________________________________________
 *
 */

BOOL ADMINAPI asc_AsidListCreate (LPASIDLIST *ppList);
BOOL ADMINAPI asc_AsidListCopy (LPASIDLIST *ppListTarget, LPASIDLIST *ppListSource);
BOOL ADMINAPI asc_AsidListAddEntry (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
BOOL ADMINAPI asc_AsidListRemoveEntry (LPASIDLIST *ppList, ASID idObject);
BOOL ADMINAPI asc_AsidListRemoveEntryByIndex (LPASIDLIST *ppList, size_t iIndex);
BOOL ADMINAPI asc_AsidListSetEntryParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
BOOL ADMINAPI asc_AsidListSetEntryParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp);
BOOL ADMINAPI asc_AsidListTest (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam = NULL);
BOOL ADMINAPI asc_AsidListFree (LPASIDLIST *ppList);

BOOL ADMINAPI asc_ObjPropListCreate (LPASOBJPROPLIST *ppList);
BOOL ADMINAPI asc_ObjPropListCopy (LPASOBJPROPLIST *ppListTarget, LPASOBJPROPLIST *ppListSource);
BOOL ADMINAPI asc_ObjPropListAddEntry (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp);
BOOL ADMINAPI asc_ObjPropListRemoveEntry (LPASOBJPROPLIST *ppList, ASID idObject);
BOOL ADMINAPI asc_ObjPropListTest (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties = NULL, LPARAM *pParam = NULL);
BOOL ADMINAPI asc_ObjPropListFree (LPASOBJPROPLIST *ppList);

BOOL ADMINAPI asc_ActionListCreate (LPASACTIONLIST *ppList);
BOOL ADMINAPI asc_ActionListCopy (LPASACTIONLIST *ppListTarget, LPASACTIONLIST *ppListSource);
BOOL ADMINAPI asc_ActionListAddEntry (LPASACTIONLIST *ppList, LPASACTION pAction);
BOOL ADMINAPI asc_ActionListRemoveEntry (LPASACTIONLIST *ppList, DWORD idAction);
BOOL ADMINAPI asc_ActionListTest (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction = NULL);
BOOL ADMINAPI asc_ActionListFree (LPASACTIONLIST *ppList);


/*
 * GENERAL PROTOTYPES _________________________________________________________
 *
 */

BOOL ADMINAPI asc_AdminServerOpen (LPCTSTR pszAddress, UINT_PTR *pidClient, ULONG *pStatus);
BOOL ADMINAPI asc_AdminServerClose (UINT_PTR idClient, ULONG *pStatus);

BOOL ADMINAPI asc_CredentialsCrack (UINT_PTR idClient, PVOID hCreds, LPTSTR pszCell, LPTSTR pszUser, SYSTEMTIME *pstExpiration, ULONG *pStatus);
UINT_PTR ADMINAPI asc_CredentialsGet (UINT_PTR idClient, LPCTSTR pszCell, ULONG *pStatus);
UINT_PTR ADMINAPI asc_CredentialsSet (UINT_PTR idClient, LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus);
BOOL ADMINAPI asc_CredentialsPush (UINT_PTR idClient, PVOID hCreds, ASID idCel, ULONG *pStatus);

BOOL ADMINAPI asc_LocalCellGet (UINT_PTR idClient, LPTSTR pszCell, ULONG *pStatus);
BOOL ADMINAPI asc_ErrorCodeTranslate (UINT_PTR idClient, ULONG code, LANGID idLanguage, STRING pszErrorText, ULONG *pStatus);

BOOL ADMINAPI asc_CellOpen (UINT_PTR idClient, PVOID hCreds, LPCTSTR pszCell, DWORD dwScope, ASID *pidCell, ULONG *pStatus);
BOOL ADMINAPI asc_CellClose (UINT_PTR idClient, ASID idCell, ULONG *pStatus);
BOOL ADMINAPI asc_CellChange (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CHANGECELL_PARAMS pChange, ULONG *pStatus);
BOOL ADMINAPI asc_CellRefreshRateSet (UINT_PTR idClient, ASID idCell, ULONG cminRefreshRate, ULONG *pStatus);

BOOL ADMINAPI asc_ObjectFind (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszName, ASID *pidObject, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectFindMultiple (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszPattern, LPAFSADMSVR_SEARCH_PARAMS pSearchParams, LPASIDLIST *ppList, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectPropertiesGet (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectPropertiesGetMultiple (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, LPASIDLIST pAsidList, LPASOBJPROPLIST *ppPropertiesList, ULONG *pStatus);

BOOL ADMINAPI asc_ObjectRefresh (UINT_PTR idClient, ASID idCell, ASID idObject, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectRefreshMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, ULONG *pStatus);

BOOL ADMINAPI asc_RandomKeyGet (UINT_PTR idClient, ASID idCell, PBYTE key, ULONG *pStatus);

BOOL ADMINAPI asc_CellNameGet_Fast (UINT_PTR idClient, ASID idCell, LPTSTR pszCell, ULONG *pStatus = NULL);
BOOL ADMINAPI asc_ObjectNameGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPTSTR pszObjectName, ULONG *pStatus = NULL);
BOOL ADMINAPI asc_ObjectTypeGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, ASOBJTYPE *pObjectType, ULONG *pStatus = NULL);
BOOL ADMINAPI asc_ObjectPropertiesGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus = NULL);

void ADMINAPI asc_Enter (void);
void ADMINAPI asc_Leave (void);
LPCRITICAL_SECTION ADMINAPI asc_GetCriticalSection (void);


/*
 * NOTIFICATIONS ______________________________________________________________
 *
 */

BOOL ADMINAPI asc_ObjectListen (UINT_PTR idClient, ASID idCell, ASID idObject, HWND hNotify, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);
BOOL ADMINAPI asc_ObjectListenMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, HWND hNotify, ULONG *pStatus);

BOOL ADMINAPI asc_ActionGet (UINT_PTR idClient, DWORD idAction, LPASACTION pAction, ULONG *pStatus);
BOOL ADMINAPI asc_ActionGetMultiple (UINT_PTR idClient, UINT_PTR idClientSearch, ASID idCellSearch, LPASACTIONLIST *ppList, ULONG *pStatus);
BOOL ADMINAPI asc_ActionListen (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);
BOOL ADMINAPI asc_ActionListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus);


/*
 * USER-ACCOUNT PROTOTYPES ____________________________________________________
 *
 */

BOOL ADMINAPI asc_UserChange (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_CHANGEUSER_PARAMS pChange, ULONG *pStatus);
BOOL ADMINAPI asc_UserPasswordSet (UINT_PTR idClient, ASID idCell, ASID idUser, int keyVersion, LPCTSTR pkeyString, PBYTE pkeyData, ULONG *pStatus);
BOOL ADMINAPI asc_UserUnlock (UINT_PTR idClient, ASID idCell, ASID idUser, ULONG *pStatus);
BOOL ADMINAPI asc_UserCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEUSER_PARAMS pCreate, ASID *pidUser, ULONG *pStatus);
BOOL ADMINAPI asc_UserDelete (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_DELETEUSER_PARAMS pDelete, ULONG *pStatus);


/*
 * GROUP-ACCOUNT PROTOTYPES ___________________________________________________
 *
 */

BOOL ADMINAPI asc_GroupChange (UINT_PTR idClient, ASID idCell, ASID idGroup, LPAFSADMSVR_CHANGEGROUP_PARAMS pChange, ULONG *pStatus);
BOOL ADMINAPI asc_GroupMembersGet (UINT_PTR idClient, ASID idCell, ASID idGroup, LPASIDLIST *ppAsidList, ULONG *pStatus);
BOOL ADMINAPI asc_GroupMemberAdd (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus);
BOOL ADMINAPI asc_GroupMemberRemove (UINT_PTR idClient, ASID idCell, ASID idGroup, ASID idMember, ULONG *pStatus);
BOOL ADMINAPI asc_GroupRename (UINT_PTR idClient, ASID idCell, ASID idGroup, LPCTSTR pszNewName, ULONG *pStatus);
BOOL ADMINAPI asc_GroupMembershipGet (UINT_PTR idClient, ASID idCell, ASID idMember, LPASIDLIST *ppAsidList, ULONG *pStatus);
BOOL ADMINAPI asc_GroupOwnershipGet (UINT_PTR idClient, ASID idCell, ASID idOwner, LPASIDLIST *ppAsidList, ULONG *pStatus);
BOOL ADMINAPI asc_GroupCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEGROUP_PARAMS pCreate, ASID *pidGroup, ULONG *pStatus);
BOOL ADMINAPI asc_GroupDelete (UINT_PTR idClient, ASID idCell, ASID idGroup, ULONG *pStatus);


#endif // TAAFSADMSVRCLIENT_H

