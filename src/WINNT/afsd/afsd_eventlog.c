////////////////////////////////////////////////////////////////////
//
//
//		E V E N T   L O G G I N G   F U N C T I O N S 
//
//
////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <stdarg.h>
#include <string.h>
#include <WINNT/afsreg.h>
#include "afsd_eventlog.h"

static CHAR	szKeyName[] = AFSREG_APPLOG_SUBKEY "\\" AFSREG_CLT_SVC_NAME;

static BOOL	GetServicePath(LPTSTR lpPathBuf, PDWORD pdwPathBufSize);
static BOOL	AddEventSource(void);

static BOOL
GetServicePath(LPTSTR lpPathBuf, PDWORD pdwPathBufSize)
{
	HKEY	hKey = NULL; 
	DWORD	dwData = 0;
	BOOL	bRet = TRUE;

	do {
		
		// Open key
		if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_SVR_SVC_SUBKEY, 0, KEY_QUERY_VALUE, &hKey ) )
		{		
			bRet = FALSE;
			break;
		}

		// prepare user's buffer and read into it
		dwData = *pdwPathBufSize;
		memset(lpPathBuf, '\0', dwData);
		if ( RegQueryValueEx( 
				hKey,			// handle to key
				"ImagePath",		// value name
				NULL,			// reserved
				NULL,			// type buffer
				(LPBYTE) lpPathBuf,	// data buffer
				&dwData))		// size of data buffer
		{
			bRet = FALSE;
			break;
		}
		
		*pdwPathBufSize = dwData;

	} while (0);
				
	if (hKey != NULL)
		RegCloseKey(hKey); 
	
	return bRet;
} 

//
// Ensure name for message file is in proper location in Registry.
//
static BOOL
AddEventSource()
{
	HKEY	hKey = NULL; 
	UCHAR	szBuf[MAX_PATH]; 
	DWORD	dwData, dwDisposition; 
	BOOL	bRet = TRUE;

	do {
		// Let's see if key already exists as a subkey under the 
		// Application key in the EventLog registry key.  If not,
		// create it.
		if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, szKeyName, 0,
				   KEY_QUERY_VALUE, &hKey ) )
		{		
			// nope - create it		
			if ( RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0,
					    NULL, REG_OPTION_NON_VOLATILE,
					    KEY_ALL_ACCESS, NULL, &hKey,
					    &dwDisposition)) 
			{
				bRet = FALSE;
				break;
			}
						
			// Set the name of the message file
			// Get "ImagePath" from TransarcAFSDaemon service
			memset(szBuf, '\0', MAX_PATH);
			dwData = MAX_PATH;
			GetServicePath(szBuf, &dwData);

			// Add the name to the EventMessageFile subkey. 
			if ( RegSetValueEx(
					hKey,			// subkey handle 
					AFSREG_SVR_APPLOG_MSGFILE_VALUE,	// value name 
					0,			// must be zero 
					REG_EXPAND_SZ,		// value type 
					(LPBYTE) szBuf,		// pointer to value data 
					(DWORD)strlen(szBuf) + 1))	// length of value data
			{
				bRet = FALSE;
				break;
			}
 
			// Set the supported event types in the TypesSupported subkey. 
			dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | 
						EVENTLOG_INFORMATION_TYPE; 
 
			if ( RegSetValueEx(
					hKey,			// subkey handle 
					AFSREG_SVR_APPLOG_MSGTYPE_VALUE,	// value name 
					0,			// must be zero 
					REG_DWORD,		// value type 
					(LPBYTE) &dwData,	// pointer to value data 
					sizeof(DWORD)))		// length of value data
			{
				bRet = FALSE;
				break;
			}
		}

		else
		{
			// key was opened - read it
			memset(szBuf, '\0', MAX_PATH);
			dwData = MAX_PATH;
			if ( RegQueryValueEx( 
					hKey,			// handle to key
					AFSREG_SVR_APPLOG_MSGFILE_VALUE,	// value name
					NULL,			// reserved
					NULL,			// type buffer
					(LPBYTE) szBuf,		// data buffer
					&dwData))		// size of data buffer
			{
				bRet = FALSE;
				break;
			}
		}
		
	} while (0);
				
	if (hKey != NULL)
		RegCloseKey(hKey); 

	return bRet;
} 

// Log an event with a formatted system message as the (only) substitution
// string, from the given message ID.
VOID
LogEventMessage(WORD wEventType, DWORD dwEventID, DWORD dwMessageID)
{
	LPTSTR msgBuf;

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
		      | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		      NULL, dwMessageID, 0, (LPTSTR)&msgBuf, 0, NULL);
	LogEvent(wEventType, dwEventID, msgBuf, NULL);
	LocalFree(msgBuf);
}

//
// Use the ReportEvent API to write an entry to the system event log.
//
#define MAXSTRINGARGS 100
VOID
LogEvent(WORD wEventType, DWORD dwEventID, LPTSTR lpString, ...)
{
	va_list listStrings;
	HANDLE	hEventSource;
	LPTSTR lpStrings[MAXSTRINGARGS];
	WORD wNumStrings;

	// Ensure that our event source is properly initialized.
	if (!AddEventSource())
		return;

	// Get a handle to the event log.
	hEventSource = RegisterEventSource(NULL, AFSREG_CLT_SVC_PARAM_KEY);
	if (hEventSource == NULL)
		return;

	// Construct the array of substitution strings.
	va_start(listStrings, lpString);
	for (wNumStrings = 0;
	     lpString != NULL && wNumStrings < MAXSTRINGARGS;
	     wNumStrings++)
	{
		lpStrings[wNumStrings] = lpString;
		// Advance to the next argument.
		lpString = va_arg(listStrings, LPTSTR);
	}
	va_end(listStrings);

	// Make sure we were not given too many args.
	if (wNumStrings >= MAXSTRINGARGS)
		return;

	// Log the event.
	ReportEvent(hEventSource,		// handle of event source
		    wEventType,			// event type
		    0,				// event category
		    dwEventID,			// event ID
		    NULL,			// current user's SID
		    wNumStrings,		// strings in lpszStrings
		    0,				// no bytes of raw data
		    lpStrings,			// array of error strings
		    NULL);			// no raw data

	DeregisterEventSource(hEventSource);
}
