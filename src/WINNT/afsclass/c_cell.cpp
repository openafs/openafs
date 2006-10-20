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

#include <WINNT/afsclass.h>
#include "internal.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

         // Each CELL object maintains a list of servers; that list has
         // hashtables placed across the (shortened) server name for
         // faster lookup; it also maintains a hashtable across the
         // servers' primary IP address (the first in the list; most
         // servers will only have one anyway). The default table size
         // in a HASHLIST is 1000 elements--that's too large for a list
         // of servers-in-a-cell, as it's enough to handle up to 30,000
         // servers before the table would need to resize iteself (see
         // the documentation in hashlist.cpp for info). Instead, we
         // choose a more reasonable default table size.
         //
#define cKEYSERVERNAME_TABLESIZE     50

#define cKEYSERVERADDR_TABLESIZE     50

         // Enable the definition below to do a better job of finding
         // user entries in PTS which have no KAS entries (for instance,
         // machine IP accounts).
         //
#define FIND_PTS_DEBRIS


/*
 * VARIABLES __________________________________________________________________
 *
 */

LPHASHLIST CELL::x_lCells = NULL;


/*
 * CONSTRUCTION _______________________________________________________________
 *
 */

void CELL::InitClass (void)
{
   if (x_lCells == NULL)
      {
      x_lCells = New (HASHLIST);
      x_lCells->SetCriticalSection (AfsClass_GetCriticalSection());
      }
}


CELL::CELL (LPTSTR pszCellName, PVOID hCreds)
{
   AfsClass_Enter();
   InitClass();

   m_hCell = 0;
   m_hKas = 0;
   lstrcpy (m_szName, pszCellName);
   m_nReqs = 0;
   m_hCreds = hCreds;
   m_apszServers = 0;

   m_fStatusOutOfDate = TRUE;
   m_fVLDBOutOfDate = TRUE;
   m_lpiThis = NULL;

   m_fServersOutOfDate = TRUE;
   m_nServersUnmonitored = 0;

   m_lServers = New (HASHLIST);
   m_lServers->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkServerName = m_lServers->CreateKey ("Server Name", CELL::KeyServerName_Compare, CELL::KeyServerName_HashObject, CELL::KeyServerName_HashData, cKEYSERVERNAME_TABLESIZE);
   m_lkServerAddr = m_lServers->CreateKey ("Server Primary Address", CELL::KeyServerAddr_Compare, CELL::KeyServerAddr_HashObject, CELL::KeyServerAddr_HashData, cKEYSERVERADDR_TABLESIZE);

   m_fUsersOutOfDate = TRUE;
   m_lUsers = New (HASHLIST);
   m_lUsers->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkUserName = m_lUsers->CreateKey ("User Name", CELL::KeyUserName_Compare, CELL::KeyUserName_HashObject, CELL::KeyUserName_HashData);
   m_lGroups = New (HASHLIST);
   m_lGroups->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkGroupName = m_lGroups->CreateKey ("Group Name", CELL::KeyGroupName_Compare, CELL::KeyGroupName_HashObject, CELL::KeyGroupName_HashData);

   AfsClass_Leave();
}


CELL::~CELL (void)
{
   FreeUsers (FALSE);
   FreeServers (FALSE);

   if (m_lpiThis)
      m_lpiThis->m_cRef --;
   Delete (m_lServers);
}


void CELL::FreeServers (BOOL fNotify)
{
   for (LPENUM pEnum = m_lServers->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPSERVER lpServer = (LPSERVER)(pEnum->GetObject());
      if (fNotify)
         lpServer->SendDeleteNotifications();
      m_lServers->Remove (lpServer);
      Delete (lpServer);
      }
   if (m_apszServers)
      {
      for (size_t ii = 0; m_apszServers[ii]; ++ii)
         FreeString (m_apszServers[ii]);
      Free (m_apszServers);
      m_apszServers = NULL;
      }
}


void CELL::FreeUsers (BOOL fNotify)
{
   LPENUM pEnum;
   for (pEnum = m_lGroups->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPPTSGROUP lpGroup = (LPPTSGROUP)(pEnum->GetObject());
      if (fNotify)
         lpGroup->SendDeleteNotifications();
      m_lGroups->Remove (lpGroup);
      Delete (lpGroup);
      }

   for (pEnum = m_lUsers->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPUSER lpUser = (LPUSER)(pEnum->GetObject());
      if (fNotify)
         lpUser->SendDeleteNotifications();
      m_lUsers->Remove (lpUser);
      Delete (lpUser);
      }
}


/*
 * CELL-LIST MANAGEMENT _______________________________________________________
 *
 */

void CELL::Close (void)
{
   AfsClass_Leave();
}


BOOL CELL::GetDefaultCell (LPTSTR pszName, ULONG *pStatus)
{
   WORKERPACKET wp;
   wp.wpClientLocalCellGet.pszCell = pszName;
   return Worker_DoTask (wtaskClientLocalCellGet, &wp, pStatus);
}


LPIDENT CELL::OpenCell (LPTSTR pszCell, PVOID hCreds, ULONG *pStatus)
{
   LPIDENT lpiCell = NULL;
   AfsClass_Enter();
   InitClass();

   LPCELL lpCell;
   if ((lpCell = ReopenCell (pszCell, pStatus)) != NULL)
      {
      lpiCell = lpCell->GetIdentifier();
      lpCell->m_nReqs++;
      lpCell->Close();
      }
   else // cell hasn't been opened before? see if we can reach the cell.
      {
      lpCell = New2 (CELL,(pszCell, hCreds));
      if ((lpCell->m_hCell = lpCell->GetCellObject (pStatus)) == NULL)
         Delete (lpCell);
      else
         {
         lpiCell = lpCell->GetIdentifier();
         lpCell->m_nReqs = 1;
         x_lCells->Add (lpCell);
         NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpiCell);
         }
      }

   AfsClass_Leave();
   return lpiCell;
}


void CELL::CloseCell (LPIDENT lpiCell)
{
   LPCELL lpCell;
   if ((lpCell = lpiCell->OpenCell()) != NULL)
      {
      if (lpCell->m_nReqs > 1)
         {
         lpCell->m_nReqs--;
         lpCell->Close();
         }
      else
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, lpiCell);
         lpCell->CloseKasObject();
         lpCell->CloseCellObject();
         lpCell->Close();
         x_lCells->Remove (lpCell);
         Delete (lpCell);
         }
      }
}


LPCELL CELL::ReopenCell (LPTSTR pszCell, ULONG *pStatus)
{
   LPCELL lpCell = NULL;
   AfsClass_Enter();
   InitClass();

   // Ordinarily we'd place a key on the cell name within the list of
   // cells--however, the most likely case only has one cell anyway.
   // So why expend the memory?
   //
   for (LPENUM pEnum = x_lCells->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPCELL lpCellFound = (LPCELL)( pEnum->GetObject() );

      if (!lstrcmpi (lpCellFound->m_szName, pszCell))
         {
         lpCell = lpCellFound;
         Delete (pEnum);
         break;
         }
      }

   if (lpCell == NULL)
      {
      AfsClass_Leave();
      if (pStatus)
         *pStatus = ERROR_FILE_NOT_FOUND;
      }

   // AfsClass_Leave() has been called only if no cell was opened in the search.
   return lpCell;
}


PVOID CELL::GetCurrentCredentials (void)
{
   return m_hCreds;
}


void CELL::SetCurrentCredentials (PVOID hCreds)
{
   CloseCellObject();

   m_hCreds = hCreds;

   GetCellObject();
}


/*
 * SERVER-LIST MANAGEMENT _____________________________________________________
 *
 */

LPSERVER CELL::OpenServer (LPTSTR pszName, ULONG *pStatus)
{
   if (!RefreshServers (TRUE, pStatus))
      return NULL;

   LPSERVER lpServer;
   if ((lpServer = (LPSERVER)(m_lkServerName->GetFirstObject (pszName))) != NULL)
      AfsClass_Enter();

   return lpServer;
}


LPSERVER CELL::OpenServer (LPSOCKADDR_IN pAddr, ULONG *pStatus)
{
   if (!RefreshServers (TRUE, pStatus))
      return NULL;

   // We'll try to use our lookup key first--since most machines only
   // have one IP address anyway, our hashtable should make this lookup
   // super fast. If it misses (i.e., if the server is multi-homed and
   // for some reason VLDB refers to it by the second address), we'll
   // have to do a brute-force search across each server in the cell.
   // Again, we could have a better-designed lookup table here--but
   // since multi-homing is the exception (by a vast majority), it's not
   // worth the extra effort and memory. This technique is fast enough.
   //
   LPSERVER lpServer;
   if ((lpServer = (LPSERVER)(m_lkServerAddr->GetFirstObject (pAddr))) != NULL)
      {
      AfsClass_Enter(); // Aren't HashLists great? We found the server.
      }
   else // Try brute-force search
      {
      HENUM hEnum;
      for (lpServer = ServerFindFirst (&hEnum, TRUE, pStatus); lpServer; lpServer = ServerFindNext (&hEnum))
         {
         SERVERSTATUS ss;
         if (lpServer->GetStatus (&ss, TRUE, pStatus))
            {
            for (size_t iAddr = 0; iAddr < ss.nAddresses; ++iAddr)
               {
               if (!memcmp (&ss.aAddresses[ iAddr ], pAddr, sizeof(SOCKADDR_IN)))
                  {
                  // don't close server! we're going to return this pointer.
                  break;
                  }
               }
            }
         lpServer->Close();
         }
      ServerFindClose (&hEnum);
      }
   return lpServer;
}


LPSERVER CELL::ServerFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return ServerFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPSERVER CELL::ServerFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPSERVER lpServer = NULL;

   if (!RefreshServers (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpServer = lpiFind->OpenServer();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lServers->FindFirst()) != NULL)
      {
      lpServer = (LPSERVER)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpServer && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpServer;
}


LPSERVER CELL::ServerFindNext (HENUM *phEnum)
{
   LPSERVER lpServer = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpServer = (LPSERVER)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpServer;
}


void CELL::ServerFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}


BOOL CELL::RefreshServers (BOOL fNotify, ULONG *pStatus)
{
   if (!m_fServersOutOfDate)
      return TRUE;

   return RefreshServerList (fNotify, pStatus);
}


BOOL CELL::RefreshServerList (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;
   BOOL fNotified = FALSE;

   if (fNotify && m_fServersOutOfDate)
      {
      NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServersBegin, GetIdentifier());
      fNotified = TRUE;
      }

   BOOL fReplaceList = m_fServersOutOfDate;
   m_fServersOutOfDate = FALSE;

   // Ordinarily we'd just clear the list of servers and
   // requery it from scratch; however, servers are an exception
   // to that technique: occasionally we may get a request to
   // just look for servers that have appeared or disappeared,
   // without refreshing data for other servers. Thus the revised
   // technique:
   //
   //    1- if fReplaceList, empty the list of servers.
   //       otherwise, set each server's fDelete flag.
   //
   //    2- enumerate the servers in the cell: for each server,
   //          if fReplaceList, add the server to the list
   //          otherwise, if the server is in the list, clear its fDelete
   //
   //    3- if !fReplaceList, enumerate the list of servers: for each server,
   //          if the server's fDelete flag is set, remove the server
   //
   for (LPENUM pEnum = m_lServers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPSERVER lpServer = (LPSERVER)(pEnum->GetObject());

      if (fReplaceList)
         {
         lpServer->SendDeleteNotifications();
         m_lServers->Remove (lpServer);
         Delete (lpServer);
         }
      else // the list of servers isn't invalidated, so just mark fDelete
         {
         lpServer->m_fDelete = TRUE;
         }
      }

   // Enumerate the servers in the cell.
   //
   PVOID hCell;
   if ((hCell = GetCellObject (&status)) == NULL)
      {
      rc = FALSE;
      FreeUsers (TRUE);
      FreeServers (TRUE);
      }
   else
      {
      WORKERPACKET wpBegin;
      wpBegin.wpClientAFSServerGetBegin.hCell = hCell;

      if (!Worker_DoTask (wtaskClientAFSServerGetBegin, &wpBegin, &status))
         {
         rc = FALSE;
         }
      else
         {
         for (;;)
            {
            WORKERPACKET wpNext;
            wpNext.wpClientAFSServerGetNext.hEnum = wpBegin.wpClientAFSServerGetBegin.hEnum;
            if (!Worker_DoTask (wtaskClientAFSServerGetNext, &wpNext, &status))
               {
               if (status == ADMITERATORDONE)
                  status = 0;
               else
                  rc = FALSE;
               break;
               }

            afs_serverEntry_p pEntry = &wpNext.wpClientAFSServerGetNext.Entry;

            TCHAR szServer[ cchNAME ];
            CopyAnsiToString (szServer, pEntry->serverName);
            if (!szServer[0])
               {
               int addrNetwork = htonl (pEntry->serverAddress[0]);
               lstrcpy (szServer, inet_ntoa (*(struct in_addr *)&addrNetwork));
               }

            // The server identified by {pEntry} is in the cell. Now if we're
            // building a list of SERVER objects from scratch, we can just
            // add it--but if we're only touching up an existing list,
            // first make sure there isn't such an animal in there now.
            //
            BOOL fNotifyAboutThisServer = FALSE;

            LPSERVER lpServer = NULL;
            if (!fReplaceList)
               {
               if ((lpServer = (LPSERVER)(m_lkServerName->GetFirstObject (szServer))) != NULL)
                  lpServer->m_fDelete = FALSE;
               }

            if (lpServer == NULL)
               {
               // Okay, it's a new server. Create a SERVER object for it and
               // add it to the list.
               //
               lpServer = New2 (SERVER,(this, szServer));
               lpServer->m_wGhost |= GHOST_HAS_SERVER_ENTRY;
               m_lServers->Add (lpServer);
               fNotifyAboutThisServer = TRUE;
               }

            // Update the server's IP addresses
            //
            lpServer->m_ss.nAddresses = 0;

            for (size_t iAddr = 0; iAddr < AFS_MAX_SERVER_ADDRESS; ++iAddr)
               {
               if (pEntry->serverAddress[ iAddr ] == 0)
                  continue;
               AfsClass_IntToAddress (&lpServer->m_ss.aAddresses[ lpServer->m_ss.nAddresses++ ], pEntry->serverAddress[ iAddr ]);
               }

            m_lServers->Update (lpServer); // That update affected a hashlistkey

            // Tell our clients that we've found a server
            //
            if (fNotify && fNotifyAboutThisServer)
               {
               if (!fNotified)
                  {
                  NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServersBegin, GetIdentifier());
                  fNotified = TRUE;
                  }
               NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpServer->GetIdentifier());
               }
            }

         WORKERPACKET wpDone;
         wpDone.wpClientAFSServerGetDone.hEnum = wpBegin.wpClientAFSServerGetBegin.hEnum;
         Worker_DoTask (wtaskClientAFSServerGetDone, &wpDone);
         }
      }

   // Finally, look through our list of servers: if any have fDelete set,
   // then we didn't find them in the cell any longer. Remove those servers.
   //
   if (rc)
      {
      for (LPENUM pEnum = m_lServers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPSERVER lpServer = (LPSERVER)(pEnum->GetObject());
         if (lpServer->m_fDelete)
            {
            if (fNotify && !fNotified)
               {
               NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServersBegin, GetIdentifier());
               fNotified = TRUE;
               }
            lpServer->SendDeleteNotifications();
            m_lServers->Remove (lpServer);
            Delete (lpServer);
            }
         }
      }

   // Fix m_apszServers if we did anything to the list of servers
   //
   if (fNotified)
      {
      if (m_apszServers)
         {
         for (size_t ii = 0; m_apszServers[ii]; ++ii)
            FreeString (m_apszServers[ii]);
         Free (m_apszServers);
         m_apszServers = NULL;
         }

      size_t cServers = 0;
      LPENUM pEnum;
      for (pEnum = m_lServers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         ++cServers;

      if (cServers)
         {
         m_apszServers = (char**)Allocate (sizeof(char*) * (1+cServers));
         memset (m_apszServers, 0x00, sizeof(char*) * (1+cServers));

         size_t iServer = 0;
         for (pEnum = m_lServers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
            {
            LPSERVER lpServer = (LPSERVER)(pEnum->GetObject());
            m_apszServers[ iServer ] = AllocateAnsi (cchNAME+1);
            CopyStringToAnsi (m_apszServers[ iServer ], lpServer->m_szName);
            ++iServer;
            }
         }
      }

   if (fNotified)
      NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServersEnd, GetIdentifier(), ((rc) ? 0 : status));

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


/*
 * SERVER-LIST KEYS ___________________________________________________________
 *
 */

BOOL CALLBACK CELL::KeyServerName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   if (!lstrcmp (((LPSERVER)pObject)->m_szName, (LPTSTR)pData))
      return TRUE;

   TCHAR szShortName[ cchNAME ];
   SERVER::ShortenName (szShortName, ((LPSERVER)pObject)->m_szName);
   if (!lstrcmp (szShortName, (LPTSTR)pData))
      return TRUE;

   return FALSE;
}

HASHVALUE CALLBACK CELL::KeyServerName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CELL::KeyServerName_HashData (pKey, ((LPSERVER)pObject)->m_szName);
}

HASHVALUE CALLBACK CELL::KeyServerName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   TCHAR szShortName[ cchNAME ];
   SERVER::ShortenName (szShortName, (LPTSTR)pData);
   return HashString (szShortName);
}


BOOL CALLBACK CELL::KeyServerAddr_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !memcmp (&((LPSERVER)pObject)->m_ss.aAddresses[0], (LPSOCKADDR)pData, sizeof(SOCKADDR_IN));
}

HASHVALUE CALLBACK CELL::KeyServerAddr_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CELL::KeyServerAddr_HashData (pKey, &((LPSERVER)pObject)->m_ss.aAddresses[0]);
}

HASHVALUE CALLBACK CELL::KeyServerAddr_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return *(DWORD*)pData;
}


BOOL CALLBACK CELL::KeyUserName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmpi (((LPUSER)pObject)->m_szPrincipal, (LPTSTR)pData);
}

HASHVALUE CALLBACK CELL::KeyUserName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CELL::KeyUserName_HashData (pKey, ((LPUSER)pObject)->m_szPrincipal);
}

HASHVALUE CALLBACK CELL::KeyUserName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


BOOL CALLBACK CELL::KeyGroupName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmpi (((LPPTSGROUP)pObject)->m_szName, (LPTSTR)pData);
}

HASHVALUE CALLBACK CELL::KeyGroupName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return CELL::KeyGroupName_HashData (pKey, ((LPPTSGROUP)pObject)->m_szName);
}

HASHVALUE CALLBACK CELL::KeyGroupName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


/*
 * CELL OBJECT ________________________________________________________________
 *
 */

PVOID CELL::GetCellObject (ULONG *pStatus)
{
   if (!m_hCell)
      {
      ULONG status;
      NOTIFYCALLBACK::SendNotificationToAll (evtOpenCellBegin, m_szName);

      WORKERPACKET wpOpen;
      wpOpen.wpClientCellOpen.pszCell = m_szName;
      wpOpen.wpClientCellOpen.hCreds = m_hCreds;

      if (Worker_DoTask (wtaskClientCellOpen, &wpOpen, &status))
         m_hCell = wpOpen.wpClientCellOpen.hCell;

      if (pStatus)
         *pStatus = status;
      NOTIFYCALLBACK::SendNotificationToAll (evtOpenCellEnd, m_szName, status);
      }

   return m_hCell;
}


BOOL CELL::CloseCellObject (ULONG *pStatus)
{
   BOOL rc = TRUE;

   if (m_hCell != NULL)
      {
      WORKERPACKET wp;
      wp.wpClientCellClose.hCell = m_hCell;
      rc = Worker_DoTask (wtaskClientCellClose, &wp, pStatus);
      m_hCell = NULL;
      }

   return rc;
}


PVOID CELL::GetKasObject (ULONG *pStatus)
{
   // m_hKas is actually never set non-NULL;
   // leaving it NULL indicates we will work happily with *any* server.
   //
   return m_hKas;
}


BOOL CELL::CloseKasObject (ULONG *pStatus)
{
   BOOL rc = TRUE;

   if (m_hKas != NULL)
      {
      WORKERPACKET wp;
      wp.wpKasServerClose.hServer = m_hKas;
      rc = Worker_DoTask (wtaskKasServerClose, &wp, pStatus);
      m_hKas = NULL;
      }

   return rc;
}


/*
 * CELL GENERAL _______________________________________________________________
 *
 */

LPIDENT CELL::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


void CELL::GetName (LPTSTR pszName)
{
   lstrcpy (pszName, m_szName);
}


PVOID CELL::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void CELL::SetUserParam (PVOID pUserNew)
{
   GetIdentifier()->SetUserParam (pUserNew);
}


BOOL CELL::fAnyServersUnmonitored (void)
{
   return (m_nServersUnmonitored > 0) ? TRUE : FALSE;
}


/*
 * CELL STATUS ________________________________________________________________
 *
 */

void CELL::Invalidate (void)
{
   if (!m_fServersOutOfDate || !m_fStatusOutOfDate || !m_fVLDBOutOfDate || !m_fUsersOutOfDate)
      {
      CloseKasObject();
      CloseCellObject();
      m_fServersOutOfDate = TRUE;
      m_fStatusOutOfDate = TRUE;
      m_fVLDBOutOfDate = TRUE;
      m_fUsersOutOfDate = TRUE;
      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


void CELL::InvalidateServers (void)
{
   if (!m_fServersOutOfDate || !m_fVLDBOutOfDate)
      {
      CloseKasObject();
      CloseCellObject();
      m_fServersOutOfDate = TRUE;
      m_fVLDBOutOfDate = TRUE;
      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


void CELL::InvalidateUsers (void)
{
   if (!m_fUsersOutOfDate)
      {
      m_fUsersOutOfDate = TRUE;
      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


BOOL CELL::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (m_fStatusOutOfDate)
      {
      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      // Hmmm...well, actually, there's nothing for us to do here.  I'm
      // leaving this around, because the refreshed-cell-status notification
      // may be useful as an appropriate hooking point.
      rc = TRUE;
      status = 0;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL CELL::RefreshVLDB (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;

   if (m_fVLDBOutOfDate)
      {
      if ((rc = RefreshVLDB (GetIdentifier(), fNotify, pStatus)) == TRUE)
         {
         m_fVLDBOutOfDate = FALSE;
         }
      }

   return rc;
}


BOOL CELL::RefreshVLDB (LPIDENT lpiRef, BOOL fNotify, ULONG *pStatus, BOOL fAnythingRelatedToThisRWFileset)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   // What is the scope of this refresh operation?  The entire cell,
   // or the filesets on a particular server or aggregate?
   //
   LPIDENT lpiRefCell = (lpiRef == NULL) ? GetIdentifier() : lpiRef->GetCell();
   LPIDENT lpiRefServer = NULL;
   LPIDENT lpiRefAggregate = NULL;
   LPIDENT lpiRefFileset = NULL;
   VOLUMEID *pvidRefFileset = NULL;
   VOLUMEID vidRefFileset;

   if (fAnythingRelatedToThisRWFileset)
      {
      pvidRefFileset = &vidRefFileset;
      lpiRef->GetFilesetID (pvidRefFileset);
      }
   else
      {
      if (lpiRef && !lpiRef->fIsCell())
         {
         lpiRefServer = lpiRef->GetServer();
         }
      if (lpiRef && (lpiRef->fIsAggregate() || lpiRef->fIsFileset()))
         {
         lpiRefAggregate = lpiRef->GetAggregate();
         }
      if (lpiRef && lpiRef->fIsFileset())
         {
         lpiRefFileset = lpiRef;
         }
      }

   // If we've been told to update only one server, aggregate or
   // fileset, find out which IP addresses correspond with that
   // server. We'll need this for comparisons later.
   //
   SERVERSTATUS ssRefServer;
   if (rc && lpiRefServer)
      {
      LPSERVER lpServer;
      if ((lpServer = lpiRefServer->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         rc = lpServer->GetStatus (&ssRefServer, fNotify, &status);
         lpServer->Close();
         }
      }

   // Likewise, if we've been told to update only one aggregate,
   // find that aggregate's ID.  We'll need it for comparisons later.
   //
   AGGREGATESTATUS asRefAggregate;
   int idPartition = NO_PARTITION;
   if (rc && lpiRefAggregate)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiRefAggregate->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         idPartition = lpAggregate->GetID();
         rc = lpAggregate->GetStatus (&asRefAggregate, fNotify, &status);
         lpAggregate->Close();
         }
      }

   // Zip through the current list of objects that we're about to refresh.
   // On each such object, remove the GHOST_HAS_VLDB_ENTRY flag,
   // and delete objects entirely if that's the only ghost flag they have.
   // (e.g., If we went through this routine earlier and created a ghost
   // aggregate because VLDB referenced it and we couldn't find mention
   // of it on the server, delete that aggregate.  We'll recreate it here
   // if necessary; otherwise, it needs to be gone.)
   //
   if (rc)
      {
      RefreshVLDB_RemoveReferences (lpiRefServer, lpiRefAggregate, lpiRefFileset, pvidRefFileset);
      }

   // We'll get a new list of filesets from VLDB, and to do that, we'll
   // need the cell's object. If we're enumerating a specific server, we'll
   // also need that server's object. Finally, if we're enumerating a
   // specific aggregate, we'll also need that aggregate's name.
   //
   PVOID hCell = NULL;
   PVOID hServer = NULL;

   if (rc)
      {
      if (!lpiRefServer)
         {
         if ((hCell = GetCellObject (&status)) == NULL)
            rc = FALSE;
         }
      else // get cell and server handles
         {
         LPSERVER lpServer;
         if ((lpServer = lpiRefServer->OpenServer()) == NULL)
            rc = FALSE;
         else
            {
            if ((hServer = lpServer->OpenVosObject (&hCell, &status)) == NULL)
               rc = FALSE;
            lpServer->Close();
            }
         }
      }

   // Go get that list of filesets, and use it to update our knowledge
   // of the cell. Remember that, if {pvidRefFileset}, we only want
   // one VLDB entry.
   //
   if (rc)
      {
      if (pvidRefFileset)
         {
         WORKERPACKET wpGet;
         wpGet.wpVosVLDBGet.hCell = hCell;
         wpGet.wpVosVLDBGet.idVolume = *pvidRefFileset;

         if (!Worker_DoTask (wtaskVosVLDBGet, &wpGet, &status))
            rc = FALSE;
         else
            RefreshVLDB_OneEntry (&wpGet.wpVosVLDBGet.Data, lpiRefServer, &ssRefServer, lpiRefAggregate, &asRefAggregate, lpiRefFileset, pvidRefFileset, fNotify);
         }
      else
         {
         WORKERPACKET wpBegin;
         wpBegin.wpVosVLDBGetBegin.hCell = hCell;
         wpBegin.wpVosVLDBGetBegin.hServer = hServer;
         wpBegin.wpVosVLDBGetBegin.idPartition = idPartition;

         if (!Worker_DoTask (wtaskVosVLDBGetBegin, &wpBegin, &status))
            rc = FALSE;
         else
            {
            for (;;)
               {
               WORKERPACKET wpNext;
               wpNext.wpVosVLDBGetNext.hEnum = wpBegin.wpVosVLDBGetBegin.hEnum;
               if (!Worker_DoTask (wtaskVosVLDBGetNext, &wpNext, &status))
                  {
                  if (status == ADMITERATORDONE)
                     status = 0;
                  else
                     rc = FALSE;
                  break;
                  }

               RefreshVLDB_OneEntry (&wpNext.wpVosVLDBGetNext.Data, lpiRefServer, &ssRefServer, lpiRefAggregate, &asRefAggregate, lpiRefFileset, pvidRefFileset, fNotify);
               }

            WORKERPACKET wpDone;
            wpDone.wpVosVLDBGetDone.hEnum = wpBegin.wpVosVLDBGetBegin.hEnum;
            Worker_DoTask (wtaskVosVLDBGetDone, &wpDone);
            }
         }
      }

   // We've finished the update. If we were asked to send notifications
   // about our progress, do so.
   //
   if (fNotify)
      {
      LPIDENT lpiNotify = (lpiRef) ? lpiRef : GetIdentifier();

      if (!lpiNotify->fIsFileset())
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshFilesetsBegin, lpiNotify);
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshFilesetsEnd, lpiNotify, status);
         }

      if (lpiNotify->fIsCell() || lpiNotify->fIsServer())
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAggregatesBegin, lpiNotify);
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAggregatesEnd, lpiNotify, status);
         }
      }

   if (hServer)
      {
      LPSERVER lpServer;
      if ((lpServer = lpiRefServer->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


void CELL::RefreshVLDB_RemoveReferences (LPIDENT lpiRefServer, LPIDENT lpiRefAggregate, LPIDENT lpiRefFileset, LPVOLUMEID pvidRefFileset)
{
   // Zip through the current list of objects that we're about to refresh.
   // On each such object, remove the GHOST_HAS_VLDB_ENTRY flag,
   // and delete objects entirely if that's the only ghost flag they have.
   // (e.g., If we went through this routine earlier and created a ghost
   // aggregate because VLDB referenced it and we couldn't find mention
   // of it on the server, delete that aggregate.  We'll recreate it here
   // if necessary; otherwise, it needs to be gone.)
   //
   // Note that by specifying {lpiRefServer} to start the enumeration,
   // we'll either enumerate only lpiRefServer, or all servers if it's NULL.
   //
   HENUM heServer;
   for (LPSERVER lpServer = ServerFindFirst (&heServer, lpiRefServer); lpServer; lpServer = ServerFindNext (&heServer))
      {

      if (!pvidRefFileset)
         {
         // Since we're about to check VLDB for references to this server,
         // remove the GHOST_HAS_VLDB_ENTRY flag from its SERVER object.
         // If that's the only thing keeping the object around, remove the
         // object entirely.
         //
         if ((lpServer->m_wGhost &= (~GHOST_HAS_VLDB_ENTRY)) == 0)
            {
            lpServer->Close();
            lpServer->SendDeleteNotifications();
            m_lServers->Remove (lpServer);
            Delete (lpServer);
            continue;
            }
         }

      // Check each of the server's aggregates, and deal with them the same
      // way.
      //
      HENUM heAggregate;
      for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&heAggregate, lpiRefAggregate); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&heAggregate))
         {

         if (!pvidRefFileset)
            {
            // Since we're about to check VLDB for references to this aggregate,
            // remove the GHOST_HAS_VLDB_ENTRY flag from its AGGREGATE object.
            // If that's the only thing keeping the object around, remove the
            // object entirely.
            //
            if ((lpAggregate->m_wGhost &= (~GHOST_HAS_VLDB_ENTRY)) == 0)
               {
               lpAggregate->Close();
               lpAggregate->SendDeleteNotifications();
               lpServer->m_lAggregates->Remove (lpAggregate);
               Delete (lpAggregate);
               continue;
               }
            }

         // Check each of the aggregate's filesets, and deal with them the same
         // way.
         //
         HENUM heFileset;
         for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&heFileset, lpiRefFileset); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&heFileset))
            {
            if ((!pvidRefFileset) || (*pvidRefFileset == lpFileset->m_fs.idReadWrite))
               {
               // Since we're about to check VLDB for references to this fileset,
               // remove the GHOST_HAS_VLDB_ENTRY flag from its FILESET object.
               // If that's the only thing keeping the object around, remove the
               // object entirely.
               //
               if ((lpFileset->m_wGhost &= (~GHOST_HAS_VLDB_ENTRY)) == 0)
                  {
                  lpFileset->Close();
                  lpFileset->SendDeleteNotifications();
                  lpAggregate->m_lFilesets->Remove (lpFileset);
                  Delete (lpFileset);
                  continue;
                  }
               }

            lpFileset->Close();
            }

         lpAggregate->Close();
         }

      lpServer->Close();
      }
}


void CELL::RefreshVLDB_OneEntry (PVOID pp, LPIDENT lpiRefServer, LPSERVERSTATUS pssRefServer, LPIDENT lpiRefAggregate, LPAGGREGATESTATUS pasRefAggregate, LPIDENT lpiRefFileset, LPVOLUMEID pvidRefFileset, BOOL fNotify)
{
   vos_vldbEntry_p pEntry = (vos_vldbEntry_p)pp;

   // If we were asked to update all the replicas of a particular
   // fileset, then we set {pvidRefFileset} above to that fileset's
   // ID. Check this VLDB entry to see if it refers to that fileset;
   // if not, we're not interested in it.
   //
   if (pvidRefFileset)
      {
      if (memcmp (&pEntry->volumeId[ VOS_READ_WRITE_VOLUME ], pvidRefFileset, sizeof(VOLUMEID)))
         return;
      }

   for (int iRepSite = 0; iRepSite < pEntry->numServers; ++iRepSite)
      {
      SOCKADDR_IN RepSiteAddr;
      AfsClass_IntToAddress (&RepSiteAddr, pEntry->volumeSites[ iRepSite ].serverAddress);

      // Every fileset replication site which VLDB knows about
      // passes through this point, within {pEntry->volumeSites[ iRepSite ]}.

      // Are we going to be refreshing the server/aggregate on which
      // this repsite lives?  If not, there's no need to process this
      // entry any further.
      //
      if (lpiRefServer)
         {
         BOOL fFilesetLivesOnThisServer = FALSE;

         for (size_t iAddress = 0; !fFilesetLivesOnThisServer && (iAddress < pssRefServer->nAddresses); ++iAddress)
            {
            if (!memcmp (&pssRefServer->aAddresses[ iAddress ], &RepSiteAddr, sizeof(SOCKADDR_IN)))
               {
               if (lpiRefAggregate)
                  {
                  if (pasRefAggregate->dwID != (DWORD)(pEntry->volumeSites[ iRepSite ].serverPartition))
                     continue;
                  }
               fFilesetLivesOnThisServer = TRUE;
               }
            }

         if (!fFilesetLivesOnThisServer)
            continue;
         }

      // Do we know about the server mentioned by this replication
      // site?
      //
      LPSERVER lpServer;
      if (lpiRefServer != NULL)
         lpServer = lpiRefServer->OpenServer();
      else
         lpServer = OpenServer (&RepSiteAddr);

      // If we found the server but aren't monitoring it,
      // forget about this fileset.
      //
      if (lpServer && !lpServer->fIsMonitored())
         {
         lpServer->Close();
         lpServer = NULL;
         continue;
         }

      // If we have no record of the server mentioned by this
      // replication site, we have to create a SERVER entry for
      // it before we can proceed.  The server will appear as
      // a "ghost".
      //
      if (!lpServer)
         {
         if (lpiRefAggregate || pvidRefFileset)
            continue;

         LPTSTR pszServer = FormatString (TEXT("%1"), TEXT("%a"), &pEntry->volumeSites[ iRepSite ].serverAddress);
         lpServer = New2 (SERVER,(this, pszServer));
         AfsClass_Enter();
         FreeString (pszServer);

         lpServer->m_fStatusOutOfDate = FALSE;
         lpServer->m_ss.nAddresses = 1;
         memcpy (&lpServer->m_ss.aAddresses[0], &RepSiteAddr, sizeof(SOCKADDR_IN));

         m_lServers->Add (lpServer);

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpServer->GetIdentifier());
         }

      lpServer->m_wGhost |= GHOST_HAS_VLDB_ENTRY;

      // Great--we now have a replication site for a particular
      // fileset known to VLDB, and a pointer to the server
      // on which it resides.  Does that server contain the
      // aggregate which VLDB expects to find?
      //
      LPAGGREGATE lpAggregate;
      if (lpiRefAggregate != NULL)
         lpAggregate = lpiRefAggregate->OpenAggregate();
      else
         lpAggregate = lpServer->OpenAggregate (pEntry->volumeSites[ iRepSite ].serverPartition);

      // If the server has no record of the aggregate mentioned
      // by this replication site, we have to create an
      // AGGREGATE entry for it before we can proceed.  The
      // aggregate will appear as a "ghost".  Note that we
      // can't update the list of aggregates on a server if
      // we've been asked to update a particular fileset,
      // because someone clearly has a pointer to the list.
      //
      if (!lpAggregate)
         {
         if (lpiRefFileset || pvidRefFileset)
            {
            lpServer->Close();
            lpServer = NULL;
            continue;
            }

         // Even if the partition doesn't exist, we can still figger out
         // its name given its ID--'cause there's a 1:1 mapping between
         // allowed IDs and allowed partition names. I guess there's
         // something to be said for forcing partitions to be named "vicep*"
         //
         TCHAR szPartition[ cchNAME ];
         WORKERPACKET wp;
         wp.wpVosPartitionIdToName.idPartition = pEntry->volumeSites[ iRepSite ].serverPartition;
         wp.wpVosPartitionIdToName.pszPartition = szPartition;
         if (!Worker_DoTask (wtaskVosPartitionIdToName, &wp))
            wsprintf (szPartition, TEXT("#%lu"), pEntry->volumeSites[ iRepSite ].serverPartition);

         lpAggregate = New2 (AGGREGATE,(lpServer, szPartition, TEXT("")));
         AfsClass_Enter();

         lpAggregate->m_fStatusOutOfDate = FALSE;
         lpAggregate->m_as.dwID = pEntry->volumeSites[ iRepSite ].serverPartition;
         lpAggregate->m_as.ckStorageTotal = 0;
         lpAggregate->m_as.ckStorageFree = 0;

         lpServer->m_lAggregates->Add (lpAggregate);

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpAggregate->GetIdentifier());
         }

      lpAggregate->m_wGhost |= GHOST_HAS_VLDB_ENTRY;

      // Great--we now have a replication site for a particular
      // fileset known to VLDB, and a pointer to the server
      // and aggregate on which it resides.  Does that aggregate
      // contain the fileset which VLDB expects to find?
      //
      // Remember that each iRepSite can represent up to three
      // filesets on that aggregate--a RW, a RO, and a BAK.
      //
      for (size_t iType = 0; iType < 3; ++iType)
         {

         // Does this repsite entry mention having this type
         // of fileset on this aggregate?
         //
         if ((vos_volumeType_t)iType == VOS_READ_WRITE_VOLUME)
            {
            if (!((DWORD)pEntry->volumeSites[ iRepSite ].serverFlags & (DWORD)VOS_VLDB_READ_WRITE))
               continue;
            }
         else if ((vos_volumeType_t)iType == VOS_READ_ONLY_VOLUME)
            {
            if (!((DWORD)pEntry->volumeSites[ iRepSite ].serverFlags & (DWORD)VOS_VLDB_READ_ONLY))
               continue;
            }
         else if ((vos_volumeType_t)iType == VOS_BACKUP_VOLUME)
            {
            if (!((DWORD)pEntry->status & (DWORD)VOS_VLDB_ENTRY_BACKEXISTS))
               continue;

            // Only look for the backup where the R/W exists
            if (!((DWORD)pEntry->volumeSites[ iRepSite ].serverFlags & (DWORD)VOS_VLDB_READ_WRITE))
               continue;
            }

         LPFILESET lpFileset = lpAggregate->OpenFileset ((LPVOLUMEID)&pEntry->volumeId[ iType ]);

         // If the aggregate has no record of the fileset mentioned
         // by this VLDB entry, we have to create a FILESET entry
         // for it.  The fileset will appear as a "ghost".
         //
         if (!lpFileset)
            {
            TCHAR szFilesetName[ cchNAME ];
            CopyAnsiToString (szFilesetName, pEntry->name);
            if ((vos_volumeType_t)iType == VOS_READ_ONLY_VOLUME)
               lstrcat (szFilesetName, TEXT(".readonly"));
            else if ((vos_volumeType_t)iType == VOS_BACKUP_VOLUME)
               lstrcat (szFilesetName, TEXT(".backup"));

            lpFileset = New2 (FILESET,(lpAggregate, &pEntry->volumeId[ iType ], szFilesetName));
            AfsClass_Enter();

            lpFileset->m_fs.id = pEntry->volumeId[ iType ];
            lpFileset->m_fs.idReadWrite = pEntry->volumeId[ VOS_READ_WRITE_VOLUME ];
            lpFileset->m_fs.idReplica = pEntry->volumeId[ VOS_READ_ONLY_VOLUME ];
            AfsClass_UnixTimeToSystemTime (&lpFileset->m_fs.timeCreation, 0);
            AfsClass_UnixTimeToSystemTime (&lpFileset->m_fs.timeLastUpdate, 0);
            AfsClass_UnixTimeToSystemTime (&lpFileset->m_fs.timeLastAccess, 0);
            AfsClass_UnixTimeToSystemTime (&lpFileset->m_fs.timeLastBackup, 0);
            AfsClass_UnixTimeToSystemTime (&lpFileset->m_fs.timeCopyCreation, 0);
            lpFileset->m_fs.nFiles = 0;
            lpFileset->m_fs.ckQuota = 0;
            lpFileset->m_fs.ckUsed = 0;
            lpFileset->m_fs.Type = (iType == 0) ? ftREADWRITE : (iType == 1) ? ftREPLICA : ftCLONE;
            lpFileset->m_fs.State = fsNORMAL;

            lpAggregate->m_lFilesets->Add (lpFileset);

            if (fNotify)
               NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpFileset->GetIdentifier());
            }

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, lpFileset->GetIdentifier());

         lpFileset->m_wGhost |= GHOST_HAS_VLDB_ENTRY;
         lpFileset->m_fStatusOutOfDate = FALSE;
         lpFileset->m_fs.idClone = pEntry->cloneId;
         lpFileset->m_fs.State &= ~fsMASK_VLDB;

         if (pEntry->status & VOS_VLDB_ENTRY_LOCKED)
            lpFileset->m_fs.State |= fsLOCKED;

         lpFileset->Close();

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, lpFileset->GetIdentifier(), 0);
         }

      if (lpServer && lpAggregate)
         {
         lpAggregate->InvalidateAllocation();
         lpAggregate->Close();
         lpAggregate = NULL;
         }
      if (lpServer)
         {
         lpServer->Close();
         lpServer = NULL;
         }
      }
}


BOOL CELL::RefreshAll (ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   BOOL fNotified = FALSE;

   if (m_fServersOutOfDate && (dwWant & AFSCLASS_WANT_VOLUMES))
      {
      if (!fNotified)
         {
         fNotified = TRUE;
         if ((++cRefreshAllReq) == 1)
            {
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllBegin, GetIdentifier());
            }
         }

      size_t nServersToRefresh = 0;

      HENUM hEnum;
      for (LPSERVER lpServer = ServerFindFirst (&hEnum); lpServer; lpServer = ServerFindNext (&hEnum))
         {
         if (lpServer->fIsMonitored())
            ++nServersToRefresh;
         lpServer->Close();
         }

      if (nServersToRefresh)
         {
         size_t iServer = 0;
         for (LPSERVER lpServer = ServerFindFirst (&hEnum); lpServer; lpServer = ServerFindNext (&hEnum))
            {
            if (lpServer->fIsMonitored())
               {
               ULONG perComplete = (ULONG)iServer * 100L / nServersToRefresh;
               NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllUpdate, lpServer->GetIdentifier(), NULL, NULL, NULL, perComplete, 0);

               // intentionally ignore errors in refreshing individual
               // servers--we only want to return an error code if
               // we couldn't refresh the *cell*.
               //
               (void)lpServer->RefreshAll (NULL, ((double)iServer / (double)nServersToRefresh), (1.0 / (double)nServersToRefresh));
               ++iServer;
               }
            lpServer->Close();
            }

         rc = RefreshVLDB (NULL, TRUE, &status);
         }
      }

   if (rc && m_fUsersOutOfDate && (dwWant & AFSCLASS_WANT_USERS))
      {
      if (!fNotified)
         {
         fNotified = TRUE;
         if ((++cRefreshAllReq) == 1)
            {
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllBegin, GetIdentifier(), NULL, NULL, NULL, 0, 0);
            }
         }

      rc = RefreshUsers (TRUE, &status);
      }

   if (fNotified)
      {
      if ((--cRefreshAllReq) == 0)
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllEnd, GetIdentifier(), NULL, NULL, NULL, 100, status);
         }
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


/*
 * USER/GROUP-LIST MANAGEMENT _________________________________________________
 *
 */

LPUSER CELL::OpenUser (LPTSTR pszName, LPTSTR pszInstance, ULONG *pStatus)
{
   ULONG status = 0;

   // First off, do we have a USER object for this guy already?
   //
   LPUSER lpUser = NULL;
   for (LPENUM pEnum = m_lkUserName->FindFirst (pszName); pEnum; pEnum = pEnum->FindNext())
      {
      LPUSER lpTest = (LPUSER)( pEnum->GetObject() );
      if (!pszInstance || !lstrcmpi (lpTest->m_szInstance, pszInstance))
         {
         lpUser = lpTest;
         AfsClass_Enter();
         Delete (pEnum);
         break;
         }
      }

   // If not, see if we can create one...
   //
   if (!lpUser)
      {
      // See if there's a KAS or PTS entry for this user.
      //
      BOOL fHasKAS = FALSE;
      BOOL fHasPTS = FALSE;

      WORKERPACKET wp;
      wp.wpKasPrincipalGet.hCell = GetCellObject (&status);
      wp.wpKasPrincipalGet.hServer = GetKasObject (&status);
      wp.wpKasPrincipalGet.pszPrincipal = pszName;
      wp.wpKasPrincipalGet.pszInstance = pszInstance;
      if (Worker_DoTask (wtaskKasPrincipalGet, &wp, &status))
         fHasKAS = TRUE;

      if (!fHasKAS)
         {
         TCHAR szFullName[ cchNAME ];
         AfsClass_GenFullUserName (szFullName, pszName, pszInstance);

         WORKERPACKET wp;
         wp.wpPtsUserGet.hCell = GetCellObject();
         wp.wpPtsUserGet.pszUser = szFullName;
         if (Worker_DoTask (wtaskPtsUserGet, &wp, &status))
            fHasPTS = TRUE;
         }
      if (fHasKAS || fHasPTS)
         {
         lpUser = New2 (USER,(this, pszName, pszInstance));
         m_lUsers->Add (lpUser);
         NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpUser->GetIdentifier());
         AfsClass_Enter();
         }
      }

   if (!lpUser && pStatus)
      *pStatus = status;
   return lpUser;
}


LPUSER CELL::UserFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return UserFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPUSER CELL::UserFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPUSER lpUser = NULL;

   if (!RefreshUsers (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpUser = lpiFind->OpenUser();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lUsers->FindFirst()) != NULL)
      {
      lpUser = (LPUSER)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpUser && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpUser;
}


LPUSER CELL::UserFindNext (HENUM *phEnum)
{
   LPUSER lpUser = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpUser = (LPUSER)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpUser;
}


void CELL::UserFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}



LPPTSGROUP CELL::OpenGroup (LPTSTR pszName, ULONG *pStatus)
{
   ULONG status;

   // First off, do we have a USER object for this guy already?
   //
   LPPTSGROUP lpGroup;
   if ((lpGroup = (LPPTSGROUP)(m_lkGroupName->GetFirstObject (pszName))) != NULL)
      AfsClass_Enter();

   // If not, see if we can create one...
   //
   if (!lpGroup)
      {
      // See if there's a PTS entry for this group.
      //
      WORKERPACKET wp;
      wp.wpPtsGroupGet.hCell = GetCellObject();
      wp.wpPtsGroupGet.pszGroup = pszName;
      if (Worker_DoTask (wtaskPtsGroupGet, &wp, &status))
         {
         lpGroup = New2 (PTSGROUP,(this, pszName));
         m_lGroups->Add (lpGroup);
         NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpGroup->GetIdentifier());
         AfsClass_Enter();
         }
      }

   if (!lpGroup && pStatus)
      *pStatus = status;
   return lpGroup;
}


LPPTSGROUP CELL::GroupFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return GroupFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPPTSGROUP CELL::GroupFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPPTSGROUP lpGroup = NULL;

   if (!RefreshUsers (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpGroup = lpiFind->OpenGroup();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lGroups->FindFirst()) != NULL)
      {
      lpGroup = (LPPTSGROUP)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpGroup && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpGroup;
}


LPPTSGROUP CELL::GroupFindNext (HENUM *phEnum)
{
   LPPTSGROUP lpGroup = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpGroup = (LPPTSGROUP)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpGroup;
}


void CELL::GroupFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}


BOOL CELL::RefreshUsers (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (m_fUsersOutOfDate)
      {
      m_fUsersOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshUsersBegin, GetIdentifier());

      // First, forget everything we think we know about all users and groups;
      // we wouldn't be here if someone didn't think it was all out of date.
      //
      FreeUsers (TRUE);

      // Then zip through KAS to build a list of all user entries;
      // we'll need hCell and hKAS for that.
      //
      WORKERPACKET wpBegin;
      wpBegin.wpKasPrincipalGetBegin.hCell = GetCellObject();
      wpBegin.wpKasPrincipalGetBegin.hServer = GetKasObject (&status);

      if (!Worker_DoTask (wtaskKasPrincipalGetBegin, &wpBegin, &status))
         rc = FALSE;
      else
         {
         for (;;)
            {
            TCHAR szPrincipal[ cchNAME ];
            TCHAR szInstance[ cchNAME ];

            WORKERPACKET wpNext;
            wpNext.wpKasPrincipalGetNext.hEnum = wpBegin.wpKasPrincipalGetBegin.hEnum;
            wpNext.wpKasPrincipalGetNext.pszPrincipal = szPrincipal;
            wpNext.wpKasPrincipalGetNext.pszInstance = szInstance;
            if (!Worker_DoTask (wtaskKasPrincipalGetNext, &wpNext, &status))
               {
               if (status == ADMITERATORDONE)
                  status = 0;
               else
                  rc = FALSE;
               break;
               }

            // Okay, we got a user from kas. Create a USER object for it.
            //
            LPUSER lpUser = New2 (USER,(this, szPrincipal, szInstance));
            m_lUsers->Add (lpUser);

            // That was easy, wasn't it? Now check this user's groups,
            // both the ones it owns and the ones to which it belongs,
            // so we can build a full list-o-groups.
            //
            LPTSTR mszGroups;
            if (lpUser->GetMemberOf (&mszGroups))
               {
               BuildGroups (mszGroups);
               FreeString (mszGroups);
               }

            if (lpUser->GetOwnerOf (&mszGroups))
               {
               BuildGroups (mszGroups);
               FreeString (mszGroups);
               }
            }

         WORKERPACKET wpDone;
         wpDone.wpKasPrincipalGetDone.hEnum = wpBegin.wpKasPrincipalGetBegin.hEnum;
         Worker_DoTask (wtaskKasPrincipalGetDone, &wpDone);
         }

#ifdef FIND_PTS_DEBRIS
      // Icky horrible painful part: to catch entries which exist on PTS
      // but not in KAS, we need to zip back through our list of groups
      // and check thier memberships.
      //
      for (LPENUM pe = m_lGroups->FindFirst(); pe; pe = pe->FindNext())
         {
         LPPTSGROUP lpGroup = (LPPTSGROUP)(pe->GetObject());

         LPTSTR mszMembers;
         if (lpGroup->GetMembers (&mszMembers))
            {
            for (LPTSTR pszMember = mszMembers; pszMember && *pszMember; pszMember += 1+lstrlen(pszMember))
               {
               // Make sure we have a user or group account for this guy.
               // Remember that the member name may have both a name and
               // an instance.
               //
               if (m_lkGroupName->GetFirstObject (pszMember))
                  continue;

               TCHAR szNameMatch[ cchNAME ];
               TCHAR szInstanceMatch[ cchNAME ];
               USER::SplitUserName (pszMember, szNameMatch, szInstanceMatch);

               LPUSER lpFound = NULL;
               for (LPENUM pEnum = m_lkUserName->FindFirst (szNameMatch); pEnum; pEnum = pEnum->FindNext())
                  {
                  LPUSER lpTest = (LPUSER)( pEnum->GetObject() );
                  if (!lstrcmpi (lpTest->m_szInstance, szInstanceMatch))
                     {
                     lpFound = lpTest;
                     Delete (pEnum);
                     break;
                     }
                  }
               if (lpFound)
                  continue;

               // Uh oh. Is this thing a user or a group? We're really only
               // interested in finding user-account debris here...
               //
               WORKERPACKET wpGet;
               wpGet.wpPtsUserGet.hCell = GetCellObject();
               wpGet.wpPtsUserGet.pszUser = pszMember;
               if (Worker_DoTask (wtaskPtsUserGet, &wpGet))
                  {
                  if (wpGet.wpPtsUserGet.Entry.nameUid > 0)
                     {
                     LPUSER lpUser = New2 (USER,(this, pszMember, TEXT("")));
                     m_lUsers->Add (lpUser);
                     }
                  }
               }
            }
         }
#endif

      // We've finally generated a complete list of the users and groups in
      // this cell. If we've been asked to, send out notifications for all
      // the things we found.
      //
      if (fNotify)
         {
         LPENUM pEnum;
         for (pEnum = m_lGroups->FindFirst(); pEnum; pEnum = pEnum->FindNext())
            {
            LPPTSGROUP lpGroup = (LPPTSGROUP)(pEnum->GetObject());
            NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpGroup->GetIdentifier());
            }

         for (pEnum = m_lUsers->FindFirst(); pEnum; pEnum = pEnum->FindNext())
            {
            LPUSER lpUser = (LPUSER)(pEnum->GetObject());
            NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpUser->GetIdentifier());
            }

         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshUsersEnd, GetIdentifier(), ((rc) ? 0 : status));
         }
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


void CELL::BuildGroups (LPTSTR mszGroups)
{
   for (LPTSTR pszGroup = mszGroups; pszGroup && *pszGroup; pszGroup += 1+lstrlen(pszGroup))
      {
      // Make sure we have this group in our list-of-groups
      //
      LPPTSGROUP lpGroup;
      if ((lpGroup = (LPPTSGROUP)m_lkGroupName->GetFirstObject (pszGroup)) == NULL)
         {
         lpGroup = New2 (PTSGROUP,(this, pszGroup));
         m_lGroups->Add (lpGroup);
         }
      }
}


BOOL CELL::RefreshAccount (LPTSTR pszAccount, LPTSTR pszInstance, OP_CELL_REFRESH_ACCOUNT Op, LPIDENT *plpi)
{
   BOOL rc = TRUE;

   // See if we can find this thing
   //
   LPIDENT lpi;
   if ((lpi = IDENT::FindUser (m_lpiThis, pszAccount, pszInstance)) != NULL)
      {
      if (lpi->m_cRef == 0)
         lpi = NULL;
      }
   if (!lpi)
      {
      if ((lpi = IDENT::FindGroup (m_lpiThis, pszAccount)) != NULL)
         if (lpi->m_cRef == 0)
            lpi = NULL;
      }

   // If we couldn't find it, and Op is _CREATED_*, then make a new account
   //
   if ((!lpi) && (Op == CELL_REFRESH_ACCOUNT_CREATED_USER))
      {
      LPUSER lpUser = New2 (USER,(this, pszAccount, pszInstance));
      m_lUsers->Add (lpUser);
      lpi = lpUser->GetIdentifier();
      NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpi);
      }
   else if ((!lpi) && (Op == CELL_REFRESH_ACCOUNT_CREATED_GROUP))
      {
      LPPTSGROUP lpGroup = New2 (PTSGROUP,(this, pszAccount));
      m_lGroups->Add (lpGroup);
      lpi = lpGroup->GetIdentifier();
      NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpi);
      }

   // If we did find it, and Op is _DELETED, then remove the account
   //
   if (lpi && (Op == CELL_REFRESH_ACCOUNT_DELETED))
      {
      if (lpi && (lpi->GetType() == itUSER))
         {
         LPUSER lpUser;
         if ((lpUser = lpi->OpenUser()) == NULL)
            rc = FALSE;
         else
            {
            lpUser->SendDeleteNotifications();
            lpUser->Close();
            m_lUsers->Remove (lpUser);
            Delete (lpUser);
            lpi = NULL;
            }
         }
      else if (lpi && (lpi->GetType() == itGROUP))
         {
         LPPTSGROUP lpGroup;
         if ((lpGroup = lpi->OpenGroup()) == NULL)
            rc = FALSE;
         else
            {
            lpGroup->SendDeleteNotifications();
            lpGroup->Close();
            m_lGroups->Remove (lpGroup);
            Delete (lpGroup);
            lpi = NULL;
            }
         }
      else
         {
         rc = FALSE;
         }
      }

   // If we still have an ident, refresh the account's properties
   //
   if (lpi && (lpi->GetType() == itUSER))
      {
      LPUSER lpUser;
      if ((lpUser = lpi->OpenUser()) == NULL)
         rc = FALSE;
      else
         {
         lpUser->Invalidate();
         lpUser->RefreshStatus();
         lpUser->Close();
         }
      }
   else if (lpi && (lpi->GetType() == itGROUP))
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = lpi->OpenGroup()) == NULL)
         rc = FALSE;
      else
         {
         lpGroup->Invalidate();
         lpGroup->RefreshStatus();
         lpGroup->Close();
         }
      }

   if (plpi)
      *plpi = lpi;
   return rc;
}


BOOL CELL::RefreshAccounts (LPTSTR mszAccounts, OP_CELL_REFRESH_ACCOUNT Op)
{
   BOOL rc = TRUE;
   for (LPTSTR psz = mszAccounts; psz && *psz; psz += 1+lstrlen(psz))
      {
      if (!RefreshAccount (psz, NULL, Op))
         rc = FALSE;
      }
   return rc;
}

