//
//	AFSD_FLUSHVOL.C
// 
//	Routines to handle flushing AFS volumes in response to 
//	System Power event notification such as Hibernate request.
//
/////////////////////////////////////////////////////////////////////

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>

#include <string.h>
#include <setjmp.h>
#include "afsd.h"
#include "afsd_init.h"
#include "smb.h"
#include "cm_conn.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <winsock2.h>

#include <osi.h>

#include "afsd_flushvol.h"
#include "afsd_eventlog.h"
#include "lanahelper.h"

extern void afsi_log(char *pattern, ...);

static FLUSHVOLTHREADINFO	gThreadInfo   = {0};
static HANDLE			gThreadHandle = NULL;


/////////////////////////////////////////////////////////////////////
//
// Call routine found in FS.EXE to flush volume.
//
// At entry, input param is UNC string for volume,
// e.g. '\\afs\all\athena.mit.edu\user\m\h\mholiday'
//
// I believe that success from 'pioctl' routine
// indicated by return value of zero (0).
//
afs_int32
afsd_ServicePerformFlushVolumeCmd(char *data)
{
    register afs_int32 code;
    struct ViceIoctl blob;

    afsi_log("Flushing Volume \"%s\"",data);
    memset(&blob, '\0', sizeof(blob));
    code = pioctl(data, VIOC_FLUSHVOLUME, &blob, 0);
    
    return code;
}

BOOL
afsd_ServicePerformFlushVolumes()
{       
    CONST CHAR	COLON = ':';
    CONST CHAR	SLASH = '\\';
    CONST DWORD	NETRESBUFSIZE = 16384;
    CHAR		bufMessage[1024];
    UINT		i;
    DWORD		dwServerSize;
    DWORD		dwRet;
    DWORD		dwCount;
    DWORD		dwNetResBufSize;
    DWORD		dwTotalVols = 0;
    DWORD		dwVolBegin, dwVolEnd;
    DWORD		dwFlushBegin, dwFlushEnd;
    HANDLE		hEnum;
    LPNETRESOURCE	lpNetResBuf, lpnr;
    PCHAR		pszShareName, pc;
    afs_int32	afsRet = 0;

    if ( lana_OnlyLoopback() ) {
        // Nothing to do if we only have a loopback interface
        return TRUE;
    }

    // Determine the root share name (\\AFS\ALL or \\<machine>-AFS\ALL),
    // and the length of the server name prefix.
    pszShareName = smb_GetSharename();
    if (pszShareName == NULL)
    {
        LogEvent(EVENTLOG_ERROR_TYPE, MSG_FLUSH_NO_SHARE_NAME, NULL);
        return FALSE;
    }
    pc = strrchr(pszShareName, SLASH);
    if ((pc == NULL) || ((dwServerSize = (DWORD)(pc - pszShareName)) < 3))
    {
        LogEvent(EVENTLOG_ERROR_TYPE, MSG_FLUSH_BAD_SHARE_NAME,
                  pszShareName, NULL);
        free(pszShareName);
        return FALSE;
    }

    // Allocate a buffer to hold network resources returned by
    // WNetEnumResource().
    lpNetResBuf = malloc(NETRESBUFSIZE);
    if (lpNetResBuf == NULL)
    {
        // Out of memory, give up now.
        LogEvent(EVENTLOG_ERROR_TYPE, MSG_FLUSH_NO_MEMORY, NULL);
        free(pszShareName);
        return FALSE;
    }

    // Initialize the flush timer.  Note that GetTickCount() returns
    // the number of milliseconds since the system started, in a DWORD,
    // so that the value wraps around every 49.7 days.  We do not bother
    // to handle the case where the flush elapsed time is greater than
    // that.
    dwFlushBegin = GetTickCount();

    dwRet = WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_ANY, 0, NULL,
                          &hEnum);
    if (dwRet != NO_ERROR)
    {
        LogEventMessage(EVENTLOG_ERROR_TYPE, MSG_FLUSH_OPEN_ENUM_ERROR,
                         dwRet);
        free(pszShareName);
        return FALSE;
    }

    // Loop to enumerate network resources, and flush those associated
    // with AFS volumes.
    while (1)
    {
        dwCount = -1;
        memset(lpNetResBuf, 0, NETRESBUFSIZE);
        dwNetResBufSize = NETRESBUFSIZE;
        dwRet = WNetEnumResource(hEnum, &dwCount,
                                  lpNetResBuf, &dwNetResBufSize);
        if (dwRet != NO_ERROR)
            break;
        // Iterate over the returned network resources.
        for (i = 0, lpnr = lpNetResBuf; i < dwCount; i++, lpnr++)
        {
            // Ensure resource has a remote name, and is connected.
            if ((lpnr->lpRemoteName == NULL) ||
                 (lpnr->dwScope != RESOURCE_CONNECTED))
                continue;
            if ((_strnicmp(lpnr->lpRemoteName, pszShareName,
                            dwServerSize) == 0) &&
                 (lpnr->lpRemoteName[dwServerSize] == SLASH))
            {
                // got one!
                // but we don't want to flush '\\[...]afs\all'
                if (_stricmp(lpnr->lpRemoteName, pszShareName) == 0)
                    continue;
                ++dwTotalVols;

                dwVolBegin = GetTickCount();
                afsRet = afsd_ServicePerformFlushVolumeCmd(lpnr->lpRemoteName);
                dwVolEnd = GetTickCount();
                if (afsRet == 0)
                {
                    LogTimingEvent(MSG_TIME_FLUSH_PER_VOLUME,
                                    lpnr->lpRemoteName,
                                    dwVolEnd - dwVolBegin);
                }
                else
                {
                    LogEvent(EVENTLOG_WARNING_TYPE,
                              MSG_FLUSH_FAILED,
                              lpnr->lpRemoteName, NULL);
                }
            }
        }
    }
    WNetCloseEnum(hEnum);
    free(lpNetResBuf);
    free(pszShareName);
    if (dwRet != ERROR_NO_MORE_ITEMS)
    {
        LogEventMessage(EVENTLOG_ERROR_TYPE, MSG_FLUSH_ENUM_ERROR,
                         dwRet);
        return FALSE;
    }

    dwFlushEnd = GetTickCount();
	
    // display total volume count in Event Logger
    sprintf(bufMessage, "%d", dwTotalVols);
    LogTimingEvent(MSG_TIME_FLUSH_TOTAL, bufMessage,
                    dwFlushEnd - dwFlushBegin);

    return TRUE;
}

// Report a timing event to the system event log.
// The lpszString1 argument is the first substitution string for the
// given event ID.  The time argument will be converted into the
// second substitution string.
static VOID
LogTimingEvent(DWORD dwEventID, LPTSTR lpString1, DWORD dwTime)
{
    CHAR	szTime[16];
	
    sprintf(szTime, "%lu", dwTime);
    LogEvent(EVENTLOG_INFORMATION_TYPE, dwEventID, lpString1, szTime,
              NULL);
}


/////////////////////////////////////////////////////////////////////
//
// GetUserToken
//
// Obtain token for the currently logged-in user.
//
// This routine looks for a window which we 'know' belongs to
// the shell, and from there we follow a route which leads to
// getting a handle on an access token owned by the shell.
//
// The return value is either a handle to a suitable token,
// or else null. 
//
// One of the times that this function might return null
// is when there is no logged-in user. Other cases include
// insufficient access to the desktop, etc. 
//
// Disclaimer:
// Portions of this routine found in various newsgroups
//
HANDLE GetUserToken(DWORD access)
{
    HANDLE hTok = NULL;
    DWORD pid = 0, tid = 0;

    // Try it the easy way first - look for a window owned by the shell on
    // our current desktop.  If we find one, use that to get the process id.
    HWND shell = FindWindowEx(NULL, NULL, "Progman", NULL);
    if (shell != NULL)
    {
        tid = GetWindowThreadProcessId(shell, &pid);
    }

    // We are possibly running on a private window station and desktop: we must
    // switch to the default (which we suppose is where we will find the
    // running shell).
    else
    {
        HWINSTA saveWinSta = GetProcessWindowStation(); 
        HDESK saveDesk = GetThreadDesktop(GetCurrentThreadId()); 
        HWINSTA winSta = NULL;
        HDESK desk = NULL;
        BOOL changeFlag = FALSE;
        BOOL dummy = saveWinSta != NULL &&
                     saveDesk != NULL &&
                     (winSta = OpenWindowStation("WinSta0", FALSE,
                                                 MAXIMUM_ALLOWED)) != NULL &&
                     (changeFlag = SetProcessWindowStation(winSta)) != 0 &&
                     (desk = OpenDesktop("Default", 0, FALSE,
                                          MAXIMUM_ALLOWED)) != NULL &&
                     SetThreadDesktop(desk) != 0;

        // Now find the window and process on this desktop
        shell = FindWindowEx(NULL, NULL, "Progman", NULL);
        if (shell != NULL) 
        {
            tid = GetWindowThreadProcessId(shell, &pid);
        }

        // Restore our own window station and desktop
        if (changeFlag)
        {
            SetProcessWindowStation(saveWinSta);
            SetThreadDesktop(saveDesk);
        }

        // Close temporary objects
        if (winSta != NULL)
            CloseWindowStation(winSta);
        if (desk != NULL) 
            CloseDesktop(desk);
    }

    //
    // If we have a process id, use that to get the process handle and 
    // from there the process' access token.
    //
    if (pid != 0)
    {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProc != NULL)
        {
            OpenProcessToken(hProc, access, &hTok) || (hTok = NULL);
            CloseHandle(hProc);
        }
    }

    // Return token if we got one
    return hTok;
}       

// impersonate logged-on user as client
BOOL
ImpersonateClient()
{
    DWORD	dwDesiredAccess = TOKEN_ALL_ACCESS;
    HANDLE	hUserToken = GetUserToken(dwDesiredAccess);
	
    if (hUserToken == NULL)
        return FALSE;
    if (ImpersonateLoggedOnUser(hUserToken) == 0)
    {
        LogEvent(EVENTLOG_ERROR_TYPE, MSG_FLUSH_IMPERSONATE_ERROR,
                  NULL);
        return FALSE;
    }
    return TRUE;
}
	
/////////////////////////////////////////////////////////////////////
//
// Thread proc
//
DWORD WINAPI 
afsd_ServiceFlushVolumesThreadProc(LPVOID lpParam)
{
    FLUSHVOLTHREADINFO ThreadInfo;
    PFLUSHVOLTHREADINFO pThreadInfo = (PFLUSHVOLTHREADINFO) lpParam; 
    HANDLE	arHandles[2] = {0};
    DWORD	dwWaitState = 0;

    // thread running - get handles
    ThreadInfo.hEventPowerEvent = pThreadInfo->hEventPowerEvent;
    ThreadInfo.hEventResumeMain = pThreadInfo->hEventResumeMain;
    ThreadInfo.hEventTerminate  = pThreadInfo->hEventTerminate;

    // setup to wait
    arHandles[0] = ThreadInfo.hEventTerminate;
    arHandles[1] = ThreadInfo.hEventPowerEvent;

    // do stuff ..
    while (1)
    {
        // wait for an event to happen
        dwWaitState = WaitForMultipleObjectsEx(2, arHandles, FALSE, INFINITE, FALSE);

        switch (dwWaitState)
        {
        case WAIT_OBJECT_0:
            // termination signaled
            RevertToSelf();
            CheckAndCloseHandle(ThreadInfo.hEventPowerEvent);
            CheckAndCloseHandle(ThreadInfo.hEventResumeMain);
            CheckAndCloseHandle(ThreadInfo.hEventTerminate);
            ExitThread(0);
            break;

        case WAIT_OBJECT_0+1:
            // Power event 
            // - flush 'em!
            if (ImpersonateClient())
            {
                afsd_ServicePerformFlushVolumes();
            }
            // acknowledge event
            ResetEvent(ThreadInfo.hEventPowerEvent);
            break;

        case WAIT_ABANDONED_0:
        case WAIT_ABANDONED_0+1:
        case WAIT_IO_COMPLETION:
        case WAIT_TIMEOUT:
            // sno*
            LogEvent(EVENTLOG_WARNING_TYPE,
                      MSG_FLUSH_UNEXPECTED_EVENT, NULL);
            break;

        }	// end switch

        // signal back to waiting mainline
        SetEvent(ThreadInfo.hEventResumeMain);

    }	// end while

    // I suppose we never get here
    ExitThread(0);
}       

/////////////////////////////////////////////////////////////////////
//
// Mainline thread routines
//

VOID	
CheckAndCloseHandle(HANDLE thisHandle)
{
    if (thisHandle != NULL)
    {
        CloseHandle(thisHandle);
        thisHandle = NULL;
    }
}

//
// Thread Creation
//
BOOL
PowerNotificationThreadCreate()
{
    BOOL	bSuccess = FALSE;
    DWORD	dwThreadId = 0;
    char    eventName[MAX_PATH];
	
    do 
    {
        // create power event notification event
        // bManualReset=TRUE, bInitialState=FALSE
        gThreadInfo.hEventPowerEvent = CreateEvent(NULL, TRUE, FALSE, 
                                                   TEXT("afsd_flushvol_EventPowerEvent"));
        if ( GetLastError() == ERROR_ALREADY_EXISTS )
            afsi_log("Event Object Already Exists: %s", eventName);
        if (gThreadInfo.hEventPowerEvent == NULL)
            break;			

        // create mainline resume event
        // bManualReset=FALSE, bInitialState=FALSE
        gThreadInfo.hEventResumeMain = CreateEvent(NULL, FALSE, FALSE, 
                                                   TEXT("afsd_flushvol_EventResumeMain"));
        if ( GetLastError() == ERROR_ALREADY_EXISTS )
            afsi_log("Event Object Already Exists: %s", eventName);
        if (gThreadInfo.hEventResumeMain == NULL)
            break;			

        // create thread terminate event
        // bManualReset=FALSE, bInitialState=FALSE
        gThreadInfo.hEventTerminate = CreateEvent(NULL, FALSE, FALSE, 
                                                  TEXT("afsd_flushvol_EventTerminate"));
        if ( GetLastError() == ERROR_ALREADY_EXISTS )
            afsi_log("Event Object Already Exists: %s", eventName);
        if (gThreadInfo.hEventTerminate == NULL)
            break;			

        // good so far - create thread
        gThreadHandle = CreateThread(NULL, 0,
                                     afsd_ServiceFlushVolumesThreadProc,
                                     (LPVOID) &gThreadInfo,
                                     0, &dwThreadId);
		
        if (!gThreadHandle)
            break;

        bSuccess = TRUE;

    } while (0);


    if (!bSuccess)
    {
        CheckAndCloseHandle(gThreadInfo.hEventPowerEvent);
        CheckAndCloseHandle(gThreadInfo.hEventResumeMain);
        CheckAndCloseHandle(gThreadInfo.hEventTerminate);
        CheckAndCloseHandle(gThreadHandle);
    }
		
    return bSuccess;
}

//
// Thread Notification
//
BOOL
PowerNotificationThreadNotify()
{
    DWORD		dwRet = 0;
    BOOL		bRet  = FALSE;

    // Notify thread of power event, and wait for the HardDead timeout period
    dwRet = SignalObjectAndWait(
                gThreadInfo.hEventPowerEvent,	// object to signal
                gThreadInfo.hEventResumeMain,	// object to watch
		HardDeadtimeout*1000,		// timeout (ms)
		FALSE				// alertable
		);

    if (dwRet == WAIT_OBJECT_0)
        bRet = TRUE;

    return bRet;
}

//
// Thread Termination
//
VOID
PowerNotificationThreadExit()
{
    // ExitThread
    if (gThreadHandle)
    {
        SetEvent(gThreadInfo.hEventTerminate);
        WaitForSingleObject(gThreadHandle, INFINITE);
        CloseHandle(gThreadHandle);
    }
}

