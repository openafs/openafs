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

extern void afsi_log(char *pattern, ...);

extern char AFSConfigKeyName[];

HANDLE WaitToTerminate;

int GlobalStatus;

unsigned int MainThreadId;
jmp_buf notifier_jmp;

extern int traceOnPanic;

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

	if (traceOnPanic) {
		_asm int 3h;
	}

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

void afsd_ServiceControlHandler(DWORD ctrlCode)
{
	HKEY parmKey;
	DWORD dummyLen, doTrace;
	long code;

	switch (ctrlCode) {
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
			ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
			SetServiceStatus(StatusHandle, &ServiceStatus);
			SetEvent(WaitToTerminate);
			break;
		case SERVICE_CONTROL_INTERROGATE:
			ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			ServiceStatus.dwWin32ExitCode = NO_ERROR;
			ServiceStatus.dwCheckPoint = 0;
			ServiceStatus.dwWaitHint = 0;
			ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
			SetServiceStatus(StatusHandle, &ServiceStatus);
			break;
		/* XXX handle system shutdown */
		/* XXX handle pause & continue */
	}
}

/* Mount a drive into AFS if the user wants us to */
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
                
                sprintf(szAfsPath, "\\Device\\LanmanRedirector\\%s\\%s-AFS\\%s", szDriveToMapTo, cm_HostName, szSubMount);
        
                dwResult = DefineDosDevice(DDD_RAW_TARGET_PATH, szDriveToMapTo, szAfsPath);
                afsi_log("GlobalAutoMap of %s to %s %s", szDriveToMapTo, szSubMount, dwResult ? "succeeded" : "failed");
        }        

        RegCloseKey(hKey);
}

void afsd_Main()
{
	long code;
	char *reason;
	int jmpret;

	osi_InitPanic(afsd_notifier);

	GlobalStatus = 0;

	WaitToTerminate = CreateEvent(NULL, TRUE, FALSE, NULL);

	StatusHandle = RegisterServiceCtrlHandler(AFS_DAEMON_SERVICE_NAME,
			(LPHANDLER_FUNCTION) afsd_ServiceControlHandler);

	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwWin32ExitCode = NO_ERROR;
	ServiceStatus.dwCheckPoint = 1;
	ServiceStatus.dwWaitHint = 15000;
	ServiceStatus.dwControlsAccepted = 0;
	SetServiceStatus(StatusHandle, &ServiceStatus);
{       
        HANDLE h; char *ptbuf[1];
	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	ptbuf[0] = "AFS start pending";
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, ptbuf, NULL);
	DeregisterEventSource(h);
}

	afsi_start();

	MainThreadId = GetCurrentThreadId();
	jmpret = setjmp(notifier_jmp);

	if (jmpret == 0) {
		code = afsd_InitCM(&reason);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

		code = afsd_InitDaemons(&reason);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

		code = afsd_InitSMB(&reason, DummyMessageBox);
		if (code != 0)
			osi_panic(reason, __FILE__, __LINE__);

		ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		ServiceStatus.dwWin32ExitCode = NO_ERROR;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWaitHint = 0;
		ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		SetServiceStatus(StatusHandle, &ServiceStatus);
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

{   
        HANDLE h; char *ptbuf[1];
	h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
	ptbuf[0] = "AFS quitting";
	ReportEvent(h,
		GlobalStatus ? EVENTLOG_ERROR_TYPE : EVENTLOG_INFORMATION_TYPE,
		0, 0, NULL, 1, 0, ptbuf, NULL);
	DeregisterEventSource(h);
}

	ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ServiceStatus.dwWin32ExitCode = NO_ERROR;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	SetServiceStatus(StatusHandle, &ServiceStatus);
}

void _CRTAPI1 main()
{
	LONG status = ERROR_SUCCESS;
	SERVICE_TABLE_ENTRY dispatchTable[] = {
		{AFS_DAEMON_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) afsd_Main},
		{NULL, NULL}
	};

	if (!StartServiceCtrlDispatcher(dispatchTable))
		status = GetLastError();
}
