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

#define cREALLOC_ADMINLISTENTRIES   32

#define cREALLOC_HOSTLISTENTRIES    16

#define cREALLOC_SERVERKEYS         16

#define ACCOUNTACCESS_TO_USERACCESS(_aa) ( ((_aa) == aaOWNER_ONLY) ? PTS_USER_OWNER_ACCESS : PTS_USER_ANYUSER_ACCESS )

#define ACCOUNTACCESS_TO_GROUPACCESS(_aa) ( ((_aa) == aaOWNER_ONLY) ? PTS_GROUP_OWNER_ACCESS : ((_aa) == aaGROUP_ONLY) ? PTS_GROUP_ACCESS : PTS_GROUP_ANYUSER_ACCESS )


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL AfsClass_GetServerLogFile (LPIDENT lpiServer, LPTSTR pszLocal, LPTSTR pszRemote, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtGetServerLogFileBegin, lpiServer, pszRemote, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosLogGet.hServer = hBOS;
      wp.wpBosLogGet.pszLogName = pszRemote;
      wp.wpBosLogGet.pszLogData = NULL;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskBosLogGet, &wp, &status)) == TRUE)
         {
         HANDLE fh;
         if ((fh = CreateFile (pszLocal, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL)) != INVALID_HANDLE_VALUE)
            {
            // Write the file a line at a time in order to make
            // sure that each line ends with "\r\n".  If we encounter
            // a line which ends in \r\n already, well, leave it alone.
            //
            for (LPTSTR psz = wp.wpBosLogGet.pszLogData; psz && *psz; )
               {
               LPTSTR pszNext;
               for (pszNext = psz; *pszNext && (*pszNext != TEXT('\r')) && (*pszNext != TEXT('\n')); ++pszNext)
                  ;
               DWORD cbWrite;
               DWORD cbWrote;
               if ((cbWrite = (DWORD)(pszNext - psz)) != 0)
                  WriteFile (fh, psz, cbWrite, &cbWrote, NULL);
               WriteFile (fh, TEXT("\r\n"), 2, &cbWrote, NULL);
               psz = (*pszNext == TEXT('\r')) ? (2+pszNext) : (*pszNext == TEXT('\n')) ? (1+pszNext) : NULL;
               }
            CloseHandle (fh);
            }

         Free (wp.wpBosLogGet.pszLogData);
         }

      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtGetServerLogFileEnd, lpiServer, pszRemote, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetServerAuth (LPIDENT lpiServer, BOOL fEnabled, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtSetServerAuthBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosAuthSet.hServer = hBOS;
      wp.wpBosAuthSet.fEnableAuth = fEnabled;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosAuthSet, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtSetServerAuthEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_StartService (LPIDENT lpiStart, BOOL fTemporary, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtStartServiceBegin, lpiStart);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiStart->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      if (lpiStart->fIsService())
         {
         TCHAR szName[ cchNAME ];
         lpiStart->GetServiceName (szName);

         if (fTemporary)
            {
            WORKERPACKET wp;
            wp.wpBosProcessExecutionStateSetTemporary.hServer = hBOS;
            wp.wpBosProcessExecutionStateSetTemporary.pszService = szName;
            wp.wpBosProcessExecutionStateSetTemporary.state = SERVICESTATE_RUNNING;

            AfsClass_Leave();
            rc = Worker_DoTask (wtaskBosProcessExecutionStateSetTemporary, &wp, &status);
            AfsClass_Enter();
            }
         else
            {
            WORKERPACKET wp;
            wp.wpBosProcessExecutionStateSet.hServer = hBOS;
            wp.wpBosProcessExecutionStateSet.pszService = szName;
            wp.wpBosProcessExecutionStateSet.state = SERVICESTATE_RUNNING;

            AfsClass_Leave();
            rc = Worker_DoTask (wtaskBosProcessExecutionStateSet, &wp, &status);
            AfsClass_Enter();
            }
         }
      else
         {
         WORKERPACKET wp;
         wp.wpBosProcessAllStart.hServer = hBOS;
         AfsClass_Leave();
         rc = Worker_DoTask (wtaskBosProcessAllStart, &wp, &status);
         AfsClass_Enter();
         }
      }

   if (rc)
      {
      if (lpiStart->fIsService())
         {
         LPSERVICE lpService;
         if ((lpService = lpiStart->OpenService (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpService->Invalidate();
            lpService->RefreshStatus();
            lpService->Close();
            }
         }
      else
         {
         if ((lpServer = lpiStart->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpServer->Invalidate();
            lpServer->RefreshAll();
            lpServer->Close();
            }
         }
      }

   if ((lpServer = lpiStart->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtStartServiceEnd, lpiStart, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_StopService (LPIDENT lpiStop, BOOL fTemporary, BOOL fWait, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtStopServiceBegin, lpiStop);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiStop->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      if (lpiStop->fIsService())
         {
         TCHAR szName[ cchNAME ];
         lpiStop->GetServiceName (szName);

         if (fTemporary)
            {
            WORKERPACKET wp;
            wp.wpBosProcessExecutionStateSetTemporary.hServer = hBOS;
            wp.wpBosProcessExecutionStateSetTemporary.pszService = szName;
            wp.wpBosProcessExecutionStateSetTemporary.state = SERVICESTATE_STOPPED;
            // TODO: wp.wpStopBosProcessTemporary.fWait = TRUE;

            AfsClass_Leave();
            rc = Worker_DoTask (wtaskBosProcessExecutionStateSetTemporary, &wp, &status);
            AfsClass_Enter();
            }
         else
            {
            WORKERPACKET wp;
            wp.wpBosProcessExecutionStateSet.hServer = hBOS;
            wp.wpBosProcessExecutionStateSet.pszService = szName;
            wp.wpBosProcessExecutionStateSet.state = SERVICESTATE_STOPPED;
            // TODO: wp.wpStopBosProcess.fWait = TRUE;

            AfsClass_Leave();
            rc = Worker_DoTask (wtaskBosProcessExecutionStateSet, &wp, &status);
            AfsClass_Enter();
            }
         }
      else if (fWait)
         {
         WORKERPACKET wp;
         wp.wpBosProcessAllWaitStop.hServer = hBOS;

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskBosProcessAllWaitStop, &wp, &status);
         AfsClass_Enter();
         }
      else // (!fWait)
         {
         WORKERPACKET wp;
         wp.wpBosProcessAllStop.hServer = hBOS;

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskBosProcessAllStop, &wp, &status);
         AfsClass_Enter();
         }
      }

   if (rc)
      {
      if (lpiStop->fIsService())
         {
         LPSERVICE lpService;
         if ((lpService = lpiStop->OpenService (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpService->Invalidate();
            lpService->RefreshStatus();
            lpService->Close();
            }
         }
      else
         {
         LPSERVER lpServer;
         if ((lpServer = lpiStop->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpServer->Invalidate();
            lpServer->RefreshAll();
            lpServer->Close();
            }
         }
      }

   if ((lpServer = lpiStop->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtStopServiceEnd, lpiStop, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_RestartService (LPIDENT lpiRestart, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtRestartServiceBegin, lpiRestart);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiRestart->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   BOOL fRestartAll = FALSE;
   if (!lpiRestart->fIsService())
      fRestartAll = TRUE;

   TCHAR szServiceRestart[ cchNAME ];
   if (lpiRestart->fIsService())
      {
      lpiRestart->GetServiceName (szServiceRestart);
      if (!lstrcmpi (szServiceRestart, TEXT("BOS")))
         fRestartAll = TRUE;
      }

   if (rc)
      {
      if (!fRestartAll)
         {
         WORKERPACKET wp;
         wp.wpBosProcessRestart.hServer = hBOS;
         wp.wpBosProcessRestart.pszService = szServiceRestart;

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskBosProcessRestart, &wp, &status);
         AfsClass_Enter();
         }
      else // (fRestartAll)
         {
         WORKERPACKET wp;
         wp.wpBosProcessAllStopAndRestart.hServer = hBOS;
         wp.wpBosProcessAllStopAndRestart.fRestartBOS = TRUE;

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskBosProcessAllStopAndRestart, &wp, &status);
         AfsClass_Enter();
         }
      }

   if (rc)
      {
      if (!fRestartAll)
         {
         LPSERVICE lpService;
         if ((lpService = lpiRestart->OpenService (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpService->Invalidate();
            lpService->RefreshStatus();
            lpService->Close();
            }
         }
      else // (fRestartAll)
         {
         LPSERVER lpServer;
         if ((lpServer = lpiRestart->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpServer->Invalidate();
            lpServer->RefreshAll();
            lpServer->Close();
            }
         }
      }

   if ((lpServer = lpiRestart->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtRestartServiceEnd, lpiRestart, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


LPIDENT AfsClass_CreateFileset (LPIDENT lpiAggregate, LPTSTR pszFileset, ULONG ckQuota, ULONG *pStatus)
{
   LPIDENT lpiFileset = NULL;
   ULONG status;
   BOOL rc = TRUE;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCreateFilesetBegin, lpiAggregate, pszFileset, 0);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiAggregate->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain idPartition
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpiAggregate->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      idPartition = lpAggregate->GetID();
      lpAggregate->Close();
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeCreate.hCell = hCell;
      wp.wpVosVolumeCreate.hServer = hVOS;
      wp.wpVosVolumeCreate.idPartition = idPartition;
      wp.wpVosVolumeCreate.pszVolume = pszFileset;
      wp.wpVosVolumeCreate.ckQuota = ckQuota;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeCreate, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiAggregate->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets (TRUE, &status);
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiAggregate->OpenCell()) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpiAggregate);
         lpCell->Close();
         }
      }

   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiAggregate->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         LPFILESET lpFileset;
         if ((lpFileset = lpAggregate->OpenFileset (pszFileset, &status)) == NULL)
            rc = FALSE;
         else
            {
            lpiFileset = lpFileset->GetIdentifier();
            lpFileset->Close();
            }
         lpAggregate->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiAggregate->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCreateFilesetEnd, lpiAggregate, pszFileset, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpiFileset : NULL;
}


BOOL AfsClass_DeleteFileset (LPIDENT lpiFileset, BOOL fVLDB, BOOL fServer, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteFilesetBegin, lpiFileset);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiFileset->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Does the fileset have a VLDB entry? Does it actually exist on the server?
   // What's its volume ID?  Its R/W ID?  Its partition ID?
   //
   VOLUMEID vidFileset;
   VOLUMEID vidReadWrite;
   int wFilesetGhost;

   // Obtain the ID of the fileset's parent aggregate.
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpiFileset->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((idPartition = lpAggregate->GetID()) == NO_PARTITION)
         rc = FALSE;
      lpAggregate->Close();
      }

   if (rc)
      {
      LPFILESET lpFileset;
      if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
         rc = FALSE;
      else
         {
         wFilesetGhost = lpFileset->GetGhostStatus();
         lpiFileset->GetFilesetID (&vidFileset);

         FILESETSTATUS fs;
         if (!lpFileset->GetStatus (&fs))
            vidReadWrite = vidFileset;
         else
            vidReadWrite = fs.idReadWrite;

         lpFileset->Close();
         }
      }

   if (!(wFilesetGhost & GHOST_HAS_VLDB_ENTRY))
      fVLDB = FALSE;
   if (!(wFilesetGhost & GHOST_HAS_SERVER_ENTRY))
      fServer = FALSE;

   if (rc && fVLDB && fServer)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeDelete.hCell = hCell;
      wp.wpVosVolumeDelete.hServer = hVOS;
      wp.wpVosVolumeDelete.idPartition = idPartition;
      wp.wpVosVolumeDelete.idVolume = vidFileset;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeDelete, &wp, &status);
      AfsClass_Enter();
      }
   else if (rc && fVLDB)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBEntryRemove.hCell = hCell;
      wp.wpVosVLDBEntryRemove.hServer = hVOS;
      wp.wpVosVLDBEntryRemove.idPartition = idPartition;
      wp.wpVosVLDBEntryRemove.idVolume = vidReadWrite;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBEntryRemove, &wp, &status);
      AfsClass_Enter();
      }
   else if (rc && fServer)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeZap.hCell = hCell;
      wp.wpVosVolumeZap.hServer = hVOS;
      wp.wpVosVolumeZap.idPartition = idPartition;
      wp.wpVosVolumeZap.idVolume = vidFileset;
      wp.wpVosVolumeZap.fForce = TRUE;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeZap, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiFileset->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets (TRUE);
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpiFileset->GetAggregate(), TRUE);
         lpCell->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiFileset->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteFilesetEnd, lpiFileset, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_MoveFileset (LPIDENT lpiFileset, LPIDENT lpiAggregateTarget, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtMoveFilesetBegin, lpiFileset, lpiAggregateTarget, NULL, NULL, 0, 0);

   LPIDENT lpiAggregateSource = lpiFileset->GetAggregate();

   // Obtain hCell, hVOS and the aggregate name for the source
   //
   PVOID hCell;
   PVOID hVOSSource = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiFileset->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOSSource = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain the ID of the source aggregate
   //
   int idPartitionSource;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpiFileset->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((idPartitionSource = lpAggregate->GetID()) == NO_PARTITION)
         rc = FALSE;
      lpAggregate->Close();
      }

   // Obtain hCell, hVOS and the aggregate name for the target
   //
   PVOID hVOSTarget = NULL;
   if ((lpServer = lpiAggregateTarget->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOSTarget = lpServer->OpenVosObject (NULL, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain the ID of the target aggregate
   //
   int idPartitionTarget;
   if ((lpAggregate = lpiAggregateTarget->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((idPartitionTarget = lpAggregate->GetID()) == NO_PARTITION)
         rc = FALSE;
      lpAggregate->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeMove.hCell = hCell;
      wp.wpVosVolumeMove.hServerFrom = hVOSSource;
      wp.wpVosVolumeMove.idPartitionFrom = idPartitionSource;
      wp.wpVosVolumeMove.hServerTo = hVOSTarget;
      wp.wpVosVolumeMove.idPartitionTo = idPartitionTarget;
      lpiFileset->GetFilesetID (&wp.wpVosVolumeMove.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeMove, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiAggregateSource->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets();
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiAggregateTarget->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets();
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPFILESET lpFileset;
      if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpFileset->Invalidate();
         lpFileset->RefreshStatus();
         lpFileset->Close();
         }
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpiAggregateSource, TRUE);
         lpCell->RefreshVLDB (lpiAggregateTarget, TRUE);
         lpCell->Close();
         }
      }

   if (hVOSSource)
      {
      if ((lpServer = lpiAggregateSource->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }
   if (hVOSTarget)
      {
      if ((lpServer = lpiAggregateTarget->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtMoveFilesetEnd, lpiFileset, lpiAggregateTarget, NULL, NULL, 0, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetFilesetQuota (LPIDENT lpiFileset, size_t ckQuotaNew, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtSetFilesetQuotaBegin, lpiFileset);

   // Obtain hCell and hVOS for the server where this fileset lives
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiFileset->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain the ID of the fileset's parent aggregate.
   //
   int idPartition;
   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiFileset->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((idPartition = lpAggregate->GetID()) == NO_PARTITION)
            rc = FALSE;
         lpAggregate->Close();
         }
      }

   // Change the fileset's quota.
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeQuotaChange.hCell = hCell;
      wp.wpVosVolumeQuotaChange.hServer = hVOS;
      wp.wpVosVolumeQuotaChange.idPartition = idPartition;
      lpiFileset->GetFilesetID (&wp.wpVosVolumeQuotaChange.idVolume);
      wp.wpVosVolumeQuotaChange.ckQuota = ckQuotaNew;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeQuotaChange, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPFILESET lpFileset;
      if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpFileset->Invalidate();
         lpFileset->RefreshStatus();
         lpFileset->Close();
         }
      }

   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiFileset->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->RefreshStatus();
         lpAggregate->Close();
         }
      }

   if (hVOS != NULL)
      {
      if ((lpServer = lpiFileset->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtSetFilesetQuotaEnd, lpiFileset, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SyncVLDB (LPIDENT lpiSync, BOOL fForce, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtSyncVLDBBegin, lpiSync);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiSync->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain the ID of the target aggregate.
   //
   int idPartition = NO_PARTITION;
   if (rc && (lpiSync->fIsAggregate() || lpiSync->fIsFileset()))
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiSync->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((idPartition = lpAggregate->GetID()) == NO_PARTITION)
            rc = FALSE;
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBSync.hCell = hCell;
      wp.wpVosVLDBSync.hServer = hVOS;
      wp.wpVosVLDBSync.idPartition = idPartition;
      wp.wpVosVLDBSync.fForce = fForce;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBSync, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      if (lpiSync->fIsServer())
         {
         LPSERVER lpServer;
         if ((lpServer = lpiSync->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpServer->Invalidate();
            rc = lpServer->RefreshAll (&status);
            lpServer->Close();
            }
         }
      else // (lpiSync->fIsAggregate())
         {
         LPAGGREGATE lpAggregate;
         if ((lpAggregate = lpiSync->OpenAggregate (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpAggregate->Invalidate();
            lpAggregate->RefreshStatus();
            lpAggregate->RefreshFilesets();
            lpAggregate->Close();

            LPCELL lpCell;
            if ((lpCell = lpiSync->OpenCell()) == NULL)
               rc = FALSE;
            else
               {
               lpCell->RefreshVLDB (lpiSync);
               lpCell->Close();
               }
            }
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiSync->OpenServer()) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtSyncVLDBEnd, lpiSync, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_ChangeAddress (LPIDENT lpiServer, LPSOCKADDR_IN pAddrOld, LPSOCKADDR_IN pAddrNew, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtChangeAddressBegin, lpiServer);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiServer->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   if (rc && pAddrNew)
      {
      WORKERPACKET wp;
      wp.wpVosFileServerAddressChange.hCell = hCell;
      wp.wpVosFileServerAddressChange.addrOld = *pAddrOld;
      wp.wpVosFileServerAddressChange.addrNew = *pAddrNew;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosFileServerAddressChange, &wp, &status);
      AfsClass_Enter();
      }
   else if (rc && !pAddrNew)
      {
      WORKERPACKET wp;
      wp.wpVosFileServerAddressRemove.hCell = hCell;
      wp.wpVosFileServerAddressRemove.addr = *pAddrOld;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosFileServerAddressRemove, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPSERVER lpServer;
      if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
         {
         lpServer->InvalidateStatus();
         lpServer->Close();
         }

      LPCELL lpCell;
      if ((lpCell = lpiServer->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->InvalidateServers ();
         rc = lpCell->RefreshServers (TRUE, &status);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtChangeAddressEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_ChangeAddress (LPIDENT lpiServer, LPSERVERSTATUS pStatusOld, LPSERVERSTATUS pStatusNew, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtChangeAddressBegin, lpiServer);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiServer->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   if (rc)
      {
      AfsClass_Leave();

      for (size_t iAddr = 0; rc && (iAddr < AFSCLASS_MAX_ADDRESSES_PER_SITE); ++iAddr)
         {
         int oldAddress;
         int newAddress;
         AfsClass_AddressToInt (&oldAddress, &pStatusOld->aAddresses[ iAddr ]);
         AfsClass_AddressToInt (&newAddress, &pStatusNew->aAddresses[ iAddr ]);

         if (oldAddress && newAddress && (oldAddress != newAddress))
            {
            WORKERPACKET wp;
            wp.wpVosFileServerAddressChange.hCell = hCell;
            wp.wpVosFileServerAddressChange.addrOld = pStatusOld->aAddresses[ iAddr ];
            wp.wpVosFileServerAddressChange.addrNew = pStatusNew->aAddresses[ iAddr ];

            rc = Worker_DoTask (wtaskVosFileServerAddressChange, &wp, &status);
            }
         else if (oldAddress && !newAddress)
            {
            WORKERPACKET wp;
            wp.wpVosFileServerAddressRemove.hCell = hCell;
            wp.wpVosFileServerAddressRemove.addr = pStatusOld->aAddresses[ iAddr ];

            rc = Worker_DoTask (wtaskVosFileServerAddressRemove, &wp, &status);
            }
         }

      AfsClass_Enter();
      }

   if (rc)
      {
      LPSERVER lpServer;
      if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
         {
         lpServer->InvalidateStatus();
         lpServer->Close();
         }

      LPCELL lpCell;
      if ((lpCell = lpiServer->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->InvalidateServers ();
         rc = lpCell->RefreshServers (TRUE, &status);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtChangeAddressEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_LockFileset (LPIDENT lpiFileset, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtLockFilesetBegin, lpiFileset);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Obtain the fileset's read-write identifier
   //
   LPIDENT lpiRW = NULL;
   LPFILESET lpFileset;
   if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((lpiRW = lpFileset->GetReadWriteIdentifier()) == NULL)
         rc = FALSE;
      lpFileset->Close();
      }

   // Perform the lock operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBEntryLock.hCell = hCell;
      lpiRW->GetFilesetID (&wp.wpVosVLDBEntryLock.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBEntryLock, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         if (lpiRW)
            lpCell->RefreshVLDB (lpiRW, TRUE, NULL, TRUE);
         else
            lpCell->RefreshVLDB (lpiFileset->GetCell());
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtLockFilesetEnd, lpiFileset, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_UnlockFileset (LPIDENT lpiFileset, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockFilesetBegin, lpiFileset);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Obtain the fileset's read-write identifier
   //
   LPIDENT lpiRW = NULL;
   LPFILESET lpFileset;
   if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((lpiRW = lpFileset->GetReadWriteIdentifier()) == NULL)
         rc = FALSE;
      lpFileset->Close();
      }

   // Perform the unlock operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBEntryUnlock.hCell = hCell;
      wp.wpVosVLDBEntryUnlock.hServer = NULL;
      wp.wpVosVLDBEntryUnlock.idPartition = NO_PARTITION;
      lpiRW->GetFilesetID (&wp.wpVosVLDBEntryUnlock.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBEntryUnlock, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         if (lpiRW)
            lpCell->RefreshVLDB (lpiRW, TRUE, NULL, TRUE);
         else
            lpCell->RefreshVLDB (lpiFileset->GetCell());
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockFilesetEnd, lpiFileset, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_UnlockAllFilesets (LPIDENT lpi, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockAllFilesetsBegin, lpi);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpi->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Obtain hServer if appropriate
   //
   PVOID hVOS = NULL;
   if (lpi && (!lpi->fIsCell()))
      {
      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((hVOS = lpServer->OpenVosObject (NULL, &status)) == NULL)
            rc = FALSE;
         lpServer->Close();
         }
      }

   // Obtain the ID of the scope aggregate.
   //
   int idPartition = NO_PARTITION;
   if (rc && (lpi->fIsFileset() || (lpi->fIsAggregate())))
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpi->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((idPartition = lpAggregate->GetID()) == NO_PARTITION)
            rc = FALSE;
         lpAggregate->Close();
         }
      }

   // Perform the unlock operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBEntryUnlock.hCell = hCell;
      wp.wpVosVLDBEntryUnlock.hServer = hVOS;
      wp.wpVosVLDBEntryUnlock.idPartition = idPartition;
      wp.wpVosVLDBEntryUnlock.idVolume = NO_VOLUME;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBEntryUnlock, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpi->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpi);
         lpCell->Close();
         }
      }

   if (hVOS)
      {
      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockAllFilesetsEnd, lpi);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


LPIDENT AfsClass_CreateReplica (LPIDENT lpiFileset, LPIDENT lpiAggregate, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;
   LPIDENT lpiReplica = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCreateReplicaBegin, lpiFileset, lpiAggregate, NULL, NULL, 0, 0);

   // Obtain hCell and hVOS for the target server
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiAggregate->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain idPartition
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpiAggregate->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      idPartition = lpAggregate->GetID();
      lpAggregate->Close();
      }

   // Modify VLDB to create mention of a new replica
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVLDBReadOnlySiteCreate.hCell = hCell;
      wp.wpVosVLDBReadOnlySiteCreate.hServer = hVOS;
      wp.wpVosVLDBReadOnlySiteCreate.idPartition = idPartition;
      lpiFileset->GetFilesetID (&wp.wpVosVLDBReadOnlySiteCreate.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVLDBReadOnlySiteCreate, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiAggregate->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets (TRUE, &status);
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiAggregate->OpenCell()) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpiAggregate);
         lpCell->Close();
         }
      }

   if (rc)
      {
      LPFILESET lpFileset;
      if ((lpFileset = lpiFileset->OpenFileset (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((lpiReplica = lpFileset->GetReadOnlyIdentifier (lpiAggregate, &status)) == NULL)
            rc = FALSE;
         lpFileset->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiAggregate->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCreateReplicaEnd, lpiFileset, lpiAggregate, NULL, NULL, 0, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpiReplica : FALSE;
}


BOOL AfsClass_DeleteReplica (LPIDENT lpiReplica, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteFilesetBegin, lpiReplica);

   // Obtain hCell and hVOS for the server
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiReplica->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Get the read/write fileset identifier and Ghost status
   //
   LPIDENT lpiRW = NULL;
   int wFilesetGhost = 0;
   LPFILESET lpFileset;
   if ((lpFileset = lpiReplica->OpenFileset (&status)) == NULL)
      rc = FALSE;
   else
      {
      wFilesetGhost = lpFileset->GetGhostStatus();
      if ((lpiRW = lpFileset->GetReadWriteIdentifier()) == NULL)
         rc = FALSE;
      lpFileset->Close();
      }

   TCHAR szAggregateName[ cchNAME ];
   lpiReplica->GetAggregateName (szAggregateName);

   // Obtain the ID of the replica's partition
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpiReplica->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      idPartition = lpAggregate->GetID();
      lpAggregate->Close();
      }

   // If the volume exists in both VLDB and on the server, just delete it
   //
   if (rc && (wFilesetGhost & GHOST_HAS_VLDB_ENTRY) && (wFilesetGhost & GHOST_HAS_SERVER_ENTRY))
      {
      WORKERPACKET wp;
      wp.wpVosVolumeDelete.hCell = hCell;
      wp.wpVosVolumeDelete.hServer = hVOS;
      wp.wpVosVolumeDelete.idPartition = idPartition;
      lpiReplica->GetFilesetID (&wp.wpVosVolumeDelete.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeDelete, &wp, &status);
      AfsClass_Enter();
      }
   else
      {
      // If necessary, modify VLDB to remove mention of this replica
      //
      if (rc && (wFilesetGhost & GHOST_HAS_VLDB_ENTRY))
         {
         WORKERPACKET wp;
         wp.wpVosVLDBReadOnlySiteDelete.hCell = hCell;
         wp.wpVosVLDBReadOnlySiteDelete.hServer = hVOS;
         wp.wpVosVLDBReadOnlySiteDelete.idPartition = idPartition;
         lpiRW->GetFilesetID (&wp.wpVosVLDBReadOnlySiteDelete.idVolume);

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskVosVLDBReadOnlySiteDelete, &wp, &status);
         AfsClass_Enter();
         }

      // If necessary, zap the volume
      //
      if (rc && (wFilesetGhost & GHOST_HAS_SERVER_ENTRY))
         {
         WORKERPACKET wp;
         wp.wpVosVolumeZap.hCell = hCell;
         wp.wpVosVolumeZap.hServer = hVOS;
         wp.wpVosVolumeZap.idPartition = idPartition;
         lpiReplica->GetFilesetID (&wp.wpVosVolumeZap.idVolume);
         wp.wpVosVolumeZap.fForce = TRUE;

         AfsClass_Leave();
         rc = Worker_DoTask (wtaskVosVolumeZap, &wp, &status);
         AfsClass_Enter();
         }
      }

   // Clean up
   //
   if (rc)
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpiReplica->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpAggregate->Invalidate();
         lpAggregate->RefreshFilesets (TRUE, &status);
         lpAggregate->Close();
         }
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiReplica->OpenCell()) == NULL)
         rc = FALSE;
      else
         {
         lpCell->RefreshVLDB (lpiReplica->GetAggregate());
         lpCell->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiReplica->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteFilesetEnd, lpiReplica, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_DeleteClone (LPIDENT lpiClone, ULONG *pStatus)
{
   return AfsClass_DeleteFileset (lpiClone, TRUE, TRUE, pStatus);
}


BOOL AfsClass_InstallFile (LPIDENT lpiServer, LPTSTR pszTarget, LPTSTR pszSource, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtInstallFileBegin, lpiServer, pszSource, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosExecutableCreate.hServer = hBOS;
      wp.wpBosExecutableCreate.pszLocal = pszSource;
      wp.wpBosExecutableCreate.pszRemoteDir = pszTarget;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosExecutableCreate, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtInstallFileEnd, lpiServer, pszSource, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_UninstallFile (LPIDENT lpiServer, LPTSTR pszUninstall, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtUninstallFileBegin, lpiServer, pszUninstall, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosExecutableRevert.hServer = hBOS;
      wp.wpBosExecutableRevert.pszFilename = pszUninstall;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosExecutableRevert, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtUninstallFileEnd, lpiServer, pszUninstall, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_PruneOldFiles (LPIDENT lpiServer, BOOL fBAK, BOOL fOLD, BOOL fCore, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtPruneFilesBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosExecutablePrune.hServer = hBOS;
      wp.wpBosExecutablePrune.fPruneBak = fBAK;
      wp.wpBosExecutablePrune.fPruneOld = fOLD;
      wp.wpBosExecutablePrune.fPruneCore = fCore;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosExecutablePrune, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtPruneFilesEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_RenameFileset (LPIDENT lpiFileset, LPTSTR pszNewName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtRenameFilesetBegin, lpiFileset, pszNewName, 0);

   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeRename.hCell = hCell;
      lpiFileset->GetFilesetID (&wp.wpVosVolumeRename.idVolume);
      wp.wpVosVolumeRename.pszVolume = pszNewName;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeRename, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFileset->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->Invalidate();
         rc = lpCell->RefreshAll (&status);
         lpCell->Close();
         }
      }


   NOTIFYCALLBACK::SendNotificationToAll (evtRenameFilesetEnd, lpiFileset, pszNewName, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


#define iswhite(_ch) ((_ch)==TEXT(' ') || (_ch)==TEXT('\t'))

LPIDENT AfsClass_CreateService (LPIDENT lpiServer, LPTSTR pszService, LPTSTR pszCommand, LPTSTR pszParams, LPTSTR pszNotifier, AFSSERVICETYPE type, SYSTEMTIME *pstIfCron, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;
   LPIDENT lpiService = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCreateServiceBegin, lpiServer, pszService, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosProcessCreate.hServer = hBOS;
      wp.wpBosProcessCreate.pszService = pszService;
      wp.wpBosProcessCreate.type = type;
      wp.wpBosProcessCreate.pszNotifier = pszNotifier;

      TCHAR szFullCommand[ MAX_PATH + MAX_PATH ];
      wsprintf (szFullCommand, TEXT("%s %s"), pszCommand, pszParams);
      wp.wpBosProcessCreate.pszCommand = szFullCommand;

      TCHAR szCronTime[ 256 ] = TEXT("");
      wp.wpBosProcessCreate.pszTimeCron = szCronTime;

      if (type == SERVICETYPE_CRON)
         AfsClass_FormatRecurringTime (szCronTime, pstIfCron);
      else
         wp.wpBosProcessCreate.pszTimeCron = NULL;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosProcessCreate, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpServer->InvalidateServices();
         if (!lpServer->RefreshServices (TRUE, &status))
            rc = FALSE;
         else
            {
            LPSERVICE lpService;
            if ((lpService = lpServer->OpenService (pszService, &status)) == NULL)
               rc = FALSE;
            else
               {
               lpiService = lpService->GetIdentifier();
               lpService->Close();
               }
            }
         lpServer->Close();
         }
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCreateServiceEnd, lpiServer, pszService, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpiService : NULL;
}


BOOL AfsClass_DeleteService (LPIDENT lpiService, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteServiceBegin, lpiService);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiService->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Before a service can be deleted, it must be stopped (otherwise, on NT,
   // the Delete operation won't block for the required Stop to complete--
   // so our wtaskDeleteBosProcess would return before the service really
   // was deleted).
   //
   if (rc)
      {
      TCHAR szService[ cchNAME ];
      lpiService->GetServiceName (szService);

      WORKERPACKET wp;
      wp.wpBosProcessExecutionStateSet.hServer = hBOS;
      wp.wpBosProcessExecutionStateSet.pszService = szService;
      wp.wpBosProcessExecutionStateSet.state = SERVICESTATE_STOPPED;
      // TODO: wp.wpStopBosProcess.fWait = TRUE;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosProcessExecutionStateSet, &wp, &status);
      AfsClass_Enter();
      }

   // Delete the service
   //
   if (rc)
      {
      TCHAR szService[ cchNAME ];
      lpiService->GetServiceName (szService);

      WORKERPACKET wp;
      wp.wpBosProcessDelete.hServer = hBOS;
      wp.wpBosProcessDelete.pszService = szService;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosProcessDelete, &wp, &status);
      AfsClass_Enter();
      }

   if (rc)
      {
      if ((lpServer = lpiService->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpServer->InvalidateServices();
         if (!lpServer->RefreshServices (TRUE, &status))
            rc = FALSE;
         lpServer->Close();
         }
      }

   if ((lpServer = lpiService->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteServiceEnd, lpiService, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_ReleaseFileset (LPIDENT lpiFilesetRW, BOOL fForce, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtReleaseFilesetBegin, lpiFilesetRW);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpiFilesetRW->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeRelease.hCell = hCell;
      wp.wpVosVolumeRelease.fForce = fForce;
      lpiFilesetRW->GetFilesetID (&wp.wpVosVolumeRelease.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeRelease, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      LPCELL lpCell;
      if ((lpCell = lpiFilesetRW->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpCell->Invalidate();
         rc = lpCell->RefreshAll (&status);
         lpCell->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpiFilesetRW->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtReleaseFilesetEnd, lpiFilesetRW, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_GetFileDates (LPIDENT lpiServer, LPTSTR pszFilename, SYSTEMTIME *pstFile, SYSTEMTIME *pstBAK, SYSTEMTIME *pstOLD, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtGetFileDatesBegin, lpiServer, pszFilename, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosExecutableTimestampGet.hServer = hBOS;
      wp.wpBosExecutableTimestampGet.pszFilename = pszFilename;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskBosExecutableTimestampGet, &wp, &status)) == TRUE)
         {
         *pstFile = wp.wpBosExecutableTimestampGet.timeNew;
         *pstBAK = wp.wpBosExecutableTimestampGet.timeBak;
         *pstOLD = wp.wpBosExecutableTimestampGet.timeOld;
         }

      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtGetFileDatesEnd, lpiServer, pszFilename, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_ExecuteCommand (LPIDENT lpiServer, LPTSTR pszCommand, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtExecuteCommandBegin, lpiServer, pszCommand, 0);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosCommandExecute.hServer = hBOS;
      wp.wpBosCommandExecute.pszCommand = pszCommand;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosCommandExecute, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtExecuteCommandEnd, lpiServer, pszCommand, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


LPADMINLIST AfsClass_AdminList_Load (LPIDENT lpiServer, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;
   LPADMINLIST lpList = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtAdminListLoadBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      lpList = New(ADMINLIST);
      memset (lpList, 0x00, sizeof(ADMINLIST));
      lpList->cRef = 1;
      lpList->lpiServer = lpiServer;

      WORKERPACKET wpBegin;
      wpBegin.wpBosAdminGetBegin.hServer = hBOS;
      if (!Worker_DoTask (wtaskBosAdminGetBegin, &wpBegin, &status))
         rc = FALSE;
      else
         {
         for (;;)
            {
            TCHAR szAdmin[ cchNAME ];

            WORKERPACKET wpNext;
            wpNext.wpBosAdminGetNext.hEnum = wpBegin.wpBosAdminGetBegin.hEnum;
            wpNext.wpBosAdminGetNext.pszAdmin = szAdmin;

            if (!Worker_DoTask (wtaskBosAdminGetNext, &wpNext, &status))
               {
               if (status == ADMITERATORDONE)
                  status = 0;
               else
                  rc = FALSE;
               break;
               }

            size_t iAdded;
            if ((iAdded = AfsClass_AdminList_AddEntry (lpList, szAdmin)) != (size_t)-1)
               {
               lpList->aEntries[ iAdded ].fAdded = FALSE;
               }
            }

         WORKERPACKET wpDone;
         wpDone.wpBosAdminGetDone.hEnum = wpBegin.wpBosAdminGetBegin.hEnum;
         Worker_DoTask (wtaskBosAdminGetDone, &wpDone);
         }
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtAdminListLoadEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpList : NULL;
}


LPADMINLIST AfsClass_AdminList_Copy (LPADMINLIST lpOld)
{
   LPADMINLIST lpNew = NULL;

   if (lpOld)
      {
      lpNew = New(ADMINLIST);
      memcpy (lpNew, lpOld, sizeof(ADMINLIST));

      lpNew->cRef = 1;
      lpNew->aEntries = 0;
      lpNew->cEntries = 0;

      if (REALLOC (lpNew->aEntries, lpNew->cEntries, lpOld->cEntries, cREALLOC_ADMINLISTENTRIES))
         {
         size_t cb = lpOld->cEntries * sizeof(ADMINLISTENTRY);
         memcpy (lpNew->aEntries, lpOld->aEntries, cb);
         }
      }

   return lpNew;
}


BOOL AfsClass_AdminList_Save (LPADMINLIST lpList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtAdminListSaveBegin, lpList->lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpList->lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      for (size_t iEntry = 0; iEntry < lpList->cEntries; ++iEntry)
         {
         if (!lpList->aEntries[ iEntry ].szAdmin[0])
            continue;

         // are we supposed to add this entry?
         //
         if (lpList->aEntries[ iEntry ].fAdded && !lpList->aEntries[ iEntry ].fDeleted)
            {
            WORKERPACKET wp;
            wp.wpBosAdminCreate.hServer = hBOS;
            wp.wpBosAdminCreate.pszAdmin = lpList->aEntries[ iEntry ].szAdmin;

            ULONG thisstatus;
            if (!Worker_DoTask (wtaskBosAdminCreate, &wp, &thisstatus))
               {
               rc = FALSE;
               status = thisstatus;
               }
            else
               {
               lpList->aEntries[ iEntry ].fAdded = FALSE;
               }
            }

         // are we supposed to delete this entry?
         //
         if (!lpList->aEntries[ iEntry ].fAdded && lpList->aEntries[ iEntry ].fDeleted)
            {
            WORKERPACKET wp;
            wp.wpBosAdminDelete.hServer = hBOS;
            wp.wpBosAdminDelete.pszAdmin = lpList->aEntries[ iEntry ].szAdmin;

            ULONG thisstatus;
            if (!Worker_DoTask (wtaskBosAdminDelete, &wp, &thisstatus))
               {
               rc = FALSE;
               status = thisstatus;
               }
            else
               {
               lpList->aEntries[ iEntry ].szAdmin[0] = TEXT('\0');
               lpList->aEntries[ iEntry ].fDeleted = FALSE;
               }
            }
         }
      }

   if ((lpServer = lpList->lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtAdminListSaveEnd, lpList->lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


void AfsClass_AdminList_Free (LPADMINLIST lpList)
{
   if (lpList && !InterlockedDecrement (&lpList->cRef))
      {
      if (lpList->aEntries)
         Free (lpList->aEntries);
      memset (lpList, 0x00, sizeof(ADMINLIST));
      Delete (lpList);
      }
}


size_t AfsClass_AdminList_AddEntry (LPADMINLIST lpList, LPTSTR pszAdmin)
{
   size_t iAdded = (size_t)-1;

   if (lpList)
      {
      size_t iEntry;
      for (iEntry = 0; iEntry < lpList->cEntries; ++iEntry)
         {
         if (!lpList->aEntries[ iEntry ].szAdmin[0])
            break;
         }
      if (iEntry >= lpList->cEntries)
         {
         (void)REALLOC (lpList->aEntries, lpList->cEntries, 1+iEntry, cREALLOC_ADMINLISTENTRIES);
         }
      if (iEntry < lpList->cEntries)
         {
         iAdded = iEntry;
         lstrcpy (lpList->aEntries[ iAdded ].szAdmin, pszAdmin);
         lpList->aEntries[ iAdded ].fAdded = TRUE;
         lpList->aEntries[ iAdded ].fDeleted = FALSE;
         }
      }

   return iAdded;
}


BOOL AfsClass_AdminList_DelEntry (LPADMINLIST lpList, size_t iIndex)
{
   BOOL rc = FALSE;

   if ( lpList &&
        (iIndex < lpList->cEntries) &&
        (lpList->aEntries[ iIndex ].szAdmin[0]) &&
        (!lpList->aEntries[ iIndex ].fDeleted) )
      {
      if (lpList->aEntries[ iIndex ].fAdded)
         lpList->aEntries[ iIndex ].szAdmin[0] = TEXT('\0');
      else
         lpList->aEntries[ iIndex ].fDeleted = TRUE;

      rc = TRUE;
      }

   return rc;
}


LPKEYLIST AfsClass_KeyList_Load (LPIDENT lpiServer, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;
   LPKEYLIST lpList = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtKeyListLoadBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      lpList = New(KEYLIST);
      memset (lpList, 0x00, sizeof(KEYLIST));
      lpList->lpiServer = lpiServer;

      WORKERPACKET wpBegin;
      wpBegin.wpBosKeyGetBegin.hServer = hBOS;
      if (!Worker_DoTask (wtaskBosKeyGetBegin, &wpBegin, &status))
         rc = FALSE;
      else
         {
         for (size_t iEnum = 0; ; ++iEnum)
            {
            WORKERPACKET wpNext;
            wpNext.wpBosKeyGetNext.hEnum = wpBegin.wpBosKeyGetBegin.hEnum;

            if (!Worker_DoTask (wtaskBosKeyGetNext, &wpNext, &status))
               {
               if (status == ADMITERATORDONE)
                  status = 0;
               else
                  rc = FALSE;
               break;
               }

            if (REALLOC (lpList->aKeys, lpList->cKeys, 1+iEnum, cREALLOC_SERVERKEYS))
               {
               lpList->aKeys[ iEnum ].keyVersion = wpNext.wpBosKeyGetNext.keyVersion;
               memcpy (&lpList->aKeys[ iEnum ].keyData, &wpNext.wpBosKeyGetNext.keyData, sizeof(ENCRYPTIONKEY));
               memcpy (&lpList->aKeys[ iEnum ].keyInfo, &wpNext.wpBosKeyGetNext.keyInfo, sizeof(ENCRYPTIONKEYINFO));
               }
            }

         WORKERPACKET wpDone;
         wpDone.wpBosKeyGetDone.hEnum = wpBegin.wpBosKeyGetBegin.hEnum;
         Worker_DoTask (wtaskBosKeyGetDone, &wpDone);
         }
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtKeyListLoadEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpList : NULL;
}


void AfsClass_KeyList_Free (LPKEYLIST lpList)
{
   if (lpList)
      {
      if (lpList->aKeys)
         Free (lpList->aKeys);
      memset (lpList, 0x00, sizeof(KEYLIST));
      Delete (lpList);
      }
}


BOOL AfsClass_AddKey (LPIDENT lpiServer, int keyVersion, LPTSTR pszString, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   TCHAR szCell[ cchNAME ];
   lpiServer->GetCellName (szCell);

   WORKERPACKET wp;
   wp.wpKasStringToKey.pszCell = szCell;
   wp.wpKasStringToKey.pszString = pszString;
   if (!Worker_DoTask (wtaskKasStringToKey, &wp, &status))
      {
      rc = FALSE;
      }
   else if (!AfsClass_AddKey (lpiServer, keyVersion, &wp.wpKasStringToKey.key, &status))
      {
      rc = FALSE;
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_AddKey (LPIDENT lpiServer, int keyVersion, LPENCRYPTIONKEY pKey, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosKeyCreate.hServer = hBOS;
      wp.wpBosKeyCreate.keyVersion = keyVersion;
      memcpy (&wp.wpBosKeyCreate.key, pKey, sizeof(ENCRYPTIONKEY));
      rc = Worker_DoTask (wtaskBosKeyCreate, &wp, &status);
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_DeleteKey (LPIDENT lpiServer, int keyVersion, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosKeyDelete.hServer = hBOS;
      wp.wpBosKeyDelete.keyVersion = keyVersion;
      rc = Worker_DoTask (wtaskBosKeyDelete, &wp, &status);
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_GetRandomKey (LPIDENT lpi, LPENCRYPTIONKEY pKey, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpi->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpKasServerRandomKeyGet.hCell = hCell;
      wp.wpKasServerRandomKeyGet.hServer = hKAS;
      rc = Worker_DoTask (wtaskKasServerRandomKeyGet, &wp, &status);

      if (rc)
         memcpy (pKey, &wp.wpKasServerRandomKeyGet.key, sizeof(ENCRYPTIONKEY));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_Clone (LPIDENT lpiRW, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCloneBegin, lpiRW, 0);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiRW->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosBackupVolumeCreate.hCell = hCell;
      lpiRW->GetFilesetID (&wp.wpVosBackupVolumeCreate.idVolume);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosBackupVolumeCreate, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      LPSERVER lpServer;
      if ((lpServer = lpiRW->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpServer->Invalidate();
         rc = lpServer->RefreshAll (&status);
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCloneEnd, lpiRW, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_CloneMultiple (LPIDENT lpi, LPTSTR pszPrefix, BOOL fExclude, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCloneMultipleBegin, lpi);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpi->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Obtain hServer if appropriate
   //
   PVOID hVOS = NULL;
   if (!lpi->fIsCell())
      {
      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((hVOS = lpServer->OpenVosObject (NULL, &status)) == NULL)
            rc = FALSE;
         lpServer->Close();
         }
      }

   // If requested, obtain the appropriate aggregate ID
   //
   int idPartition = NO_PARTITION;
   if (rc && (lpi->fIsFileset() || lpi->fIsAggregate()))
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpi->OpenAggregate (&status)) == NULL)
         rc = FALSE;
      else
         {
         if ((idPartition = lpAggregate->GetID()) == NO_PARTITION)
            rc = FALSE;
         lpAggregate->Close();
         }
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosBackupVolumeCreateMultiple.hCell = hCell;
      wp.wpVosBackupVolumeCreateMultiple.hServer = hVOS;
      wp.wpVosBackupVolumeCreateMultiple.idPartition = idPartition;
      wp.wpVosBackupVolumeCreateMultiple.pszPrefix = pszPrefix;
      wp.wpVosBackupVolumeCreateMultiple.fExclude = fExclude;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosBackupVolumeCreateMultiple, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      if (lpi->fIsCell())
         {
         LPCELL lpCell;
         if ((lpCell = lpi->OpenCell (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpCell->Invalidate();
            rc = lpCell->RefreshAll (&status);
            lpCell->Close();
            }
         }
      else
         {
         LPSERVER lpServer;
         if ((lpServer = lpi->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpServer->Invalidate();
            rc = lpServer->RefreshAll (&status);
            lpServer->Close();
            }
         }
      }

   if (hVOS)
      {
      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCloneMultipleEnd, lpi, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_DumpFileset (LPIDENT lpi, LPTSTR pszFilename, LPSYSTEMTIME pstDate, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDumpFilesetBegin, lpi, pszFilename, 0);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpi->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain idPartition
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpi->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      idPartition = lpAggregate->GetID();
      lpAggregate->Close();
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeDump.hCell = hCell;
      wp.wpVosVolumeDump.hServer = hVOS;
      wp.wpVosVolumeDump.pszFilename = pszFilename;
      wp.wpVosVolumeDump.idPartition = idPartition;
      lpi->GetFilesetID (&wp.wpVosVolumeDump.idVolume);

      if (pstDate)
         memcpy (&wp.wpVosVolumeDump.stStart, pstDate, sizeof(SYSTEMTIME));
      else
         memset (&wp.wpVosVolumeDump.stStart, 0x00, sizeof(SYSTEMTIME));

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeDump, &wp, &status);
      AfsClass_Enter();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDumpFilesetEnd, lpi, pszFilename, status);
   AfsClass_Leave();

   if (hVOS)
      {
      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_RestoreFileset (LPIDENT lpi, LPTSTR pszFileset, LPTSTR pszFilename, BOOL fIncremental, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtRestoreFilesetBegin, lpi, NULL, pszFileset, pszFilename, 0, 0);

   // Obtain hCell and hVOS
   //
   PVOID hCell;
   PVOID hVOS = NULL;
   LPSERVER lpServer;
   if ((lpServer = lpi->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hVOS = lpServer->OpenVosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   // Obtain idPartition
   //
   int idPartition;
   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpi->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      idPartition = lpAggregate->GetID();
      lpAggregate->Close();
      }

   // Perform the actual operation
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpVosVolumeRestore.hCell = hCell;
      wp.wpVosVolumeRestore.hServer = hVOS;
      wp.wpVosVolumeRestore.idPartition = idPartition;
      wp.wpVosVolumeRestore.pszVolume = pszFileset;
      wp.wpVosVolumeRestore.pszFilename = pszFilename;
      wp.wpVosVolumeRestore.fIncremental = fIncremental;

      if (lpi->fIsFileset())
         lpi->GetFilesetID (&wp.wpVosVolumeRestore.idVolume);
      else
         wp.wpVosVolumeRestore.idVolume = NO_VOLUME;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskVosVolumeRestore, &wp, &status);
      AfsClass_Enter();
      }

   // Clean up
   //
   if (rc)
      {
      if ((lpServer = lpi->OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         lpServer->Invalidate();
         rc = lpServer->RefreshAll (&status);
         lpServer->Close();
         }
      }

   if (hVOS)
      {
      if ((lpServer = lpi->OpenServer (&status)) != NULL)
         {
         lpServer->CloseVosObject();
         lpServer->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtRestoreFilesetEnd, lpi, NULL, pszFileset, pszFilename, 0, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_GetRestartTimes (LPIDENT lpiServer, BOOL *pfWeekly, LPSYSTEMTIME pstWeekly, BOOL *pfDaily, LPSYSTEMTIME pstDaily, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtGetRestartTimesBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpBosExecutableRestartTimeGet.hServer = hBOS;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosExecutableRestartTimeGet, &wp, &status);
      AfsClass_Enter();

      if (rc)
         {
         *pfWeekly = wp.wpBosExecutableRestartTimeGet.fWeeklyRestart;
         *pstWeekly = wp.wpBosExecutableRestartTimeGet.timeWeekly;
         *pfDaily = wp.wpBosExecutableRestartTimeGet.fDailyRestart;
         *pstDaily = wp.wpBosExecutableRestartTimeGet.timeDaily;
         }
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtGetRestartTimesEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetRestartTimes (LPIDENT lpiServer, LPSYSTEMTIME pstWeekly, LPSYSTEMTIME pstDaily, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtSetRestartTimesBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      SYSTEMTIME timeNever;
      memset (&timeNever, 0x00, sizeof(SYSTEMTIME));

      WORKERPACKET wp;
      wp.wpBosExecutableRestartTimeSet.hServer = hBOS;
      wp.wpBosExecutableRestartTimeSet.fWeeklyRestart = (pstWeekly != NULL) ? TRUE : FALSE;
      wp.wpBosExecutableRestartTimeSet.timeWeekly = (pstWeekly != NULL) ? *pstWeekly : timeNever;
      wp.wpBosExecutableRestartTimeSet.fDailyRestart = (pstDaily != NULL) ? TRUE : FALSE;
      wp.wpBosExecutableRestartTimeSet.timeDaily = (pstDaily != NULL) ? *pstDaily : timeNever;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosExecutableRestartTimeSet, &wp, &status);
      AfsClass_Enter();
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtSetRestartTimesEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_MoveReplica (LPIDENT lpiReplica, LPIDENT lpiAggregateTarget, ULONG *pStatus)
{
   BOOL rc = TRUE;

   // Find the identifier for this replica's read/write fileset.
   //
   LPIDENT lpiFilesetRW = NULL;
   LPFILESET lpFileset;
   if ((lpFileset = lpiReplica->OpenFileset (pStatus)) == NULL)
      rc = FALSE;
   else
      {
      if ((lpiFilesetRW = lpFileset->GetReadWriteIdentifier (pStatus)) == NULL)
         rc = FALSE;
      lpFileset->Close();
      }

   // If the fileset replica currently resides on the same server
   // as the target aggregate, we'll follow the following steps:
   //
   // 1. Delete the old fileset replica -> on error, quit
   // 2. Create the new fileset replica -> on error, recreate old replica, quit
   //
   // If the fileset replica instead currently resides on a different
   // server, we can follow the preferred steps:
   //
   // 1. Create the new fileset replica -> on error, quit
   // 2. Delete the old fileset replica -> on error, delete the new replica, quit
   //
   if (rc)
      {
      LPIDENT lpiReplicaNew;

      if (lpiReplica->GetServer() == lpiAggregateTarget->GetServer())
         {
         LPIDENT lpiAggregateOriginal = lpiReplica->GetAggregate();

         if (!AfsClass_DeleteReplica (lpiReplica, pStatus))
            {
            rc = FALSE;
            }
         else if ((lpiReplicaNew = AfsClass_CreateReplica (lpiFilesetRW, lpiAggregateTarget, pStatus)) == NULL)
            {
            (void)AfsClass_CreateReplica (lpiFilesetRW, lpiAggregateOriginal);
            rc = FALSE;
            }
         }
      else // different server?
         {
         if ((lpiReplicaNew = AfsClass_CreateReplica (lpiFilesetRW, lpiAggregateTarget, pStatus)) == NULL)
            {
            rc = FALSE;
            }
         else if (!AfsClass_DeleteReplica (lpiReplica, pStatus))
            {
            (void)AfsClass_DeleteReplica (lpiReplicaNew, pStatus);
            rc = FALSE;
            }
         }
      }

   return rc;
}


BOOL AfsClass_Salvage (LPIDENT lpiSalvage, LPTSTR *ppszLogData, int nProcesses, LPTSTR pszTempDir, LPTSTR pszLogFile, BOOL fForce, BOOL fReadonly, BOOL fLogInodes, BOOL fLogRootInodes, BOOL fRebuildDirs, BOOL fReadBlocks, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtSalvageBegin, lpiSalvage);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiSalvage->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (ppszLogData)
      *ppszLogData = NULL;

   // Step one: perform the actual salvage. This will dump a log file onto
   // the target computer.
   //
   if (rc)
      {
      LPTSTR pszAggregate = NULL;
      TCHAR szAggregate[ cchNAME ];
      if (lpiSalvage->fIsAggregate() || lpiSalvage->fIsFileset())
         {
         lpiSalvage->GetAggregateName (szAggregate);
         pszAggregate = szAggregate;
         }

      LPTSTR pszFileset = NULL;
      TCHAR szFileset[ cchNAME ];
      if (lpiSalvage->fIsFileset())
         {
         VOLUMEID vid;
         lpiSalvage->GetFilesetID (&vid);
         wsprintf (szFileset, TEXT("%lu"), vid);
         pszFileset = szFileset;
         }

      if (pszLogFile == NULL)
         pszLogFile = TEXT("SalvageLog");

      WORKERPACKET wp;
      wp.wpBosSalvage.hCell = hCell;
      wp.wpBosSalvage.hServer = hBOS;
      wp.wpBosSalvage.pszAggregate = pszAggregate;
      wp.wpBosSalvage.pszFileset = pszFileset;
      wp.wpBosSalvage.nProcesses = nProcesses;
      wp.wpBosSalvage.pszTempDir = pszTempDir;
      wp.wpBosSalvage.pszLogFile = pszLogFile;
      wp.wpBosSalvage.fForce = fForce;
      wp.wpBosSalvage.fReadonly = fReadonly;
      wp.wpBosSalvage.fLogInodes = fLogInodes;
      wp.wpBosSalvage.fLogRootInodes = fLogRootInodes;
      wp.wpBosSalvage.fRebuildDirs = fRebuildDirs;
      wp.wpBosSalvage.fReadBlocks = fReadBlocks;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskBosSalvage, &wp, &status);
      AfsClass_Enter();
      }

   // Step two: retrieve the log file from that salvage operation.
   // If we can't get the log file back, that's not fatal--just return
   // a NULL pointer for the log data.
   //
   if (rc && ppszLogData)
      {
      WORKERPACKET wp;
      wp.wpBosLogGet.hServer = hBOS;
      wp.wpBosLogGet.pszLogName = pszLogFile;
      wp.wpBosLogGet.pszLogData = NULL;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskBosLogGet, &wp, &status)) == TRUE)
         {
         // Okay, well, we have the log in memory now. Problem is,
         // it has UNIX-style CR's... and so is missing the LF which
         // PCs expect before each CR.  Wow--look at all the
         // acronyms!  Count the CRs, alloc a larger buffer, and stuff
         // in the LFs before each CR.
         //
         size_t cchRequired = 1;
         for (LPTSTR pchIn = wp.wpBosLogGet.pszLogData; *pchIn; ++pchIn)
            {
            cchRequired += (*pchIn == TEXT('\r')) ? 0 : (*pchIn == TEXT('\n')) ? 2 : 1;
            }

         if ((*ppszLogData = AllocateString (cchRequired)) != NULL)
            {
            LPTSTR pszOut = *ppszLogData;
            for (LPTSTR pchIn = wp.wpBosLogGet.pszLogData; *pchIn; ++pchIn)
               {
               if (*pchIn == TEXT('\n'))
                  *pszOut++ = TEXT('\r');
               if (*pchIn != TEXT('\r'))
                  *pszOut++ = *pchIn;
               }
            *pszOut++ = TEXT('\0');
            }
         }
      AfsClass_Enter();
      }

   if ((lpServer = lpiSalvage->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtSalvageEnd, lpiSalvage, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


void AfsClass_FreeSalvageLog (LPTSTR pszLogData)
{
   if (pszLogData)
      Free (pszLogData);
}


LPHOSTLIST AfsClass_HostList_Load (LPIDENT lpiServer, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;
   LPHOSTLIST lpList = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtHostListLoadBegin, lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      lpList = New(HOSTLIST);
      memset (lpList, 0x00, sizeof(HOSTLIST));
      lpList->cRef = 1;
      lpList->lpiServer = lpiServer;

      WORKERPACKET wpBegin;
      wpBegin.wpBosHostGetBegin.hServer = hBOS;
      if (!Worker_DoTask (wtaskBosHostGetBegin, &wpBegin, &status))
         rc = FALSE;
      else
         {
         for (;;)
            {
            TCHAR szHost[ cchNAME ];

            WORKERPACKET wpNext;
            wpNext.wpBosHostGetNext.hEnum = wpBegin.wpBosHostGetBegin.hEnum;
            wpNext.wpBosHostGetNext.pszServer = szHost;

            if (!Worker_DoTask (wtaskBosHostGetNext, &wpNext, &status))
               {
               if (status == ADMITERATORDONE)
                  status = 0;
               else
                  rc = FALSE;
               break;
               }

            size_t iAdded;
            if ((iAdded = AfsClass_HostList_AddEntry (lpList, szHost)) != (size_t)-1)
               {
               lpList->aEntries[ iAdded ].fAdded = FALSE;
               }
            }

         WORKERPACKET wpDone;
         wpDone.wpBosHostGetDone.hEnum = wpBegin.wpBosHostGetBegin.hEnum;
         Worker_DoTask (wtaskBosHostGetDone, &wpDone);
         }
      }

   if ((lpServer = lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtHostListLoadEnd, lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpList : NULL;
}


LPHOSTLIST AfsClass_HostList_Copy (LPHOSTLIST lpOld)
{
   LPHOSTLIST lpNew = NULL;

   if (lpOld)
      {
      lpNew = New(HOSTLIST);
      memcpy (lpNew, lpOld, sizeof(HOSTLIST));

      lpNew->cRef = 1;
      lpNew->aEntries = 0;
      lpNew->cEntries = 0;

      if (REALLOC (lpNew->aEntries, lpNew->cEntries, lpOld->cEntries, cREALLOC_HOSTLISTENTRIES))
         {
         size_t cb = lpOld->cEntries * sizeof(HOSTLISTENTRY);
         memcpy (lpNew->aEntries, lpOld->aEntries, cb);
         }
      }

   return lpNew;
}


BOOL AfsClass_HostList_Save (LPHOSTLIST lpList, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtHostListSaveBegin, lpList->lpiServer);

   PVOID hCell;
   PVOID hBOS;
   LPSERVER lpServer;
   if ((lpServer = lpList->lpiServer->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
         rc = FALSE;
      lpServer->Close();
      }

   if (rc)
      {
      for (size_t iEntry = 0; iEntry < lpList->cEntries; ++iEntry)
         {
         if (!lpList->aEntries[ iEntry ].szHost[0])
            continue;

         // are we supposed to add this entry?
         //
         if (lpList->aEntries[ iEntry ].fAdded && !lpList->aEntries[ iEntry ].fDeleted)
            {
            WORKERPACKET wp;
            wp.wpBosHostCreate.hServer = hBOS;
            wp.wpBosHostCreate.pszServer = lpList->aEntries[ iEntry ].szHost;

            ULONG thisstatus;
            if (!Worker_DoTask (wtaskBosHostCreate, &wp, &thisstatus))
               {
               rc = FALSE;
               status = thisstatus;
               }
            else
               {
               lpList->aEntries[ iEntry ].fAdded = FALSE;
               }
            }

         // are we supposed to delete this entry?
         //
         if (!lpList->aEntries[ iEntry ].fAdded && lpList->aEntries[ iEntry ].fDeleted)
            {
            WORKERPACKET wp;
            wp.wpBosHostDelete.hServer = hBOS;
            wp.wpBosHostDelete.pszServer = lpList->aEntries[ iEntry ].szHost;

            ULONG thisstatus;
            if (!Worker_DoTask (wtaskBosHostDelete, &wp, &thisstatus))
               {
               rc = FALSE;
               status = thisstatus;
               }
            else
               {
               lpList->aEntries[ iEntry ].szHost[0] = TEXT('\0');
               lpList->aEntries[ iEntry ].fDeleted = FALSE;
               }
            }
         }
      }

   if ((lpServer = lpList->lpiServer->OpenServer (&status)) != NULL)
      {
      lpServer->CloseBosObject();
      lpServer->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtHostListSaveEnd, lpList->lpiServer, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


void AfsClass_HostList_Free (LPHOSTLIST lpList)
{
   if (lpList && !InterlockedDecrement (&lpList->cRef))
      {
      if (lpList->aEntries)
         Free (lpList->aEntries);
      memset (lpList, 0x00, sizeof(HOSTLIST));
      Delete (lpList);
      }
}


size_t AfsClass_HostList_AddEntry (LPHOSTLIST lpList, LPTSTR pszHost)
{
   size_t iAdded = (size_t)-1;

   if (lpList)
      {
      size_t iEntry;
      for (iEntry = 0; iEntry < lpList->cEntries; ++iEntry)
         {
         if (!lpList->aEntries[ iEntry ].szHost[0])
            break;
         }
      if (iEntry >= lpList->cEntries)
         {
         (void)REALLOC (lpList->aEntries, lpList->cEntries, 1+iEntry, cREALLOC_HOSTLISTENTRIES);
         }
      if (iEntry < lpList->cEntries)
         {
         iAdded = iEntry;
         lstrcpy (lpList->aEntries[ iAdded ].szHost, pszHost);
         lpList->aEntries[ iAdded ].fAdded = TRUE;
         lpList->aEntries[ iAdded ].fDeleted = FALSE;
         }
      }

   return iAdded;
}


BOOL AfsClass_HostList_DelEntry (LPHOSTLIST lpList, size_t iIndex)
{
   BOOL rc = FALSE;

   if ( lpList &&
        (iIndex < lpList->cEntries) &&
        (lpList->aEntries[ iIndex ].szHost[0]) &&
        (!lpList->aEntries[ iIndex ].fDeleted) )
      {
      if (lpList->aEntries[ iIndex ].fAdded)
         lpList->aEntries[ iIndex ].szHost[0] = TEXT('\0');
      else
         lpList->aEntries[ iIndex ].fDeleted = TRUE;

      rc = TRUE;
      }

   return rc;
}


BOOL AfsClass_GetPtsProperties (LPIDENT lpiCell, LPPTSPROPERTIES pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   memset (pProperties, 0x00, sizeof(PTSPROPERTIES));

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Go get the necessary properties
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsUserMaxGet.hCell = hCell;

      if ((rc = Worker_DoTask (wtaskPtsUserMaxGet, &wp, &status)) == TRUE)
         pProperties->idUserMax = wp.wpPtsUserMaxGet.idUserMax;
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsGroupMaxGet.hCell = hCell;

      if ((rc = Worker_DoTask (wtaskPtsGroupMaxGet, &wp, &status)) == TRUE)
         pProperties->idGroupMax = wp.wpPtsGroupMaxGet.idGroupMax;
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetPtsProperties (LPIDENT lpiCell, LPPTSPROPERTIES pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Modify the specified properties
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsUserMaxSet.hCell = hCell;
      wp.wpPtsUserMaxSet.idUserMax = pProperties->idUserMax;
      rc = Worker_DoTask (wtaskPtsUserMaxSet, &wp, &status);
      }

   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsGroupMaxSet.hCell = hCell;
      wp.wpPtsGroupMaxSet.idGroupMax = pProperties->idGroupMax;
      Worker_DoTask (wtaskPtsGroupMaxSet, &wp, &status);
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


LPIDENT AfsClass_CreateUser (LPIDENT lpiCell, LPTSTR pszUserName, LPTSTR pszInstance, LPTSTR pszPassword, UINT_PTR idUser, BOOL fCreateKAS, BOOL fCreatePTS, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   if (pszInstance && !*pszInstance)
      pszInstance = NULL;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCreateUserBegin, lpiCell, pszUserName, 0);

   // We'll need both hCell and hKAS.
   //
   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   // First try to create a KAS entry.
   //
   if (rc && fCreateKAS)
      {
      WORKERPACKET wp;
      wp.wpKasPrincipalCreate.hCell = hCell;
      wp.wpKasPrincipalCreate.hServer = hKAS;
      wp.wpKasPrincipalCreate.pszPrincipal = pszUserName;
      wp.wpKasPrincipalCreate.pszInstance = pszInstance;
      wp.wpKasPrincipalCreate.pszPassword = pszPassword;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskKasPrincipalCreate, &wp, &status);
      AfsClass_Enter();
      }

   // If that succeeded, try to create a PTS entry as well.
   //
   if (rc && fCreatePTS)
      {
      TCHAR szUserName[ cchNAME ];
      lstrcpy (szUserName, pszUserName);
      if (pszInstance)
         wsprintf (&szUserName[ lstrlen(szUserName) ], TEXT(".%s"), pszInstance);

      WORKERPACKET wp;
      wp.wpPtsUserCreate.hCell = hCell;
      wp.wpPtsUserCreate.pszUser = szUserName;
      wp.wpPtsUserCreate.idUser = (int) idUser;

      AfsClass_Leave();

      if ((rc = Worker_DoTask (wtaskPtsUserCreate, &wp, &status)) == FALSE)
         {
         if (status == PREXIST)
            rc = TRUE;
         }

      if (!rc)
         {
         // If we couldn't make a KAS entry as well, remove the KAS entry.
         //
         if (fCreateKAS)
            {
            WORKERPACKET wpDel;
            wpDel.wpKasPrincipalDelete.hCell = hCell;
            wpDel.wpKasPrincipalDelete.hServer = hKAS;
            wpDel.wpKasPrincipalDelete.pszPrincipal = pszUserName;
            wpDel.wpKasPrincipalDelete.pszInstance = pszInstance;
            Worker_DoTask (wtaskKasPrincipalDelete, &wpDel);
            }
         }

      AfsClass_Enter();
      }

   // If we were able to create the user's accounts successfully, refresh
   // the cell status and return the new user's ident.
   //
   LPIDENT lpiUser;

   if (rc)
      {
      if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         if (!lpCell->RefreshAccount (pszUserName, pszInstance, CELL_REFRESH_ACCOUNT_CREATED_USER, &lpiUser))
            rc = FALSE;
         else if (!lpiUser)
            rc = FALSE;
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCreateUserEnd, lpiCell, pszUserName, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpiUser : NULL;
}


BOOL AfsClass_SetUserProperties (LPIDENT lpiUser, LPUSERPROPERTIES pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtChangeUserBegin, lpiUser);

   // We'll need both hCell and hKAS.
   //
   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpiUser->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   // We'll also need this user's current status
   //
   LPUSER lpUser;
   USERSTATUS us;
   if ((lpUser = lpiUser->OpenUser (&status)) == NULL)
      rc = FALSE;
   else
      {
      if (!lpUser->GetStatus (&us, TRUE, &status))
         rc = FALSE;
      lpUser->Close();
      }

   // Modify the user's KAS entry (if necessary)
   //
   DWORD dwKasMask = ( MASK_USERPROP_fAdmin |
                       MASK_USERPROP_fGrantTickets |
                       MASK_USERPROP_fCanEncrypt |
                       MASK_USERPROP_fCanChangePassword |
                       MASK_USERPROP_fCanReusePasswords |
                       MASK_USERPROP_timeAccountExpires |
                       MASK_USERPROP_cdayPwExpires |
                       MASK_USERPROP_csecTicketLifetime |
                       MASK_USERPROP_nFailureAttempts |
                       MASK_USERPROP_csecFailedLoginLockTime );

   if (rc && (pProperties->dwMask & dwKasMask))
      {
      TCHAR szPrincipal[ cchNAME ];
      TCHAR szInstance[ cchNAME ];
      lpiUser->GetUserName (szPrincipal, szInstance);

      WORKERPACKET wp;
      wp.wpKasPrincipalFieldsSet.hCell = hCell;
      wp.wpKasPrincipalFieldsSet.hServer = hKAS;
      wp.wpKasPrincipalFieldsSet.pszPrincipal = szPrincipal;
      wp.wpKasPrincipalFieldsSet.pszInstance = szInstance;
      wp.wpKasPrincipalFieldsSet.fIsAdmin = (pProperties->dwMask & MASK_USERPROP_fAdmin) ? pProperties->fAdmin : us.KASINFO.fIsAdmin;
      wp.wpKasPrincipalFieldsSet.fGrantTickets = (pProperties->dwMask & MASK_USERPROP_fGrantTickets) ? pProperties->fGrantTickets : us.KASINFO.fCanGetTickets;
      wp.wpKasPrincipalFieldsSet.fCanEncrypt = (pProperties->dwMask & MASK_USERPROP_fCanEncrypt) ? pProperties->fCanEncrypt : us.KASINFO.fEncrypt;
      wp.wpKasPrincipalFieldsSet.fCanChangePassword = (pProperties->dwMask & MASK_USERPROP_fCanChangePassword) ? pProperties->fCanChangePassword : us.KASINFO.fCanChangePassword;
      wp.wpKasPrincipalFieldsSet.fCanReusePasswords = (pProperties->dwMask & MASK_USERPROP_fCanReusePasswords) ? pProperties->fCanReusePasswords : us.KASINFO.fCanReusePasswords;
      memcpy (&wp.wpKasPrincipalFieldsSet.timeExpires, (pProperties->dwMask & MASK_USERPROP_timeAccountExpires) ? &pProperties->timeAccountExpires : &us.KASINFO.timeExpires, sizeof(SYSTEMTIME));
      wp.wpKasPrincipalFieldsSet.cdayPwExpires = (pProperties->dwMask & MASK_USERPROP_cdayPwExpires) ? pProperties->cdayPwExpires : us.KASINFO.cdayPwExpire;
      wp.wpKasPrincipalFieldsSet.csecTicketLifetime = (pProperties->dwMask & MASK_USERPROP_csecTicketLifetime) ? pProperties->csecTicketLifetime : us.KASINFO.csecTicketLifetime;
      wp.wpKasPrincipalFieldsSet.nFailureAttempts = (pProperties->dwMask & MASK_USERPROP_nFailureAttempts) ? pProperties->nFailureAttempts : us.KASINFO.cFailLogin;
      wp.wpKasPrincipalFieldsSet.csecFailedLoginLockTime = (pProperties->dwMask & MASK_USERPROP_csecFailedLoginLockTime) ? pProperties->csecFailedLoginLockTime : us.KASINFO.csecFailLoginLock;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskKasPrincipalFieldsSet, &wp, &status);
      AfsClass_Enter();
      }


   // Modify the user's PTS entry (if necessary)
   //
   DWORD dwPtsMask = ( MASK_USERPROP_cGroupCreationQuota |
                       MASK_USERPROP_aaListStatus |
                       MASK_USERPROP_aaGroupsOwned |
                       MASK_USERPROP_aaMembership );

   if (rc && (pProperties->dwMask & dwPtsMask))
      {
      TCHAR szFullName[ cchNAME ];
      lpiUser->GetFullUserName (szFullName);

      WORKERPACKET wp;
      wp.wpPtsUserModify.hCell = hCell;
      wp.wpPtsUserModify.pszUser = szFullName;
      memset (&wp.wpPtsUserModify.Delta, 0x00, sizeof(wp.wpPtsUserModify.Delta));

      if (pProperties->dwMask & MASK_USERPROP_cGroupCreationQuota)
         {
         wp.wpPtsUserModify.Delta.flag = (pts_UserUpdateFlag_t)( (LONG)wp.wpPtsUserModify.Delta.flag | (LONG)PTS_USER_UPDATE_GROUP_CREATE_QUOTA );
         wp.wpPtsUserModify.Delta.groupCreationQuota = pProperties->cGroupCreationQuota;
         }

      if (pProperties->dwMask & (MASK_USERPROP_aaListStatus | MASK_USERPROP_aaGroupsOwned | MASK_USERPROP_aaMembership))
         {
         wp.wpPtsUserModify.Delta.flag = (pts_UserUpdateFlag_t)( (LONG)wp.wpPtsUserModify.Delta.flag | (LONG)PTS_USER_UPDATE_PERMISSIONS );
         wp.wpPtsUserModify.Delta.listStatus = ACCOUNTACCESS_TO_USERACCESS( (pProperties->dwMask & MASK_USERPROP_aaListStatus) ? pProperties->aaListStatus : us.PTSINFO.aaListStatus );
         wp.wpPtsUserModify.Delta.listGroupsOwned = ACCOUNTACCESS_TO_USERACCESS( (pProperties->dwMask & MASK_USERPROP_aaGroupsOwned) ? pProperties->aaGroupsOwned : us.PTSINFO.aaGroupsOwned );
         wp.wpPtsUserModify.Delta.listMembership = ACCOUNTACCESS_TO_USERACCESS( (pProperties->dwMask & MASK_USERPROP_aaMembership) ? pProperties->aaMembership : us.PTSINFO.aaMembership );
         }

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsUserModify, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to modify the user's properties successfully, refresh
   // that user's status.
   //
   if ((lpUser = lpiUser->OpenUser (&status)) != NULL)
      {
      lpUser->Invalidate();
      lpUser->RefreshStatus();
      lpUser->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtChangeUserBegin, lpiUser, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetUserPassword (LPIDENT lpiUser, int keyVersion, LPTSTR pszPassword, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   TCHAR szCell[ cchNAME ];
   lpiUser->GetCellName (szCell);

   WORKERPACKET wp;
   wp.wpKasStringToKey.pszCell = szCell;
   wp.wpKasStringToKey.pszString = pszPassword;
   if (!Worker_DoTask (wtaskKasStringToKey, &wp, &status))
      {
      rc = FALSE;
      }
   else if (!AfsClass_SetUserPassword (lpiUser, keyVersion, &wp.wpKasStringToKey.key, &status))
      {
      rc = FALSE;
      }

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_SetUserPassword (LPIDENT lpiUser, int keyVersion, LPENCRYPTIONKEY pKey, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtChangeUserPasswordBegin, lpiUser);

   // We'll need both hCell and hKAS.
   //
   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpiUser->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   // Change the user's password
   //
   if (rc)
      {
      TCHAR szPrincipal[ cchNAME ];
      TCHAR szInstance[ cchNAME ];
      lpiUser->GetUserName (szPrincipal, szInstance);

      WORKERPACKET wp;
      wp.wpKasPrincipalKeySet.hCell = hCell;
      wp.wpKasPrincipalKeySet.hServer = hKAS;
      wp.wpKasPrincipalKeySet.pszPrincipal = szPrincipal;
      wp.wpKasPrincipalKeySet.pszInstance = szInstance;
      wp.wpKasPrincipalKeySet.keyVersion = keyVersion;
      memcpy (&wp.wpKasPrincipalKeySet.key.key, &pKey->key, ENCRYPTIONKEY_LEN);

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskKasPrincipalKeySet, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to modify the user's password successfully, refresh
   // that user's status.
   //
   LPUSER lpUser;
   if ((lpUser = lpiUser->OpenUser (&status)) != NULL)
      {
      lpUser->Invalidate();
      lpUser->RefreshStatus();
      lpUser->Close();
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtChangeUserPasswordEnd, lpiUser, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_DeleteUser (LPIDENT lpiUser, BOOL fDeleteKAS, BOOL fDeletePTS, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteUserBegin, lpiUser);

   // We'll need both hCell and hKAS.
   //
   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpiUser->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   // Find out whether this user has KAS and/or PTS entries. Also
   // get the various lists of groups for this user...
   //
   LPUSER lpUser;
   LPTSTR mszOwnerOf = NULL;
   LPTSTR mszMemberOf = NULL;
   if ((lpUser = lpiUser->OpenUser (&status)) == NULL)
      rc = FALSE;
   else
      {
      lpUser->GetOwnerOf (&mszOwnerOf);
      lpUser->GetMemberOf (&mszMemberOf);
      lpUser->Close();
      }

   // Delete the user's PTS entry
   //
   if (rc && fDeletePTS)
      {
      TCHAR szFullName[ cchNAME ];
      lpiUser->GetFullUserName (szFullName);

      WORKERPACKET wp;
      wp.wpPtsUserDelete.hCell = hCell;
      wp.wpPtsUserDelete.pszUser = szFullName;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskPtsUserDelete, &wp, &status)) == FALSE)
         {
         if (status == ADMPTSFAILEDNAMETRANSLATE) // User had no PTS entry?
            rc = TRUE;
         }
      AfsClass_Enter();
      }

   // Delete the user's KAS entry
   //
   if (rc && fDeleteKAS)
      {
      TCHAR szPrincipal[ cchNAME ];
      TCHAR szInstance[ cchNAME ];
      lpiUser->GetUserName (szPrincipal, szInstance);

      WORKERPACKET wp;
      wp.wpKasPrincipalDelete.hCell = hCell;
      wp.wpKasPrincipalDelete.hServer = hKAS;
      wp.wpKasPrincipalDelete.pszPrincipal = szPrincipal;
      wp.wpKasPrincipalDelete.pszInstance = szInstance;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskKasPrincipalDelete, &wp, &status)) == FALSE)
         {
         if (status == KANOENT)
            rc = TRUE;
         }
      AfsClass_Enter();
      }

   // If we were able to delete the user's accounts successfully, refresh
   // the cell status.
   //
   if (rc)
      {
      if ((lpCell = lpiUser->OpenCell (&status)) != NULL)
         {
         TCHAR szPrincipal[ cchNAME ];
         TCHAR szInstance[ cchNAME ];
         lpiUser->GetUserName (szPrincipal, szInstance);

         lpCell->RefreshAccount (szPrincipal, szInstance, CELL_REFRESH_ACCOUNT_DELETED);
         lpCell->RefreshAccounts (mszOwnerOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszMemberOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteUserEnd, lpiUser, status);
   AfsClass_Leave();

   if (mszOwnerOf)
      FreeString (mszOwnerOf);
   if (mszMemberOf)
      FreeString (mszMemberOf);
   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_UnlockUser (LPIDENT lpiUser, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockUserBegin, lpiUser);

   // We'll need both hCell and hKAS.
   //
   PVOID hCell;
   PVOID hKAS;
   LPCELL lpCell;
   if ((lpCell = lpiUser->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      else
         hKAS = lpCell->GetKasObject (&status);
      lpCell->Close();
      }

   // Unlock the user's KAS entry
   //
   if (rc)
      {
      TCHAR szPrincipal[ cchNAME ];
      TCHAR szInstance[ cchNAME ];
      lpiUser->GetUserName (szPrincipal, szInstance);

      WORKERPACKET wp;
      wp.wpKasPrincipalUnlock.hCell = hCell;
      wp.wpKasPrincipalUnlock.hServer = hKAS;
      wp.wpKasPrincipalUnlock.pszPrincipal = szPrincipal;
      wp.wpKasPrincipalUnlock.pszInstance = szInstance;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskKasPrincipalUnlock, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to unlock the user's accounts successfully, refresh
   // the user's properties.
   //
   if (rc)
      {
      LPUSER lpUser;
      if ((lpUser = lpiUser->OpenUser (&status)) != NULL)
         {
         lpUser->Invalidate();
         lpUser->RefreshStatus();
         lpUser->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtUnlockUserEnd, lpiUser, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


LPIDENT AfsClass_CreateGroup (LPIDENT lpiCell, LPTSTR pszGroupName, LPIDENT lpiOwner, int idGroup, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtCreateGroupBegin, lpiCell, pszGroupName, 0);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Create a PTS entry for the new group
   //
   if (rc)
      {
      TCHAR szOwner[ cchNAME ] = TEXT("");
      if (lpiOwner && lpiOwner->fIsUser())
         lpiOwner->GetFullUserName (szOwner);
      else if (lpiOwner && lpiOwner->fIsGroup())
         lpiOwner->GetGroupName (szOwner);

      WORKERPACKET wp;
      wp.wpPtsGroupCreate.hCell = hCell;
      wp.wpPtsGroupCreate.pszGroup = pszGroupName;
      wp.wpPtsGroupCreate.pszOwner = (szOwner[0]) ? szOwner : NULL;
      wp.wpPtsGroupCreate.idGroup = idGroup;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupCreate, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to create the group successfully, refresh
   // the cell status and return the new group's ident.
   //
   LPIDENT lpiGroup;

   if (rc)
      {
      if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         if (!lpCell->RefreshAccount (pszGroupName, NULL, CELL_REFRESH_ACCOUNT_CREATED_GROUP, &lpiGroup))
            rc = FALSE;
         else if (!lpiGroup)
            rc = FALSE;
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtCreateGroupEnd, lpiCell, pszGroupName, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return (rc) ? lpiGroup : NULL;
}


BOOL AfsClass_SetGroupProperties (LPIDENT lpiGroup, LPGROUPPROPERTIES pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtChangeGroupBegin, lpiGroup);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiGroup->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // We'll also need this group's current status
   //
   LPPTSGROUP lpGroup;
   PTSGROUPSTATUS gs;
   if ((lpGroup = lpiGroup->OpenGroup (&status)) == NULL)
      rc = FALSE;
   else
      {
      if (!lpGroup->GetStatus (&gs, TRUE, &status))
         rc = FALSE;
      lpGroup->Close();
      }

   // Modify the group's PTS entry (if requested)
   //
   DWORD dwPtsMask = ( MASK_GROUPPROP_aaListStatus |
                       MASK_GROUPPROP_aaListGroupsOwned |
                       MASK_GROUPPROP_aaListMembers |
                       MASK_GROUPPROP_aaAddMember |
                       MASK_GROUPPROP_aaDeleteMember );

   if (rc && (pProperties->dwMask & dwPtsMask))
      {
      TCHAR szGroup[ cchNAME ];
      lpiGroup->GetGroupName (szGroup);

      WORKERPACKET wp;
      wp.wpPtsGroupModify.hCell = hCell;
      wp.wpPtsGroupModify.pszGroup = szGroup;
      memset (&wp.wpPtsGroupModify.Delta, 0x00, sizeof(wp.wpPtsGroupModify.Delta));
      wp.wpPtsGroupModify.Delta.listStatus = ACCOUNTACCESS_TO_GROUPACCESS( (pProperties->dwMask & MASK_GROUPPROP_aaListStatus) ? pProperties->aaListStatus : gs.aaListStatus );
      wp.wpPtsGroupModify.Delta.listGroupsOwned = ACCOUNTACCESS_TO_GROUPACCESS( (pProperties->dwMask & MASK_GROUPPROP_aaListGroupsOwned) ? pProperties->aaListGroupsOwned : gs.aaListGroupsOwned );
      wp.wpPtsGroupModify.Delta.listMembership = ACCOUNTACCESS_TO_GROUPACCESS( (pProperties->dwMask & MASK_GROUPPROP_aaListMembers) ? pProperties->aaListMembers : gs.aaListMembers );
      wp.wpPtsGroupModify.Delta.listAdd = ACCOUNTACCESS_TO_GROUPACCESS( (pProperties->dwMask & MASK_GROUPPROP_aaAddMember) ? pProperties->aaAddMember : gs.aaAddMember );
      wp.wpPtsGroupModify.Delta.listDelete = ACCOUNTACCESS_TO_GROUPACCESS( (pProperties->dwMask & MASK_GROUPPROP_aaDeleteMember) ? pProperties->aaDeleteMember : gs.aaDeleteMember );

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupModify, &wp, &status);
      AfsClass_Enter();
      }

   // Change the group's owner (if requested)
   //
   if (rc && (pProperties->dwMask & MASK_GROUPPROP_szOwner))
      {
      TCHAR szGroup[ cchNAME ];
      lpiGroup->GetGroupName (szGroup);

      WORKERPACKET wp;
      wp.wpPtsGroupOwnerChange.hCell = hCell;
      wp.wpPtsGroupOwnerChange.pszGroup = szGroup;
      wp.wpPtsGroupOwnerChange.pszOwner = pProperties->szOwner;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupOwnerChange, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to modify the group's properties successfully, refresh
   // either the group's status. If the group's owner changed, also refresh
   // the group's old and new owners.
   //
   if (rc)
      {
      if ((lpCell = lpiGroup->OpenCell (&status)) != NULL)
         {
         TCHAR szAccount[ cchNAME ];
         lpiGroup->GetGroupName (szAccount);
         lpCell->RefreshAccount (szAccount, NULL, CELL_REFRESH_ACCOUNT_CHANGED);

         if (pProperties->dwMask & MASK_GROUPPROP_szOwner)
            {
            lpCell->RefreshAccount (gs.szOwner, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
            lpCell->RefreshAccount (pProperties->szOwner, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
            }

         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtChangeGroupBegin, lpiGroup, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_RenameGroup (LPIDENT lpiGroup, LPTSTR pszNewName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtRenameGroupBegin, lpiGroup);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiGroup->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Get this group's list of members (etc)
   //
   LPTSTR mszOwnerOf = NULL;
   LPTSTR mszMemberOf = NULL;
   LPTSTR mszMembers = NULL;

   LPPTSGROUP lpGroup;
   if ((lpGroup = lpiGroup->OpenGroup (&status)) != NULL)
      {
      lpGroup->GetOwnerOf (&mszOwnerOf);
      lpGroup->GetMemberOf (&mszMemberOf);
      lpGroup->GetMembers (&mszMembers);
      lpGroup->Close();
      }

   // Rename the group's PTS entry
   //
   if (rc)
      {
      TCHAR szGroup[ cchNAME ];
      lpiGroup->GetGroupName (szGroup);

      WORKERPACKET wp;
      wp.wpPtsGroupRename.hCell = hCell;
      wp.wpPtsGroupRename.pszGroup = szGroup;
      wp.wpPtsGroupRename.pszNewName = pszNewName;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupRename, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to rename the group successfully, refresh the cell status.
   //
   if (rc)
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = lpiGroup->OpenGroup (&status)) != NULL)
         {
         lpGroup->ChangeIdentName (pszNewName);
         lpGroup->Close();
         }
      if ((lpCell = lpiGroup->OpenCell (&status)) != NULL)
         {
         lpCell->RefreshAccount (pszNewName, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszOwnerOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszMemberOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszMembers, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtRenameGroupEnd, lpiGroup, status);
   AfsClass_Leave();

   if (mszOwnerOf)
      FreeString (mszOwnerOf);
   if (mszMemberOf)
      FreeString (mszMemberOf);
   if (mszMembers)
      FreeString (mszMembers);
   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_DeleteGroup (LPIDENT lpiGroup, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteGroupBegin, lpiGroup);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiGroup->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   // Get this group's list of members (etc)
   //
   LPTSTR mszOwnerOf = NULL;
   LPTSTR mszMemberOf = NULL;
   LPTSTR mszMembers = NULL;

   LPPTSGROUP lpGroup;
   if ((lpGroup = lpiGroup->OpenGroup (&status)) != NULL)
      {
      lpGroup->GetOwnerOf (&mszOwnerOf);
      lpGroup->GetMemberOf (&mszMemberOf);
      lpGroup->GetMembers (&mszMembers);
      lpGroup->Close();
      }

   // Delete the group's PTS entry
   //
   if (rc)
      {
      TCHAR szGroup[ cchNAME ];
      lpiGroup->GetGroupName (szGroup);

      WORKERPACKET wp;
      wp.wpPtsGroupDelete.hCell = hCell;
      wp.wpPtsGroupDelete.pszGroup = szGroup;

      AfsClass_Leave();
      if ((rc = Worker_DoTask (wtaskPtsGroupDelete, &wp, &status)) == FALSE)
         {
         if (status == ADMPTSFAILEDNAMETRANSLATE) // Group had no PTS entry?
            rc = TRUE;
         }
      AfsClass_Enter();
      }

   // If we were able to delete the group successfully, refresh the cell status.
   //
   if (rc)
      {
      if ((lpCell = lpiGroup->OpenCell (&status)) != NULL)
         {
         TCHAR szGroup[ cchNAME ];
         lpiGroup->GetGroupName (szGroup);
         lpCell->RefreshAccounts (mszOwnerOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszMemberOf, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccounts (mszMembers, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccount (szGroup, NULL, CELL_REFRESH_ACCOUNT_DELETED);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtDeleteGroupEnd, lpiGroup, status);
   AfsClass_Leave();

   if (mszOwnerOf)
      FreeString (mszOwnerOf);
   if (mszMemberOf)
      FreeString (mszMemberOf);
   if (mszMembers)
      FreeString (mszMembers);
   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_AddUserToGroup (LPIDENT lpiGroup, LPIDENT lpiUser, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtGroupMemberAddBegin, lpiGroup, lpiUser, NULL, NULL, 0, 0);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiGroup->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   TCHAR szGroup[ cchNAME ];
   lpiGroup->GetGroupName (szGroup);

   TCHAR szMember[ cchNAME ];
   if (lpiUser->fIsUser())
      lpiUser->GetFullUserName (szMember);
   else // (lpiUser->fIsGroup())
      lpiUser->GetGroupName (szMember);

   // Add this user to the specified group
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsGroupMemberAdd.hCell = hCell;
      wp.wpPtsGroupMemberAdd.pszGroup = szGroup;
      wp.wpPtsGroupMemberAdd.pszUser = szMember;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupMemberAdd, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to change the group successfully, update the group's
   // and user's properties.
   //
   if (rc)
      {
      if ((lpCell = lpiGroup->OpenCell (&status)) != NULL)
         {
         lpCell->RefreshAccount (szGroup, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccount (szMember, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtGroupMemberAddEnd, lpiGroup, lpiUser, NULL, NULL, 0, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}


BOOL AfsClass_RemoveUserFromGroup (LPIDENT lpiGroup, LPIDENT lpiUser, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status;

   AfsClass_Enter();
   NOTIFYCALLBACK::SendNotificationToAll (evtGroupMemberRemoveBegin, lpiGroup, lpiUser, NULL, NULL, 0, 0);

   // Obtain hCell
   //
   PVOID hCell;
   LPCELL lpCell;
   if ((lpCell = lpiGroup->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      if ((hCell = lpCell->GetCellObject (&status)) == NULL)
         rc = FALSE;
      lpCell->Close();
      }

   TCHAR szGroup[ cchNAME ];
   lpiGroup->GetGroupName (szGroup);

   TCHAR szMember[ cchNAME ];
   if (lpiUser->fIsUser())
      lpiUser->GetFullUserName (szMember);
   else // (lpiUser->fIsGroup())
      lpiUser->GetGroupName (szMember);

   // Remove this user from the specified group
   //
   if (rc)
      {
      WORKERPACKET wp;
      wp.wpPtsGroupMemberRemove.hCell = hCell;
      wp.wpPtsGroupMemberRemove.pszGroup = szGroup;
      wp.wpPtsGroupMemberRemove.pszUser = szMember;

      AfsClass_Leave();
      rc = Worker_DoTask (wtaskPtsGroupMemberRemove, &wp, &status);
      AfsClass_Enter();
      }

   // If we were able to change the group successfully, update the group's
   // and user's properties.
   //
   if (rc)
      {
      if ((lpCell = lpiGroup->OpenCell (&status)) != NULL)
         {
         lpCell->RefreshAccount (szGroup, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->RefreshAccount (szMember, NULL, CELL_REFRESH_ACCOUNT_CHANGED);
         lpCell->Close();
         }
      }

   NOTIFYCALLBACK::SendNotificationToAll (evtGroupMemberRemoveEnd, lpiGroup, lpiUser, NULL, NULL, 0, status);
   AfsClass_Leave();

   if (pStatus && !rc)
      *pStatus = status;
   return rc;
}

