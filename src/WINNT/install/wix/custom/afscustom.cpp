/*

Copyright 2004 by the Massachusetts Institute of Technology
Copyright 2008 by Secure Endpoints Inc.

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/**************************************************************
* afscustom.cpp : Dll implementing custom action to install AFS
*
*         The functions in this file are for use as entry points
*         for calls from MSI only. The specific MSI parameters
*         are noted in the comments section of each of the
*         functions.
*
* rcsid: $Id$
**************************************************************/

// Only works for Win2k and above

#define _WIN32_WINNT 0x0500
#define UNICODE
#define _UNICODE

#include "afscustom.h"
#include <varargs.h>
#include <tchar.h>
#include <strsafe.h>


void ShowMsiError( MSIHANDLE hInstall, DWORD errcode, DWORD param ){
	MSIHANDLE hRecord;

	hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetInteger(hRecord, 1, errcode);
	MsiRecordSetInteger(hRecord, 2, param);

	MsiProcessMessage( hInstall, INSTALLMESSAGE_ERROR, hRecord );

	MsiCloseHandle( hRecord );
}

/* Abort the installation (called as an immediate custom action) */
MSIDLLEXPORT AbortMsiImmediate( MSIHANDLE hInstall ) {
    DWORD rv;
	DWORD dwSize = 0;
	LPTSTR sReason = NULL;
	LPTSTR sFormatted = NULL;
	MSIHANDLE hRecord = NULL;
	LPTSTR cAbortReason = _T("ABORTREASON");

	rv = MsiGetProperty( hInstall, cAbortReason, _T(""), &dwSize );
	if(rv != ERROR_MORE_DATA) goto _cleanup;

	sReason = new TCHAR[ ++dwSize ];

	rv = MsiGetProperty( hInstall, cAbortReason, sReason, &dwSize );

	if(rv != ERROR_SUCCESS) goto _cleanup;

    hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetString(hRecord, 0, sReason);

	dwSize = 0;

	rv = MsiFormatRecord(hInstall, hRecord, _T(""), &dwSize);
	if(rv != ERROR_MORE_DATA) goto _cleanup;

	sFormatted = new TCHAR[ ++dwSize ];

	rv = MsiFormatRecord(hInstall, hRecord, sFormatted, &dwSize);

	if(rv != ERROR_SUCCESS) goto _cleanup;

	MsiCloseHandle(hRecord);

	hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetInteger(hRecord, 1, ERR_ABORT);
	MsiRecordSetString(hRecord,2, sFormatted);
	MsiProcessMessage(hInstall, INSTALLMESSAGE_ERROR, hRecord);

_cleanup:
	if(sFormatted) delete[] sFormatted;
	if(hRecord) MsiCloseHandle( hRecord );
	if(sReason) delete[] sReason;

	return ~ERROR_SUCCESS;
}

/* Configure the client and server services */
MSIDLLEXPORT ConfigureClientService( MSIHANDLE hInstall ) {
	DWORD rv = ConfigService( 1 );
	if(rv != ERROR_SUCCESS) {
		ShowMsiError( hInstall, ERR_SCC_FAILED, rv );
	}
	return rv;
}

MSIDLLEXPORT ConfigureServerService( MSIHANDLE hInstall ) {
	DWORD rv = ConfigService( 2 );
	if(rv != ERROR_SUCCESS) {
		ShowMsiError( hInstall, ERR_SCS_FAILED, rv );
	}
	return ERROR_SUCCESS;
}

DWORD ConfigService( int svc ) {
	SC_HANDLE scm = NULL;
	SC_HANDLE hsvc = NULL;
	SC_LOCK scl = NULL;
	DWORD rv = ERROR_SUCCESS;

	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(scm == NULL) {rv = GetLastError(); goto _cleanup; }

	scl = LockServiceDatabase(scm);
	if(scl == NULL) {rv = GetLastError(); goto _cleanup; }

	hsvc = OpenService( scm, ((svc==1)? _T("TransarcAFSDaemon") : _T("TransarcAFSServer")), SERVICE_ALL_ACCESS);
	if(hsvc == NULL) {rv = GetLastError(); goto _cleanup; }

	SERVICE_FAILURE_ACTIONS sfa;
	SC_ACTION saact[3];

	sfa.dwResetPeriod = 3600; // one hour
	sfa.lpRebootMsg = NULL;
	sfa.lpCommand = NULL;
	sfa.cActions = 3;
	sfa.lpsaActions = saact;

	saact[0].Type = SC_ACTION_RESTART;
	saact[0].Delay = 5000;
	saact[1].Type = SC_ACTION_RESTART;
	saact[1].Delay = 5000;
	saact[2].Type = SC_ACTION_NONE;
	saact[2].Delay = 5000;

	if(!ChangeServiceConfig2(hsvc, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa))
		rv = GetLastError();

_cleanup:
	if(hsvc) CloseServiceHandle(hsvc);
	if(scl) UnlockServiceDatabase(scl);
	if(scm) CloseServiceHandle(scm);

	return rv;
}

/* Sets the registry keys required for the functioning of the network
provider */

MSIDLLEXPORT InstallNetProvider( MSIHANDLE hInstall ) {
    return InstNetProvider( hInstall, 1 );
}

MSIDLLEXPORT UninstallNetProvider( MSIHANDLE hInstall) {
    return InstNetProvider( hInstall, 0 );
}

DWORD InstNetProvider(MSIHANDLE hInstall, int bInst) {
    LPTSTR strOrder;
    HKEY hkOrder;
    LONG rv;
    DWORD dwSize;
    HANDLE hProcHeap;

    strOrder = (LPTSTR) 0;

    CHECK(rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, STR_KEY_ORDER, 0, KEY_READ | KEY_WRITE, &hkOrder ));

    dwSize = 0;
    CHECK(rv = RegQueryValueEx( hkOrder, STR_VAL_ORDER, NULL, NULL, NULL, &dwSize ) );

    strOrder = new TCHAR[ (dwSize + STR_SERVICE_LEN) * sizeof(TCHAR) ];

    CHECK(rv = RegQueryValueEx( hkOrder, STR_VAL_ORDER, NULL, NULL, (LPBYTE) strOrder, &dwSize));

    npi_CheckAndAddRemove( strOrder, STR_SERVICE , bInst);

    dwSize = (lstrlen( strOrder ) + 1) * sizeof(TCHAR);

    CHECK(rv = RegSetValueEx( hkOrder, STR_VAL_ORDER, NULL, REG_SZ, (LPBYTE) strOrder, dwSize ));

    /* everything else should be set by the MSI tables */
    rv = ERROR_SUCCESS;
_cleanup:

    if( rv != ERROR_SUCCESS ) {
        ShowMsiError( hInstall, ERR_NPI_FAILED, rv );
    }

    if(strOrder) delete[] strOrder;

    return rv;
}

/* Check and add or remove networkprovider key value
	str : target string
	str2: string to add/remove
	bInst: == 1 if string should be added to target if not already there, otherwise remove string from target if present.
    */
int npi_CheckAndAddRemove( LPTSTR str, LPTSTR str2, int bInst ) {

    LPTSTR target, charset, match;
    int ret=0;

    target = new TCHAR[lstrlen(str)+3];
    lstrcpy(target,_T(","));
    lstrcat(target,str);
    lstrcat(target,_T(","));
    charset = new TCHAR[lstrlen(str2)+3];
    lstrcpy(charset,_T(","));
    lstrcat(charset,str2);
    lstrcat(charset,_T(","));

    match = _tcsstr(target, charset);

    if ((match) && (bInst)) {
        ret = INP_ERR_PRESENT;
        goto cleanup;
    }

    if ((!match) && (!bInst)) {
        ret = INP_ERR_ABSENT;
        goto cleanup;
    }

    if (bInst) // && !match
    {
       lstrcat(str, _T(","));
       lstrcat(str, str2);
       ret = INP_ERR_ADDED;
       goto cleanup;
    }

    // if (!bInst) && (match)
    {
       lstrcpy(str+(match-target),match+lstrlen(str2)+2);
       str[lstrlen(str)-1]=_T('\0');
       ret = INP_ERR_REMOVED;
       goto cleanup;
    }

cleanup:

    delete[] target;
    delete[] charset;
    return ret;
}

/* Uninstall NSIS */
MSIDLLEXPORT UninstallNsisInstallation( MSIHANDLE hInstall )
{
	DWORD rv = ERROR_SUCCESS;
	// lookup the NSISUNINSTALL property value
	LPTSTR cNsisUninstall = _T("NSISUNINSTALL");
	HANDLE hIo = NULL;
	DWORD dwSize = 0;
	LPTSTR strPathUninst = NULL;
	HANDLE hJob = NULL;
	STARTUPINFO sInfo;
	PROCESS_INFORMATION pInfo;

	pInfo.hProcess = NULL;
	pInfo.hThread = NULL;

	rv = MsiGetProperty( hInstall, cNsisUninstall, _T(""), &dwSize );
	if(rv != ERROR_MORE_DATA) goto _cleanup;

	strPathUninst = new TCHAR[ ++dwSize ];

	rv = MsiGetProperty( hInstall, cNsisUninstall, strPathUninst, &dwSize );
	if(rv != ERROR_SUCCESS) goto _cleanup;

	// Create a process for the uninstaller
	sInfo.cb = sizeof(sInfo);
	sInfo.lpReserved = NULL;
	sInfo.lpDesktop = _T("");
	sInfo.lpTitle = _T("Foo");
	sInfo.dwX = 0;
	sInfo.dwY = 0;
	sInfo.dwXSize = 0;
	sInfo.dwYSize = 0;
	sInfo.dwXCountChars = 0;
	sInfo.dwYCountChars = 0;
	sInfo.dwFillAttribute = 0;
	sInfo.dwFlags = 0;
	sInfo.wShowWindow = 0;
	sInfo.cbReserved2 = 0;
	sInfo.lpReserved2 = 0;
	sInfo.hStdInput = 0;
	sInfo.hStdOutput = 0;
	sInfo.hStdError = 0;

	if(!CreateProcess(
		strPathUninst,
		_T("Uninstall /S"),
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&sInfo,
		&pInfo)) {
			pInfo.hProcess = NULL;
			pInfo.hThread = NULL;
			rv = 40;
			goto _cleanup;
		};

	// Create a job object to contain the NSIS uninstall process tree

	JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;

	acp.CompletionKey = 0;

	hJob = CreateJobObject(NULL, _T("NSISUninstallObject"));
	if(!hJob) {
		rv = 41;
		goto _cleanup;
	}

	hIo = CreateIoCompletionPort(INVALID_HANDLE_VALUE,0,0,0);
	if(!hIo) {
		rv = 42;
		goto _cleanup;
	}

	acp.CompletionPort = hIo;

	SetInformationJobObject( hJob, JobObjectAssociateCompletionPortInformation, &acp, sizeof(acp));

	AssignProcessToJobObject( hJob, pInfo.hProcess );

	ResumeThread( pInfo.hThread );

	DWORD a,b,c;
	for(;;) {
		if(!GetQueuedCompletionStatus(hIo, &a, (PULONG_PTR) &b, (LPOVERLAPPED *) &c, INFINITE)) {
			Sleep(1000);
			continue;
		}
		if(a == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
			break;
		}
	}

	rv = ERROR_SUCCESS;

_cleanup:
	if(hIo) CloseHandle(hIo);
	if(pInfo.hProcess)	CloseHandle( pInfo.hProcess );
	if(pInfo.hThread) 	CloseHandle( pInfo.hThread );
	if(hJob) CloseHandle(hJob);
	if(strPathUninst) delete[] strPathUninst;

	if(rv != ERROR_SUCCESS) {
        ShowMsiError( hInstall, ERR_NSS_FAILED, rv );
	}
	return rv;
}

/* Create or remove the 'AFS Client Admins' group.  Initially
   it will hold members of the Administrator group. */

MSIDLLEXPORT CreateAFSClientAdminGroup( MSIHANDLE hInstall ) {
    UINT rv;
    rv = createAfsAdminGroup();
    if(rv) {
        if(rv == ERROR_ALIAS_EXISTS) {
            /* The group already exists, probably from a previous
               installation.  We let things be. */
            return ERROR_SUCCESS;
        }

        ShowMsiError( hInstall, ERR_GROUP_CREATE_FAILED, rv );
        return rv;
    }

    rv = initializeAfsAdminGroup();
    if(rv)
        ShowMsiError( hInstall, ERR_GROUP_MEMBER_FAILED, rv );
    return rv;
}

MSIDLLEXPORT RemoveAFSClientAdminGroup( MSIHANDLE hInstall ) {
    removeAfsAdminGroup();
    return ERROR_SUCCESS;
}

#define AFSCLIENT_ADMIN_GROUPNAMEW L"AFS Client Admins"
#define AFSCLIENT_ADMIN_COMMENTW L"AFS Client Administrators"

UINT createAfsAdminGroup(void) {
    LOCALGROUP_INFO_1 gInfo;
    DWORD dwError;
    NET_API_STATUS status;

    gInfo.lgrpi1_name = AFSCLIENT_ADMIN_GROUPNAMEW;
    gInfo.lgrpi1_comment = AFSCLIENT_ADMIN_COMMENTW;
    status = NetLocalGroupAdd(NULL, 1, (LPBYTE) &gInfo, &dwError);

    return status;
}

/* LookupAliasFromRid is from Microsoft KB 157234
 *
 * Author: Scott Field (sfield)    02-Oct-96
 */

BOOL
LookupAliasFromRid( LPWSTR TargetComputer, DWORD Rid, LPWSTR Name,
                    PDWORD cchName )
{
    SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
    SID_NAME_USE snu;
    PSID pSid;
    WCHAR DomainName[DNLEN+1];
    DWORD cchDomainName = DNLEN;
    BOOL bSuccess = FALSE;

    //
    // Sid is the same regardless of machine, since the well-known
    // BUILTIN domain is referenced.
    //

    if(AllocateAndInitializeSid( &sia, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 Rid, 0, 0, 0, 0, 0, 0, &pSid )) {
        bSuccess = LookupAccountSidW( TargetComputer, pSid, Name, cchName,
                                      DomainName, &cchDomainName, &snu );
        FreeSid(pSid);
    }

    return bSuccess;
}

UINT initializeAfsAdminGroup(void) {
    PSID psidAdmin = NULL;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    NET_API_STATUS status;
    LOCALGROUP_MEMBERS_INFO_0 *gmAdmins = NULL;
    DWORD dwNEntries, dwTEntries;
    WCHAR AdminGroupName[UNLEN+1];
    DWORD cchName = UNLEN;

    if (!LookupAliasFromRid( NULL, DOMAIN_ALIAS_RID_ADMINS, AdminGroupName, &cchName ))
    {
        /* if we fail, we will try the English string "Administrators" */
        wcsncpy(AdminGroupName, L"Administrators", UNLEN+1);
        AdminGroupName[UNLEN] = 0;
    }

    status = NetLocalGroupGetMembers(NULL, AdminGroupName, 0, (LPBYTE *) &gmAdmins, MAX_PREFERRED_LENGTH, &dwNEntries, &dwTEntries, NULL);
    if(status)
        return status;

    status = NetLocalGroupAddMembers(NULL, AFSCLIENT_ADMIN_GROUPNAMEW, 0, (LPBYTE) gmAdmins, dwNEntries);

    NetApiBufferFree( gmAdmins );

    return status;
}

UINT removeAfsAdminGroup(void) {
    NET_API_STATUS status;
    status = NetLocalGroupDel(NULL, AFSCLIENT_ADMIN_GROUPNAMEW);
    return status;
}

const TCHAR * reg_NP = _T("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider");
const TCHAR * reg_NP_Backup = _T("SOFTWARE\\OpenAFS\\Client\\BackupSettings\\NetworkProvider");

const TCHAR * reg_NP_Domains = _T("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider\\Domain");
const TCHAR * reg_NP_Domains_Backup = _T("SOFTWARE\\OpenAFS\\Client\\BackupSettings\\NetworkProvider\\Domain");

const TCHAR * reg_Param = _T("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters");
const TCHAR * reg_Param_Backup = _T("SOFTWARE\\OpenAFS\\Client\\BackupSettings\\Parameters");

const TCHAR * reg_Client = _T("SOFTWARE\\OpenAFS\\Client");
const TCHAR * reg_Client_Backup = _T("SOFTWARE\\OpenAFS\\Client\\BackupSettings\\Client");

const TCHAR * reg_Backup = _T("SOFTWARE\\OpenAFS\\Client\\BackupSettings");

const TCHAR * reg_NP_values[] = {
    _T("LogonOptions"),
    _T("VerboseLogging"),
    _T("LogonScript"),
    _T("FailLoginsSilently"),
    _T("LoginRetryInterval"),
    _T("LoginSleepInterval"),
    _T("Realm"),
    _T("TheseCells"),
    NULL
};

const TCHAR * reg_Everything[] = {
    _T("*"),
    NULL
};

const TCHAR * reg_Client_values[] = {
    _T("CellServDBDir"),
    _T("VerifyServiceSignature"),
    _T("IoctlDebug"),
    _T("MiniDumpType"),
    _T("SMBAsyncStoreSize"),
    _T("StoreAnsiFilenames"),
    _T("StartAfscredsOnStartup"),
    _T("AfscredsShortcutParams"),
    NULL
};

struct registry_backup {
    const TCHAR * key;
    const TCHAR ** values;
    const TCHAR * backup_key;
} registry_backups[] = {

    // Subkeys must be specified before parent keys.

    { reg_NP_Domains, reg_Everything, reg_NP_Domains_Backup },
    { reg_NP, reg_NP_values, reg_NP_Backup },
    { reg_Param, reg_Everything, reg_Param_Backup },
    { reg_Client, reg_Client_values, reg_Client_Backup },
    { NULL, NULL, NULL }
};

void
ShowMsiActionData(MSIHANDLE hInstall, const TCHAR * format, ...)
{
    va_list vl;
    TCHAR buf[1024];
    MSIHANDLE hRec;

    va_start(vl, format);
    StringCbVPrintf(buf, sizeof(buf), format, vl);
    va_end(vl);

    hRec = MsiCreateRecord(2);
    MsiRecordSetString(hRec, 1, buf);
    MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hRec);
    MsiCloseHandle(hRec);
}

void
do_reg_copy_value(MSIHANDLE hInstall, HKEY hk_src, HKEY hk_dest, const TCHAR * value)
{
    BYTE static_buffer[4096];
    BYTE * buffer = static_buffer;
    DWORD cb = sizeof(static_buffer);
    LONG rv;
    DWORD type = 0;

    rv = RegQueryValueEx(hk_src, value, 0, &type, buffer, &cb);
    if (rv == ERROR_MORE_DATA) {
        // Some types require space for a terminating NUL which might not be present
        buffer = new BYTE[++cb];
        if (buffer == NULL) {
            ShowMsiActionData(hInstall, _T("Out of memory"));
            return;
        }

        rv = RegQueryValueEx(hk_src, value, 0, &type, buffer, &cb);
    }

    if (rv == ERROR_SUCCESS) {
        rv = RegSetValueEx(hk_dest, value, 0, type, buffer, cb);
        if (rv == ERROR_SUCCESS) {
            ShowMsiActionData(hInstall, _T("Copied value [%s]"), value);
        } else {
            ShowMsiActionData(hInstall, _T("Can't write value [%s]. Return code =%d"), value, rv);
        }
    } else if (rv != ERROR_FILE_NOT_FOUND) {
        ShowMsiActionData(hInstall, _T("Can't read registry value [%s].  Return code = %d"), value, rv);
    }

    if (buffer != static_buffer)
        delete[] buffer;
}

//! Copy a registry key and optionally all its subkeys
//
// @return TRUE if the source was deleted
BOOL
do_reg_copy(MSIHANDLE hInstall,
            HKEY hk_root_src, const TCHAR * src,
            HKEY hk_root_dest, const TCHAR * dest,
            const TCHAR ** values, BOOL delete_source)
{
    BOOL retval = FALSE;
    HKEY hk_src = 0;
    HKEY hk_dest = 0;

    LONG rv;

    rv = RegOpenKeyEx(hk_root_src, src, 0, KEY_READ, &hk_src);
    if (rv != ERROR_SUCCESS) {
        if (rv == ERROR_FILE_NOT_FOUND) {
            ShowMsiActionData(hInstall, _T("Source key %s does not exist"), src);
        } else {
            ShowMsiActionData(hInstall, _T("Can't open source key while copying %s.  Return value=%d"), src, rv);
        }
        goto cleanup;
    }

    ShowMsiActionData(hInstall, _T("Copying registry key %s to %s"), src, dest);

    // If dest == NULL, then we are just being asked to cleanup src
    if (dest == NULL)
        goto del_source;

    rv = RegCreateKeyEx(hk_root_dest, dest, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &hk_dest, NULL);
    if (rv != ERROR_SUCCESS) {
        ShowMsiActionData(hInstall, _T("Can't create target key.  Return value=%d"), rv);
        goto cleanup;
    }

    if (values[0][0] == _T('*')) {
        BOOL retry;

        for (DWORD index = 0; ; index++) {
            TCHAR name[16384];
            DWORD cch = 16384;

            rv = RegEnumValue(hk_src, index, name, &cch, NULL, NULL, NULL, NULL);
            if (rv == ERROR_SUCCESS) {
                do_reg_copy_value(hInstall, hk_src, hk_dest, name);
            } else {
                break;
            }
        }

        do {
            retry = FALSE;

            for (DWORD index = 0; ; index++) {
                TCHAR name[256];
                DWORD cch = 256;

                rv = RegEnumKeyEx(hk_src, index, name, &cch, NULL, NULL, NULL, NULL);
                if (rv == ERROR_SUCCESS) {
                    if (do_reg_copy(hInstall, hk_src, name, hk_dest, name,
                                    reg_Everything, delete_source)) {
                        index--;
                        retry = TRUE;
                    }
                } else {
                    break;
                }
            }

        } while (retry);

    } else {

        for (const TCHAR ** pv = values; *pv; pv++) {
            do_reg_copy_value(hInstall, hk_src, hk_dest, *pv);
        }
    }

del_source:
    if (delete_source) {
        rv = RegDeleteKey(hk_root_src, src);
        retval = (rv == ERROR_SUCCESS);
        if (rv == ERROR_SUCCESS) {
            ShowMsiActionData(hInstall, _T("Deleted source key"));
        } else {
            ShowMsiActionData(hInstall, _T("Unable to delete source key. Return code=%d"), rv);
        }
    }

cleanup:
    if (hk_dest != 0)
        RegCloseKey(hk_dest);
    if (hk_src != 0)
        RegCloseKey(hk_src);

    return retval;
}


MSIDLLEXPORT BackupAFSClientRegistryKeys( MSIHANDLE hInstall )
{
    registry_backup *b;
    MSIHANDLE hRec;

    hRec = MsiCreateRecord(4);
    MsiRecordSetString(hRec, 1, _T("BackupAFSClientRegistryKeys"));
    MsiRecordSetString(hRec, 2, _T("Backing up AFS client configuration registry keys"));
    MsiRecordSetString(hRec, 3, _T("[1]"));
    MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hRec);
    MsiCloseHandle(hRec);

    for (b = registry_backups; b->key != NULL; b++) {
        do_reg_copy(hInstall, HKEY_LOCAL_MACHINE, b->key, HKEY_LOCAL_MACHINE, b->backup_key,
                    b->values, FALSE);
    }

    return ERROR_SUCCESS;
}

MSIDLLEXPORT RestoreAFSClientRegistryKeys( MSIHANDLE hInstall )
{
    registry_backup *b;
    MSIHANDLE hRec;

    hRec = MsiCreateRecord(4);
    MsiRecordSetString(hRec, 1, _T("RestoreAFSClientRegistryKeys"));
    MsiRecordSetString(hRec, 2, _T("Restoring AFS client configuration registry keys"));
    MsiRecordSetString(hRec, 3, _T("[1]"));
    MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hRec);
    MsiCloseHandle(hRec);

    for (b = registry_backups; b->key != NULL; b++) {
        do_reg_copy(hInstall, HKEY_LOCAL_MACHINE, b->backup_key, HKEY_LOCAL_MACHINE, b->key,
                    b->values, TRUE);
    }
    do_reg_copy(hInstall, HKEY_LOCAL_MACHINE, reg_Backup, NULL, NULL, NULL, TRUE);

    return ERROR_SUCCESS;
}

LONG
SetMsiPropertyFromRegValue(MSIHANDLE hInstall, HKEY hk,
                           const TCHAR * property, const TCHAR * value_name)
{
    LONG rv;
    BYTE static_buffer[4096];
    BYTE *buffer = static_buffer;
    DWORD cb = sizeof(static_buffer);
    DWORD type = 0;

    rv = RegQueryValueEx(hk, value_name, NULL, &type, buffer, &cb);
    if (rv == ERROR_FILE_NOT_FOUND)
        return rv;

    if (rv == ERROR_MORE_DATA ||
        ((type == REG_SZ || type == REG_EXPAND_SZ) && cb >= sizeof(static_buffer))) {

        if (type == REG_SZ || type == REG_EXPAND_SZ)
            cb += sizeof(TCHAR);

        buffer = new BYTE[cb];
        rv = RegQueryValueEx(hk, value_name, NULL, &type, buffer, &cb);
    }

    if (rv != ERROR_SUCCESS)
        goto cleanup;

    switch (type) {
    case REG_DWORD:
    {
        DWORD d = *((DWORD *) buffer);
        StringCbPrintf((TCHAR *) static_buffer, sizeof(static_buffer), _T("%d"), (int) d);
        rv = MsiSetProperty(hInstall, property, (const TCHAR *) static_buffer);
        break;
    }

    case REG_SZ:
    case REG_EXPAND_SZ:
    {
        TCHAR * s = (TCHAR *) buffer;

        if (s[cb / sizeof(TCHAR) - 1] != _T('\0'))
            s[cb / sizeof(TCHAR)] = _T('\0');
        rv = MsiSetProperty(hInstall, property, s);
        break;
    }

    default:
        rv = ERROR_FILE_NOT_FOUND;
    }

cleanup:
    if (buffer != static_buffer)
        delete[] buffer;

    return rv;
}

LONG
SetAfscredsOptionsFromRegValue(MSIHANDLE hInstall, HKEY hk)
{
    TCHAR buffer[16];
    DWORD cb = sizeof(buffer);
    LONG rv;
    DWORD type;

    rv = RegQueryValueEx(hk, _T("AfscredsShortcutParams"), NULL, &type, (LPBYTE) buffer, &cb);
    if (rv != ERROR_SUCCESS || type != REG_SZ || cb == sizeof(buffer)) {
        return ERROR_FILE_NOT_FOUND;
    }

    MsiSetProperty(hInstall, _T("CREDSAUTOINIT"), (_tcsstr(buffer, _T("-a")) != NULL)? _T("-a"): NULL);
    MsiSetProperty(hInstall, _T("CREDSIPCHDET"), (_tcsstr(buffer, _T("-n")) != NULL)? _T("-n"): NULL);
    MsiSetProperty(hInstall, _T("CREDSQUIET"), (_tcsstr(buffer, _T("-q")) != NULL)? _T("-q"): NULL);
    MsiSetProperty(hInstall, _T("CREDSRENEWDRMAP"), (_tcsstr(buffer, _T("-m")) != NULL)? _T("-m"): NULL);
    MsiSetProperty(hInstall, _T("CREDSSHOW"), (_tcsstr(buffer, _T("-s")) != NULL)? _T("-s"): NULL);

    return rv;
}

/// Pull in known configuration values for use during this installation
//
MSIDLLEXPORT DetectSavedConfiguration( MSIHANDLE hInstall )
{
    // Attempt a backup.  If there is something to save, it will be.
    // If not, nothing has changed.
    //
    BackupAFSClientRegistryKeys( hInstall );

    {
        MSIHANDLE hRec;

        hRec = MsiCreateRecord(4);
        MsiRecordSetString(hRec, 1, _T("DetectSavedConfiguration"));
        MsiRecordSetString(hRec, 2, _T("Detecting saved configuration"));
        MsiRecordSetString(hRec, 3, _T("[1]"));
        MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hRec);
        MsiCloseHandle(hRec);
    }

    HKEY hk_client = 0, hk_param = 0, hk_np = 0;
    LONG rv;
    BOOL found = FALSE;

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_Param_Backup, 0, KEY_READ, &hk_param);
    if (rv == ERROR_SUCCESS) {
        SetMsiPropertyFromRegValue(hInstall, hk_param, _T("AFSCELLNAME"), _T("Cell"));
        SetMsiPropertyFromRegValue(hInstall, hk_param, _T("FREELANCEMODE"), _T("FreelanceClient"));
        SetMsiPropertyFromRegValue(hInstall, hk_param, _T("USEDNS"), _T("UseDNS"));
        SetMsiPropertyFromRegValue(hInstall, hk_param, _T("SECURITYLEVEL"), _T("SecurityLevel"));
        RegCloseKey(hk_param);
        found = TRUE;
    }

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_Client_Backup, 0, KEY_READ, &hk_client);
    if (rv == ERROR_SUCCESS) {
        SetMsiPropertyFromRegValue(hInstall, hk_client, _T("CREDSSTARTUP"), _T("StartAfscredsOnStartup"));
        SetAfscredsOptionsFromRegValue(hInstall, hk_client);
        RegCloseKey(hk_client);
        found = TRUE;
    }

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_NP_Backup, 0, KEY_READ, &hk_np);
    if (rv == ERROR_SUCCESS) {
        SetMsiPropertyFromRegValue(hInstall, hk_np, _T("LOGONOPTIONS"), _T("LogonOptions"));
        RegCloseKey(hk_np);
        found = TRUE;
    }

    MsiSetProperty(hInstall, _T("SAVED_CONFIG"), (found)?_T("1"):NULL);

    return 0;
}
