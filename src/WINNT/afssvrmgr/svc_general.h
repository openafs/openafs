/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_GENERAL_H
#define SVC_GENERAL_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

PVOID Services_LoadPreferences (LPIDENT lpiService);
BOOL Services_SavePreferences (LPIDENT lpiService);

void Services_GuessLogName (LPTSTR pszLogFile, LPTSTR pszServiceName);


#endif

