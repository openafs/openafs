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

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/pioctl_nt.h>
#include <afs/kautils.h>

#include "afsd.h"
#include "cm_config.h"
#include "krb.h"
#include "afskfw.h"

DWORD LogonOption,TraceOption;

HANDLE hDLL;

WSADATA WSAjunk;

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

	/*if(!a) */
		a = AFS_DAEMON_EVENT_NAME;
	h = RegisterEventSource(NULL, a);
	va_start(marker,b);
	StringCbVPrintf(buf, MAXBUF_+1,b,marker);
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
/*	case KTC_NOCM:
	case KTC_NOCMRPC:
		return WN_NO_NETWORK; */
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

		case WNNC_START:
			/* Say we are already started, even though we might wait after we receive NPLogonNotify */
			return 1;

		default:
			return 0;
	}
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
			if(rv == ERROR_SUCCESS) DebugEvent(NULL, #v " found in hkDom with type [%d]", dwType); \
		} \
		if(hkDoms && (rv != ERROR_SUCCESS || dwType != t)) { \
			dwSize = sizeof(v); \
			rv = RegQueryValueEx(hkDoms, n, 0, &dwType, (LPBYTE) &(v), &dwSize); \
			if(rv == ERROR_SUCCESS) DebugEvent(NULL, #v " found in hkDoms with type [%d]", dwType); \
		} \
		if(hkNp && (rv != ERROR_SUCCESS || dwType != t)) { \
			dwSize = sizeof(v); \
			rv = RegQueryValueEx(hkNp, n, 0, &dwType, (LPBYTE) &(v), &dwSize); \
			if(rv == ERROR_SUCCESS) DebugEvent(NULL, #v " found in hkNp with type [%d]", dwType); \
		} \
		if(rv != ERROR_SUCCESS || dwType != t) { \
			v = d; \
			DebugEvent(NULL, #v " being set to default"); \
		} \
	} while(0)

/* Get domain specific configuration info.  We are returning void because if anything goes wrong
   we just return defaults.
 */
void GetDomainLogonOptions( PLUID lpLogonId, char * username, char * domain, LogonOptions_t *opt ) {
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

	DebugEvent(NULL,"In GetDomainLogonOptions for user [%s] in domain [%s]", username, domain);
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

	rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REG_CLIENT_PARMS_KEY, 0, KEY_READ, &hkParm );
	if(rv != ERROR_SUCCESS) {
		hkParm = NULL;
		DebugEvent(NULL, "GetDomainLogonOption: Can't open parms key [%d]", rv);
	}

	rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REG_CLIENT_PROVIDER_KEY, 0, KEY_READ, &hkNp );
	if(rv != ERROR_SUCCESS) {
		hkNp = NULL;
		DebugEvent(NULL, "GetDomainLogonOptions: Can't open NP key [%d]", rv);
	}

	if(hkNp) {
		rv = RegOpenKeyEx( hkNp, REG_CLIENT_DOMAINS_SUBKEY, 0, KEY_READ, &hkDoms );
		if( rv != ERROR_SUCCESS ) {
			hkDoms = NULL;
			DebugEvent(NULL, "GetDomainLogonOptions: Can't open Domains key [%d]", rv);
		}
	}

	if(hkDoms && effDomain) {
		rv = RegOpenKeyEx( hkDoms, effDomain, 0, KEY_READ, &hkDom );
		if( rv != ERROR_SUCCESS ) {
			hkDom = NULL;
			DebugEvent( NULL, "GetDomainLogonOptions: Can't open domain key for [%s] [%d]", effDomain, rv);
			/* If none of the domains match, we shouldn't use the domain key either */
			RegCloseKey(hkDoms);
			hkDoms = NULL;
		}
	} else
		DebugEvent( NULL, "Not opening domain key for [%s]", effDomain);

	/* Each individual can either be specified on the domain key, the domains key or in the
	   net provider key.  They fail over in that order.  If none is found, we just use the 
	   defaults. */

	/* LogonOption */
	LOOKUPKEYCHAIN(opt->LogonOption, REG_DWORD, DEFAULT_LOGON_OPTION, REG_CLIENT_LOGON_OPTION_PARM);

	/* FailLoginsSilently */
	dwSize = sizeof(dwDummy);
	rv = RegQueryValueEx(hkParm, REG_CLIENT_FAIL_SILENTLY_PARM, 0, &dwType, (LPBYTE) &dwDummy, &dwSize);
	if(rv != ERROR_SUCCESS)
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
	} else {
		/* username and domain for logon session is not necessarily the same as
		   username and domain passed into network provider. */
		PSECURITY_LOGON_SESSION_DATA plsd;
		char lsaUsername[MAX_USERNAME_LENGTH];
		char lsaDomain[MAX_DOMAIN_LENGTH];
		size_t len, tlen;

        LsaGetLogonSessionData(lpLogonId, &plsd);
        
		UnicodeStringToANSI(plsd->UserName, lsaUsername, MAX_USERNAME_LENGTH);
		UnicodeStringToANSI(plsd->LogonDomain, lsaDomain, MAX_DOMAIN_LENGTH);

		DebugEvent(NULL,"PLSD username[%s] domain[%s]",lsaUsername,lsaDomain);

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
	}

	DebugEvent(NULL,"Looking up logon script");
	/* Logon script */
	/* First find out where the key is */
	hkTemp = NULL;
	rv = ~ERROR_SUCCESS;
	dwType = 0;
	if(hkDom)
	    rv = RegQueryValueExW(hkDom, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
	if(rv == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
		hkTemp = hkDom;
		DebugEvent(NULL,"Located logon script in hkDom");
	}
	else if(hkDoms)
	    rv = RegQueryValueExW(hkDoms, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
	if(rv == ERROR_SUCCESS && !hkTemp && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
		hkTemp = hkDoms;
		DebugEvent(NULL,"Located logon script in hkDoms");
	}
	/* Note that the LogonScript in the NP key is only used if we are doing high security. */
	else if(hkNp && ISHIGHSECURITY(opt->LogonOption))
	    rv = RegQueryValueExW(hkNp, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, NULL, &dwSize);
	if(rv == ERROR_SUCCESS && !hkTemp && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
		hkTemp = hkNp;
		DebugEvent(NULL,"Located logon script in hkNp");
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
		MultiByteToWideChar(CP_ACP,0,opt->smbName,-1,wuname,len*sizeof(WCHAR));

		DebugEvent(NULL,"Username is set for [%S]", wuname);

		/* dwSize still has the size of the required buffer in bytes. */
        regscript = malloc(dwSize);
		rv = RegQueryValueExW(hkTemp, REG_CLIENT_LOGON_SCRIPT_PARMW, 0, &dwType, (LPBYTE) regscript, &dwSize);
		if(rv != ERROR_SUCCESS) {/* what the ..? */
			DebugEvent(NULL,"Can't look up logon script [%d]",rv);
			goto doneLogonScript;
		}
		
		DebugEvent(NULL,"Found logon script [%S]", regscript);

		if(dwType == REG_EXPAND_SZ) {
			DWORD dwReq;

	   		dwSize += MAX_PATH * sizeof(WCHAR);  /* make room for environment expansion. */
			regexscript = malloc(dwSize);
			dwReq = ExpandEnvironmentStringsW(regscript, regexscript, dwSize / sizeof(WCHAR));
			free(regscript);
			regscript = regexscript;
			regexscript = NULL;
			if(dwReq > (dwSize / sizeof(WCHAR))) {
				DebugEvent(NULL,"Overflow while expanding environment strings.");
				goto doneLogonScript;
			}
		}

		DebugEvent(NULL,"After expanding env strings [%S]", regscript);

		if(wcsstr(regscript, L"%s")) {
	        dwSize += len * sizeof(WCHAR); /* make room for username expansion */
			regexuscript = (WCHAR *) LocalAlloc(LMEM_FIXED, dwSize);
			hr = StringCbPrintfW(regexuscript, dwSize, regscript, wuname);
		} else {
			regexuscript = (WCHAR *) LocalAlloc(LMEM_FIXED, dwSize);
			hr = StringCbCopyW(regexuscript, dwSize, regscript);
		}

		DebugEvent(NULL,"After expanding username [%S]", regexuscript);

		if(hr == S_OK)
			opt->logonScript = regexuscript;
		else
			LocalFree(regexuscript);

doneLogonScript:
		if(wuname) free(wuname);
		if(regscript) free(regscript);
		if(regexscript) free(regexscript);
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

	MSV1_0_INTERACTIVE_LOGON *IL;

	DWORD code;

	int pw_exp;
	char *reason;
	char *ctemp;

	BOOLEAN interactive;
	BOOLEAN flag;
	DWORD LSPtype, LSPsize;
	HKEY NPKey;

	HWND hwndOwner = (HWND)StationHandle;

	BOOLEAN afsWillAutoStart;

    BOOLEAN uppercased_name = TRUE;

	LogonOptions_t opt; /* domain specific logon options */
	int retryInterval;
	int sleepInterval;

    /* Initialize Logon Script to none */
	*lpLogonScript=NULL;
    
	/* TODO: We should check the value of lpAuthentInfoType before assuming that it is
	         MSV1_0_INTERACTIVE_LOGON though for our purposes KERB_INTERACTIVE_LOGON is
			 co-incidentally equivalent. */
	IL = (MSV1_0_INTERACTIVE_LOGON *) lpAuthentInfo;

	/* Are we interactive? */
	interactive = (wcscmp(lpStationName, L"WinSta0") == 0);

	/* Convert from Unicode to ANSI */

	/*TODO: Use SecureZeroMemory to erase passwords */
	UnicodeStringToANSI(IL->UserName, uname, 256);
	UnicodeStringToANSI(IL->Password, password, 256);
	UnicodeStringToANSI(IL->LogonDomainName, logonDomain, 256);

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
	RegQueryValueEx(NPKey, REG_CLIENT_TRACE_OPTION_PARM, NULL,
                     &LSPtype, (LPBYTE)&TraceOption, &LSPsize);

	RegCloseKey (NPKey);

	/*
	 * Get Logon options
	 */

	GetDomainLogonOptions( lpLogonId, uname, logonDomain, &opt );
	retryInterval = opt.retryInterval;
	sleepInterval = opt.sleepInterval;
	*lpLogonScript = opt.logonScript;

	DebugEvent(NULL,"Got logon script: %S",opt.logonScript);

	afsWillAutoStart = AFSWillAutoStart();

	DebugEvent("AFS AfsLogon - NPLogonNotify","LogonOption[%x], Service AutoStart[%d]",
                opt.LogonOption,afsWillAutoStart);
    
    /* Check for zero length password if integrated logon*/
	if ( ISLOGONINTEGRATED(opt.LogonOption) )  {
        if ( password[0] == 0 ) {
            code = GT_PW_NULL;
            reason = "zero length password is illegal";
            code=0;
        }

        /* Get cell name if doing integrated logon.  
		   We might overwrite this if we are logging into an AD realm and we find out that
		   the user's home dir is in some other cell. */
		code = cm_GetRootCellName(cell);
		if (code < 0) { 
			code = KTC_NOCELL;
			reason = "unknown cell";
			code=0;
		}

		/* We get the user's home directory path, if applicable, though we can't lookup the
		   cell right away because the client service may not have started yet. This call
		   also sets the AD_REALM flag in opt.flags if applicable. */
		if(ISREMOTE(opt.flags))
			GetAdHomePath(homePath,MAX_PATH,lpLogonId,&opt);
    }

    /* loop until AFS is started. */
    while (TRUE) {
		if(ISADREALM(opt.flags)) {
			code = GetFileCellName(homePath,cell,256);
			if(!code) {
				DebugEvent(NULL,"profile path [%s] is in cell [%s]",homePath,cell);
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
				DebugEvent(NULL,"KFW_AFS_get_cred  uname=[%s] smbname=[%s] cell=[%s] code=[%d]",uname,opt.smbName,cell,code);
			}
			else {
                code = ka_UserAuthenticateGeneral2(KA_USERAUTH_VERSION+KA_USERAUTH_AUTHENT_LOGON,
                                                uname, "", cell, password, opt.smbName, 0, &pw_exp, 0,
                                                &reason);
				DebugEvent("AFS AfsLogon - (INTEGRATED only)ka_UserAuthenticateGeneral2","Code[%x]",
							code);
			}
            if ( code && code != KTC_NOCM && code != KTC_NOCMRPC && uppercased_name ) {
                for ( ctemp = uname; *ctemp ; ctemp++) {
                    *ctemp = tolower(*ctemp);
                }
                uppercased_name = FALSE;
                continue;
            }
		}
		else {  
            /*JUST check to see if its running*/
		    if (IsServiceRunning())
                break;
		    code = KTC_NOCM;
		    if (!afsWillAutoStart)
                break;
		}

		/* is service started yet?*/
        DebugEvent("AFS AfsLogon - ka_UserAuthenticateGeneral2","Code[%x] uname[%s] Cell[%s]",
                   code,uname,cell);

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
            if (!interactive || opt.failSilently)
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

		StringCbPrintf(msg, sizeof(msg), "Integrated login failed: %s", reason);

		if (interactive && !opt.failSilently)
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

	if(opt.smbName) free(opt.smbName);

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

#include <userenv.h>
#include <Winwlx.h>
#include "lanahelper.h"

VOID AFS_Logoff_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    DWORD code;
    TCHAR profileDir[256] = TEXT("");
    TCHAR uncprefix[64] = TEXT("\\\\");
    DWORD  len;

    len = 256;
    lana_GetNetbiosName(&uncprefix[2], LANA_NETBIOS_NAME_FULL);

    if ( GetUserProfileDirectory(pInfo->hToken, profileDir, &len) ) {
        if (_tcsnicmp(uncprefix, profileDir, _tcslen(uncprefix))) {
            if (code = ktc_ForgetAllTokens())
                DebugEvent(NULL,"AFS AfsLogon - AFS_Logoff_Event - ForgetAllTokens failed [%lX]",code);
            else
                DebugEvent0("AFS AfsLogon - AFS_Logoff_Event - ForgetAllTokens succeeded");
        } else {
            DebugEvent0("AFS AfsLogon - AFS_Logoff_Event - Tokens left in place; profile in AFS");
        }
    }
}   





