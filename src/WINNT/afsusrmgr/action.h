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

void Actions_SetDefaultView (LPVIEWINFO lpvi);

void Actions_OpenWindow (void);
void Actions_CloseWindow (void);
void Actions_WindowToTop (BOOL fTop);

void Actions_OnNotify (WPARAM wp, LPARAM lp);


#endif

