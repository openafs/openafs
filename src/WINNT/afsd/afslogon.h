/*

Copyright 2004 by the Massachusetts Institute of Technology

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

/* We only support VC 1200 and above anyway */
#pragma once

#include <windows.h>
#include <npapi.h>
#include <ntsecapi.h>
#include <strsafe.h>


#define REG_CLIENT_DOMAINS_SUBKEY	"Domain"
#define REG_CLIENT_RETRY_INTERVAL_PARM  "LoginRetryInterval"
#define REG_CLIENT_SLEEP_INTERVAL_PARM	"LoginSleepInterval"
#define REG_CLIENT_FAIL_SILENTLY_PARM   "FailLoginsSilently"
#define REG_CLIENT_TRACE_OPTION_PARM	"TraceOption"
#define REG_CLIENT_LOGON_OPTION_PARM	"LogonOptions"
#define REG_CLIENT_LOGON_SCRIPT_PARMW	L"LogonScript"
#define REG_CLIENT_THESE_CELLS_PARM     "TheseCells"
#define REG_CLIENT_LOGOFF_TOKENS_PARM	"LogoffPreserveTokens"
#define DEFAULT_RETRY_INTERVAL          60                        /* seconds*/
#define DEFAULT_FAIL_SILENTLY           FALSE
#define DEFAULT_SLEEP_INTERVAL          5                         /* seconds*/
#define DEFAULT_LOGON_OPTION			1

#define TRACE_OPTION_EVENT 1

#define ISLOGONTRACE(v) ( ((v) & TRACE_OPTION_EVENT)==TRACE_OPTION_EVENT)

#define ISLOGONINTEGRATED(v) ( ((v) & LOGON_OPTION_INTEGRATED)==LOGON_OPTION_INTEGRATED)
#define ISHIGHSECURITY(v) ( ((v) & LOGON_OPTION_HIGHSECURITY)==LOGON_OPTION_HIGHSECURITY)

#define ISREMOTE(v) ( ((v) & LOGON_FLAG_REMOTE)==LOGON_FLAG_REMOTE)
#define ISADREALM(v) ( ((v) & LOGON_FLAG_AD_REALM)==LOGON_FLAG_AD_REALM)
extern DWORD TraceOption;

#define LOGON_FLAG_LOCAL	0
#define LOGON_FLAG_REMOTE	1
#define LOGON_FLAG_AD_REALM 2

typedef struct LogonOptions_type {
	DWORD	LogonOption;
	BOOLEAN	failSilently;
	int	retryInterval;
	int	sleepInterval;
	char *	smbName;
	LPWSTR	logonScript;
	DWORD	flags; /* LOGON_FLAG_* */
        char *  theseCells;
} LogonOptions_t;

/* */
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#define MAX_DOMAIN_LENGTH 256

BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved);

DWORD APIENTRY NPGetCaps(DWORD index);

DWORD APIENTRY NPLogonNotify(
	PLUID lpLogonId,
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	LPWSTR *lpLogonScript);

DWORD APIENTRY NPPasswordChangeNotify(
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	DWORD dwChangeInfo);

#ifdef __cplusplus
extern "C" {
#endif

void DebugEvent0(char *a);
void DebugEvent(char *b,...);

CHAR *GenRandomName(CHAR *pbuf);

BOOLEAN AFSWillAutoStart(void);

DWORD MapAuthError(DWORD code);

BOOL IsServiceRunning (void);

static BOOL WINAPI UnicodeStringToANSI(UNICODE_STRING uInputString, LPSTR lpszOutputString, int nOutStringLen);

void GetDomainLogonOptions( PLUID lpLogonId, char * username, char * domain, LogonOptions_t *opt );
DWORD GetFileCellName(char * path, char * cell, size_t cellLen);
DWORD GetAdHomePath(char * homePath, size_t homePathLen, PLUID lpLogonId, LogonOptions_t * opt);
DWORD QueryAdHomePathFromSid(char * homePath, size_t homePathLen, PSID psid, PWSTR domain);
BOOL GetLocalShortDomain(PWSTR Domain, DWORD cbDomain);

#ifdef __cplusplus
}
#endif
