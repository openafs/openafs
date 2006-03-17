/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/*
	System:		VICE-TWO
	Module:		fssync.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */
#ifdef notdef

/* All this is going away in early 1989 */
int newVLDB;			/* Compatibility flag */

#endif
static int newVLDB = 1;


#ifndef AFS_PTHREAD_ENV
#define USUAL_PRIORITY (LWP_MAX_PRIORITY - 2)

/*
 * stack size increased from 8K because the HP machine seemed to have trouble
 * with the smaller stack
 */
#define USUAL_STACK_SIZE	(24 * 1024)
#endif /* !AFS_PTHREAD_ENV */

/*
   fssync-client.c
   File server synchronization with external volume utilities.
   client-side implementation
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <time.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <errno.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <signal.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "daemon_com.h"
#include "fssync.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"

#ifdef FSSYNC_BUILD_CLIENT

/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)

extern int LogLevel;

static SYNC_client_state fssync_state = { -1, 2040, FSYNC_PROTO_VERSION, 5, 120 };

#ifdef AFS_PTHREAD_ENV
static pthread_mutex_t vol_fsync_mutex;
static volatile vol_fsync_mutex_init = 0;
#define VFSYNC_LOCK \
    assert(pthread_mutex_lock(&vol_fsync_mutex) == 0)
#define VFSYNC_UNLOCK \
    assert(pthread_mutex_unlock(&vol_fsync_mutex) == 0)
#else
#define VFSYNC_LOCK
#define VFSYNC_UNLOCK
#endif

int
FSYNC_clientInit(void)
{
#ifdef AFS_PTHREAD_ENV
    /* this is safe since it gets called with VOL_LOCK held, or before we go multithreaded */
    if (!vol_fsync_mutex_init) {
	assert(pthread_mutex_init(&vol_fsync_mutex, NULL) == 0);
	vol_fsync_mutex_init = 1;
    }
#endif
    return SYNC_connect(&fssync_state);
}

void
FSYNC_clientFinis(void)
{
    SYNC_closeChannel(&fssync_state);
}

int
FSYNC_clientChildProcReconnect(void)
{
    return SYNC_reconnect(&fssync_state);
}

/* fsync client interface */
afs_int32
FSYNC_askfs(SYNC_command * com, SYNC_response * res)
{
    afs_int32 code;

    VFSYNC_LOCK;
    code = SYNC_ask(&fssync_state, com, res);
    VFSYNC_UNLOCK;

    switch (code) {
    case SYNC_OK:
    case SYNC_FAILED:
	break;
    case SYNC_COM_ERROR:
    case SYNC_BAD_COMMAND:
	Log("FSYNC_askfs: fatal FSSYNC protocol error; volume management functionality disabled until next fileserver restart\n");
	break;
    case SYNC_DENIED:
	Log("FSYNC_askfs: FSSYNC request denied for reason=%d\n", res->hdr.reason);
	break;
    default:
	Log("FSYNC_askfs: unknown protocol response %d\n", code);
	break;
    }
    return code;
}

afs_int32
FSYNC_GenericOp(void * ext_hdr, size_t ext_len,
	      int command, int reason,
	      SYNC_response * res_in)
{
    SYNC_response res_l, *res;
    SYNC_command com;

    if (res_in) {
	res = res_in;
    } else {
	res = &res_l;
	res_l.payload.buf = NULL;
	res_l.payload.len = 0;
    }

    memset(&com, 0, sizeof(com));

    com.hdr.programType = programType;
    com.hdr.command = command;
    com.hdr.reason = reason;
    com.hdr.command_len = sizeof(com.hdr) + ext_len;
    com.payload.buf = ext_hdr;
    com.payload.len = ext_len;

    return FSYNC_askfs(&com, res);
}

afs_int32
FSYNC_VolOp(VolumeId volume, char * partition, 
	    int command, int reason,
	    SYNC_response * res)
{
    FSSYNC_VolOp_hdr vcom;

    memset(&vcom, 0, sizeof(vcom));

    vcom.volume = volume;
    if (partition)
	strlcpy(vcom.partName, partition, sizeof(vcom.partName));

    return FSYNC_GenericOp(&vcom, sizeof(vcom), command, reason, res);
}

afs_int32
FSYNC_StatsOp(FSSYNC_StatsOp_hdr * scom, int command, int reason,
	      SYNC_response * res)
{
    return FSYNC_GenericOp(scom, sizeof(*scom), command, reason, res);
}


#endif /* FSSYNC_BUILD_CLIENT */
