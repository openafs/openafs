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

#include <WINNT/TaLocale.h>


/*
 * HashList - super-quick list manager
 *
 */

#include <windows.h>
#include <WINNT/hashlist.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define iINVALID             ((size_t)-1)

        // We over-allocate the m_aObjects[] arrays whenever one fills up,
        // in order to perform fewer allocations. The allocation size
        // is defined by the macro below:
        //
#define cREALLOC_OBJECTS       8192

        // We also over-allocate the m_aKeys[] array in a hashlist, but
        // nothing so dramatic here. It's simply always allocated to a
        // multiple of 4.
        //
#define cREALLOC_KEYS            4

        // The hashtables within each HASHLISTKEY also automatically resize
        // themselves whenever they're too small for the number of objects
        // they're supporting. There are two algorithms: a slow progression
        // for user-defined keys, and a rapid progression for the index key.
        // The difference is utilized because the index key (the key used
        // internally by each hashlist to provide object-address-to-index
        // lookups for fast Remove() calls) responds well to huge hash table
        // sizes (because it hashes on addresses, which are evenly
        // distributed). User-defined keys don't always use hashing indexes
        // that respond so well to additional hashtable size, so they generally
        // end up with smaller hash tables (so as not to waste memory).
        // The algorithms below cause the following progression (presuming
        // the default starting size of 1000 buckets):
        //
        // Buckets in normal keys:             Buckets in index key:
        //    1000 (up to  30000 objects)         1000 (up to   23000 objects)
        //    3000 (up to  90000 objects)         4600 (up to  105800 objects)
        //    9000 (up to 270000 objects)        21160 (up to  486680 objects)
        //   27000 (up to 810000 objects)        97336 (up to 2238728 objects)
        //
#define MIN_TABLE_SIZE(_cObjects)     ((_cObjects) / 30)
#define TARGET_TABLE_SIZE(_cObjects)  ((_cObjects) / 10)

#define MIN_TABLE_SIZE_FOR_KEY(_cObjects)     ((_cObjects) / 23)
#define TARGET_TABLE_SIZE_FOR_KEY(_cObjects)  ((_cObjects) /  5)


/*
 * REALLOC ____________________________________________________________________
 *
 */

#ifdef REALLOC
#undef REALLOC
#endif
#define REALLOC(_a,_c,_r,_i) HashList_ReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL HashList_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)GlobalAlloc (GMEM_FIXED, cbElement * cNew)) == NULL)
      return FALSE;

   if (*pcTarget == 0)
      memset (pNew, 0x00, cbElement * cNew);
   else
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      memset (&((char*)pNew)[ cbElement * (*pcTarget) ], 0x00, cbElement * (cNew - *pcTarget));
      GlobalFree ((HGLOBAL)*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}


/*
 * EXPANDARRAY CLASS __________________________________________________________
 *
 */

#define cEXPANDARRAYHEAPELEMENTS  1024
#define cREALLOC_EXPANDARRAYHEAPS   16

EXPANDARRAY::EXPANDARRAY (size_t cbElement, size_t cElementsPerHeap)
{
   if ((m_cbElement = cbElement) == 0)
      m_cbElement = sizeof(DWORD);
   if ((m_cElementsPerHeap = cElementsPerHeap) == 0)
      m_cElementsPerHeap = cEXPANDARRAYHEAPELEMENTS;

   m_cHeaps = 0;
   m_aHeaps = 0;
}

EXPANDARRAY::~EXPANDARRAY (void)
{
   if (m_aHeaps)
      {
      for (size_t ii = 0; ii < m_cHeaps; ++ii)
         {
         if (m_aHeaps[ ii ])
            GlobalFree ((HGLOBAL)(m_aHeaps[ ii ]));
         }
      GlobalFree ((HGLOBAL)m_aHeaps);
      }
}

PVOID EXPANDARRAY::GetAt (size_t iElement)
{
   size_t iHeap = iElement / m_cElementsPerHeap;
   size_t iIndex = iElement % m_cElementsPerHeap;
   if ((iHeap >= m_cHeaps) || (!m_aHeaps[iHeap]))
      return NULL;

   PVOID pElement;
   pElement = &((PBYTE)m_aHeaps[ iHeap ]->aElements)[ iIndex * m_cbElement ];
   return pElement;
}

PVOID EXPANDARRAY::SetAt (size_t iElement, PVOID pData)
{
   size_t iHeap = iElement / m_cElementsPerHeap;
   size_t iIndex = iElement % m_cElementsPerHeap;

   if (!REALLOC (m_aHeaps, m_cHeaps, 1+iHeap, cREALLOC_EXPANDARRAYHEAPS))
      return NULL;

   if (!m_aHeaps[ iHeap ])
      {
      size_t cbHeap = sizeof(EXPANDARRAYHEAP) + (m_cElementsPerHeap * m_cbElement);
      if ((m_aHeaps[ iHeap ] = (LPEXPANDARRAYHEAP)GlobalAlloc (GMEM_FIXED, cbHeap)) == NULL)
         return NULL;
      memset (m_aHeaps[ iHeap ], 0x00, cbHeap);
      m_aHeaps[ iHeap ]->aElements = ((PBYTE)m_aHeaps[ iHeap ]) + sizeof(EXPANDARRAYHEAP);
      }

   PVOID pElement;
   pElement = &((PBYTE)m_aHeaps[ iHeap ]->aElements)[ iIndex * m_cbElement ];
   if (pData)
      memcpy (pElement, pData, m_cbElement);
   return pElement;
}


/*
 * HASHLIST CLASS _____________________________________________________________
 *
 */

#define GetEntry(_pArray,_iEntry)  ((LPHASHLISTENTRY)((_pArray)->GetAt(_iEntry)))

HASHLIST::HASHLIST (void)
{
   InitializeCriticalSection (&m_cs);
   m_pcs = &m_cs;
   Enter();

   m_iFirst = iINVALID;
   m_iLast = iINVALID;
   m_iNextFree = 0;
   m_aObjects = new EXPANDARRAY (sizeof(HASHLISTENTRY), cREALLOC_OBJECTS);
   m_cObjects = 0;
   m_cObjectsMax = 0;
   m_apKeys = NULL;
   m_cpKeys = 0;

   m_pKeyIndex = new HASHLISTKEY (this, TEXT("Index"), HASHLIST::KeyIndex_CompareObjectData, HASHLIST::KeyIndex_HashObject, HASHLIST::KeyIndex_HashData, cTABLESIZE_DEFAULT);

   Leave();
}


HASHLIST::~HASHLIST (void)
{
   Enter();

   if (m_apKeys != NULL)
      {
      for (size_t iKey = 0; iKey < m_cpKeys; ++iKey)
         {
         if (m_apKeys[ iKey ] != NULL)
            delete m_apKeys[ iKey ];
         }
      GlobalFree ((HGLOBAL)m_apKeys);
      }

   if (m_pKeyIndex)
      {
      delete m_pKeyIndex;
      }

   if (m_aObjects != NULL)
      {
      delete m_aObjects;
      }

   Leave();
   DeleteCriticalSection (&m_cs);
}


void HASHLIST::Enter (void)
{
   EnterCriticalSection (m_pcs);
}


void HASHLIST::Leave (void)
{
   LeaveCriticalSection (m_pcs);
}


void HASHLIST::SetCriticalSection (CRITICAL_SECTION *pcs)
{
   m_pcs = (pcs == NULL) ? &m_cs : pcs;
}


BOOL HASHLIST::AddUnique (PVOID pEntryToAdd, LPHASHLISTKEY pKey)
{
   if ( ((pKey == NULL) && (fIsInList (pEntryToAdd))) ||
        ((pKey != NULL) && (pKey->fIsInList (pEntryToAdd))) )
      {
      return TRUE;
      }

   return Add (pEntryToAdd);
}


BOOL HASHLIST::Add (PVOID pEntryToAdd)
{
   BOOL rc = FALSE;
   Enter();

   if (pEntryToAdd)
      {
      size_t iObject = m_iNextFree;

      if ((GetEntry(m_aObjects,iObject) && (GetEntry(m_aObjects,iObject)->pObject)))
         {
         for (iObject = 0; iObject < m_cObjectsMax; ++iObject)
            {
            if ( (!GetEntry(m_aObjects,iObject)) || (!GetEntry(m_aObjects,iObject)->pObject) )
               break;
            }
         }

      m_iNextFree = 1+iObject;

      HASHLISTENTRY Entry;
      Entry.pObject = pEntryToAdd;
      Entry.iNext = iINVALID;
      Entry.iPrevious = m_iLast;
      m_aObjects->SetAt (iObject, &Entry);

      if (m_iLast != iINVALID)
         {
         LPHASHLISTENTRY pEntry;
         if ((pEntry = GetEntry(m_aObjects,m_iLast)) != NULL)
            pEntry->iNext = iObject;
         }

      m_iLast = iObject;
      if (m_iFirst == iINVALID)
         m_iFirst = iObject;

      m_pKeyIndex->Add (iObject, (PVOID)(iObject+1));

      for (size_t iKey = 0; iKey < m_cpKeys; ++iKey)
         {
         if (m_apKeys[ iKey ] == NULL)
            continue;
         m_apKeys[ iKey ]->Add (iObject, pEntryToAdd);
         }

      ++m_cObjects;
      m_cObjectsMax = max (m_cObjectsMax, m_cObjects);
      rc = TRUE;
      }

   Leave();
   return rc;
}


void HASHLIST::Remove (PVOID pEntryToRemove)
{
   Enter();

   if (pEntryToRemove)
      {
      // The first step is to find which m_aObjects entry corresponds
      // with this object. Ordinarily that'd be a brute-force search;
      // however, to speed things up, we have a special key placed on
      // the address of the object for determining its index. It lets
      // us replace this code:
      //
      // for (size_t iObject = 0; iObject < m_cObjectsMax; ++iObject)
      //   {
      //   if (m_aObjects[ iObject ].pObject == pEntryToRemove)
      //      break;
      //   }
      //
      // Because this index key uses a hashtable that's resized very
      // aggressively, performance tests of up to 100,000 objects
      // indicate that removing an object now clocks in as O(1), rather
      // than the O(N) that it would be if we used the brute-force approach.
      // At 100,000 objects it does use up some 70k of memory, but wow
      // is it fast...
      //
      size_t iObject = iINVALID;

      PVOID pFind;
      if ((pFind = m_pKeyIndex->GetFirstObject (pEntryToRemove)) != NULL)
         iObject = *(size_t *)&pFind -1;

      // Now remove the object.
      //
      if (iObject != iINVALID)
         {
         LPHASHLISTENTRY pEntry;
         if ((pEntry = GetEntry(m_aObjects,iObject)) != NULL)
            {
            pEntry->pObject = NULL;

            if (pEntry->iPrevious != iINVALID)
               {
               LPHASHLISTENTRY pPrevious;
               if ((pPrevious = GetEntry(m_aObjects,pEntry->iPrevious)) != NULL)
                  pPrevious->iNext = pEntry->iNext;
               }
            if (pEntry->iNext != iINVALID)
               {
               LPHASHLISTENTRY pNext;
               if ((pNext = GetEntry(m_aObjects,pEntry->iNext)) != NULL)
                  pNext->iPrevious = pEntry->iPrevious;
               }

            if (m_iLast == iObject)
               m_iLast = pEntry->iPrevious;
            if (m_iFirst == iObject)
               m_iFirst = pEntry->iNext;

            for (size_t iKey = 0; iKey < m_cpKeys; ++iKey)
               {
               if (m_apKeys[ iKey ] == NULL)
                  continue;
               m_apKeys[ iKey ]->Remove (iObject);
               }

            m_pKeyIndex->Remove (iObject);
            --m_cObjects;
            }
         }
      }

   Leave();
}


BOOL HASHLIST::Update (PVOID pEntryToUpdate)
{
   BOOL rc = TRUE;
   Enter();

   PVOID pFind;
   if ((pFind = m_pKeyIndex->GetFirstObject (pEntryToUpdate)) == NULL)
      rc = FALSE;
   else
      {
      size_t iObject = *(size_t *)&pFind -1;

      for (size_t iKey = 0; iKey < m_cpKeys; ++iKey)
         {
         if (m_apKeys[ iKey ] == NULL)
            continue;

         m_apKeys[ iKey ]->Remove (iObject);
         m_apKeys[ iKey ]->Add (iObject, pEntryToUpdate);
         }
      }

   Leave();
   return rc;
}


BOOL HASHLIST::fIsInList (PVOID pEntry)
{
   PVOID pFind;
   if ((pFind = m_pKeyIndex->GetFirstObject (pEntry)) == NULL)
      return FALSE;
   return TRUE;
}


LPHASHLISTKEY HASHLIST::CreateKey (LPCTSTR pszKeyName, LPHASHFUNC_COMPAREOBJECTDATA fnCompareObjectData, LPHASHFUNC_HASHOBJECT fnHashObject, LPHASHFUNC_HASHDATA fnHashData, size_t cTableSize)
{
   LPHASHLISTKEY pKey = NULL;
   Enter();

   size_t iKey;
   for (iKey = 0; iKey < m_cpKeys; ++iKey)
      {
      if (m_apKeys[ iKey ] == NULL)
         break;
      }
   if (REALLOC (m_apKeys, m_cpKeys, 1+iKey, cREALLOC_KEYS))
      {
      m_apKeys[ iKey ] = new HASHLISTKEY (this, pszKeyName, fnCompareObjectData, fnHashObject, fnHashData, cTableSize);
      pKey = m_apKeys[ iKey ];

      if (MIN_TABLE_SIZE(m_cObjectsMax) > pKey->m_cBuckets)
         {
         pKey->Resize();
         }
      else for (size_t iObject = 0; iObject < m_cObjectsMax; ++iObject)
         {
         LPHASHLISTENTRY pEntry;
         if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
            continue;
         if (pEntry->pObject != NULL)
            m_apKeys[ iKey ]->Add (iObject, pEntry->pObject);
         }
      }

   Leave();
   return pKey;
}


LPHASHLISTKEY HASHLIST::FindKey (LPCTSTR pszKeyName)
{
   LPHASHLISTKEY pKey = NULL;
   Enter();

   for (size_t iKey = 0; !pKey && (iKey < m_cpKeys); ++iKey)
      {
      if (m_apKeys[ iKey ] == NULL)
         continue;
      if (!lstrcmpi (m_apKeys[ iKey ]->m_szKeyName, pszKeyName))
         pKey = m_apKeys[ iKey ];
      }

   Leave();
   return pKey;
}


void HASHLIST::RemoveKey (LPHASHLISTKEY pKey)
{
   Enter();

   for (size_t iKey = 0; iKey < m_cpKeys; ++iKey)
      {
      if (m_apKeys[ iKey ] == pKey)
         {
         delete m_apKeys[ iKey ];
         m_apKeys[ iKey ] = NULL;
         break;
         }
      }

   Leave();
}


LPENUM HASHLIST::FindFirst (void)
{
   LPENUM pEnum = NULL;
   Enter();

   if (m_iFirst != iINVALID)
      pEnum = New2 (ENUMERATION,(this, m_iFirst));

   Leave();
   return pEnum;
}


LPENUM HASHLIST::FindLast (void)
{
   LPENUM pEnum = NULL;
   Enter();

   if (m_iLast != iINVALID)
      pEnum = New2 (ENUMERATION,(this, m_iLast));

   Leave();
   return pEnum;
}


PVOID HASHLIST::GetFirstObject (void)
{
   PVOID pObject = NULL;
   Enter();

   if (m_iFirst != iINVALID)
      pObject = GetEntry(m_aObjects,m_iFirst)->pObject;

   Leave();
   return pObject;
}


PVOID HASHLIST::GetLastObject (void)
{
   PVOID pObject = NULL;
   Enter();

   if (m_iLast != iINVALID)
      pObject = GetEntry(m_aObjects,m_iLast)->pObject;

   Leave();
   return pObject;
}


size_t HASHLIST::GetCount (void)
{
   return m_cObjects;
}


BOOL CALLBACK HASHLIST::KeyIndex_CompareObjectData (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   size_t iIndex = (size_t)pObject -1;
   LPHASHLIST pList = pKey->GetHashList();
   return (GetEntry(pList->m_aObjects,iIndex)->pObject == pData) ? TRUE : FALSE;
}


HASHVALUE CALLBACK HASHLIST::KeyIndex_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   size_t iIndex = (size_t)pObject -1;
   LPHASHLIST pList = pKey->GetHashList();
   return KeyIndex_HashData (pKey, GetEntry(pList->m_aObjects,iIndex)->pObject);
}


HASHVALUE CALLBACK HASHLIST::KeyIndex_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return ((DWORD)pData) >> 4;  // The "data" is the object's address.
}


/*
 * HASHLISTKEY CLASS _____________________________________________________________
 *
 */

HASHLISTKEY::HASHLISTKEY (LPHASHLIST pList, LPCTSTR pszKeyName, LPHASHFUNC_COMPAREOBJECTDATA fnCompareObjectData, LPHASHFUNC_HASHOBJECT fnHashObject, LPHASHFUNC_HASHDATA fnHashData, size_t cTableSize)
{
   m_aObjects = new EXPANDARRAY (sizeof(HASHLISTENTRY), cREALLOC_OBJECTS);

   m_cBuckets = (cTableSize == 0) ? cTABLESIZE_DEFAULT : cTableSize;
   m_aBuckets = (struct HASHBUCKET *)GlobalAlloc (GMEM_FIXED, m_cBuckets * sizeof(struct HASHBUCKET));
   for (size_t iBucket = 0; iBucket < m_cBuckets; ++iBucket)
      {
      m_aBuckets[ iBucket ].iFirst = iINVALID;
      m_aBuckets[ iBucket ].iLast = iINVALID;
      }

   m_pList = pList;
   lstrcpy (m_szKeyName, pszKeyName);
   m_fnCompareObjectData = fnCompareObjectData;
   m_fnHashObject = fnHashObject;
   m_fnHashData = fnHashData;
}


HASHLISTKEY::~HASHLISTKEY (void)
{
   m_pList->Enter();

   for (size_t iObject = 0; iObject < m_pList->m_cObjectsMax; ++iObject)
      {
      if (!GetEntry(m_aObjects,iObject))
         continue;
      if (!GetEntry(m_aObjects,iObject)->pObject)
         continue;
      Remove (iObject);
      }

   if (m_aObjects)
      delete m_aObjects;
   if (m_aBuckets)
      GlobalFree ((HGLOBAL)m_aBuckets);

   m_pList->Leave();
}


LPHASHLIST HASHLISTKEY::GetHashList (void)
{
   return m_pList;
}


BOOL HASHLISTKEY::CompareObjectData (PVOID pObject, PVOID pData)
{
   return (*m_fnCompareObjectData)(this, pObject, pData);
}

HASHVALUE HASHLISTKEY::HashObject (PVOID pObject)
{
   return ((*m_fnHashObject)(this, pObject)) % m_cBuckets;
}

HASHVALUE HASHLISTKEY::HashData (PVOID pData)
{
   return ((*m_fnHashData)(this, pData)) % m_cBuckets;
}


LPENUM HASHLISTKEY::FindFirst (PVOID pData)
{
   LPENUM pEnum = NULL;
   m_pList->Enter();

   HASHVALUE hvSearch = HashData (pData);

   size_t iObject;
   if ((iObject = m_aBuckets[ hvSearch ].iFirst) != iINVALID)
      {
      LPHASHLISTENTRY pEntry;
      do {
         if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
            break;
         if (CompareObjectData (pEntry->pObject, pData))
            {
            pEnum = New2 (ENUMERATION,(m_pList, iObject, this, pData));
            break;
            }
         } while ((iObject = pEntry->iNext) != iINVALID);
      }

   m_pList->Leave();
   return pEnum;
}


LPENUM HASHLISTKEY::FindLast (PVOID pData)
{
   LPENUM pEnum = NULL;
   m_pList->Enter();

   HASHVALUE hvSearch = HashData (pData);

   size_t iObject;
   if ((iObject = m_aBuckets[ hvSearch ].iLast) != iINVALID)
      {
      LPHASHLISTENTRY pEntry;
      do {
         if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
            break;
         if (CompareObjectData (pEntry->pObject, pData))
            {
            pEnum = New2 (ENUMERATION,(m_pList, iObject, this, pData));
            break;
            }
         } while ((iObject = pEntry->iPrevious) != iINVALID);
      }

   m_pList->Leave();
   return pEnum;
}


PVOID HASHLISTKEY::GetFirstObject (PVOID pData)
{
   PVOID pObject = NULL;
   m_pList->Enter();

   HASHVALUE hvSearch = HashData (pData);

   size_t iObject;
   if ((iObject = m_aBuckets[ hvSearch ].iFirst) != iINVALID)
      {
      LPHASHLISTENTRY pEntry;
      do {
         if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
            break;
         if (CompareObjectData (pEntry->pObject, pData))
            {
            pObject = pEntry->pObject;
            break;
            }
         } while ((iObject = pEntry->iNext) != iINVALID);
      }

   m_pList->Leave();
   return pObject;
}


PVOID HASHLISTKEY::GetLastObject (PVOID pData)
{
   PVOID pObject = NULL;
   m_pList->Enter();

   HASHVALUE hvSearch = HashData (pData);

   size_t iObject;
   if ((iObject = m_aBuckets[ hvSearch ].iLast) != iINVALID)
      {
      LPHASHLISTENTRY pEntry;
      do {
         if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
            break;
         if (CompareObjectData (pEntry->pObject, pData))
            {
            pObject = pEntry->pObject;
            break;
            }
         } while ((iObject = pEntry->iPrevious) != iINVALID);
      }

   m_pList->Leave();
   return pObject;
}


BOOL HASHLISTKEY::fIsInList (PVOID pEntry)
{
   PVOID pFind;
   if ((pFind = GetFirstObject (pEntry)) == NULL)
      return FALSE;
   return TRUE;
}


void HASHLISTKEY::Add (size_t iObject, PVOID pObject)
{
   m_pList->Enter();

   if ( ((this == m_pList->m_pKeyIndex) && (MIN_TABLE_SIZE_FOR_KEY(m_pList->m_cObjectsMax) > m_cBuckets)) ||
        ((this != m_pList->m_pKeyIndex) && (MIN_TABLE_SIZE(m_pList->m_cObjectsMax) > m_cBuckets)) )
      {
      Resize();
      }
   else
      {
      HASHVALUE hv = HashObject (pObject);
      struct HASHBUCKET *pBucket = &m_aBuckets[ hv ];

      HASHLISTENTRY Entry;
      Entry.pObject = pObject;
      Entry.hv = hv;
      Entry.iNext = iINVALID;
      Entry.iPrevious = pBucket->iLast;
      m_aObjects->SetAt (iObject, &Entry);

      pBucket->iLast = iObject;

      if (Entry.iPrevious != iINVALID)
         {
         LPHASHLISTENTRY pPrevious;
         if ((pPrevious = GetEntry(m_aObjects,Entry.iPrevious)) != NULL)
            pPrevious->iNext = iObject;
         }

      if (pBucket->iFirst == iINVALID)
         pBucket->iFirst = iObject;
      }

   m_pList->Leave();
}


void HASHLISTKEY::Remove (size_t iObject)
{
   m_pList->Enter();

   LPHASHLISTENTRY pEntry;
   if ((pEntry = GetEntry(m_aObjects,iObject)) != NULL)
      {
      pEntry->pObject = NULL;

      if (pEntry->iPrevious != iINVALID)
         {
         LPHASHLISTENTRY pPrevious;
         if ((pPrevious = GetEntry(m_aObjects,pEntry->iPrevious)) != NULL)
            pPrevious->iNext = pEntry->iNext;
         }

      if (pEntry->iNext != iINVALID)
         {
         LPHASHLISTENTRY pNext;
         if ((pNext = GetEntry(m_aObjects,pEntry->iNext)) != NULL)
            pNext->iPrevious = pEntry->iPrevious;
         }

      if (m_aBuckets[ pEntry->hv ].iLast == iObject)
         m_aBuckets[ pEntry->hv ].iLast = pEntry->iPrevious;
      if (m_aBuckets[ pEntry->hv ].iFirst == iObject)
         m_aBuckets[ pEntry->hv ].iFirst = pEntry->iNext;
      }

   m_pList->Leave();
}


void HASHLISTKEY::Resize (void)
{
   if (this == m_pList->m_pKeyIndex)
      {
      REALLOC (m_aBuckets, m_cBuckets, TARGET_TABLE_SIZE_FOR_KEY(m_pList->m_cObjectsMax), 1);
      }
   else
      {
      REALLOC (m_aBuckets, m_cBuckets, TARGET_TABLE_SIZE(m_pList->m_cObjectsMax), 1);
      }

   for (size_t iBucket = 0; iBucket < m_cBuckets; ++iBucket)
      {
      m_aBuckets[ iBucket ].iFirst = iINVALID;
      m_aBuckets[ iBucket ].iLast = iINVALID;
      }

   size_t iObject;
   for (iObject = 0; ; ++iObject)
      {
      LPHASHLISTENTRY pEntry;
      if ((pEntry = GetEntry(m_aObjects,iObject)) == NULL)
         break;
      pEntry->pObject = NULL;
      }

   // Re-add the objects to this key. One caveat: if this is the
   // hashlist's index key, then the format of the items is different--
   // retain that difference.
   //
   if (this == m_pList->m_pKeyIndex)
      {
      for (iObject = 0; ; ++iObject)
         {
         LPHASHLISTENTRY pEntry;
         if ((pEntry = GetEntry(m_pList->m_aObjects,iObject)) == NULL)
            break;
         if (pEntry->pObject != NULL)
            Add (iObject, (PVOID)(iObject+1));
         }
      }
   else // normal, user-defined key
      {
      for (iObject = 0; ; ++iObject)
         {
         LPHASHLISTENTRY pEntry;
         if ((pEntry = GetEntry(m_pList->m_aObjects,iObject)) == NULL)
            break;
         if (pEntry->pObject != NULL)
            Add (iObject, pEntry->pObject);
         }
      }
}


LPHASHLISTKEYDEBUGINFO HASHLISTKEY::GetDebugInfo (void)
{
   m_pList->Enter();

   LPHASHLISTKEYDEBUGINFO pInfo = new HASHLISTKEYDEBUGINFO;
   memset (pInfo, 0x00, sizeof(HASHLISTKEYDEBUGINFO));

   // Find out how full each bucket is.
   //
   REALLOC (pInfo->aBuckets, pInfo->cBuckets, m_cBuckets, 1);

   size_t iBucket;
   for (iBucket = 0; iBucket < pInfo->cBuckets; ++iBucket)
      {
      for (size_t iObject = m_aBuckets[ iBucket ].iFirst;
           iObject != iINVALID;
           iObject = GetEntry(m_aObjects,iObject)->iNext)
         {
         (pInfo->aBuckets[ iBucket ])++;
         }
      }

   // Calculate a "percent effectiveness". This is a pretty fuzzy
   // calculation; we want 100% if all buckets are the same size
   // (plus or minus one element), and 0% if all buckets except one are
   // empty but that one bucket has more than cBuckets elements. In
   // between, we'll try to create a roughly linear gradient. The
   // calculation is effectively the proportion of the number of
   // objects which are evenly distributed to the number of objects
   // overall.
   //
   if (pInfo->cBuckets == 0)
      {
      pInfo->perEffective = 100;
      }
   else
      {
      // We want an accurate count of objects, not the over-allocated
      // count given by m_cObjectsMax.
      //
      size_t cObjects = 0;
      for (iBucket = 0; iBucket < pInfo->cBuckets; ++iBucket)
         cObjects += pInfo->aBuckets[ iBucket ];

      if (cObjects == 0)
         {
         pInfo->perEffective = 100;
         }
      else
         {
         // Determine what "even distribution" means--that's pretty easy.
         // We increase the count by one to indicate that slight unevenness
         // will occur unless the number of objects is a multiple of the
         // number of buckets.
         //
         size_t cPerfectLength = (cObjects / pInfo->cBuckets) + 1;

         size_t cObjectsInPlace = 0;
         for (iBucket = 0; iBucket < pInfo->cBuckets; ++iBucket)
            cObjectsInPlace += min( pInfo->aBuckets[ iBucket ], cPerfectLength );

         // Now calculating that percent effectiveness is easy. If everything
         // is evenly distributed, cObjectsInPlace will == cObjects--and 
         // we want to call it 100%. If eveything is on one chain, then
         // cObjectsInPlace will be really small compared to cObjects.
         //
         pInfo->perEffective = (WORD)(cObjectsInPlace * 100 / cObjects);
         }
      }

   m_pList->Leave();
   return pInfo;
}


void HASHLISTKEY::FreeDebugInfo (LPHASHLISTKEYDEBUGINFO pInfo)
{
   if (pInfo->aBuckets)
      GlobalFree ((HGLOBAL)(pInfo->aBuckets));
}


/*
 * ENUMERATION CLASS _____________________________________________________________
 *
 */

ENUMERATION::ENUMERATION (LPHASHLIST pList, size_t iObject, LPHASHLISTKEY pKey, PVOID pData)
{
   m_pList = pList;
   m_pKey = pKey;
   m_pData = pData;
   m_iObject = iObject;

   PrepareWalk(); // finds m_iPrevious and m_iNext

   m_pList->Enter();
}


ENUMERATION::~ENUMERATION (void)
{
   m_pList->Leave();
}


PVOID ENUMERATION::GetObject (void)
{
   return GetEntry(m_pList->m_aObjects,m_iObject)->pObject;
}


LPENUMERATION ENUMERATION::FindNext (void)
{
   for (;;)
      {
      m_iObject = m_iNext;
      PrepareWalk();

      if (m_iObject == iINVALID)
         break;

      if (m_pKey == NULL)
         return this;

      if (m_pKey->CompareObjectData (GetEntry(m_pList->m_aObjects,m_iObject)->pObject, m_pData))
         return this;
      }

   Delete (this);
   return NULL;
}


LPENUMERATION ENUMERATION::FindPrevious (void)
{
   for (;;)
      {
      m_iObject = m_iPrevious;
      PrepareWalk();

      if (m_iObject == iINVALID)
         break;

      if (m_pKey == NULL)
         return this;

      if (m_pKey->CompareObjectData (GetEntry(m_pList->m_aObjects,m_iObject)->pObject, m_pData))
         return this;
      }

   Delete (this);
   return NULL;
}


void ENUMERATION::PrepareWalk (void)
{
   if (m_iObject == iINVALID)
      {
      m_iPrevious = iINVALID;
      m_iNext = iINVALID;
      }
   else if (m_pKey == NULL)
      {
      m_iPrevious = GetEntry(m_pList->m_aObjects,m_iObject)->iPrevious;
      m_iNext = GetEntry(m_pList->m_aObjects,m_iObject)->iNext;
      }
   else
      {
      m_iPrevious = GetEntry(m_pKey->m_aObjects,m_iObject)->iPrevious;
      m_iNext = GetEntry(m_pKey->m_aObjects,m_iObject)->iNext;
      }
}


/*
 * GENERAL-PURPOSE ____________________________________________________________
 *
 */

HASHVALUE HashString (LPCTSTR pszString)
{
#ifdef UNICODE
   return HashUnicodeString (pszString);
#else
   return HashAnsiString (pszString);
#endif
}

HASHVALUE HashAnsiString (LPCSTR pszStringA)
{
   HASHVALUE hv = 0;

   size_t cch;
   for (cch = lstrlenA(pszStringA); cch >= 4; pszStringA += 4, cch -= 4)
      hv += *(DWORD *)pszStringA;

   for (; cch; pszStringA++, cch--)
      hv += *pszStringA;

   return hv;
}

HASHVALUE HashUnicodeString (LPWSTR pszStringW)
{
   HASHVALUE hv = 0;

   size_t cch;
   for (cch = lstrlenW(pszStringW); cch >= 2; pszStringW += 2, cch -= 2)
      {
      hv += *(DWORD *)pszStringW;   // since HIBYTE(*psz) is usually zero,
      hv = (hv >> 24) | (hv << 8);  // rotate {hv} high-ward by 8 bits
      }

   for (; cch; pszStringW++, cch--)
      hv += *pszStringW;

   return hv;
}

