/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AGG_GENERAL_H
#define AGG_GENERAL_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

PVOID Aggregates_LoadPreferences (LPIDENT lpiAggregate);
BOOL Aggregates_SavePreferences (LPIDENT lpiAggregate);

LPIDENT Aggregates_GetFocused (HWND hDlg);
LPIDENT Aggregates_GetSelected (HWND hDlg);


#endif

