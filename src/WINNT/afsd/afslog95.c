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
#include <NETSPI.h>
#include <winsock2.h>
#include <afs/pioctl_nt.h>
#include <afs/kautils.h>
#include "cm_config.h"
#include "krb.h"

HANDLE hDLL;

WSADATA WSAjunk;

char NPName95[] = "System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider";

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
	(void) RegOpenKeyEx(HKEY_LOCAL_MACHINE, NPName95,
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
		case WNNC_SPEC_VERSION:
			return WNNC_SPEC_VERSION51;
		case WNNC_NET_TYPE:
			/* Don't have our own type; use somebody else's. */
			return WNNC_NET_LANMAN;
		case WNNC_DRIVER_VERSION:
			return WNNC_DRIVER(1,1);
		case WNNC_START:
			return WNNC_START_DONE;
		case WNNC_AUTHENTICATION:
			return WNNC_AUTH_LOGON | WNNC_AUTH_LOGOFF;
		default:
			return 0;
	}
}

DWORD APIENTRY NPLogon(
	HWND hwndOwner,
	LPLOGONINFO lpAuthentInfo,
	LPLOGONINFO lpPreviousAuthentInfo,
	LPTSTR lpLogonScript,
	DWORD dwBufferSize,
	DWORD dwFlags)
{
	DWORD code;
	int pw_exp;
	char *reason;
	char cell[256];

	if (dwFlags & LOGON_DONE)
		return 0;

	/* Check for zero length password */
	if (lpAuthentInfo->lpUsername[0] == 0) {
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

	code = ka_UserAuthenticateGeneral(
		KA_USERAUTH_VERSION + KA_USERAUTH_AUTHENT_LOGON,
		lpAuthentInfo->lpUsername,
		"", cell,
		lpAuthentInfo->lpPassword,
		0, &pw_exp, 0, &reason);

checkauth:
	if (code) {
		MessageBox(hwndOwner, reason, "AFS logon", MB_OK);
		code = MapAuthError(code);
	}

	return code;
}

DWORD APIENTRY NPLogoff(
	HWND hwndOwner,
	LPLOGONINFO lpAuthentInfo,
	DWORD dwReason)
{
	return 0;
}
