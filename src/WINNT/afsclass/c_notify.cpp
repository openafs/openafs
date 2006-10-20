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

#define cREALLOC_NOTIFY  4	// allocate space for 4 notifies at once


/*
 * VARIABLES __________________________________________________________________
 *
 */

size_t            NOTIFYCALLBACK::nNotifyList = 0;
LPNOTIFYCALLBACK *NOTIFYCALLBACK::aNotifyList = NULL;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */

NOTIFYCALLBACK::NOTIFYCALLBACK (NOTIFYCALLBACKPROC procUser, LPARAM lpUser)
{
   procSupplied = procUser;
   lpSupplied = lpUser;
   size_t iNotify;

   for (iNotify = 0; iNotify < nNotifyList; ++iNotify)
      {
      if (aNotifyList[ iNotify ] == NULL)
         break;
      }

   if (iNotify >= nNotifyList)
      {
      REALLOC (aNotifyList, nNotifyList, 1+iNotify, cREALLOC_NOTIFY );
      }

   if (iNotify < nNotifyList)
      {
      aNotifyList[ iNotify ] = this;
      }
}


NOTIFYCALLBACK::~NOTIFYCALLBACK (void)
{
   for (size_t iNotify = 0; iNotify < nNotifyList; ++iNotify)
      {
      if (aNotifyList[ iNotify ] == this)
         aNotifyList[ iNotify ] = NULL;
      }
}


BOOL NOTIFYCALLBACK::SendNotificationToAll (NOTIFYEVENT evt, ULONG status)
{
   return SendNotificationToAll (evt, NULL, NULL, NULL, NULL, 0, status);
}

BOOL NOTIFYCALLBACK::SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, ULONG status)
{
   return SendNotificationToAll (evt, lpi1, NULL, NULL, NULL, 0, status);
}

BOOL NOTIFYCALLBACK::SendNotificationToAll (NOTIFYEVENT evt, LPTSTR psz1, ULONG status)
{
   return SendNotificationToAll (evt, NULL, NULL, psz1, NULL, 0, status);
}

BOOL NOTIFYCALLBACK::SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, LPTSTR psz1, ULONG status)
{
   return SendNotificationToAll (evt, lpi1, NULL, psz1, NULL, 0, status);
}

BOOL NOTIFYCALLBACK::SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, LPIDENT lpi2, LPTSTR psz1, LPTSTR psz2, DWORD dw1, ULONG status)
{
   BOOL rc = TRUE;

   NOTIFYPARAMS Params;
   memset (&Params, 0x00, sizeof(Params));
   Params.lpi1 = lpi1;
   Params.lpi2 = lpi2;
   lstrcpy (Params.sz1, (psz1) ? psz1 : TEXT(""));
   lstrcpy (Params.sz2, (psz2) ? psz2 : TEXT(""));
   Params.dw1 = dw1;
   Params.status = status;

   for (size_t iNotify = 0; iNotify < nNotifyList; ++iNotify)
      {
      if (aNotifyList[ iNotify ] != NULL)
         {
         Params.lpUser = aNotifyList[ iNotify ]->lpSupplied;
         if (!aNotifyList[ iNotify ]->SendNotification (evt, &Params))
            rc = FALSE;
         }
      }

   return rc;
}


BOOL NOTIFYCALLBACK::SendNotification (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   BOOL rc = TRUE;

   if (procSupplied != NULL) {
      try {
         if (!(*procSupplied)( evt, pParams ))
            rc = FALSE;
      } catch(...) {
         // whoops--never trust a callback.
#ifdef DEBUG
         DebugBreak();
#endif
         rc = FALSE;
      }
   }

   return rc;
}

