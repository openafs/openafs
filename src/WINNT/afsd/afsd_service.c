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
#include <string.h>
#include <setjmp.h>
#include "afsd.h"
#include "afsd_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#include <osi.h>

#ifdef DEBUG
//#define NOTSERVICE
#endif
#ifdef _DEBUG
#include <crtdbg.h>
#endif

/*
// The following is defined if you want to receive Power notifications,
// including Hibernation, and also subsequent flushing of AFS volumes
//
#define REGISTER_POWER_NOTIFICATIONS
//
// Check
*/
#include "afsd_flushvol.h"

extern void afsi_log(char *pattern, ...);

HANDLE WaitToTerminate;

int GlobalStatus;

unsigned int MainThreadId;
jmp_buf notifier_jmp;

extern int traceOnPanic;
extern HANDLE afsi_file;

/*
 * Notifier function for use by osi_panic
 */
static void afsd_notifier(char *msgp, char *filep, long line)
{
	char tbuffer[100];
	char *ptbuf[1];
	HANDLE h;

	if (filep)
		sprintf(tbuffer, "Error at file %s, line %d: %s",
			filep, line, msgp);
	else
		sprintf(tbuffer, "Error at unknown location: %s", msgp);

	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	ptbuf[0] = tbuffer;
	ReportEvent(h, EVENTLOG_ERROR_TYPE, 0, line, NULL, 1, 0, ptbuf, NULL);
	DeregisterEventSource(h);

	GlobalStatus = line;

	osi_LogEnable(afsd_logp);

	afsd_ForceTrace(TRUE);

    afsi_log("--- begin dump ---");
    cm_DumpSCache(afsi_file, "a");
#ifdef keisa
    cm_dnlcDump(afsi_file, "a");
#endif
    cm_DumpBufHashTable(afsi_file, "a");
    smb_DumpVCP(afsi_file, "a");			
    afsi_log("--- end   dump ---");
    
    DebugBreak();	

	SetEvent(WaitToTerminate);

	if (GetCurrentThreadId() == MainThreadId)
		longjmp(notifier_jmp, 1);
	else
		ExitThread(1);
}

/*
 * For use miscellaneously in smb.c; need to do better
 */
static int DummyMessageBox(HWND h, LPCTSTR l1, LPCTSTR l2, UINT ui)
{
	return 0;
}

static SERVICE_STATUS		ServiceStatus;
static SERVICE_STATUS_HANDLE	StatusHandle;

DWORD
afsd_ServiceFlushVolume(DWORD dwlpEventData)
{
    DWORD   dwRet = ERROR_NETWORK_BUSY; /* or NO_ERROR */

    /*
    **  If UI bit is not set, user interaction is not possible
    **      BUT, since we are a NON-interactive service, and therefore
    **  have NO user I/O, it doesn't much matter.
    **  This benign code left here as example of how to find this out
    */
    BOOL bUI = (dwlpEventData & 1);

    /* flush volume */
    if ( PowerNotificationThreadNotify() )
    {
        dwRet = NO_ERROR;
    }

    else
    {
        /* flush was unsuccessful, or timeout - deny shutdown */
        dwRet = ERROR_NETWORK_BUSY;
    }

    /*      to deny hibernate, simply return
    //      any value besides NO_ERROR.
    //      For example:
    //      dwRet = ERROR_NETWORK_BUSY;
    */

    return dwRet;
}

/*
**    Extended ServiceControlHandler that provides Event types
**    for monitoring Power events, for example.
*/
DWORD
afsd_ServiceControlHandlerEx(
              DWORD  ctrlCode,
              DWORD  dwEventType,
              LPVOID lpEventData,
              LPVOID lpContext
              )
{
	HKEY parmKey;
	DWORD dummyLen, doTrace;
	long code;
    DWORD dwRet = ERROR_CALL_NOT_IMPLEMENTED;

	switch (ctrlCode) 
    {
    case SERVICE_CONTROL_STOP:
        /* Shutdown RPC */
        RpcMgmtStopServerListening(NULL);

        /* Force trace if requested */
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            AFSConfigKeyName,
                            0, KEY_QUERY_VALUE, &parmKey);
        if (code != ERROR_SUCCESS)
            goto doneTrace;

        dummyLen = sizeof(doTrace);
        code = RegQueryValueEx(parmKey, "TraceOnShutdown",
                               NULL, NULL,
                               (BYTE *) &doTrace, &dummyLen);
        RegCloseKey (parmKey);
        if (code != ERROR_SUCCESS)
            doTrace = 0;
        if (doTrace)
            afsd_ForceTrace(FALSE);

      doneTrace:
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 1;
        ServiceStatus.dwWaitHint = 10000;
        ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_POWEREVENT;
        SetServiceStatus(StatusHandle, &ServiceStatus);
        SetEvent(WaitToTerminate);
        dwRet = NO_ERROR;
        break;

    case SERVICE_CONTROL_INTERROGATE:
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 0;
        ServiceStatus.dwWaitHint = 0;
        ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_POWEREVENT;
        SetServiceStatus(StatusHandle, &ServiceStatus);
        dwRet = NO_ERROR;
        break;

		/* XXX handle system shutdown */
		/* XXX handle pause & continue */
		case SERVICE_CONTROL_POWEREVENT:                                              
		{                                                                                     
			/*                                                                                
            **	dwEventType of this notification == WPARAM of WM_POWERBROADCAST               
			**	Return NO_ERROR == return TRUE for that message, i.e. accept request          
			**	Return any error code to deny request,                                        
			**	i.e. as if returning BROADCAST_QUERY_DENY                                     
			*/                                                                                
			switch((int) dwEventType)                                                         
            {                                                                               
			case PBT_APMQUERYSUSPEND:                                                         
			case PBT_APMQUERYSTANDBY:                                                         
                                                                                            
#ifdef	REGISTER_POWER_NOTIFICATIONS				                                      
				/* handle event */                                                            
				dwRet = afsd_ServiceFlushVolume((DWORD) lpEventData);                         
#else                                                                                       
				dwRet = NO_ERROR;                                                             
#endif                                                                                      
				break;                                                                        
							                                                                  
            /* allow remaining case PBT_WhatEver */                                           
			case PBT_APMSUSPEND:                                                              
			case PBT_APMSTANDBY:                                                              
			case PBT_APMRESUMECRITICAL:                                                       
			case PBT_APMRESUMESUSPEND:                                                        
			case PBT_APMRESUMESTANDBY:                                                        
			case PBT_APMBATTERYLOW:                                                           
			case PBT_APMPOWERSTATUSCHANGE:                                                    
			case PBT_APMOEMEVENT:                                                             
			case PBT_APMRESUMEAUTOMATIC:                                                      
			default:                                                                          
				dwRet = NO_ERROR;                                                             
            }
        }
    }		/* end switch(ctrlCode) */                                                        
	return dwRet;   
}

#if 1
/* This code was moved to Drivemap.cpp*/
/* Mount a drive into AFS if the user wants us to */
/* DEE Could check first if we are run as SYSTEM */
void CheckMountDrive()
{
        char szAfsPath[_MAX_PATH];
        char szDriveToMapTo[5];
        DWORD dwResult;
        char szKeyName[256];
        HKEY hKey;
        DWORD dwIndex = 0;
        DWORD dwDriveSize;
        DWORD dwSubMountSize;
        char szSubMount[256];
        DWORD dwType;

        sprintf(szKeyName, "%s\\GlobalAutoMapper", AFSConfigKeyName);

	dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE, &hKey);
	if (dwResult != ERROR_SUCCESS)
                return;

        while (1) {
                dwDriveSize = sizeof(szDriveToMapTo);
                dwSubMountSize = sizeof(szSubMount);
                dwResult = RegEnumValue(hKey, dwIndex++, szDriveToMapTo, &dwDriveSize, 0, &dwType, szSubMount, &dwSubMountSize);
                if (dwResult != ERROR_MORE_DATA) {
                        if (dwResult != ERROR_SUCCESS) {
                                if (dwResult != ERROR_NO_MORE_ITEMS)
                                        afsi_log("Failed to read GlobalAutoMapper values: %d\n", dwResult);
                                break;
                        }
                }
                
#if 0
                sprintf(szAfsPath, "\\Device\\LanmanRedirector\\%s\\%s-AFS\\%s", szDriveToMapTo, cm_HostName, szSubMount);
        
                dwResult = DefineDosDevice(DDD_RAW_TARGET_PATH, szDriveToMapTo, szAfsPath);
#else
		{
		    NETRESOURCE nr;
		    memset (&nr, 0x00, sizeof(NETRESOURCE));
 
		    sprintf(szAfsPath,"\\\\%s-AFS\\%s",cm_HostName,szSubMount);
		    
		    nr.dwScope = RESOURCE_GLOBALNET;
		    nr.dwType=RESOURCETYPE_DISK;
		    nr.lpLocalName=szDriveToMapTo;
		    nr.lpRemoteName=szAfsPath;
		    nr.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
		    nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;

		    dwResult = WNetAddConnection2(&nr,NULL,NULL,FALSE);
		}
#endif
                afsi_log("GlobalAutoMap of %s to %s %s", szDriveToMapTo, szSubMount, dwResult ? "succeeded" : "failed");
        }        

        RegCloseKey(hKey);
}
#endif

typedef BOOL ( APIENTRY * AfsdInitHook )(void);
#define AFSD_INIT_HOOK "AfsdInitHook"
#define AFSD_HOOK_DLL  "afsdhook.dll"

void afsd_Main(DWORD argc, LPTSTR *argv)
{
	long code;
	char *reason;
	int jmpret;
    HANDLE hInitHookDll;
    AfsdInitHook initHook;

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/ | 
                   _CRTDBG_CHECK_CRT_DF /* | _CRTDBG_DELAY_FREE_MEM_DF */ );
#endif 

	osi_InitPanic(afsd_notifier);
	osi_InitTraceOption();

	GlobalStatus = 0;

	WaitToTerminate = CreateEvent(NULL, TRUE, FALSE, TEXT("afsd_service_WaitToTerminate"));
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", TEXT("afsd_service_WaitToTerminate"));

#ifndef NOTSERVICE
	StatusHandle = RegisterServiceCtrlHandlerEx(AFS_DAEMON_SERVICE_NAME,
			(LPHANDLER_FUNCTION_EX) afsd_ServiceControlHandlerEx,
                                                 NULL /* user context */
                                                 );

	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwWin32ExitCode = NO_ERROR;
	ServiceStatus.dwCheckPoint = 1;
	ServiceStatus.dwWaitHint = 30000;
    /* accept Power Events */
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_POWEREVENT;
	SetServiceStatus(StatusHandle, &ServiceStatus);
#endif

    {       
    HANDLE h; char *ptbuf[1];
    h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
    ptbuf[0] = "AFS start pending";
    ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, ptbuf, NULL);
    DeregisterEventSource(h);
    }

#ifdef REGISTER_POWER_NOTIFICATIONS
    /* create thread used to flush cache */
    PowerNotificationThreadCreate();
#endif

	afsi_start();

    /* allow an exit to be called prior to any initialization */
    hInitHookDll = LoadLibrary(AFSD_HOOK_DLL);
    if (hInitHookDll)
    {
        BOOL hookRc = FALSE;
        initHook = ( AfsdInitHook ) GetProcAddress(hInitHookDll, AFSD_INIT_HOOK);
        if (initHook)
        {
            hookRc = initHook();
        }
        FreeLibrary(hInitHookDll);
               
        if (hookRc == FALSE)
        {
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwWin32ExitCode = NO_ERROR;
            ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 0;
            ServiceStatus.dwControlsAccepted = 0;
            SetServiceStatus(StatusHandle, &ServiceStatus);
                       
            /* exit if initialization failed */
            return;
        }
        else
        {
            /* allow another 15 seconds to start */
            ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            ServiceStatus.dwServiceSpecificExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
            ServiceStatus.dwWin32ExitCode = NO_ERROR;
            ServiceStatus.dwCheckPoint = 2;
            ServiceStatus.dwWaitHint = 20000;
            /* accept Power Events */
            ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_POWEREVENT;
            SetServiceStatus(StatusHandle, &ServiceStatus);
        }
    }

    MainThreadId = GetCurrentThreadId();
	jmpret = setjmp(notifier_jmp);

	if (jmpret == 0) {
		code = afsd_InitCM(&reason);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

#ifndef NOTSERVICE
        ServiceStatus.dwCheckPoint++;
        ServiceStatus.dwWaitHint -= 5000;
        SetServiceStatus(StatusHandle, &ServiceStatus);
#endif
		code = afsd_InitDaemons(&reason);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

#ifndef NOTSERVICE
        ServiceStatus.dwCheckPoint++;
        ServiceStatus.dwWaitHint -= 5000;
        SetServiceStatus(StatusHandle, &ServiceStatus);
#endif
		code = afsd_InitSMB(&reason, DummyMessageBox);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

#ifndef NOTSERVICE
		ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		ServiceStatus.dwWin32ExitCode = NO_ERROR;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWaitHint = 0;

        /* accept Power events */
		ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_POWEREVENT;
		SetServiceStatus(StatusHandle, &ServiceStatus);
#endif
        {
	    HANDLE h; char *ptbuf[1];
		h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
		ptbuf[0] = "AFS running";
		ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, ptbuf, NULL);
		DeregisterEventSource(h);
        }
	}

    /* Check if we should mount a drive into AFS */
    CheckMountDrive();

	WaitForSingleObject(WaitToTerminate, INFINITE);

    ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
	ServiceStatus.dwWin32ExitCode = NO_ERROR;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 5000;
	ServiceStatus.dwControlsAccepted = 0;
	SetServiceStatus(StatusHandle, &ServiceStatus);

    {   
    HANDLE h; char *ptbuf[1];
	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	ptbuf[0] = "AFS quitting";
	ReportEvent(h, GlobalStatus ? EVENTLOG_ERROR_TYPE : EVENTLOG_INFORMATION_TYPE,
                0, 0, NULL, 1, 0, ptbuf, NULL);
    DeregisterEventSource(h);
    }

#ifdef	REGISTER_POWER_NOTIFICATIONS
	/* terminate thread used to flush cache */
	PowerNotificationThreadExit();
#endif

    /* Remove the ExceptionFilter */
    SetUnhandledExceptionFilter(NULL);

	ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ServiceStatus.dwWin32ExitCode = GlobalStatus ? ERROR_EXCEPTION_IN_SERVICE : NO_ERROR;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;
	ServiceStatus.dwControlsAccepted = 0;
	SetServiceStatus(StatusHandle, &ServiceStatus);
    return;
}

DWORD __stdcall afsdMain_thread(void* notUsed)
{
	afsd_Main(0, (LPTSTR*)NULL);
    return(0);
}

void main()
{
	SERVICE_TABLE_ENTRY dispatchTable[] = {
		{AFS_DAEMON_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) afsd_Main},
		{NULL, NULL}
	};

    afsd_SetUnhandledExceptionFilter();

	if (!StartServiceCtrlDispatcher(dispatchTable))
    {
        LONG status = GetLastError();
	    if (status == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        {
            DWORD tid;
            CreateThread(NULL, 0, afsdMain_thread, 0, 0, &tid);
		
            printf("Hit <Enter> to terminate OpenAFS Client Service\n");
            getchar();              
            SetEvent(WaitToTerminate);
        }
    }
}
