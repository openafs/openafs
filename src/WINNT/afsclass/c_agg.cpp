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


AGGREGATE::AGGREGATE (LPSERVER lpServerParent, LPTSTR pszName, LPTSTR pszDevice)
{
   m_lpiServer = lpServerParent->GetIdentifier();
   m_lpiCell = lpServerParent->m_lpiCell;
   m_lpiThis = NULL;

   lstrcpy (m_szName, pszName);
   lstrcpy (m_szDevice, pszDevice);
   m_wGhost = 0;
   m_idPartition = NO_PARTITION;

   m_fFilesetsOutOfDate = TRUE;
   m_lFilesets = New (HASHLIST);
   m_lFilesets->SetCriticalSection (AfsClass_GetCriticalSection());
   m_lkFilesetName = m_lFilesets->CreateKey ("Fileset Name", AGGREGATE::KeyFilesetName_Compare, AGGREGATE::KeyFilesetName_HashObject, AGGREGATE::KeyFilesetName_HashData);
   m_lkFilesetID = m_lFilesets->CreateKey ("Fileset ID", AGGREGATE::KeyFilesetID_Compare, AGGREGATE::KeyFilesetID_HashObject, AGGREGATE::KeyFilesetID_HashData);

   m_fStatusOutOfDate = TRUE;
   m_fAllocationOutOfDate = TRUE;
   memset (&m_as, 0x00, sizeof(AGGREGATESTATUS));
}


AGGREGATE::~AGGREGATE (void)
{
   for (LPENUM pEnum = m_lFilesets->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
      {
      LPFILESET lpFileset = (LPFILESET)(pEnum->GetObject());
      m_lFilesets->Remove (lpFileset);
      Delete (lpFileset);
      }
   Delete (m_lFilesets);

   if (m_lpiThis)
      m_lpiThis->m_cRef --;
}


void AGGREGATE::SendDeleteNotifications (void)
{
   for (LPENUM pEnum = m_lFilesets->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPFILESET lpFileset = (LPFILESET)(pEnum->GetObject());
      lpFileset->SendDeleteNotifications();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void AGGREGATE::Close (void)
{
   AfsClass_Leave();
}


LPIDENT AGGREGATE::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


void AGGREGATE::Invalidate (void)
{
   if (!m_fStatusOutOfDate || !m_fFilesetsOutOfDate)
      {
      if (m_wGhost & GHOST_HAS_SERVER_ENTRY)
         {
         m_fStatusOutOfDate = TRUE;
         m_fFilesetsOutOfDate = TRUE;
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


void AGGREGATE::InvalidateAllocation (void)
{
   m_fAllocationOutOfDate = TRUE;
}


BOOL AGGREGATE::RefreshFilesets (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fFilesetsOutOfDate)
      {
      m_fFilesetsOutOfDate = FALSE;

      // We'll need m_as.dwID to do this.
      //
      if (!RefreshStatus (fNotify, pStatus))
         return FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshFilesetsBegin, GetIdentifier());

      // First thing is to forget about what filesets we think we have now.
      //
      for (LPENUM pEnum = m_lFilesets->FindLast(); pEnum; pEnum = pEnum->FindPrevious())
         {
         LPFILESET lpFileset = (LPFILESET)(pEnum->GetObject());
         lpFileset->SendDeleteNotifications();
         m_lFilesets->Remove (lpFileset);
         Delete (lpFileset);
         }

      // Next, the harder part: look through the server to find a list
      // of filesets.
      //
      LPSERVER lpServer;
      if ((lpServer = OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         PVOID hCell;
         PVOID hVOS;
         if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
            rc = FALSE;
         else
            {
            WORKERPACKET wpBegin;
            wpBegin.wpVosVolumeGetBegin.hCell = hCell;
            wpBegin.wpVosVolumeGetBegin.hServer = hVOS;
            wpBegin.wpVosVolumeGetBegin.idPartition = m_idPartition;

            if (!Worker_DoTask (wtaskVosVolumeGetBegin, &wpBegin, &status))
               rc = FALSE;
            else
               {
               for (;;)
                  {
                  WORKERPACKET wpNext;
                  wpNext.wpVosVolumeGetNext.hEnum = wpBegin.wpVosVolumeGetBegin.hEnum;
                  if (!Worker_DoTask (wtaskVosVolumeGetNext, &wpNext, &status))
                     {
                     if (status == ADMITERATORDONE)
                        status = 0;
                     else
                        rc = FALSE;
                     break;
                     }

                  LPTSTR pszName = AnsiToString (wpNext.wpVosVolumeGetNext.Data.name);
                  LPFILESET lpFileset = New2 (FILESET,(this, &wpNext.wpVosVolumeGetNext.Data.id, pszName));
                  FreeString (pszName, wpNext.wpVosVolumeGetNext.Data.name);

                  lpFileset->SetStatusFromVOS (&wpNext.wpVosVolumeGetNext.Data);
                  lpFileset->m_wGhost |= GHOST_HAS_SERVER_ENTRY;
                  m_lFilesets->Add (lpFileset);
                  NOTIFYCALLBACK::SendNotificationToAll (evtCreate, lpFileset->GetIdentifier());
                  }

               WORKERPACKET wpDone;
               wpDone.wpVosVolumeGetDone.hEnum = wpBegin.wpVosVolumeGetBegin.hEnum;
               Worker_DoTask (wtaskVosVolumeGetDone, &wpDone);
               }

            lpServer->CloseVosObject();
            }

         lpServer->Close();
         }

      InvalidateAllocation();

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshFilesetsEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


BOOL AGGREGATE::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      LPSERVER lpServer;
      if ((lpServer = OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         PVOID hCell;
         PVOID hVOS;
         if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
            rc = FALSE;
         else
            {
            WORKERPACKET wp;
            wp.wpVosPartitionGet.hCell = hCell;
            wp.wpVosPartitionGet.hServer = hVOS;
            wp.wpVosPartitionGet.idPartition = m_idPartition;

            if (!Worker_DoTask (wtaskVosPartitionGet, &wp, &status))
               rc = FALSE;
            else
               {
               m_as.ckStorageTotal = wp.wpVosPartitionGet.Data.totalSpace;
               m_as.ckStorageFree = wp.wpVosPartitionGet.Data.totalFreeSpace;
               }

            m_as.dwID = GetID();
            lpServer->CloseVosObject();
            }

         lpServer->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (rc && m_fAllocationOutOfDate)
      {
      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      m_as.ckStorageAllocated = CalculateAllocation (fNotify);
      m_fAllocationOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier());
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


size_t AGGREGATE::CalculateAllocation (BOOL fNotify)
{
   size_t ckAllocated = 0;

   for (LPENUM pEnum = m_lFilesets->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPFILESET lpFileset = (LPFILESET)(pEnum->GetObject());

      FILESETSTATUS fs;
      if (lpFileset->GetStatus (&fs))
         {
         if (fs.Type == ftREADWRITE)
            ckAllocated += fs.ckQuota;
         }
      }

   return ckAllocated;
}


void AGGREGATE::GetName (LPTSTR pszName)
{
   lstrcpy (pszName, m_szName);
}


void AGGREGATE::GetDevice (LPTSTR pszDevice)
{
   lstrcpy (pszDevice, m_szDevice);
}


LPCELL AGGREGATE::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}


LPSERVER AGGREGATE::OpenServer (ULONG *pStatus)
{
   return m_lpiServer->OpenServer (pStatus);
}


/*
 * FILESETS ___________________________________________________________________
 *
 */

LPFILESET AGGREGATE::OpenFileset (LPTSTR pszName, ULONG *pStatus)
{
   if (!RefreshFilesets (TRUE, pStatus))
      return NULL;

   LPFILESET lpFileset;
   if ((lpFileset = (LPFILESET)(m_lkFilesetName->GetFirstObject (pszName))) != NULL)
      AfsClass_Enter();

   return lpFileset;
}


LPFILESET AGGREGATE::OpenFileset (VOLUMEID *pvidFileset, ULONG *pStatus)
{
   if (!RefreshFilesets (TRUE, pStatus))
      return NULL;

   LPFILESET lpFileset;
   if ((lpFileset = (LPFILESET)(m_lkFilesetID->GetFirstObject (pvidFileset))) != NULL)
      AfsClass_Enter();

   return lpFileset;
}


LPFILESET AGGREGATE::FilesetFindFirst (HENUM *phEnum, BOOL fNotify, ULONG *pStatus)
{
   return FilesetFindFirst (phEnum, NULL, fNotify, pStatus);
}


LPFILESET AGGREGATE::FilesetFindFirst (HENUM *phEnum, LPIDENT lpiFind, BOOL fNotify, ULONG *pStatus)
{
   LPFILESET lpFileset = NULL;

   if (!RefreshFilesets (fNotify, pStatus))
      return NULL;

   if (lpiFind != NULL)
      {
      lpFileset = lpiFind->OpenFileset();
      *phEnum = NULL;
      }
   else if ((*phEnum = m_lFilesets->FindFirst()) != NULL)
      {
      lpFileset = (LPFILESET)( (*phEnum)->GetObject() );
      AfsClass_Enter();
      }

   if (!lpFileset && pStatus)
      *pStatus = ERROR_FILE_NOT_FOUND;
   return lpFileset;
}


LPFILESET AGGREGATE::FilesetFindNext (HENUM *phEnum)
{
   LPFILESET lpFileset = NULL;

   if (*phEnum)
      {
      if ((*phEnum = (*phEnum)->FindNext()) != NULL)
         {
         lpFileset = (LPFILESET)( (*phEnum)->GetObject() );
         AfsClass_Enter();
         }
      }

   return lpFileset;
}


void AGGREGATE::FilesetFindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}


BOOL AGGREGATE::GetStatus (LPAGGREGATESTATUS lpas, BOOL fNotify, ULONG *pStatus)
{
   if (!RefreshStatus (fNotify, pStatus))
      return FALSE;

   memcpy (lpas, &m_as, sizeof(AGGREGATESTATUS));
   return TRUE;
}


short AGGREGATE::GetGhostStatus (void)
{
   return m_wGhost;
}


PVOID AGGREGATE::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void AGGREGATE::SetUserParam (PVOID pUserNew)
{
   GetIdentifier()->SetUserParam (pUserNew);
}


int AGGREGATE::GetID (void)
{
   if (m_idPartition == NO_PARTITION)
      {
      WORKERPACKET wp;
      wp.wpVosPartitionNameToId.pszPartition = m_szName;

      ULONG status;
      if (Worker_DoTask (wtaskVosPartitionNameToId, &wp, &status))
         m_idPartition = wp.wpVosPartitionNameToId.idPartition;
      }

   return m_idPartition;
}


/*
 * HASH KEYS __________________________________________________________________
 *
 */

BOOL CALLBACK AGGREGATE::KeyFilesetName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmp (((LPFILESET)pObject)->m_szName, (LPTSTR)pData);
}

HASHVALUE CALLBACK AGGREGATE::KeyFilesetName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return AGGREGATE::KeyFilesetName_HashData (pKey, ((LPFILESET)pObject)->m_szName);
}

HASHVALUE CALLBACK AGGREGATE::KeyFilesetName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


BOOL CALLBACK AGGREGATE::KeyFilesetID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !memcmp (&((LPFILESET)pObject)->m_idVolume, (LPVOLUMEID)pData, sizeof(VOLUMEID));
}

HASHVALUE CALLBACK AGGREGATE::KeyFilesetID_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return AGGREGATE::KeyFilesetID_HashData (pKey, &((LPFILESET)pObject)->m_idVolume);
}

HASHVALUE CALLBACK AGGREGATE::KeyFilesetID_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*((LPVOLUMEID)pData);
}

