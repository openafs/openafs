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
#include <WINNT/AfsAppLib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   ASID idCell;
   LPHASHLIST pCache;
   LPHASHLISTKEY pCacheKeyAsid;
   DWORD cReqCache;
   } CELLCACHE, *LPCELLCACHE;

static struct
   {
   LPHASHLIST pCells;
   LPHASHLISTKEY pCellsKeyAsid;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK CacheKeyAsid_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
HASHVALUE CALLBACK CacheKeyAsid_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
HASHVALUE CALLBACK CacheKeyAsid_HashData (LPHASHLISTKEY pKey, PVOID pData);

BOOL CALLBACK CellsKeyAsid_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
HASHVALUE CALLBACK CellsKeyAsid_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
HASHVALUE CALLBACK CellsKeyAsid_HashData (LPHASHLISTKEY pKey, PVOID pData);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPCELLCACHE GetCellCache (ASID idCell)
{
   if (!l.pCells)
      return NULL;

   return (LPCELLCACHE)(l.pCellsKeyAsid->GetFirstObject (&idCell));
}


BOOL CreateCellCache (ASID idCell)
{
   asc_Enter();

   if (!l.pCells)
      {
      l.pCells = New (HASHLIST);
      l.pCells->SetCriticalSection (asc_GetCriticalSection());
      l.pCellsKeyAsid = l.pCells->CreateKey (TEXT("ASID"), CellsKeyAsid_Compare, CellsKeyAsid_HashObject, CellsKeyAsid_HashData);
      }

   LPCELLCACHE pcc;
   if ((pcc = GetCellCache (idCell)) == NULL)
      {
      pcc = New (CELLCACHE);
      memset (pcc, 0x00, sizeof(CELLCACHE));
      pcc->idCell = idCell;
      pcc->pCache = New (HASHLIST);
      pcc->pCache->SetCriticalSection (asc_GetCriticalSection());
      pcc->pCacheKeyAsid = pcc->pCache->CreateKey (TEXT("ASID"), CacheKeyAsid_Compare, CacheKeyAsid_HashObject, CacheKeyAsid_HashData);
      l.pCells->Add (pcc);
      }
   pcc->cReqCache ++;

   asc_Leave();
   return TRUE;
}


BOOL DestroyCellCache (ASID idCell)
{
   asc_Enter();

   LPCELLCACHE pcc;
   if ((pcc = GetCellCache (idCell)) == NULL)
      {
      asc_Leave();
      return FALSE;
      }

   if (!pcc->cReqCache || !(--(pcc->cReqCache)))
      {
      if (pcc->pCache)
         {
         for (LPENUM pEnum = pcc->pCache->FindFirst(); pEnum; pEnum = pEnum->FindNext())
            {
            LPASOBJPROP pProp = (LPASOBJPROP)( pEnum->GetObject() );
            pcc->pCache->Remove (pProp);
            Delete (pProp);
            }

         Delete (pcc->pCache);
         }

      l.pCells->Remove (pcc);
      Delete (pcc);
      }

   asc_Leave();
   return TRUE;
}


LPASOBJPROP GetCachedProperties (ASID idCell, ASID idObject)
{
   LPASOBJPROP pCachedProperties = NULL;
   asc_Enter();

   LPCELLCACHE pcc;
   if ((pcc = GetCellCache (idCell)) != NULL)
      {
      pCachedProperties = (LPASOBJPROP)(pcc->pCacheKeyAsid->GetFirstObject (&idObject));
      }

   asc_Leave();
   return pCachedProperties;
}


void UpdateCachedProperties (ASID idCell, ASID idObject, LPASOBJPROP pProperties)
{
   if (pProperties)
      {
      asc_Enter();

      LPCELLCACHE pcc;
      if ((pcc = GetCellCache (idCell)) != NULL)
         {
         LPASOBJPROP pCachedProperties;
         if ((pCachedProperties = (LPASOBJPROP)(pcc->pCacheKeyAsid->GetFirstObject (&idObject))) == NULL)
            {
            pCachedProperties = New (ASOBJPROP);
            memcpy (pCachedProperties, pProperties, sizeof(ASOBJPROP));
            pcc->pCache->Add (pCachedProperties);
            }
         else // Just update?
            {
            memcpy (pCachedProperties, pProperties, sizeof(ASOBJPROP));
            // Note: don't need to call pcc->pCache->Update(), because
            // we haven't affected any indices (the old and new ASOBJPROP
            // structures should have the same ASID)
            }
         }

      NotifyObjectListeners (idCell, idObject);
      asc_Leave();
      }
}


BOOL RefreshCachedProperties (UINT_PTR idClient, ASID idCell, ASID idObject, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      LPASOBJPROP pProperties = GetCachedProperties (idCell, idObject);
      DWORD verProperties = (pProperties) ? (pProperties->verProperties) : verPROP_NO_OBJECT;

      ASOBJPROP NewProperties;
      if ((rc = AfsAdmSvr_GetObject (idClient, RETURN_IF_OUT_OF_DATE, GetLevel, idObject, verProperties, &NewProperties, &status)) != FALSE)
         {
         if (NewProperties.idObject == idObject)
            {
            UpdateCachedProperties (idCell, idObject, &NewProperties);
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


BOOL RefreshCachedProperties (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (pAsidList->cEntries)
      {
      RpcTryExcept
         {
         for (size_t iObject = 0; iObject < pAsidList->cEntries; ++iObject)
            {
            LPASOBJPROP pProperties = GetCachedProperties (idCell, pAsidList->aEntries[ iObject ].idObject);
            pAsidList->aEntries[ iObject ].lParam = (pProperties) ? (pProperties->verProperties) : verPROP_NO_OBJECT;
            }

         LPASOBJPROPLIST pNewProperties = NULL;
         if ((rc = AfsAdmSvr_GetObjects (idClient, RETURN_IF_OUT_OF_DATE, GetLevel, pAsidList, &pNewProperties, &status)) != FALSE)
            {
            if (pNewProperties)
               {
               for (size_t iObject = 0; iObject < pNewProperties->cEntries; ++iObject)
                  {
                  UpdateCachedProperties (idCell, pNewProperties->aEntries[ iObject ].ObjectProperties.idObject, &pNewProperties->aEntries[ iObject ].ObjectProperties);
                  }
               AfsAdmSvr_FreeObjPropList (&pNewProperties);
               }
            }
         }
      RpcExcept(1)
         {
         rc = FALSE;
         status = RPC_S_CALL_FAILED_DNE;
         }
      RpcEndExcept
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


/*
 * HASHLIST KEYS ______________________________________________________________
 *
 */

BOOL CALLBACK CacheKeyAsid_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((LPASOBJPROP)pObject)->idObject == *(ASID*)pData);
}

HASHVALUE CALLBACK CacheKeyAsid_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CacheKeyAsid_HashData (pKey, &((LPASOBJPROP)pObject)->idObject);
}

HASHVALUE CALLBACK CacheKeyAsid_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*(ASID*)pData;
}


BOOL CALLBACK CellsKeyAsid_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((LPCELLCACHE)pObject)->idCell == *(ASID*)pData);
}

HASHVALUE CALLBACK CellsKeyAsid_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CellsKeyAsid_HashData (pKey, &((LPCELLCACHE)pObject)->idCell);
}

HASHVALUE CALLBACK CellsKeyAsid_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*(ASID*)pData;
}

