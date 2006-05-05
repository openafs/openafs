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

#include <WINNT/TaAfsAdmSvr.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_ASIDLIST     64

#define cREALLOC_OBJPROPLIST  32

#define cREALLOC_ACTIONLIST    8


/*
 * REALLOC ____________________________________________________________________
 *
 * This thing, RPC_REALLOC, is a specialized version of the REALLOC() code that
 * you'll find in most libraries. It's intended to deal with structures that
 * look like this:
 *
 * typedef struct {
 *    DWORD dwGarbage1;
 *    size_t cElements;     <- The interesting bit
 *    DWORD dwGarbage2;
 *    MYTYPE aElements[1];  <- <- <- The really interesting bit
 * } MYSTRUCT;
 *
 * Remember that since the array is growable, it must be the last element
 * in the structure. This thing's called RPC_REALLOC because these growable
 * arrays are well-suited for transferring via rpc--just make the aElements
 * line in your .IDL file look like:
 *
 *    [size_is(cElements) length_is(cElements)] MYTYPE aElements[*];
 *
 * As an example of its use, say we want to fill in element 15:
 *
 *    void FillInElement15 (MYSTRUCT *pStruct, MYTYPE *pNewData)
 *    {
 *       int iElement = 15;
 *       if (RPC_REALLOC (MYSTRUCT, pStruct, aElements, cElements, 1+iElement, cREALLOC_MYSTRUCT))
 *       {
 *          memcpy (&pStruct->aElements[ iElement ], pNewData, sizeof(MYTYPE));
 *       }
 *    }
 *
 * As always, the "cREALLOC_MYSTRUCT" can be any positive value; it specifies
 * the granularity with which the the array will be over-allocated. Note that
 * unlike the normal REALLOC() routine, the {aElements} and {cElements}
 * parameters are presumed to be members of the specified pointer-to-structure.
 * If {*pStruct} is NULL upon entry, it will be allocated and zero-filled.
 *
 */

#define OFFSETOF(_type,_a) (size_t)&(((_type *)0)->_a)
#define RPC_REALLOC(_type,_str,_a,_c,_r,_ci) AfsAdmSvr_ReallocFunction ((PVOID*)&(_str), OFFSETOF(_type,_a), OFFSETOF(_type,_c), sizeof((_str)->_a[0]), (size_t)_r, (size_t)_ci, 0x00)

BOOL AfsAdmSvr_ReallocFunction (PVOID *ppStructure, size_t cbHeader, size_t iposCount, size_t cbElement, size_t cReq, size_t cInc, BYTE chFill)
{
   // Use cInc to over-estimate how much space is really required; that
   // way, allocating space for one element actually allocates space for
   // many--so that next time we get here, we won't have to do any work.
   //
   if (cInc)
      cReq = cInc * ( (cReq + cInc - 1) / cInc );
   cReq = max (cReq, 1);

   // See how much space is allocated now. If we have no structure to start
   // with, obviously we have no array elements either.
   //
   size_t cNow = 0;
   if (*ppStructure)
      cNow = *(size_t *)( ((PBYTE)(*ppStructure)) + iposCount );

   if (cNow < cReq)
      {
      // Hmmm... there wasn't enough space. Allocate a new structure.
      //
      size_t cbAlloc = cbHeader + cbElement * cReq;
      PVOID pNewStructure;
      if ((pNewStructure = Allocate (cbAlloc)) == NULL)
         return FALSE;

      memset (pNewStructure, 0x00, cbHeader);
      memset ((PBYTE)pNewStructure + cbHeader, chFill, cbAlloc - cbHeader);
      if (*ppStructure)
         memcpy (pNewStructure, *ppStructure, cbHeader);

      *(size_t *)( ((PBYTE)pNewStructure) + iposCount ) = cReq;

      // Transfer any information from the old structure's elements
      //
      if (cNow)
         memcpy ((PBYTE)pNewStructure + cbHeader, ((PBYTE)(*ppStructure)) + cbHeader, cNow * cbElement);

      // If there was one, free the old structure
      //
      if (*ppStructure)
         Free (*ppStructure);
      *ppStructure = pNewStructure;
      }

   return TRUE;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

      // ASIDLIST - Managed type for lists of cell objects
      //
LPASIDLIST AfsAdmSvr_CreateAsidList (void)
{
   LPASIDLIST pList = NULL;
   if (!RPC_REALLOC (ASIDLIST, pList, aEntries, cEntriesAllocated, 0, cREALLOC_ASIDLIST))
      return NULL;
   return pList;
}

LPASIDLIST AfsAdmSvr_CopyAsidList (LPASIDLIST pListSource)
{
   LPASIDLIST pList = NULL;
   if (!RPC_REALLOC (ASIDLIST, pList, aEntries, cEntriesAllocated, pListSource->cEntries, cREALLOC_ASIDLIST))
      return NULL;
   if (pList->cEntriesAllocated)
      memcpy (pList->aEntries, pListSource->aEntries, sizeof(pList->aEntries[0]) * pList->cEntriesAllocated);
   pList->cEntries = pListSource->cEntries;
   return pList;
}

BOOL AfsAdmSvr_AddToAsidList (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   if (!ppList || !*ppList)
      return FALSE;

   if (!RPC_REALLOC (ASIDLIST, *ppList, aEntries, cEntriesAllocated, (*ppList)->cEntries +1, cREALLOC_ASIDLIST))
      return FALSE;

   (*ppList)->aEntries[ (*ppList)->cEntries ].idObject = idObject;
   (*ppList)->aEntries[ (*ppList)->cEntries ].lParam = lp;
   (*ppList)->cEntries ++;
   return TRUE;
}

BOOL AfsAdmSvr_RemoveFromAsidList (LPASIDLIST *ppList, ASID idObject)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   BOOL fFound = FALSE;
   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; )
      {
      if ((*ppList)->aEntries[ iEntry ].idObject != idObject)
         iEntry ++;
      else if ((fFound = AfsAdmSvr_RemoveFromAsidListByIndex (ppList, iEntry)) == FALSE)
         break;
      }

   return fFound;
}


BOOL AfsAdmSvr_RemoveFromAsidListByIndex (LPASIDLIST *ppList, size_t iIndex)
{
   if (iIndex >= (*ppList)->cEntries)
      return FALSE;

   if (iIndex < (*ppList)->cEntries -1)
      memcpy (&(*ppList)->aEntries[ iIndex ], &(*ppList)->aEntries[ (*ppList)->cEntries-1 ], sizeof((*ppList)->aEntries[0]));
   (*ppList)->cEntries --;
   return TRUE;
}


BOOL AfsAdmSvr_SetAsidListParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp)
{
   BOOL fFound = FALSE;
   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; ++iEntry)
      {
      if ((*ppList)->aEntries[ iEntry ].idObject == idObject)
         {
         (*ppList)->aEntries[ iEntry ].lParam = lp;
         fFound = TRUE;
         }
      }
   return fFound;
}


BOOL AfsAdmSvr_SetAsidListParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp)
{
   if (iIndex >= (*ppList)->cEntries)
      return FALSE;

   (*ppList)->aEntries[ iIndex ].lParam = lp;
   return TRUE;
}


BOOL AfsAdmSvr_IsInAsidList (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; ++iEntry)
      {
      if ((*ppList)->aEntries[ iEntry ].idObject == idObject)
         {
         if (pParam)
            *pParam = (*ppList)->aEntries[ iEntry ].lParam;
         return TRUE;
         }
      }

   return FALSE;
}


void AfsAdmSvr_FreeAsidList (LPASIDLIST *ppList)
{
   if (ppList && *ppList)
      {
      Free (*ppList);
      (*ppList) = 0;
      }
}


      // ASOBJPROPLIST - Managed type for lists of cell objects
      //
LPASOBJPROPLIST AfsAdmSvr_CreateObjPropList (void)
{
   LPASOBJPROPLIST pList = NULL;
   if (!RPC_REALLOC (ASOBJPROPLIST, pList, aEntries, cEntriesAllocated, 0, cREALLOC_OBJPROPLIST))
      return NULL;
   return pList;
}

LPASOBJPROPLIST AfsAdmSvr_CopyObjPropList (LPASOBJPROPLIST pListSource)
{
   LPASOBJPROPLIST pList = NULL;
   if (!RPC_REALLOC (ASOBJPROPLIST, pList, aEntries, cEntriesAllocated, pListSource->cEntries, cREALLOC_OBJPROPLIST))
      return NULL;
   if (pList->cEntriesAllocated)
      memcpy (pList->aEntries, pListSource->aEntries, sizeof(pList->aEntries[0]) * pList->cEntriesAllocated);
   pList->cEntries = pListSource->cEntries;
   return pList;
}

BOOL AfsAdmSvr_AddToObjPropList (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp)
{
   if (!ppList || !*ppList)
      return FALSE;

   if (!RPC_REALLOC (ASOBJPROPLIST, *ppList, aEntries, cEntriesAllocated, (*ppList)->cEntries +1, cREALLOC_OBJPROPLIST))
      return NULL;

   memcpy (&(*ppList)->aEntries[ (*ppList)->cEntries ].ObjectProperties, pProperties, sizeof(ASOBJPROP));
   (*ppList)->aEntries[ (*ppList)->cEntries ].lParam = lp;
   (*ppList)->cEntries ++;
   return TRUE;
}

BOOL AfsAdmSvr_RemoveFromObjPropList (LPASOBJPROPLIST *ppList, ASID idObject)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   BOOL fFound = FALSE;
   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; )
      {
      if ((*ppList)->aEntries[ iEntry ].ObjectProperties.idObject != idObject)
         iEntry ++;
      else
         {
         fFound = TRUE;
         if (iEntry < (*ppList)->cEntries -1)
            memcpy (&(*ppList)->aEntries[ iEntry ], &(*ppList)->aEntries[ (*ppList)->cEntries-1 ], sizeof((*ppList)->aEntries[0]));
         (*ppList)->cEntries --;
         }
      }

   return fFound;
}

BOOL AfsAdmSvr_IsInObjPropList (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties, LPARAM *pParam)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; ++iEntry)
      {
      if ((*ppList)->aEntries[ iEntry ].ObjectProperties.idObject == idObject)
         {
         if (pProperties)
            memcpy (pProperties, &(*ppList)->aEntries[ iEntry ].ObjectProperties, sizeof(ASOBJPROP));
         if (pParam)
            *pParam = (*ppList)->aEntries[ iEntry ].lParam;
         return TRUE;
         }
      }

   return FALSE;
}

void AfsAdmSvr_FreeObjPropList (LPASOBJPROPLIST *ppList)
{
   if (ppList && *ppList)
      {
      Free (*ppList);
      (*ppList) = 0;
      }
}


      // ASACTIONLIST - Managed type for lists of cell objects
      //
LPASACTIONLIST AfsAdmSvr_CreateActionList (void)
{
   LPASACTIONLIST pList = NULL;
   if (!RPC_REALLOC (ASACTIONLIST, pList, aEntries, cEntriesAllocated, 0, cREALLOC_ACTIONLIST))
      return NULL;
   return pList;
}

LPASACTIONLIST AfsAdmSvr_CopyActionList (LPASACTIONLIST pListSource)
{
   LPASACTIONLIST pList = NULL;
   if (!RPC_REALLOC (ASACTIONLIST, pList, aEntries, cEntriesAllocated, pListSource->cEntries, cREALLOC_ACTIONLIST))
      return NULL;
   if (pList->cEntriesAllocated)
      memcpy (pList->aEntries, pListSource->aEntries, sizeof(pList->aEntries[0]) * pList->cEntriesAllocated);
   pList->cEntries = pListSource->cEntries;
   return pList;
}

BOOL AfsAdmSvr_AddToActionList (LPASACTIONLIST *ppList, LPASACTION pAction)
{
   if (!ppList || !*ppList)
      return FALSE;

   if (!RPC_REALLOC (ASACTIONLIST, *ppList, aEntries, cEntriesAllocated, (*ppList)->cEntries +1, cREALLOC_OBJPROPLIST))
      return NULL;

   memcpy (&(*ppList)->aEntries[ (*ppList)->cEntries ].Action, pAction, sizeof(ASACTION));
   (*ppList)->cEntries ++;
   return TRUE;
}

BOOL AfsAdmSvr_RemoveFromActionList (LPASACTIONLIST *ppList, DWORD idAction)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   BOOL fFound = FALSE;
   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; )
      {
      if ((*ppList)->aEntries[ iEntry ].Action.idAction != idAction)
         iEntry ++;
      else
         {
         fFound = TRUE;
         if (iEntry < (*ppList)->cEntries -1)
            memcpy (&(*ppList)->aEntries[ iEntry ], &(*ppList)->aEntries[ (*ppList)->cEntries-1 ], sizeof((*ppList)->aEntries[0]));
         (*ppList)->cEntries --;
         }
      }

   return fFound;
}

BOOL AfsAdmSvr_IsInActionList (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction)
{
   if (!ppList || !(*ppList) || !(*ppList)->cEntries)
      return FALSE;

   for (ULONG iEntry = 0; iEntry < (*ppList)->cEntries; ++iEntry)
      {
      if ((*ppList)->aEntries[ iEntry ].Action.idAction == idAction)
         {
         if (pAction)
            memcpy (pAction, &(*ppList)->aEntries[ iEntry ].Action, sizeof(ASACTION));
         return TRUE;
         }
      }

   return FALSE;
}

void AfsAdmSvr_FreeActionList (LPASACTIONLIST *ppList)
{
   if (ppList && *ppList)
      {
      Free (*ppList);
      (*ppList) = 0;
      }
}

