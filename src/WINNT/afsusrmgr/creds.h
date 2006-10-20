/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CREDS_H
#define CREDS_H

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

int OpenCellDialog (void);
BOOL NewCredsDialog (void);
void CheckForExpiredCredentials (void);
BOOL CheckCredentials (BOOL fComplain);
void ShowCurrentCredentials (void);

#endif

