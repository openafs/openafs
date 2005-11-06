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

#include "TaAfsAdmSvrInternal.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   ASID idCell;
   DWORD timeLastRefresh;
   DWORD cminRefreshRate;
   HANDLE hThread;
   } REFRESHTHREAD, *LPREFRESHTHREAD;

static struct
   {
   REFRESHTHREAD *aRefreshThreads;
   size_t cRefreshThreads;
   CRITICAL_SECTION *pcsRefreshThreads;
   } l;


/*
 * AUTO-REFRESH _______________________________________________________________
 *
 */

DWORD WINAPI AfsAdmSvr_AutoRefreshThread (PVOID lp)
{
   ASID idCell = (ASID)lp;
   for (;;)
      {
      EnterCriticalSection (l.pcsRefreshThreads);

      BOOL fFound = FALSE;
      DWORD cminRefreshRate;
      DWORD timeLastRefresh;
      for (size_t iThread = 0; (!fFound) && (iThread < l.cRefreshThreads); ++iThread)
         {
         LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
         if (pThread->idCell == idCell)
            {
            fFound = TRUE;
            cminRefreshRate = pThread->cminRefreshRate;
            timeLastRefresh = pThread->timeLastRefresh;
            }
         }

      LeaveCriticalSection (l.pcsRefreshThreads);

      if (!fFound)
         break;

      DWORD timeNextRefresh = timeLastRefresh + cminRefreshRate;
      if ((cminRefreshRate) && ((!timeLastRefresh) || (AfsAdmSvr_GetCurrentTime() >= timeNextRefresh)))
         {
         Print (dlDETAIL, TEXT("Auto-refresh: %lu minutes elapsed; refreshing cell 0x%08lX"), cminRefreshRate, idCell);

         LPCELL lpCell;
         if ((lpCell = ((LPIDENT)idCell)->OpenCell()) != NULL)
            {
            lpCell->Invalidate();

            ULONG status;
            if (!lpCell->RefreshAll (&status))
               Print (dlERROR, TEXT("Auto-refresh: RefreshCell failed; error 0x%08lX"), status);
            else
               Print (dlSTANDARD, TEXT("Auto-refresh of cell 0x%08lX successful"), idCell);
            lpCell->Close();
            }
         }

      Sleep (60L * 1000L); // sleep for one minute
      }

   return 0;
}


void AfsAdmSvr_PrepCellRefresh (void)
{
   if (!l.pcsRefreshThreads)
      {
      l.pcsRefreshThreads = New (CRITICAL_SECTION);
      InitializeCriticalSection (l.pcsRefreshThreads);
      }
}


void AfsAdmSvr_SetCellRefreshRate (ASID idCell, ULONG cminRefreshRate)
{
   AfsAdmSvr_PrepCellRefresh();
   EnterCriticalSection (l.pcsRefreshThreads);

   size_t iThread;
   for (iThread = 0; iThread < l.cRefreshThreads; ++iThread)
      {
      LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
      if (pThread->idCell == idCell)
         {
         pThread->cminRefreshRate = cminRefreshRate;
         break;
         }
      }
   if (iThread == l.cRefreshThreads)
      {
      for (iThread = 0; iThread < l.cRefreshThreads; ++iThread)
         {
         LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
         if (pThread->idCell == 0)
            break;
         }
      if (REALLOC (l.aRefreshThreads, l.cRefreshThreads, 1+iThread, 1))
         {
         DWORD idThread;

         LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
         pThread->idCell = idCell;
         pThread->timeLastRefresh = AfsAdmSvr_GetCurrentTime();
         pThread->cminRefreshRate = cminRefreshRate;
         pThread->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)AfsAdmSvr_AutoRefreshThread, (LPVOID)idCell, 0, &idThread);
         }
      }

   LeaveCriticalSection (l.pcsRefreshThreads);
}


void AfsAdmSvr_StopCellRefreshThread (ASID idCell)
{
   AfsAdmSvr_PrepCellRefresh();
   EnterCriticalSection (l.pcsRefreshThreads);

   for (size_t iThread = 0; iThread < l.cRefreshThreads; ++iThread)
      {
      LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
      if (pThread->idCell == idCell)
         pThread->idCell = 0;
      }

   LeaveCriticalSection (l.pcsRefreshThreads);
}


void AfsAdmSvr_MarkRefreshThread (ASID idCell)
{
   AfsAdmSvr_PrepCellRefresh();
   EnterCriticalSection (l.pcsRefreshThreads);

   for (size_t iThread = 0; iThread < l.cRefreshThreads; ++iThread)
      {
      LPREFRESHTHREAD pThread = &l.aRefreshThreads[ iThread ];
      if (pThread->idCell == idCell)
         pThread->timeLastRefresh = AfsAdmSvr_GetCurrentTime();
      }

   LeaveCriticalSection (l.pcsRefreshThreads);
}


/*
 * OBJECT PROPERTIES __________________________________________________________
 *
 */

void AfsAdmSvr_ObtainRudimentaryProperties (LPASOBJPROP pProperties)
{
   LPIDENT lpi = (LPIDENT)(pProperties->idObject);

   switch (GetAsidType (pProperties->idObject))
      {
      case itCELL:
         pProperties->Type = TYPE_CELL;
         lpi->GetCellName (pProperties->szName);
         break;

      case itSERVER:
         pProperties->Type = TYPE_SERVER;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         lpi->GetServerName (pProperties->szName);
         break;

      case itSERVICE:
         pProperties->Type = TYPE_SERVICE;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         pProperties->idParentServer = (ASID)( lpi->GetServer() );
         lpi->GetServiceName (pProperties->szName);
         break;

      case itAGGREGATE:
         pProperties->Type = TYPE_PARTITION;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         pProperties->idParentServer = (ASID)( lpi->GetServer() );
         lpi->GetAggregateName (pProperties->szName);
         break;

      case itFILESET:
         pProperties->Type = TYPE_VOLUME;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         pProperties->idParentServer = (ASID)( lpi->GetServer() );
         pProperties->idParentPartition = (ASID)( lpi->GetAggregate() );
         lpi->GetFilesetName (pProperties->szName);
         break;

      case itUSER:
         pProperties->Type = TYPE_USER;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         lpi->GetUserName (pProperties->szName);
         break;

      case itGROUP:
         pProperties->Type = TYPE_GROUP;
         pProperties->idParentCell = (ASID)( lpi->GetCell() );
         lpi->GetGroupName (pProperties->szName);
         break;

      default:
         pProperties->verProperties = verPROP_NO_OBJECT;
         return;
      }

   if (pProperties->verProperties == verPROP_NO_OBJECT)
      pProperties->verProperties = verPROP_RUDIMENTARY;
}


BOOL AfsAdmSvr_ObtainFullProperties (LPASOBJPROP pProperties, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   AfsAdmSvr_ObtainRudimentaryProperties (pProperties);

   if (pProperties->verProperties != verPROP_NO_OBJECT)
      {
      LPIDENT lpi = (LPIDENT)(pProperties->idObject);

      switch (GetAsidType (pProperties->idObject))
         {
         case itCELL:
            PTSPROPERTIES PtsProperties;
            if ((rc = AfsClass_GetPtsProperties (lpi, &PtsProperties, &status)) == TRUE)
               {
               pProperties->u.CellProperties.idUserMax = (DWORD)PtsProperties.idUserMax;
               pProperties->u.CellProperties.idGroupMax = (DWORD)PtsProperties.idGroupMax;
               }
            break;

         case itSERVER:
            LPSERVER lpServer;
            if ((lpServer = lpi->OpenServer (&status)) == NULL)
               rc = FALSE;
            else
               {
               SERVERSTATUS ss;
               if (!lpServer->GetStatus (&ss, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  pProperties->u.ServerProperties.nAddresses = 0;
                  for (size_t ii = 0; ii < ss.nAddresses; ++ii)
                     {
                     if (!ss.aAddresses[ii].sin_addr.s_addr)
                        continue;
                     if (pProperties->u.ServerProperties.nAddresses < ASOBJPROP_SERVER_MAXADDRESS)
                        {
                        pProperties->u.ServerProperties.aAddresses[ pProperties->u.ServerProperties.nAddresses ] = ntohl(ss.aAddresses[ii].sin_addr.s_addr);
                        pProperties->u.ServerProperties.nAddresses ++;
                        }
                     }
                  }
               lpServer->Close();
               }
            break;

         case itSERVICE:
            LPSERVICE lpService;
            if ((lpService = lpi->OpenService (&status)) == NULL)
               rc = FALSE;
            else
               {
               SERVICESTATUS ss;
               if (!lpService->GetStatus (&ss, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  pProperties->u.ServiceProperties.ServiceType = ss.type;
                  pProperties->u.ServiceProperties.ServiceState = ss.state;
                  lstrcpy (pProperties->u.ServiceProperties.szAuxStatus, ss.szAuxStatus);
                  lstrcpy (pProperties->u.ServiceProperties.szParams, ss.szParams);
                  lstrcpy (pProperties->u.ServiceProperties.szNotifier, ss.szNotifier);
                  memcpy (&pProperties->u.ServiceProperties.timeLastStart, &ss.timeLastStart, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.ServiceProperties.timeLastStop, &ss.timeLastStop, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.ServiceProperties.timeLastFail, &ss.timeLastFail, sizeof(SYSTEMTIME));
                  pProperties->u.ServiceProperties.nStarts = ss.nStarts;
                  pProperties->u.ServiceProperties.dwErrLast = ss.dwErrLast;
                  pProperties->u.ServiceProperties.dwSigLast = ss.dwSigLast;
                  }
               lpService->Close();
               }
            break;

         case itAGGREGATE:
            LPAGGREGATE lpAggregate;
            if ((lpAggregate = lpi->OpenAggregate(&status)) == NULL)
               rc = FALSE;
            else
               {
               AGGREGATESTATUS as;
               if (!lpAggregate->GetStatus (&as, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  pProperties->u.PartitionProperties.dwID = as.dwID;
                  pProperties->u.PartitionProperties.ckStorageTotal = as.ckStorageTotal;
                  pProperties->u.PartitionProperties.ckStorageFree = as.ckStorageFree;
                  pProperties->u.PartitionProperties.ckStorageAllocated = as.ckStorageAllocated;
                  }
               lpAggregate->Close();
               }
            break;

         case itFILESET:
            LPFILESET lpFileset;
            if ((lpFileset = lpi->OpenFileset(&status)) == NULL)
               rc = FALSE;
            else
               {
               FILESETSTATUS fs;
               if (!lpFileset->GetStatus (&fs, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  pProperties->u.VolumeProperties.id = fs.id;
                  pProperties->u.VolumeProperties.idReadWrite = fs.idReadWrite;
                  pProperties->u.VolumeProperties.idReplica = fs.idReplica;
                  pProperties->u.VolumeProperties.idClone = fs.idClone;
                  memcpy (&pProperties->u.VolumeProperties.timeCreation, &fs.timeCreation, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.VolumeProperties.timeLastUpdate, &fs.timeLastUpdate, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.VolumeProperties.timeLastAccess, &fs.timeLastAccess, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.VolumeProperties.timeLastBackup, &fs.timeLastBackup, sizeof(SYSTEMTIME));
                  memcpy (&pProperties->u.VolumeProperties.timeCopyCreation, &fs.timeCopyCreation, sizeof(SYSTEMTIME));
                  pProperties->u.VolumeProperties.nFiles = fs.nFiles;
                  pProperties->u.VolumeProperties.ckQuota = (UINT_PTR)fs.ckQuota;
                  pProperties->u.VolumeProperties.ckUsed = (UINT_PTR)fs.ckUsed;
                  pProperties->u.VolumeProperties.FilesetType = fs.Type;
                  pProperties->u.VolumeProperties.FilesetState = fs.State;
                  }
               lpFileset->Close();
               }
            break;

         case itUSER:
            LPUSER lpUser;
            if ((lpUser = lpi->OpenUser (&status)) == NULL)
               rc = FALSE;
            else
               {
               USERSTATUS us;
               if (!lpUser->GetStatus (&us, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  lpUser->GetName (NULL, pProperties->u.UserProperties.szInstance);

                  if ((pProperties->u.UserProperties.fHaveKasInfo = us.fHaveKasInfo) == TRUE)
                     {
                     pProperties->u.UserProperties.KASINFO.fIsAdmin = us.KASINFO.fIsAdmin;
                     pProperties->u.UserProperties.KASINFO.fCanGetTickets = us.KASINFO.fCanGetTickets;
                     pProperties->u.UserProperties.KASINFO.fEncrypt = us.KASINFO.fEncrypt;
                     pProperties->u.UserProperties.KASINFO.fCanChangePassword = us.KASINFO.fCanChangePassword;
                     pProperties->u.UserProperties.KASINFO.fCanReusePasswords = us.KASINFO.fCanReusePasswords;
                     memcpy (&pProperties->u.UserProperties.KASINFO.timeExpires, &us.KASINFO.timeExpires, sizeof(SYSTEMTIME));
                     memcpy (&pProperties->u.UserProperties.KASINFO.timeLastPwChange, &us.KASINFO.timeLastPwChange, sizeof(SYSTEMTIME));
                     memcpy (&pProperties->u.UserProperties.KASINFO.timeLastMod, &us.KASINFO.timeLastMod, sizeof(SYSTEMTIME));
                     pProperties->u.UserProperties.KASINFO.szUserLastMod[0] = TEXT('\0');
                     pProperties->u.UserProperties.KASINFO.csecTicketLifetime = us.KASINFO.csecTicketLifetime;
                     pProperties->u.UserProperties.KASINFO.keyVersion = us.KASINFO.keyVersion;
                     memcpy (&pProperties->u.UserProperties.KASINFO.keyData, us.KASINFO.key.key, ENCRYPTIONKEYLENGTH);
                     pProperties->u.UserProperties.KASINFO.dwKeyChecksum = us.KASINFO.dwKeyChecksum;
                     pProperties->u.UserProperties.KASINFO.cdayPwExpire = us.KASINFO.cdayPwExpire;
                     pProperties->u.UserProperties.KASINFO.cFailLogin = us.KASINFO.cFailLogin;
                     pProperties->u.UserProperties.KASINFO.csecFailLoginLock = us.KASINFO.csecFailLoginLock;
                     if (us.KASINFO.lpiLastMod)
                        us.KASINFO.lpiLastMod->GetUserName (pProperties->u.UserProperties.KASINFO.szUserLastMod);
                     }
                  if ((pProperties->u.UserProperties.fHavePtsInfo = us.fHavePtsInfo) == TRUE)
                     {
                     pProperties->u.UserProperties.PTSINFO.cgroupCreationQuota = us.PTSINFO.cgroupCreationQuota;
                     pProperties->u.UserProperties.PTSINFO.cgroupMember = us.PTSINFO.cgroupMember;
                     pProperties->u.UserProperties.PTSINFO.uidName = us.PTSINFO.uidName;
                     pProperties->u.UserProperties.PTSINFO.uidOwner = us.PTSINFO.uidOwner;
                     pProperties->u.UserProperties.PTSINFO.uidCreator = us.PTSINFO.uidCreator;
                     lstrcpy (pProperties->u.UserProperties.PTSINFO.szOwner, us.PTSINFO.szOwner);
                     lstrcpy (pProperties->u.UserProperties.PTSINFO.szCreator, us.PTSINFO.szCreator);
                     pProperties->u.UserProperties.PTSINFO.aaListStatus = us.PTSINFO.aaListStatus;
                     pProperties->u.UserProperties.PTSINFO.aaGroupsOwned = us.PTSINFO.aaGroupsOwned;
                     pProperties->u.UserProperties.PTSINFO.aaMembership = us.PTSINFO.aaMembership;
                     }
                  }
               lpUser->Close();
               }
            break;

         case itGROUP:
            lpi->GetGroupName (pProperties->szName);

            LPPTSGROUP lpGroup;
            if ((lpGroup = lpi->OpenGroup (&status)) == NULL)
               rc = FALSE;
            else
               {
               PTSGROUPSTATUS gs;
               if (!lpGroup->GetStatus (&gs, TRUE, &status))
                  rc = FALSE;
               else
                  {
                  pProperties->u.GroupProperties.nMembers = gs.nMembers;
                  pProperties->u.GroupProperties.uidName = gs.uidName;
                  pProperties->u.GroupProperties.uidOwner = gs.uidOwner;
                  pProperties->u.GroupProperties.uidCreator = gs.uidCreator;
                  pProperties->u.GroupProperties.aaListStatus = gs.aaListStatus;
                  pProperties->u.GroupProperties.aaListGroupsOwned = gs.aaListGroupsOwned;
                  pProperties->u.GroupProperties.aaListMembers = gs.aaListMembers;
                  pProperties->u.GroupProperties.aaAddMember = gs.aaAddMember;
                  pProperties->u.GroupProperties.aaDeleteMember = gs.aaDeleteMember;
                  lstrcpy (pProperties->u.GroupProperties.szOwner, gs.szOwner);
                  lstrcpy (pProperties->u.GroupProperties.szCreator, gs.szCreator);
                  }
               lpGroup->Close();
               }
            break;
         }
      }

   if (rc && (pProperties->verProperties < verPROP_FIRST_SCAN))
      pProperties->verProperties = verPROP_FIRST_SCAN;

   if (!rc)
      pProperties->verProperties = verPROP_NO_OBJECT;

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_PropertiesDiffer (LPASOBJPROP pProp1, LPASOBJPROP pProp2)
{
   if (pProp1->idObject != pProp2->idObject)
      return TRUE;
   if (pProp1->Type != pProp2->Type)
      return TRUE;
   if (pProp1->idParentCell != pProp2->idParentCell)
      return TRUE;
   if (pProp1->idParentServer != pProp2->idParentServer)
      return TRUE;
   if (pProp1->idParentPartition != pProp2->idParentPartition)
      return TRUE;
   if (lstrcmp (pProp1->szName, pProp2->szName))
      return TRUE;
   if (memcmp (&pProp1->u, &pProp2->u, sizeof(pProp1->u)))
      return TRUE;

   return FALSE;
}


void AfsAdmSvr_TestProperties (ASID idObject)
{
   LPASOBJPROP pProperties;
   if ((pProperties = (LPASOBJPROP)((LPIDENT)idObject)->GetUserParam()) == NULL)
      return;

   if (pProperties->verProperties >= verPROP_FIRST_SCAN)
      {
      ASOBJPROP NewProperties;
      memcpy (&NewProperties, pProperties, sizeof(ASOBJPROP));
      memset (&NewProperties.u, 0x00, sizeof(NewProperties.u));

      if (AfsAdmSvr_ObtainFullProperties (&NewProperties))
         {
         if (AfsAdmSvr_PropertiesDiffer (&NewProperties, pProperties))
            {
            LPIDENT lpi = (LPIDENT)(pProperties->idObject);

            LPASOBJPROP pStoredProp;
            if ((pStoredProp = (LPASOBJPROP)(lpi->GetUserParam())) != NULL)
               {
               pStoredProp->idParentCell = NewProperties.idParentCell;
               pStoredProp->idParentServer = NewProperties.idParentServer;
               pStoredProp->idParentPartition = NewProperties.idParentPartition;
               lstrcpy (pStoredProp->szName, NewProperties.szName);
               memcpy (&pStoredProp->u, &NewProperties.u, sizeof(NewProperties.u));
               pStoredProp->verProperties ++;
               }
            }
         }
      }
}


BOOL CALLBACK AfsAdmSvr_NotifyCallback (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   switch (evt)
      {
      case evtCreate:
         LPIDENT lpiCreate;
         if ((lpiCreate = pParams->lpi1) != NULL)
            {
            if ((lpiCreate->GetUserParam()) == NULL)
               {
               LPASOBJPROP pProperties = New (ASOBJPROP);
               memset (pProperties, 0x00, sizeof(ASOBJPROP));
               pProperties->idObject = (ASID)lpiCreate;
               AfsAdmSvr_ObtainRudimentaryProperties (pProperties);
               lpiCreate->SetUserParam(pProperties);
               }

            AfsAdmSvr_TestProperties ((ASID)lpiCreate);
            }
         break;

      case evtDestroy:
         if (GetAsidType ((ASID)pParams->lpi1) == itCELL)
            AfsAdmSvr_StopCellRefreshThread ((ASID)(pParams->lpi1));
         break;

      case evtRefreshAllBegin:
         AfsAdmSvr_Action_StartRefresh ((ASID)(pParams->lpi1));
         break;

      case evtRefreshAllEnd:
         AfsAdmSvr_Action_StopRefresh ((ASID)(pParams->lpi1));
         break;

      case evtRefreshStatusEnd:
         AfsAdmSvr_TestProperties ((ASID)(pParams->lpi1));
         break;
      }

   return TRUE;
}


LPASOBJPROP AfsAdmSvr_GetCurrentProperties (ASID idObject, ULONG *pStatus)
{
   switch (GetAsidType (idObject))
      {
      case itCELL:
         LPCELL lpCell;
         if ((lpCell = ((LPIDENT)idObject)->OpenCell (pStatus)) == NULL)
            return NULL;
         lpCell->Close();
         break;

      case itSERVER:
         LPSERVER lpServer;
         if ((lpServer = ((LPIDENT)idObject)->OpenServer (pStatus)) == NULL)
            return NULL;
         lpServer->Close();
         break;

      case itSERVICE:
         LPSERVICE lpService;
         if ((lpService = ((LPIDENT)idObject)->OpenService (pStatus)) == NULL)
            return NULL;
         lpService->Close();
         break;

      case itAGGREGATE:
         LPAGGREGATE lpAggregate;
         if ((lpAggregate = ((LPIDENT)idObject)->OpenAggregate (pStatus)) == NULL)
            return NULL;
         lpAggregate->Close();
         break;

      case itFILESET:
         LPFILESET lpFileset;
         if ((lpFileset = ((LPIDENT)idObject)->OpenFileset (pStatus)) == NULL)
            return NULL;
         lpFileset->Close();
         break;

      case itUSER:
         LPUSER lpUser;
         if ((lpUser = ((LPIDENT)idObject)->OpenUser (pStatus)) == NULL)
            return NULL;
         lpUser->Close();
         break;

      case itGROUP:
         LPPTSGROUP lpGroup;
         if ((lpGroup = ((LPIDENT)idObject)->OpenGroup (pStatus)) == NULL)
            return NULL;
         lpGroup->Close();
         break;

      default:
         return (LPASOBJPROP)NULL_(ERROR_INVALID_PARAMETER,pStatus);
      }

   LPASOBJPROP pCurrentProperties;
   if ((pCurrentProperties = (LPASOBJPROP)((LPIDENT)idObject)->GetUserParam()) == NULL)
      {
      return (LPASOBJPROP)NULL_(ERROR_NOT_ENOUGH_MEMORY,pStatus);
      }

   return pCurrentProperties;
}


BOOL AfsAdmSvr_InvalidateObjectProperties (ASID idObject, ULONG *pStatus)
{
   switch (GetAsidType (idObject))
      {
      case itCELL:
         LPCELL lpCell;
         if ((lpCell = ((LPIDENT)idObject)->OpenCell (pStatus)) == NULL)
            return FALSE;
         lpCell->Invalidate();
         lpCell->Close();
         break;

      case itSERVER:
         LPSERVER lpServer;
         if ((lpServer = ((LPIDENT)idObject)->OpenServer (pStatus)) == NULL)
            return FALSE;
         lpServer->Invalidate();
         lpServer->Close();
         break;

      case itSERVICE:
         LPSERVICE lpService;
         if ((lpService = ((LPIDENT)idObject)->OpenService (pStatus)) == NULL)
            return FALSE;
         lpService->Invalidate();
         lpService->Close();
         break;

      case itAGGREGATE:
         LPAGGREGATE lpAggregate;
         if ((lpAggregate = ((LPIDENT)idObject)->OpenAggregate (pStatus)) == NULL)
            return FALSE;
         lpAggregate->Invalidate();
         lpAggregate->Close();
         break;

      case itFILESET:
         LPFILESET lpFileset;
         if ((lpFileset = ((LPIDENT)idObject)->OpenFileset (pStatus)) == NULL)
            return FALSE;
         lpFileset->Invalidate();
         lpFileset->Close();
         break;

      case itUSER:
         LPUSER lpUser;
         if ((lpUser = ((LPIDENT)idObject)->OpenUser (pStatus)) == NULL)
            return FALSE;
         lpUser->Invalidate();
         lpUser->Close();
         break;

      case itGROUP:
         LPPTSGROUP lpGroup;
         if ((lpGroup = ((LPIDENT)idObject)->OpenGroup (pStatus)) == NULL)
            return FALSE;
         lpGroup->Invalidate();
         lpGroup->Close();
         break;

      default:
         return FALSE_(ERROR_INVALID_PARAMETER,pStatus);
      }

   return TRUE;
}

