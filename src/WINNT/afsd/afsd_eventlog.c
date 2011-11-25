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
#define AFS_VERSION_STRINGS
#include "afs_component_version_number.h"

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
    UCHAR	szBuf[MAX_PATH] = "afsd_service.exe";
    DWORD	dwData, dwDisposition;
    static BOOL	bRet = TRUE;
    static BOOL bOnce = TRUE;

    if (!bOnce)
        return bRet;

    if ( RegOpenKeyEx( HKEY_LOCAL_MACHINE, AFSREG_APPLOG_SUBKEY, 0,
                       KEY_SET_VALUE, &hLogKey ) )
    {
        // nope - create it
        if ( RegCreateKeyEx(HKEY_LOCAL_MACHINE, AFSREG_APPLOG_SUBKEY, 0,
                             NULL, REG_OPTION_NON_VOLATILE,
                             KEY_ALL_ACCESS, NULL, &hLogKey,
                             &dwDisposition))
        {
            bRet = FALSE;
            goto done;
        }
    }

    // Let's see if key already exists as a subkey under the
    // Application key in the EventLog registry key.  If not,
    // create it.
    if ( RegOpenKeyEx( hLogKey, AFSREG_CLT_APPLOG_SUBKEY, 0,
                       KEY_SET_VALUE, &hKey ) )
    {
        // nope - create it
        if ( RegCreateKeyEx(hLogKey, AFSREG_CLT_APPLOG_SUBKEY, 0,
                             NULL, REG_OPTION_NON_VOLATILE,
                             KEY_ALL_ACCESS, NULL, &hKey,
                             &dwDisposition))
        {
            bRet = FALSE;
            goto done;
        }
    }

    // Add the name to the EventMessageFile subkey.
    if ( RegSetValueEx( hKey,			// subkey handle
                        AFSREG_APPLOG_MSGFILE_VALUE,	// value name
                        0,			// must be zero
                        REG_SZ,		        // value type
                        (LPBYTE) szBuf,		// pointer to value data
                        (DWORD)strlen(szBuf) + 1))	// length of value data
    {
        bRet = FALSE;
        goto done;
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
	goto done;
    }

  done:
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
#define STRLEN  128

VOID
LogEvent(WORD wEventType, DWORD dwEventID, ...)
{
    va_list 	listArgs;
    HANDLE	hEventSource;
    HANDLE      hMutex = NULL;
    LPTSTR 	lpArgs[MAXARGS];
    CHAR 	lpStrings[MAXARGS][STRLEN];
    static CHAR lpLastStrings[MAXARGS][STRLEN];
    WORD 	wNumArgs = 0;
    static WORD wLastNumArgs = MAXARGS;
    static time_t lastMessageTime = 0;
    static WORD wLastEventType = 0;
    static DWORD dwLastEventID = 0;
    time_t      now;
    DWORD       code;
    BOOL        bLogMessage = TRUE;
    WORD        i = 0, j;

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
    case MSG_SERVICE_INCORRECT_VERSIONS:
    case MSG_SERVICE_STOPPING:
    case MSG_SERVICE_STOPPED:
    case MSG_SERVICE_ERROR_STOP:
    case MSG_CRYPT_OFF:
    case MSG_CRYPT_ON:
	break;
    case MSG_SERVICE_START_PENDING:
        wNumArgs = 1;
        lpArgs[0] = AFSVersion;
        break;
    case MSG_SERVICE_RUNNING:
        wNumArgs = 1;
        lpArgs[0] = "SMB interface";
        break;
    case MSG_FLUSH_BAD_SHARE_NAME:
    case MSG_FLUSH_OPEN_ENUM_ERROR:
    case MSG_FLUSH_ENUM_ERROR:
    case MSG_FLUSH_FAILED:
    case MSG_RX_HARD_DEAD_TIME_EXCEEDED:
    case MSG_SERVICE_ERROR_STOP_WITH_MSG:
    case MSG_SMB_SEND_PACKET_FAILURE:
    case MSG_UNEXPECTED_SMB_SESSION_CLOSE:
    case MSG_RX_MSGSIZE_EXCEEDED:
    case MSG_RX_BUSY_CALL_CHANNEL:
	wNumArgs = 1;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
    	break;
    case MSG_TIME_FLUSH_PER_VOLUME:
    case MSG_TIME_FLUSH_TOTAL:
    case MSG_SMB_MAX_MPX_COUNT:
    case MSG_SMB_MAX_BUFFER_SIZE:
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
    case MSG_SERVER_REPORTS_VBUSY:
    case MSG_SERVER_REPORTS_VRESTARTING:
	wNumArgs = 3;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[1],STRLEN,"%d",va_arg(listArgs,afs_int32));
	lpArgs[1] = lpStrings[1];
	lpArgs[2] = va_arg(listArgs, LPTSTR);
        break;
    case MSG_ALL_SERVERS_BUSY:
    case MSG_ALL_SERVERS_OFFLINE:
    case MSG_ALL_SERVERS_DOWN:
    case MSG_RX_IDLE_DEAD_TIMEOUT:
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
    case MSG_DIRTY_BUFFER_AT_SHUTDOWN:
	wNumArgs = 6;
	lpArgs[0] = va_arg(listArgs, LPTSTR);
        lpArgs[1] = va_arg(listArgs, LPTSTR);
	StringCbPrintf(lpStrings[2],STRLEN,"%u",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[3],STRLEN,"%u",va_arg(listArgs,int));
	StringCbPrintf(lpStrings[4],STRLEN,"%I64u",va_arg(listArgs,afs_int64));
	StringCbPrintf(lpStrings[5],STRLEN,"%I64u",va_arg(listArgs,afs_int64));
	lpArgs[2] = lpStrings[2];
	lpArgs[3] = lpStrings[3];
	lpArgs[4] = lpStrings[4];
	lpArgs[5] = lpStrings[5];
    	break;
    }
    va_end(listArgs);

    // Make sure we were not given too many args.
    if (wNumArgs >= MAXARGS)
        goto done;

    hMutex = CreateMutex( NULL, TRUE, "AFSD Event Log Mutex");
    if (hMutex == NULL)
        goto done;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        code = WaitForSingleObject( hMutex, 500);
        if (code != WAIT_OBJECT_0)
            goto done;
    }

    /*
     * We rate limit consecutive duplicate messages to one every
     * five seconds.
     */
    now = time(NULL);
    if (now < lastMessageTime + 5 &&
        wEventType == wLastEventType &&
        dwEventID == dwLastEventID &&
        wNumArgs == wLastNumArgs) {
        for (i=0; i<wNumArgs; i++) {
            if ( strncmp(lpArgs[i], lpLastStrings[i], STRLEN))
                break;
        }
        if (i == wNumArgs)
            bLogMessage = FALSE;
    }

    if ( bLogMessage) {
        wLastNumArgs = wNumArgs;
        wLastEventType = wEventType;
        dwLastEventID = dwEventID;
        lastMessageTime = now;

        for ( j = (i == wNumArgs ? 0 : i) ; i < wNumArgs; i++) {
            StringCbCopyEx( lpLastStrings[i], STRLEN, lpArgs[i], NULL, NULL, STRSAFE_NULL_ON_FAILURE);
        }
    }

    ReleaseMutex(hMutex);

    // Log the event.
    if ( bLogMessage)
        code = ReportEvent(hEventSource,		// handle of event source
                           wEventType,		// event type
                           0,			// event category
                           dwEventID,		// event ID
                           NULL,			// current user's SID
                           wNumArgs,		// strings in lpszArgs
                           0,			// no bytes of raw data
                           wNumArgs ? lpArgs : NULL,// array of error strings
                           NULL);			// no raw data

  done:
    if (hMutex)
        CloseHandle(hMutex);

    DeregisterEventSource(hEventSource);
}


