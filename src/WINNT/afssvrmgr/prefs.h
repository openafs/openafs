/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PREFS_H
#define PREFS_H

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ErasePreferences (LPTSTR pszCell = NULL, LPTSTR pszServer = NULL);
BOOL RestorePreferences (LPIDENT, LPVOID, size_t);
BOOL StorePreferences (LPIDENT, LPVOID, size_t);

HKEY OpenSubsetsKey (LPTSTR pszCell, BOOL fCreate); // fCreate: 0=open, 1=create, 2=delete
HKEY OpenSubsetsSubKey (LPTSTR pszCell, LPTSTR pszSubset, BOOL fCreate);


#endif

