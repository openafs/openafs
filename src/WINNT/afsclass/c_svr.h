/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_SERVER_H
#define AFSCLASS_SERVER_H

#include <WINNT/afsclass.h>


/*
 * SERVER CLASS _______________________________________________________________
 *
 */

#define AFSCLASS_MAX_ADDRESSES_PER_SITE 16

typedef struct
   {
   size_t nAddresses;
   SOCKADDR_IN aAddresses[ AFSCLASS_MAX_ADDRESSES_PER_SITE ];
   } SERVERSTATUS, *LPSERVERSTATUS;

class SERVER
   {
   friend class CELL;
   friend class SERVICE;
   friend class AGGREGATE;
   friend class IDENT;

   public:
      void Close (void);
      void Invalidate (void);
      void InvalidateStatus (void);
      void InvalidateServices (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshServices (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshAggregates (BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      BOOL RefreshAll (ULONG *pStatus = NULL, double dInit = 0.0, double dFactor = 1.0);

      static void ShortenName (LPTSTR pszTarget, LPTSTR pszSource, BOOL fForce = FALSE);

      // Server properties
      //
      BOOL fIsMonitored (void);
      BOOL SetMonitor (BOOL fShouldMonitor, ULONG *pStatus = NULL);

      LPIDENT GetIdentifier (void);
      LPCELL OpenCell (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszName);
      void GetLongName (LPTSTR pszName);

      short GetGhostStatus (void);	// returns GHOST_*
      BOOL GetStatus (LPSERVERSTATUS lpss, BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

      PVOID OpenBosObject (PVOID *phCell = NULL, ULONG *pStatus = NULL);
      BOOL CloseBosObject (ULONG *pStatus = NULL);

      PVOID OpenVosObject (PVOID *phCell = NULL, ULONG *pStatus = NULL);
      BOOL CloseVosObject (ULONG *pStatus = NULL);

      // Aggregates within a server
      //
      LPAGGREGATE OpenAggregate (LPTSTR pszName, ULONG *pStatus = NULL);
      LPAGGREGATE OpenAggregate (ULONG dwID, ULONG *pStatus = NULL);
      LPAGGREGATE AggregateFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPAGGREGATE AggregateFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPAGGREGATE AggregateFindNext (HENUM *phEnum);
      void AggregateFindClose (HENUM *phEnum);

      // Services within a server
      //
      LPSERVICE OpenService (LPTSTR pszName, ULONG *pStatus = NULL);
      LPSERVICE ServiceFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPSERVICE ServiceFindFirst (HENUM *phEnum, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      LPSERVICE ServiceFindNext (HENUM *phEnum);
      void ServiceFindClose (HENUM *phEnum);

   private:
      SERVER (LPCELL lpCellParent, LPTSTR pszName);
      ~SERVER (void);
      void FreeAll (void);
      void FreeAggregates (void);
      void FreeServices (void);
      void SendDeleteNotifications (void);

      BOOL CanTalkToServer (ULONG *pStatus = NULL);
      static DWORD WINAPI CanTalkToServer_ThreadProc (PVOID lp);

      static BOOL CALLBACK KeyAggregateName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK KeyAggregateName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK KeyAggregateName_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK KeyAggregateID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK KeyAggregateID_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK KeyAggregateID_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK KeyServiceName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK KeyServiceName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK KeyServiceName_HashData (LPHASHLISTKEY pKey, PVOID pData);

   private:
      LPIDENT m_lpiCell;
      TCHAR   m_szName[ cchNAME ];

      LPCELL  m_lpCellBOS;
      PVOID   m_hCellBOS;
      PVOID   m_hBOS;
      size_t  m_cReqBOS;

      PVOID   m_hCellVOS;
      PVOID   m_hVOS;
      size_t  m_cReqVOS;

      short   m_wGhost;
      LPIDENT m_lpiThis;
      BOOL    m_fMonitor;
      ULONG   m_lastStatus;

      BOOL m_fCanGetAggregates;
      BOOL m_fAggregatesOutOfDate;
      LPHASHLIST m_lAggregates;
      LPHASHLISTKEY m_lkAggregateName;
      LPHASHLISTKEY m_lkAggregateID;

      BOOL m_fCanGetServices;
      BOOL m_fServicesOutOfDate;
      LPHASHLIST m_lServices;
      LPHASHLISTKEY m_lkServiceName;

      BOOL m_fVLDBOutOfDate;
      BOOL m_fStatusOutOfDate;
      SERVERSTATUS m_ss;

      BOOL m_fDelete;
   };


#endif  // AFSCLASS_SERVER_H

