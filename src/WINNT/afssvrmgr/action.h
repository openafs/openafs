/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef ACTION_H
#define ACTION_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ActionNotification_MainThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

void Action_OpenWindow (void);
void Action_CloseWindow (void);
void Action_SetDefaultView (LPVIEWINFO lpvi);
void Action_WindowToTop (BOOL fTop);

BOOL Action_fAnyActive (void);

void Action_ShowConfirmations (BOOL fShow);


#endif

