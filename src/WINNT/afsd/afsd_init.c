/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/afs_args.h>

#include <windows.h>
#include <string.h>
#include <nb30.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <mbctype.h>
#include <winsock2.h>
#include <ErrorRep.h>

#include <osi.h>
#include "afsd.h"
#ifdef USE_BPLUS
#include "cm_btree.h"
#endif
#include <rx\rx.h>
#include <rx\rx_null.h>
#include <rx\rxstat.h>
#include <WINNT/syscfg.h>
#include <WINNT/afsreg.h>
#include <afs\afscbint.h>

#include "smb.h"
#include "cm_rpc.h"
#include "lanahelper.h"
#include <strsafe.h>
#include "cm_memmap.h"
#include "msrpc.h"
#ifdef DEBUG
#include <crtdbg.h>
#endif

extern afs_uint32 cryptall;
extern afs_uint32 cm_anonvldb;
extern int cm_enableServerLocks;
extern int cm_followBackupPath;
extern int cm_deleteReadOnly;
#ifdef USE_BPLUS
extern afs_int32 cm_BPlusTrees;
#endif
extern afs_int32 cm_OfflineROIsValid;
extern afs_int32 cm_giveUpAllCBs;
extern const clientchar_t **smb_ExecutableExtensions;

osi_log_t *afsd_logp;

cm_config_data_t        cm_data;

fschar_t cm_rootVolumeName[VL_MAXNAMELEN];
DWORD cm_rootVolumeNameLen;

fschar_t cm_mountRoot[1024];
DWORD cm_mountRootLen;

clientchar_t cm_mountRootC[1024];
DWORD cm_mountRootCLen;

int cm_readonlyVolumeVersioning = 0;
int cm_logChunkSize;
int cm_chunkSize;

int smb_UseV3 = 1;

int LANadapter;

int numBkgD;
int numSvThreads;
long rx_mtu = -1;
int traceOnPanic = 0;

int logReady = 0;

char cm_HostName[200];
long cm_HostAddr;
unsigned short cm_callbackport = CM_DEFAULT_CALLBACKPORT;

char cm_NetbiosName[MAX_NB_NAME_LENGTH] = "NOT.YET.SET";
clientchar_t cm_NetbiosNameC[MAX_NB_NAME_LENGTH] = _C("NOT.YET.SET");

char cm_CachePath[MAX_PATH];
DWORD cm_ValidateCache = 1;

BOOL reportSessionStartups = FALSE;

cm_initparams_v1 cm_initParams;

clientchar_t *cm_sysName = 0;
unsigned int  cm_sysNameCount = 0;
clientchar_t *cm_sysNameList[MAXNUMSYSNAMES];

DWORD TraceOption = 0;

/*
 * AFSD Initialization Log
 *
 * This is distinct from the regular debug logging facility.
 * Log items go directly to a file, not to an array in memory, so that even
 * if AFSD crashes, the log can be inspected.
 */

HANDLE afsi_file;

int cm_dnsEnabled = 1;


static int afsi_log_useTimestamp = 1;

void
afsi_log(char *pattern, ...)
{
    char s[256], t[100], d[100], u[512];
    DWORD zilch;
    va_list ap;
    va_start(ap, pattern);

    StringCbVPrintfA(s, sizeof(s), pattern, ap);
    if ( afsi_log_useTimestamp ) {
        GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));
        GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, d, sizeof(d));
        StringCbPrintfA(u, sizeof(u), "%s %s: %s\r\n", d, t, s);
        if (afsi_file != INVALID_HANDLE_VALUE)
            WriteFile(afsi_file, u, (DWORD)strlen(u), &zilch, NULL);
#ifdef NOTSERVICE
        printf("%s", u);
#endif
    } else {
        if (afsi_file != INVALID_HANDLE_VALUE)
            WriteFile(afsi_file, s, (DWORD)strlen(s), &zilch, NULL);
    }
}

void
afsi_start()
{
    char wd[MAX_PATH+1];
    char t[100], u[100], *p, *path;
    int zilch;
    DWORD code;
    DWORD dwLow, dwHigh;
    HKEY parmKey;
    DWORD dummyLen;
    DWORD maxLogSize = 100 * 1024;

    afsi_file = INVALID_HANDLE_VALUE;
    code = GetTempPath(sizeof(wd)-15, wd);
    if ( code == 0 || code > (sizeof(wd)-15) )
        return;         /* unable to create a log */

    StringCbCatA(wd, sizeof(wd), "\\afsd_init.log");
    GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));
    afsi_file = CreateFile(wd, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(maxLogSize);
        code = RegQueryValueEx(parmKey, "MaxLogSize", NULL, NULL,
                                (BYTE *) &maxLogSize, &dummyLen);
        RegCloseKey (parmKey);
    }

    if (maxLogSize) {
        dwLow = GetFileSize( afsi_file, &dwHigh );
        if ( dwHigh > 0 || dwLow >= maxLogSize ) {
            CloseHandle(afsi_file);
            afsi_file = CreateFile( wd, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                    CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
        }
    }

    SetFilePointer(afsi_file, 0, NULL, FILE_END);
    GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, u, sizeof(u));
    StringCbCatA(t, sizeof(t), ": Create log file\r\n");
    StringCbCatA(u, sizeof(u), ": Created log file\r\n");
    WriteFile(afsi_file, t, (DWORD)strlen(t), &zilch, NULL);
    WriteFile(afsi_file, u, (DWORD)strlen(u), &zilch, NULL);
    p = "PATH=";
    code = GetEnvironmentVariable("PATH", NULL, 0);
    path = malloc(code);
    code = GetEnvironmentVariable("PATH", path, code);
    WriteFile(afsi_file, p, (DWORD)strlen(p), &zilch, NULL);
    WriteFile(afsi_file, path, (DWORD)strlen(path), &zilch, NULL);
    WriteFile(afsi_file, "\r\n", (DWORD)1, &zilch, NULL);
    free(path);

    /* Initialize C RTL Code Page conversion functions */
    /* All of the path info obtained from the SMB client is in the OEM code page */
    afsi_log("OEM Code Page = %d", GetOEMCP());
    afsi_log("locale =  %s", setlocale(LC_ALL,NULL));
#ifdef COMMENT
    /* Two things to look into.  First, should mbstowcs() be performing
     * character set translations from OEM to Unicode in smb3.c;
     * Second, do we need to set this translation in each function
     * due to multi-threading.
     */
    afsi_log("locale -> %s", setlocale(LC_ALL, ".OCP"));
    afsi_log("_setmbcp = %d -> %d", _setmbcp(_MB_CP_OEM), _getmbcp());
#endif /* COMMENT */
}

/*
 * Standard AFSD trace
 */

void afsd_ForceTrace(BOOL flush)
{
    HANDLE handle;
    int len;
    char buf[256];

    if (!logReady)
        return;

    len = GetTempPath(sizeof(buf)-10, buf);
    StringCbCopyA(&buf[len], sizeof(buf)-len, "/afsd.log");
    handle = CreateFile(buf, GENERIC_WRITE, FILE_SHARE_READ,
                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        logReady = 0;
        osi_panic("Cannot create log file", __FILE__, __LINE__);
    }
    osi_LogPrint(afsd_logp, handle);
    if (flush)
        FlushFileBuffers(handle);
    CloseHandle(handle);
}

static void afsd_InitServerPreferences(void)
{
    HKEY hkPrefs = 0;
    DWORD dwType, dwSize;
    DWORD dwPrefs = 0;
    DWORD dwIndex;
    TCHAR szHost[256];
    DWORD dwHostSize = 256;
    DWORD dwRank;
    struct sockaddr_in	saddr;
    cm_server_t       *tsp;

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Server Preferences\\VLDB",
                      0,
                      KEY_READ|KEY_QUERY_VALUE,
                      &hkPrefs) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkPrefs,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwPrefs, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0 ; dwIndex < dwPrefs; dwIndex++ ) {

            dwSize = sizeof(DWORD);
            dwHostSize = 256;

            if (RegEnumValue( hkPrefs, dwIndex, szHost, &dwHostSize, NULL,
                              &dwType, (LPBYTE)&dwRank, &dwSize))
            {
                afsi_log("RegEnumValue(hkPrefs) failed");
                continue;
            }

            afsi_log("VLDB Server Preference: %s = %d",szHost, dwRank);

            if (isdigit(szHost[0]))
            {
                if ((saddr.sin_addr.S_un.S_addr = inet_addr (szHost)) == INADDR_NONE)
                    continue;
            } else {
                HOSTENT *pEntry;
                if ((pEntry = gethostbyname (szHost)) == NULL)
                    continue;

                saddr.sin_addr.S_un.S_addr = *(unsigned long *)pEntry->h_addr;
            }
            saddr.sin_port = htons(7003);
            saddr.sin_family = AF_INET;
            dwRank += (rand() & 0x000f);

            tsp = cm_FindServer(&saddr, CM_SERVER_VLDB, FALSE);
            if ( tsp )		/* an existing server - ref count increased */
            {
                lock_ObtainMutex(&tsp->mx);
                tsp->ipRank = (USHORT)dwRank;
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
		tsp->adminRank = tsp->ipRank;
                lock_ReleaseMutex(&tsp->mx);

                /* set preferences for an existing vlserver */
                cm_ChangeRankCellVLServer(tsp);
                cm_PutServer(tsp);  /* decrease refcount */
            }
            else	/* add a new server without a cell */
            {
                tsp = cm_NewServer(&saddr, CM_SERVER_VLDB, NULL, NULL, CM_FLAG_NOPROBE); /* refcount = 1 */
                lock_ObtainMutex(&tsp->mx);
                tsp->ipRank = (USHORT)dwRank;
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
		tsp->adminRank = tsp->ipRank;
                lock_ReleaseMutex(&tsp->mx);
            }
        }

        RegCloseKey(hkPrefs);
    }

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Server Preferences\\File",
                      0,
                      KEY_READ|KEY_QUERY_VALUE,
                      &hkPrefs) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkPrefs,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwPrefs, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0 ; dwIndex < dwPrefs; dwIndex++ ) {

            dwSize = sizeof(DWORD);
            dwHostSize = 256;

            if (RegEnumValue( hkPrefs, dwIndex, szHost, &dwHostSize, NULL,
                              &dwType, (LPBYTE)&dwRank, &dwSize))
            {
                afsi_log("RegEnumValue(hkPrefs) failed");
                continue;
            }

            afsi_log("File Server Preference: %s = %d",szHost, dwRank);

            if (isdigit(szHost[0]))
            {
                if ((saddr.sin_addr.S_un.S_addr = inet_addr (szHost)) == INADDR_NONE)
                    continue;
            } else {
                HOSTENT *pEntry;
                if ((pEntry = gethostbyname (szHost)) == NULL)
                    continue;

                saddr.sin_addr.S_un.S_addr = *(unsigned long *)pEntry->h_addr;
            }
            saddr.sin_port = htons(7000);
            saddr.sin_family = AF_INET;
            dwRank += (rand() & 0x000f);

            tsp = cm_FindServer(&saddr, CM_SERVER_FILE, FALSE);
            if ( tsp )		/* an existing server - ref count increased */
            {
                lock_ObtainMutex(&tsp->mx);
                tsp->ipRank = (USHORT)dwRank;
		_InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
		tsp->adminRank = tsp->ipRank;
                lock_ReleaseMutex(&tsp->mx);

                /* find volumes which might have RO copy
                /* on server and change the ordering of
                 * their RO list
                 */
                cm_ChangeRankVolume(tsp);
                cm_PutServer(tsp);  /* decrease refcount */
            }
            else	/* add a new server without a cell */
            {
                tsp = cm_NewServer(&saddr, CM_SERVER_FILE, NULL, NULL, CM_FLAG_NOPROBE); /* refcount = 1 */
                lock_ObtainMutex(&tsp->mx);
                tsp->ipRank = (USHORT)dwRank;
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
		tsp->adminRank = tsp->ipRank;
                lock_ReleaseMutex(&tsp->mx);
            }
        }

        RegCloseKey(hkPrefs);
    }
}


#ifndef _WIN64
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

static BOOL
is_wow64(void)
{
    static BOOL bChecked = FALSE;
    static BOOL bIsWow64 = FALSE;

    if (!bChecked)
    {
        HANDLE h1 = NULL;
        LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;

        h1 = GetModuleHandle("kernel32.dll");
        fnIsWow64Process =
            (LPFN_ISWOW64PROCESS)GetProcAddress(h1, "IsWow64Process");

        /* If we don't find the fnIsWow64Process function then we
         * are not running in a broken Wow64
         */
        if (fnIsWow64Process)
            fnIsWow64Process(GetCurrentProcess(), &bIsWow64);

        bChecked = TRUE;
    }

    return bIsWow64;
}
#endif /* _WIN64 */

/*
 * AFSD Initialization
 */

static int
afsd_InitRoot(char **reasonP)
{
    long code;
    cm_req_t req;

    cm_InitReq(&req);

    if (cm_freelanceEnabled) {
        cm_FakeRootFid(&cm_data.rootFid);
    } else {
	int attempts = 10;

        osi_Log0(afsd_logp, "Loading Root Volume from cell");
	do {
	    code = cm_FindVolumeByName(cm_data.rootCellp, cm_rootVolumeName, cm_rootUserp,
				       &req, CM_GETVOL_FLAG_CREATE, &cm_data.rootVolumep);
	    afsi_log("cm_FindVolumeByName code %x root vol %x", code,
		      (code ? (cm_volume_t *)-1 : cm_data.rootVolumep));
	} while (code && --attempts);
        if (code != 0) {
            *reasonP = "can't find root volume in root cell";
            return -1;
        }

        /* compute the root fid */
        cm_SetFid(&cm_data.rootFid, cm_data.rootCellp->cellID, cm_GetROVolumeID(cm_data.rootVolumep), 1, 1);
    }

    code = cm_GetSCache(&cm_data.rootFid, &cm_data.rootSCachep, cm_rootUserp, &req);
    afsi_log("cm_GetSCache code %x scache %x", code,
             (code ? (cm_scache_t *)-1 : cm_data.rootSCachep));
    if (code != 0) {
        *reasonP = "unknown error";
        return -1;
    }

    return 0;
}

int
afsd_InitCM(char **reasonP)
{
    osi_uid_t debugID;
    afs_uint64 cacheBlocks;
    DWORD cacheSize;
    DWORD blockSize;
    long logChunkSize;
    DWORD stats;
    DWORD volumes;
    DWORD cells;
    DWORD dwValue;
    DWORD rx_enable_peer_stats;
    DWORD rx_enable_process_stats;
    DWORD rx_udpbufsize = -1;
    DWORD lockOrderValidation;
    long traceBufSize;
    long maxcpus;
    long ltt, ltto;
    long rx_nojumbo;
    int  rx_max_rwin_size;
    int  rx_max_swin_size;
    int  rx_min_peer_timeout;
    long virtualCache = 0;
    fschar_t rootCellName[256];
    struct rx_service *serverp;
    static struct rx_securityClass *nullServerSecurityClassp;
    struct hostent *thp;
    char *msgBuf;
    char buf[1024];
    HKEY parmKey;
    DWORD dummyLen;
    DWORD regType;
    long code;
    /*int freelanceEnabled;*/
    WSADATA WSAjunk;
    int i;
    int cm_noIPAddr;         /* number of client network interfaces */
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
    DWORD dwPriority;

    WSAStartup(0x0101, &WSAjunk);

    init_et_to_sys_error();

    cm_utilsInit();

    /* setup osidebug server at RPC slot 1000 */
    osi_LongToUID(1000, &debugID);
    code = osi_InitDebug(&debugID);
    afsi_log("osi_InitDebug code %d", code);

#ifndef _WIN64
    if (is_wow64())
    {
        *reasonP = "32-bit OpenAFS Service is incompatible with the WOW64 environment";
        return -1;
    }
#endif

    //	osi_LockTypeSetDefault("stat");	/* comment this out for speed */
    if (code != 0) {
        if (code == RPC_S_NO_PROTSEQS)
            *reasonP = "No RPC Protocol Sequences registered.  Check HKLM\\SOFTWARE\\Microsoft\\RPC\\ClientProtocols";
        else
            *reasonP = "unknown error";
        return -1;
    }

    /* who are we ? */
    gethostname(cm_HostName, sizeof(cm_HostName));
    afsi_log("gethostname %s", cm_HostName);
    thp = gethostbyname(cm_HostName);
    memcpy(&cm_HostAddr, thp->h_addr_list[0], 4);

    /* seed random number generator */
    srand(ntohl(cm_HostAddr));

    /* Look up configuration parameters in Registry */
    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                        0, KEY_QUERY_VALUE, &parmKey);
    if (code != ERROR_SUCCESS) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                       NULL, code, 0, (LPTSTR)&msgBuf, 0, NULL);
        StringCbPrintfA(buf, sizeof(buf),
                         "Failure in configuration while opening Registry: %s",
                         msgBuf);
        osi_panic(buf, __FILE__, __LINE__);
    }

    dummyLen = sizeof(dwPriority);
    code = RegQueryValueEx(parmKey, "PriorityClass", NULL, NULL,
                            (BYTE *) &dwPriority, &dummyLen);
    if (code != ERROR_SUCCESS || dwPriority == 0) {
        dwPriority = HIGH_PRIORITY_CLASS;
    }
    if (dwPriority != GetPriorityClass(GetCurrentProcess()))
        SetPriorityClass(GetCurrentProcess(), dwPriority);
    afsi_log("PriorityClass 0x%x", GetPriorityClass(GetCurrentProcess()));

    dummyLen = sizeof(lockOrderValidation);
    code = RegQueryValueEx(parmKey, "LockOrderValidation", NULL, NULL,
                            (BYTE *) &lockOrderValidation, &dummyLen);
    if (code != ERROR_SUCCESS) {
#ifdef DEBUG
        lockOrderValidation = 1;
#else
        lockOrderValidation = 0;
#endif
    }
    osi_SetLockOrderValidation(lockOrderValidation);
    afsi_log("Lock Order Validation %s", lockOrderValidation ? "On" : "Off");

    dummyLen = sizeof(maxcpus);
    code = RegQueryValueEx(parmKey, "MaxCPUs", NULL, NULL,
                            (BYTE *) &maxcpus, &dummyLen);
    if (code != ERROR_SUCCESS) {
        maxcpus = 2;
    }

    {
        HANDLE hProcess;
        DWORD_PTR processAffinityMask, systemAffinityMask;

        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_SET_INFORMATION,
                               FALSE, GetCurrentProcessId());
        if ( hProcess != NULL &&
             GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask) )
        {
            int i, n, bits, cpu_count = 0;
            DWORD_PTR mask, newAffinityMask;

#if defined(_WIN64)
            bits = 64;
#else
            bits = 32;
#endif
            for ( i=0, n=0, mask=1, newAffinityMask=0; i<bits; i++ ) {
                if ( processAffinityMask & mask ) {
                    cpu_count++;
                    if (n<maxcpus) {
                        newAffinityMask |= mask;
                        n++;
                    }
                }
                mask *= 2;
            }

            if (maxcpus == 0) {
                afsi_log("No CPU Restrictions; %d cpu(s) available", cpu_count);
            } else {
                SetProcessAffinityMask(hProcess, newAffinityMask);
            }
            CloseHandle(hProcess);
            afsi_log("CPU Restrictions set to %d cpu(s); %d cpu(s) available", maxcpus, cpu_count);
        } else {
            afsi_log("CPU Restrictions requested %d cpu(s); unable to access process information", maxcpus);
        }
    }

    dummyLen = sizeof(TraceOption);
    code = RegQueryValueEx(parmKey, "TraceOption", NULL, NULL,
                            (BYTE *) &TraceOption, &dummyLen);
    afsi_log("Trace Options = %lX", TraceOption);

    dummyLen = sizeof(traceBufSize);
    code = RegQueryValueEx(parmKey, "TraceBufferSize", NULL, NULL,
                            (BYTE *) &traceBufSize, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Trace Buffer size %d", traceBufSize);
    else {
        traceBufSize = CM_CONFIGDEFAULT_TRACEBUFSIZE;
        afsi_log("Default trace buffer size %d", traceBufSize);
    }

    /* setup and enable debug log */
    afsd_logp = osi_LogCreate("afsd", traceBufSize);
    afsi_log("osi_LogCreate log addr %x", PtrToUlong(afsd_logp));
    if ((TraceOption & 0x8)
#ifdef DEBUG
	 || 1
#endif
	 ) {
	osi_LogEnable(afsd_logp);
    }
    logReady = 1;

    osi_Log0(afsd_logp, "Log init");

    dummyLen = sizeof(smb_monitorReqs);
    code = RegQueryValueEx(parmKey, "SMBRequestMonitor", NULL, NULL,
                           (BYTE *) &smb_monitorReqs, &dummyLen);
    afsi_log("SMB request monitoring is %s", (smb_monitorReqs != 0)? "enabled": "disabled");

    dummyLen = sizeof(cacheSize);
    code = RegQueryValueEx(parmKey, "CacheSize", NULL, NULL,
                            (BYTE *) &cacheSize, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Cache size %d", cacheSize);
    else {
        cacheSize = CM_CONFIGDEFAULT_CACHESIZE;
        afsi_log("Default cache size %d", cacheSize);
    }

    dummyLen = sizeof(logChunkSize);
    code = RegQueryValueEx(parmKey, "ChunkSize", NULL, NULL,
                            (BYTE *) &logChunkSize, &dummyLen);
    if (code == ERROR_SUCCESS) {
        if (logChunkSize < 12 || logChunkSize > 30) {
            afsi_log("Invalid chunk size %d, using default",
                      logChunkSize);
            logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
        }
    } else {
        logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
    }
    cm_logChunkSize = logChunkSize;
    cm_chunkSize = 1 << logChunkSize;
    afsi_log("Chunk size %u (%d)", cm_chunkSize, cm_logChunkSize);

    dummyLen = sizeof(blockSize);
    code = RegQueryValueEx(parmKey, "blockSize", NULL, NULL,
                            (BYTE *) &blockSize, &dummyLen);
    if (code == ERROR_SUCCESS) {
        if (blockSize < 1 ||
            (blockSize > 1024 && (blockSize % CM_CONFIGDEFAULT_BLOCKSIZE != 0)))
        {
            afsi_log("Invalid block size %u specified, using default", blockSize);
            blockSize = CM_CONFIGDEFAULT_BLOCKSIZE;
        } else {
            /*
             * if the blockSize is less than 1024 we permit the blockSize to be
             * specified in multiples of the default blocksize
             */
            if (blockSize <= 1024)
                blockSize *= CM_CONFIGDEFAULT_BLOCKSIZE;
        }
    } else {
        blockSize = CM_CONFIGDEFAULT_BLOCKSIZE;
    }
    if (blockSize > cm_chunkSize) {
        afsi_log("Block size (%d) cannot be larger than Chunk size (%d).",
                  blockSize, cm_chunkSize);
        blockSize = cm_chunkSize;
    }
    if (cm_chunkSize % blockSize != 0) {
        afsi_log("Block size (%d) must be a factor of Chunk size (%d).",
                  blockSize, cm_chunkSize);
        blockSize = CM_CONFIGDEFAULT_BLOCKSIZE;
    }
    afsi_log("Block size %u", blockSize);

    dummyLen = sizeof(numBkgD);
    code = RegQueryValueEx(parmKey, "Daemons", NULL, NULL,
                            (BYTE *) &numBkgD, &dummyLen);
    if (code == ERROR_SUCCESS) {
        if (numBkgD > CM_MAX_DAEMONS)
            numBkgD = CM_MAX_DAEMONS;
        afsi_log("%d background daemons", numBkgD);
    } else {
        numBkgD = CM_CONFIGDEFAULT_DAEMONS;
        afsi_log("Defaulting to %d background daemons", numBkgD);
    }

    dummyLen = sizeof(numSvThreads);
    code = RegQueryValueEx(parmKey, "ServerThreads", NULL, NULL,
                            (BYTE *) &numSvThreads, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("%d server threads", numSvThreads);
    else {
        numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;
        afsi_log("Defaulting to %d server threads", numSvThreads);
    }

    dummyLen = sizeof(stats);
    code = RegQueryValueEx(parmKey, "Stats", NULL, NULL,
                            (BYTE *) &stats, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Status cache entries: %d", stats);
    else {
        stats = CM_CONFIGDEFAULT_STATS;
        afsi_log("Default status cache entries: %d", stats);
    }

    dummyLen = sizeof(volumes);
    code = RegQueryValueEx(parmKey, "Volumes", NULL, NULL,
                            (BYTE *) &volumes, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Volumes cache entries: %d", volumes);
    else {
        volumes = CM_CONFIGDEFAULT_STATS / 3;
        afsi_log("Default volume cache entries: %d", volumes);
    }

    dummyLen = sizeof(cells);
    code = RegQueryValueEx(parmKey, "Cells", NULL, NULL,
                            (BYTE *) &cells, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Cell cache entries: %d", cells);
    else {
        cells = CM_CONFIGDEFAULT_CELLS;
        afsi_log("Default cell cache entries: %d", cells);
    }

    dummyLen = sizeof(ltt);
    code = RegQueryValueEx(parmKey, "LogoffTokenTransfer", NULL, NULL,
                            (BYTE *) &ltt, &dummyLen);
    if (code != ERROR_SUCCESS)
        ltt = 1;
    smb_LogoffTokenTransfer = ltt;
    afsi_log("Logoff token transfer %s",  (ltt ? "on" : "off"));

    if (ltt) {
        dummyLen = sizeof(ltto);
        code = RegQueryValueEx(parmKey, "LogoffTokenTransferTimeout",
                                NULL, NULL, (BYTE *) &ltto, &dummyLen);
        if (code != ERROR_SUCCESS)
            ltto = 120;
    } else {
        ltto = 0;
    }
    smb_LogoffTransferTimeout = ltto;
    afsi_log("Logoff token transfer timeout %d seconds", ltto);

    dummyLen = sizeof(cm_rootVolumeName);
    code = RegQueryValueEx(parmKey, "RootVolume", NULL, NULL,
                            (LPBYTE) cm_rootVolumeName, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Root volume %s", cm_rootVolumeName);
    else {
        cm_FsStrCpy(cm_rootVolumeName, lengthof(cm_rootVolumeName), "root.afs");
        afsi_log("Default root volume name root.afs");
    }

    cm_mountRootCLen = sizeof(cm_mountRootC);
    code = RegQueryValueExW(parmKey, L"MountRoot", NULL, NULL,
                            (LPBYTE) cm_mountRootC, &cm_mountRootCLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("Mount root %S", cm_mountRootC);
        cm_mountRootCLen = (DWORD)cm_ClientStrLen(cm_mountRootC);
    } else {
        cm_ClientStrCpy(cm_mountRootC, lengthof(cm_mountRootC), _C("/afs"));
        cm_mountRootCLen = (DWORD)cm_ClientStrLen(cm_mountRootC);
        /* Don't log */
    }

    cm_ClientStringToFsString(cm_mountRootC, -1, cm_mountRoot, lengthof(cm_mountRoot));
    cm_mountRootLen = (DWORD)cm_FsStrLen(cm_mountRoot);

    dummyLen = sizeof(buf);
    code = RegQueryValueEx(parmKey, "CachePath", NULL, &regType,
                           buf, &dummyLen);
    if (code == ERROR_SUCCESS && buf[0]) {
        if (regType == REG_EXPAND_SZ) {
            dummyLen = ExpandEnvironmentStrings(buf, cm_CachePath, sizeof(cm_CachePath));
            if (dummyLen > sizeof(cm_CachePath)) {
                afsi_log("Cache path [%s] longer than %d after expanding env strings", buf, sizeof(cm_CachePath));
                osi_panic("CachePath too long", __FILE__, __LINE__);
            }
        } else {
            StringCbCopyA(cm_CachePath, sizeof(cm_CachePath), buf);
        }
        afsi_log("Cache path %s", cm_CachePath);
    } else {
        dummyLen = ExpandEnvironmentStrings("%TEMP%\\AFSCache", cm_CachePath, sizeof(cm_CachePath));
        if (dummyLen > sizeof(cm_CachePath)) {
            afsi_log("Cache path [%%TEMP%%\\AFSCache] longer than %d after expanding env strings",
                     sizeof(cm_CachePath));
            osi_panic("CachePath too long", __FILE__, __LINE__);
        }
        afsi_log("Default cache path %s", cm_CachePath);
    }

    dummyLen = sizeof(virtualCache);
    code = RegQueryValueEx(parmKey, "NonPersistentCaching", NULL, NULL,
                            (LPBYTE)&virtualCache, &dummyLen);
    afsi_log("Cache type is %s", (virtualCache?"VIRTUAL":"FILE"));

    if (!virtualCache) {
        dummyLen = sizeof(cm_ValidateCache);
        code = RegQueryValueEx(parmKey, "ValidateCache", NULL, NULL,
                               (LPBYTE)&cm_ValidateCache, &dummyLen);
        if ( cm_ValidateCache < 0 || cm_ValidateCache > 2 )
            cm_ValidateCache = 1;
        switch (cm_ValidateCache) {
        case 0:
            afsi_log("Cache Validation disabled");
            break;
        case 1:
            afsi_log("Cache Validation on Startup");
            break;
        case 2:
            afsi_log("Cache Validation on Startup and Shutdown");
            break;
        }
    }

    dummyLen = sizeof(traceOnPanic);
    code = RegQueryValueEx(parmKey, "TrapOnPanic", NULL, NULL,
                            (BYTE *) &traceOnPanic, &dummyLen);
    if (code != ERROR_SUCCESS)
        traceOnPanic = 1;              /* log */
    afsi_log("Set to %s on panic", traceOnPanic ? "trap" : "not trap");

    dummyLen = sizeof(reportSessionStartups);
    code = RegQueryValueEx(parmKey, "ReportSessionStartups", NULL, NULL,
                            (BYTE *) &reportSessionStartups, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Session startups %s be recorded in the Event Log",
                  reportSessionStartups ? "will" : "will not");
    else {
        reportSessionStartups = 0;
        /* Don't log */
    }

    for ( i=0; i < MAXNUMSYSNAMES; i++ ) {
        cm_sysNameList[i] = osi_Alloc(MAXSYSNAME * sizeof(clientchar_t));
        cm_sysNameList[i][0] = '\0';
    }
    cm_sysName = cm_sysNameList[0];

    {
        clientchar_t *p, *q;
        clientchar_t * cbuf = (clientchar_t *) buf;
        dummyLen = sizeof(buf);
        code = RegQueryValueExW(parmKey, L"SysName", NULL, NULL, (LPBYTE) cbuf, &dummyLen);
        if (code != ERROR_SUCCESS || !cbuf[0]) {
#if defined(_IA64_)
            cm_ClientStrCpy(cbuf, lengthof(buf), _C("ia64_win64"));
#elif defined(_AMD64_)
            cm_ClientStrCpy(cbuf, lengthof(buf), _C("amd64_win64 x86_win32 i386_w2k"));
#else /* assume x86 32-bit */
            cm_ClientStrCpy(cbuf, lengthof(buf), _C("x86_win32 i386_w2k i386_nt40"));
#endif
        }
        afsi_log("Sys name %S", cbuf);

        /* breakup buf into individual search string entries */
        for (p = q = cbuf; p < cbuf + dummyLen; p++) {
            if (*p == '\0' || iswspace(*p)) {
                memcpy(cm_sysNameList[cm_sysNameCount],q,(p-q) * sizeof(clientchar_t));
                cm_sysNameList[cm_sysNameCount][p-q] = '\0';
                cm_sysNameCount++;
                do {
                    if (*p == '\0')
                        goto done_sysname;
                    p++;
                } while (*p == '\0' || isspace(*p));
                q = p;
                p--;
            }
        }
    }
  done_sysname:
    cm_ClientStrCpy(cm_sysName, MAXSYSNAME, cm_sysNameList[0]);

    dummyLen = sizeof(cryptall);
    code = RegQueryValueEx(parmKey, "SecurityLevel", NULL, NULL,
                           (BYTE *) &cryptall, &dummyLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("SecurityLevel is %s", cryptall == 1?"crypt": cryptall == 2?"auth":"clear");
    } else {
        cryptall = 0;
        afsi_log("Default SecurityLevel is clear");
    }

    if (cryptall == 1)
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_ON);
    else if (cryptall == 2)
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_AUTH);
    else
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_OFF);

    dummyLen = sizeof(cryptall);
    code = RegQueryValueEx(parmKey, "ForceAnonVLDB", NULL, NULL,
                            (BYTE *) &cm_anonvldb, &dummyLen);
    afsi_log("CM ForceAnonVLDB is %s", cm_anonvldb ? "on" : "off");

    dummyLen = sizeof(cm_dnsEnabled);
    code = RegQueryValueEx(parmKey, "UseDNS", NULL, NULL,
                            (BYTE *) &cm_dnsEnabled, &dummyLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("DNS %s be used to find AFS cell servers",
                  cm_dnsEnabled ? "will" : "will not");
    }
    else {
        cm_dnsEnabled = 1;   /* default on */
        afsi_log("Default to use DNS to find AFS cell servers");
    }

#ifdef AFS_FREELANCE_CLIENT
    dummyLen = sizeof(cm_freelanceEnabled);
    code = RegQueryValueEx(parmKey, "FreelanceClient", NULL, NULL,
                            (BYTE *) &cm_freelanceEnabled, &dummyLen);
    afsi_log("Freelance client feature %s activated",
              cm_freelanceEnabled ? "is" : "is not");

    dummyLen = sizeof(cm_freelanceImportCellServDB);
    code = RegQueryValueEx(parmKey, "FreelanceImportCellServDB", NULL, NULL,
                            (BYTE *) &cm_freelanceImportCellServDB, &dummyLen);
    afsi_log("Freelance client %s import CellServDB",
              cm_freelanceImportCellServDB ? "does" : "does not");
#endif /* AFS_FREELANCE_CLIENT */

    dummyLen = sizeof(smb_UseUnicode);
    code = RegQueryValueEx(parmKey, "NegotiateUnicode", NULL, NULL,
                           (BYTE *) &smb_UseUnicode, &dummyLen);
    if (code != ERROR_SUCCESS) {
        smb_UseUnicode = 1; /* default on */
    }
    afsi_log("SMB Server Unicode Support is %s",
              smb_UseUnicode ? "enabled" : "disabled");

    dummyLen = sizeof(smb_hideDotFiles);
    code = RegQueryValueEx(parmKey, "HideDotFiles", NULL, NULL,
                           (BYTE *) &smb_hideDotFiles, &dummyLen);
    if (code != ERROR_SUCCESS) {
        smb_hideDotFiles = 1; /* default on */
    }
    afsi_log("Dot files/dirs will %sbe marked hidden",
              smb_hideDotFiles ? "" : "not ");

    dummyLen = sizeof(dwValue);
    code = RegQueryValueEx(parmKey, "UnixModeFileDefault", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        smb_unixModeDefaultFile = (dwValue & 07777);
    }
    afsi_log("Default unix mode bits for files is 0%04o", smb_unixModeDefaultFile);

    dummyLen = sizeof(dwValue);
    code = RegQueryValueEx(parmKey, "UnixModeDirDefault", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        smb_unixModeDefaultDir = (dwValue & 07777);
    }
    afsi_log("Default unix mode bits for directories is 0%04o", smb_unixModeDefaultDir);

    dummyLen = sizeof(smb_maxMpxRequests);
    code = RegQueryValueEx(parmKey, "MaxMpxRequests", NULL, NULL,
                           (BYTE *) &smb_maxMpxRequests, &dummyLen);
    if (code != ERROR_SUCCESS) {
        smb_maxMpxRequests = 50;
    }
    afsi_log("Maximum number of multiplexed sessions is %d", smb_maxMpxRequests);

    dummyLen = sizeof(smb_maxVCPerServer);
    code = RegQueryValueEx(parmKey, "MaxVCPerServer", NULL, NULL,
                           (BYTE *) &smb_maxVCPerServer, &dummyLen);
    if (code != ERROR_SUCCESS) {
        smb_maxVCPerServer = 100;
    }
    afsi_log("Maximum number of VCs per server is %d", smb_maxVCPerServer);

    dummyLen = sizeof(smb_authType);
    code = RegQueryValueEx(parmKey, "SMBAuthType", NULL, NULL,
                            (BYTE *) &smb_authType, &dummyLen);

    if (code != ERROR_SUCCESS ||
         (smb_authType != SMB_AUTH_EXTENDED && smb_authType != SMB_AUTH_NTLM && smb_authType != SMB_AUTH_NONE)) {
        smb_authType = SMB_AUTH_EXTENDED; /* default is to use extended authentication */
    }
    afsi_log("SMB authentication type is %s", ((smb_authType == SMB_AUTH_NONE)?"NONE":((smb_authType == SMB_AUTH_EXTENDED)?"EXTENDED":"NTLM")));

    dummyLen = sizeof(rx_max_rwin_size);
    code = RegQueryValueEx(parmKey, "RxMaxRecvWinSize", NULL, NULL,
                           (BYTE *) &rx_max_rwin_size, &dummyLen);
    if (code == ERROR_SUCCESS)
        rx_SetMaxReceiveWindow(rx_max_rwin_size);
    afsi_log("Rx Maximum Receive Window Size is %d", rx_GetMaxReceiveWindow());

    dummyLen = sizeof(rx_max_swin_size);
    code = RegQueryValueEx(parmKey, "RxMaxSendWinSize", NULL, NULL,
                           (BYTE *) &rx_max_swin_size, &dummyLen);
    if (code == ERROR_SUCCESS)
        rx_SetMaxSendWindow(rx_max_swin_size);
    afsi_log("Rx Maximum Send Window Size is %d", rx_GetMaxSendWindow());

    dummyLen = sizeof(rx_min_peer_timeout);
    code = RegQueryValueEx(parmKey, "RxMinPeerTimeout", NULL, NULL,
                           (BYTE *) &rx_min_peer_timeout, &dummyLen);
    if (code == ERROR_SUCCESS)
        rx_SetMinPeerTimeout(rx_min_peer_timeout);
    afsi_log("Rx Minimum Peer Timeout is %d ms", rx_GetMinPeerTimeout());

    dummyLen = sizeof(rx_pmtu_discovery);
    code = RegQueryValueEx(parmKey, "RxPMTUDiscovery", NULL, NULL,
                           (BYTE *) &rx_pmtu_discovery, &dummyLen);
    afsi_log("Rx PMTU Discovery is %d ms", rx_pmtu_discovery);

    dummyLen = sizeof(rx_nojumbo);
    code = RegQueryValueEx(parmKey, "RxNoJumbo", NULL, NULL,
                           (BYTE *) &rx_nojumbo, &dummyLen);
    if (code != ERROR_SUCCESS) {
        DWORD jumbo;
        dummyLen = sizeof(jumbo);
        code = RegQueryValueEx(parmKey, "RxJumbo", NULL, NULL,
                                (BYTE *) &jumbo, &dummyLen);
        if (code != ERROR_SUCCESS) {
            rx_nojumbo = 1;
        } else {
            rx_nojumbo = !jumbo;
        }
    }
    if (rx_nojumbo)
        afsi_log("RX Jumbograms are disabled");
    else
        afsi_log("RX Jumbograms are enabled");

    dummyLen = sizeof(rx_extraPackets);
    code = RegQueryValueEx(parmKey, "RxExtraPackets", NULL, NULL,
                           (BYTE *) &rx_extraPackets, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_extraPackets = (numBkgD + numSvThreads + 5) * 64;
    }
    if (rx_extraPackets)
        afsi_log("RX extraPackets is %d", rx_extraPackets);

    dummyLen = sizeof(rx_udpbufsize);
    code = RegQueryValueEx(parmKey, "RxUdpBufSize", NULL, NULL,
                           (BYTE *) &rx_udpbufsize, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_udpbufsize = 256*1024;
    }
    if (rx_udpbufsize != -1)
        afsi_log("RX udpbufsize is %d", rx_udpbufsize);

    dummyLen = sizeof(rx_mtu);
    code = RegQueryValueEx(parmKey, "RxMaxMTU", NULL, NULL,
                           (BYTE *) &rx_mtu, &dummyLen);
    if (code != ERROR_SUCCESS || !rx_mtu) {
        rx_mtu = -1;
    }
    if (rx_mtu != -1)
        afsi_log("RX maximum MTU is %d", rx_mtu);

    dummyLen = sizeof(rx_enable_peer_stats);
    code = RegQueryValueEx(parmKey, "RxEnablePeerStats", NULL, NULL,
                           (BYTE *) &rx_enable_peer_stats, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_enable_peer_stats = 1;
    }
    if (rx_enable_peer_stats)
        afsi_log("RX Peer Statistics gathering is enabled");
    else
        afsi_log("RX Peer Statistics gathering is disabled");

    dummyLen = sizeof(rx_enable_process_stats);
    code = RegQueryValueEx(parmKey, "RxEnableProcessStats", NULL, NULL,
                           (BYTE *) &rx_enable_process_stats, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_enable_process_stats = 1;
    }
    if (rx_enable_process_stats)
        afsi_log("RX Process Statistics gathering is enabled");
    else
        afsi_log("RX Process Statistics gathering is disabled");

    dummyLen = sizeof(dwValue);
    dwValue = 0;
    code = RegQueryValueEx(parmKey, "RxEnableHotThread", NULL, NULL,
                            (BYTE *) &dwValue, &dummyLen);
     if (code != ERROR_SUCCESS || dwValue != 0) {
         rx_EnableHotThread();
         afsi_log("RX Hot Thread is enabled");
     }
     else
         afsi_log("RX Hot Thread is disabled");

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "CallBackPort", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_callbackport = (unsigned short) dwValue;
    }
    afsi_log("CM CallBackPort is %u", cm_callbackport);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "EnableServerLocks", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_enableServerLocks = (unsigned short) dwValue;
    }
    switch (cm_enableServerLocks) {
    case 0:
	afsi_log("EnableServerLocks: never");
    	break;
    case 2:
	afsi_log("EnableServerLocks: always");
	break;
    case 1:
    default:
	afsi_log("EnableServerLocks: server requested");
    	break;
    }

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "DeleteReadOnly", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_deleteReadOnly = (unsigned short) dwValue;
    }
    afsi_log("CM DeleteReadOnly is %u", cm_deleteReadOnly);

#ifdef USE_BPLUS
    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "BPlusTrees", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_BPlusTrees = (unsigned short) dwValue;
    }
    afsi_log("CM BPlusTrees is %u", cm_BPlusTrees);

    if (cm_BPlusTrees && !cm_InitBPlusDir()) {
        cm_BPlusTrees = 0;
        afsi_log("CM BPlusTree initialization failure; disabled for this session");
    }
#else
    afsi_log("CM BPlusTrees is not supported");
#endif

    if ((RegQueryValueExW( parmKey, L"PrefetchExecutableExtensions", 0,
                           &regType, NULL, &dummyLen) == ERROR_SUCCESS) &&
         (regType == REG_MULTI_SZ))
    {
        clientchar_t * pSz;
        dummyLen += 3; /* in case the source string is not nul terminated */
        pSz = malloc(dummyLen);
        if ((RegQueryValueExW( parmKey, L"PrefetchExecutableExtensions", 0, &regType,
                               (LPBYTE) pSz, &dummyLen) == ERROR_SUCCESS) &&
             (regType == REG_MULTI_SZ))
        {
            int cnt;
            clientchar_t * p;

            for (cnt = 0, p = pSz; (p - pSz < dummyLen) && *p; cnt++, p += cm_ClientStrLen(p) + 1);

            smb_ExecutableExtensions = malloc(sizeof(clientchar_t *) * (cnt+1));

            for (cnt = 0, p = pSz; (p - pSz < dummyLen) && *p; cnt++, p += cm_ClientStrLen(p) + 1) {
                smb_ExecutableExtensions[cnt] = p;
                afsi_log("PrefetchExecutableExtension: \"%S\"", p);
            }
            smb_ExecutableExtensions[cnt] = NULL;
        }

        if (!smb_ExecutableExtensions)
            free(pSz);
    }
    if (!smb_ExecutableExtensions)
        afsi_log("No PrefetchExecutableExtensions");

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "OfflineReadOnlyIsValid", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_OfflineROIsValid = (unsigned short) dwValue;
    }
    afsi_log("CM OfflineReadOnlyIsValid is %u", cm_OfflineROIsValid);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "GiveUpAllCallBacks", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_giveUpAllCBs = (unsigned short) dwValue;
    }
    afsi_log("CM GiveUpAllCallBacks is %u", cm_giveUpAllCBs);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "FollowBackupPath", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_followBackupPath = (unsigned short) dwValue;
    }
    afsi_log("CM FollowBackupPath is %u", cm_followBackupPath);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "PerFileAccessCheck", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_accessPerFileCheck = (int) dwValue;
    }
    afsi_log("CM PerFileAccessCheck is %d", cm_accessPerFileCheck);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "ReadOnlyVolumeVersioning", NULL, NULL,
                           (BYTE *) &dwValue, &dummyLen);
    if (code == ERROR_SUCCESS) {
        cm_readonlyVolumeVersioning = (unsigned short) dwValue;
    }
    afsi_log("CM ReadOnlyVolumeVersioning is %u", cm_readonlyVolumeVersioning);

    RegCloseKey (parmKey);

    cacheBlocks = ((afs_uint64)cacheSize * 1024) / blockSize;

    /* get network related info */
    cm_noIPAddr = CM_MAXINTERFACE_ADDR;
    code = syscfg_GetIFInfo(&cm_noIPAddr,
                             cm_IPAddr, cm_SubnetMask,
                             cm_NetMtu, cm_NetFlags);

    if ( (cm_noIPAddr <= 0) || (code <= 0 ) )
        afsi_log("syscfg_GetIFInfo error code %d", code);
    else
        afsi_log("First Network address %x SubnetMask %x",
                  cm_IPAddr[0], cm_SubnetMask[0]);

    /*
     * Save client configuration for GetCacheConfig requests
     */
    cm_initParams.nChunkFiles = 0;
    cm_initParams.nStatCaches = stats;
    cm_initParams.nDataCaches = (afs_uint32)(cacheBlocks > 0xFFFFFFFF ? 0xFFFFFFFF : cacheBlocks);
    cm_initParams.nVolumeCaches = volumes;
    cm_initParams.firstChunkSize = cm_chunkSize;
    cm_initParams.otherChunkSize = cm_chunkSize;
    cm_initParams.cacheSize = cacheSize;
    cm_initParams.setTime = 0;
    cm_initParams.memCache = 1;

    /* init user daemon, and other packages */
    cm_InitUser();

    cm_InitConn();

    cm_InitServer();

    cm_InitIoctl();

    smb_InitIoctl();

    cm_InitCallback();

    cm_InitNormalization();

    code = cm_InitMappedMemory(virtualCache, cm_CachePath, stats, volumes, cells, cm_chunkSize, cacheBlocks, blockSize);
    afsi_log("cm_InitMappedMemory code %x", code);
    if (code != 0) {
        *reasonP = "error initializing cache file";
        return -1;
    }

#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
    if (cm_InitDNS(cm_dnsEnabled) == -1)
        cm_dnsEnabled = 0;  /* init failed, so deactivate */
    afsi_log("cm_InitDNS %d", cm_dnsEnabled);
#endif

    /* Set RX parameters before initializing RX */
    if ( rx_nojumbo ) {
        rx_SetNoJumbo();
        afsi_log("rx_SetNoJumbo successful");
    }

    if ( rx_mtu != -1 ) {
        extern void rx_SetMaxMTU(int);

        rx_SetMaxMTU(rx_mtu);
        afsi_log("rx_SetMaxMTU %d successful", rx_mtu);
    }

    if ( rx_udpbufsize != -1 ) {
        rx_SetUdpBufSize(rx_udpbufsize);
        afsi_log("rx_SetUdpBufSize %d", rx_udpbufsize);
    }

    rx_SetBusyChannelError(1);  /* Activate busy call channel reporting */

    /* initialize RX, and tell it to listen to the callbackport,
     * which is used for callback RPC messages.
     */
    code = rx_Init(htons(cm_callbackport));
    if (code != 0) {
	afsi_log("rx_Init code %x - retrying with a random port number", code);
	code = rx_Init(0);
    }
    afsi_log("rx_Init code %x", code);
    if (code != 0) {
        *reasonP = "afsd: failed to init rx client";
        return -1;
    }

    /* create an unauthenticated service #1 for callbacks */
    nullServerSecurityClassp = rxnull_NewServerSecurityObject();
    serverp = rx_NewService(0, 1, "AFS", &nullServerSecurityClassp, 1,
                             RXAFSCB_ExecuteRequest);
    afsi_log("rx_NewService addr %x", PtrToUlong(serverp));
    if (serverp == NULL) {
        *reasonP = "unknown error";
        return -1;
    }
    rx_SetMinProcs(serverp, 2);
    rx_SetMaxProcs(serverp, 4);
    rx_SetCheckReach(serverp, 1);

    nullServerSecurityClassp = rxnull_NewServerSecurityObject();
    serverp = rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats",
                             &nullServerSecurityClassp, 1, RXSTATS_ExecuteRequest);
    afsi_log("rx_NewService addr %x", PtrToUlong(serverp));
    if (serverp == NULL) {
        *reasonP = "unknown error";
        return -1;
    }
    rx_SetMinProcs(serverp, 2);
    rx_SetMaxProcs(serverp, 4);

    /* start server threads, *not* donating this one to the pool */
    rx_StartServer(0);
    afsi_log("rx_StartServer");

    if (rx_enable_peer_stats)
	rx_enablePeerRPCStats();

    if (rx_enable_process_stats)
	rx_enableProcessRPCStats();

    code = cm_GetRootCellName(rootCellName);
    afsi_log("cm_GetRootCellName code %d, cm_freelanceEnabled= %d, rcn= %s",
             code, cm_freelanceEnabled, (code ? "<none>" : rootCellName));
    if (code != 0 && !cm_freelanceEnabled)
    {
        *reasonP = "can't find root cell name in " AFS_CELLSERVDB;
        return -1;
    }
    else if (cm_freelanceEnabled)
        cm_data.rootCellp = NULL;

    if (code == 0 && !cm_freelanceEnabled)
    {
        cm_data.rootCellp = cm_GetCell(rootCellName, CM_FLAG_CREATE);
        afsi_log("cm_GetCell addr %x", PtrToUlong(cm_data.rootCellp));
        if (cm_data.rootCellp == NULL)
        {
            *reasonP = "can't find root cell in " AFS_CELLSERVDB;
            return -1;
        }
    }

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled)
        cm_InitFreelance();
#endif

    /* Initialize the RPC server for session keys */
    RpcInit();

    /* Initialize the RPC server for pipe services */
    MSRPC_Init();

    afsd_InitServerPreferences();

    code = afsd_InitRoot(reasonP);

    return code;
}

int afsd_ShutdownCM(void)
{
    MSRPC_Shutdown();

    cm_ReleaseSCache(cm_data.rootSCachep);

    cm_utilsCleanup();

    cm_shutdown = 1;

    return 0;
}

int afsd_InitDaemons(char **reasonP)
{
    cm_InitDaemon(numBkgD);
    afsi_log("cm_InitDaemon complete");

    return 0;
}

int afsd_InitSMB(char **reasonP, void *aMBfunc)
{
    HKEY parmKey;
    DWORD dummyLen;
    DWORD dwValue;
    DWORD code;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(DWORD);
        code = RegQueryValueEx(parmKey, "StoreAnsiFilenames", NULL, NULL,
                                (BYTE *) &dwValue, &dummyLen);
        if (code == ERROR_SUCCESS)
            smb_StoreAnsiFilenames = dwValue ? 1 : 0;
        afsi_log("StoreAnsiFilenames = %d", smb_StoreAnsiFilenames);

        dummyLen = sizeof(DWORD);
        code = RegQueryValueEx(parmKey, "EnableSMBAsyncStore", NULL, NULL,
                                (BYTE *) &dwValue, &dummyLen);
        if (code == ERROR_SUCCESS)
            smb_AsyncStore = dwValue == 2 ? 2 : (dwValue ? 1 : 0);
        afsi_log("EnableSMBAsyncStore = %d", smb_AsyncStore);

        dummyLen = sizeof(DWORD);
        code = RegQueryValueEx(parmKey, "SMBAsyncStoreSize", NULL, NULL,
                                (BYTE *) &dwValue, &dummyLen);
        if (code == ERROR_SUCCESS) {
            /* Should check for >= blocksize && <= chunksize && round down to multiple of blocksize */
            if (dwValue > cm_chunkSize)
                smb_AsyncStoreSize = cm_chunkSize;
            else if (dwValue <  cm_data.buf_blockSize)
                smb_AsyncStoreSize = cm_data.buf_blockSize;
            else
                smb_AsyncStoreSize = (dwValue & ~(cm_data.buf_blockSize-1));
        } else
            smb_AsyncStoreSize = CM_CONFIGDEFAULT_ASYNCSTORESIZE;
        afsi_log("SMBAsyncStoreSize = %d", smb_AsyncStoreSize);

        RegCloseKey (parmKey);
    }

    /* Do this last so that we don't handle requests before init is done.
     * Here we initialize the SMB listener.
     */
    smb_Init(afsd_logp, smb_UseV3, numSvThreads, aMBfunc);
    afsi_log("smb_Init complete");

    return 0;
}

#ifdef ReadOnly
#undef ReadOnly
#endif

#ifdef File
#undef File
#endif

#pragma pack( push, before_imagehlp, 8 )
#include <imagehlp.h>
#pragma pack( pop, before_imagehlp )

#define MAXNAMELEN 1024

void afsd_printStack(HANDLE hThread, CONTEXT *c)
{
    HANDLE hProcess = GetCurrentProcess();
    int frameNum;
#if defined(_AMD64_)
    DWORD64 offset;
#elif defined(_X86_)
    DWORD offset;
#endif
    DWORD symOptions;
    char functionName[MAXNAMELEN];

    IMAGEHLP_MODULE Module;
    IMAGEHLP_LINE Line;

    STACKFRAME s;
    IMAGEHLP_SYMBOL *pSym;

    afsi_log_useTimestamp = 0;

    pSym = (IMAGEHLP_SYMBOL *) GlobalAlloc(0, sizeof (IMAGEHLP_SYMBOL) + MAXNAMELEN);

    memset( &s, '\0', sizeof s );
    if (!SymInitialize(hProcess, NULL, 1) )
    {
        afsi_log("SymInitialize(): GetLastError() = %lu\n", GetLastError() );

        SymCleanup( hProcess );
        GlobalFree(pSym);

        return;
    }

    symOptions = SymGetOptions();
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions &= ~SYMOPT_UNDNAME;
    SymSetOptions( symOptions );

    /*
     * init STACKFRAME for first call
     * Notes: AddrModeFlat is just an assumption. I hate VDM debugging.
     * Notes: will have to be #ifdef-ed for Alphas; MIPSes are dead anyway,
     * and good riddance.
     */
#if defined (_ALPHA_) || defined (_MIPS_) || defined (_PPC_)
#error The STACKFRAME initialization in afsd_printStack() for this platform
#error must be properly configured
#elif defined(_AMD64_)
    s.AddrPC.Offset = c->Rip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c->Rbp;
    s.AddrFrame.Mode = AddrModeFlat;
#else
    s.AddrPC.Offset = c->Eip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c->Ebp;
    s.AddrFrame.Mode = AddrModeFlat;
#endif

    memset( pSym, '\0', sizeof (IMAGEHLP_SYMBOL) + MAXNAMELEN );
    pSym->SizeOfStruct = sizeof (IMAGEHLP_SYMBOL);
    pSym->MaxNameLength = MAXNAMELEN;

    memset( &Line, '\0', sizeof Line );
    Line.SizeOfStruct = sizeof Line;

    memset( &Module, '\0', sizeof Module );
    Module.SizeOfStruct = sizeof Module;

    offset = 0;

    afsi_log("\n--# FV EIP----- RetAddr- FramePtr StackPtr Symbol" );

    for ( frameNum = 0; ; ++ frameNum )
    {
        /*
         * get next stack frame (StackWalk(), SymFunctionTableAccess(),
         * SymGetModuleBase()). if this returns ERROR_INVALID_ADDRESS (487) or
         * ERROR_NOACCESS (998), you can assume that either you are done, or
         * that the stack is so hosed that the next deeper frame could not be
         * found.
         */
        if ( ! StackWalk( IMAGE_FILE_MACHINE_I386, hProcess, hThread, &s, c,
                          NULL, SymFunctionTableAccess, SymGetModuleBase,
                          NULL ) )
            break;

        /* display its contents */
        afsi_log("\n%3d %c%c %08lx %08lx %08lx %08lx ",
                 frameNum, s.Far? 'F': '.', s.Virtual? 'V': '.',
                 s.AddrPC.Offset, s.AddrReturn.Offset,
                 s.AddrFrame.Offset, s.AddrStack.Offset );

        if ( s.AddrPC.Offset == 0 )
        {
            afsi_log("(-nosymbols- PC == 0)" );
        }
        else
        {
            /* show procedure info from a valid PC */
            if (!SymGetSymFromAddr(hProcess, s.AddrPC.Offset, &offset, pSym))
            {
                if ( GetLastError() != ERROR_INVALID_ADDRESS )
                {
                    afsi_log("SymGetSymFromAddr(): errno = %lu",
                             GetLastError());
                }
            }
            else
            {
                UnDecorateSymbolName(pSym->Name, functionName, MAXNAMELEN,
                                     UNDNAME_NAME_ONLY);
                afsi_log("%s", functionName );

                if ( offset != 0 )
                {
                    afsi_log(" %+ld bytes", (long) offset);
                }
            }

            if (!SymGetLineFromAddr(hProcess, s.AddrPC.Offset, &offset, &Line))
            {
                if (GetLastError() != ERROR_INVALID_ADDRESS)
                {
                    afsi_log("Error: SymGetLineFromAddr(): errno = %lu",
                             GetLastError());
                }
            }
            else
            {
                afsi_log("    Line: %s(%lu) %+ld bytes", Line.FileName,
                         Line.LineNumber, offset);
            }
        }

        /* no return address means no deeper stackframe */
        if (s.AddrReturn.Offset == 0)
        {
            SetLastError(0);
            break;
        }
    }

    if (GetLastError() != 0)
    {
        afsi_log("\nStackWalk(): errno = %lu\n", GetLastError());
    }

    SymCleanup(hProcess);
    GlobalFree(pSym);
}

#ifdef _DEBUG
static DWORD *afsd_crtDbgBreakCurrent = NULL;
static DWORD afsd_crtDbgBreaks[256];
#endif

static EFaultRepRetVal (WINAPI *pReportFault)(LPEXCEPTION_POINTERS pep, DWORD dwMode) = NULL;
static BOOL (WINAPI *pMiniDumpWriteDump)(HANDLE hProcess,DWORD ProcessId,HANDLE hFile,
                                  MINIDUMP_TYPE DumpType,
                                  PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                  PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                  PMINIDUMP_CALLBACK_INFORMATION CallbackParam) = NULL;


static HANDLE
OpenDumpFile(void)
{
    char tmp[256];
    char wd[256];
    SYSTEMTIME st;
    DWORD code;

    code = GetEnvironmentVariable("TEMP", tmp, sizeof(tmp));
    if ( code == 0 || code > sizeof(tmp) )
    {
        if (!GetWindowsDirectory(tmp, sizeof(tmp)))
            return NULL;
    }
    GetLocalTime(&st);
    StringCbPrintfA(wd, sizeof(wd),
                    "%s\\afsd-%04d-%02d-%02d-%02d_%02d_%02d.dmp", tmp,
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return CreateFile( wd, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
}

void
GenerateMiniDump(PEXCEPTION_POINTERS ep)
{
    if (IsDebuggerPresent())
        return;

    if (ep == NULL)
    {
        // Generate exception to get proper context in dump
        __try
        {
            RaiseException(DBG_CONTINUE, 0, 0, NULL);
        }
        __except(GenerateMiniDump(GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION)
        {
        }
    }
    else
    {
        MINIDUMP_EXCEPTION_INFORMATION eInfo;
        HANDLE hFile = NULL;
        HMODULE hDbgHelp = NULL;

        hDbgHelp = LoadLibrary("Dbghelp.dll");
        if ( hDbgHelp == NULL )
            return;

        (FARPROC) pMiniDumpWriteDump = GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        if ( pMiniDumpWriteDump == NULL ) {
            FreeLibrary(hDbgHelp);
            return;
        }

        hFile = OpenDumpFile();

        if ( hFile ) {
            HKEY parmKey;
            DWORD dummyLen;
            DWORD dwValue;
            DWORD code;
            DWORD dwMiniDumpType = MiniDumpWithDataSegs;

            code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                                 0, KEY_QUERY_VALUE, &parmKey);
            if (code == ERROR_SUCCESS) {
                dummyLen = sizeof(DWORD);
                code = RegQueryValueEx(parmKey, "MiniDumpType", NULL, NULL,
                                        (BYTE *) &dwValue, &dummyLen);
                if (code == ERROR_SUCCESS)
                    dwMiniDumpType = dwValue;
                RegCloseKey (parmKey);
            }

            eInfo.ThreadId = GetCurrentThreadId();
            eInfo.ExceptionPointers = ep;
            eInfo.ClientPointers = FALSE;

            pMiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(),
                                hFile, dwMiniDumpType, ep ? &eInfo : NULL,
                                NULL, NULL);

            CloseHandle(hFile);
        }
        FreeLibrary(hDbgHelp);
    }
}

LONG __stdcall afsd_ExceptionFilter(EXCEPTION_POINTERS *ep)
{
    CONTEXT context;
#ifdef _DEBUG
    BOOL allocRequestBrk = FALSE;
#endif
    HMODULE hLib = NULL;

    afsi_log("UnhandledException : code : 0x%x, address: 0x%x\n",
             ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);

#ifdef _DEBUG
    if (afsd_crtDbgBreakCurrent &&
        *afsd_crtDbgBreakCurrent == _CrtSetBreakAlloc(*afsd_crtDbgBreakCurrent))
    {
        allocRequestBrk = TRUE;
        afsi_log("Breaking on alloc request # %d\n", *afsd_crtDbgBreakCurrent);
    }
#endif

    /* save context if we want to print the stack information */
    context = *ep->ContextRecord;

    afsd_printStack(GetCurrentThread(), &context);

    GenerateMiniDump(ep);

    hLib = LoadLibrary("Faultrep.dll");
    if ( hLib ) {
        (FARPROC) pReportFault = GetProcAddress(hLib, "ReportFault");
        if ( pReportFault )
            pReportFault(ep, 0);
        FreeLibrary(hLib);
    }

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        afsi_log("\nEXCEPTION_BREAKPOINT - continue execution ...\n");

#ifdef _DEBUG
        if (allocRequestBrk)
        {
            afsd_crtDbgBreakCurrent++;
            _CrtSetBreakAlloc(*afsd_crtDbgBreakCurrent);
        }
#endif
#if defined(_X86)
        ep->ContextRecord->Eip++;
#endif
#if defined(_AMD64_)
        ep->ContextRecord->Rip++;
#endif
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void afsd_SetUnhandledExceptionFilter()
{
#ifndef NOTRACE
    SetUnhandledExceptionFilter(afsd_ExceptionFilter);
#endif
}

#ifdef _DEBUG
void afsd_DbgBreakAllocInit()
{
    memset(afsd_crtDbgBreaks, -1, sizeof(afsd_crtDbgBreaks));
    afsd_crtDbgBreakCurrent = afsd_crtDbgBreaks;
}

void afsd_DbgBreakAdd(DWORD requestNumber)
{
    int i;
    for (i = 0; i < sizeof(afsd_crtDbgBreaks) - 1; i++)
	{
        if (afsd_crtDbgBreaks[i] == -1)
	    {
            break;
	    }
	}
    afsd_crtDbgBreaks[i] = requestNumber;

    _CrtSetBreakAlloc(afsd_crtDbgBreaks[0]);
}
#endif
