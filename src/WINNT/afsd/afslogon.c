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

#include <windows.h>
#include <npapi.h>
#include <winsock2.h>
#include "afsd.h"
#include <afs/pioctl_nt.h>
#include <afs/kautils.h>
#include "cm_config.h"
#include "krb.h"

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

DWORD LogonOption,TraceOption;

HANDLE hDLL;

WSADATA WSAjunk;

#define REG_CLIENT_PARMS_KEY            "SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"
#define REG_CLIENT_PROVIDER_KEY			"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider"
#define REG_CLIENT_RETRY_INTERVAL_PARM  "LoginRetryInterval"
#define REG_CLIENT_FAIL_SILENTLY_PARM   "FailLoginsSilently"
#define DEFAULT_RETRY_INTERVAL          30                        /* seconds*/
#define DEFAULT_FAIL_SILENTLY           FALSE
#define DEFAULT_SLEEP_INTERVAL          5                         /* seconds*/

#define ISLOGONINTEGRATED(v) ( ((v) & LOGON_OPTION_INTEGRATED)==LOGON_OPTION_INTEGRATED)
#define ISHIGHSECURITY(v) ( ((v) & LOGON_OPTION_HIGHSECURITY)==LOGON_OPTION_HIGHSECURITY)

#define TRACE_OPTION_EVENT 1
#define ISLOGONTRACE(v) ( ((v) & TRACE_OPTION_EVENT)==TRACE_OPTION_EVENT)

#ifdef COMMENT
/* Structure def copied from DDK (NTDEF.H) */
typedef struct UNICODE_STRING {
	USHORT Length;		/* number of bytes of Buffer actually used */
	USHORT MaximumLength;	/* sizeof buffer in bytes */
	WCHAR *Buffer;		/* 16 bit characters */
} UNICODE_STRING;

/* Structure def copied from NP API documentation */
typedef struct _MSV1_0_INTERACTIVE_LOGON {
	DWORD		MessageType;	/* Actually this is an enum; ignored */
	UNICODE_STRING	LogonDomainName;
	UNICODE_STRING	UserName;
	UNICODE_STRING	Password;
} MSV1_0_INTERACTIVE_LOGON;
#endif

/*
 * GetLogonScript
 *
 * We get a logon script pathname from the HKEY_LOCAL_MACHINE registry.
 * I don't know what good this does; I just copied it from DFS.
 *
 * Returns NULL on failure.
 */


void DebugEvent0(char *a) 
{
	HANDLE h; char *ptbuf[1];
	if (!ISLOGONTRACE(TraceOption))
		return;
	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	ptbuf[0] = a;
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);
	DeregisterEventSource(h);
}

#define MAXBUF_ 131
void DebugEvent(char *a,char *b,...) 
{
	HANDLE h; char *ptbuf[1],buf[MAXBUF_+1];
	va_list marker;
	if (!ISLOGONTRACE(TraceOption))
		return;
	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	va_start(marker,b);
	_vsnprintf(buf,MAXBUF_,b,marker);
    buf[MAXBUF_] = '\0';
	ptbuf[0] = buf;
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);\
	DeregisterEventSource(h);
	va_end(marker);
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

WCHAR *GetLogonScript(CHAR *pname)
{
	WCHAR *script,*buf;
	DWORD code;
	DWORD LSPtype, LSPsize;
	HKEY NPKey;
	WCHAR randomName[MAXRANDOMNAMELEN];

	/*
	 * Get Network Provider key.
	 * Assume this works or we wouldn't be here.
	 */
	(void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_CLIENT_PROVIDER_KEY,
			    0, KEY_QUERY_VALUE, &NPKey);

	/*
	 * Get Logon Script pathname length
	 */

	code = RegQueryValueExW(NPKey, L"LogonScript", NULL,
				&LSPtype, NULL, &LSPsize);

	if (code) {
		RegCloseKey (NPKey);
		return NULL;
	}

	if (LSPtype != REG_SZ) {	/* Maybe handle REG_EXPAND_SZ? */
		RegCloseKey (NPKey);
		return NULL;
	}

	buf=(WCHAR *)LocalAlloc(LMEM_FIXED,LSPsize);
	script=(WCHAR *)LocalAlloc(LMEM_FIXED,LSPsize+(MAXRANDOMNAMELEN)*sizeof(WCHAR));
	/*
	 * Explicitly call UNICODE version
	 * Assume it will succeed since it did before
	 */
	(void) RegQueryValueExW(NPKey, L"LogonScript", NULL,
				&LSPtype, (LPBYTE)buf, &LSPsize);
	MultiByteToWideChar(CP_ACP,0,pname,strlen(pname)+1,randomName,(strlen(pname)+1)*sizeof(WCHAR));
	swprintf(script,buf,randomName);
	LocalFree(buf);

#ifdef DEBUG_VERBOSE
    {
        HANDLE h; char *ptbuf[1],buf[132],tbuf[255];
		WideCharToMultiByte(CP_ACP,0,script,LSPsize,tbuf,255,NULL,NULL);
        h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
        sprintf(buf, "Script[%s,%d] Return Code[%x]",tbuf,LSPsize,code);
        ptbuf[0] = buf;
        ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, ptbuf, NULL);
        DeregisterEventSource(h);
    }
#endif

	RegCloseKey (NPKey);
	return script;
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
	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
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
	case KTC_NOCM:
	case KTC_NOCMRPC:
		return WN_NO_NETWORK;
/*	case INTK_BADPW: return WN_BAD_PASSWORD;*/
/*	case KERB_ERR_PRINCIPAL_UNKNOWN: return WN_BAD_USER;*/
	default: return WN_SUCCESS;
	}
}

BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved)
{
	hDLL = dll;
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			/* Initialize AFS libraries */
			rx_Init(0);
            initAFSDirPath();
			ka_Init(0);
			break;

		/* Everything else succeeds but does nothing. */
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		default:
			break;
	}

	return TRUE;
}

DWORD APIENTRY NPGetCaps(DWORD index)
{
	switch (index) {
		case WNNC_NET_TYPE:
			/* Don't have our own type; use somebody else's. */
			return WNNC_NET_SUN_PC_NFS;
		default:
			return 0;
	}
}

static void GetLoginBehavior(int *pRetryInterval, BOOLEAN *pFailSilently)
{
    long result;
    HKEY hKey;
    DWORD dummyLen;
                
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_CLIENT_PARMS_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        *pRetryInterval = DEFAULT_RETRY_INTERVAL;
        *pFailSilently = DEFAULT_FAIL_SILENTLY;
        return;
    }
        
    result = RegQueryValueEx(hKey, REG_CLIENT_RETRY_INTERVAL_PARM, 0, 0, (BYTE *)pRetryInterval, &dummyLen);
    if (result != ERROR_SUCCESS)
        *pRetryInterval = DEFAULT_RETRY_INTERVAL;
       	        
    result = RegQueryValueEx(hKey, REG_CLIENT_FAIL_SILENTLY_PARM, 0, 0, (BYTE *)pFailSilently, &dummyLen);
    if (result != ERROR_SUCCESS)
        *pFailSilently = DEFAULT_FAIL_SILENTLY;

    /* Make sure this is really a bool value in the strict sense*/
    *pFailSilently = !!*pFailSilently;

    RegCloseKey(hKey);
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
    DebugEvent("AFS AfsLogon - Test Service Running","Return Code[%x] ?Running[%d]",Status.dwCurrentState,(Status.dwCurrentState == SERVICE_RUNNING));
    return (Status.dwCurrentState == SERVICE_RUNNING);
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
    
    if (((LPBYTE) uInputString.Buffer)[1] == '\0')
    {
        // Looks like unicode, better translate it
        // UNICODE_STRING specifies the length of the buffer string in Bytes not WCHARS
        WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) uInputString.Buffer, uInputString.Length/2,
                            lpszOutputString, nOutStringLen-1, NULL, NULL);
        lpszOutputString[max(uInputString.Length/2,nOutStringLen-1)] = '\0';
        return TRUE;
    }
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
	char uname[256]="";
	char *ctemp;
	char password[256]="";
	char cell[256]="<non-integrated logon>";
	MSV1_0_INTERACTIVE_LOGON *IL;
	DWORD code;
	int pw_exp;
	char *reason;
	BOOLEAN interactive;
	BOOLEAN flag;
	DWORD LSPtype, LSPsize;
	HKEY NPKey;
	HWND hwndOwner = (HWND)StationHandle;
    BOOLEAN failSilently;
    int retryInterval;
    int sleepInterval = DEFAULT_SLEEP_INTERVAL;        /* seconds        */
    BOOLEAN afsWillAutoStart;
	CHAR RandomName[MAXRANDOMNAMELEN];
    BOOLEAN uppercased_name = TRUE;

    /* Initialize Logon Script to none */
	*lpLogonScript=NULL;
        
	IL = (MSV1_0_INTERACTIVE_LOGON *) lpAuthentInfo;

	/* Are we interactive? */
	interactive = (wcscmp(lpStationName, L"WinSta0") == 0);

	/* Convert from Unicode to ANSI */
	UnicodeStringToANSI(IL->UserName, uname, 256);
	UnicodeStringToANSI(IL->Password, password, 256);

	/* Make sure AD-DOMANS sent from login that is sent to us is striped */
    ctemp = strchr(uname, '@');
    if (ctemp) *ctemp = 0;

    /* is the name all uppercase? */
    for ( ctemp = uname; *ctemp ; ctemp++) {
        if ( islower(*ctemp) ) {
            uppercased_name = FALSE;
            break;
        }
    }

	(void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_CLIENT_PARMS_KEY,
                        0, KEY_QUERY_VALUE, &NPKey);
	LSPsize=sizeof(TraceOption);
	RegQueryValueEx(NPKey, "TraceOption", NULL,
                     &LSPtype, (LPBYTE)&TraceOption, &LSPsize);
    RegCloseKey (NPKey);
	
	/*
	 * Get Logon OPTIONS
	 */

	(void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_CLIENT_PROVIDER_KEY,
                         0, KEY_QUERY_VALUE, &NPKey);

	LSPsize=sizeof(LogonOption);
	code = RegQueryValueEx(NPKey, "LogonOptions", NULL,
                            &LSPtype, (LPBYTE)&LogonOption, &LSPsize);

	RegCloseKey (NPKey);
	if ((code!=0) || (LSPtype!=REG_DWORD))
		LogonOption=LOGON_OPTION_INTEGRATED;	/*default to integrated logon only*/

	afsWillAutoStart = AFSWillAutoStart();
        
	DebugEvent("AFS AfsLogon - NPLogonNotify","LogonOption[%x], Service AutoStart[%d]",
                LogonOption,afsWillAutoStart);
    
    /* Get local machine specified login behavior (or defaults) */
    GetLoginBehavior(&retryInterval, &failSilently);
        
    /* Check for zero length password if integrated logon*/
	if ( ISLOGONINTEGRATED(LogonOption) )  {
        if ( password[0] == 0 ) {
            code = GT_PW_NULL;
            reason = "zero length password is illegal";
            code=0;
        }

        /* Get cell name if doing integrated logon */
		code = cm_GetRootCellName(cell);
		if (code < 0) { 
			code = KTC_NOCELL;
			reason = "unknown cell";
			code=0;
		}

        /*only do if high security option is on*/
        if (ISHIGHSECURITY(LogonOption))
            *lpLogonScript = GetLogonScript(GenRandomName(RandomName));	
    }

    /* loop until AFS is started. */
    while (TRUE) {
        code=0;
		
        /* is service started yet?*/
        DebugEvent("AFS AfsLogon - ka_UserAuthenticateGeneral2","Code[%x] uname[%s] Cell[%s]",
                   code,uname,cell);

        /* if Integrated Logon only */
        if (ISLOGONINTEGRATED(LogonOption) && !ISHIGHSECURITY(LogonOption))
		{			
            if ( KFW_is_available() )
                code = KFW_AFS_get_cred(uname, cell, password, 0, uname, &reason);
            else
                code = ka_UserAuthenticateGeneral2(KA_USERAUTH_VERSION+KA_USERAUTH_AUTHENT_LOGON,
                                                uname, "", cell, password, uname, 0, &pw_exp, 0,
                                                &reason);
			DebugEvent("AFS AfsLogon - (INTEGRATED only)ka_UserAuthenticateGeneral2","Code[%x]",
                        code);
            if ( code && code != KTC_NOCM && code != KTC_NOCMRPC && uppercased_name ) {
                for ( ctemp = uname; *ctemp ; ctemp++) {
                    *ctemp = tolower(*ctemp);
                }
                uppercased_name = FALSE;
                continue;
            }
		} 
        /* if Integrated Logon and High Security pass random generated name*/
        else if (ISLOGONINTEGRATED(LogonOption) && ISHIGHSECURITY(LogonOption))
		{
            if ( KFW_is_available() )
                code = KFW_AFS_get_cred(uname, cell, password, 0, RandomName, &reason);
            else
                code = ka_UserAuthenticateGeneral2(KA_USERAUTH_VERSION+KA_USERAUTH_AUTHENT_LOGON,
                                                uname, "", cell, password,RandomName, 0, &pw_exp, 0,
                                                &reason);
			DebugEvent("AFS AfsLogon - (Both)ka_UserAuthenticateGeneral2","Code[%x] RandomName[%s]",
                       code, RandomName);

            if ( code && code != KTC_NOCM && code != KTC_NOCMRPC && uppercased_name ) {
                for ( ctemp = uname; *ctemp ; ctemp++) {
                    *ctemp = tolower(*ctemp);
                }
                uppercased_name = FALSE;
                continue;
            }
		} else {  
            /*JUST check to see if its running*/
		    if (IsServiceRunning())
                break;
		    code = KTC_NOCM;
		    if (!afsWillAutoStart)
                break;
		}

		/* If we've failed because the client isn't running yet and the
         * client is set to autostart (and therefore it makes sense for
         * us to wait for it to start) then sleep a while and try again. 
         * If the error was something else, then give up. */
		if (code != KTC_NOCM && code != KTC_NOCMRPC || !afsWillAutoStart)
			break;
		
        /* If the retry interval has expired and we still aren't
         * logged in, then just give up if we are not in interactive
         * mode or the failSilently flag is set, otherwise let the
         * user know we failed and give them a chance to try again. */
        if (retryInterval <= 0) {
            reason = "AFS not running";
            if (!interactive || failSilently)
                break;
			flag = MessageBox(hwndOwner,
                               "AFS is still starting.  Retry?",
                               "AFS Logon",
                               MB_ICONQUESTION | MB_RETRYCANCEL);
			if (flag == IDCANCEL)
                break;

            /* Wait just a little while and try again */
            retryInterval = sleepInterval = DEFAULT_SLEEP_INTERVAL;
        }

        if (retryInterval < sleepInterval)
			sleepInterval = retryInterval;

		Sleep(sleepInterval * 1000);

        retryInterval -= sleepInterval;
    }

    /* remove any kerberos 5 tickets currently held by the SYSTEM account */
    if ( KFW_is_available() )
        KFW_AFS_destroy_tickets_for_cell(cell);

	if (code) {
        char msg[128];
        sprintf(msg, "Integrated login failed: %s", reason);

		if (interactive && !failSilently)
			MessageBox(hwndOwner, msg, "AFS Logon", MB_OK);
		else {
            HANDLE h;
            char *ptbuf[1];

            h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
            ptbuf[0] = msg;
            ReportEvent(h, EVENTLOG_WARNING_TYPE, 0, 1008, NULL,
                         1, 0, ptbuf, NULL);
            DeregisterEventSource(h);
        }
	    code = MapAuthError(code);
		SetLastError(code);

		if (ISLOGONINTEGRATED(LogonOption) && (code!=0))
		{
			if (*lpLogonScript)
				LocalFree(*lpLogonScript);
			*lpLogonScript = NULL;
			if (!afsWillAutoStart)	// its not running, so if not autostart or integrated logon then just skip
				code = 0;

		}
	}

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
	DebugEvent0("AFS AfsLogon - NPPasswordChangeNotify");
	return 0;
}

