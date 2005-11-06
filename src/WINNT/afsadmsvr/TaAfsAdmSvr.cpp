/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include "TaAfsAdmSvrInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

      // AfsAdmSvr_Connect
      // ...obtains a cookie to represent the calling process. The cookie should
      //    be freed with AfsAdmSvr_Disconnect when the process disconnects.
      //
extern "C" int AfsAdmSvr_Connect (STRING szClientAddress, UINT_PTR *pidClient, ULONG *pStatus)
{
   // Make sure AfsClass initialized properly. If it's already init'd,
   // this won't hurt at all.
   //
   ULONG status;
   if (!AfsClass_Initialize (&status))
      {
      Print (TEXT("Denying client %s due to AfsClass initialization failure"), szClientAddress);
      return FALSE_(status, pStatus);
      }

   // Find a free CLIENTINFO structure for this caller
   //
   if (!AfsAdmSvr_AttachClient (szClientAddress, (PVOID *)pidClient, pStatus))
      return FALSE;

   Print (TEXT("Connected to client %s (ID 0x%08lX)"), AfsAdmSvr_GetClientName (*pidClient), *pidClient);

   return TRUE;
}


      // AfsAdmSvr_Ping
      // ...reminds the admin server that the specified client is still around.
      //    this call should be made at least every csecAFSADMSVR_CLIENT_PING
      //    seconds, lest the admin server think you've disconnected. (The
      //    client library TaAfsAdmSvrClient.lib automatically handles this.)
      //
extern "C" int AfsAdmSvr_Ping (UINT_PTR idClient, ULONG *pStatus)
{
   AfsAdmSvr_Enter();

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return Leave_FALSE_(ERROR_INVALID_PARAMETER, pStatus);

   AfsAdmSvr_PingClient (idClient);
   AfsAdmSvr_Leave();
   return TRUE;
}


      // AfsAdmSvr_Disconnect
      // ...releases and invalidates the cookie representing the calling process.
      //
extern "C" int AfsAdmSvr_Disconnect (UINT_PTR idClient, ULONG *pStatus)
{
   AfsAdmSvr_Enter();

   // Make sure this is a valid client, and free its l.aClients[] entry if so.
   //
   if (!AfsAdmSvr_fIsValidClient (idClient))
      return Leave_FALSE_(ERROR_INVALID_PARAMETER, pStatus);

   Print (TEXT("Disconnected from client %s (ID 0x%08lX)"), AfsAdmSvr_GetClientName (idClient), idClient);

   AfsAdmSvr_DetachClient (idClient);
   AfsAdmSvr_Leave();
   return TRUE;
}


      // AfsAdmSvr_CrackCredentials
      // ...queries the specified AFS credentials token for its cell, user
      //    and expiration date.
      //
extern "C" int AfsAdmSvr_CrackCredentials (UINT_PTR idClient, UINT_PTR hCreds, STRING pszCell, STRING pszUser, SYSTEMTIME *pstExpiration, ULONG *pStatus)
{
   ULONG status;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CrackCredentials (0x%08lX)"), idClient, hCreds);

   unsigned long dateExpirationQuery;
   int fHasKasTokenQuery;
   char szUser[ cchSTRING ];
   char szUser2[ cchSTRING ];
   char szCell[ cchSTRING ];
   char *pszCellQuery = (pszCell) ? (char *)pszCell : szCell;
   char *pszUserQuery = (pszUser) ? (char *)pszUser : szUser;
   if (!afsclient_TokenQuery ((PVOID)hCreds, &dateExpirationQuery, pszUserQuery, szUser2, pszCellQuery, &fHasKasTokenQuery, (afs_status_p)&status))
      return FALSE_(status, pStatus, iOp);

   if (pstExpiration)
      AfsAppLib_UnixTimeToSystemTime (pstExpiration, dateExpirationQuery);

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_GetCredentials
      // ...queries the user's current AFS credentials for the specified cell
      //    if the user already has credentials in the cell, returns a nonzero
      //    token {hCreds}, suitable for use in AfsAdmSvr_OpenCell().
      //
extern "C" UINT_PTR AfsAdmSvr_GetCredentials (UINT_PTR idClient, STRING pszCell, ULONG *pStatus)
{
   ULONG status;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetCredentials (%s)"), idClient, pszCell);

   const char *pszCellTest = (pszCell && *pszCell) ? (const char *)pszCell : NULL;

   UINT_PTR hCreds = NULL;
   if (!afsclient_TokenGetExisting (pszCellTest, (PVOID *)&hCreds, (afs_status_p)&status))
      return FALSE_(status, pStatus, iOp);

   AfsAdmSvr_EndOperation (iOp);
   return hCreds;
}


      // AfsAdmSvr_SetCredentials
      // ...obtains new AFS credentials within the administrative server process
      //    on behalf of the specified user. if successful, returns a nonzero
      //    token {hCreds}, suitable for use in AfsAdmSvr_OpenCell().
      //
extern "C" UINT_PTR AfsAdmSvr_SetCredentials (UINT_PTR idClient, STRING pszCell, STRING pszUser, STRING pszPassword, ULONG *pStatus)
{
   ULONG status;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: SetCredentials (%s,%s)"), idClient, pszCell, pszUser);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   const char *pszCellSet = (pszCell && *pszCell) ? (const char *)pszCell : NULL;

   UINT_PTR hCreds;
   if (!afsclient_TokenGetNew (pszCellSet, (const char *)pszUser, (const char *)pszPassword, (PVOID *)&hCreds, (afs_status_p)&status))
      return FALSE_(status,pStatus,iOp);

   AfsAdmSvr_EndOperation (iOp);
   return hCreds;
}


      // AfsAdmSvr_PushCredentials
      // ...requests that the specified AFS credentials be used hereafter
      //    when manipulating the specified cell. You should follow this
      //    call with a Refresh request if necessary.
      //
extern "C" int AfsAdmSvr_PushCredentials (UINT_PTR idClient, UINT_PTR hCreds, ASID idCell, ULONG *pStatus)
{
   ULONG status;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: PushCredentials (hCreds=0x%08lX, idCell=0x%08lX)"), idClient, hCreds, idCell);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (GetAsidType (idCell) != itCELL)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   lpCell->SetCurrentCredentials ((PVOID)hCreds);
   lpCell->Close();

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_GetLocalCell
      // ...obtains the name of the primary cell used by the admin server
      //
extern "C" int AfsAdmSvr_GetLocalCell (UINT_PTR idClient, STRING pszCellName, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetLocalCell"), idClient);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (!CELL::GetDefaultCell (pszCellName, pStatus))
      {
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_ErrorCodeTranslate
      // ...translates an error code into an English string
      //
extern "C" int AfsAdmSvr_ErrorCodeTranslate (UINT_PTR idClient, ULONG code, LANGID idLanguage, STRING pszErrorText, ULONG *pStatus)
{
   if (!AfsAppLib_TranslateError (pszErrorText, code, idLanguage))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   LPTSTR pch;
   if ((pch = (LPTSTR)lstrrchr (pszErrorText, TEXT('('))) != NULL)
      *pch = TEXT('\0');
   while (lstrlen(pszErrorText) && pszErrorText[ lstrlen(pszErrorText)-1 ] == TEXT(' '))
      pszErrorText[ lstrlen(pszErrorText)-1 ] = TEXT('\0');
   return TRUE;
}


      // AfsAdmSvr_GetAction
      // ...returns information about a particular operation in progress.
      //
extern "C" int AfsAdmSvr_GetAction (UINT_PTR idClient, DWORD idAction, LPASACTION pAction, ULONG *pStatus)
{
   Print (dlDETAIL, TEXT("Client 0x%08lX: GetAction (idAction=0x%08lX)"), idClient, idAction);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   if (!AfsAdmSvr_GetOperation (idAction, pAction))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   Print (dlERROR, TEXT("Client 0x%08lX: GetAction succeeded"));
   return TRUE;
}


      // AfsAdmSvr_GetActions
      // ...returns a list of operations in progress. The list returned can
      //    be constrained to only including those operations initiated by
      //    a particular client and/or performed in a particular cell.
      //
extern "C" int AfsAdmSvr_GetActions (UINT_PTR idClient, UINT_PTR idClientSearch, ASID idCellSearch, LPASACTIONLIST *ppList, ULONG *pStatus)
{
   Print (dlDETAIL, TEXT("Client 0x%08lX: GetActions (idClientSearch=0x%08lX, idCellSearch=0x%08lX)"), idClient, idClientSearch, idCellSearch);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   if ((*ppList = AfsAdmSvr_GetOperations (idClientSearch, idCellSearch)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: GetActions failed"), idClient);
      return FALSE_(ERROR_NOT_ENOUGH_MEMORY,pStatus);
      }

   Print (dlERROR, TEXT("Client 0x%08lX: GetActions succeeded; %ld actions"), idClient, (*ppList) ? (*ppList)->cEntries : 0);
   return TRUE;
}


      // AfsAdmSvr_OpenCell
      // ...opens a cell for administration.
      //
extern "C" int AfsAdmSvr_OpenCell (UINT_PTR idClient, UINT_PTR hCreds, STRING pszCellName, DWORD dwScopeFlags, ASID *pidCell, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: OpenCell"), idClient);

   AfsAdmSvr_AddToMinScope (dwScopeFlags);

   LPIDENT lpiCell;
   if ((lpiCell = CELL::OpenCell ((LPTSTR)pszCellName, (PVOID)hCreds, pStatus)) == NULL)
      {
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: OpenCell succeeded (idCell=0x%08lX)"), idClient, lpiCell);

   *pidCell = (ASID)lpiCell;
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_CloseCell
      // ...used by client to open a cell for administration.
      //
extern "C" int AfsAdmSvr_CloseCell (UINT_PTR idClient, ASID idCell, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CloseCell (idCell=0x%08lX)"), idClient, idCell);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (GetAsidType (idCell) != itCELL)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   CELL::CloseCell ((LPIDENT)idCell);

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_FindObject
      // AfsAdmSvr_FindObjects
      // ...used to search through all objects in the cell, obtaining a list
      //    of those which match the specified criteria. For FindObjects, the
      //    {*ppList} parameter will be filled in with an allocated list of
      //    ASIDs, and should be freed using the AfsAdmSvr_FreeAsidList()
      //    routine (clients using the TaAfsAdmSvrClient.lib library should
      //    call asc_AsidListFree(), which is a wrapper for that routine).
      //    The _FindObject routine can be used to find exactly one object--
      //    for instance, finding the ASID for a particular user or volume--
      //    while the _FindObjects routine returns a list of all objects
      //    which match the specified criteria--all volumes on a partition,
      //    or all users named "b*" within a cell.
      //
extern "C" int AfsAdmSvr_FindObject (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, AFSADMSVR_SEARCH_REFRESH SearchRefresh, STRING szName, ASID *pidObject, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: FindObject (scope=0x%08lX, type=%lu, name='%s')"), idClient, idSearchScope, ObjectType, szName);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (GetAsidType (idSearchScope) == itUNUSED)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // We've got a special case here: if possible, we don't want to have to
   // refresh the contents of the entire cell. So if the client is looking
   // for a user or group, we can just try to grab that object by its name;
   // afsclass supports an interface for just this case.
   //
   switch (ObjectType)
      {
      case TYPE_USER:
         rc = AfsAdmSvr_Search_OneUser (pidObject, idSearchScope, szName, &status);
         break;

      case TYPE_GROUP:
         rc = AfsAdmSvr_Search_OneGroup (pidObject, idSearchScope, szName, &status);
         break;

      default:
         // We'll have to do the search the hard way. First
         // see if we need to refresh this cell/server.
         //
         if (!AfsAdmSvr_SearchRefresh (idSearchScope, ObjectType, SearchRefresh, &status))
            return FALSE_(status,pStatus,iOp);

         // Look for the specified object.
         //
         switch (GetAsidType (idSearchScope))
            {
            case itCELL:
               if (ObjectType == TYPE_SERVER)
                  rc = AfsAdmSvr_Search_ServerInCell (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_SERVICE)
                  rc = AfsAdmSvr_Search_ServiceInCell (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_PARTITION)
                  rc = AfsAdmSvr_Search_PartitionInCell (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_VOLUME)
                  rc = AfsAdmSvr_Search_VolumeInCell (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_USER)
                  rc = AfsAdmSvr_Search_UserInCell (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_GROUP)
                  rc = AfsAdmSvr_Search_GroupInCell (pidObject, idSearchScope, szName, &status);
               else
                  {
                  rc = FALSE;
                  status = ERROR_INVALID_PARAMETER;
                  }
               break;

            case itSERVER:
               if (ObjectType == TYPE_SERVICE)
                  rc = AfsAdmSvr_Search_ServiceInServer (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_PARTITION)
                  rc = AfsAdmSvr_Search_PartitionInServer (pidObject, idSearchScope, szName, &status);
               else if (ObjectType == TYPE_VOLUME)
                  rc = AfsAdmSvr_Search_VolumeInServer (pidObject, idSearchScope, szName, &status);
               else
                  {
                  rc = FALSE;
                  status = ERROR_INVALID_PARAMETER;
                  }
               break;

            case itAGGREGATE:
               if (ObjectType == TYPE_VOLUME)
                  rc = AfsAdmSvr_Search_VolumeInPartition (pidObject, idSearchScope, szName, &status);
               else
                  {
                  rc = FALSE;
                  status = ERROR_INVALID_PARAMETER;
                  }
               break;
            }
         break;
      }

   if (!rc && pStatus)
      *pStatus = status;

   if (!rc)
      Print (dlERROR, TEXT("Client 0x%08lX: FindObject failed (status=0x%08lX)"), idClient, status);
   else // (rc)
      Print (dlDETAIL, TEXT("Client 0x%08lX: FindObject succeeded; returning idObject=0x%08lX"), idClient, *pidObject);

   AfsAdmSvr_EndOperation (iOp);
   return rc;
}


extern "C" int AfsAdmSvr_FindObjects (UINT_PTR idClient, ASID idSearchScope, ASOBJTYPE ObjectType, AFSADMSVR_SEARCH_REFRESH SearchRefresh, STRING szPattern, LPAFSADMSVR_SEARCH_PARAMS pSearchParams, LPASIDLIST *ppList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: FindObjects (scope=0x%08lX, type=%lu, pat='%s')"), idClient, idSearchScope, ObjectType, szPattern);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (GetAsidType (idSearchScope) == itUNUSED)
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // First see if we need to refresh this cell/server
   //
   if (!AfsAdmSvr_SearchRefresh (idSearchScope, ObjectType, SearchRefresh, &status))
      return FALSE_(status,pStatus,iOp);

   // Prepare an ASIDLIST, and call whatever subroutine is necessary to
   // perform the actual search.
   //
   if ((*ppList = AfsAdmSvr_CreateAsidList()) == NULL)
      return FALSE_(ERROR_NOT_ENOUGH_MEMORY,pStatus,iOp);

   LPTSTR pszPattern = (szPattern && szPattern[0]) ? (LPTSTR)szPattern : NULL;

   switch (GetAsidType (idSearchScope))
      {
      case itCELL:
         if (ObjectType == TYPE_ANY)
            rc = AfsAdmSvr_Search_AllInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_SERVER)
            rc = AfsAdmSvr_Search_ServersInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_SERVICE)
            rc = AfsAdmSvr_Search_ServicesInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_PARTITION)
            rc = AfsAdmSvr_Search_PartitionsInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_VOLUME)
            rc = AfsAdmSvr_Search_VolumesInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_USER)
            rc = AfsAdmSvr_Search_UsersInCell (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_GROUP)
            rc = AfsAdmSvr_Search_GroupsInCell (ppList, idSearchScope, pszPattern, &status);
         else
            {
            rc = FALSE;
            status = ERROR_INVALID_PARAMETER;
            }
         break;

      case itSERVER:
         if (ObjectType == TYPE_ANY)
            rc = AfsAdmSvr_Search_AllInServer (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_SERVICE)
            rc = AfsAdmSvr_Search_ServicesInServer (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_PARTITION)
            rc = AfsAdmSvr_Search_PartitionsInServer (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_VOLUME)
            rc = AfsAdmSvr_Search_VolumesInServer (ppList, idSearchScope, pszPattern, &status);
         else
            {
            rc = FALSE;
            status = ERROR_INVALID_PARAMETER;
            }
         break;

      case itAGGREGATE:
         if (ObjectType == TYPE_ANY)
            rc = AfsAdmSvr_Search_VolumesInPartition (ppList, idSearchScope, pszPattern, &status);
         else if (ObjectType == TYPE_VOLUME)
            rc = AfsAdmSvr_Search_VolumesInPartition (ppList, idSearchScope, pszPattern, &status);
         else
            {
            rc = FALSE;
            status = ERROR_INVALID_PARAMETER;
            }
         break;
      }

   if (rc && (*ppList) && (pSearchParams))
      AfsAdmSvr_Search_Advanced (ppList, pSearchParams);

   if (!rc && (*ppList))
      AfsAdmSvr_FreeAsidList (ppList);
   if (!rc && pStatus)
      *pStatus = status;

   if (!rc)
      Print (dlERROR, TEXT("Client 0x%08lX: FindObjects failed (status=0x%08lX)"), idClient, status);
   else // (rc)
      Print (dlDETAIL, TEXT("Client 0x%08lX: FindObjects succeeded; returning %lu item(s)"), idClient, (*ppList)->cEntries);

   AfsAdmSvr_EndOperation (iOp);
   return rc;
}


      // AfsAdmSvr_GetObject
      // AfsAdmSvr_GetObjects
      // ...returns server-cached information about the specified object (or
      //    objects).
      //
extern "C" int AfsAdmSvr_GetObject (UINT_PTR idClient, AFSADMSVR_GET_TYPE GetType, AFSADMSVR_GET_LEVEL GetLevel, ASID idObject, UINT_PTR verProperties, LPASOBJPROP pProperties, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL2, TEXT("Client 0x%08lX: GetObject (Type=%lu, Level=%lu, idObject=0x%08lX, ver=%ld)"), idClient, (LONG)GetType, (LONG)GetLevel, idObject, verProperties);

   memset (pProperties, 0x00, sizeof(ASOBJPROP));

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   LPASOBJPROP pCurrentProperties;
   if ((pCurrentProperties = AfsAdmSvr_GetCurrentProperties (idObject, pStatus)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: GetObject failed; no properties"), idClient, idObject);
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   // At this point pCurrentProperties may just be rudimentary properties.
   // If the user has requested GET_ALL_DATA, we'll have to get full properties.
   //
   if ( (GetLevel == GET_ALL_DATA) && (pCurrentProperties->verProperties < verPROP_FIRST_SCAN) )
      {
      if (!AfsAdmSvr_ObtainFullProperties (pCurrentProperties, pStatus))
         {
         Print (dlERROR, TEXT("Client 0x%08lX: GetObject failed; no full properties"), idClient, idObject);
         AfsAdmSvr_EndOperation (iOp);
         return FALSE;
         }
      }

   // Now determine if we need to return anything at all; if the user specified
   // RETURN_IF_OUT_OF_DATE, it's possible that there's no need to do so.
   //
   if ((pCurrentProperties->verProperties > verProperties) || (GetType == RETURN_DATA_ALWAYS))
      {
      memcpy (pProperties, pCurrentProperties, sizeof(ASOBJPROP));
      }

   Print (dlDETAIL2, TEXT("Client 0x%08lX: GetObject succeeded (idObject=0x%08lX)"), idClient, idObject);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


extern "C" int AfsAdmSvr_GetObjects (UINT_PTR idClient, AFSADMSVR_GET_TYPE GetType, AFSADMSVR_GET_LEVEL GetLevel, LPASIDLIST pListObjects, LPASOBJPROPLIST *ppListObjectProperties, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetObjects (Type=%lu, Level=%lu, nObjects=%lu)"), idClient, (LONG)GetType, (LONG)GetLevel, (pListObjects) ? (pListObjects->cEntries) : 0);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   *ppListObjectProperties = NULL;
   for (size_t iObject = 0; iObject < pListObjects->cEntries; ++iObject)
      {
      ASOBJPROP ObjectProperties;

      ULONG status;
      if (AfsAdmSvr_GetObject (idClient, GetType, GetLevel, pListObjects->aEntries[ iObject ].idObject, pListObjects->aEntries[ iObject ].lParam, &ObjectProperties, &status))
         {
         if (ObjectProperties.idObject == pListObjects->aEntries[ iObject ].idObject)
            {
            if (!*ppListObjectProperties)
               *ppListObjectProperties = AfsAdmSvr_CreateObjPropList();

            if (*ppListObjectProperties)
               AfsAdmSvr_AddToObjPropList (ppListObjectProperties, &ObjectProperties, 0);
            }
         }
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: GetObjects succeeded; returning %lu properties"), idClient, (*ppListObjectProperties) ? ((*ppListObjectProperties)->cEntries) : 0);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_RefreshObject
      // AfsAdmSvr_RefreshObjects
      // ...invalidates the server's cache of information about the specified
      //    object or objects.
      //
extern "C" int AfsAdmSvr_RefreshObject (UINT_PTR idClient, ASID idObject, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RefreshObject (idObject=0x%08lX)"), idClient, idObject);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   if (!AfsAdmSvr_InvalidateObjectProperties (idObject, pStatus))
      {
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


extern "C" int AfsAdmSvr_RefreshObjects (UINT_PTR idClient, LPASIDLIST pListObjects, ULONG *pStatus)
{
   size_t iOp = AfsAdmSvr_BeginOperation (idClient);

   Print (dlDETAIL, TEXT("Client 0x%08lX: RefreshObjects (nObjects=%lu)"), idClient, (pListObjects) ? (pListObjects->cEntries) : 0);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   for (size_t iObject = 0; iObject < pListObjects->cEntries; ++iObject)
      {
      ULONG status;
      AfsAdmSvr_RefreshObject (idClient, pListObjects->aEntries[ iObject ].idObject, &status);
      }

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_CallbackHost
      // ...provides a context in which the server can issue callback functions
      //    via the AfsAdmSvrCallBack_* routines, which the client must implement.
      //    This routine will only return if the server is shut down. It should
      //    be called on a dedicated thread by the client. (TaAfsAdmSvrClient.lib
      //    automatically handles this.)
      //
extern "C" void AfsAdmSvr_CallbackHost (void)
{
   AfsAdmSvr_CallbackManager();
}



      // AfsAdmSvr_GetRandomKey
      // ...returns a randomly-generated 8-byte encryption key
      //
extern "C" int AfsAdmSvr_GetRandomKey (UINT_PTR idClient, ASID idCell, BYTE keyData[ ENCRYPTIONKEYLENGTH ], ULONG *pStatus)
{
   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   return AfsClass_GetRandomKey ((LPIDENT)idCell, (LPENCRYPTIONKEY)keyData, pStatus);
}

