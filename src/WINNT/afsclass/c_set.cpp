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


FILESET::FILESET (LPAGGREGATE lpAggregateParent, LPVOLUMEID pvid, LPTSTR pszName)
{
   m_lpiAggregate = lpAggregateParent->GetIdentifier();
   m_lpiServer = lpAggregateParent->m_lpiServer;
   m_lpiCell = lpAggregateParent->m_lpiCell;
   m_lpiThis = NULL;
   m_lpiThisRW = NULL;
   m_lpiThisBK = NULL;
   m_idVolume = *pvid;

   lstrcpy (m_szName, pszName);
   m_wGhost = 0;

   m_fStatusOutOfDate = TRUE;
   memset (&m_fs, 0x00, sizeof(FILESETSTATUS));
   m_fs.id = *pvid;

   lpAggregateParent->InvalidateAllocation();
}


FILESET::~FILESET (void)
{
   if (m_lpiThis)
      m_lpiThis->m_cRef --;
}


void FILESET::SendDeleteNotifications (void)
{
   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void FILESET::Close (void)
{
   AfsClass_Leave();
}


LPIDENT FILESET::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      TCHAR szCell[ cchNAME ];
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      m_lpiAggregate->GetCellName (szCell);
      m_lpiAggregate->GetLongServerName (szServer);
      m_lpiAggregate->GetAggregateName (szAggregate);

      // Finding the identifier for a fileset is a little tricky, because
      // (a) a fileset identifier's unique "key" includes the fileset's
      // aggregate, and (b) filesets can move around in the cell.
      //
      // We'll search through our list of IDENTs and see if we can find
      // an old IDENT object that refers to this fileset; if we can't find
      // one, we'll create a new one. To make sure we have an accurate match,
      // we'll require that the IDENT we find match all of the following
      // criteria:
      //    1- The identifier must point to a fileset
      //    2- The identifier must have the same fileset ID as this FILESET
      //    3- The identifier's cRef must be zero (i.e., there should
      //       not be another FILESET object out there which thinks *it*
      //       uses that IDENT)
      //    4- The identifier must (obviously) point to the cell in which
      //       this FILESET object resides
      //    5- If this is a fileset replica, the IDENT must be on the same
      //       aggregate as this fileset
      //
      // Note that the IDENT class maintains its list of IDENTs in a
      // HASHLIST that a key placed on volume IDs. We'll use that key
      // to speed up our search enormously.
      //
      BOOL fRequireSameAggregate = ProbablyReplica();

      for (LPENUM pEnum = IDENT::x_lkFilesetID->FindFirst (&m_idVolume); pEnum; pEnum = pEnum->FindNext())
         {

         // Only volumes which match this fileset ID will get here.
         //
         LPIDENT lpiFind = (LPIDENT)( pEnum->GetObject() );

         if (lpiFind->m_iType != itFILESET)
            continue;
         if (lpiFind->m_cRef != 0)
            continue;
         if (lstrcmpi (szCell, lpiFind->m_szCell))
            continue;
         if (fRequireSameAggregate)
            {
            if (lstrcmpi (szServer, lpiFind->m_szServer))
               continue;
            if (lstrcmpi (szAggregate, lpiFind->m_szAggregate))
               continue;
            }

         // Found a match! Update the IDENT's name and location,
         // to ensure it jives with reality... for instance, if
         // a fileset has been moved, we'll need to fix the
         // server and aggregate names. Since this affects one
         // of the keys in the IDENT class's hashlist, update that list.
         //
         Delete (pEnum);
         m_lpiThis = lpiFind;
         lstrcpy (m_lpiThis->m_szServer, szServer);
         lstrcpy (m_lpiThis->m_szAggregate, szAggregate);
         lstrcpy (m_lpiThis->m_szFileset, m_szName);
         m_lpiThis->Update();
         break;
         }

      if (m_lpiThis == NULL)
         m_lpiThis = New2 (IDENT,(this));  // Create a new IDENT if necessary.

      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


LPIDENT FILESET::GetReadWriteIdentifier (ULONG *pStatus)
{
   if (m_lpiThisRW == NULL)
      {
      if (RefreshStatus (TRUE, pStatus))
         {
         if ((m_lpiThisRW = IDENT::FindIdent (this, &m_fs.idReadWrite)) == NULL)
            {
            if (pStatus)
               *pStatus = ERROR_FILE_NOT_FOUND;
            }
         }
      }

   return m_lpiThisRW;
}


LPIDENT FILESET::GetReadOnlyIdentifier (LPIDENT lpiParent, ULONG *pStatus)
{
   LPIDENT lpiRO = NULL;

   if (RefreshStatus (TRUE, pStatus))
      {
      if (lpiParent == NULL)
         lpiParent = m_lpiCell;

      if (m_fs.idReplica)
         {
         lpiRO = IDENT::FindFileset (lpiParent, &m_fs.idReplica);
         }
      else
         {
         TCHAR szNameRO[ cchNAME ];
         lstrcpy (szNameRO, m_szName);

         LPCTSTR pszExtension;
         if ((pszExtension = lstrrchr (szNameRO, TEXT('.'))) == NULL)
            lstrcat (szNameRO, TEXT(".readonly"));
         else if (lstrcmpi (pszExtension, TEXT(".readonly")) && lstrcmpi (pszExtension, TEXT(".backup")))
            lstrcat (szNameRO, TEXT(".readonly"));

         lpiRO = IDENT::FindFileset (lpiParent, szNameRO);
         }

      if (lpiRO == NULL)
         {
         if (pStatus)
            *pStatus = ERROR_FILE_NOT_FOUND;
         }
      }

   return lpiRO;
}


LPIDENT FILESET::GetCloneIdentifier (ULONG *pStatus)
{
   if (m_lpiThisBK == NULL)
      {
      if (RefreshStatus (TRUE, pStatus))
         {
         if ((m_lpiThisBK = IDENT::FindIdent (this, &m_fs.idClone)) == NULL)
            {
            if (pStatus)
               *pStatus = ERROR_FILE_NOT_FOUND;
            }
         }
      }

   return m_lpiThisBK;
}


void FILESET::Invalidate (void)
{
   if (!m_fStatusOutOfDate)
      {
      if (m_wGhost & GHOST_HAS_SERVER_ENTRY)
         m_fStatusOutOfDate = TRUE;

      LPAGGREGATE lpAggregate;
      if ((lpAggregate = m_lpiAggregate->OpenAggregate()) != NULL)
         {
         lpAggregate->InvalidateAllocation();
         lpAggregate->Close();
         }

      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


BOOL FILESET::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fStatusOutOfDate && (m_wGhost & GHOST_HAS_SERVER_ENTRY))
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
            wp.wpVosVolumeGet.hCell = hCell;
            wp.wpVosVolumeGet.hServer = hVOS;
            wp.wpVosVolumeGet.idPartition = NO_PARTITION;
            wp.wpVosVolumeGet.idVolume = m_idVolume;

            LPAGGREGATE lpAggregate;
            if ((lpAggregate = m_lpiAggregate->OpenAggregate()) != NULL)
               {
               wp.wpVosVolumeGet.idPartition = lpAggregate->GetID();
               lpAggregate->Close();
               }

            if (!Worker_DoTask (wtaskVosVolumeGet, &wp, &status))
               rc = FALSE;
            else
               {
               SetStatusFromVOS (&wp.wpVosVolumeGet.Data);

               if ((lpAggregate = m_lpiAggregate->OpenAggregate()) != NULL)
                  {
                  lpAggregate->InvalidateAllocation();
                  lpAggregate->Close();
                  }
               }

            lpServer->CloseVosObject();
            }

         lpServer->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


void FILESET::GetName (LPTSTR pszName)
{
   lstrcpy (pszName, m_szName);
}


void FILESET::GetID (LPVOLUMEID pvid)
{
   *pvid = m_idVolume;
}


LPCELL FILESET::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}


LPSERVER FILESET::OpenServer (ULONG *pStatus)
{
   return m_lpiServer->OpenServer (pStatus);
}


LPAGGREGATE FILESET::OpenAggregate (ULONG *pStatus)
{
   return m_lpiAggregate->OpenAggregate (pStatus);
}


BOOL FILESET::GetStatus (LPFILESETSTATUS lpfs, BOOL fNotify, ULONG *pStatus)
{
   if (!RefreshStatus (fNotify, pStatus))
      return FALSE;

   memcpy (lpfs, &m_fs, sizeof(FILESETSTATUS));
   return TRUE;
}


short FILESET::GetGhostStatus (void)
{
   return m_wGhost;
}


PVOID FILESET::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void FILESET::SetUserParam (PVOID pUserNew)
{
   GetIdentifier()->SetUserParam (pUserNew);
}


BOOL FILESET::ProbablyReplica (void)
{
   LPCTSTR pszExtension;

   if ((pszExtension = lstrrchr (m_szName, TEXT('.'))) == NULL)
      return FALSE;

   if (lstrcmpi (pszExtension, TEXT(".readonly")))
      return FALSE;

   return TRUE;
}


void FILESET::SetStatusFromVOS (PVOID lp)
{
   vos_volumeEntry_p pEntry = (vos_volumeEntry_p)lp;

   m_fs.id = m_idVolume;
   m_fs.idReadWrite = pEntry->readWriteId;
   m_fs.idReplica = pEntry->readOnlyId;
   m_fs.idClone = pEntry->backupId;
   AfsClass_UnixTimeToSystemTime (&m_fs.timeCreation,   pEntry->creationDate);
   AfsClass_UnixTimeToSystemTime (&m_fs.timeLastUpdate, pEntry->lastUpdateDate);
   AfsClass_UnixTimeToSystemTime (&m_fs.timeLastAccess, pEntry->lastAccessDate);
   AfsClass_UnixTimeToSystemTime (&m_fs.timeLastBackup, pEntry->lastBackupDate);
   AfsClass_UnixTimeToSystemTime (&m_fs.timeCopyCreation, pEntry->copyCreationDate);
   m_fs.nFiles = pEntry->fileCount;
   m_fs.ckQuota = pEntry->maxQuota;
   m_fs.ckUsed = pEntry->currentSize;

   switch (pEntry->type)
      {
      case VOS_BACKUP_VOLUME:
         m_fs.Type = ftCLONE;
         m_fs.idClone = pEntry->id;
         break;
      case VOS_READ_ONLY_VOLUME:
         m_fs.Type = ftREPLICA;
         m_fs.idReplica = pEntry->id;
         break;
      case VOS_READ_WRITE_VOLUME:
         m_fs.Type = ftREADWRITE;
         m_fs.idReadWrite = pEntry->id;
         break;
      }

   m_fs.State &= fsMASK_VLDB;

   switch (pEntry->status)
      {
      case VOS_SALVAGE:
         m_fs.State |= fsSALVAGE;
         break;
      case VOS_NO_VNODE:
         m_fs.State |= fsNO_VNODE;
         break;
      case VOS_NO_VOL:
         m_fs.State |= fsNO_VOL;
         break;
      case VOS_NO_SERVICE:
         m_fs.State |= fsNO_SERVICE;
         break;
      case VOS_OFFLINE:
         m_fs.State |= fsOFFLINE;
         break;
      case VOS_DISK_FULL:
         m_fs.State |= fsDISK_FULL;
         break;
      case VOS_OVER_QUOTA:
         m_fs.State |= fsOVER_QUOTA;
         break;
      case VOS_BUSY:
         m_fs.State |= fsBUSY;
         break;
      case VOS_MOVED:
         m_fs.State |= fsMOVED;
         break;
      }
}

