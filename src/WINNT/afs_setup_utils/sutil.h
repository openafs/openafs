/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_SUTIL_H
#define AFS_SUTIL_H

/* Some install/uninstall related utilities */

#ifdef __cplusplus
extern "C" {
#endif

extern BOOL InNetworkProviderOrder(char *pszNetworkProvider, BOOL *pbIn);
extern BOOL AddToProviderOrder(char *pszWhatToAdd);
extern BOOL RemoveFromProviderOrder(char *pszWhatToDel);
extern BOOL ReadSystemEnv(char **ppszEnvValue, char *pszEnvName);
extern BOOL WriteSystemEnv(char *pszEnvValue, char *pszEnvName);
extern BOOL AddToSystemPath(char *pszPath);
extern BOOL RemoveFromSystemPath(char *pszPath);
extern BOOL IsWinNT();

#ifdef __cplusplus
};
#endif

#endif /* AFS_SUTIL_H */
