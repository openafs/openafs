/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCLIENTBIND_H
#define TAAFSADMSVRCLIENTBIND_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL ADMINAPI BindToAdminServer (LPCTSTR pszAddress, BOOL fWait, DWORD *pidClient, ULONG *pStatus);
BOOL ADMINAPI UnbindFromAdminServer (DWORD idClient, ULONG *pStatus);
BOOL ADMINAPI ForkNewAdminServer (ULONG *pStatus);

LPCTSTR ADMINAPI ResolveAddress (LPCTSTR pszAddress);


#endif // TAAFSADMSVRCLIENTBIND_H

