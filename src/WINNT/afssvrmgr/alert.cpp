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

#include "svrmgr.h"
#include "alert.h"
#include "creds.h"


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

BOOL Alert_Scout_CheckServer (LPSERVER lpServer);

void Alert_BeginUpdate (LPIDENT lpi, LPSERVER *ppServer);
void Alert_EndUpdate (LPIDENT lpi, LPSERVER lpServer);

void Alert_RemoveFunc (LPOBJECTALERTS lpoa, size_t iAlert);
LPTSTR Alert_GetDescriptionFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer, BOOL fFull);
LPTSTR Alert_GetRemedyFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer);
LPTSTR Alert_GetButtonFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer);

DWORD WINAPI Alert_ScoutProc (LPVOID lp);
void Alert_Scout_SetUpToDate (LPOBJECTALERTS lpoa);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPOBJECTALERTS Alert_GetObjectAlerts (LPIDENT lpi, BOOL fAlwaysServer, ULONG *pStatus)
{
   LPOBJECTALERTS lpoa = NULL;

   if (fAlwaysServer || lpi->fIsServer())
      {
      LPSERVER_PREF lpsp;
      if ((lpsp = (LPSERVER_PREF)lpi->GetServer()->GetUserParam()) != NULL)
         {
         lpoa = &lpsp->oa;
         }
      }
   else if (lpi->fIsService())
      {
      LPSERVICE_PREF lpsp;
      if ((lpsp = (LPSERVICE_PREF)lpi->GetUserParam()) != NULL)
         {
         lpoa = &lpsp->oa;
         }
      }
   else if (lpi->fIsAggregate())
      {
      LPAGGREGATE_PREF lpap;
      if ((lpap = (LPAGGREGATE_PREF)lpi->GetUserParam()) != NULL)
         {
         lpoa = &lpap->oa;
         }
      }
   else if (lpi->fIsFileset())
      {
      LPFILESET_PREF lpfp;
      if ((lpfp = (LPFILESET_PREF)lpi->GetUserParam()) != NULL)
         {
         lpoa = &lpfp->oa;
         }
      }

   return lpoa;
}


void Alert_BeginUpdate (LPIDENT lpi, LPSERVER *ppServer)
{
   if (lpi->fIsServer())
      {
      *ppServer = NULL;
      }
   else
      {
      *ppServer = lpi->OpenServer();
      }
}


void Alert_EndUpdate (LPIDENT lpi, LPSERVER lpServer)
{
   // If we just updated some aggregate, fileset or service, then the
   // associated server's secondary alerts are probably out-of-date.
   // Update them.
   //
   if (lpServer != NULL)
      {
      LPOBJECTALERTS lpoaServer = Alert_GetObjectAlerts (lpServer->GetIdentifier());
      LPOBJECTALERTS lpoaChild  = Alert_GetObjectAlerts (lpi);

      if (lpoaServer)
         {
         for (size_t iAlert = 0; iAlert < lpoaServer->nAlerts; )
            {
            if ( (lpoaServer->aAlerts[ iAlert ].alert == alertSECONDARY) &&
                 (lpoaServer->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary == lpi) )
               {
               Alert_RemoveFunc (lpoaServer, iAlert);
               }
            else
               {
               ++iAlert;
               }
            }
         }

      if (lpoaServer && lpoaChild)
         {
         BOOL fNeedBadCredsWarning = FALSE;
         BOOL fHaveBadCredsWarning = FALSE;

         size_t iAlert;
         for (iAlert = 0; iAlert < lpoaServer->nAlerts; ++iAlert)
            {
            if (lpoaServer->aAlerts[ iAlert ].alert == alertSECONDARY)
               {
               ALERT alert = Alert_GetAlert (lpoaServer->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                                             lpoaServer->aAlerts[ iAlert ].aiSECONDARY.iSecondary);
               if (alert == alertNO_SVRENT)
                  fNeedBadCredsWarning = TRUE;
               }
            }
         for (iAlert = 0; iAlert < lpoaChild->nAlerts; ++iAlert)
            {
            if (lpoaChild->aAlerts[ iAlert ].alert == alertNO_SVRENT)
               fNeedBadCredsWarning = TRUE;
            }
         if (lpoaServer->nAlerts &&
             lpoaServer->aAlerts[ 0 ].alert == alertBADCREDS)
            {
            fHaveBadCredsWarning = TRUE;
            }

         if (fNeedBadCredsWarning)
            {
            fNeedBadCredsWarning = !CheckCredentials (FALSE);
            }

         if (fHaveBadCredsWarning && !fNeedBadCredsWarning)
            {
            Alert_RemoveFunc (lpoaServer, 0);
            }
         else if (fNeedBadCredsWarning && !fHaveBadCredsWarning)
            {
            for (iAlert = min( lpoaServer->nAlerts, nAlertsMAX-1 );
                 iAlert > 0;
                 --iAlert)
               {
               memcpy (&lpoaServer->aAlerts[ iAlert ], &lpoaServer->aAlerts[ iAlert-1 ], sizeof(ALERTINFO));
               }
            lpoaServer->aAlerts[0].alert = alertBADCREDS;
            lpoaServer->nAlerts = min( nAlertsMAX, lpoaServer->nAlerts+1 );
            }

         for (iAlert = 0; iAlert < lpoaChild->nAlerts; ++iAlert)
            {
            if (lpoaServer->nAlerts < nAlertsMAX)
               {
               lpoaServer->aAlerts[ lpoaServer->nAlerts ].alert = alertSECONDARY;
               lpoaServer->aAlerts[ lpoaServer->nAlerts ].aiSECONDARY.lpiSecondary = lpi;
               lpoaServer->aAlerts[ lpoaServer->nAlerts ].aiSECONDARY.iSecondary = iAlert;
               lpoaServer->nAlerts ++;
               }
            }
         }

      lpServer->Close();
      }
}


void Alert_SetDefaults (LPOBJECTALERTS lpoa)
{
   if (lpoa != NULL)
      {
      memset (lpoa, 0x00, sizeof(OBJECTALERTS));
      lpoa->cTickRefresh = DEFAULT_SCOUT_REFRESH_RATE;
      }
}


void Alert_Initialize (LPOBJECTALERTS lpoa)
{
   if (lpoa != NULL)
      {
      lpoa->dwTickNextTest = 0;
      lpoa->dwTickNextRefresh = 0;
      lpoa->nAlerts = 0;
      }
}


void Alert_RemoveSecondary (LPIDENT lpiChild)
{
   BOOL fChangedAlerts = FALSE;

   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpiChild, TRUE)) != NULL)
      {
      size_t iAlert;
      for (iAlert = 0; iAlert < lpoa->nAlerts; )
         {
         if ( (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY) &&
              (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary == lpiChild) )
            {
            Alert_RemoveFunc (lpoa, iAlert);
            fChangedAlerts = TRUE;
            }
         else
            {
            ++iAlert;
            }
         }


      BOOL fNeedBadCredsWarning = FALSE;

      for (iAlert = 0; iAlert < lpoa->nAlerts; ++iAlert)
         {
         if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
            {
            ALERT alert = Alert_GetAlert (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                                          lpoa->aAlerts[ iAlert ].aiSECONDARY.iSecondary);
            if (alert == alertNO_SVRENT)
               fNeedBadCredsWarning = TRUE;
            }
         }

      if ( (!fNeedBadCredsWarning) &&
           (lpoa->nAlerts && (lpoa->aAlerts[ 0 ].alert == alertBADCREDS)) )
         {
         Alert_RemoveFunc (lpoa, 0);
         fChangedAlerts = TRUE;
         }
      }

   if (fChangedAlerts)
      {
      PostNotification (evtAlertsChanged, lpiChild->GetServer());
      PostNotification (evtAlertsChanged, lpiChild);
      }
}


void Alert_Remove (LPIDENT lpi, size_t iAlert)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) != NULL)
      {
      if (iAlert < lpoa->nAlerts)
         {
         LPSERVER lpServer;
         Alert_BeginUpdate (lpi, &lpServer);

         Alert_RemoveFunc (lpoa, iAlert);

         Alert_EndUpdate (lpi, lpServer);
         }
      }
}


void Alert_RemoveFunc (LPOBJECTALERTS lpoa, size_t iAlert)
{
   if (iAlert < lpoa->nAlerts-1)
      {
      memcpy (&lpoa->aAlerts[ iAlert ], &lpoa->aAlerts[ lpoa->nAlerts-1 ], sizeof(ALERTINFO));
      }
   lpoa->nAlerts --;
}


void Alert_AddPrimary (LPIDENT lpi, LPALERTINFO lpai)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) != NULL)
      {
      if (lpoa->nAlerts < nAlertsMAX)
         {
         LPSERVER lpServer;
         Alert_BeginUpdate (lpi, &lpServer);

         memcpy (&lpoa->aAlerts[ lpoa->nAlerts ], lpai, sizeof(ALERTINFO));
         lpoa->nAlerts ++;

         Alert_EndUpdate (lpi, lpServer);
         }
      }
}


void Alert_Scout_ServerStatus (LPIDENT lpi, ULONG status)
{
   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpi)) != NULL)
      {
      BOOL fChanged = FALSE;

      for (size_t iAlert = 0; iAlert < lpoa->nAlerts; ++iAlert)
         {
         if (lpoa->aAlerts[ iAlert ].alert == alertTIMEOUT)
            {
            fChanged = TRUE;
            Alert_RemoveFunc (lpoa, iAlert);
            break;
            }
         }

      if (status != 0)
         {
         size_t iInsert = 0;
         if (lpoa->nAlerts && (lpoa->aAlerts[0].alert == alertBADCREDS))
            iInsert = 1;

         size_t iHole;
         for (iHole = iInsert; iHole < lpoa->nAlerts; ++iHole)
            {
            if (lpoa->aAlerts[ iHole ].alert == alertINVALID)
               break;
            }
         if (iHole < nAlertsMAX)
            {
            for (size_t iTarget = iHole; iTarget > iInsert; --iTarget)
               {
               memcpy (&lpoa->aAlerts[ iTarget ], &lpoa->aAlerts[ iTarget-1 ], sizeof(ALERTINFO));
               }

            lpoa->nAlerts ++;
            lpoa->aAlerts[ iInsert ].alert = alertTIMEOUT;
            lpoa->aAlerts[ iInsert ].aiTIMEOUT.status = status;
            GetSystemTime (&lpoa->aAlerts[ iInsert ].aiTIMEOUT.stLastAttempt);

            fChanged = TRUE;
            }
         }

      if (fChanged)
         {
         PostNotification (evtAlertsChanged, lpi);
         }
      }
}


size_t Alert_GetCount (LPIDENT lpi)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return 0;

   return lpoa->nAlerts;
}


ALERT Alert_GetAlert (LPIDENT lpi, size_t iAlert)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return alertINVALID;

   if (iAlert > lpoa->nAlerts)
      return alertINVALID;

   if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
      {
      return Alert_GetAlert (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                             lpoa->aAlerts[ iAlert ].aiSECONDARY.iSecondary);
      }

   return lpoa->aAlerts[ iAlert ].alert;
}


LPIDENT Alert_GetIdent (LPIDENT lpi, size_t iAlert)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return NULL;

   if (iAlert > lpoa->nAlerts)
      return NULL;

   if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
      {
      return lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary;
      }

   return lpi;
}


LPTSTR Alert_GetQuickDescription (LPIDENT lpi)
{
   LPTSTR pszStatus = NULL;

   size_t cAlerts;
   if ((cAlerts = Alert_GetCount (lpi)) <= 1)
      pszStatus = Alert_GetDescription (lpi, 0, FALSE);
   else if (lpi->fIsServer())
      pszStatus = FormatString (IDS_SERVER_MULTIPLE_PROBLEMS, TEXT("%lu"), cAlerts);
   else if (lpi->fIsService())
      pszStatus = FormatString (IDS_SERVICE_MULTIPLE_PROBLEMS, TEXT("%lu"), cAlerts);
   else if (lpi->fIsAggregate())
      pszStatus = FormatString (IDS_AGGREGATE_MULTIPLE_PROBLEMS, TEXT("%lu"), cAlerts);
   else if (lpi->fIsFileset())
      pszStatus = FormatString (IDS_FILESET_MULTIPLE_PROBLEMS, TEXT("%lu"), cAlerts);

   return pszStatus;
}


LPTSTR Alert_GetDescription (LPIDENT lpi, size_t iAlert, BOOL fFull)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return NULL;

   if (!lpoa->nAlerts && lpi->fIsServer())
      {
      LPSERVER_PREF lpsp;
      if ((lpsp = (LPSERVER_PREF)lpi->GetUserParam()) != NULL)
         {
         if (!lpsp->fIsMonitored)
            {
            TCHAR szName[ cchNAME ];
            lpi->GetServerName (szName);
            return FormatString (IDS_ALERT_DESCSHORT_UNMONITORED, TEXT("%s"), szName);
            }
         }
      }

   if (iAlert >= lpoa->nAlerts)
      return NULL;

   if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
      {
      return Alert_GetDescriptionFunc (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                                       lpoa->aAlerts[ iAlert ].aiSECONDARY.iSecondary,
                                       lpi,
                                       fFull);
      }

   return Alert_GetDescriptionFunc (lpi, iAlert, NULL, fFull);
}


LPTSTR Alert_GetRemedy (LPIDENT lpi, size_t iAlert)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return NULL;

   if (iAlert >= lpoa->nAlerts)
      return NULL;

   if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
      {
      return Alert_GetRemedyFunc (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                                  lpoa->aAlerts[ iAlert ].aiSECONDARY.iSecondary,
                                  lpi);
      }

   return Alert_GetRemedyFunc (lpi, iAlert, NULL);
}


LPTSTR Alert_GetButton (LPIDENT lpi, size_t iAlert)
{
   LPOBJECTALERTS lpoa;

   if ((lpoa = Alert_GetObjectAlerts (lpi)) == NULL)
      return NULL;

   if (iAlert >= lpoa->nAlerts)
      return NULL;

   if (lpoa->aAlerts[ iAlert ].alert == alertSECONDARY)
      {
      return Alert_GetButtonFunc (lpoa->aAlerts[ iAlert ].aiSECONDARY.lpiSecondary,
                                  lpoa->aAlerts[ iAlert ].aiSECONDARY.iSecondary,
                                  lpi);
      }

   return Alert_GetButtonFunc (lpi, iAlert, NULL);
}


LPTSTR Alert_GetDescriptionFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer, BOOL fFull)
{
   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpiPrimary)) != NULL)
      {
      int ids;
      TCHAR szServer[ cchRESOURCE ];
      TCHAR szService[ cchRESOURCE ];
      TCHAR szAggregate[ cchRESOURCE ];
      TCHAR szFileset[ cchRESOURCE ];

      switch (lpoa->aAlerts[ iAlertPrimary ].alert)
         {
         case alertTIMEOUT:
            ids = (fFull) ? IDS_ALERT_DESCFULL_TIMEOUT : IDS_ALERT_DESCSHORT_TIMEOUT;
            lpiPrimary->GetServerName (szServer);
            return FormatString (ids, TEXT("%s%t%e"), szServer, &lpoa->aAlerts[ iAlertPrimary ].aiTIMEOUT.stLastAttempt, lpoa->aAlerts[ iAlertPrimary ].aiTIMEOUT.status);

         case alertFULL:
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            if (lpiPrimary->fIsAggregate())
               {
               ids = (fFull) ? IDS_ALERT_DESCFULL_AGG_FULL : IDS_ALERT_DESCSHORT_AGG_FULL;
               return FormatString (ids, TEXT("%s%s%d%.1B"), szServer, szAggregate, lpoa->aAlerts[ iAlertPrimary ].aiFULL.perWarning, 1024.0 * (double)lpoa->aAlerts[ iAlertPrimary ].aiFULL.ckWarning);
               }
            else if (lpiPrimary->fIsFileset())
               {
               ids = (fFull) ? IDS_ALERT_DESCFULL_SET_FULL : IDS_ALERT_DESCSHORT_SET_FULL;
               lpiPrimary->GetFilesetName (szFileset);
               return FormatString (ids, TEXT("%s%s%s%d%.1B"), szServer, szAggregate, szFileset, lpoa->aAlerts[ iAlertPrimary ].aiFULL.perWarning, 1024.0 * (double)lpoa->aAlerts[ iAlertPrimary ].aiFULL.ckWarning);
               }
            break;

         case alertNO_VLDBENT:
            ids = (fFull) ? IDS_ALERT_DESCFULL_NO_VLDBENT : IDS_ALERT_DESCSHORT_NO_VLDBENT;
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            lpiPrimary->GetFilesetName (szFileset);
            return FormatString (ids, TEXT("%s%s%s"), szServer, szAggregate, szFileset);

         case alertNO_SVRENT:
            if (lpiPrimary->fIsFileset())
               {
               ids = (fFull) ? IDS_ALERT_DESCFULL_NO_SVRENT_SET : IDS_ALERT_DESCSHORT_NO_SVRENT_SET;
               lpiPrimary->GetServerName (szServer);
               lpiPrimary->GetAggregateName (szAggregate);
               lpiPrimary->GetFilesetName (szFileset);
               return FormatString (ids, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
               }
            else
               {
               ids = (fFull) ? IDS_ALERT_DESCFULL_NO_SVRENT_AGG : IDS_ALERT_DESCSHORT_NO_SVRENT_AGG;
               lpiPrimary->GetServerName (szServer);
               lpiPrimary->GetAggregateName (szAggregate);
               return FormatString (ids, TEXT("%s%s"), szServer, szAggregate);
               }
            break;

         case alertSTOPPED:
            ids = (fFull) ? IDS_ALERT_DESCFULL_STOPPED : IDS_ALERT_DESCSHORT_STOPPED;
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetServiceName (szService);
            return FormatString (ids, TEXT("%s%s%t%t%lu"), szServer, szService, &lpoa->aAlerts[ iAlertPrimary ].aiSTOPPED.stStopped, &lpoa->aAlerts[ iAlertPrimary ].aiSTOPPED.stLastError, lpoa->aAlerts[ iAlertPrimary ].aiSTOPPED.errLastError);

         case alertBADCREDS:
            ids = (fFull) ? IDS_ALERT_DESCFULL_BADCREDS : IDS_ALERT_DESCSHORT_BADCREDS;
            lpiPrimary->GetServerName (szServer);
            return FormatString (ids, TEXT("%s"), szServer);

         case alertOVERALLOC:
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            ids = (fFull) ? IDS_ALERT_DESCFULL_AGG_ALLOC : IDS_ALERT_DESCSHORT_AGG_ALLOC;
            return FormatString (ids, TEXT("%s%s%.1B%.1B"), szServer, szAggregate, 1024.0 * (double)(lpoa->aAlerts[ iAlertPrimary ].aiOVERALLOC.ckCapacity), 1024.0 * (double)(lpoa->aAlerts[ iAlertPrimary ].aiOVERALLOC.ckAllocated));

         case alertSTATE_NO_VNODE:
            ids = (fFull) ? IDS_ALERT_DESCFULL_STATE_NO_VNODE : IDS_ALERT_DESCSHORT_STATE_NO_VNODE;
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            lpiPrimary->GetFilesetName (szFileset);
            return FormatString (ids, TEXT("%s%s%s%08lX"), szServer, szAggregate, szFileset, lpoa->aAlerts[ iAlertPrimary ].aiSTATE.State);

         case alertSTATE_NO_SERVICE:
            ids = (fFull) ? IDS_ALERT_DESCFULL_STATE_NO_SERVICE : IDS_ALERT_DESCSHORT_STATE_NO_SERVICE;
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            lpiPrimary->GetFilesetName (szFileset);
            return FormatString (ids, TEXT("%s%s%s%08lX"), szServer, szAggregate, szFileset, lpoa->aAlerts[ iAlertPrimary ].aiSTATE.State);

         case alertSTATE_OFFLINE:
            ids = (fFull) ? IDS_ALERT_DESCFULL_STATE_OFFLINE : IDS_ALERT_DESCSHORT_STATE_OFFLINE;
            lpiPrimary->GetServerName (szServer);
            lpiPrimary->GetAggregateName (szAggregate);
            lpiPrimary->GetFilesetName (szFileset);
            return FormatString (ids, TEXT("%s%s%s%08lX"), szServer, szAggregate, szFileset, lpoa->aAlerts[ iAlertPrimary ].aiSTATE.State);
         }
      }

   return NULL;
}


LPTSTR Alert_GetRemedyFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer)
{
   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpiPrimary)) != NULL)
      {
      switch (lpoa->aAlerts[ iAlertPrimary ].alert)
         {
         case alertTIMEOUT:
            return FormatString (IDS_ALERT_FIX_TIMEOUT);
         case alertFULL:
            if (lpiPrimary->fIsAggregate())
               return FormatString (IDS_ALERT_FIX_AGG_FULL);
            else if (lpiPrimary->fIsFileset())
               return FormatString (IDS_ALERT_FIX_SET_FULL);
            break;
         case alertNO_VLDBENT:
            return FormatString (IDS_ALERT_FIX_NO_VLDBENT);
         case alertNO_SVRENT:
            if (lpiPrimary->fIsFileset())
               return FormatString (IDS_ALERT_FIX_NO_SVRENT_SET);
            else
               return FormatString (IDS_ALERT_FIX_NO_SVRENT_AGG);
            break;
         case alertSTOPPED:
            return FormatString (IDS_ALERT_FIX_STOPPED);
         case alertBADCREDS:
            return FormatString (IDS_ALERT_FIX_BADCREDS);
         case alertOVERALLOC:
            return FormatString (IDS_ALERT_FIX_AGG_ALLOC);
         case alertSTATE_NO_VNODE:
            return FormatString (IDS_ALERT_FIX_STATE_NO_VNODE);
         case alertSTATE_NO_SERVICE:
            return FormatString (IDS_ALERT_FIX_STATE_NO_SERVICE);
         case alertSTATE_OFFLINE:
            return FormatString (IDS_ALERT_FIX_STATE_OFFLINE);
         }
      }

   return NULL;
}


LPTSTR Alert_GetButtonFunc (LPIDENT lpiPrimary, size_t iAlertPrimary, LPIDENT lpiServer)
{
   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpiPrimary)) != NULL)
      {
      switch (lpoa->aAlerts[ iAlertPrimary ].alert)
         {
         case alertTIMEOUT:
            return FormatString (IDS_ALERT_BUTTON_TRYAGAIN);
         case alertFULL:
            return FormatString (IDS_ALERT_BUTTON_WARNINGS);
         case alertNO_VLDBENT:
            return NULL;
         case alertNO_SVRENT:
            return NULL;
         case alertSTOPPED:
            return FormatString (IDS_ALERT_BUTTON_VIEWLOG);
         case alertBADCREDS:
            return FormatString (IDS_ALERT_BUTTON_GETCREDS);
         case alertOVERALLOC:
            return NULL;
         case alertSTATE_NO_VNODE:
            return NULL;
         case alertSTATE_NO_SERVICE:
            return NULL;
         case alertSTATE_OFFLINE:
            return NULL;
         }
      }

   return NULL;
}


/*
 * SCOUT ______________________________________________________________________
 *
 * (okay, well, our simulated Scout anyway)
 *
 */

static HANDLE hScout = 0;	// scout's thread
static HANDLE heScoutWakeup = 0;	// scout's wakeup event

BOOL Alert_StartScout (ULONG *pStatus)
{
   if (hScout == 0)  // create scout?
      {
      heScoutWakeup = CreateEvent (NULL, FALSE, FALSE, TEXT("AfsSvrMgr Alert Scout Wakeup"));

      DWORD dwThreadID;
      if ((hScout = CreateThread (NULL, 0,
                                  (LPTHREAD_START_ROUTINE)Alert_ScoutProc,
                                  NULL, 0,
                                  &dwThreadID)) == NULL)
         {
         if (pStatus)
            *pStatus = GetLastError();
         return FALSE;
         }

      SetThreadPriority (hScout, THREAD_PRIORITY_BELOW_NORMAL);
      }
   else // or just wake up scout from its slumber?
      {
      PulseEvent (heScoutWakeup);
      }

   return TRUE;
}


BOOL Alert_Scout_QueueCheckServer (LPIDENT lpiServer, ULONG *pStatus)
{
   Alert_Scout_SetOutOfDate (lpiServer);
   return Alert_StartScout (pStatus);
}


DWORD WINAPI Alert_ScoutProc (LPVOID lp)
{
   // We'll keep working forever...
   //
   for (;;)
      {
      AfsClass_Enter();

      LPCELL lpCell = (g.lpiCell == NULL) ? NULL : g.lpiCell->OpenCell();
      if (lpCell != NULL)
         {
         // See if our credentials have expired
         //
         CheckForExpiredCredentials();

         // See if any new servers have arrived, or old servers disappeared.
         //
         lpCell->RefreshServerList();

         // Check all the out-of-date servers we can find.
         //
         HENUM hEnum;
         for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
            {
            LPIDENT lpiServer = lpServer->GetIdentifier();
            LPOBJECTALERTS lpoa;

            if ( ((lpoa = Alert_GetObjectAlerts (lpiServer)) != NULL) &&
                 (lpoa->dwTickNextTest <= GetTickCount()) )
               {

               // Okay!  We've found a server that needs to be tested for
               // alert conditions.  Do that now, and when we're done, set
               // its next query-time to some distance in the future.
               //
               if (lpoa->dwTickNextRefresh == 0)
                  {
                  if (lpoa->cTickRefresh != 0)
                     lpoa->dwTickNextRefresh = lpoa->cTickRefresh + GetTickCount();
                  }
               else if (lpoa->dwTickNextRefresh <= GetTickCount())
                  {
                  (void)lpServer->Invalidate();
                  (void)lpServer->RefreshAll();
                  lpoa->dwTickNextRefresh = lpoa->cTickRefresh + GetTickCount();
                  }

               (void)Alert_Scout_CheckServer (lpServer);
               }

            lpServer->Close();
            }

         lpCell->Close();
         }

      AfsClass_Leave();

      // Now that we have completed a pass over the servers in this cell,
      // and now that we're not holding any critical sections on which
      // other threads would otherwise block, go to sleep for a while.
      //
      WaitForSingleObjectEx (heScoutWakeup, 45L * cmsec1SECOND, FALSE);
      }

   return 0;
}


void Alert_Scout_SetOutOfDate (LPIDENT lpi)
{
   LPOBJECTALERTS lpoa;
   if ((lpoa = Alert_GetObjectAlerts (lpi, TRUE)) != NULL)
      {
      lpoa->dwTickNextTest = GetTickCount() -1;
      }
}


void Alert_Scout_SetUpToDate (LPOBJECTALERTS lpoa)
{
   if (lpoa != NULL)
      {
      lpoa->dwTickNextTest = GetTickCount() + lpoa->cTickRefresh;
      }
}


BOOL Alert_Scout_CheckServer (LPSERVER lpServer)
{
   BOOL rc = TRUE;

   LPSERVER_PREF lpsp;
   if ((lpsp = (LPSERVER_PREF)lpServer->GetUserParam()) != NULL)
      {
      LPOBJECTALERTS lpoa;
      if ((lpoa = Alert_GetObjectAlerts (lpServer->GetIdentifier())) != NULL)
         {
         PostNotification (evtScoutBegin, lpServer->GetIdentifier());

         BOOL fChangedServerAlerts = FALSE;

         DWORD dwTickNextTestWhenStarted = lpoa->dwTickNextTest;

         // First look through the server's aggregates and filesets, to
         // find any which have usages over their warning threshholds.
         //
         HENUM heAggregate;
         for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&heAggregate); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&heAggregate))
            {
            BOOL fChangedAggregateAlerts = FALSE;
            LPIDENT lpiAggregate = lpAggregate->GetIdentifier();

            LPOBJECTALERTS lpoaAggregate;
            if ((lpoaAggregate = Alert_GetObjectAlerts (lpAggregate->GetIdentifier())) != NULL)
               {
               for (size_t iAlert = 0; iAlert < lpoaAggregate->nAlerts; )
                  {
                  if ( (lpoaAggregate->aAlerts[ iAlert ].alert == alertFULL) ||
                       (lpoaAggregate->aAlerts[ iAlert ].alert == alertOVERALLOC) ||
                       (lpoaAggregate->aAlerts[ iAlert ].alert == alertNO_SVRENT) )
                     {
                     fChangedAggregateAlerts = TRUE;
                     fChangedServerAlerts = TRUE;
                     Alert_Remove (lpAggregate->GetIdentifier(), iAlert);
                     }
                  else
                     ++iAlert;
                  }

               LPAGGREGATE_PREF lpap;
               if ((lpap = (LPAGGREGATE_PREF)lpAggregate->GetUserParam()) != NULL)
                  {
                  short wGhost = lpAggregate->GetGhostStatus();
                  if (lpsp->fWarnAggNoServ && !(wGhost & GHOST_HAS_SERVER_ENTRY))
                     {
                     ALERTINFO ai;
                     ai.alert = alertNO_SVRENT;
                     Alert_AddPrimary (lpAggregate->GetIdentifier(), &ai);
                     fChangedAggregateAlerts = TRUE;
                     fChangedServerAlerts = TRUE;
                     }

                  if (lpsp->fWarnAggAlloc && lpap->fWarnAggAlloc)
                     {
                     AGGREGATESTATUS as;
                     if (lpAggregate->GetStatus (&as, TRUE))
                        {
                        if (as.ckStorageAllocated > as.ckStorageTotal)
                           {
                           ALERTINFO ai;
                           ai.alert = alertOVERALLOC;
                           ai.aiOVERALLOC.ckAllocated = as.ckStorageAllocated;
                           ai.aiOVERALLOC.ckCapacity = as.ckStorageTotal;
                           Alert_AddPrimary (lpAggregate->GetIdentifier(), &ai);
                           fChangedAggregateAlerts = TRUE;
                           fChangedServerAlerts = TRUE;
                           }
                        }
                     }

                  short perWarnAggFull = lpap->perWarnAggFull;
                  if (perWarnAggFull == -1)
                     perWarnAggFull = lpsp->perWarnAggFull;
                  if (perWarnAggFull != 0)
                     {
                     AGGREGATESTATUS as;
                     if (lpAggregate->GetStatus (&as, TRUE))
                        {
                        if (as.ckStorageTotal != 0)
                           {
                           short perNow = (short)( (double)(as.ckStorageTotal - as.ckStorageFree) * 100.0 / (double)(as.ckStorageTotal) );

                           if (perNow > perWarnAggFull)
                              {
                              ALERTINFO ai;
                              ai.alert = alertFULL;
                              ai.aiFULL.perWarning = perWarnAggFull;
                              ai.aiFULL.ckWarning = (ULONG)( (double)perWarnAggFull * (double)(as.ckStorageTotal) / 100.0 );
                              Alert_AddPrimary (lpAggregate->GetIdentifier(), &ai);
                              fChangedAggregateAlerts = TRUE;
                              fChangedServerAlerts = TRUE;
                              }
                           }
                        }
                     }
                  }
               }

            HENUM heFileset;
            for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&heFileset); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&heFileset))
               {
               BOOL fChangedFilesetAlerts = FALSE;
               LPIDENT lpiFileset = lpFileset->GetIdentifier();

               LPOBJECTALERTS lpoaFileset;
               if ((lpoaFileset = Alert_GetObjectAlerts (lpFileset->GetIdentifier())) != NULL)
                  {
                  for (size_t iAlert = 0; iAlert < lpoaFileset->nAlerts; )
                     {
                     if ( (lpoaFileset->aAlerts[ iAlert ].alert == alertFULL) ||
                          (lpoaFileset->aAlerts[ iAlert ].alert == alertSTATE_NO_VNODE) ||
                          (lpoaFileset->aAlerts[ iAlert ].alert == alertSTATE_NO_SERVICE) ||
                          (lpoaFileset->aAlerts[ iAlert ].alert == alertSTATE_OFFLINE) ||
                          (lpoaFileset->aAlerts[ iAlert ].alert == alertNO_VLDBENT) ||
                          (lpoaFileset->aAlerts[ iAlert ].alert == alertNO_SVRENT) )
                        {
                        fChangedFilesetAlerts = TRUE;
                        fChangedServerAlerts = TRUE;
                        Alert_Remove (lpFileset->GetIdentifier(), iAlert);
                        }
                     else
                        ++iAlert;
                     }
                  }

               LPFILESET_PREF lpfp;
               if ((lpfp = (LPFILESET_PREF)lpFileset->GetUserParam()) != NULL)
                  {
                  FILESETSTATUS fs;
                  if (lpFileset->GetStatus (&fs, TRUE))
                     {
                     if (fs.State & fsNO_VNODE)
                        {
                        ALERTINFO ai;
                        ai.alert = alertSTATE_NO_VNODE;
                        ai.aiSTATE.State = fs.State;
                        Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                        fChangedFilesetAlerts = TRUE;
                        fChangedServerAlerts = TRUE;
                        }
                     else if (fs.State & fsNO_SERVICE)
                        {
                        ALERTINFO ai;
                        ai.alert = alertSTATE_NO_SERVICE;
                        ai.aiSTATE.State = fs.State;
                        Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                        fChangedFilesetAlerts = TRUE;
                        fChangedServerAlerts = TRUE;
                        }
                     else if (fs.State & fsOFFLINE)
                        {
                        ALERTINFO ai;
                        ai.alert = alertSTATE_OFFLINE;
                        ai.aiSTATE.State = fs.State;
                        Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                        fChangedFilesetAlerts = TRUE;
                        fChangedServerAlerts = TRUE;
                        }

                     short perWarnSetFull = lpfp->perWarnSetFull;
                     if (perWarnSetFull == -1)
                        perWarnSetFull = lpsp->perWarnSetFull;
                     if (perWarnSetFull != 0)
                        {
                        if (fs.Type == ftREADWRITE)
                           {
                           if (fs.ckQuota != 0)
                              {
                              short perNow = (short)( (double)(fs.ckUsed) * 100.0 / (double)(fs.ckQuota) );

                              if (perNow > perWarnSetFull)
                                 {
                                 ALERTINFO ai;
                                 ai.alert = alertFULL;
                                 ai.aiFULL.perWarning = perWarnSetFull;
                                 ai.aiFULL.ckWarning = (ULONG)( (double)perWarnSetFull * (double)(fs.ckQuota) / 100.0 );
                                 Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                                 fChangedFilesetAlerts = TRUE;
                                 fChangedServerAlerts = TRUE;
                                 }
                              }
                           }
                        }
                     }

                  short wGhost = lpFileset->GetGhostStatus();
                  if (lpsp->fWarnSetNoVLDB && !(wGhost & GHOST_HAS_VLDB_ENTRY))
                     {
                     ALERTINFO ai;
                     ai.alert = alertNO_VLDBENT;
                     Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                     fChangedFilesetAlerts = TRUE;
                     fChangedServerAlerts = TRUE;
                     }
                  if (lpsp->fWarnSetNoServ && !(wGhost & GHOST_HAS_SERVER_ENTRY) && !(fs.Type == ftREPLICA))
                     {
                     ALERTINFO ai;
                     ai.alert = alertNO_SVRENT;
                     Alert_AddPrimary (lpFileset->GetIdentifier(), &ai);
                     fChangedFilesetAlerts = TRUE;
                     fChangedServerAlerts = TRUE;
                     }
                  }

               lpFileset->Close();
               if (fChangedFilesetAlerts)
                  {
                  PostNotification (evtAlertsChanged, lpiFileset);
                  }
               }

            lpAggregate->Close();
            if (fChangedAggregateAlerts)
               {
               PostNotification (evtAlertsChanged, lpiAggregate);
               }
            }

         // Next look through the server's servces to find any
         // which have stopped.
         //
         HENUM heService;
         for (LPSERVICE lpService = lpServer->ServiceFindFirst (&heService); lpService; lpService = lpServer->ServiceFindNext (&heService))
            {
            BOOL fChangedServiceAlerts = FALSE;
            LPIDENT lpiService = lpService->GetIdentifier();

            LPOBJECTALERTS lpoaService;
            if ((lpoaService = Alert_GetObjectAlerts (lpService->GetIdentifier())) != NULL)
               {
               for (size_t iAlert = 0; iAlert < lpoaService->nAlerts; )
                  {
                  if (lpoaService->aAlerts[ iAlert ].alert == alertSTOPPED)
                     {
                     fChangedServiceAlerts = TRUE;
                     fChangedServerAlerts = TRUE;
                     Alert_Remove (lpService->GetIdentifier(), iAlert);
                     }
                  else
                     ++iAlert;
                  }

               LPSERVICE_PREF lpcp;
               if ((lpcp = (LPSERVICE_PREF)lpService->GetUserParam()) != NULL)
                  {
                  if (lpcp->fWarnSvcStop && lpsp->fWarnSvcStop)
                     {
                     SERVICESTATUS ss;
                     if (lpService->GetStatus (&ss, TRUE))
                        {
                        if (ss.state != SERVICESTATE_RUNNING)
                           {
                           ALERTINFO ai;
                           ai.alert = alertSTOPPED;
                           memcpy (&ai.aiSTOPPED.stStopped,   &ss.timeLastStop, sizeof(SYSTEMTIME));
                           memcpy (&ai.aiSTOPPED.stLastError, &ss.timeLastFail, sizeof(SYSTEMTIME));
                           ai.aiSTOPPED.errLastError = ss.dwErrLast;
                           Alert_AddPrimary (lpService->GetIdentifier(), &ai);
                           fChangedServiceAlerts = TRUE;
                           fChangedServerAlerts = TRUE;
                           }
                        }
                     }
                  }
               }

            lpService->Close();
            if (fChangedServiceAlerts)
               {
               PostNotification (evtAlertsChanged, lpiService);
               }
            }

         if (rc && (dwTickNextTestWhenStarted == lpoa->dwTickNextTest))
            {
            Alert_Scout_SetUpToDate (lpoa);
            }

         if (fChangedServerAlerts)
            {
            PostNotification (evtAlertsChanged, lpServer->GetIdentifier());
            }

         PostNotification (evtScoutEnd, lpServer->GetIdentifier());
         }
      }

   return rc;
}

