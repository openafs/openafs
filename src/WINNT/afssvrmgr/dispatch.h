/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DISPATCH_H
#define DISPATCH_H

#include "messages.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   WHEN_CELL_OPENED,
   WHEN_OBJECT_CHANGES,	// (supply PVOID=lpiObject)
   WHEN_SVRS_CHANGE,	// (supply PVOID=lpiCell)
   WHEN_SETS_CHANGE,	// (supply PVOID=lpiServer or lpiAgg)
   WHEN_AGGS_CHANGE,	// (supply PVOID=lpiServer)
   WHEN_SVCS_CHANGE	// (supply PVOID=lpiServer)
   } NOTIFYWHEN;

typedef struct
   {
   HWND hwndTarget;
   NOTIFYEVENT evt;
   NOTIFYPARAMS Params;
   } NOTIFYSTRUCT, *LPNOTIFYSTRUCT;

#define evtAlertsChanged  (NOTIFYEVENT)(evtUser)
#define evtScoutBegin     (NOTIFYEVENT)(evtUser+1) // lpEvt = (LPIDENT)lpiServer
#define evtScoutEnd       (NOTIFYEVENT)(evtUser+2) // lpEvt = (LPIDENT)lpiServer


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void CreateNotificationDispatch (void);

void PostNotification (NOTIFYEVENT evt, LPIDENT lpi1);

void NotifyMe (NOTIFYWHEN when, LPIDENT lpObject, HWND hWnd, LPARAM lpUser);
void DontNotifyMe (NOTIFYWHEN when, LPIDENT lpObject, HWND hWnd);
void DontNotifyMeEver (HWND hWnd);

void DispatchNotification_OnPump (void);


#endif

