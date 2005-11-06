/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * HashList - super-quick list manager
 *
 */

#ifndef HASHLIST_H
#define HASHLIST_H

#ifndef EXPORTED
#define EXPORTED
#endif


/*
 * A HASHLIST effectively implements an in-memory database: it maintains
 * the objects you specify in a managed list, placing hashtables across
 * whatever key fields you select. The practical upshot is that it's a
 * thread-safe, convenient resizable-array manager that provides fast
 * searching features.
 *
 * There are many tidbits about using HashLists mentioned in the examples
 * below--some are pretty important, so read through them carefully.
 *
 * EXAMPLES ___________________________________________________________________
 *
 * // Some boring example dataset--three OBJECT structures
 *
 * typedef struct OBJECT {
 *    DWORD dwKey;
 *    TCHAR szText[ 256 ];
 * } OBJECT;
 *
 * static OBJECT aObjects[] = {
 *    { 275, "First Object" },
 *    { 473, "Second Object" },
 *    {  32, "Third Object" },
 * };
 *
 * // When adding data to the list, note that it's not necessary to create
 * // keys at any particular time (or at all). Objects already in the list
 * // when you add a new key will automatically get indexed, just as will any
 * // new objects that you add later. Remember that you can't track NULL
 * // objects in the list--hl.Add(0) is illegal. An object can be added in
 * // the list any number of times.
 *
 * HASHLIST hl;
 * hl.Add (&aObjects[0]);
 * hl.CreateKey ("MyKey", MyKey_Compare, MyKey_HashObject, MyKey_HashData);
 * hl.Add (&aObjects[1]);
 * hl.Add (&aObjects[2]);
 *
 * // Walk the hashlist. There are a few things worthy of note here
 * // besides the enumeration technique itself: calling FindFirst() returns
 * // a pointer to an *allocated* ENUMERATION structure. The structure
 * // is automatically freed whenever a FindNext() or FindPrevious() is
 * // about to return NULL--so loops like the one below don't have to
 * // do anything to free the ENUMERATION object. Since a hashlist holds
 * // a critical section while any ENUMERATION object exists for it,
 * // it's important to remember that if you do not walk the list to its end
 * // you must explicitly free the enumeration object yourself.
 *
 * for (LPENUM pEnum = hl.FindFirst(); pEnum; pEnum = pEnum->FindNext())
 * {
 *    OBJECT *pObj = (OBJECT*)(pEnum->GetObject());
 *    printf ("%s", pObj->szText);
 * }
 *
 * // Find the first object with 473 as its dwKey element. Note that since
 * // we're not walking the whole list, we have to free the ENUMERATION object
 * // when we're done.
 *
 * HASHLISTKEY pKey = hl.FindKey ("MyKey");
 * DWORD dwKeyFind = 473;
 *
 * LPENUM pEnum;
 * if ((pEnum = pKey->FindFirst (&dwKeyFind)) != NULL)
 * {
 *    OBJECT *pObj = (OBJECT*)(pEnum->GetObject());
 *    printf ("%s", pObj->szText);
 *    delete pEnum;
 * }
 *
 * // A shorter way to get just the first object with 473 as a dwKey element.
 * // Since it doesn't return an ENUMERATION object, you don't have to free
 * // anything.
 *
 * OBJECT *pObj = (OBJECT*)pKey->GetFirstObject (&dwKeyFind);
 *
 * // Find all objects with 473 as a dwKey element. Since we're walking the
 * // whole list, the ENUMERATION object will be deleted automatically.
 *
 * for (LPENUM pEnum = pKey->FindFirst (&dwKeyFind); pEnum; pEnum = pEnum->FindNext()) 
 * {
 *    OBJECT *pObj = (OBJECT*)(pEnum->GetObject());
 *    printf ("%s", pObj->szText);
 * }
 *
 * // HashLists also provide fast (i.e., effectively constant-time) testing
 * // to see if an object is already in the list; that allows use of the
 * // AddUnique() item, which ensures duplicates won't occur.
 *
 * HASHLIST hl;
 * hl.AddUnique (&aObjects[0]);
 * hl.AddUnique (&aObjects[0]);  // succeeds but doesn't add item
 * hl.Remove (&aObjects[0]);     // list is now empty
 *
 * HASHLIST hl;
 * HASHLISTKEY pKey = hl.CreateKey ("IntKey", IntKey_Compare, IntKey_HashObject, IntKey_HashData);
 * int a = 153;
 * int b = 287;
 * int c = 153;
 * hl.AddUnique (&a, pKey);
 * hl.AddUnique (&b, pKey);
 * hl.AddUnique (&c, pKey);
 * // The list now contains two items: {&a} and {&b}.
 * // Because {&c} was not unique over key {pKey}, it was not added.
 *
 * // Remove the first object with a dwKey of 473 from the list. Since we're
 * // starting an enumeration but not walking it to its end, we'll have
 * // to free the ENUMERATION object explicitly.
 *
 * LPENUM pEnum;
 * if ((pEnum = pKey->FindFirst (&dwKeyFind)) != NULL) 
 * {
 *    hl.Remove (pEnum->GetObject());
 *    delete pEnum;
 * }
 *
 * // Remove all objects in the list--technique one. This technique emphasises
 * // an useful bit of data: if you remove the object that your ENUMERATION
 * // object represents, you can still use that enumeration object to continue
 * // the enumeration! For the fun of it, we'll show erasing the list both
 * // forwards and backwards.
 *
 * for (LPENUM pEnum = hl.FindFirst(); pEnum; pEnum = pEnum->FindNext()) {
 *    hl.Remove (pEnum->GetObject());
 * }
 *
 * for (LPENUM pEnum = hl.FindLast(); pEnum; pEnum = pEnum->FindPrevious()) {
 *    hl.Remove (pEnum->GetObject());
 * }
 *
 * // Remove all objects--technique two. This technique is a little messier:
 * // it finds the first object in the list, removes it, and re-starts the
 * // enumeration. Since an enumeration is started but not walked to its end,
 * // the ENUMERATION object must be freed explicitly in each iteration.
 *
 * LPENUM pEnum;
 * while ((pEnum = hl.FindFirst()) != NULL) {
 *    hl.Remove (pEnum->GetObject());
 *    delete pEnum;
 * }
 *
 * // Remove all objects--technique three. This is essentially the same
 * // as technique two (find the first object, remove it, repeat), except
 * // that it avoids using an ENUMERATION object so there's nothing to free.
 *
 * OBJECT *pObject;
 * while ((pObject = hl.GetFirstObject()) != NULL)
 *    hl.Remove (pObject);
 *
 * // If you change something in an object that you think would affect one or
 * // more of the list's keys, you should call tell the HashList so it
 * // can update itself. An important note: if you're enumerating all items,
 * // you can call Update() for any item without affecting the enumeration;
 * // however, if you're enumerating along a key when you call Update(),
 * // you'll need to stop the enumeration.
 *
 * aObjects[2].dwKey = 182;
 * hl.Update (&aObjects[2]); // (same as hl.Remove(...) + hl.Add(...))
 *
 * // It's important to remember that the HashList only knows about your
 * // objects as generic pointers--it doesn't free them if you remove
 * // them from the list.
 *
 * OBJECT *pObject = new OBJECT;
 * hl.Add (pObject);
 * hl.Remove (pObject);
 * delete pObject;
 *
 * // Another point about freeing objects that you have in the list:
 * // it's safe to delete the object even before you remove it from the
 * // list, but if you do so, make sure you remove the object immediately.
 * // Also, to be thread-safe, you should surround the section with
 * // Enter()/Leave(); otherwise, another thread may wake up and try to
 * // work with the list between when you free the object and when you
 * // remove it from the list.
 *
 * OBJECT *pObject = new OBJECT;
 * hl.Add (pObject);
 * hl.Enter();
 * delete pObject;
 * hl.Remove (pObject);
 * hl.Leave();
 *
 * // Each key requires that you supply three callback functions:
 * // one to extract the key from an Object and hash it, one to
 * // just hash a key directly, and one to check an object to see
 * // if it matches the specified key exactly. A HASHVALUE is really
 * // just a DWORD--don't worry about the range (since the hashlist
 * // will automatically modulo the return by the size of its table).
 *
 * HASHVALUE CALLBACK MyKey_HashObject (LPHASHLISTKEY pKey, PVOID pObject) {
 *    return MyKey_HashData (pKey, &((OBJECT*)pObject)->dwKey);
 * }
 *
 * HASHVALUE CALLBACK MyKey_HashData (LPHASHLISTKEY pKey, PVOID pData) {
 *    return (HASHVALUE)*(DWORD*)pData;
 * }
 *
 * BOOL CALLBACK MyKey_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData) {
 *    return (((OBJECT*)pObject)->dwKey == *(DWORD*)pData;
 * }
 *
 */

#define cTABLESIZE_DEFAULT 1000

typedef class EXPORTED EXPANDARRAY EXPANDARRAY, *LPEXPANDARRAY;
typedef class EXPORTED HASHLIST HASHLIST, *LPHASHLIST;
typedef class EXPORTED HASHLISTKEY HASHLISTKEY, *LPHASHLISTKEY;
typedef class EXPORTED ENUMERATION ENUMERATION, ENUM, *LPENUMERATION, *LPENUM;

typedef UINT_PTR HASHVALUE;
typedef BOOL (CALLBACK * LPHASHFUNC_COMPAREOBJECTDATA)(LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
typedef HASHVALUE (CALLBACK * LPHASHFUNC_HASHOBJECT)(LPHASHLISTKEY pKey, PVOID pObject);
typedef HASHVALUE (CALLBACK * LPHASHFUNC_HASHDATA)(LPHASHLISTKEY pKey, PVOID pData);

typedef struct HASHLISTENTRY
   {
   PVOID pObject;
   HASHVALUE hv;
   size_t iPrevious;
   size_t iNext;
   } HASHLISTENTRY, *LPHASHLISTENTRY;

typedef struct HASHLISTKEYDEBUGINFO
   {
   size_t  cBuckets;  // number of buckets in the hashtable currently
   size_t *aBuckets;  // number of items in each bucket in the hashtable
   WORD perEffective; // percent effectiveness of hashtable (100%=even distrib)
   } HASHLISTKEYDEBUGINFO, *LPHASHLISTKEYDEBUGINFO;


/*
 * EXPANDARRAY CLASS __________________________________________________________
 *
 */

typedef struct
   {
   PVOID aElements; // = EXPANDARRAYHEAP.aElements + 4;
   // Followed by allocated space for aElements
   } EXPANDARRAYHEAP, *LPEXPANDARRAYHEAP;

class EXPORTED EXPANDARRAY
   {
   public:
      EXPANDARRAY (size_t cbElement, size_t cElementsPerHeap = 0);
      ~EXPANDARRAY (void);

      PVOID GetAt (size_t iElement);
      PVOID SetAt (size_t iElement, PVOID pData = NULL);

   private:
      size_t m_cbElement;
      size_t m_cElementsPerHeap;

      size_t m_cHeaps;
      LPEXPANDARRAYHEAP *m_aHeaps;
   };


/*
 * HASHLIST CLASS _____________________________________________________________
 *
 */

class EXPORTED HASHLIST
   {
   friend class HASHLISTKEY;
   friend class ENUMERATION;

   public:
      HASHLIST (void);
      ~HASHLIST (void);

      // A HashList is basically just a resizable array of objects.
      // As such, the class exposes methods for adding objects to,
      // and removing objects from, the list.
      //
      BOOL Add (PVOID pObjectToAdd);
      void Remove (PVOID pObjectToRemove);
      BOOL Update (PVOID pObjectToUpdate);  // like Remove() followed by Add()

      // Calling AddUnique() instead of Add() to add an item ensures
      // that the specified item will only be added if it does not
      // already exist in the list. If a key is also specified during the
      // AddUnique() call, then the test will be performed over a specific
      // key--detecting duplicate items by checking the key's indexed fields,
      // rather than just comparing item's addresses.
      //
      BOOL AddUnique (PVOID pObjectToAdd, LPHASHLISTKEY pKeyUnique = NULL);
      BOOL fIsInList (PVOID pObject);

      // Each HashList can use one or more "keys" for looking up items;
      // each key represents with an internally-maintained hash table
      // and corresponding hashing functions (which you supply when you
      // create a key). Keys can be added or removed at any time.
      //
      LPHASHLISTKEY CreateKey (LPCTSTR pszKeyName,
                               LPHASHFUNC_COMPAREOBJECTDATA fnCompareObjectData,
                               LPHASHFUNC_HASHOBJECT fnHashObject,
                               LPHASHFUNC_HASHDATA fnHashData,
                               size_t cInitialTableSize = cTABLESIZE_DEFAULT);
      LPHASHLISTKEY FindKey (LPCTSTR pszKeyName);
      void RemoveKey (LPHASHLISTKEY pKey);

      // A list isn't much good without methods for enumeration.
      // A HashList provides enumeration in the form of a list
      // rather than an array (to prevent problems with holes when
      // items are removed). You can walk the list in either direction.
      // An important note: a HashList does not guarantee the order
      // of enumeration will match the order of insertion (and, in fact,
      // it probably won't).
      //
      // Alternately, you can initiate the walk from a HASHLISTKEY;
      // doing so allows you to quickly enumerate only items which match a
      // particular value on that key, improving search performance.
      //
      LPENUM FindFirst (void);
      LPENUM FindLast (void);
      PVOID GetFirstObject (void);
      PVOID GetLastObject (void);

      // It's also possible to quickly determine the number of objects
      // in the list, without enumerating them all.
      //
      size_t GetCount (void);

      // All HASHLIST routines are thread-safe, but you can wrap compound
      // operations using the same critical section as the HASHLIST uses
      // internally. You can also specify an alternate critical section
      // for the HASHLIST to use instead.
      //
      void Enter (void);
      void Leave (void);
      void SetCriticalSection (CRITICAL_SECTION *pcs); // NULL = use internal

   private:
      size_t m_iFirst;
      size_t m_iLast;
      size_t m_iNextFree;

      LPEXPANDARRAY m_aObjects;
      size_t m_cObjects;
      size_t m_cObjectsMax;

      LPHASHLISTKEY *m_apKeys;
      size_t m_cpKeys;

      LPHASHLISTKEY m_pKeyIndex;
      static BOOL CALLBACK KeyIndex_CompareObjectData (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK KeyIndex_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK KeyIndex_HashData (LPHASHLISTKEY pKey, PVOID pData);

      CRITICAL_SECTION m_cs;
      CRITICAL_SECTION *m_pcs;
   };


/*
 * HASHLISTKEY CLASS _____________________________________________________________
 *
 */

class EXPORTED HASHLISTKEY
   {
   friend class HASHLIST;
   friend class ENUMERATION;

   public:

      // Since every HASHLISTKEY is a member of exactly one HASHLIST,
      // it contains a pointer back to that HASHLIST. You can get that
      // pointer, to determine which list the key is used in.
      //
      LPHASHLIST GetHashList (void);

      // Each HASHLISTKEY contains an internal (hidden) hashtable,
      // and pointers to the functions which you supplied to hash objects
      // for inclusion in that table. For convenience, it exposes methods
      // for calling those functions.
      //
      BOOL CompareObjectData (PVOID pObject, PVOID pData);
      HASHVALUE HashObject (PVOID pObject);
      HASHVALUE HashData (PVOID pData);

      // Instead of initiating an enumeration through the HASHLIST, you may
      // initiate an enumeration through a HASHLISTKEY. Doing so indicates
      // that you only want to enumerate items whose keys match a particular
      // value; the HASHLISTKEY's internal hashtable is employed to speed the
      // search up.
      //
      LPENUM FindFirst (PVOID pData);
      LPENUM FindLast (PVOID pData);
      PVOID GetFirstObject (PVOID pData);
      PVOID GetLastObject (PVOID pData);

      BOOL fIsInList (PVOID pObject);  // Equivalent to (!!GetFirstObject())

      // For debugging purposes, when developing your hashing algorithms
      // you may want to use the following routines to examine the distribution
      // of data within the key. They enable you to see how many items are
      // on each bucket within the hash table; other statistics are available.
      //
      LPHASHLISTKEYDEBUGINFO GetDebugInfo (void);
      void FreeDebugInfo (LPHASHLISTKEYDEBUGINFO pInfo);

   private:
      HASHLISTKEY (LPHASHLIST pList, LPCTSTR pszKeyName, LPHASHFUNC_COMPAREOBJECTDATA fnCompareObjectData, LPHASHFUNC_HASHOBJECT fnHashObject, LPHASHFUNC_HASHDATA fnHashData, size_t cTableSize);
      ~HASHLISTKEY (void);

      void Add (size_t iObject, PVOID pObject);
      void Remove (size_t iObject);

      void Resize (void);

   private:
      LPEXPANDARRAY m_aObjects;

      struct HASHBUCKET {
         size_t iFirst;
         size_t iLast;
      } *m_aBuckets;
      size_t m_cBuckets;

      LPHASHLIST m_pList;
      TCHAR m_szKeyName[ 256 ];
      LPHASHFUNC_COMPAREOBJECTDATA m_fnCompareObjectData;
      LPHASHFUNC_HASHOBJECT m_fnHashObject;
      LPHASHFUNC_HASHDATA m_fnHashData;
   };


/*
 * ENUMERATION CLASS _____________________________________________________________
 *
 */

class EXPORTED ENUMERATION
   {
   friend class HASHLIST;
   friend class HASHLISTKEY;

   public:

      // Each ENUMERATION object represents an object in the HASHLIST.
      // You can find the HASHLIST object that the ENUMERATION represents.
      //
      PVOID GetObject (void);

      // Additionally, an ENUMERATION object allows you to traverse
      // (forwards or backwards) the list of objects which meet your search
      // criteria. If you started the enumeration through a HASHLIST
      // object then all objects in the list will be enumerated; if you
      // started the enumeration through a HASHLISTKEY object then only
      // those objects which have the specified value for that key will be
      // enumerated. If either of the routines below is about to return NULL,
      // it will automatically 'delete this' before doing so--ending the
      // enumeration.
      //
      LPENUMERATION FindNext (void);
      LPENUMERATION FindPrevious (void);

      // You obtain an ENUMERATION object by calling a FindFirst() method,
      // either through the HASHLIST object or one of its keys. As mentioned
      // in the examples above, the ENUMERATION object is automatically
      // freed whenever FindNext() or FindPrevious() returns NULL--however,
      // if you stop the enumeration before then, you'll have to explicitly
      // free the object with 'delete'. Failure to do so means (1) a memory
      // leak, and more seriously, (2) the active thread will hold a critical
      // section for the hashlist forever.
      //
      ~ENUMERATION (void);

   private:
      ENUMERATION (LPHASHLIST pList, size_t iObject, LPHASHLISTKEY pKey = NULL, PVOID pData = NULL);

      void ENUMERATION::PrepareWalk (void);

      size_t m_iObject;
      size_t m_iNext;
      size_t m_iPrevious;
      LPHASHLIST m_pList;
      LPHASHLISTKEY m_pKey;
      PVOID m_pData;
   };


/*
 * GENERAL-PURPOSE ____________________________________________________________
 *
 */

EXPORTED HASHVALUE HashString (LPCTSTR pszString);
EXPORTED HASHVALUE HashAnsiString (LPCSTR pszStringA);
EXPORTED HASHVALUE HashUnicodeString (LPWSTR pszStringW);


#endif // HASHLIST_H

