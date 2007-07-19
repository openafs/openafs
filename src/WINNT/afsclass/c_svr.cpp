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

#include <WINNT/afsclass.h>
#include "internal.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

         // Each SERVER object maintains a list of aggregates and a list
         // of services; those lists have hashtables placed across their
         // names (and, for aggregates, their IDs) for faster lookup.
         // The default table size in a HASHLIST is 1000 elements--that's
         // too large for a list of aggregates or services on a server,
         // as it's enough to handle up to 30,000 objects before the table
         // would need to resize iteself (see the documentation in
         // hashlist.cpp for info). Instead, we choose a more reasonable
         // default table size.
         //
#define cKEYAGGREGATENAME_TABLESIZE  100
#define cKEYAGGREGATEID_TABLESIZE    100

#define cKEYSERVICENAME_TABLESIZE    50


/*
 * VARIABLES __________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */


SERVER::SERVER (LPCELL lpCellParent, LPTSTR pszName)
{
   m_lpiCell = lpCellParent->GetIdentifier();

   lstrcpy (m_szName, pszName);

   m_lpCellBOS = NULL;
   m_hCellBOS = NULL;
   m_hBOS = NULL;
   m_cReqBOS = 0;

   m_hCellVOS = NULL;
   m_hVOS = NULL;
   m_cReqVOS = 0;

   m_fCanGetAggregates = TRUE;
   m_fAggregatesOutOfDate = TRUE;
   m_lAggregates = New (HASHLIST);
   m_lAggregates->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkAggregateName = m_lAggregates->CreateKey ("Aggregate Name", SERVER::KeyAggregateName_Compare, SERVER::KeyAggregateName_HashObject, SERVER::KeyAggregateName_HashData, cKEYAGGREGATENAME_TABLESIZE);
   m_lkAggregateID = m_lAggregates->CreateKey ("Aggregate ID", SERVER::KeyAggregateID_Compare, SERVER::KeyAggregateID_HashObject, SERVER::KeyAggregateID_HashData, cKEYAGGREGATEID_TABLESIZE);

   m_fCanGetServices = TRUE;
   m_fServicesOutOfDate = TRUE;
   m_lServices = New (HASHLIST);
   m_lServices->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkServiceName = m_lServices->CreateKey ("Service Name", SERVER::KeyServiceName_Compare, SERVER::KeyServiceName_HashObject, SERVER::KeyServiceName_HashData, cKEYSERVICENAME_TABLESIZE);

   m_wGhost = 0;
   m_lpiThis = NULL;
   m_fMonitor = TRUE;
   m_fDelete = FALSE;
   m_lastStatus = 0;

   m_fVLDBOutOfDate = FALSE; /* FIXME: added because it was missing */
   m_fStatusOutOfDate = TRUE;
   memset (&m_ss, 0x00, sizeof(SERVERSTATUS));
}


SERVER::~SERVER (void)
{
   if (!m_fMonitor)
      {
      LPCELL lpCell;
      if ((lpCell = m_lpiCell->OpenCell()) != NULL)
         {
         (lpCell->m_nServersUnmonitored)--;
         lpCell->Close();
         }
      }

   if (m_lpiThis)
      m_lpiThis->m_cRef --;

   FreeAll();
   Delete (m_lAggregates);
   Delete (m_lServices);
}


void SERVER::FreeAll (void)
{
   if (m_cReqBOS)
      {
      m_cReqBOS = 1;
      CloseBosObject();
      }
   if (m_cReqVOS)
      {
      m_cReqVOS = 1;
      CloseVosObject();
      }

   FreeAggregates();
   FreeServices();
}


void SERVER::FreeAggregates (void)
{
   for (LPENUM pEnum = m_lAggregates->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPAGGREGATE lpAggregate = (LPAGGREGATE)(pEnum->GetObject());
      m_lAggregates->Remove (lpAggregate);
      Delete (lpAggregate);
      }
}


void SERVER::FreeServices (void)
{
   for (LPENUM pEnum = m_lServices->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPSERVICE lpService = (LPSERVICE)(pEnum->GetObject());
      m_lServices->Remove (lpService);
      Delete (lpService);
      }
}


void SERVER::SendDeleteNotifications (void)
{
   LPENUM pEnum;
   for (pEnum = m_lAggregates->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPAGGREGATE lpAggregate = (LPAGGREGATE)(pEnum->GetObject());
      lpAggregate->SendDeleteNotifications ();
      }

   for (pEnum = m_lServices->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPSERVICE lpService = (LPSERVICE)(pEnum->GetObject());
      lpService->SendDeleteNotifications();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void SERVER::Close (void)
{
   AfsClass_Leave();
}


LPIDENT SERVER::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


PVOID SERVER::OpenBosObject (PVOID *phCell, ULONG *pStatus)
{
   if (!m_hBOS)
      {
      if ((m_lpCellBOS = m_lpiCell->OpenCell (pStatus)) != NULL)
         {
         if ((m_hCellBOS = m_lpCellBOS->GetCellObject (pStatus)) != NULL)
            {
            TCHAR szCell[ cchNAME ];
            m_lpiCell->GetCellName (szCell);

            WORKERPACKET wp;
            wp.wpBosServerOpen.hCell = m_hCellBOS;
            wp.wpBosServerOpen.pszServer = m_szName;
            if (Worker_DoTask (wtaskBosServerOpen, &wp, pStatus))
               m_hBOS = wp.wpBosServerOpen.hServer;
            }

         if (!m_hBOS)
            {
            m_lpCellBOS->Close();
            m_lpCellBOS = NULL;
            m_hCellBOS = NULL;
            }
         }
      }

   if (m_hBOS)
      {
      if (phCell)
         *phCell = m_hCellBOS;
      ++m_cReqBOS;
      }
   return m_hBOS;
}


BOOL SERVER::CloseBosObject (ULONG *pStatus)
{
   BOOL rc = TRUE;

   if ((m_cReqBOS > 0) && ((--m_cReqBOS) == 0))
      {
      if (m_hBOS != NULL)
         {
         WORKERPACKET wp;
         wp.wpBosServerClose.hServer = m_hBOS;
         if (!Worker_DoTask (wtaskBosServerClose, &wp, pStatus))
            rc = FALSE;
         m_hBOS = NULL;
         }
      if (m_lpCellBOS != NULL)
         {
         m_lpCellBOS->Close();
         m_lpCellBOS = NULL;
         }
      }

   return rc;
}


PVOID SERVER::OpenVosObject (PVOID *phCell, ULONG *pStatus)
{
   if (!m_hCellVOS)
      {
      LPCELL lpCell;
      if ((lpCell = m_lpiCell->OpenCell (pStatus)) != NULL)
         {
         m_hCellVOS = lpCell->GetCellObject (pStatus);
         lpCell->Close();
         }
      }

   if (m_hCellVOS && !m_hVOS)
      {
      TCHAR szCell[ cchNAME ];
      m_lpiCell->GetCellName (szCell);

      WORKERPACKET wp;
      wp.wpVosServerOpen.hCell = m_hCellVOS;
      wp.wpVosServerOpen.pszServer = m_szName;
      if (Worker_DoTask (wtaskVosServerOpen, &wp, pStatus))
         m_hVOS = wp.wpVosServerOpen.hServer;
      }

   if (m_hVOS)
      {
      if (phCell)
         *phCell = m_hCellVOS;
      ++m_cReqVOS;
      }
   return m_hVOS;
}


BOOL SERVER::CloseVosObject (ULONG *pStatus)
{
   BOOL rc = TRUE;

   if ((m_cReqVOS > 0) && ((--m_cReqVOS) == 0))
      {
      if (m_hVOS != NULL)
         {
         WORKERPACKET wp;
         wp.wpVosServerClose.hServer = m_hVOS;
         if (!Worker_DoTask (wtaskVosServerClose, &wp, pStatus))
            rc = FALSE;
         }

      m_hVOS = NULL;
      m_hCellVOS = NULL;
      }

   return rc;
}


void SERVER::Invalidate (void)
{
   if (!m_fAggregatesOutOfDate || !m_fServicesOutOfDate || !m_fStatusOutOfDate)
      {
      if (m_wGhost & GHOST_HAS_SERVER_ENTRY)
         {
         m_fAggregatesOutOfDate = TRUE;
         m_fServicesOutOfDate = TRUE;
         m_fStatusOutOfDate = TRUE;
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


void SERVER::InvalidateStatus (void)
{
   if (!m_fStatusOutOfDate)
      {
      if (m_wGhost & GHOST_HAS_SERVER_ENTRY)
         {
         m_fStatusOutOfDate = TRUE;
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


void SERVER::InvalidateServices (void)
{
   if (!m_fServicesOutOfDate)
      {
      if (m_wGhost & GHOST_HAS_SERVER_ENTRY)
         {
         m_fServicesOutOfDate = TRUE;
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


BOOL SERVER::RefreshAggregates (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fAggregatesOutOfDate)
      {
      m_fAggregatesOutOfDate = FALSE;

      if (fIsMonitored())
         {
         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAggregatesBegin, GetIdentifier());

         // First thing is to forget about what aggregates we think we have
         // now.
         //
         LPENUM pEnum;
         for (pEnum = m_lAggregates->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
            {
            LPAGGREGATE lpAggregate = (LPAGGREGATE)(pEnum->GetObject());
            lpAggregate->SendDeleteNotifications();
            m_lAggregates->Remove (lpAggregate);
            Delete (lpAggregate);
            }

         // Next, the harder part: look through the server to find a list
         // of aggregates.
         //
         PVOID hCell;
         PVOID hVOS;
         if ((hVOS = OpenVosObject (&hCell, &status)) == NULL)
            rc = FALSE;
         else
            {
            WORKERPACKET wpBegin;
            wpBegin.wpVosPartitionGetBegin.hCell = hCell;
            wpBegin.wpVosPartitionGetBegin.hServer = hVOS;

            if (!Worker_DoTask (wtaskVosPartitionGetBegin, &wpBegin, &status))
               rc = FALSE;
            else
               {
               for (;;)
                  {
                  WORKERPACKET wpNext;
                  wpNext.wpVosPartitionGetNext.hEnum = wpBegin.wpVosPartitionGetBegin.hEnum;
                  if (!Worker_DoTask (wtaskVosPartitionGetNext, &wpNext, &status))
                     {
                     if (status == ADMITERATORDONE)
                        status = 0;
                     else
                        rc = FALSE;
                     break;
                     }

                  vos_partitionEntry_p pData = &wpNext.wpVosPartitionGetNext.Data;

                  LPTSTR pszName = AnsiToString (pData->name);
                  LPTSTR pszDevice = AnsiToString (pData->deviceName);

                  LPAGGREGATE lpAggregate = New2 (AGGREGATE,(this, pszName, pszDevice));

                  lpAggregate->m_as.dwID = lpAggregate->GetID();

                  FreeString (pszDevice, pData->deviceName);
                  FreeString (pszName,   pData->name);

                  lpAggregate->m_wGhost |= GHOST_HAS_SERVER_ENTRY;
                  lpAggregate->m_as.ckStorageTotal = pData->totalSpace;
                  lpAggregate->m_as.ckStorageFree = pData->totalFreeSpace;
                  m_lAggregates->Add (lpAggregate);

                  NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpAggregate->GetIdentifier());
                  }

               WORKERPACKET wpDone;
               wpDone.wpVosPartitionGetDone.hEnum = wpBegin.wpVosPartitionGetBegin.hEnum;
               Worker_DoTask (wtaskVosPartitionGetDone, &wpDone);
               }

            CloseVosObject();
            }

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAggregatesEnd, GetIdentifier(), ((rc) ? 0 : status));
         }
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


BOOL SERVER::RefreshServices (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fServicesOutOfDate)
      {
      m_fServicesOutOfDate = FALSE;

      if (fIsMonitored())
         {
         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServicesBegin, GetIdentifier());

         // First thing is to forget about what services we think we have now.
         //
         LPENUM pEnum;
         for (pEnum = m_lServices->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
            {
            LPSERVICE lpService = (LPSERVICE)(pEnum->GetObject());
            lpService->SendDeleteNotifications();
            m_lServices->Remove (lpService);
            Delete (lpService);
            }

         // Next, the harder part: look through the server to find a list
         // of services.
         //
         PVOID hCell;
         PVOID hBOS;
         if ((hBOS = OpenBosObject (&hCell, &status)) == NULL)
            rc = FALSE;
         else
            {
            WORKERPACKET wpBegin;
            wpBegin.wpBosProcessNameGetBegin.hServer = hBOS;
            if (!Worker_DoTask (wtaskBosProcessNameGetBegin, &wpBegin, &status))
               rc = FALSE;
            else
               {
               LPSERVICE lpService = New2 (SERVICE,(this, TEXT("BOS")));
               m_lServices->Add (lpService);
               NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpService->GetIdentifier());

               for (;;)
                  {
                  TCHAR szServiceName[ cchNAME ];

                  WORKERPACKET wpNext;
                  wpNext.wpBosProcessNameGetNext.hEnum = wpBegin.wpBosProcessNameGetBegin.hEnum;
                  wpNext.wpBosProcessNameGetNext.pszService = szServiceName;

                  if (!Worker_DoTask (wtaskBosProcessNameGetNext, &wpNext, &status))
                     {
                     if (status == ADMITERATORDONE)
                        status = 0;
                     else
                        rc = FALSE;
                     break;
                     }

                  lpService = New2 (SERVICE,(this, wpNext.wpBosProcessNameGetNext.pszService));
                  m_lServices->Add (lpService);
                  NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpService->GetIdentifier());
                  }

               WORKERPACKET wpDone;
               wpDone.wpBosProcessNameGetDone.hEnum = wpBegin.wpBosProcessNameGetBegin.hEnum;
               Worker_DoTask (wtaskBosProcessNameGetDone, &wpDone);
               }

            CloseBosObject();
            }

         if (fNotify)
            NOTIFYCALLBACK::SendNotificationToAll (evtRefreshServicesEnd, GetIdentifier(), ((rc) ? 0 : status));
         }
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


BOOL SERVER::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      LPCELL lpCell;
      if ((lpCell = OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         PVOID hCell;
         if ((hCell = lpCell->GetCellObject (&status)) == NULL)
            rc = FALSE;
         else
            {
            WORKERPACKET wp;
            wp.wpClientAFSServerGet.hCell = hCell;
            wp.wpClientAFSServerGet.pszServer = m_szName;

            if (!Worker_DoTask (wtaskClientAFSServerGet, &wp, &status))
               rc = FALSE;
            else
               {
               m_ss.nAddresses = 0;

               for (size_t iAddr = 0; iAddr < AFS_MAX_SERVER_ADDRESS; ++iAddr)
                  {
                  if (wp.wpClientAFSServerGet.Entry.serverAddress[ iAddr ] == 0)
                     continue;
                  AfsClass_IntToAddress (&m_ss.aAddresses[ m_ss.nAddresses++ ], wp.wpClientAFSServerGet.Entry.serverAddress[ iAddr ]);
                  }

               lpCell->m_lServers->Update (this); // That update affected a hashlistkey
               }
            }
         lpCell->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


void SERVER::GetName (LPTSTR pszName)
{
   SERVER::ShortenName (pszName, m_szName);
}


void SERVER::GetLongName (LPTSTR pszName)
{
   lstrcpy (pszName, m_szName);
}


LPCELL SERVER::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}

BOOL SERVER::GetStatus (LPSERVERSTATUS lpss, BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;

   if (m_fMonitor)
      rc = RefreshStatus (fNotify, pStatus);

   memcpy (lpss, &m_ss, sizeof(SERVERSTATUS));
   return rc;
}


short SERVER::GetGhostStatus (void)
{
   return m_wGhost;
}


PVOID SERVER::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void SERVER::SetUserParam (PVOID pUserNew)
{
   GetIdentifier()->SetUserParam (pUserNew);
}


void SERVER::ShortenName (LPTSTR pszTarget, LPTSTR pszSource, BOOL fForce)
{
   lstrcpy (pszTarget, pszSource);

   if (fForce || !fLongServerNames)
      {
      // If the name is really an IP address, don't shorten it.
      //
      BOOL fIsIPAddress = TRUE;
      for (LPTSTR pch = pszTarget; *pch && fIsIPAddress; ++pch)
         {
         if (!isdigit(*pch) && !(*pch == TEXT('.')))
            fIsIPAddress = FALSE;
         }

      if (!fIsIPAddress)
         {
         if ((pszTarget = (LPTSTR)lstrchr (pszTarget, TEXT('.'))) != NULL)
            *pszTarget = TEXT('\0');
         }
      }
}


BOOL SERVER::fIsMonitored (void)
{
   return m_fMonitor;
}


BOOL SERVER::SetMonitor (BOOL fShouldMonitor, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (m_fMonitor != fShouldMonitor)
      {
      LPCELL lpCell;
      if ((lpCell = m_lpiCell->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

         if ((m_fMonitor = fShouldMonitor) == FALSE)
            {
            FreeAll();
            (lpCell->m_nServersUnmonitored)++;
            }
         else // (fMonitor == TRUE)
            {
            (lpCell->m_nServersUnmonitored)--;
            Invalidate();
            rc = RefreshAll (&status);
            }

         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), m_lastStatus);
         lpCell->Close();
         }
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


/*
 * REFRESHALL __________________________________________________________________
 *
 * If a server is down, or doesn't have an VOS process running, it could
 * take some time before we time out trying to talk to the server.  During
 * the course of a refresh, the first timeout-and-fail that we will hit is
 * our call to wtaskVosPartitionGetBegin; since this call is very quick if
 * it's going to be successful, we can safely perform this call once up-front
 * to test to see if the server is listening at all.  That test is performed
 * on a separate thread, so that in the event the request times out, we can
 * simply discard the thread and let it terminate on its own.
 *
 */

typedef struct
   {
   BOOL fInUse;
   BOOL fCanceled;
   LPSERVER lpServer;
   PVOID hCell;
   } REFRESHSECTION, *LPREFRESHSECTION;

static REFRESHSECTION    *aRefSec = NULL;
static size_t             cRefSec = 0;
static LPCRITICAL_SECTION pcsRefSec = NULL;

void AfsClass_InitRefreshSections (void)
{
   if (pcsRefSec == NULL)
      {
      pcsRefSec = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsRefSec);
      }
}


void AfsClass_SkipRefresh (int idSection)
{
   AfsClass_InitRefreshSections();
   EnterCriticalSection (pcsRefSec);

   if (aRefSec && (idSection < (int)cRefSec))
      {
      if (aRefSec[ idSection ].fInUse)
         {
         aRefSec[ idSection ].fCanceled = TRUE;
         }
      }

   LeaveCriticalSection (pcsRefSec);
}


DWORD WINAPI SERVER::CanTalkToServer_ThreadProc (PVOID lp)
{
   int idSection = PtrToInt(lp);

   // Until we post a notification saying that we've entered
   // a section, we don't need to worry about the aRefSec[] entry
   // being invalid. Once that post is made, the user can skip
   // the section at any time--so we'll have to check frequently,
   // always under the pcsRefSec critical section.
   //
   NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllSectionStart, NULL, NULL, NULL, NULL, idSection, 0);

   BOOL fAggregatesOK = FALSE;
   BOOL fServicesOK = FALSE;
   BOOL fContinue = TRUE;

   // Try to get the BOS object for this server.  Remember, there's
   // a catch here: we can only assume that the aRefSec[idSection].lpServer
   // pointer is valid so long as we're within the pcsRefSec critical
   // section! (if we're in that critsec, we can verify that no one
   // has canceled the operation--and if no one has, there is a thread
   // hanging around which holds the library's critsec, which ensures
   // the lpServer pointer won't have been freed.)
   // 
   PVOID hCell;
   PVOID hBOS = NULL;
   PVOID hVOS = NULL;

   TCHAR szServer[ cchNAME ];

   EnterCriticalSection (pcsRefSec);
   if ( ((!aRefSec[ idSection ].fInUse) || (aRefSec[ idSection ].fCanceled)) )
      fContinue = FALSE;
   else
      {
      hCell = aRefSec[ idSection ].hCell;
      aRefSec[ idSection ].lpServer->GetLongName (szServer);
      }
   LeaveCriticalSection (pcsRefSec);

   if (fContinue)
      {
      WORKERPACKET wp;
      wp.wpBosServerOpen.hCell = hCell;
      wp.wpBosServerOpen.pszServer = szServer;

      ULONG status;
      fContinue = Worker_DoTask (wtaskBosServerOpen, &wp, &status);

      EnterCriticalSection (pcsRefSec);
      if ( ((!aRefSec[ idSection ].fInUse) || (aRefSec[ idSection ].fCanceled)) )
         fContinue = FALSE;
      else if (!fContinue)
         aRefSec[ idSection ].lpServer->m_lastStatus = status;
      else
         hBOS = wp.wpBosServerOpen.hServer;
      LeaveCriticalSection (pcsRefSec);
      }

   if (fContinue)
      {
      WORKERPACKET wpBegin;
      wpBegin.wpBosProcessNameGetBegin.hServer = hBOS;

      ULONG status;
      fContinue = Worker_DoTask (wtaskBosProcessNameGetBegin, &wpBegin, &status);

      EnterCriticalSection (pcsRefSec);
      if ( ((!aRefSec[ idSection ].fInUse) || (aRefSec[ idSection ].fCanceled)) )
         fContinue = FALSE;
      else if (fContinue)
         aRefSec[ idSection ].lpServer->m_lastStatus = status;
      LeaveCriticalSection (pcsRefSec);

      if (fContinue)
         {
         WORKERPACKET wpDone;
         wpDone.wpBosProcessNameGetDone.hEnum = wpBegin.wpBosProcessNameGetBegin.hEnum;
         Worker_DoTask (wtaskBosProcessNameGetDone, &wpDone);

         // We can talk to BOS!
         fServicesOK = TRUE;
         }
      }

   // If we couldn't talk to BOS, it's a sure bet the server is down--
   // and regardless, if BOS isn't around, VOS isn't either.  So
   // we may not even have to test that.
   //
   if (fContinue)
      {
      WORKERPACKET wp;
      wp.wpVosServerOpen.hCell = hCell;
      wp.wpVosServerOpen.pszServer = szServer;

      ULONG status;
      fContinue = Worker_DoTask (wtaskVosServerOpen, &wp, &status);

      EnterCriticalSection (pcsRefSec);
      if ( ((!aRefSec[ idSection ].fInUse) || (aRefSec[ idSection ].fCanceled)) )
         fContinue = FALSE;
      else if (!fContinue)
         aRefSec[ idSection ].lpServer->m_lastStatus = status;
      else
         hVOS = wp.wpVosServerOpen.hServer;
      LeaveCriticalSection (pcsRefSec);
      }

   if (fContinue)
      {
      WORKERPACKET wpBegin;
      wpBegin.wpVosPartitionGetBegin.hCell = hCell;
      wpBegin.wpVosPartitionGetBegin.hServer = hVOS;

      ULONG status;
      fContinue = Worker_DoTask (wtaskVosPartitionGetBegin, &wpBegin, &status);

      EnterCriticalSection (pcsRefSec);
      if ( ((!aRefSec[ idSection ].fInUse) || (aRefSec[ idSection ].fCanceled)) )
         fContinue = FALSE;
      else if (fContinue)
         aRefSec[ idSection ].lpServer->m_lastStatus = status;
      LeaveCriticalSection (pcsRefSec);

      if (fContinue)
         {
         WORKERPACKET wpDone;
         wpDone.wpVosPartitionGetDone.hEnum = wpBegin.wpVosPartitionGetBegin.hEnum;
         Worker_DoTask (wtaskVosPartitionGetDone, &wpDone);

         // We can talk to VOS!
         fAggregatesOK = TRUE;
         }
      }

   // Close the VOS and BOS objects we obtained.
   //
   if (hBOS)
      {
      WORKERPACKET wp;
      wp.wpBosServerClose.hServer = hBOS;
      Worker_DoTask (wtaskBosServerClose, &wp);
      }
   if (hVOS)
      {
      WORKERPACKET wp;
      wp.wpVosServerClose.hServer = hVOS;
      Worker_DoTask (wtaskVosServerClose, &wp);
      }

   // Return our entry in the RefSec array back to the pool.
   // If the request was never canceled, there is another
   // thread waiting to hear our results--update the server
   // entry specified by RefSec before leaving.
   //
   NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllSectionEnd, NULL, NULL, NULL, NULL, idSection, 0);

   EnterCriticalSection (pcsRefSec);
   if ( (aRefSec[ idSection ].fInUse) && (!aRefSec[ idSection ].fCanceled) )
      {
      aRefSec[ idSection ].lpServer->m_fCanGetAggregates = fAggregatesOK;
      aRefSec[ idSection ].lpServer->m_fCanGetServices = fServicesOK;
      }
   aRefSec[ idSection ].fInUse = FALSE;
   LeaveCriticalSection (pcsRefSec);
   return 1;
}


BOOL SERVER::CanTalkToServer (ULONG *pStatus)
{
   // Ensure the server exists in the cell at all--
   // this call just updates the server's IP addresses
   // etc (information it gets from the database servers)
   // and doesn't require talking to the server itself.
   //
   if (!RefreshStatus (FALSE, pStatus))
      return FALSE;

   // Find a new refsec array element to use...
   //
   AfsClass_InitRefreshSections();
   EnterCriticalSection (pcsRefSec);

   int idSection;
   for (idSection = 0; idSection < (int)cRefSec; ++idSection)
      {
      if (!aRefSec[ idSection ].fInUse)
         break;
      }
   if (idSection == (int)cRefSec)
      {
      if (!REALLOC (aRefSec, cRefSec, 1+idSection, 4))
         {
         if (pStatus)
            *pStatus = GetLastError();
         LeaveCriticalSection (pcsRefSec);
         return FALSE;
         }
      }
   aRefSec[ idSection ].fInUse = TRUE;
   aRefSec[ idSection ].fCanceled = FALSE;
   aRefSec[ idSection ].lpServer = this;
   aRefSec[ idSection ].hCell = NULL;

   LPCELL lpCell;
   if ((lpCell = OpenCell()) != NULL)
      {
      aRefSec[ idSection ].hCell = lpCell->GetCellObject();
      lpCell->Close();
      }

   LeaveCriticalSection (pcsRefSec);

   // Until we find out differently, assume that we won't be
   // able to query VOS or BOS on this server.
   //
   m_fCanGetAggregates = FALSE;
   m_fCanGetServices = FALSE;
   m_lastStatus = 0;

   // Fork a separate thread, on which to quickly try to talk
   // to the server.
   //
   DWORD dwThreadID;
   HANDLE hThread;
   if ((hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)CanTalkToServer_ThreadProc, IntToPtr(idSection), 0, &dwThreadID)) == NULL)
      {
      EnterCriticalSection (pcsRefSec);
      aRefSec[ idSection ].fInUse = FALSE;
      LeaveCriticalSection (pcsRefSec);
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   SetThreadPriority (hThread, THREAD_PRIORITY_BELOW_NORMAL);

   // Wait for that thread to terminate, or for our
   // newly-allocated RefSec entry to be marked Canceled.
   //
   DWORD dw;
   for (dw = STILL_ACTIVE; dw == STILL_ACTIVE; )
      {
      EnterCriticalSection (pcsRefSec);

      GetExitCodeThread (hThread, &dw);
      if (dw == STILL_ACTIVE)
         {
         if ( (aRefSec[ idSection ].fInUse) &&
              (aRefSec[ idSection ].lpServer == this) &&
              (aRefSec[ idSection ].fCanceled) )
            {
            if (m_lastStatus == 0)
               m_lastStatus = ERROR_CANCELLED;
            dw = 0;
            }
         }

      LeaveCriticalSection (pcsRefSec);

      if (dw == STILL_ACTIVE)
         Sleep(100);	// wait another brief instant
      }

   // dw == 0 : user canceled operation (thread is still running!)
   // dw == 1 : thread completed successfully, and set fCanTalkTo* flags.
   //
   // Note that the thread will clear aRefSec[idSection].fInUse when it
   // terminates (so, if dw!=-1, it has already done so).
   //
   if (pStatus)
      *pStatus = m_lastStatus;
   return (dw == 0) ? FALSE : TRUE;
}


BOOL SERVER::RefreshAll (ULONG *pStatus, double dInit, double dFactor)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (m_fAggregatesOutOfDate || m_fServicesOutOfDate)
      {
      if ((++cRefreshAllReq) == 1)
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllBegin, GetIdentifier(), 0);
         }

      double perAGGREGATES = 65.0; // % of time spent finding aggs & sets
      double perSERVICES   = 25.0; // % of time spent finding services
      double perVLDB       = 10.0; // % of time spent finding VLDB info

      if (cRefreshAllReq >= 2) // being called as part of a cell-wide op?
         {
         perAGGREGATES = 80.0; // % of time spent finding aggs & sets
         perSERVICES   = 20.0; // % of time spent finding services
         perVLDB       = 0.0;  // we won't query VLDB stuff ourself.
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      if (!CanTalkToServer (&status))  // Determines fCanGetAggregates, fCanGetServices
         {
         if (m_fMonitor)
            SetMonitor (FALSE);
         rc = FALSE;
         }
      else
         {
         if (!m_fCanGetAggregates)
            {
            FreeAggregates();
            m_fAggregatesOutOfDate = FALSE;
            }
         else
            {
            size_t nAggregates = 0;
            size_t iAggregate = 0;
            HENUM hEnum;
            LPAGGREGATE lpAggregate;
            for (lpAggregate = AggregateFindFirst (&hEnum); lpAggregate; lpAggregate = AggregateFindNext (&hEnum))
               {
               ++nAggregates;
               lpAggregate->Close();
               }
               
            if (nAggregates)
               {
               for (lpAggregate = AggregateFindFirst (&hEnum); lpAggregate; lpAggregate = AggregateFindNext (&hEnum))
                  {
                  ULONG perComplete = (ULONG)( ((double)perAGGREGATES / 100.0) * ((double)iAggregate * 100.0 / nAggregates) );
                  NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllUpdate, lpAggregate->GetIdentifier(), NULL, NULL, NULL, (ULONG)( 100.0 * dInit + dFactor * (double)perComplete ), 0);

                  lpAggregate->RefreshFilesets (TRUE);
                  lpAggregate->Close();

                  ++iAggregate;
                  }
               }
            }

         if (!m_fCanGetServices)
            {
            FreeServices();
            m_fServicesOutOfDate = FALSE;
            }
         else
            {
            size_t nServices = 0;
            size_t iService = 0;
            HENUM hEnum;
            LPSERVICE lpService;
            for (lpService = ServiceFindFirst (&hEnum); lpService; lpService = ServiceFindNext (&hEnum))
               {
               ++nServices;
               lpService->Close();
               }

            if (nServices)
               {
               for (lpService = ServiceFindFirst (&hEnum); lpService; lpService = ServiceFindNext (&hEnum))
                  {
                  ULONG perComplete = (ULONG)( (double)perAGGREGATES + ((double)perSERVICES / 100.0) * ((double)iService * 100.0 / nServices) );
                  NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllUpdate, lpService->GetIdentifier(), NULL, NULL, NULL, (ULONG)( 100.0 * dInit + dFactor * (double)perComplete ), 0);

                  lpService->RefreshStatus (TRUE);
                  lpService->Close();

                  ++iService;
                  }
               }
            }

         if (cRefreshAllReq == 1) // not being called as part of a cell-wide op?
            {
            LPCELL lpCell;
            if ((lpCell = OpenCell()) != NULL)
               {
               lpCell->RefreshVLDB (GetIdentifier(), TRUE, NULL);
               lpCell->Close();
               }
            }
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), m_lastStatus);

      if ((--cRefreshAllReq) == 0)
         {
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshAllEnd, GetIdentifier(), NULL, NULL, NULL, 100, m_lastStatus);
         }
      }

   if (rc && m_lastStatus)
      rc = FALSE;
   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


/*
 * AGGREGATES _________________________________________________________________
 *
 */

LPAGGREGATE SERVER::OpenAggregate (LPTSTR pszName, ULONG *pStatus)
{
   if (!RefreshAggregates (TRUE, pStatus))
      return NULL;

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = (LPAGGREGATE)(m_lkAggregateName->GetFirstObject (pszName))) != NULL)
      AfsClass_Enter();

   return lpAggregate;
}


LPAGGREGATE SERVER::OpenAggregate (ULONG dwID, ULONG *pStatus)
{
   if (!RefreshAggregates (TRUE, pStatus))
      return NULL;

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = (LPAGGREGATE)(m_lkAggregateID->GetFirstObject (&dwID))) != NULL)
      AfsClass_Enter();

   return lpAggregate;
}


LPAGGREGATE SERVER::AggregateFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return AggregateFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPAGGREGATE SERVER::AggregateFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPAGGREGATE lpAggregate = NULL;

   if (!RefreshAggregates (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpAggregate = lpiFind->OpenAggregate();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lAggregates->FindFirst()) != NULL)
      {
      lpAggregate = (LPAGGREGATE)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpAggregate && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpAggregate;
}


LPAGGREGATE SERVER::AggregateFindNext (HENUM *phEnum)
{
   LPAGGREGATE lpAggregate = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpAggregate = (LPAGGREGATE)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpAggregate;
}


void SERVER::AggregateFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}


/*
 * SERVICES ___________________________________________________________________
 *
 */

LPSERVICE SERVER::OpenService (LPTSTR pszName, ULONG *pStatus)
{
   if (!RefreshServices (TRUE, pStatus))
      return NULL;

   LPSERVICE lpService;
   if ((lpService = (LPSERVICE)(m_lkServiceName->GetFirstObject (pszName))) != NULL)
      AfsClass_Enter();

   return lpService;
}


LPSERVICE SERVER::ServiceFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return ServiceFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPSERVICE SERVER::ServiceFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPSERVICE lpService = NULL;

   if (!RefreshServices (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpService = lpiFind->OpenService();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lServices->FindFirst()) != NULL)
      {
      lpService = (LPSERVICE)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpService && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpService;
}


LPSERVICE SERVER::ServiceFindNext (HENUM *phEnum)
{
   LPSERVICE lpService = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpService = (LPSERVICE)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpService;
}


void SERVER::ServiceFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}


/*
 * HASH KEYS __________________________________________________________________
 *
 */

BOOL CALLBACK SERVER::KeyAggregateName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmp (((LPAGGREGATE)pObject)->m_szName, (LPTSTR)pData);
}

HASHVALUE CALLBACK SERVER::KeyAggregateName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return SERVER::KeyAggregateName_HashData (pKey, ((LPAGGREGATE)pObject)->m_szName);
}

HASHVALUE CALLBACK SERVER::KeyAggregateName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


BOOL CALLBACK SERVER::KeyAggregateID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((LPAGGREGATE)pObject)->m_as.dwID == *(ULONG*)pData);
}

HASHVALUE CALLBACK SERVER::KeyAggregateID_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return SERVER::KeyAggregateID_HashData (pKey, &((LPAGGREGATE)pObject)->m_as.dwID);
}

HASHVALUE CALLBACK SERVER::KeyAggregateID_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*(ULONG*)pData;
}


BOOL CALLBACK SERVER::KeyServiceName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmp (((LPSERVICE)pObject)->m_szName, (LPTSTR)pData);
}

HASHVALUE CALLBACK SERVER::KeyServiceName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return SERVER::KeyServiceName_HashData (pKey, ((LPSERVICE)pObject)->m_szName);
}

HASHVALUE CALLBACK SERVER::KeyServiceName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}

