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


HANDLE hDLL;

WSADATA WSAjunk;

char NPName[] = "System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider";

#define REG_CLIENT_PARMS_KEY            "SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"
#define REG_CLIENT_RETRY_INTERVAL_PARM  "LoginRetryInterval"
#define REG_CLIENT_FAIL_SILENTLY_PARM   "FailLoginsSilently"
#define DEFAULT_RETRY_INTERVAL          30                        // seconds
#define DEFAULT_FAIL_SILENTLY           FALSE
#define DEFAULT_SLEEP_INTERVAL          5                         // seconds


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

/*
 * GetLogonScript
 *
 * We get a logon script pathname from the HKEY_LOCAL_MACHINE registry.
 * I don't know what good this does; I just copied it from DFS.
 *
 * Returns NULL on failure.
 */
WCHAR *GetLogonScript(void)
{
	WCHAR *script;
	DWORD code;
	DWORD LSPtype, LSPsize;
	HKEY NPKey;

	/*
	 * Get Network Provider key.
	 * Assume this works or we wouldn't be here.
	 */
	(void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, NPName,
			    0, KEY_QUERY_VALUE, &NPKey);

	/*
	 * Get Logon Script pathname length
	 */
	code = RegQueryValueEx(NPKey, "LogonScript", NULL,
				&LSPtype, NULL, &LSPsize);

	if (code) {
		RegCloseKey (NPKey);
		return NULL;
	}

	if (LSPtype != REG_SZ) {	/* Maybe handle REG_EXPAND_SZ? */
		RegCloseKey (NPKey);
		return NULL;
	}

	script = (WCHAR *)LocalAlloc(LMEM_FIXED, LSPsize);

	/*
	 * Explicitly call UNICODE version
	 * Assume it will succeed since it did before
	 */
	(void) RegQueryValueExW(NPKey, L"LogonScript", NULL,
				&LSPtype, (LPBYTE)script, &LSPsize);

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
	pConfig = (LPQUERY_SERVICE_CONFIG)GlobalAlloc(GMEM_FIXED, BufSize);
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
		case INTK_BADPW: return WN_BAD_PASSWORD;
		case KERB_ERR_PRINCIPAL_UNKNOWN: return WN_BAD_USER;
		default: return WN_NO_NETWORK;
	}
}

BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved)
{
	hDLL = dll;
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			/* Initialize AFS libraries */
			rx_Init(0);
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

        // Make sure this is really a bool value in the strict sense
        *pFailSilently = !!*pFailSilently;
       	        
        RegCloseKey(hKey);
}

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
	char uname[256];
	char password[256];
	char cell[256];
	MSV1_0_INTERACTIVE_LOGON *IL;
	DWORD code;
	int pw_exp;
	char *reason;
	BOOLEAN interactive;
	BOOLEAN flag;
	HWND hwndOwner = (HWND)StationHandle;
        BOOLEAN failSilently;
        int retryInterval;
        int sleepInterval = DEFAULT_SLEEP_INTERVAL;        // seconds        
        BOOLEAN afsWillAutoStart;
        
	IL = (MSV1_0_INTERACTIVE_LOGON *) lpAuthentInfo;

	/* Are we interactive? */
	interactive = (wcscmp(lpStationName, L"WinSta0") == 0);

	/* Convert from Unicode to ANSI */
	wcstombs(uname, IL->UserName.Buffer, 256);
	wcstombs(password, IL->Password.Buffer, 256);

	/* Check for zero length password */
	if (password[0] == 0) {
		code = GT_PW_NULL;
		reason = "zero length password is illegal";
		goto checkauth;
	}

	/* Get cell name */
	code = cm_GetRootCellName(cell);
	if (code < 0) {
		code = KTC_NOCELL;
		reason = "unknown cell";
		goto checkauth;
	}

        /* Get user specified login behavior (or defaults) */
        GetLoginBehavior(&retryInterval, &failSilently);
        
        afsWillAutoStart = AFSWillAutoStart();
        
        /* Possibly loop until AFS is started. */
        while (1) {
                code = ka_UserAuthenticateGeneral(
			KA_USERAUTH_VERSION+KA_USERAUTH_AUTHENT_LOGON,
			uname, "", cell, password, 0, &pw_exp, 0,
			&reason);
			
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

checkauth:
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
	}

	/* Get logon script */
	if (interactive)
		*lpLogonScript = GetLogonScript();

	if (code) {
	        code = MapAuthError(code);
		SetLastError(code);
	}

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
	return 0;
}

