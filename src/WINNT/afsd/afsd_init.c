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
#include <WINNT/syscfg.h>
#include <WINNT/afsreg.h>

#include "smb.h"
#include "cm_rpc.h"
#include "lanahelper.h"
#include <strsafe.h>
#include "cm_memmap.h"

extern int RXAFSCB_ExecuteRequest(struct rx_call *z_call);
extern int RXSTATS_ExecuteRequest(struct rx_call *z_call);

extern afs_int32 cryptall;
extern int cm_enableServerLocks;
extern int cm_followBackupPath;
extern int cm_deleteReadOnly;
#ifdef USE_BPLUS
extern afs_int32 cm_BPlusTrees;
#endif
extern afs_int32 cm_OfflineROIsValid;
extern afs_int32 cm_giveUpAllCBs;
extern const char **smb_ExecutableExtensions;

osi_log_t *afsd_logp;

cm_config_data_t        cm_data;

char cm_rootVolumeName[VL_MAXNAMELEN];
DWORD cm_rootVolumeNameLen;
char cm_mountRoot[1024];
DWORD cm_mountRootLen;
int cm_logChunkSize;
int cm_chunkSize;
#ifdef AFS_FREELANCE_CLIENT
char *cm_FakeRootDir;
#endif /* freelance */

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

char cm_NetbiosName[MAX_NB_NAME_LENGTH] = "";

char cm_CachePath[MAX_PATH];
DWORD cm_CachePathLen;
DWORD cm_ValidateCache = 1;

BOOL reportSessionStartups = FALSE;

cm_initparams_v1 cm_initParams;

char *cm_sysName = 0;
unsigned int   cm_sysNameCount = 0;
char *cm_sysNameList[MAXNUMSYSNAMES];

DWORD TraceOption = 0;

/*
 * AFSD Initialization Log
 *
 * This is distinct from the regular debug logging facility.
 * Log items go directly to a file, not to an array in memory, so that even
 * if AFSD crashes, the log can be inspected.
 */

HANDLE afsi_file;

#ifdef AFS_AFSDB_ENV
int cm_dnsEnabled = 1;
#endif


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

extern initUpperCaseTable();
void afsd_initUpperCaseTable() 
{
    initUpperCaseTable();
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

static void
configureBackConnectionHostNames(void)
{
    /* On Windows XP SP2, Windows 2003 SP1, and all future Windows operating systems
     * there is a restriction on the use of SMB authentication on loopback connections.
     * There are two work arounds available:
     * 
     *   (1) We can disable the check for matching host names.  This does not
     *   require a reboot:
     *   [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Lsa]
     *     "DisableLoopbackCheck"=dword:00000001
     *
     *   (2) We can add the AFS SMB/CIFS service name to an approved list.  This
     *   does require a reboot:
     *   [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Lsa\MSV1_0]
     *     "BackConnectionHostNames"=multi-sz
     *
     * The algorithm will be:
     *   (1) Check to see if cm_NetbiosName exists in the BackConnectionHostNames list
     *   (2a) If not, add it to the list.  (This will not take effect until the next reboot.)
     *   (2b1)    and check to see if DisableLoopbackCheck is set.
     *   (2b2)    If not set, set the DisableLoopbackCheck value to 0x1 
     *   (2b3)                and create HKLM\SOFTWARE\OpenAFS\Client  UnsetDisableLoopbackCheck
     *   (2c) else If cm_NetbiosName exists in the BackConnectionHostNames list,
     *             check for the UnsetDisableLoopbackCheck value.  
     *             If set, set the DisableLoopbackCheck flag to 0x0 
     *             and delete the UnsetDisableLoopbackCheck value
     *
     * Starting in Longhorn Beta 1, an entry in the BackConnectionHostNames value will
     * force Windows to use the loopback authentication mechanism for the specified 
     * services.
     */
    HKEY hkLsa;
    HKEY hkMSV10;
    HKEY hkClient;
    DWORD dwType;
    DWORD dwSize, dwAllocSize;
    DWORD dwValue;
    PBYTE pHostNames = NULL, pName = NULL;
    BOOL  bNameFound = FALSE;   

    if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                       "SYSTEM\\CurrentControlSet\\Control\\Lsa\\MSV1_0",
                       0,
                       KEY_READ|KEY_WRITE,
                       &hkMSV10) == ERROR_SUCCESS )
    {
        if ((RegQueryValueEx( hkMSV10, "BackConnectionHostNames", 0, 
			     &dwType, NULL, &dwAllocSize) == ERROR_SUCCESS) &&
            (dwType == REG_MULTI_SZ)) 
        {
	    dwAllocSize += 1 /* in case the source string is not nul terminated */
		+ strlen(cm_NetbiosName) + 2;
	    pHostNames = malloc(dwAllocSize);
	    dwSize = dwAllocSize;
            if (RegQueryValueEx( hkMSV10, "BackConnectionHostNames", 0, &dwType, 
				 pHostNames, &dwSize) == ERROR_SUCCESS) 
            {
		for (pName = pHostNames; 
		     (pName - pHostNames < dwSize) && *pName ; 
		     pName += strlen(pName) + 1)
		{
		    if ( !stricmp(pName, cm_NetbiosName) ) {
			bNameFound = TRUE;
			break;
		    }   
		}
	    }
        }
             
        if ( !bNameFound ) {
            size_t size = strlen(cm_NetbiosName) + 2;
            if ( !pHostNames ) {
                pHostNames = malloc(size);
		pName = pHostNames;
            }
            StringCbCopyA(pName, size, cm_NetbiosName);
            pName += size - 1;
            *pName = '\0';  /* add a second nul terminator */

            dwType = REG_MULTI_SZ;
	    dwSize = pName - pHostNames + 1;
            RegSetValueEx( hkMSV10, "BackConnectionHostNames", 0, dwType, pHostNames, dwSize);

            if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                               "SYSTEM\\CurrentControlSet\\Control\\Lsa",
                               0,
                               KEY_READ|KEY_WRITE,
                               &hkLsa) == ERROR_SUCCESS )
            {
                dwSize = sizeof(DWORD);
                if ( RegQueryValueEx( hkLsa, "DisableLoopbackCheck", 0, &dwType, (LPBYTE)&dwValue, &dwSize) != ERROR_SUCCESS ||
                     dwValue == 0 ) {
                    dwType = REG_DWORD;
                    dwSize = sizeof(DWORD);
                    dwValue = 1;
                    RegSetValueEx( hkLsa, "DisableLoopbackCheck", 0, dwType, (LPBYTE)&dwValue, dwSize);

                    if (RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                                        AFSREG_CLT_OPENAFS_SUBKEY,
                                        0,
                                        NULL,
                                        REG_OPTION_NON_VOLATILE,
                                        KEY_READ|KEY_WRITE,
                                        NULL,
                                        &hkClient,
                                        NULL) == ERROR_SUCCESS) {

                        dwType = REG_DWORD;
                        dwSize = sizeof(DWORD);
                        dwValue = 1;
                        RegSetValueEx( hkClient, "RemoveDisableLoopbackCheck", 0, dwType, (LPBYTE)&dwValue, dwSize);
                        RegCloseKey(hkClient);
                    }
                    RegCloseKey(hkLsa);
                }
            }
        } else {
            if (RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                                AFSREG_CLT_OPENAFS_SUBKEY,
                                0,
                                NULL,
                                REG_OPTION_NON_VOLATILE,
                                KEY_READ|KEY_WRITE,
                                NULL,
                                &hkClient,
                                NULL) == ERROR_SUCCESS) {

                dwSize = sizeof(DWORD);
                if ( RegQueryValueEx( hkClient, "RemoveDisableLoopbackCheck", 0, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
                     dwValue == 1 ) {
                    if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                                       "SYSTEM\\CurrentControlSet\\Control\\Lsa",
                                       0,
                                       KEY_READ|KEY_WRITE,
                                       &hkLsa) == ERROR_SUCCESS )
                    {
                        RegDeleteValue(hkLsa, "DisableLoopbackCheck");
                        RegCloseKey(hkLsa);
                    }
                }
                RegDeleteValue(hkClient, "RemoveDisableLoopbackCheck");
                RegCloseKey(hkClient);
            }
        }
        RegCloseKey(hkMSV10);
    }

    if (pHostNames)
	free(pHostNames);
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
            saddr.sin_family = AF_INET;
            dwRank += (rand() & 0x000f);

            tsp = cm_FindServer(&saddr, CM_SERVER_VLDB);
            if ( tsp )		/* an existing server - ref count increased */
            {
                tsp->ipRank = (USHORT)dwRank; /* no need to protect by mutex*/

                /* set preferences for an existing vlserver */
                cm_ChangeRankCellVLServer(tsp);
                cm_PutServer(tsp);  /* decrease refcount */
            }
            else	/* add a new server without a cell */
            {
                tsp = cm_NewServer(&saddr, CM_SERVER_VLDB, NULL, CM_FLAG_NOPROBE); /* refcount = 1 */
                tsp->ipRank = (USHORT)dwRank;
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
            saddr.sin_family = AF_INET;
            dwRank += (rand() & 0x000f);

            tsp = cm_FindServer(&saddr, CM_SERVER_FILE);
            if ( tsp )		/* an existing server - ref count increased */
            {
                tsp->ipRank = (USHORT)dwRank; /* no need to protect by mutex*/

                /* find volumes which might have RO copy 
                /* on server and change the ordering of 
                 * their RO list 
                 */
                cm_ChangeRankVolume(tsp);
                cm_PutServer(tsp);  /* decrease refcount */
            }
            else	/* add a new server without a cell */
            {
                tsp = cm_NewServer(&saddr, CM_SERVER_FILE, NULL, CM_FLAG_NOPROBE); /* refcount = 1 */
                tsp->ipRank = (USHORT)dwRank;
            }
        }

        RegCloseKey(hkPrefs);
    }
}

/*
 * AFSD Initialization
 */

int afsd_InitCM(char **reasonP)
{
    osi_uid_t debugID;
    afs_uint64 cacheBlocks;
    DWORD cacheSize;
    DWORD blockSize;
    long logChunkSize;
    DWORD stats;
    DWORD dwValue;
    DWORD rx_enable_peer_stats = 0;
    DWORD rx_enable_process_stats = 0;
    long traceBufSize;
    long maxcpus;
    long ltt, ltto;
    long rx_nojumbo;
    long virtualCache = 0;
    char rootCellName[256];
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
    char *p, *q; 
    int cm_noIPAddr;         /* number of client network interfaces */
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */

    WSAStartup(0x0101, &WSAjunk);

    afsd_initUpperCaseTable();
    init_et_to_sys_error();

    /* setup osidebug server at RPC slot 1000 */
    osi_LongToUID(1000, &debugID);
    code = osi_InitDebug(&debugID);
    afsi_log("osi_InitDebug code %d", code);

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

    dummyLen = sizeof(maxcpus);
    code = RegQueryValueEx(parmKey, "MaxCPUs", NULL, NULL,
                            (BYTE *) &maxcpus, &dummyLen);
    if (code == ERROR_SUCCESS) {
        HANDLE hProcess;
        DWORD_PTR processAffinityMask, systemAffinityMask;

        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_SET_INFORMATION,
                               FALSE, GetCurrentProcessId());
        if ( hProcess != NULL &&
             GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask) )
        {
            int i, n, bits;
            DWORD_PTR mask, newAffinityMask;

#if defined(_WIN64)
            bits = 64;
#else
            bits = 32;
#endif
            for ( i=0, n=0, mask=1, newAffinityMask=0; i<bits && n<maxcpus; i++ ) {
                if ( processAffinityMask & mask ) {
                    newAffinityMask |= mask;
                    n++;
                }
                mask *= 2;
            }

            SetProcessAffinityMask(hProcess, newAffinityMask);
            CloseHandle(hProcess);
            afsi_log("CPU Restrictions set to %d cpu(s); %d cpu(s) available", maxcpus, n);
        } else {
            afsi_log("CPU Restrictions set to %d cpu(s); unable to access process information", maxcpus);
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
        afsi_log("Status cache size %d", stats);
    else {
        stats = CM_CONFIGDEFAULT_STATS;
        afsi_log("Default status cache size %d", stats);
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
                            cm_rootVolumeName, &dummyLen);
    if (code == ERROR_SUCCESS)
        afsi_log("Root volume %s", cm_rootVolumeName);
    else {
        StringCbCopyA(cm_rootVolumeName, sizeof(cm_rootVolumeName), "root.afs");
        afsi_log("Default root volume name root.afs");
    }

    cm_mountRootLen = sizeof(cm_mountRoot);
    code = RegQueryValueEx(parmKey, "MountRoot", NULL, NULL,
                            cm_mountRoot, &cm_mountRootLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("Mount root %s", cm_mountRoot);
        cm_mountRootLen = (DWORD)strlen(cm_mountRoot);
    } else {
        StringCbCopyA(cm_mountRoot, sizeof(cm_mountRoot), "/afs");
        cm_mountRootLen = 4;
        /* Don't log */
    }

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
        cm_sysNameList[i] = osi_Alloc(MAXSYSNAME);
        cm_sysNameList[i][0] = '\0';
    }
    cm_sysName = cm_sysNameList[0];

    dummyLen = sizeof(buf);
    code = RegQueryValueEx(parmKey, "SysName", NULL, NULL, buf, &dummyLen);
    if (code != ERROR_SUCCESS || !buf[0]) {
#if defined(_IA64_)
        StringCbCopyA(buf, sizeof(buf), "ia64_win64");
#elif defined(_AMD64_)
        StringCbCopyA(buf, sizeof(buf), "amd64_win64 x86_win32 i386_w2k");
#else /* assume x86 32-bit */
        StringCbCopyA(buf, sizeof(buf), "x86_win32 i386_w2k i386_nt40");
#endif
    }
    afsi_log("Sys name %s", buf); 

    /* breakup buf into individual search string entries */
    for (p = q = buf; p < buf + dummyLen; p++)
    {
        if (*p == '\0' || isspace(*p)) {
            memcpy(cm_sysNameList[cm_sysNameCount],q,p-q);
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
  done_sysname:
    StringCbCopyA(cm_sysName, MAXSYSNAME, cm_sysNameList[0]);

    dummyLen = sizeof(cryptall);
    code = RegQueryValueEx(parmKey, "SecurityLevel", NULL, NULL,
                            (BYTE *) &cryptall, &dummyLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("SecurityLevel is %s", cryptall?"crypt":"clear");
    } else {
        cryptall = 0;
        afsi_log("Default SecurityLevel is clear");
    }

    if (cryptall)
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_ON);
    else
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_OFF);

#ifdef AFS_AFSDB_ENV
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
#else /* AFS_AFSDB_ENV */
    afsi_log("AFS not built with DNS support to find AFS cell servers");
#endif /* AFS_AFSDB_ENV */

#ifdef AFS_FREELANCE_CLIENT
    dummyLen = sizeof(cm_freelanceEnabled);
    code = RegQueryValueEx(parmKey, "FreelanceClient", NULL, NULL,
                            (BYTE *) &cm_freelanceEnabled, &dummyLen);
    if (code == ERROR_SUCCESS) {
        afsi_log("Freelance client feature %s activated",
                  cm_freelanceEnabled ? "is" : "is not");
    }       
    else {
        cm_freelanceEnabled = 1;  /* default on */
    }
#endif /* AFS_FREELANCE_CLIENT */

    dummyLen = sizeof(smb_hideDotFiles);
    code = RegQueryValueEx(parmKey, "HideDotFiles", NULL, NULL,
                           (BYTE *) &smb_hideDotFiles, &dummyLen);
    if (code != ERROR_SUCCESS) {
        smb_hideDotFiles = 1; /* default on */
    }
    afsi_log("Dot files/dirs will %sbe marked hidden",
              smb_hideDotFiles ? "" : "not ");

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

    dummyLen = sizeof(rx_nojumbo);
    code = RegQueryValueEx(parmKey, "RxNoJumbo", NULL, NULL,
                           (BYTE *) &rx_nojumbo, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_nojumbo = 0;
    }
    if (rx_nojumbo)
        afsi_log("RX Jumbograms are disabled");

    dummyLen = sizeof(rx_extraPackets);
    code = RegQueryValueEx(parmKey, "RxExtraPackets", NULL, NULL,
                           (BYTE *) &rx_extraPackets, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_extraPackets = 120;
    }
    if (rx_extraPackets)
        afsi_log("RX extraPackets is %d", rx_extraPackets);

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
        rx_enable_peer_stats = 0;
    }
    if (rx_enable_peer_stats)
        afsi_log("RX Peer Statistics gathering is enabled");

    dummyLen = sizeof(rx_enable_process_stats);
    code = RegQueryValueEx(parmKey, "RxEnableProcessStats", NULL, NULL,
                           (BYTE *) &rx_enable_process_stats, &dummyLen);
    if (code != ERROR_SUCCESS) {
        rx_enable_process_stats = 0;
    }
    if (rx_enable_process_stats)
        afsi_log("RX Process Statistics gathering is enabled");

    dummyLen = sizeof(dwValue);
    dwValue = 0;
    code = RegQueryValueEx(parmKey, "RxEnableHotThread", NULL, NULL,
                            (BYTE *) &dwValue, &dummyLen);
     if (code == ERROR_SUCCESS && dwValue != 0) {
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

    if ((RegQueryValueEx( parmKey, "PrefetchExecutableExtensions", 0, 
                          &regType, NULL, &dummyLen) == ERROR_SUCCESS) &&
         (regType == REG_MULTI_SZ)) 
    {
        char * pSz;
        dummyLen += 3; /* in case the source string is not nul terminated */
        pSz = malloc(dummyLen);
        if ((RegQueryValueEx( parmKey, "PrefetchExecutableExtensions", 0, &regType, 
                             pSz, &dummyLen) == ERROR_SUCCESS) &&
             (regType == REG_MULTI_SZ))
        {
            int cnt;
            char * p;

            for (cnt = 0, p = pSz; (p - pSz < dummyLen) && *p; cnt++, p += strlen(p) + 1);

            smb_ExecutableExtensions = malloc(sizeof(char *) * (cnt+1));

            for (cnt = 0, p = pSz; (p - pSz < dummyLen) && *p; cnt++, p += strlen(p) + 1)
            {
                smb_ExecutableExtensions[cnt] = p;
                afsi_log("PrefetchExecutableExtension: \"%s\"", p);
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
    afsi_log("CM OfflineReadOnlyIsValid is %u", cm_deleteReadOnly);
    
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
    cm_initParams.nVolumeCaches = stats/2;
    cm_initParams.firstChunkSize = cm_chunkSize;
    cm_initParams.otherChunkSize = cm_chunkSize;
    cm_initParams.cacheSize = cacheSize;
    cm_initParams.setTime = 0;
    cm_initParams.memCache = 1;

    /* Ensure the AFS Netbios Name is registered to allow loopback access */
    configureBackConnectionHostNames();

    /* init user daemon, and other packages */
    cm_InitUser();

    cm_InitConn();

    cm_InitServer();
        
    cm_InitIoctl();
        
    smb_InitIoctl();
        
    cm_InitCallback();

    code = cm_InitMappedMemory(virtualCache, cm_CachePath, stats, cm_chunkSize, cacheBlocks, blockSize);
    afsi_log("cm_InitMappedMemory code %x", code);
    if (code != 0) {
        *reasonP = "error initializing cache file";
        return -1;
    }

#ifdef AFS_AFSDB_ENV
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
    if (cm_InitDNS(cm_dnsEnabled) == -1)
        cm_dnsEnabled = 0;  /* init failed, so deactivate */
    afsi_log("cm_InitDNS %d", cm_dnsEnabled);
#endif
#endif

    /* Set RX parameters before initializing RX */
    if ( rx_nojumbo ) {
        rx_SetNoJumbo();
        afsi_log("rx_SetNoJumbo successful");
    }

    if ( rx_mtu != -1 ) {
        rx_SetMaxMTU(rx_mtu);
        afsi_log("rx_SetMaxMTU %d successful", rx_mtu);
    }

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

    nullServerSecurityClassp = rxnull_NewServerSecurityObject();
    serverp = rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats",
                             &nullServerSecurityClassp, 1, RXSTATS_ExecuteRequest);
    afsi_log("rx_NewService addr %x", PtrToUlong(serverp));
    if (serverp == NULL) {
        *reasonP = "unknown error";
        return -1;
    }
        
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

    afsd_InitServerPreferences();
    return 0;
}

int afsd_ShutdownCM(void)
{
    cm_ReleaseSCache(cm_data.rootSCachep);

    return 0;
}

int afsd_InitDaemons(char **reasonP)
{
    long code;
    cm_req_t req;

    cm_InitReq(&req);

    /* this should really be in an init daemon from here on down */

    if (!cm_freelanceEnabled) {
	int attempts = 10;

        osi_Log0(afsd_logp, "Loading Root Volume from cell");
	do {
	    code = cm_GetVolumeByName(cm_data.rootCellp, cm_rootVolumeName, cm_rootUserp,
				       &req, CM_GETVOL_FLAG_CREATE, &cm_data.rootVolumep);
	    afsi_log("cm_GetVolumeByName code %x root vol %x", code,
		      (code ? (cm_volume_t *)-1 : cm_data.rootVolumep));
	} while (code && --attempts);
        if (code != 0) {
            *reasonP = "can't find root volume in root cell";
            return -1;
        }
    }

    /* compute the root fid */
    if (!cm_freelanceEnabled) {
        cm_data.rootFid.cell = cm_data.rootCellp->cellID;
        cm_data.rootFid.volume = cm_GetROVolumeID(cm_data.rootVolumep);
        cm_data.rootFid.vnode = 1;
        cm_data.rootFid.unique = 1;
    }
    else
        cm_FakeRootFid(&cm_data.rootFid);
        
    code = cm_GetSCache(&cm_data.rootFid, &cm_data.rootSCachep, cm_rootUserp, &req);
    afsi_log("cm_GetSCache code %x scache %x", code,
             (code ? (cm_scache_t *)-1 : cm_data.rootSCachep));
    if (code != 0) {
        *reasonP = "unknown error";
        return -1;
    }

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
    char wd[256];
    DWORD code;

    code = GetEnvironmentVariable("TEMP", wd, sizeof(wd));
    if ( code == 0 || code > sizeof(wd) )
    {
        if (!GetWindowsDirectory(wd, sizeof(wd)))
            return NULL;
    }
    StringCbCatA(wd, sizeof(wd), "\\afsd.dmp");
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
    SetUnhandledExceptionFilter(afsd_ExceptionFilter);
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
