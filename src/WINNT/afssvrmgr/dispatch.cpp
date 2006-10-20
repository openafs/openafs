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
#include "dispatch.h"
#include "action.h"
#include "svr_general.h"
#include "svc_general.h"
#include "agg_general.h"
#include "set_general.h"
#include "subset.h"
#include "display.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_DISPATCH  32


/*
 * VARIABLES __________________________________________________________________
 *
 */

static CRITICAL_SECTION csDispatch;

static LPNOTIFYCALLBACK Handler = NULL;

static struct
   {
   NOTIFYWHEN when;
   LPIDENT lpiObject;
   HWND hWnd;
   LPARAM lpUser;
   } *aDispatchList = NULL;

static size_t nDispatchList = 0;


typedef struct NOTIFYQUEUEITEM
   {
   struct NOTIFYQUEUEITEM *pNext;
   NOTIFYEVENT evt;
   NOTIFYPARAMS Params;
   } NOTIFYQUEUEITEM, *LPNOTIFYQUEUEITEM;

static LPNOTIFYQUEUEITEM pDispatchQueuePop = NULL;
static LPNOTIFYQUEUEITEM pDispatchQueuePushAfter = NULL;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK DispatchNotification (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

void DispatchNotification_AltThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

void DispatchNotification_MainThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void CreateNotificationDispatch (void)
{
   InitializeCriticalSection (&csDispatch);

   Handler = New2 (NOTIFYCALLBACK,((NOTIFYCALLBACKPROC)DispatchNotification, 0L));
}


void PostNotification (NOTIFYEVENT evt, LPIDENT lpi)
{
   NOTIFYCALLBACK::SendNotificationToAll (evt, lpi);
}


/*
 * DispatchNotification
 *
 * This routine is called by our NOTIFYCALLBACK object ("Handler") whenever
 * anything interesting happens within the library.  Note that it's called
 * on whatever thread the library is using--for that reason, we only handle
 * some of the requests right away.  The rest get posted to our DispatchQueue,
 * to be popped off by g.hMain's thread.
 *
 */

BOOL CALLBACK DispatchNotification (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   // Some things must be done right now--for instance, if we've just created
   // or deleted a SERVER object, we'll load or free that server's preferences
   // instantly.  So, give our on-alternate-thread handler a chance to
   // look over the notification.
   //
   DispatchNotification_AltThread (evt, pParams);

   // Then push this notification onto our DispatchQueue, so that the main
   // pump will pop off each request in turn and take a look at it.
   //
   EnterCriticalSection (&csDispatch);

   LPNOTIFYQUEUEITEM lpnqi = New (NOTIFYQUEUEITEM);
   lpnqi->pNext = NULL;
   lpnqi->evt = evt;
   memcpy (&lpnqi->Params, pParams, sizeof(NOTIFYPARAMS));

   if (pDispatchQueuePushAfter != NULL)
      pDispatchQueuePushAfter->pNext = lpnqi;
   pDispatchQueuePushAfter = lpnqi;

   if (pDispatchQueuePop == NULL)
      pDispatchQueuePop = lpnqi;

   LeaveCriticalSection (&csDispatch);

   return TRUE;
}


void DispatchNotification_OnPump (void)
{
   BOOL fFound = TRUE;

   do {
      NOTIFYQUEUEITEM nqi;

      EnterCriticalSection (&csDispatch);

      if (!pDispatchQueuePop)
         fFound = FALSE;
      else
         {
         LPNOTIFYQUEUEITEM lpnqiNext = pDispatchQueuePop->pNext;
         memcpy (&nqi, pDispatchQueuePop, sizeof(NOTIFYQUEUEITEM));

         if (pDispatchQueuePushAfter == pDispatchQueuePop)
            pDispatchQueuePushAfter = NULL;
         Delete (pDispatchQueuePop);
         pDispatchQueuePop = lpnqiNext;
         }

      LeaveCriticalSection (&csDispatch);

      if (fFound)
         {
         DispatchNotification_MainThread (nqi.evt, &nqi.Params);
         }
      } while (fFound);
}


void DispatchNotification_AltThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   LPIDENT lpiEvt = pParams->lpi1;

   switch (evt)
      {
      case evtRefreshStatusEnd:
         if (lpiEvt && (lpiEvt->fIsService() || lpiEvt->fIsAggregate() || lpiEvt->fIsFileset()))
            {
            Alert_RemoveSecondary (lpiEvt);
            Alert_Scout_QueueCheckServer (lpiEvt);
            }
         if (lpiEvt && lpiEvt->fIsServer())
            {
            LPSERVER_PREF lpsp;
            if ((lpsp = (LPSERVER_PREF)lpiEvt->GetUserParam()) != NULL)
               {
               LPSERVER lpServer;
               if ((lpServer = lpiEvt->OpenServer()) != NULL)
                  {
                  if (lpsp->fIsMonitored != lpServer->fIsMonitored())
                     {
                     g.sub = Subsets_SetMonitor (g.sub, lpiEvt, lpServer->fIsMonitored());
                     UpdateDisplay_ServerWindow (FALSE, lpiEvt);
                     }
                  lpsp->fIsMonitored = lpServer->fIsMonitored();
                  lpServer->Close();
                  }
               }

            Alert_Scout_ServerStatus (lpiEvt, pParams->status);
            }
         break;

      // When we get a create request, use the object's Get/SetUserParam()
      // methods to attach an allocated structure to the thing--the structure
      // contains the preferences for the server/fileset/etc (for instance,
      // its warning threshholds, any current scout problems, etc).
      // On delete requests, free that structure.
      //
      case evtCreate:
         if (lpiEvt->fIsServer())
            {
            PVOID pPref = Server_LoadPreferences (lpiEvt);
            lpiEvt->SetUserParam (pPref);

            // Should this server be monitored?
            //
            if (!Subsets_fMonitorServer (g.sub, lpiEvt))
               {
               LPSERVER lpServer;
               if ((lpServer = lpiEvt->OpenServer()) != NULL)
                  {
                  lpServer->SetMonitor (FALSE);
                  lpServer->Close();
                  }
               }

            Alert_Scout_SetOutOfDate (lpiEvt);
            }
         else if (lpiEvt->fIsService())
            {
            PVOID pPref = Services_LoadPreferences (lpiEvt);
            lpiEvt->SetUserParam (pPref);
            }
         else if (lpiEvt->fIsAggregate())
            {
            PVOID pPref = Aggregates_LoadPreferences (lpiEvt);
            lpiEvt->SetUserParam (pPref);
            }
         else if (lpiEvt->fIsFileset())
            {
            PVOID pPref = Filesets_LoadPreferences (lpiEvt);
            lpiEvt->SetUserParam (pPref);
            }
         if (!lpiEvt->fIsCell())
            {
            Alert_Scout_QueueCheckServer (lpiEvt);
            }
         break;

      // When we get a create request, use the object's Get/SetUserParam()
      // methods to attach an allocated structure to the thing--the structure
      // contains the preferences for the server/fileset/etc (for instance,
      // its warning threshholds, any current scout problems, etc).
      // On delete requests, free that structure.
      //
      case evtDestroy:
         if (lpiEvt->fIsServer())
            {
            PVOID pPref = lpiEvt->GetUserParam();
            lpiEvt->SetUserParam (0);
            if (pPref)  Delete (pPref);
            }
         else if (lpiEvt->fIsService() || lpiEvt->fIsAggregate() || lpiEvt->fIsFileset())
            {
            Alert_RemoveSecondary (lpiEvt);
            PVOID pPref = lpiEvt->GetUserParam();
            lpiEvt->SetUserParam (0);
            if (pPref)  Delete (pPref);
            }
         break;
      }
}


void DispatchNotification_MainThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   LPIDENT lpiEvt = pParams->lpi1;

   // There are several notifications which are sent when beginning or ending
   // lengthy operations.  These "actions" each get a window indicating
   // progress, and get added to our ongoing list of actions-in-progress.
   //
   ActionNotification_MainThread (evt, pParams);

   // The real reason for this routine is as a dispatcher for the AFSClass's
   // notifications: various windows throughout the app register themselves
   // with this dispatcher, and thereby get a message whenever a particular
   // event of interest to that window happens.  Just what notifications
   // are "of interest" is specified by the window when it registers with
   // this dispatcher.
   //
   for (size_t iDispatch = 0; iDispatch < nDispatchList; ++iDispatch)
      {
      if (!aDispatchList[ iDispatch ].hWnd)
         continue;

      BOOL fDispatch = FALSE;

      // WHEN_CELL_OPENED + NULL      -> notify if any new cell is opened
      // WHEN_OBJECT_CHANGES + NULL   -> notify if anything at all changes
      // WHEN_OBJECT_CHANGES + lpi    -> notify if this object changes
      // WHEN_SVCS(etc)_CHANGE + NULL -> notify if any service at all changes
      // WHEN_SVCS(etc)_CHANGE + lpi  -> notify if any svc on this svr changes

      switch (aDispatchList[ iDispatch ].when)
         {
         case WHEN_CELL_OPENED:
            if (evt == evtCreate && lpiEvt->fIsCell())
               fDispatch = TRUE;
            break;

         case WHEN_OBJECT_CHANGES:
            if ( (aDispatchList[ iDispatch ].lpiObject == lpiEvt) ||
                 (aDispatchList[ iDispatch ].lpiObject == NULL) )
               {
               if (evt != evtCreate)
                  fDispatch = TRUE;
               }
            break;

         case WHEN_SVRS_CHANGE:
            switch (evt)
               {
               case evtInvalidate:
               case evtRefreshServersBegin:
               case evtRefreshServersEnd:
                  if ( (lpiEvt && lpiEvt->fIsCell()) ||
                       (aDispatchList[ iDispatch ].lpiObject == lpiEvt) ||
                       (aDispatchList[ iDispatch ].lpiObject == NULL) )
                     {
                     if (lpiEvt && lpiEvt->fIsCell())
                        fDispatch = TRUE;
                     }
                  break;

               case evtCreate:
               case evtDestroy:
               case evtRefreshStatusBegin:
               case evtRefreshStatusEnd:
               case evtAlertsChanged:
                  if (lpiEvt && lpiEvt->fIsServer())
                     {
                     if (aDispatchList[ iDispatch ].lpiObject == NULL)
                        fDispatch = TRUE;
                     else
                        {
                        LPIDENT lpiEvtCell = lpiEvt->GetCell();

                        if (aDispatchList[ iDispatch ].lpiObject == lpiEvtCell)
                           fDispatch = TRUE;
                        }
                     }
                  break;
               }
            break;

         case WHEN_SETS_CHANGE:
            switch (evt)
               {
               case evtInvalidate:
               case evtRefreshFilesetsBegin:
               case evtRefreshFilesetsEnd:
                  {
                  LPIDENT lpiEvtSvr = NULL;
                  if (lpiEvt && !lpiEvt->fIsCell())
                     lpiEvtSvr = lpiEvt->GetServer();

                  if ( (lpiEvt && lpiEvt->fIsCell()) ||
                       (aDispatchList[ iDispatch ].lpiObject == lpiEvt)    ||
                       (aDispatchList[ iDispatch ].lpiObject == lpiEvtSvr) ||
                       (aDispatchList[ iDispatch ].lpiObject == NULL) )
                     {
                     if (lpiEvt && (lpiEvt->fIsCell() || lpiEvt->fIsServer() || lpiEvt->fIsAggregate()))
                        fDispatch = TRUE;
                     }
                  }
                  break;

               case evtCreate:
               case evtDestroy:
               case evtRefreshStatusBegin:
               case evtRefreshStatusEnd:
               case evtAlertsChanged:
                  if (lpiEvt && lpiEvt->fIsFileset())
                     {
                     if (aDispatchList[ iDispatch ].lpiObject == NULL)
                        fDispatch = TRUE;
                     else
                        {
                        LPIDENT lpiEvtAgg = lpiEvt->GetAggregate();
                        LPIDENT lpiEvtSvr = lpiEvt->GetServer();

                        if (aDispatchList[ iDispatch ].lpiObject == lpiEvtAgg)
                           fDispatch = TRUE;
                        if (aDispatchList[ iDispatch ].lpiObject == lpiEvtSvr)
                           fDispatch = TRUE;
                        }
                     }
                  break;
               }
            break;

         case WHEN_AGGS_CHANGE:
            switch (evt)
               {
               case evtRefreshAggregatesBegin:
               case evtRefreshAggregatesEnd:
                  if ( (lpiEvt && lpiEvt->fIsCell()) ||
                       (aDispatchList[ iDispatch ].lpiObject == lpiEvt) ||
                       (aDispatchList[ iDispatch ].lpiObject == NULL) )
                     {
                     if (lpiEvt && (lpiEvt->fIsCell() || lpiEvt->fIsServer()))
                        fDispatch = TRUE;
                     }
                  break;

               case evtCreate:
               case evtDestroy:
               case evtRefreshStatusBegin:
               case evtRefreshStatusEnd:
               case evtAlertsChanged:
                  if (lpiEvt && lpiEvt->fIsAggregate())
                     {
                     if (aDispatchList[ iDispatch ].lpiObject == NULL)
                        fDispatch = TRUE;
                     else
                        {
                        LPIDENT lpiEvtSvr = lpiEvt->GetServer();

                        if (aDispatchList[ iDispatch ].lpiObject == lpiEvtSvr)
                           fDispatch = TRUE;
                        }
                     }
                  break;
               }
            break;

         case WHEN_SVCS_CHANGE:
            switch (evt)
               {
               case evtRefreshServicesBegin:
               case evtRefreshServicesEnd:
                  if ( (lpiEvt && lpiEvt->fIsCell()) ||
                       (aDispatchList[ iDispatch ].lpiObject == lpiEvt) ||
                       (aDispatchList[ iDispatch ].lpiObject == NULL) )
                     {
                     if (lpiEvt && (lpiEvt->fIsCell() || lpiEvt->fIsServer()))
                        fDispatch = TRUE;
                     }
                  break;

               case evtCreate:
               case evtDestroy:
               case evtRefreshStatusBegin:
               case evtRefreshStatusEnd:
               case evtAlertsChanged:
                  if (lpiEvt && lpiEvt->fIsService())
                     {
                     if (aDispatchList[ iDispatch ].lpiObject == NULL)
                        fDispatch = TRUE;
                     else
                        {
                        LPIDENT lpiEvtSvr = lpiEvt->GetServer();

                        if (aDispatchList[ iDispatch ].lpiObject == lpiEvtSvr)
                           fDispatch = TRUE;
                        }
                     }
                  break;
               }
            break;
         }

      if (fDispatch)
         {
         LPNOTIFYSTRUCT lpns = New (NOTIFYSTRUCT);
         lpns->hwndTarget = aDispatchList[ iDispatch ].hWnd;
         lpns->evt = evt;
         memcpy (&lpns->Params, pParams, sizeof(NOTIFYPARAMS));
         lpns->Params.lpUser = aDispatchList[ iDispatch ].lpUser;

         PostMessage (aDispatchList[ iDispatch ].hWnd,
                      WM_NOTIFY_FROM_DISPATCH,
                      (WPARAM)0,
                      (LPARAM)lpns);
         }
      }
}


void NotifyMe (NOTIFYWHEN when, LPIDENT lpiObject, HWND hWnd, LPARAM lpUser)
{
   EnterCriticalSection (&csDispatch);

   size_t iDispatch;
   for (iDispatch = 0; iDispatch < nDispatchList; ++iDispatch)
      {
      if ( (aDispatchList[ iDispatch ].hWnd == hWnd) &&
           (aDispatchList[ iDispatch ].when == when) &&
           (aDispatchList[ iDispatch ].lpiObject == lpiObject) )
         {
         LeaveCriticalSection (&csDispatch);
         return;
         }
      }

   for (iDispatch = 0; iDispatch < nDispatchList; ++iDispatch)
      {
      if (!aDispatchList[ iDispatch ].hWnd)
         break;
      }

   if (iDispatch >= nDispatchList)
      {
      if (!REALLOC (aDispatchList, nDispatchList, 1+iDispatch, cREALLOC_DISPATCH))
         {
         LeaveCriticalSection (&csDispatch);
         return;
         }
      }

   aDispatchList[ iDispatch ].hWnd = hWnd;
   aDispatchList[ iDispatch ].when = when;
   aDispatchList[ iDispatch ].lpiObject = lpiObject;
   aDispatchList[ iDispatch ].lpUser = lpUser;

   LeaveCriticalSection (&csDispatch);
}


void DontNotifyMe (NOTIFYWHEN when, LPIDENT lpiObject, HWND hWnd)
{
   EnterCriticalSection (&csDispatch);

   for (size_t iDispatch = 0; iDispatch < nDispatchList; ++iDispatch)
      {
      if ( (aDispatchList[ iDispatch ].hWnd == hWnd) &&
           (aDispatchList[ iDispatch ].when == when) &&
           (aDispatchList[ iDispatch ].lpiObject == lpiObject) )
         {
         aDispatchList[ iDispatch ].hWnd = NULL;
         }
      }

   LeaveCriticalSection (&csDispatch);
}


void DontNotifyMeEver (HWND hWnd)
{
   EnterCriticalSection (&csDispatch);

   for (size_t iDispatch = 0; iDispatch < nDispatchList; ++iDispatch)
      {
      if (aDispatchList[ iDispatch ].hWnd == hWnd)
         {
         aDispatchList[ iDispatch ].hWnd = NULL;
         }
      }

   LeaveCriticalSection (&csDispatch);
}

