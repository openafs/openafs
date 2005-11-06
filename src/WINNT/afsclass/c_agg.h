/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_AGGREGATE_H
#define AFSCLASS_AGGREGATE_H


/*
 * AGGREGATE CLASS ____________________________________________________________
 *
 */

typedef struct
   {
   ULONG dwID;
   size_t ckStorageTotal;
   size_t ckStorageFree;
   size_t ckStorageAllocated;
   } AGGREGATESTATUS, *LPAGGREGATESTATUS;

class AGGREGATE
   {
   friend class CELL;
   friend class SERVER;
   friend class FILESET;
   friend class IDENT;

   public:
      void Close (void);
      void Invalidate (void);
      void InvalidateAllocation (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated
      BOOL RefreshFilesets (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated

      // Aggregate properties
      //
      LPIDENT GetIdentifier (void);
      LPCELL OpenCell (ULONG *pStatus = NULL);
      LPSERVER OpenServer (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszName);
      void GetDevice (LPTSTR pszDevice);
      int GetID (void);

      BOOL GetStatus (LPAGGREGATESTATUS lpas, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      short GetGhostStatus (void);	// returns GHOST_*

      PVOID GetUserParam (void);
      void  SetUserParam (PVOID pUserParam);

      // Filesets within an aggregate
      //
      LPFILESET OpenFileset (LPTSTR pszName, ULONG *pStatus = NULL);
      LPFILESET OpenFileset (VOLUMEID *pvidFileset, ULONG *pStatus = NULL);
      LPFILESET FilesetFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPFILESET FilesetFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPFILESET FilesetFindNext (HENUM *phEnum);
      void FilesetFindClose (HENUM *phEnum);

   private:
      AGGREGATE (LPSERVER lpServerParent, LPTSTR pszName, LPTSTR pszDevice);
      ~AGGREGATE (void);
      void SendDeleteNotifications (void);

      size_t CalculateAllocation (BOOL fNotify);

      static BOOL CALLBACK AGGREGATE::KeyFilesetName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK AGGREGATE::KeyFilesetName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK AGGREGATE::KeyFilesetName_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK AGGREGATE::KeyFilesetID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK AGGREGATE::KeyFilesetID_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK AGGREGATE::KeyFilesetID_HashData (LPHASHLISTKEY pKey, PVOID pData);

   // Private data
   //
   private:
      LPIDENT m_lpiCell;
      LPIDENT m_lpiServer;
      LPIDENT m_lpiThis;
      TCHAR m_szName[ cchNAME ];
      TCHAR m_szDevice[ cchNAME ];
      short m_wGhost;
      int m_idPartition;

      BOOL m_fFilesetsOutOfDate;
      LPHASHLIST m_lFilesets;
      LPHASHLISTKEY m_lkFilesetName;
      LPHASHLISTKEY m_lkFilesetID;

      BOOL m_fStatusOutOfDate;
      BOOL m_fAllocationOutOfDate;
      AGGREGATESTATUS m_as;
   };


#endif // AFSCLASS_AGGREGATE_H

