/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_CELL_H
#define AFSCLASS_CELL_H

#include <WINNT/afsclass.h>
#include <WINNT/c_svr.h>
#include <WINNT/c_agg.h>
#include <WINNT/c_usr.h>
#include <WINNT/c_grp.h>


typedef enum
   {
   CELL_REFRESH_ACCOUNT_CREATED_USER,
   CELL_REFRESH_ACCOUNT_CREATED_GROUP,
   CELL_REFRESH_ACCOUNT_CHANGED,
   CELL_REFRESH_ACCOUNT_DELETED,
   } OP_CELL_REFRESH_ACCOUNT;

/*
 * CELL CLASS _________________________________________________________________
 *
 */

class CELL
   {
   friend class IDENT;
   friend class SERVER;
   friend class SERVICE;
   friend class AGGREGATE;
   friend class FILESET;
   friend class USER;
   friend class PTSGROUP;

   public:
      static LPIDENT OpenCell (LPTSTR pszCellName, PVOID hCreds, ULONG *pStatus = NULL);
      static void CloseCell (LPIDENT lpiCell);
      static LPCELL ReopenCell (LPTSTR szCell, ULONG *pStatus = NULL);

   public:
      void Close (void);
      void Invalidate (void);
      void InvalidateServers (void);
      void InvalidateUsers (void);
      BOOL RefreshServerList (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshServers (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated
      BOOL RefreshVLDB (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshVLDB (LPIDENT lpiRefresh, BOOL fNotify = TRUE, ULONG *pStatus = NULL, BOOL fAnythingRelatedToThisRWFileset = FALSE);
      BOOL RefreshUsers (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshAll (ULONG *pStatus = NULL);
      BOOL RefreshAccount (LPTSTR pszAccount, LPTSTR pszInstance, OP_CELL_REFRESH_ACCOUNT Op, LPIDENT *plpi = NULL);
      BOOL RefreshAccounts (LPTSTR mszAccounts, OP_CELL_REFRESH_ACCOUNT Op);

      // Cell properties
      //
      LPIDENT GetIdentifier (void);
      void GetName (LPTSTR pszName);
      static BOOL GetDefaultCell (LPTSTR pszName, ULONG *pStatus = NULL);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

      BOOL fAnyServersUnmonitored (void);

      PVOID GetCellObject (ULONG *pStatus = NULL);
      PVOID GetKasObject (ULONG *pStatus = NULL);

      PVOID GetCurrentCredentials (void);
      void SetCurrentCredentials (PVOID hCreds);

      // Servers within a cell
      //
      LPSERVER OpenServer (LPTSTR pszName, ULONG *pStatus = NULL);
      LPSERVER OpenServer (LPSOCKADDR_IN pAddr, ULONG *pStatus = NULL);
      LPSERVER ServerFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPSERVER ServerFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPSERVER ServerFindNext (HENUM *phEnum);
      void ServerFindClose (HENUM *phEnum);

      // Users and Groups within a cell
      //
      LPUSER OpenUser (LPTSTR pszName, LPTSTR pszInstance = NULL, ULONG *pStatus = NULL);
      LPUSER UserFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPUSER UserFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPUSER UserFindNext (HENUM *phEnum);
      void UserFindClose (HENUM *phEnum);

      LPPTSGROUP OpenGroup (LPTSTR pszName, ULONG *pStatus = NULL);
      LPPTSGROUP GroupFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPPTSGROUP GroupFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPPTSGROUP GroupFindNext (HENUM *phEnum);
      void GroupFindClose (HENUM *phEnum);

   private:
      static void InitClass (void);
      CELL (LPTSTR pszCellName, PVOID hCreds);
      ~CELL (void);
      void FreeUsers (BOOL fNotify);
      void FreeServers (BOOL fNotify);

      BOOL CloseCellObject (ULONG *pStatus = NULL);
      BOOL CloseKasObject (ULONG *pStatus = NULL);

      void RefreshVLDB_RemoveReferences (LPIDENT lpiRefServer, LPIDENT lpiRefAggregate, LPIDENT lpiRefFileset, LPVOLUMEID pvidRefFileset);
      void RefreshVLDB_OneEntry (PVOID pEntry, LPIDENT lpiRefServer, LPSERVERSTATUS pssRefServer, LPIDENT lpiRefAggregate, LPAGGREGATESTATUS pasRefAggregate, LPIDENT lpiRefFileset, LPVOLUMEID pvidRefFileset, BOOL fNotify);
      void BuildGroups (LPTSTR mszGroups);

      static BOOL CALLBACK CELL::KeyServerName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK CELL::KeyServerName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK CELL::KeyServerName_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK CELL::KeyServerAddr_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK CELL::KeyServerAddr_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK CELL::KeyServerAddr_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK CELL::KeyUserName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK CELL::KeyUserName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK CELL::KeyUserName_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK CELL::KeyGroupName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK CELL::KeyGroupName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK CELL::KeyGroupName_HashData (LPHASHLISTKEY pKey, PVOID pData);

   private:
      PVOID m_hCell;
      PVOID m_hKas;
      TCHAR m_szName[ cchNAME ];
      size_t m_nReqs;
      PVOID m_hCreds;
      char **m_apszServers;

      BOOL m_fStatusOutOfDate;
      BOOL m_fVLDBOutOfDate;
      LPIDENT m_lpiThis;

      static LPHASHLIST x_lCells;

      BOOL m_fServersOutOfDate;
      size_t m_nServersUnmonitored;
      LPHASHLIST m_lServers;
      LPHASHLISTKEY m_lkServerName;
      LPHASHLISTKEY m_lkServerAddr;

      BOOL m_fUsersOutOfDate;
      LPHASHLIST m_lUsers;
      LPHASHLISTKEY m_lkUserName;
      LPHASHLIST m_lGroups;
      LPHASHLISTKEY m_lkGroupName;
   };


#endif  // AFSCLASS_CELL_H

