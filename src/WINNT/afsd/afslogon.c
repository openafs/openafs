/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "afslogon.h"

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <winsock2.h>
#include <lm.h>
#include <nb30.h>

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/pioctl_nt.h>
#include <afs/kautils.h>

#include "afsd.h"
#include "cm_config.h"
#include "krb.h"
#include "afskfw.h"
#include "lanahelper.h"

#include <WINNT\afsreg.h>

DWORD TraceOption = 0;

HANDLE hDLL;

#define AFS_LOGON_EVENT_NAME TEXT("AFS Logon")

void DebugEvent0(char *a) 
{
    HANDLE h; char *ptbuf[1];
    
    if (!ISLOGONTRACE(TraceOption))
        return;
    
    h = RegisterEventSource(NULL, AFS_LOGON_EVENT_NAME);
    ptbuf[0] = a;
    ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);
    DeregisterEventSource(h);
}

#define MAXBUF_ 512
void DebugEvent(char *b,...) 
{
    HANDLE h; char *ptbuf[1],buf[MAXBUF_+1];
    va_list marker;

    if (!ISLOGONTRACE(TraceOption))
        return;

    h = RegisterEventSource(NULL, AFS_LOGON_EVENT_NAME);
    va_start(marker,b);
    StringCbVPrintf(buf, MAXBUF_+1,b,marker);
    buf[MAXBUF_] = '\0';
    ptbuf[0] = buf;
    ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);
    DeregisterEventSource(h);
    va_end(marker);
}

static HANDLE hInitMutex = NULL;
static BOOL bInit = FALSE;

BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved)
{
    hDLL = dll;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        /* Initialization Mutex */
	if (!bInit) {
	    hInitMutex = CreateMutex(NULL, FALSE, NULL);
	    SetEnvironmentVariable(DO_NOT_REGISTER_VARNAME, "");
	}
        break;

    case DLL_PROCESS_DETACH:
	/* do nothing on unload because we might 
	 * be reloaded.
	 */
	CloseHandle(hInitMutex);
	hInitMutex = NULL;
	bInit = FALSE;
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
        /* Everything else succeeds but does nothing. */
        break;
    }

    return TRUE;
}

void AfsLogonInit(void)
{
    if ( bInit == FALSE ) {
        if ( WaitForSingleObject( hInitMutex, INFINITE ) == WAIT_OBJECT_0 ) {
	    /* initAFSDirPath() initializes an array and sets a 
	     * flag so that the initialization can only occur
	     * once.  No cleanup will be done when the DLL is 
	     * unloaded so the initialization will not be 
	     * performed again on a subsequent reload
	     */
	    initAFSDirPath();

	    /* ka_Init initializes a number of error tables.
	     * and then calls ka_CellConfig() which grabs 
	     * an afsconf_dir structure via afsconf_Open().
	     * Upon a second attempt to call ka_CellConfig()
	     * the structure will be released with afsconf_Close()
	     * and then re-opened.  Could this corrupt memory?
	     * 
	     * We only need this if we are not using KFW.
	     */
	    if (!KFW_is_available())
		ka_Init(0);
	    bInit = TRUE;
	}
	ReleaseMutex(hInitMutex);
    }
}

CHAR *GenRandomName(CHAR *pbuf)
{
    int i;
    srand( (unsigned)time( NULL ) );
    for (i=0;i<MAXRANDOMNAMELEN-1;i++)
        pbuf[i]='a'+(rand() % 26);
    pbuf[MAXRANDOMNAMELEN-1]=0;
    return pbuf;
}

BOOLEAN AFSWillAutoStart(void)
{
    SC_HANDLE scm;
    SC_HANDLE svc;
    BOOLEAN flag;
    BOOLEAN result = FALSE;
    LPQUERY_SERVICE_CONFIG pConfig = NULL;
    DWORD BufSize;
    LONG status;

    /* Open services manager */
    scm = OpenSCManager(NULL, NULL, GENERIC_READ);
    if (!scm) return FALSE;

    /* Open AFSD service */
    svc = OpenService(scm, "TransarcAFSDaemon", SERVICE_QUERY_CONFIG);
    if (!svc)
        goto close_scm;

    /* Query AFSD service config, first just to get buffer size */
    /* Expected to fail, so don't test return value */
    (void) QueryServiceConfig(svc, NULL, 0, &BufSize);
    status = GetLastError();
    if (status != ERROR_INSUFFICIENT_BUFFER)
        goto close_svc;

    /* Allocate buffer */
    pConfig = (LPQUERY_SERVICE_CONFIG)GlobalAlloc(GMEM_FIXED,BufSize);
    if (!pConfig)
        goto close_svc;

    /* Query AFSD service config, this time for real */
    flag = QueryServiceConfig(svc, pConfig, BufSize, &BufSize);
    if (!flag)
        goto free_pConfig;

    /* Is it autostart? */
    if (pConfig->dwStartType < SERVICE_DEMAND_START)
        result = TRUE;

  free_pConfig:
    GlobalFree(pConfig);
  close_svc:
    CloseServiceHandle(svc);
  close_scm:
    CloseServiceHandle(scm);

    return result;
}

DWORD MapAuthError(DWORD code)
{
    switch (code) {
        /* Unfortunately, returning WN_NO_NETWORK results in the MPR abandoning
         * logon scripts for all credential managers, although they will still
         * receive logon notifications.  Since we don't want this, we return
         * WN_SUCCESS.  This is highly undesirable, but we also don't want to
         * break other network providers.
         */
 /* case KTC_NOCM:
    case KTC_NOCMRPC:
    return WN_NO_NETWORK; */
    default: return WN_SUCCESS;
  }
}

DWORD APIENTRY NPGetCaps(DWORD index)
{
    switch (index) {
    case WNNC_NET_TYPE:
        /* Don't have our own type; use somebody else's. */
        return WNNC_NET_SUN_PC_NFS;

    case WNNC_START:
        /* Say we are already started, even though we might wait after we receive NPLogonNotify */
        return 1;

    default:
        return 0;
    }
}       

NET_API_STATUS 
NetUserGetProfilePath( LPCWSTR Domain, LPCWSTR UserName, char * profilePath, 
                       DWORD profilePathLen )
{
    NET_API_STATUS code;
    LPWSTR ServerName = NULL;
    LPUSER_INFO_3 p3 = NULL;

    NetGetAnyDCName(NULL, Domain, (LPBYTE *)&ServerName);
    /* if NetGetAnyDCName fails, ServerName == NULL
     * NetUserGetInfo will obtain local user information 
     */
    code = NetUserGetInfo(ServerName, UserName, 3, (LPBYTE *)&p3);
    if (code == NERR_Success)
    {
        code = NERR_UserNotFound;
        if (p3) {
            if (p3->usri3_profile) {
                DWORD len = lstrlenW(p3->usri3_profile);
                if (len > 0) {
                    /* Convert From Unicode to ANSI (UTF-8 for future) */
                    len = len < profilePathLen ? len : profilePathLen - 1;
                    WideCharToMultiByte(CP_UTF8, 0, p3->usri3_profile, len, profilePath, len, NULL, NULL);
                    profilePath[len] = '\0';
                    code = NERR_Success;
                }
            }
            NetApiBufferFree(p3);
        }
    }
    if (ServerName) 
        NetApiBufferFree(ServerName);
    return code;
}

BOOL IsServiceRunning (void)
{
    SERVICE_STATUS Status;
    SC_HANDLE hManager;
    memset (&Status, 0x00, sizeof(Status));
    Status.dwCurrentState = SERVICE_STOPPED;

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
    DebugEvent("AFS AfsLogon - Test Service Running Return Code[%x] ?Running[%d]",Status.dwCurrentState,(Status.dwCurrentState == SERVICE_RUNNING));
    return (Status.dwCurrentState == SERVICE_RUNNING);
}   

BOOL IsServiceStartPending (void)
{
    SERVICE_STATUS Status;
    SC_HANDLE hManager;
    memset (&Status, 0x00, sizeof(Status));
    Status.dwCurrentState = SERVICE_STOPPED;

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
    DebugEvent("AFS AfsLogon - Test Service Start Pending Return Code[%x] ?Start Pending[%d]",Status.dwCurrentState,(Status.dwCurrentState == SERVICE_START_PENDING));
    return (Status.dwCurrentState == SERVICE_START_PENDING);
}   

/* LOOKUPKEYCHAIN: macro to look up the value in the list of keys in order until it's found
   v:variable to receive value (reference type)
   t:type
   d:default, in case the value isn't on any of the keys
   n:name of value */
#define LOOKUPKEYCHAIN(v,t,d,n) \
	do { \
		rv = ~ERROR_SUCCESS; \
		dwType = t; \
		if(hkDom) { \
			dwSize = sizeof(v); \
			rv = RegQueryValueEx(hkDom, n, 0, &dwType, (LPBYTE) &(v), &dwSize); \
			if(rv == ERROR_SUCCESS) DebugEvent(#v " found in hkDom with type [%d]", dwType); \
		} \
		if(hkDoms && (rv != ERROR_SUCCESS || dwType != t)) { \
			dwSize = sizeof(v); \
			rv = RegQueryValueEx(hkDoms, n, 0, &dwType, (LPBYTE) &(v), &dwSize); \
			if(rv == ERROR_SUCCESS) DebugEvent(#v " found in hkDoms with type [%d]", dwType); \
		} \
		if(hkNp && (rv != ERROR_SUCCESS || dwType != t)) { \
			dwSize = sizeof(v); \
			rv = RegQueryValueEx(hkNp, n, 0, &dwType, (LPBYTE) &(v), &dwSize); \
			if(rv == ERROR_SUCCESS) DebugEvent(#v " found in hkNp with type [%d]", dwType); \
		} \
		if(rv != ERROR_SUCCESS || dwType != t) { \
			v = d; \
			DebugEvent(#v " being set to default"); \
		} \
	} while(0)

/* Get domain specific configuration info.  We are returning void because if anything goes wrong
   we just return defaults.
 */
void 
GetDomainLogonOptions( PLUID lpLogonId, char * username, char * domain, LogonOptions_t *opt ) {
    HKEY hkParm = NULL; /* Service parameter */
    HKEY hkNp = NULL;   /* network provider key */
    HKEY hkDoms = NULL; /* domains key */
    HKEY hkDom = NULL;  /* DOMAINS/domain key */
    HKEY hkTemp = NULL;
    LONG rv;
    DWORD dwSize;
    DWORD dwType;
    DWORD dwDummy;
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    char *effDomain;

    memset(opt, 0, sizeof(LogonOptions_t));

    DebugEvent("In GetDomainLogonOptions for user [%s] in domain [%s]", username, domain);
    /* If the domain is the same as the Netbios computer name, we use the LOCALHOST domain name*/
    opt->flags = LOGON_FLAG_REMOTE;
    if(domain) {
        dwSize = MAX_COMPUTERNAME_LENGTH;
        if(GetComputerName(computerName, &dwSize)) {
            if(!stricmp(computerName, domain)) {
                effDomain = "LOCALHOST";
                opt->flags = LOGON_FLAG_LOCAL;
            }
            else
                effDomain = domain;
        }
    } else
        effDomain = NULL;

    rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 0, KEY_READ, &hkParm );
    if(rv != ERROR_SUCCESS) {
        hkParm = NULL;
        DebugEvent("GetDomainLogonOption: Can't open parms key [%d]", rv);
    }

    rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PROVIDER_SUBKEY, 0, KEY_READ, &hkNp );
    if(rv != ERROR_SUCCESS) {
        hkNp = NULL;
        DebugEvent("GetDomainLogonOptions: Can't open NP key [%d]", rv);
    }

    if(hkNp) {
        rv = RegOpenKeyEx( hkNp, REG_CLIENT_DOMAINS_SUBKEY, 0, KEY_READ, &hkDoms );
        if( rv != ERROR_SUCCESS ) {
            hkDoms = NULL;
            DebugEvent("GetDomainLogonOptions: Can't open Domains key [%d]", rv);
        }
    }

    if(hkDoms && effDomain) {
        rv = RegOpenKeyEx( hkDoms, effDomain, 0, KEY_READ, &hkDom );
        if( rv != ERROR_SUCCESS ) {
            hkDom = NULL;
            DebugEvent("GetDomainLogonOptions: Can't open domain key for [%s] [%d]", effDomain, rv);
            /* If none of the domains match, we shouldn't use the domain key either */
            RegCloseKey(hkDoms);
            hkDoms = NULL;
        }
    } else
        DebugEvent("Not opening domain key for [%s]", effDomain);

    /* Each individual can either be specified on the domain key, the domains key or in the
       net provider key.  They fail over in that order.  If none is found, we just use the 
       defaults. */

    /* LogonOption */
    LOOKUPKEYCHAIN(opt->LogonOption, REG_DWORD, DEFAULT_LOGON_OPTION, REG_CLIENT_LOGON_OPTION_PARM);

    /* FailLoginsSilently */
    dwSize = sizeof(dwDummy);
    rv = RegQueryValueEx(hkParm, REG_CLIENT_FAIL_SILENTLY_PARM, 0, &dwType, (LPBYTE) &dwDummy, &dwSize);
    if (rv != ERROR_SUCCESS)
        LOOKUPKEYCHAIN(dwDummy, REG_DWORD, DEFAULT_FAIL_SILENTLY, REG_CLIENT_FAIL_SILENTLY_PARM);
    opt->failSilently = !!dwDummy;

    /* Retry interval */
    LOOKUPKEYCHAIN(opt->retryInterval, REG_DWORD, DEFAULT_RETRY_INTERVAL, REG_CLIENT_RETRY_INTERVAL_PARM);

    /* Sleep interval */
    LOOKUPKEYCHAIN(opt->sleepInterval, REG_DWORD, DEFAULT_SLEEP_INTERVAL, REG_CLIENT_SLEEP_INTERVAL_PARM);

    opt->logonScript = NULL;
    opt->smbName = NULL;

    if(!ISLOGONINTEGRATED(opt->LogonOption)) {
        goto cleanup; /* no need to lookup the logon script */
    }

    /* come up with SMB username */
    if(ISHIGHSECURITY(opt->LogonOption)) {
        opt->smbName = malloc( MAXRANDOMNAMELEN );
        GenRandomName(opt->smbName);
    } else if (lpLogonId) {
        /* username and domain for logon session is not necessarily the same as
           username and domain passed into network provider. */
        PSECURITY_LOGON_SESSION_DATA plsd;
        char lsaUsername[MAX_USERNAME_LENGTH];
        char lsaDomain[MAX_DOMAIN_LENGTH];
        size_t len, tlen;

        LsaGetLogonSessionData(lpLogonId, &plsd);
        
        UnicodeStringToANSI(plsd->UserName, lsaUsername, MAX_USERNAME_LENGTH);
        UnicodeStringToANSI(plsd->LogonDomain, lsaDomain, MAX_DOMAIN_LENGTH);

        DebugEvent("PLSD username[%s] domain[%s]",lsaUsername,lsaDomain);

        if(SUCCEEDED(StringCbLength(lsaUsername, MAX_USERNAME_LENGTH, &tlen)))
            len = tlen;
        else
            goto bad_strings;

        if(SUCCEEDED(StringCbLength(lsaDomain, MAX_DOMAIN_LENGTH, &tlen)))
            len += tlen;
        else
            goto bad_strings;

        len += 2;

        opt->smbName = malloc(len);

        StringCbCopy(opt->smbName, len, lsaDomain);
        StringCbCat(opt->smbName, len, "\\");
        StringCbCat(opt->smbName, len, lsaUsername);

        strlwr(opt->smbName);

      bad_strings:
        LsaFreeReturnBuffer(plsd);
    } else {
        size_t len;

        DebugEvent("No LUID given. Constructing username using [%s] and [%s]",
                   username, domain);
 
        len = strlen(username) + strlen(domain) + 2;

        opt->smbName = malloc(len);

        StringCbCopy(opt->smbName, len, username);
        StringCbCat(opt->smbName, len, "\\");
        StringCbCat(opt->smbName, len, domain);

        strlwr(opt->smbName);
    }

    DebugEvent("Looking up logon script");
    /* Logon script */
    /* First find out where the key is */
    hkTemp = NULL;
    rv = ~ERROR_SUCCESS;
    dwType = 0;
    if(hkDom)
        rv = RegQueryValueExW(hkDom, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
    if(rv == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
        hkTemp = hkDom;
        DebugEvent("Located logon script in hkDom");
    }
    else if(hkDoms)
        rv = RegQueryValueExW(hkDoms, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
    if(rv == ERROR_SUCCESS && !hkTemp && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
        hkTemp = hkDoms;
        DebugEvent("Located logon script in hkDoms");
    }
    /* Note that the LogonScript in the NP key is only used if we are doing high security. */
    else if(hkNp && ISHIGHSECURITY(opt->LogonOption))
        rv = RegQueryValueExW(hkNp, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
    if(rv == ERROR_SUCCESS && !hkTemp && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
        hkTemp = hkNp;
        DebugEvent("Located logon script in hkNp");
    }

    if(hkTemp) {
        WCHAR *regscript	= NULL;
        WCHAR *regexscript	= NULL;
        WCHAR *regexuscript	= NULL;
        WCHAR *wuname		= NULL;
        HRESULT hr;

        size_t len;

        StringCbLength(opt->smbName, MAX_USERNAME_LENGTH, &len);
        len ++;

        wuname = malloc(len * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP,0,opt->smbName,-1,wuname,(int)(len*sizeof(WCHAR)));

        DebugEvent("Username is set for [%S]", wuname);

        /* dwSize still has the size of the required buffer in bytes. */
        regscript = malloc(dwSize);
        rv = RegQueryValueExW(hkTemp, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, (LPBYTE) regscript, &dwSize);
        if(rv != ERROR_SUCCESS) {/* what the ..? */
            DebugEvent("Can't look up logon script [%d]",rv);
            goto doneLogonScript;
        }

        DebugEvent("Found logon script [%S]", regscript);

        if(dwType == REG_EXPAND_SZ) {
            DWORD dwReq;

            dwSize += MAX_PATH * sizeof(WCHAR);  /* make room for environment expansion. */
            regexscript = malloc(dwSize);
            dwReq = ExpandEnvironmentStringsW(regscript, regexscript, dwSize / sizeof(WCHAR));
            free(regscript);
            regscript = regexscript;
            regexscript = NULL;
            if(dwReq > (dwSize / sizeof(WCHAR))) {
                DebugEvent("Overflow while expanding environment strings.");
                goto doneLogonScript;
            }
        }

        DebugEvent("After expanding env strings [%S]", regscript);

        if(wcsstr(regscript, L"%s")) {
            dwSize += (DWORD)(len * sizeof(WCHAR)); /* make room for username expansion */
            regexuscript = (WCHAR *) LocalAlloc(LMEM_FIXED, dwSize);
            hr = StringCbPrintfW(regexuscript, dwSize, regscript, wuname);
        } else {
            regexuscript = (WCHAR *) LocalAlloc(LMEM_FIXED, dwSize);
            hr = StringCbCopyW(regexuscript, dwSize, regscript);
        }

        DebugEvent("After expanding username [%S]", regexuscript);

        if(hr == S_OK)
            opt->logonScript = regexuscript;
        else
            LocalFree(regexuscript);

      doneLogonScript:
        if(wuname) free(wuname);
        if(regscript) free(regscript);
        if(regexscript) free(regexscript);
    }

    DebugEvent("Looking up TheseCells");
    /* Logon script */
    /* First find out where the key is */
    hkTemp = NULL;
    rv = ~ERROR_SUCCESS;
    dwType = 0;
    if (hkDom)
        rv = RegQueryValueEx(hkDom, REG_CLIENT_THESE_CELLS_PARM, 0, &dwType, NULL, &dwSize);
    if (rv == ERROR_SUCCESS && dwType == REG_MULTI_SZ) {
        hkTemp = hkDom;
        DebugEvent("Located TheseCells in hkDom");
    } else if (hkDoms)
        rv = RegQueryValueEx(hkDoms, REG_CLIENT_THESE_CELLS_PARM, 0, &dwType, NULL, &dwSize);
    if (rv == ERROR_SUCCESS && !hkTemp && dwType == REG_MULTI_SZ) {
        hkTemp = hkDoms;
        DebugEvent("Located TheseCells in hkDoms");
    } else if (hkNp)
        rv = RegQueryValueEx(hkNp, REG_CLIENT_THESE_CELLS_PARM, 0, &dwType, NULL, &dwSize);
    if (rv == ERROR_SUCCESS && !hkTemp && dwType == REG_MULTI_SZ) {
        hkTemp = hkNp;
        DebugEvent("Located TheseCells in hkNp");
    }

    if (hkTemp) {
        CHAR * thesecells;

        /* dwSize still has the size of the required buffer in bytes. */
        thesecells = malloc(dwSize);
        rv = RegQueryValueEx(hkTemp, REG_CLIENT_THESE_CELLS_PARM, 0, &dwType, (LPBYTE) thesecells, &dwSize);
        if(rv != ERROR_SUCCESS) {/* what the ..? */
            DebugEvent("Can't look up TheseCells [%d]",rv);
            goto doneTheseCells;
        }

        DebugEvent("Found TheseCells [%s]", thesecells);
        opt->theseCells = thesecells;

      doneTheseCells:
        ;
    }

  cleanup:
    if(hkNp) RegCloseKey(hkNp);
    if(hkDom) RegCloseKey(hkDom);
    if(hkDoms) RegCloseKey(hkDoms);
    if(hkParm) RegCloseKey(hkParm);
}       

#undef LOOKUPKEYCHAIN

/* Try to find out which cell the given path is in.  We must retain
   the contents of *cell in case of failure. *cell is assumed to be
   at least cellLen chars */
DWORD GetFileCellName(char * path, char * cell, size_t cellLen) {
    struct ViceIoctl blob;
    char tcell[MAX_PATH];
    DWORD code;

    blob.in_size = 0;
    blob.out_size = MAX_PATH;
    blob.out = tcell;

    code = pioctl(path, VIOC_FILE_CELL_NAME, &blob, 1);

    if(!code) {
        strncpy(cell, tcell, cellLen);
        cell[cellLen - 1] = '\0';
    }
    return code;
}       


static BOOL
WINAPI
UnicodeStringToANSI(UNICODE_STRING uInputString, LPSTR lpszOutputString, int nOutStringLen)
{
    CPINFO CodePageInfo;

    GetCPInfo(CP_ACP, &CodePageInfo);

    if (CodePageInfo.MaxCharSize > 1)
        // Only supporting non-Unicode strings
        return FALSE;
    
    if (uInputString.Buffer && ((LPBYTE) uInputString.Buffer)[1] == '\0')
    {
        // Looks like unicode, better translate it
        // UNICODE_STRING specifies the length of the buffer string in Bytes not WCHARS
        WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) uInputString.Buffer, uInputString.Length/2,
                            lpszOutputString, nOutStringLen-1, NULL, NULL);
        lpszOutputString[min(uInputString.Length/2,nOutStringLen-1)] = '\0';
        return TRUE;
    }
    else
        lpszOutputString[0] = '\0';
    return FALSE;
}  // UnicodeStringToANSI

DWORD APIENTRY NPLogonNotify(
	PLUID lpLogonId,
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	LPWSTR *lpLogonScript)
{
    char uname[MAX_USERNAME_LENGTH]="";
    char password[MAX_PASSWORD_LENGTH]="";
    char logonDomain[MAX_DOMAIN_LENGTH]="";
    char cell[256]="<non-integrated logon>";
    char homePath[MAX_PATH]="";
    char szLogonId[128] = "";

    MSV1_0_INTERACTIVE_LOGON *IL;

    DWORD code = 0, code2;

    int pw_exp;
    char *reason;
    char *ctemp;

    BOOLEAN interactive;
    BOOLEAN flag;
    DWORD LSPtype, LSPsize;
    HKEY NPKey;

    HWND hwndOwner = (HWND)StationHandle;

    BOOLEAN afsWillAutoStart;

    BOOLEAN lowercased_name = TRUE;

    LogonOptions_t opt; /* domain specific logon options */
    int retryInterval;
    int sleepInterval;

    (void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &NPKey);
    LSPsize=sizeof(TraceOption);
    RegQueryValueEx(NPKey, REG_CLIENT_TRACE_OPTION_PARM, NULL,
                     &LSPtype, (LPBYTE)&TraceOption, &LSPsize);

    RegCloseKey (NPKey);

    DebugEvent("NPLogonNotify - LoginId(%d,%d)", lpLogonId->HighPart, lpLogonId->LowPart);

    /* Make sure the AFS Libraries are initialized */
    AfsLogonInit();

    /* Initialize Logon Script to none */
    *lpLogonScript=NULL;
    
    /* MSV1_0_INTERACTIVE_LOGON and KERB_INTERACTIVE_LOGON are equivalent for
     * our purposes */

    if ( wcscmp(lpAuthentInfoType,L"MSV1_0:Interactive") && 
         wcscmp(lpAuthentInfoType,L"Kerberos:Interactive") )
    {
        DebugEvent("Unsupported Authentication Info Type: %S",
                   lpAuthentInfoType);
        return 0;
    }

    IL = (MSV1_0_INTERACTIVE_LOGON *) lpAuthentInfo;

    /* Are we interactive? */
    interactive = (wcscmp(lpStationName, L"WinSta0") == 0);

    /* Convert from Unicode to ANSI */

    /*TODO: Use SecureZeroMemory to erase passwords */
    UnicodeStringToANSI(IL->UserName, uname, MAX_USERNAME_LENGTH);
    UnicodeStringToANSI(IL->Password, password, MAX_PASSWORD_LENGTH);
    UnicodeStringToANSI(IL->LogonDomainName, logonDomain, MAX_DOMAIN_LENGTH);

    /* Make sure AD-DOMANS sent from login that is sent to us is striped */
    ctemp = strchr(uname, '@');
    if (ctemp) *ctemp = 0;

    /* is the name all lowercase? */
    for ( ctemp = uname; *ctemp ; ctemp++) {
        if ( !islower(*ctemp) ) {
            lowercased_name = FALSE;
            break;
        }
    }

    /*
     * Get Logon options
     */

    GetDomainLogonOptions( lpLogonId, uname, logonDomain, &opt );
    retryInterval = opt.retryInterval;
    sleepInterval = opt.sleepInterval;
    *lpLogonScript = opt.logonScript;

    if (retryInterval < sleepInterval)
        sleepInterval = retryInterval;

    DebugEvent("Got logon script: %S",opt.logonScript);

    afsWillAutoStart = AFSWillAutoStart();

    DebugEvent("LogonOption[%x], Service AutoStart[%d]",
                opt.LogonOption,afsWillAutoStart);
    
    /* Check for zero length password if integrated logon*/
    if ( ISLOGONINTEGRATED(opt.LogonOption) )  {
        if ( password[0] == 0 ) {
            DebugEvent("Password is the empty string");
            code = GT_PW_NULL;
            reason = "zero length password is illegal";
            code=0;
        }

        /* Get cell name if doing integrated logon.  
           We might overwrite this if we are logging into an AD realm and we find out that
           the user's home dir is in some other cell. */
        DebugEvent("About to call cm_GetRootCellName(%s)",cell);
        code = cm_GetRootCellName(cell);
        if (code < 0) { 
            DebugEvent("Unable to obtain Root Cell");
            code = KTC_NOCELL;
            reason = "unknown cell";
            code=0;
        } else {
            DebugEvent("Cell is %s",cell);
        }       

        /* We get the user's home directory path, if applicable, though we can't lookup the
           cell right away because the client service may not have started yet. This call
           also sets the AD_REALM flag in opt.flags if applicable. */
        if (ISREMOTE(opt.flags)) {
            DebugEvent("Is Remote");
            GetAdHomePath(homePath,MAX_PATH,lpLogonId,&opt);
        }
    }

    /* loop until AFS is started. */
    if (afsWillAutoStart) {
	while (IsServiceRunning() || IsServiceStartPending()) {
	    DebugEvent("while(autostart) LogonOption[%x], Service AutoStart[%d]",
			opt.LogonOption,afsWillAutoStart);

	    if (ISADREALM(opt.flags)) {
		code = GetFileCellName(homePath,cell,256);
		if (!code) {
		    DebugEvent("profile path [%s] is in cell [%s]",homePath,cell);
		}
		/* Don't bail out if GetFileCellName failed.
		 * The home dir may not be in AFS after all. 
		 */
	    } else
		code=0;
		
	    /* if Integrated Logon  */
	    if (ISLOGONINTEGRATED(opt.LogonOption))
	    {			
		if ( KFW_is_available() ) {
		    code = KFW_AFS_get_cred(uname, cell, password, 0, opt.smbName, &reason);
		    DebugEvent("KFW_AFS_get_cred  uname=[%s] smbname=[%s] cell=[%s] code=[%d]",
				uname,opt.smbName,cell,code);
		    if (code == 0 && opt.theseCells) { 
			char * principal, *p;
			size_t len, tlen;

			StringCchLength(cell, MAX_DOMAIN_LENGTH, &tlen);
			len = tlen;
			StringCchLength(uname, MAX_USERNAME_LENGTH, &tlen);
			len += tlen + 2;

			/* tlen is now the length of uname in characters */
			principal = (char *)malloc(len * sizeof(char));
			if ( principal ) {
			    StringCchCopy(principal, len, uname);
			    p = principal + tlen;
			    *p++ = '@';
			    StringCchCopy(p, len - tlen - 1, cell);
			    for ( ;*p; p++) {
				*p = toupper(*p);
			    }

			    p = opt.theseCells;
			    while ( *p ) {
				code2 = KFW_AFS_get_cred(principal, p, 0, 0, opt.smbName, &reason);
				DebugEvent("KFW_AFS_get_cred  uname=[%s] smbname=[%s] cell=[%s] code=[%d]",
					    principal,opt.smbName,p,code2);
				p += strlen(p) + 1;
			    }
			    free(principal);
			}
		    }
		} else {
		    code = ka_UserAuthenticateGeneral2(KA_USERAUTH_VERSION+KA_USERAUTH_AUTHENT_LOGON,
							uname, "", cell, password, opt.smbName, 0, &pw_exp, 0,
							&reason);
		    DebugEvent("AFS AfsLogon - (INTEGRATED only)ka_UserAuthenticateGeneral2 Code[%x] uname[%s] smbname=[%s] Cell[%s] PwExp=[%d] Reason=[%s]",
				code,uname,opt.smbName,cell,pw_exp,reason?reason:"");
		}       
		if ( code && code != KTC_NOCM && code != KTC_NOCMRPC && !lowercased_name ) {
		    for ( ctemp = uname; *ctemp ; ctemp++) {
			*ctemp = tolower(*ctemp);
		    }
		    lowercased_name = TRUE;
		    goto sleeping;
		}

		/* is service started yet?*/

		/* If we've failed because the client isn't running yet and the
		 * client is set to autostart (and therefore it makes sense for
		 * us to wait for it to start) then sleep a while and try again. 
		 * If the error was something else, then give up. */
		if (code != KTC_NOCM && code != KTC_NOCMRPC)
		    break;
	    }
	    else {  
		/*JUST check to see if its running*/
		if (IsServiceRunning())
		    break;
		if (!IsServiceStartPending()) {
		    code = KTC_NOCMRPC;
		    reason = "AFS Service start failed";
		    break;
		}
	    }

	    /* If the retry interval has expired and we still aren't
	     * logged in, then just give up if we are not in interactive
	     * mode or the failSilently flag is set, otherwise let the
	     * user know we failed and give them a chance to try again. */
	    if (retryInterval <= 0) {
		reason = "AFS not running";
		if (!interactive || opt.failSilently)
		    break;
		flag = MessageBox(hwndOwner,
				   "AFS is still starting.  Retry?",
				   "AFS Logon",
				   MB_ICONQUESTION | MB_RETRYCANCEL);
		if (flag == IDCANCEL)
		    break;

		/* Wait just a little while and try again */
		retryInterval = opt.retryInterval;
	    }

	  sleeping:
	    Sleep(sleepInterval * 1000);
	    retryInterval -= sleepInterval;
	}
    }
    DebugEvent("while loop exited");
    /* remove any kerberos 5 tickets currently held by the SYSTEM account
     * for this user 
     */
    if (ISLOGONINTEGRATED(opt.LogonOption) && KFW_is_available()) {
	sprintf(szLogonId,"%d.%d",lpLogonId->HighPart, lpLogonId->LowPart);
	KFW_AFS_copy_cache_to_system_file(uname, szLogonId);

	KFW_AFS_destroy_tickets_for_principal(uname);
    }

    if (code) {
	char msg[128];
	HANDLE h;
	char *ptbuf[1];

	StringCbPrintf(msg, sizeof(msg), "Integrated login failed: %s", reason);

	if (ISLOGONINTEGRATED(opt.LogonOption) && interactive && !opt.failSilently)
	    MessageBox(hwndOwner, msg, "AFS Logon", MB_OK);

	h = RegisterEventSource(NULL, AFS_LOGON_EVENT_NAME);
	ptbuf[0] = msg;
	ReportEvent(h, EVENTLOG_WARNING_TYPE, 0, 1008, NULL,
		     1, 0, ptbuf, NULL);
	DeregisterEventSource(h);
	    
        code = MapAuthError(code);
        SetLastError(code);

        if (ISLOGONINTEGRATED(opt.LogonOption) && (code!=0))
        {
            if (*lpLogonScript)
                LocalFree(*lpLogonScript);
            *lpLogonScript = NULL;
            if (!afsWillAutoStart)	// its not running, so if not autostart or integrated logon then just skip
                code = 0;
        }
    }

    if (opt.theseCells) free(opt.theseCells);
    if (opt.smbName) free(opt.smbName);

    DebugEvent("AFS AfsLogon - Exit","Return Code[%x]",code);
    return code;
}       

DWORD APIENTRY NPPasswordChangeNotify(
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	DWORD dwChangeInfo)
{
    /* Make sure the AFS Libraries are initialized */
    AfsLogonInit();

    DebugEvent0("AFS AfsLogon - NPPasswordChangeNotify");
    return 0;
}

#include <userenv.h>
#include <Winwlx.h>
#include <afs/vice.h>
#include <afs/fs_utils.h>

BOOL IsPathInAfs(const CHAR *strPath)
{
    char space[2048];
    struct ViceIoctl blob;
    int code;

    blob.in_size = 0;
    blob.out_size = 2048;
    blob.out = space;

    code = pioctl((LPTSTR)((LPCTSTR)strPath), VIOC_FILE_CELL_NAME, &blob, 1);
    if (code)
        return FALSE;
    return TRUE;
}

#ifdef COMMENT
typedef struct _WLX_NOTIFICATION_INFO {  
    ULONG Size;  
    ULONG Flags;  
    PWSTR UserName;  
    PWSTR Domain;  
    PWSTR WindowStation;  
    HANDLE hToken;  
    HDESK hDesktop;  
    PFNMSGECALLBACK pStatusCallback;
} WLX_NOTIFICATION_INFO, *PWLX_NOTIFICATION_INFO;
#endif

VOID AFS_Startup_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    DWORD LSPtype, LSPsize;
    HKEY NPKey;

    /* Make sure the AFS Libraries are initialized */
    AfsLogonInit();

    (void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                        0, KEY_QUERY_VALUE, &NPKey);
    LSPsize=sizeof(TraceOption);
    RegQueryValueEx(NPKey, REG_CLIENT_TRACE_OPTION_PARM, NULL,
                     &LSPtype, (LPBYTE)&TraceOption, &LSPsize);

    RegCloseKey (NPKey);
    DebugEvent0("AFS_Startup_Event");
}

VOID AFS_Logoff_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    DWORD code;
    TCHAR profileDir[1024] = TEXT("");
    DWORD  len = 1024;
    PTOKEN_USER  tokenUser = NULL;
    DWORD  retLen;
    DWORD LSPtype, LSPsize;
    HKEY NPKey;
    DWORD LogoffPreserveTokens = 0;
    LogonOptions_t opt;

    /* Make sure the AFS Libraries are initialized */
    AfsLogonInit();

    DebugEvent0("AFS_Logoff_Event - Start");

    (void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &NPKey);
    LSPsize=sizeof(LogoffPreserveTokens);
    RegQueryValueEx(NPKey, REG_CLIENT_LOGOFF_TOKENS_PARM, NULL,
                     &LSPtype, (LPBYTE)&LogoffPreserveTokens, &LSPsize);
    RegCloseKey (NPKey);

    if (!LogoffPreserveTokens) {
	memset(&opt, 0, sizeof(LogonOptions_t));

	if (pInfo->UserName && pInfo->Domain) {
	    char username[MAX_USERNAME_LENGTH] = "";
	    char domain[MAX_DOMAIN_LENGTH] = "";
	    size_t szlen = 0;

	    StringCchLengthW(pInfo->UserName, MAX_USERNAME_LENGTH, &szlen);
	    WideCharToMultiByte(CP_UTF8, 0, pInfo->UserName, szlen,
				 username, sizeof(username), NULL, NULL);

	    StringCchLengthW(pInfo->Domain, MAX_DOMAIN_LENGTH, &szlen);
	    WideCharToMultiByte(CP_UTF8, 0, pInfo->Domain, szlen,
				 domain, sizeof(domain), NULL, NULL);

	    GetDomainLogonOptions(NULL, username, domain, &opt);
	}

        if (ISREMOTE(opt.flags)) {
	    if (!GetTokenInformation(pInfo->hToken, TokenUser, NULL, 0, &retLen))
	    {
		if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
		    tokenUser = (PTOKEN_USER) LocalAlloc(LPTR, retLen);

		    if (!GetTokenInformation(pInfo->hToken, TokenUser, tokenUser, retLen, &retLen))
		    {
			DebugEvent("AFS_Logoff_Event - GetTokenInformation failed: GLE = %lX", GetLastError());
		    }
		}
	    }

	    /* We can't use pInfo->Domain for the domain since in the cross realm case 
	     * this is source domain and not the destination domain.
	     */
	    if (QueryAdHomePathFromSid( profileDir, sizeof(profileDir), tokenUser->User.Sid, pInfo->Domain)) {
		WCHAR Domain[64]=L"";
		GetLocalShortDomain(Domain, sizeof(Domain));
		if (QueryAdHomePathFromSid( profileDir, sizeof(profileDir), tokenUser->User.Sid, Domain)) {
		    if (NetUserGetProfilePath(pInfo->Domain, pInfo->UserName, profileDir, len))
			GetUserProfileDirectory(pInfo->hToken, profileDir, &len);
		}
	    }

	    if (strlen(profileDir)) {
		DebugEvent("AFS_Logoff_Event - Profile Directory: %s", profileDir);
		if (!IsPathInAfs(profileDir)) {
		    if (code = ktc_ForgetAllTokens())
			DebugEvent("AFS_Logoff_Event - ForgetAllTokens failed [%lX]",code);
		    else
			DebugEvent0("AFS_Logoff_Event - ForgetAllTokens succeeded");
		} else {
		    DebugEvent0("AFS_Logoff_Event - Tokens left in place; profile in AFS");
		}
	    } else {
		DebugEvent0("AFS_Logoff_Event - Unable to load profile");
	    }

	    if ( tokenUser )
		LocalFree(tokenUser);
	} else {
	    DebugEvent0("AFS_Logoff_Event - Local Logon");
	    if (code = ktc_ForgetAllTokens())
		DebugEvent("AFS_Logoff_Event - ForgetAllTokens failed [%lX]",code);
	    else
		DebugEvent0("AFS_Logoff_Event - ForgetAllTokens succeeded");
	}
    } else {
	DebugEvent0("AFS_Logoff_Event - Preserving Tokens");
    }

    DebugEvent0("AFS_Logoff_Event - End");
}   

VOID AFS_Logon_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    TCHAR profileDir[1024] = TEXT("");
    DWORD  len = 1024;
    PTOKEN_USER  tokenUser = NULL;
    DWORD  retLen;
    WCHAR szUserW[128] = L"";
    char  szUserA[128] = "";
    char  szClient[MAX_PATH];
    char szPath[MAX_PATH] = "";
    NETRESOURCE nr;
    DWORD res;
    DWORD dwSize;
    LogonOptions_t opt;

    /* Make sure the AFS Libraries are initialized */
    AfsLogonInit();

    DebugEvent0("AFS_Logon_Event - Start");

    DebugEvent("AFS_Logon_Event Process ID: %d",GetCurrentProcessId());

    memset(&opt, 0, sizeof(LogonOptions_t));

    if (pInfo->UserName && pInfo->Domain) {
        char username[MAX_USERNAME_LENGTH] = "";
        char domain[MAX_DOMAIN_LENGTH] = "";
        size_t szlen = 0;

	DebugEvent0("AFS_Logon_Event - pInfo UserName and Domain");

        StringCchLengthW(pInfo->UserName, MAX_USERNAME_LENGTH, &szlen);
        WideCharToMultiByte(CP_UTF8, 0, pInfo->UserName, szlen,
                            username, sizeof(username), NULL, NULL);
        
        StringCchLengthW(pInfo->Domain, MAX_DOMAIN_LENGTH, &szlen);
        WideCharToMultiByte(CP_UTF8, 0, pInfo->Domain, szlen,
                            domain, sizeof(domain), NULL, NULL);

	DebugEvent0("AFS_Logon_Event - Calling GetDomainLogonOptions");
        GetDomainLogonOptions(NULL, username, domain, &opt);
    } else {
	if (!pInfo->UserName)
	    DebugEvent0("AFS_Logon_Event - No pInfo->UserName");
	if (!pInfo->Domain)
	    DebugEvent0("AFS_Logon_Event - No pInfo->Domain");
    }

    DebugEvent("AFS_Logon_Event - opt.LogonOption = %lX opt.flags = %lX", 
		opt.LogonOption, opt.flags);

    if (!ISLOGONINTEGRATED(opt.LogonOption) || !ISREMOTE(opt.flags)) {
        DebugEvent0("AFS_Logon_Event - Logon is not integrated or not remote");
        goto done_logon_event;
    }

    DebugEvent0("AFS_Logon_Event - Calling GetTokenInformation");

    if (!GetTokenInformation(pInfo->hToken, TokenUser, NULL, 0, &retLen))
    {
        if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
            tokenUser = (PTOKEN_USER) LocalAlloc(LPTR, retLen);

            if (!GetTokenInformation(pInfo->hToken, TokenUser, tokenUser, retLen, &retLen))
            {
                DebugEvent("AFS_Logon_Event - GetTokenInformation failed: GLE = %lX", GetLastError());
            }
        }
    }

    /* We can't use pInfo->Domain for the domain since in the cross realm case 
     * this is source domain and not the destination domain.
     */
    if (QueryAdHomePathFromSid( profileDir, sizeof(profileDir), tokenUser->User.Sid, pInfo->Domain)) {
        WCHAR Domain[64]=L"";
        GetLocalShortDomain(Domain, sizeof(Domain));
        if (QueryAdHomePathFromSid( profileDir, sizeof(profileDir), tokenUser->User.Sid, Domain)) {
            if (NetUserGetProfilePath(pInfo->Domain, pInfo->UserName, profileDir, len))
                GetUserProfileDirectory(pInfo->hToken, profileDir, &len);
        }
    }
    
    if (strlen(profileDir)) {
        DebugEvent("AFS_Logon_Event - Profile Directory: %s", profileDir);
    } else {
        DebugEvent0("AFS_Logon_Event - Unable to load profile");
    }

  done_logon_event:
    dwSize = sizeof(szUserA);
    if (!KFW_AFS_get_lsa_principal(szUserA, &dwSize)) {
        StringCbPrintfW(szUserW, sizeof(szUserW), L"%s\\%s", pInfo->Domain, pInfo->UserName);
        WideCharToMultiByte(CP_ACP, 0, szUserW, -1, szUserA, MAX_PATH, NULL, NULL);
    }

    if (szUserA[0])
    {
        lana_GetNetbiosName(szClient, LANA_NETBIOS_NAME_FULL);
        StringCbPrintf(szPath, sizeof(szPath), "\\\\%s", szClient);

        DebugEvent("AFS_Logon_Event - Logon Name: %s", szUserA);

        memset (&nr, 0x00, sizeof(NETRESOURCE));
        nr.dwType=RESOURCETYPE_DISK;
        nr.lpLocalName=0;
        nr.lpRemoteName=szPath;
        res = WNetAddConnection2(&nr,NULL,szUserA,0);
        if (res)
            DebugEvent("AFS_Logon_Event - WNetAddConnection2(%s,%s) failed: 0x%X",
                        szPath, szUserA,res);
        else
            DebugEvent0("AFS_Logon_Event - WNetAddConnection2() succeeded");
    } else 
        DebugEvent("AFS_Logon_Event - User name conversion failed: GLE = 0x%X",GetLastError());

    if ( tokenUser )
        LocalFree(tokenUser);

    DebugEvent0("AFS_Logon_Event - End");
}

static BOOL
GetSecurityLogonSessionData(HANDLE hToken, PSECURITY_LOGON_SESSION_DATA * ppSessionData)
{
    NTSTATUS Status = 0;
    TOKEN_STATISTICS Stats;
    DWORD   ReqLen;
    BOOL    Success;

    if (!ppSessionData)
        return FALSE;
    *ppSessionData = NULL;

    Success = GetTokenInformation( hToken, TokenStatistics, &Stats, sizeof(TOKEN_STATISTICS), &ReqLen );
    if ( !Success )
        return FALSE;

    Status = LsaGetLogonSessionData( &Stats.AuthenticationId, ppSessionData );
    if ( FAILED(Status) || !ppSessionData )
        return FALSE;

    return TRUE;
}

VOID KFW_Logon_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    WCHAR szUserW[128] = L"";
    char  szUserA[128] = "";
    char szPath[MAX_PATH] = "";
    char szLogonId[128] = "";
    DWORD count;
    char filename[MAX_PATH];
    char newfilename[MAX_PATH];
    char commandline[MAX_PATH+256];
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;

    LUID LogonId = {0, 0};
    PSECURITY_LOGON_SESSION_DATA pLogonSessionData = NULL;

    HKEY hKey1 = NULL, hKey2 = NULL;

    /* Make sure the KFW Libraries are initialized */
    AfsLogonInit();

    DebugEvent0("KFW_Logon_Event - Start");

    GetSecurityLogonSessionData( pInfo->hToken, &pLogonSessionData );

    if ( pLogonSessionData ) {
        LogonId = pLogonSessionData->LogonId;
        DebugEvent("KFW_Logon_Event - LogonId(%d,%d)", LogonId.HighPart, LogonId.LowPart);

        sprintf(szLogonId,"%d.%d",LogonId.HighPart, LogonId.LowPart);
        LsaFreeReturnBuffer( pLogonSessionData );
    } else {
        DebugEvent0("KFW_Logon_Event - Unable to determine LogonId");
        return;
    }

    count = GetEnvironmentVariable("TEMP", filename, sizeof(filename));
    if ( count > sizeof(filename) || count == 0 ) {
        GetWindowsDirectory(filename, sizeof(filename));
    }

    count = GetEnvironmentVariable("TEMP", filename, sizeof(filename));
    if ( count > sizeof(filename) || count == 0 ) {
        GetWindowsDirectory(filename, sizeof(filename));
    }

    if ( strlen(filename) + strlen(szLogonId) + 2 > sizeof(filename) ) {
        DebugEvent0("KFW_Logon_Event - filename too long");
	return;
    }

    strcat(filename, "\\");
    strcat(filename, szLogonId);    

    KFW_AFS_set_file_cache_dacl(filename, pInfo->hToken);

    KFW_AFS_obtain_user_temp_directory(pInfo->hToken, newfilename, sizeof(newfilename));

    if ( strlen(newfilename) + strlen(szLogonId) + 2 > sizeof(newfilename) ) {
        DebugEvent0("KFW_Logon_Event - new filename too long");
	return;
    }

    strcat(newfilename, "\\");
    strcat(newfilename, szLogonId);    

    if (!MoveFileEx(filename, newfilename, 
		     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DebugEvent("KFW_Logon_Event - MoveFileEx failed GLE = 0x%x", GetLastError());
	return;
    }

    sprintf(commandline, "afscpcc.exe \"%s\"", newfilename);

    GetStartupInfo(&startupinfo);
    if (CreateProcessAsUser( pInfo->hToken,
                             "afscpcc.exe",
                             commandline,
                             NULL,
                             NULL,
                             FALSE,
                             CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                             NULL,
                             NULL,
                             &startupinfo,
                             &procinfo)) 
    {
	DebugEvent("KFW_Logon_Event - CommandLine %s", commandline);

	WaitForSingleObject(procinfo.hProcess, 30000);

	CloseHandle(procinfo.hThread);
	CloseHandle(procinfo.hProcess);
    } else {
	DebugEvent0("KFW_Logon_Event - CreateProcessFailed");
    }

    DeleteFile(filename);

    DebugEvent0("KFW_Logon_Event - End");
}

