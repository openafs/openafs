/* Copyright (C) 1999  Transarc Corporation.  All rights reserved.
 *
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
