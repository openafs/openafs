/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <fastlist.h>
#include "drivemap.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define iRankREMOVED  0

typedef struct
   {
   int ipServer;
   TCHAR szServer[ cchRESOURCE ];
   int iRank;
   BOOL fChanged;
   HLISTITEM hItem;
   } SERVERPREF, *PSERVERPREF;

typedef struct
   {
   BOOL fVLServers;
   SERVERPREF *aPrefs;
   size_t cPrefs;
   } SERVERPREFS, *PSERVERPREFS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

DWORD Config_GetServiceState (void);

void Config_GetCellName (LPTSTR pszName);
BOOL Config_SetCellName (LPCTSTR pszName, ULONG *pStatus = NULL);

void Config_GetGatewayFlag (BOOL *pfFlag);
BOOL Config_SetGatewayFlag (BOOL fFlag, ULONG *pStatus = NULL);

void Config_GetGatewayName (LPTSTR pszName);
BOOL Config_SetGatewayName (LPCTSTR pszName, ULONG *pStatus = NULL);

BOOL Config_ContactGateway (LPTSTR pszGateway, LPTSTR pszCell);
void Config_FixGatewayDrives (void);

void Config_GetTrayIconFlag (BOOL *pfFlag);
BOOL Config_SetTrayIconFlag (BOOL fFlag, ULONG *pStatus = NULL);

PSERVERPREFS Config_GetServerPrefs (BOOL fVLServers);
BOOL Config_SetServerPrefs (PSERVERPREFS pPrefs, ULONG *pStatus = NULL);
void Config_FreeServerPrefs (PSERVERPREFS pPrefs);

void Config_GetCacheSize (ULONG *pckCache);
BOOL Config_SetCacheSize (ULONG ckCache, ULONG *pStatus = NULL);
BOOL Config_GetCacheInUse (ULONG *pckCacheInUse, ULONG *pStatus = NULL);

void Config_GetChunkSize (ULONG *pckChunk);
BOOL Config_SetChunkSize (ULONG ckChunk, ULONG *pStatus = NULL);

void Config_GetStatEntries (ULONG *pcEntries);
BOOL Config_SetStatEntries (ULONG cEntries, ULONG *pStatus = NULL);

void Config_GetProbeInt (ULONG *pcsecProbe);
BOOL Config_SetProbeInt (ULONG csecProbe, ULONG *pStatus = NULL);

void Config_GetNumThreads (ULONG *pcThreads);
BOOL Config_SetNumThreads (ULONG cThreads, ULONG *pStatus = NULL);

void Config_GetNumDaemons (ULONG *pcDaemons);
BOOL Config_SetNumDaemons (ULONG cDaemons, ULONG *pStatus = NULL);

void Config_GetSysName (LPTSTR pszName);
BOOL Config_SetSysName (LPCTSTR pszName, ULONG *pStatus = NULL);

void Config_GetRootVolume (LPTSTR pszName);
BOOL Config_SetRootVolume (LPCTSTR pszName, ULONG *pStatus = NULL);

void Config_GetMountRoot (LPTSTR pszPath);
BOOL Config_SetMountRoot (LPCTSTR pszPath, ULONG *pStatus = NULL);

void Config_GetCachePath (LPTSTR pszPath);
BOOL Config_SetCachePath (LPCTSTR pszPath, ULONG *pStatus = NULL);

void Config_GetLanAdapter (ULONG *pnLanAdapter);
BOOL Config_SetLanAdapter (ULONG nLanAdapter, ULONG *pStatus = NULL);

void Config_GetTrapOnPanic (BOOL *pfFlag);
BOOL Config_SetTrapOnPanic (BOOL fFlag, ULONG *pStatus = NULL);

void Config_GetTraceBufferSize (ULONG *pnBufSize);
BOOL Config_SetTraceBufferSize (ULONG nBufSize, ULONG *pStatus = NULL);

void Config_GetLoginRetryInterval (ULONG *pnBufSize);
BOOL Config_SetLoginRetryInterval (ULONG nInterval, ULONG *pStatus = NULL);

void Config_GetFailLoginsSilently (BOOL *pfFlag);
BOOL Config_SetFailLoginsSilently (BOOL fFlag, ULONG *pStatus = NULL);

void Config_GetReportSessionStartups (BOOL *pfFlag);
BOOL Config_SetReportSessionStartups (BOOL fFlag, ULONG *pStatus = NULL);

void Config_GetGlobalDriveList (DRIVEMAPLIST *pDriveList);

BOOL Config_ReadGlobalNum (LPCTSTR pszLHS, DWORD *pdwRHS);
BOOL Config_ReadGlobalString (LPCTSTR pszLHS, LPTSTR pszRHS, size_t cchMax);
void Config_WriteGlobalNum (LPCTSTR pszLHS, DWORD dwRHS);
void Config_WriteGlobalString (LPCTSTR pszLHS, LPCTSTR pszRHS);

BOOL Config_ReadUserNum (LPCTSTR pszLHS, DWORD *pdwRHS);
BOOL Config_ReadUserString (LPCTSTR pszLHS, LPTSTR pszRHS, size_t cchMax);
void Config_WriteUserNum (LPCTSTR pszLHS, DWORD dwRHS);
void Config_WriteUserString (LPCTSTR pszLHS, LPCTSTR pszRHS);


#endif

