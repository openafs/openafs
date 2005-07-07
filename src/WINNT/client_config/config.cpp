/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afs_config.h"
#include <WINNT\afsreg.h>

extern "C" {

#include "../afsd/fs_utils.h"

#define __CM_CONFIG_INTERFACES_ONLY__
#include "../afsd/cm_config.h"

#define __CM_IOCTL_INTERFACES_ONLY__
#include "../afsd/cm_ioctl.h"

} // extern "C"


#define cREALLOC_PREFS       32

#define cSERVERPREFS_CHUNK  256

#define PIOCTL_MAXSIZE     2048



/*
 * ROUTINES ___________________________________________________________________
 *
 */

static DWORD log2 (DWORD dwValue)
{
   DWORD dwLog;
   for (dwLog = 0; (DWORD)(1<<dwLog) < dwValue; ++dwLog)
      ;
   return dwLog;
}

DWORD Config_GetServiceState (void)
{
   if (!g.fIsWinNT)
      {
      TCHAR szGateway[ cchRESOURCE ] = TEXT("");
      Config_GetGatewayName (szGateway);
      return (szGateway[0]) ? SERVICE_RUNNING : SERVICE_STOPPED;
      }

   SERVICE_STATUS Status;
   memset (&Status, 0x00, sizeof(Status));

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         QueryServiceStatus (hService, &Status);
         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   return Status.dwCurrentState;
}


/*
 * Configuration Routine ______________________________________________________
 *
 */

void Config_GetGatewayFlag (BOOL *pfFlag)
{
    if (!Config_ReadGlobalNum (TEXT("IsGateway"), (DWORD*)pfFlag))
        *pfFlag = FALSE;
}


BOOL Config_SetGatewayFlag (BOOL fFlag, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("IsGateway"), fFlag);
    g.fNeedRestart = TRUE;
    return TRUE;
}


void Config_GetGatewayName (LPTSTR pszName)
{
    if (!Config_ReadGlobalString (TEXT("Gateway"), pszName, MAX_PATH))
        GetString (pszName, IDS_GATEWAY_UNKNOWN);
    else if (!*pszName)
        GetString (pszName, IDS_GATEWAY_UNKNOWN);
}


BOOL Config_SetGatewayName (LPCTSTR pszName, ULONG *pStatus)
{
   TCHAR szBogus[ cchRESOURCE ];
   GetString (szBogus, IDS_GATEWAY_UNKNOWN);
   if (!lstrcmpi (szBogus, pszName))
      {
      Config_WriteGlobalString (TEXT("Gateway"), TEXT(""));
      }
   else
      {
      Config_WriteGlobalString (TEXT("Gateway"), pszName);
      }

   return TRUE;
}


void Config_FixGatewayDrives (void)
{
   // Zip through the user's network drives and reconnect
   // them as necessary.
   //
   for (TCHAR chDrive = chDRIVE_A; chDrive <= chDRIVE_Z; ++chDrive)
      {
      TCHAR szSubmount[ MAX_PATH ];
      if (!GetDriveSubmount (chDrive, szSubmount))
         continue;

      // We've got a mapping into AFS!  Remove it, rather forcefully.
      //
      TCHAR szDrive[] = "@:";
      szDrive[0] = chDrive;
      WNetCancelConnection (szDrive, TRUE);
      }

   // Now recreate our drive mappings, based on the user's already-
   // specified preferences.
   //
   DRIVEMAPLIST List;
   QueryDriveMapList (&List);

   for (size_t ii = 0; ii < 26; ++ii)
      {
      if (!List.aDriveMap[ii].szMapping[0])
         continue;
      ActivateDriveMap (List.aDriveMap[ii].chDrive, List.aDriveMap[ii].szMapping, List.aDriveMap[ii].szSubmount, List.aDriveMap[ii].fPersistent);
      }
}


void Config_GetCellName (LPTSTR pszName)
{
   if (!Config_ReadGlobalString (TEXT("Cell"), pszName, MAX_PATH))
      GetString (pszName, IDS_CELL_UNKNOWN);
   else if (!*pszName)
      GetString (pszName, IDS_CELL_UNKNOWN);
}


BOOL Config_ContactGateway (LPTSTR pszGateway, LPTSTR pszCell)
{
   BOOL rc = FALSE;

   BYTE OutData[ PIOCTL_MAXSIZE ];
   memset (OutData, 0x00, sizeof(OutData));

   struct ViceIoctl IOInfo;
   IOInfo.in_size = 0;
   IOInfo.in = 0;
   IOInfo.out = (char *)OutData;
   IOInfo.out_size = PIOCTL_MAXSIZE;

   TCHAR szOldGateway[ MAX_PATH ];
   Config_GetGatewayName (szOldGateway);
   Config_SetGatewayName (pszGateway);

   ULONG status;
   if ((status = pioctl (0, VIOC_GET_WS_CELL, &IOInfo, 1)) == 0)
      {
      if (OutData[0])
         {
         lstrcpy (pszCell, (LPCTSTR)OutData);
         rc = TRUE;
         }
      }

   Config_SetGatewayName (szOldGateway);

   return rc;
}


BOOL Config_SetCellName (LPCTSTR pszName, ULONG *pStatus)
{
   TCHAR szBogus[ cchRESOURCE ];
   GetString (szBogus, IDS_CELL_UNKNOWN);
   if (lstrcmpi (szBogus, pszName))
      {
      Config_WriteGlobalString (TEXT("Cell"), pszName);
      g.fNeedRestart = TRUE;
      }
   return TRUE;
}

#if 0
/* 	These two functions are not needed as of the 1.2.2a updates.
	The old implementation used to 'bind' afslogon.dll to the credentials manager
	when the Integrated Logon was selected.

	With version 1.2.2a afslogon.dll is always 'bound' to the credentials manager; therefore,
	the binding operation is done during installation.  Note: the Integrated Logon is
	selected by an entry in the registry (LogonOptions).
*/
void Config_GetAuthentFlag (BOOL *pfFlag)
{
   *pfFlag = FALSE;

   HKEY hk;
   if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Control\\NetworkProvider\\Order"), 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS)
      {
      if (g.fIsWinNT)
         {
         TCHAR szProviders[ MAX_PATH ] = TEXT("");
         DWORD dwSize = sizeof(szProviders);

         if (RegQueryValueEx (hk, TEXT("ProviderOrder"), NULL, NULL, (PBYTE)szProviders, &dwSize) == ERROR_SUCCESS)
            {
            for (LPTSTR pch = szProviders; *pch; )
               {
               if (!lstrncmpi (pch, TEXT("TransarcAFSDaemon"), lstrlen(TEXT("TransarcAFSDaemon"))))
                  *pfFlag = TRUE;

               for ( ; *pch && (*pch != TEXT(',')); ++pch)
                  ;
               for ( ; *pch == TEXT(','); ++pch)
                  ;
               }
            }

         RegCloseKey (hk);
         }
      else // (!g.fIsWinNT)
         {
         TCHAR szLHS[ MAX_PATH ] = TEXT("");
         DWORD dwSize = sizeof(szLHS);

         if (RegQueryValueEx (hk, TEXT("TransarcAFSDaemon"), NULL, NULL, (PBYTE)szLHS, &dwSize) == ERROR_SUCCESS)
            *pfFlag = TRUE;
         }
      }
}


BOOL Config_SetAuthentFlag (BOOL fFlag, ULONG *pStatus)
{
   ULONG status = 0;
   BOOL rc = FALSE;

   HKEY hk;
   DWORD dwDisp;
   if ((status = RegCreateKeyEx (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Control\\NetworkProvider\\Order"), 0, TEXT("container"), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &hk, &dwDisp)) == ERROR_SUCCESS)
      {
      if (g.fIsWinNT)
         {
         TCHAR szOldProviders[ MAX_PATH ] = TEXT("");
         TCHAR szNewProviders[ MAX_PATH ] = TEXT("");
         DWORD dwSize = sizeof(szOldProviders);
         RegQueryValueEx (hk, TEXT("ProviderOrder"), NULL, NULL, (PBYTE)szOldProviders, &dwSize);

         for (LPTSTR pch = szOldProviders; *pch; )
            {
            BOOL fCopy = TRUE;
            if (!lstrncmpi (pch, TEXT("TransarcAFSDaemon"), lstrlen(TEXT("TransarcAFSDaemon"))))
               {
               fCopy = fFlag;
               fFlag = FALSE;
               }

            if (fCopy)
               {
               LPTSTR pchOut = &szNewProviders[ lstrlen(szNewProviders) ];
               if (szNewProviders[0])
                  *pchOut++ = TEXT(',');
               for ( ; *pch && (*pch != TEXT(',')); )
                  *pchOut++ = *pch++;
               *pchOut = TEXT('\0');
               }

            for ( ; *pch && (*pch != TEXT(',')); ++pch)
               ;
            for ( ; *pch == TEXT(','); ++pch)
               ;
            }

         if (fFlag)
            {
            if (szNewProviders[0])
               lstrcat (szNewProviders, TEXT(","));
            lstrcat (szNewProviders, TEXT("TransarcAFSDaemon"));
            }

         if ((status = RegSetValueEx (hk, TEXT("ProviderOrder"), NULL, REG_SZ, (PBYTE)szNewProviders, sizeof(TCHAR)*(1+lstrlen(szNewProviders)))) == ERROR_SUCCESS)
            rc = TRUE;
         }
      else // (!g.fIsWinNT)
         {
         TCHAR szLHS[ cchRESOURCE ] = TEXT("TransarcAFSDaemon");
         TCHAR szRHS[ cchRESOURCE ] = TEXT("");

         if (fFlag)
            {
            if ((status = RegSetValueEx (hk, szLHS, NULL, REG_SZ, (PBYTE)szRHS, sizeof(TCHAR)*(lstrlen(szRHS)+1))) == 0)
               rc = TRUE;
            }
         else
            {
            RegDeleteValue (hk, szLHS);
            rc = TRUE;
            }
         }

      RegCloseKey (hk);
      }

   if (pStatus && !rc)
      *pStatus = status;
   if (!rc)
      Message (MB_ICONHAND, GetErrorTitle(), IDS_FAILCONFIG_AUTHENT, TEXT("%ld"), status);
   return rc;
}
#endif

void Config_GetTrayIconFlag (BOOL *pfFlag)
{
   if (!Config_ReadUserNum (TEXT("ShowTrayIcon"), (DWORD*)pfFlag))
      *pfFlag = FALSE;
}


BOOL Config_SetTrayIconFlag (BOOL fFlag, ULONG *pStatus)
{
   Config_WriteUserNum (TEXT("ShowTrayIcon"), fFlag);

   HWND hSearch;
   for (hSearch = GetWindow (GetDesktopWindow(), GW_CHILD);
        hSearch && IsWindow(hSearch);
        hSearch = GetWindow (hSearch, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];
      if (GetClassName (hSearch, szClassName, cchRESOURCE))
         {
         if (!lstrcmpi (szClassName, TEXT("AfsCreds")))
            break;
         }
      }

   if (hSearch && IsWindow(hSearch))
      {
      UINT msgCheckTerminate = RegisterWindowMessage (TEXT("AfsCredsCheckTerminate"));
      PostMessage (hSearch, msgCheckTerminate, 0, 0);
      }
   else if (fFlag && !(hSearch && IsWindow(hSearch)))
      {
      WinExec (TEXT("AfsCreds.exe /quiet"), SW_SHOW);
      }

   return TRUE;
}


PSERVERPREFS Config_GetServerPrefs (BOOL fVLServers)
{
   PSERVERPREFS pPrefs = New (SERVERPREFS);
   memset (pPrefs, 0x00, sizeof(SERVERPREFS));
   pPrefs->fVLServers = fVLServers;

   if (Config_GetServiceState() == SERVICE_RUNNING)
      {
      // We retrieve server prefs in batches--start that loop now.
      //
      size_t iOut = 0;
      for (int iOffset = 0; ; )
         {
         cm_SPrefRequest_t InData;
         memset (&InData, 0x00, sizeof(InData));
         InData.offset = iOffset;
         InData.flags = (pPrefs->fVLServers) ? CM_SPREF_VLONLY : 0;
         InData.num_servers = cSERVERPREFS_CHUNK;

         BYTE OutDataStorage[ sizeof(cm_SPrefInfo_t) + cSERVERPREFS_CHUNK * sizeof(cm_SPref_t) ];
         memset (OutDataStorage, 0x00, sizeof(OutDataStorage));
         cm_SPrefInfo_t *pOutData = (cm_SPrefInfo_t *)OutDataStorage;

         struct ViceIoctl IOInfo;
         IOInfo.in_size = sizeof(InData);
         IOInfo.in = (char *)&InData;
         IOInfo.out = (char *)pOutData;
         IOInfo.out_size = sizeof(cm_SPrefInfo_t) + cSERVERPREFS_CHUNK * sizeof(cm_SPref_t);

         if (pioctl (0, VIOC_GETSPREFS, &IOInfo, 1) != 0)
            break;

         if (!REALLOC (pPrefs->aPrefs, pPrefs->cPrefs, iOut + pOutData->num_servers, cREALLOC_PREFS))
            break;

         for (size_t ii = 0; ii < pOutData->num_servers; ++ii)
            {
            pPrefs->aPrefs[ iOut ].ipServer = pOutData->servers[ ii ].host.s_addr;
            pPrefs->aPrefs[ iOut ].iRank = pOutData->servers[ ii ].rank;
            iOut ++;
            }

         if ((iOffset = pOutData->next_offset) == 0)
            break;
         }
     }

   return pPrefs;
}


BOOL Config_SetServerPrefs (PSERVERPREFS pPrefs, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (pPrefs)
      {
      size_t cChanged = 0;
      size_t ii;
      for (ii = 0; ii < pPrefs->cPrefs; ++ii)
         {
         if (pPrefs->aPrefs[ ii ].fChanged)
            ++cChanged;
         }

      if (cChanged)
         {
         if (Config_GetServiceState() != SERVICE_RUNNING)
            {
            rc = FALSE;
            status = ERROR_SERVICE_NOT_ACTIVE;
            }
         else
            {
            size_t cbInDataStorage = sizeof(cm_SSetPref_t) + cChanged * sizeof(cm_SPref_t);

            PBYTE pInDataStorage;
            if ((pInDataStorage = (PBYTE)Allocate (cbInDataStorage)) == NULL)
               {
               rc = FALSE;
               status = ERROR_NOT_ENOUGH_MEMORY;
               }
            else
               {
               memset (pInDataStorage, 0x00, sizeof(cbInDataStorage));

               cm_SSetPref_t *pInData = (cm_SSetPref_t*)pInDataStorage;
               pInData->flags = (pPrefs->fVLServers) ? CM_SPREF_VLONLY : 0;
               pInData->num_servers = cChanged;

               size_t iOut = 0;
               for (ii = 0; ii < pPrefs->cPrefs; ++ii)
                  {
                  if (pPrefs->aPrefs[ ii ].fChanged)
                     {
                     pInData->servers[ iOut ].host.s_addr = pPrefs->aPrefs[ ii ].ipServer;
                     pInData->servers[ iOut ].rank = (unsigned short)pPrefs->aPrefs[ ii ].iRank;
                     iOut++;
                     }
                  }

               BYTE OutDataStorage[ PIOCTL_MAXSIZE ];

               struct ViceIoctl IOInfo;
               IOInfo.in_size = cbInDataStorage;
               IOInfo.in = (char *)pInData;
               IOInfo.out = (char *)OutDataStorage;
               IOInfo.out_size = PIOCTL_MAXSIZE;

               if ((status = pioctl (0, VIOC_SETSPREFS, &IOInfo, 1)) != 0)
                  {
                  rc = FALSE;
                  }

               Free (pInDataStorage);
               }
            }
         }
      }

   if (pStatus && !rc)
      *pStatus = status;
   if (!rc)
      Message (MB_ICONHAND, GetErrorTitle(), IDS_FAILCONFIG_PREFS, TEXT("%ld"), status);
   return rc;
}


void Config_FreeServerPrefs (PSERVERPREFS pPrefs)
{
   if (pPrefs->aPrefs)
      Free (pPrefs->aPrefs);
   memset (pPrefs, 0xFD, sizeof(SERVERPREFS));
   Delete (pPrefs);
}


void Config_GetCacheSize (ULONG *pckCache)
{
    if (!Config_ReadGlobalNum (TEXT("CacheSize"), (DWORD*)pckCache))
        *pckCache = CM_CONFIGDEFAULT_CACHESIZE;
}


BOOL Config_SetCacheSize (ULONG ckCache, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("CacheSize"), ckCache);
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetChunkSize (ULONG *pckChunk)
{
    if (!Config_ReadGlobalNum (TEXT("ChunkSize"), (DWORD*)pckChunk))
        *pckChunk = CM_CONFIGDEFAULT_CHUNKSIZE;
    *pckChunk = max (*pckChunk, 10);
    *pckChunk = (1 << ((*pckChunk)-10));
}


BOOL Config_SetChunkSize (ULONG ckChunk, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("ChunkSize"), log2(ckChunk * 1024));
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetStatEntries (ULONG *pcEntries)
{
    if (!Config_ReadGlobalNum (TEXT("Stats"), (DWORD*)pcEntries))
        *pcEntries = CM_CONFIGDEFAULT_STATS;
}


BOOL Config_SetStatEntries (ULONG cEntries, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("Stats"), cEntries);
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetProbeInt (ULONG *pcsecProbe)
{
    *pcsecProbe = 30;
    // TODO: NEED REGISTRY SETTING
}


BOOL Config_SetProbeInt (ULONG csecProbe, ULONG *pStatus)
{
    BOOL rc = TRUE;
    ULONG status = 0;

    // TODO: NEED REGISTRY SETTING
    if (Config_GetServiceState() == SERVICE_RUNNING)
    {
        struct chservinfo checkserv;
        memset (&checkserv, 0x00, sizeof(checkserv));
        checkserv.magic = 0x12345678;
        checkserv.tinterval = csecProbe;

        BYTE OutData[ PIOCTL_MAXSIZE ];
        memset (OutData, 0x00, sizeof(OutData));

        struct ViceIoctl IOInfo;
        IOInfo.in_size = sizeof(checkserv);
        IOInfo.in = (char *)&checkserv;
        IOInfo.out = (char *)OutData;
        IOInfo.out_size = PIOCTL_MAXSIZE;

        if ((status = pioctl (0, VIOCCKSERV, &IOInfo, 1)) != 0)
        {
            rc = FALSE;
        }
    }

    if (pStatus && !rc)
        *pStatus = status;
    if (!rc)
        Message (MB_ICONHAND, GetErrorTitle(), IDS_FAILCONFIG_PROBE, TEXT("%ld"), status);
    return rc;
}



void Config_GetNumThreads (ULONG *pcThreads)
{
    if (!Config_ReadGlobalNum (TEXT("ServerThreads"), (DWORD*)pcThreads))
        *pcThreads = CM_CONFIGDEFAULT_SVTHREADS;
}


BOOL Config_SetNumThreads (ULONG cThreads, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("ServerThreads"), cThreads);
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetNumDaemons (ULONG *pcDaemons)
{
    if (!Config_ReadGlobalNum (TEXT("Daemons"), (DWORD*)pcDaemons))
        *pcDaemons = CM_CONFIGDEFAULT_DAEMONS;
}


BOOL Config_SetNumDaemons (ULONG cDaemons, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("Daemons"), cDaemons);
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetSysName (LPTSTR pszName)
{
    if (!Config_ReadGlobalString (TEXT("SysName"), pszName, MAX_PATH))
        lstrcpy (pszName, TEXT("i386_nt40"));
}


BOOL Config_SetSysName (LPCTSTR pszName, ULONG *pStatus)
{
    BOOL rc = TRUE;
    ULONG status = 0;

    if (Config_GetServiceState() == SERVICE_RUNNING)
    {
        struct {
            ULONG cbData;
            TCHAR szData[ PIOCTL_MAXSIZE ];
        } InData;
        memset (&InData, 0x00, sizeof(InData));
        USHORT i=0, j=0, len=lstrlen(pszName);
        
        if ( len == 0 ) {
            Message (MB_ICONHAND, GetErrorTitle(), IDS_FAILCONFIG_SYSNAME, TEXT("A sysname must be specified"));
            return(-1);
        }

        while ( pszName[i] ) {
            if ( !isspace(pszName[i]) ) {
                InData.szData[j++] = pszName[i];
            } else if (InData.szData[j-1] != '\0') {
                InData.szData[j++] = '\0';
                InData.cbData++;
            }
            i++;
        }
        InData.szData[j++] = '\0';
        InData.cbData++;      /* one word */

        BYTE OutData[ PIOCTL_MAXSIZE ];
        memset (OutData, 0x00, sizeof(OutData));

        struct ViceIoctl IOInfo;
        IOInfo.in_size = sizeof(ULONG) + j;
        IOInfo.in = (char *)&InData;
        IOInfo.out = (char *)OutData;
        IOInfo.out_size = PIOCTL_MAXSIZE;

        if ((status = pioctl (0, VIOC_AFS_SYSNAME, &IOInfo, 1)) != 0)
        {
            rc = FALSE;
        }
    }

    if (rc)
    {
        Config_WriteGlobalString (TEXT("SysName"), pszName);
    }

    if (pStatus && !rc)
        *pStatus = status;
    if (!rc)
        Message (MB_ICONHAND, GetErrorTitle(), IDS_FAILCONFIG_SYSNAME, TEXT("%ld"), status);
    return rc;
}



void Config_GetRootVolume (LPTSTR pszName)
{
    if (!Config_ReadGlobalString (TEXT("RootVolume"), pszName, MAX_PATH))
        lstrcpy (pszName, TEXT("root.afs"));
}


BOOL Config_SetRootVolume (LPCTSTR pszName, ULONG *pStatus)
{
    Config_WriteGlobalString (TEXT("RootVolume"), pszName);
    g.fNeedRestart = TRUE;
    return TRUE;
}



void Config_GetMountRoot (LPTSTR pszPath)
{
    if (!Config_ReadGlobalString (TEXT("MountRoot"), pszPath, MAX_PATH))
        lstrcpy (pszPath, TEXT("/afs"));
}


BOOL Config_SetMountRoot (LPCTSTR pszPath, ULONG *pStatus)
{
    Config_WriteGlobalString (TEXT("MountRoot"), pszPath);
    g.fNeedRestart = TRUE;
    return TRUE;
}


BOOL Config_GetCacheInUse (ULONG *pckCacheInUse, ULONG *pStatus)
{
    BOOL rc = TRUE;
    ULONG status = 0;

    *pckCacheInUse = 0;

    if (Config_GetServiceState() != SERVICE_RUNNING)
    {
        rc = FALSE;
        status = ERROR_SERVICE_NOT_ACTIVE;
    }
    else
    {
        BYTE OutData[ PIOCTL_MAXSIZE ];
        memset (OutData, 0x00, sizeof(OutData));

        struct ViceIoctl IOInfo;
        IOInfo.in_size = 0;
        IOInfo.in = (char *)0;
        IOInfo.out = (char *)OutData;
        IOInfo.out_size = PIOCTL_MAXSIZE;

        if ((status = pioctl (0, VIOCGETCACHEPARMS, &IOInfo, 1)) != 0)
        {
            rc = FALSE;
        }
        else
        {
            *pckCacheInUse = ((LONG*)OutData)[1];
        }
    }

    if (pStatus && !rc)
        *pStatus = status;
    return rc;
}

void Config_GetCachePath (LPTSTR pszCachePath)
{
    if (!Config_ReadGlobalString (TEXT("CachePath"), pszCachePath, MAX_PATH)) {
        TCHAR szPath[MAX_PATH];
        GetWindowsDirectory(szPath, sizeof(szPath));
        szPath[2] = 0;	/* get drive letter only */
        strcat(szPath, "\\AFSCache");

        lstrcpy (pszCachePath, szPath);
    }
}        

BOOL Config_SetCachePath(LPCTSTR pszPath, ULONG *pStatus)
{
    Config_WriteGlobalString (TEXT("CachePath"), pszPath);
    g.fNeedRestart = TRUE;
    return TRUE;
}

void Config_GetLanAdapter (ULONG *pnLanAdapter)
{
    if (!Config_ReadGlobalNum (TEXT("LANadapter"), (DWORD*)pnLanAdapter))
        *pnLanAdapter = -1;
}

BOOL Config_SetLanAdapter (ULONG nLanAdapter, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("LANadapter"), nLanAdapter);
    g.fNeedRestart = TRUE;
    return TRUE;
}

void Config_GetTrapOnPanic (BOOL *pfFlag)
{
    if (!Config_ReadGlobalNum (TEXT("TrapOnPanic"), (DWORD*)pfFlag))
        *pfFlag = TRUE;
}

BOOL Config_SetTrapOnPanic (BOOL fFlag, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("TrapOnPanic"), fFlag);
    g.fNeedRestart = TRUE;
    return TRUE;
}

void Config_GetTraceBufferSize (ULONG *pnBufSize)
{
    if (!Config_ReadGlobalNum (TEXT("TraceBufferSize"), (DWORD*)pnBufSize))
        *pnBufSize = 5000;
}

BOOL Config_SetTraceBufferSize (ULONG nBufSize, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("TraceBufferSize"), nBufSize);
    g.fNeedRestart = TRUE;
    return TRUE;
}

void Config_GetLoginRetryInterval (ULONG *pnInterval)
{
    if (!Config_ReadGlobalNum (TEXT("LoginRetryInterval"), (DWORD*)pnInterval))
        *pnInterval = 30;
}

BOOL Config_SetLoginRetryInterval (ULONG nInterval, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("LoginRetryInterval"), nInterval);
    return TRUE;
}

void Config_GetFailLoginsSilently (BOOL *pfFlag)
{
    if (!Config_ReadGlobalNum (TEXT("FailLoginsSilently"), (DWORD*)pfFlag))
        *pfFlag = FALSE;
}

BOOL Config_SetFailLoginsSilently (BOOL fFlag, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("FailLoginsSilently"), fFlag);
    return TRUE;
}

void Config_GetReportSessionStartups (BOOL *pfFlag)
{
    if (!Config_ReadGlobalNum (TEXT("ReportSessionStartups"), (DWORD*)pfFlag))
        *pfFlag = FALSE;
}

BOOL Config_SetReportSessionStartups (BOOL fFlag, ULONG *pStatus)
{
    Config_WriteGlobalNum (TEXT("ReportSessionStartups"), fFlag);
    return TRUE;
}

