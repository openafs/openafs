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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL IsServiceRunning (void);
BOOL IsServicePersistent (void);
BOOL IsServiceConfigured (void);

int GetCurrentCredentials (void);

int DestroyCurrentCredentials (LPCTSTR pszCell);

int ObtainNewCredentials (LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, BOOL Silent);

int GetDefaultCell (LPTSTR pszCell);

void GetGatewayName (LPTSTR pszGateway);

BOOL Creds_OpenLibraries (void);
void Creds_CloseLibraries (void);

#ifdef __cplusplus
}
#endif
#endif

