/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SHORTCUT_H
#define SHORTCUT_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Shortcut_Init (void);
void Shortcut_Exit (void);
BOOL Shortcut_Create (LPTSTR pszTarget, LPCTSTR pszSource, LPTSTR pszDesc = NULL, LPTSTR pszArgs = NULL);
void Shortcut_FixStartup (LPCTSTR pszLinkName, BOOL fAutoStart);


#endif

