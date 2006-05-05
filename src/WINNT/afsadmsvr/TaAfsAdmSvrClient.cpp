/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsAdmSvrClientInternal.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct
   {
   BOOL fInitializedSockets;
   size_t cReqForAdminServer;
   } l;


/*
 * ROUTINES ___________________________________________________________________
 *
 */

extern "C" void __RPC_FAR * __RPC_USER MIDL_user_allocate (size_t cbAllocate)
{
   return (void __RPC_FAR *)Allocate (cbAllocate);
}

extern "C" void __RPC_USER MIDL_user_free (void __RPC_FAR *pData)
{
   Free (pData);
}


/*
 * DATA STRUCTURES ____________________________________________________________
 *
 */

BOOL ADMINAPI asc_AsidListCreate (LPASIDLIST *ppList)
{
   return ((*ppList = AfsAdmSvr_CreateAsidList()) != NULL);
}

BOOL ADMINAPI asc_AsidListCopy (LPASIDLIST *ppListTarget, LPASIDLIST *ppListSource)
{
   return ((*ppListTarget = AfsAdmSvr_CopyAsidList (*ppListSource)) != NULL);
}

BOOL ADMINAPI asc_AsidListAddEntry (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   return AfsAdmSvr_AddToAsidList (ppList, idObject, lp);
}

BOOL ADMINAPI asc_AsidListRemoveEntry (LPASIDLIST *ppList, ASID idObject)
{
   return AfsAdmSvr_RemoveFromAsidList (ppList, idObject);
}

BOOL ADMINAPI asc_AsidListRemoveEntryByIndex (LPASIDLIST *ppList, size_t iIndex)
{
   return AfsAdmSvr_RemoveFromAsidListByIndex (ppList, iIndex);
}

BOOL ADMINAPI asc_AsidListSetEntryParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   return AfsAdmSvr_SetAsidListParam (ppList, idObject, lp);
}

BOOL ADMINAPI asc_AsidListSetEntryParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp)
{
   return AfsAdmSvr_SetAsidListParamByIndex (ppList, iIndex, lp);
}

BOOL ADMINAPI asc_AsidListTest (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam)
{
   return AfsAdmSvr_IsInAsidList (ppList, idObject, pParam);
}

BOOL ADMINAPI asc_AsidListFree (LPASIDLIST *ppList)
{
   AfsAdmSvr_FreeAsidList (ppList);
   return TRUE;
}


BOOL ADMINAPI asc_ObjPropListCreate (LPASOBJPROPLIST *ppList)
{
   return ((*ppList = AfsAdmSvr_CreateObjPropList()) != NULL);
}

BOOL ADMINAPI asc_ObjPropListCopy (LPASOBJPROPLIST *ppListTarget, LPASOBJPROPLIST *ppListSource)
{
   return ((*ppListTarget = AfsAdmSvr_CopyObjPropList (*ppListSource)) != NULL);
}

BOOL ADMINAPI asc_ObjPropListAddEntry (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp)
{
   return AfsAdmSvr_AddToObjPropList (ppList, pProperties, lp);
}

BOOL ADMINAPI asc_ObjPropListRemoveEntry (LPASOBJPROPLIST *ppList, ASID idObject)
{
   return AfsAdmSvr_RemoveFromObjPropList (ppList, idObject);
}

BOOL ADMINAPI asc_ObjPropListTest (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties, LPARAM *pParam)
{
   return AfsAdmSvr_IsInObjPropList (ppList, idObject, pProperties, pParam);
}

BOOL ADMINAPI asc_ObjPropListFree (LPASOBJPROPLIST *ppList)
{
   AfsAdmSvr_FreeObjPropList (ppList);
   return TRUE;
}


BOOL ADMINAPI asc_ActionListCreate (LPASACTIONLIST *ppList)
{
   return ((*ppList = AfsAdmSvr_CreateActionList()) != NULL);
}

BOOL ADMINAPI asc_ActionListCopy (LPASACTIONLIST *ppListTarget, LPASACTIONLIST *ppListSource)
{
   return ((*ppListTarget = AfsAdmSvr_CopyActionList (*ppListSource)) != NULL);
}

BOOL ADMINAPI asc_ActionListAddEntry (LPASACTIONLIST *ppList, LPASACTION pAction)
{
   return AfsAdmSvr_AddToActionList (ppList, pAction);
}

BOOL ADMINAPI asc_ActionListRemoveEntry (LPASACTIONLIST *ppList, DWORD idAction)
{
   return AfsAdmSvr_RemoveFromActionList (ppList, idAction);
}

BOOL ADMINAPI asc_ActionListTest (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction)
{
   return AfsAdmSvr_IsInActionList (ppList, idAction, pAction);
}

BOOL ADMINAPI asc_ActionListFree (LPASACTIONLIST *ppList)
{
   AfsAdmSvr_FreeActionList (ppList);
   return TRUE;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL ADMINAPI asc_AdminServerOpen (LPCTSTR pszAddress, UINT_PTR *pidClient, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (!l.fInitializedSockets)
      {
      WSADATA Data;
      WSAStartup (0x0101, &Data);
      l.fInitializedSockets = TRUE;
      }

   if ((++l.cReqForAdminServer) == 1)
      {
      LPCTSTR pszResolvedAddress = ResolveAddress (pszAddress);
      if (!BindToAdminServer (pszResolvedAddress, FALSE, pidClient, &status))
         {
         if (status != RPC_S_CALL_FAILED_DNE) // server rejected us?
            rc = FALSE;
         else if (pszResolvedAddress || !ForkNewAdminServer (&status))
            rc = FALSE;
         else
            rc = BindToAdminServer (pszResolvedAddress, TRUE, pidClient, &status);
         }
      }

   if (rc)
      StartPingThread (*pidClient);
   if (rc)
      StartCallbackThread();
   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_AdminServerClose (UINT_PTR idClient, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   StopCallbackThread();
   StopPingThread (idClient);

   if (l.cReqForAdminServer && ((--l.cReqForAdminServer) == 0))
      {
      UnbindFromAdminServer (idClient, &status);
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}



BOOL ADMINAPI asc_CredentialsCrack (UINT_PTR idClient, PVOID hCreds, LPTSTR pszCell, LPTSTR pszUser, SYSTEMTIME *pstExpiration, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szCell = TEXT("");
      STRING szUser = TEXT("");

      if ((rc = AfsAdmSvr_CrackCredentials (idClient, (UINT_PTR)hCreds, szCell, szUser, pstExpiration, &status)) != FALSE)
         {
         lstrcpy (pszCell, szCell);
         lstrcpy (pszUser, szUser);
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


UINT_PTR ADMINAPI asc_CredentialsGet (UINT_PTR idClient, LPCTSTR pszCell, ULONG *pStatus)
{
   UINT_PTR rc = 0;
   ULONG status = 0;

   RpcTryExcept
      {
      if (pszCell)
         {
         STRING szCell;
         lstrcpy (szCell, pszCell);

         rc = AfsAdmSvr_GetCredentials (idClient, szCell, &status);
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


UINT_PTR ADMINAPI asc_CredentialsSet (UINT_PTR idClient, LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus)
{
   UINT_PTR rc = 0;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szCell;
      lstrcpy (szCell, pszCell);

      STRING szUser;
      lstrcpy (szUser, pszUser);

      STRING szPassword;
      lstrcpy (szPassword, pszPassword);

      // TODO: Ensure we do some encryption here, or using an
      // encrypted socket, or something... can't just be pushing
      // the user's unencrypted password across the wire.

      rc = AfsAdmSvr_SetCredentials (idClient, szCell, szUser, szPassword, &status);
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


BOOL ADMINAPI asc_CredentialsPush (UINT_PTR idClient, PVOID hCreds, ASID idCell, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = (AfsAdmSvr_PushCredentials (idClient, PtrToUlong(hCreds), idCell, &status)?TRUE:FALSE);
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


BOOL ADMINAPI asc_LocalCellGet (UINT_PTR idClient, LPTSTR pszCell, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szCell;
      if ((rc = AfsAdmSvr_GetLocalCell (idClient, szCell, &status)) != FALSE)
         {
         lstrcpy (pszCell, szCell);
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


BOOL ADMINAPI asc_ErrorCodeTranslate (UINT_PTR idClient, ULONG code, LANGID idLanguage, STRING pszErrorText, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szText;
      if ((rc = AfsAdmSvr_ErrorCodeTranslate (idClient, code, idLanguage, szText, &status)) != FALSE)
         {
         lstrcpy (pszErrorText, szText);
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


BOOL ADMINAPI asc_CellOpen (UINT_PTR idClient, PVOID hCreds, LPCTSTR pszCell, DWORD dwScope, ASID *pidCell, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szCell;
      lstrcpy (szCell, pszCell);

      if ((rc = (AfsAdmSvr_OpenCell (idClient, PtrToUlong(hCreds), szCell, dwScope, pidCell, &status)?TRUE:FALSE)) != FALSE)
         {
         if (!CreateCellCache (*pidCell))
            {
            (void)AfsAdmSvr_CloseCell (idClient, *pidCell, &status);
            rc = FALSE;
            status = ERROR_NOT_ENOUGH_MEMORY;
            }
         else // get rudimentary properties about the cell
            {
            rc = RefreshCachedProperties (idClient, *pidCell, *pidCell, GET_RUDIMENTARY_DATA, &status);
            }
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


BOOL ADMINAPI asc_CellClose (UINT_PTR idClient, ASID idCell, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      rc = AfsAdmSvr_CloseCell (idClient, idCell, &status);
      DestroyCellCache (idCell);
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


BOOL ADMINAPI asc_ObjectFind (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszName, ASID *pidObject, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      STRING szName = TEXT("");
      if (pszName)
         lstrcpy (szName, pszName);
      rc = AfsAdmSvr_FindObject (idClient, idSearchScope, ObjectType, SEARCH_ALL_OBJECTS, szName, pidObject, &status);
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


BOOL ADMINAPI asc_ObjectFindMultiple (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, LPCTSTR pszPattern, LPAFSADMSVR_SEARCH_PARAMS pSearchParams, LPASIDLIST *ppList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      *ppList = NULL;

      STRING szPattern = TEXT("");
      if (pszPattern)
         lstrcpy (szPattern, pszPattern);

      AFSADMSVR_SEARCH_PARAMS SearchParams;
      if (pSearchParams)
         memcpy (&SearchParams, pSearchParams, sizeof(AFSADMSVR_SEARCH_PARAMS));
      else
         {
         memset (&SearchParams, 0x00, sizeof(AFSADMSVR_SEARCH_PARAMS));
         SearchParams.SearchType = SEARCH_NO_LIMITATIONS;
         }

      rc = AfsAdmSvr_FindObjects (idClient, idSearchScope, ObjectType, SEARCH_ALL_OBJECTS, szPattern, &SearchParams, ppList, &status);
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


BOOL ADMINAPI asc_ObjectPropertiesGet (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (!RefreshCachedProperties (idClient, idCell, idObject, GetLevel, &status))
      {
      rc = FALSE;
      }
   else
      {
      LPASOBJPROP pFound;
      if ((pFound = GetCachedProperties (idCell, idObject)) == NULL)
         {
         status = ERROR_NOT_ENOUGH_MEMORY;
         rc = FALSE;
         }
      else
         {
         memcpy (pProperties, pFound, sizeof(ASOBJPROP));
         }
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ObjectPropertiesGetMultiple (UINT_PTR idClient, AFSADMSVR_GET_LEVEL GetLevel, ASID idCell, LPASIDLIST pAsidList, LPASOBJPROPLIST *ppPropertiesList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (!RefreshCachedProperties (idClient, idCell, pAsidList, GetLevel, &status))
      {
      rc = FALSE;
      }
   else
      {
      *ppPropertiesList = NULL;
      for (size_t iAsidList = 0; iAsidList < pAsidList->cEntries; ++iAsidList)
         {
         LPASOBJPROP pFound;
         if ((pFound = GetCachedProperties (idCell, pAsidList->aEntries[ iAsidList ].idObject)) != NULL)
            {
            if (!*ppPropertiesList)
               asc_ObjPropListCreate(ppPropertiesList);
            if (*ppPropertiesList)
               AfsAdmSvr_AddToObjPropList (ppPropertiesList, pFound, pAsidList->aEntries[ iAsidList ].lParam);
            }
         }
      }

   if (!rc && *ppPropertiesList)
      AfsAdmSvr_FreeObjPropList (ppPropertiesList);
   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ObjectListen (UINT_PTR idClient, ASID idCell, ASID idObject, HWND hNotify, ULONG *pStatus)
{
   if (!idObject)
      {
      if (*pStatus)
         *pStatus = ERROR_INVALID_PARAMETER;
      return FALSE;
      }

   if (!AddObjectNotification (hNotify, idCell, idObject))
      {
      if (*pStatus)
         *pStatus = ERROR_NOT_ENOUGH_MEMORY;
      return FALSE;
      }

   TestForNotifications (idClient, idCell, idObject);
   return TRUE;
}


BOOL ADMINAPI asc_ObjectListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   ClearObjectNotifications (hNotify);
   return TRUE;
}


BOOL ADMINAPI asc_ObjectListenMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, HWND hNotify, ULONG *pStatus)
{
   if (!pAsidList)
      {
      if (*pStatus)
         *pStatus = ERROR_INVALID_PARAMETER;
      return FALSE;
      }

   for (size_t ii = 0; ii < pAsidList->cEntriesAllocated; ++ii)
      {
      if (!pAsidList->aEntries[ ii ].idObject)
         continue;

      if (!AddObjectNotification (hNotify, idCell, pAsidList->aEntries[ ii ].idObject))
         {
         if (*pStatus)
            *pStatus = ERROR_NOT_ENOUGH_MEMORY;
         return FALSE;
         }

      TestForNotifications (idClient, idCell, pAsidList->aEntries[ ii ].idObject);
      }

   return TRUE;
}


BOOL ADMINAPI asc_ObjectRefresh (UINT_PTR idClient, ASID idCell, ASID idObject, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   // First have the server invalidate its cache of information; regardless
   // of the name, this is actually just an Invalidate call, not a Refresh call
   //
   RpcTryExcept
      {
      rc = AfsAdmSvr_RefreshObject (idClient, idObject, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   // If that suceeded, see if there is anyone listening for changes
   // in this object or any of its children. If so, this call
   // will requery the server for the latest properties for all
   // listened-for objects, which will make us post notifications if
   // we get new data back.
   //
   if (rc)
      {
      TestForNotifications (idClient, idCell);
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ObjectRefreshMultiple (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   // First have the server invalidate its cache of information; regardless
   // of the name, this is actually just an Invalidate call, not a Refresh call
   //
   RpcTryExcept
      {
      rc = AfsAdmSvr_RefreshObjects (idClient, pAsidList, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   // If that suceeded, see if there is anyone listening for changes
   // in any of these objects or their children. If so, this call
   // will requery the server for the latest properties for all
   // listened-for objects, which will make us post notifications if
   // we get new data back.
   //
   if (rc)
      {
      TestForNotifications (idClient, idCell);
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_RandomKeyGet (UINT_PTR idClient, ASID idCell, PBYTE key, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      memset (key, 0x00, sizeof(BYTE) * ENCRYPTIONKEYLENGTH);
      rc = AfsAdmSvr_GetRandomKey (idClient, idCell, key, &status);
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


BOOL ADMINAPI asc_CellNameGet_Fast (UINT_PTR idClient, ASID idCell, LPTSTR pszCell, ULONG *pStatus)
{
   return asc_ObjectNameGet_Fast (idClient, idCell, idCell, pszCell, pStatus);
}


BOOL ADMINAPI asc_ObjectNameGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPTSTR pszObject, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPASOBJPROP pProperties;
   if ((pProperties = GetCachedProperties (idCell, idObject)) == NULL)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else if (pProperties->verProperties == verPROP_NO_OBJECT)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else
      {
      lstrcpy (pszObject, pProperties->szName);
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ObjectTypeGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, ASOBJTYPE *pObjectType, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPASOBJPROP pProperties;
   if ((pProperties = GetCachedProperties (idCell, idObject)) == NULL)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else if (pProperties->verProperties == verPROP_NO_OBJECT)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else
      {
      *pObjectType = pProperties->Type;
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ObjectPropertiesGet_Fast (UINT_PTR idClient, ASID idCell, ASID idObject, LPASOBJPROP pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPASOBJPROP pPropFound;
   if ((pPropFound = GetCachedProperties (idCell, idObject)) == NULL)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else if (pPropFound->verProperties == verPROP_NO_OBJECT)
      {
      rc = FALSE;
      status = ERROR_NO_DATA;
      }
   else if (pProperties)
      {
      memcpy (pProperties, pPropFound, sizeof(ASOBJPROP));
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_ActionGet (UINT_PTR idClient, DWORD idAction, LPASACTION pAction, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      memset (pAction, 0x00, sizeof(ASACTION));
      rc = AfsAdmSvr_GetAction (idClient, idAction, pAction, &status);
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


BOOL ADMINAPI asc_ActionGetMultiple (UINT_PTR idClient, UINT_PTR idClientSearch, ASID idCellSearch, LPASACTIONLIST *ppList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      *ppList = NULL;
      rc = AfsAdmSvr_GetActions (idClient, idClientSearch, idCellSearch, ppList, &status);
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


BOOL ADMINAPI asc_ActionListen (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   if (!SetActionNotification (hNotify, TRUE))
      {
      if (*pStatus)
         *pStatus = ERROR_NOT_ENOUGH_MEMORY;
      return FALSE;
      }
   return TRUE;
}


BOOL ADMINAPI asc_ActionListenClear (UINT_PTR idClient, HWND hNotify, ULONG *pStatus)
{
   if (!SetActionNotification (hNotify, FALSE))
      {
      if (*pStatus)
         *pStatus = ERROR_NOT_ENOUGH_MEMORY;
      return FALSE;
      }
   return TRUE;
}


      // AfsAdmSvrCallback_Action
      // ...called by the server in the context of the CallbackHost() routine;
      //    this routine is used to notify the client whenever an action is
      //    initiated or completed.
      //
extern "C" void AfsAdmSvrCallback_Action (LPASACTION pAction, BOOL fFinished)
{
   NotifyActionListeners (pAction, fFinished);
}

