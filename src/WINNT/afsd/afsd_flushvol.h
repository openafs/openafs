//
//	AFSD_FLUSHVOL.H
// 
//	Include file for routines that handle flushing AFS volumes
//	in response to System Power event notification such as
//	Hibernate request.
//
/////////////////////////////////////////////////////////////////////

#ifndef _AFSD_FLUSHVOL_H_
#define _AFSD_FLUSHVOL_H_

#include <Winnetwk.h>
#include "fs_utils.h"

// handles 
typedef struct _tagFLUSHVOLTHREADINFO {
	HANDLE	hEventPowerEvent;
	HANDLE	hEventResumeMain;
	HANDLE	hEventTerminate;
} FLUSHVOLTHREADINFO, *PFLUSHVOLTHREADINFO;

// invokes fs.exe pioctl
static afs_int32	afsd_ServicePerformFlushVolumeCmd(char *data);

// thread callback
static DWORD WINAPI afsd_ServiceFlushVolumesThreadProc(LPVOID lpParameter);

// helper function
static VOID	CheckAndCloseHandle(HANDLE thisHandle);

// thread construction/notification/destruction
BOOL	PowerNotificationThreadCreate(VOID);
BOOL	PowerNotificationThreadNotify(VOID);
VOID	PowerNotificationThreadExit(VOID);

// impersonation helper(s)
static HANDLE	GetUserToken(DWORD access);
static BOOL	ImpersonateClient(void);

// event logging
static VOID LogTimingEvent(DWORD dwEventID, LPTSTR lpString1, DWORD dwTime);

#endif	// _AFSD_FLUSHVOL_H_

