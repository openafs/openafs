/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AFSD_H_ENV__
#define __AFSD_H_ENV__ 1

#define USE_BPLUS 1

#include <afs/param.h>

#ifndef DJGPP
BOOL InitClass(HANDLE);
BOOL InitInstance(HANDLE, int);

LONG APIENTRY MainWndProc(HWND, unsigned int, unsigned int, long);
BOOL APIENTRY About(HWND, unsigned int, unsigned int, long);
#endif /* !DJGPP */

#ifndef DJGPP
#include <nb30.h>
#else /* DJGPP */
#include <sys/farptr.h>
#include <go32.h>
#include "dosdefs95.h"
#include "largeint95.h"
#endif /* !DJGPP */

#include "cm.h"

#include <osi.h>
#include <afs/vldbint.h>
#include <afs/afsint.h>
#include <afs/prs_fs.h>

#include "cm_config.h"
#include "cm_user.h"
#include "cm_scache.h"
#include "cm_callback.h"
#ifdef DISKCACHE95
#include "cm_diskcache95.h"
#endif /* DISKCACHE95 */
#include "cm_conn.h"
#include "cm_aclent.h"
#include "cm_server.h"
#include "cm_cell.h"
#include "cm_volstat.h"
#include "cm_volume.h"
#include "cm_dcache.h"
#include "cm_access.h"
#include "cm_utils.h"
#include "cm_vnodeops.h"
#include "cm_dir.h"
#include "cm_daemon.h"
#include "cm_ioctl.h"
#include "cm_dnlc.h"
#include "cm_buf.h"
#include "cm_memmap.h"
#include "cm_freelance.h"
#include "smb_ioctl.h"
#include "afsd_init.h"
#ifdef DJGPP
#include "afs/afsmsg95.h"
#else
#include "afsd_eventlog.h"
#endif


#define AFS_DAEMON_SERVICE_NAME AFSREG_CLT_SVC_NAME
#define AFS_DAEMON_EVENT_NAME   AFSREG_CLT_SW_NAME

void afs_exit();

extern void afsi_log(char *pattern, ...);

/* globals from the base afsd */

extern int cm_logChunkSize;
extern int cm_chunkSize;

extern cm_volume_t *cm_rootVolumep;

extern cm_cell_t *cm_rootCellp;

extern cm_fid_t cm_rootFid;

extern cm_scache_t *cm_rootSCachep;

extern osi_log_t *afsd_logp;

extern char cm_mountRoot[];
extern DWORD cm_mountRootLen;

extern char cm_CachePath[];

extern BOOL isGateway;

extern BOOL reportSessionStartups;

#ifdef AFS_FREELANCE_CLIENT
extern char *cm_FakeRootDir;				// the fake root.afs directory

extern int cm_noLocalMountPoints;			// no. of fake mountpoints

extern cm_localMountPoint_t* cm_localMountPoints;	// array of fake mountpoints

extern int cm_fakeDirSize;				// size (in bytes) of fake root.afs directory

extern int cm_fakeDirCallback;				// state of the fake root.afs directory. indicates
													// if it needs to be refreshed

extern int cm_fakeGettingCallback;			// 1 if currently updating the fake root.afs directory,
													// 0 otherwise

extern int cm_fakeDirVersion;				// the version number of the root.afs directory. used 
#endif /* AFS_FREELANCE_CLIENT */

extern int cm_dnsEnabled;
extern int cm_freelanceEnabled;

extern long rx_mtu;

extern HANDLE WaitToTerminate;

#undef  DFS_SUPPORT
#define LOG_PACKET 1
#undef  NOTSERVICE
#define LOCK_TESTING 1

#define WORKER_THREADS 10

#define AFSD_HOOK_DLL  "afsdhook.dll"
#define AFSD_INIT_HOOK "AfsdInitHook"
typedef BOOL ( APIENTRY * AfsdInitHook )(void);
#define AFSD_RX_STARTED_HOOK "AfsdRxStartedHook"
typedef BOOL ( APIENTRY * AfsdRxStartedHook )(void);
#define AFSD_SMB_STARTED_HOOK "AfsdSmbStartedHook"
typedef BOOL ( APIENTRY * AfsdSmbStartedHook )(void);
#define AFSD_STARTED_HOOK "AfsdStartedHook"
typedef BOOL ( APIENTRY * AfsdStartedHook )(void);
#define AFSD_DAEMON_HOOK "AfsdDaemonHook"
typedef BOOL ( APIENTRY * AfsdDaemonHook )(void);
#define AFSD_STOPPING_HOOK "AfsdStoppingHook"
typedef BOOL ( APIENTRY * AfsdStoppingHook )(void);
#define AFSD_STOPPED_HOOK "AfsdStoppedHook"
typedef BOOL ( APIENTRY * AfsdStoppedHook )(void);

#define SERVICE_CONTROL_CUSTOM_DUMP 128
#endif /* AFSD_H_ENV */
