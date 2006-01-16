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
#include <strsafe.h>
#include <WINNT/afsreg.h>
#include "afsd.h"
#include "afsd_eventlog.h"

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
	if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_SUBKEY, 0, KEY_QUERY_VALUE, &hKey ) )
	{		
	    bRet = FALSE;
	    break;
	}

	// prepare user's buffer and read into it
	dwData = *pdwPathBufSize;
	memset(lpPathBuf, '\0', dwData);
	if ( RegQueryValueEx( hKey,			// handle to key
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
    HKEY	hKey = NULL, hLogKey; 
    UCHAR	szBuf[MAX_PATH]; 
    DWORD	dwData, dwDisposition; 
    BOOL	bRet = TRUE;

    do {
	if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_APPLOG_SUBKEY, 0,
			   KEY_QUERY_VALUE, &hLogKey ) )
	{			
	    // nope - create it		
	    if ( RegCreateKeyEx(HKEY_LOCAL_MACHINE, AFSREG_APPLOG_SUBKEY, 0,
				 NULL, REG_OPTION_NON_VOLATILE,
				 KEY_ALL_ACCESS, NULL, &hLogKey,
				 &dwDisposition)) 
	    {	
		bRet = FALSE;
		break;
	    }
	}

	// Let's see if key already exists as a subkey under the 
	// Application key in the EventLog registry key.  If not,
	// create it.
	if ( RegOpenKeyEx( hLogKey, AFSREG_CLT_APPLOG_SUBKEY, 0,
			   KEY_QUERY_VALUE, &hKey ) )
	{			
	    // nope - create it		
	    if ( RegCreateKeyEx(hLogKey, AFSREG_CLT_APPLOG_SUBKEY, 0,
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
	    if ( RegSetValueEx( hKey,			// subkey handle 
				AFSREG_APPLOG_MSGFILE_VALUE,	// value name 
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

	    if ( RegSetValueEx( hKey,			// subkey handle 
				AFSREG_APPLOG_MSGTYPE_VALUE,	// value name 
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
	    if ( RegQueryValueEx( hKey,			// handle to key
				  AFSREG_APPLOG_MSGFILE_VALUE,	// value name
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

    if (hLogKey != NULL)
	RegCloseKey(hLogKey); 

    return bRet;
} 	

// Log an event with a formatted system message as the (only) substitution
// string, from the given message ID.
VOID
LogEventMessage(WORD wEventType, DWORD dwEventID, DWORD dwMessageID)
{
    LPTSTR msgBuf;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		   NULL, dwMessageID, 0, (LPTSTR)&msgBuf, 0, NULL);
    LogEvent(wEventType, dwEventID, msgBuf, NULL);
    LocalFree(msgBuf);
}

//
// Use the ReportEvent API to write an entry to the system event log.
//
#define MAXARGS 8
#define STRLEN  64
VOID
LogEvent(WORD wEventType, DWORD dwEventID, ...)
{
    va_list 	listArgs;
    HANDLE	hEventSource;
    LPTSTR 	lpArgs[MAXARGS];
    CHAR 	lpStrings[MAXARGS][STRLEN];
    WORD 	wNumArgs = 0;
    WORD 	wNumStrings = 0;
	DWORD   code;

    // Ensure that our event source is properly initialized.
    if (!AddEventSource())
	return;

    // Get a handle to the event log.
    hEventSource = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
    if (hEventSource == NULL)
	return;

    // Construct the array of substitution strings.
    va_start(listArgs, dwEventID);

    switch ( dwEventID ) {
    case MSG_FLUSH_NO_SHARE_NAME:
    case MSG_FLUSH_NO_MEMORY:
    case MSG_FLUSH_IMPERSONATE_ERROR:
    case MSG_FLUSH_UNEXPECTED_EVENT:
    case MSG_UNHANDLED_EXCEPTION:
    case MSG_SMB_ZERO_TRANSACTION_COUNT:
    case MSG_SERVICE_START_PENDING:
    case MSG_SERVICE_INCORRECT_VERSIONS:
    case MSG_SERVICE_RUNNING:
    case MSG_SERVICE_STOPPING:
    case MSG_SERVICE_ERROR_STOP:
    case MSG_CRYPT_OFF:
    case MSG_CRYPT_ON:
	break;
    case MSG_FLUSH_BAD_SHARE_NAME:
    case MSG_FLUSH_OPEN_ENUM_ERROR:
    case MSG_FLUSH_ENUM_ERROR:
    case MSG_FLUSH_FAILED:
    case MSG_RX_HARD_DEAD_TIME_EXCEEDED:
    case MSG_SERVICE_ERROR_STOP_WITH_MSG:
    case MSG_SMB_SEND_PACKET_FAILURE:
    case MSG_UNEXPECTED_SMB_SESSION_CLOSE:
	wNumArgs = 1;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
    	break;
    case MSG_TIME_FLUSH_PER_VOLUME:
    case MSG_TIME_FLUSH_TOTAL:
	wNumArgs = 2;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	lpArgs[1] = va_arg(listArgs, LPTSTR);
    	break;
    case MSG_SERVER_REPORTS_VNOVOL:
    case MSG_SERVER_REPORTS_VMOVED:
    case MSG_SERVER_REPORTS_VOFFLINE:
    case MSG_SERVER_REPORTS_VSALVAGE:
    case MSG_SERVER_REPORTS_VNOSERVICE:
    case MSG_SERVER_REPORTS_VIO:
	wNumArgs = 2;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,afs_int32));
	lpArgs[1] = lpStrings[1];
    	break;
    case MSG_BAD_SMB_PARAM:
	wNumArgs = 5;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[2],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[3],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[4],STRLEN,"%d",va_arg(listArgs,WORD));
	lpArgs[1] = lpStrings[1];
	lpArgs[2] = lpStrings[2];
	lpArgs[3] = lpStrings[3];
	lpArgs[4] = lpStrings[4];
    	break;
    case MSG_BAD_SMB_PARAM_WITH_OFFSET:
	wNumArgs = 6;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[2],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[3],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[4],STRLEN,"%d",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[5],STRLEN,"%d",va_arg(listArgs,WORD));
	lpArgs[1] = lpStrings[1];
	lpArgs[2] = lpStrings[2];
	lpArgs[3] = lpStrings[3];
	lpArgs[4] = lpStrings[4];
	lpArgs[5] = lpStrings[5];
    	break;
    case MSG_BAD_SMB_TOO_SHORT:
    case MSG_BAD_SMB_INVALID:
    case MSG_BAD_SMB_INCOMPLETE:
	wNumArgs = 1;
	StringCbPrintf(lpStrings[0],STRLEN,"%d",va_arg(listArgs,WORD));
	lpArgs[0] = lpStrings[0];
    	break;
    case MSG_SMB_SESSION_START:
	wNumArgs = 1;
	StringCbPrintf(lpStrings[0],STRLEN,"%d",va_arg(listArgs,long));
	lpArgs[0] = lpStrings[0];
    	break;
    case MSG_BAD_SMB_WRONG_SESSION:
	wNumArgs = 2;
	StringCbPrintf(lpStrings[0],STRLEN,"%d",va_arg(listArgs,DWORD));
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,WORD));
	lpArgs[0] = lpStrings[0];
	lpArgs[1] = lpStrings[1];
    	break;
    case MSG_BAD_VCP:
	wNumArgs = 4;
	StringCbPrintf(lpStrings[0],STRLEN,"%d",va_arg(listArgs,UCHAR));
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,UCHAR));
	StringCbPrintf(lpStrings[2],STRLEN,"%d",va_arg(listArgs,UCHAR));
	StringCbPrintf(lpStrings[3],STRLEN,"%d",va_arg(listArgs,UCHAR));
	lpArgs[0] = lpStrings[0];
	lpArgs[1] = lpStrings[1];
	lpArgs[2] = lpStrings[2];
	lpArgs[3] = lpStrings[3];
    	break;
    case MSG_SERVICE_ERROR_STOP_WITH_MSG_AND_LOCATION:
	wNumArgs = 3;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,int));
	lpArgs[1] = lpStrings[1];
	lpArgs[2] = va_arg(listArgs,LPTSTR);
    	break;
    }
    va_end(listArgs);

    // Make sure we were not given too many args.
    if (wNumArgs >= MAXARGS)
	return;

    // Log the event.
    code = ReportEvent(hEventSource,		// handle of event source
		 wEventType,		// event type
		 0,			// event category
		 dwEventID,		// event ID
		 NULL,			// current user's SID
		 wNumArgs,		// strings in lpszArgs
		 0,			// no bytes of raw data
		 wNumArgs ? lpArgs : NULL,		// array of error strings
		 NULL);			// no raw data


    DeregisterEventSource(hEventSource);
}	


