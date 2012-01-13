/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006,2008 Sine Nomine Associates
 */

/*
	System:		VICE-TWO
	Module:		fssync.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

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
#include <afs/afs_assert.h>
#include <signal.h>
#include <string.h>

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
#include "common.h"

#ifdef FSSYNC_BUILD_CLIENT

extern int LogLevel;

static SYNC_client_state fssync_state =
    { -1,                    /* file descriptor */
      FSSYNC_ENDPOINT_DECL,  /* server endpoint */
      FSYNC_PROTO_VERSION,   /* protocol version */
      5,                     /* connect retry limit */
      120,                   /* hard timeout */
      "FSSYNC",              /* protocol name string */
    };

#ifdef AFS_PTHREAD_ENV
static pthread_mutex_t vol_fsync_mutex;
static volatile int vol_fsync_mutex_init = 0;
#define VFSYNC_LOCK MUTEX_ENTER(&vol_fsync_mutex)
#define VFSYNC_UNLOCK MUTEX_EXIT(&vol_fsync_mutex)
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
	MUTEX_INIT(&vol_fsync_mutex, "vol fsync", MUTEX_DEFAULT, 0);
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
	Log("FSYNC_askfs: internal FSSYNC protocol error %d\n", code);
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


/**
 *  FSSYNC volume operations client interface.
 *
 * @param[in]    volume     volume id
 * @param[in]    partName   partition name string
 * @param[in]    com        FSSYNC command code
 * @param[in]    reason     FSSYNC reason sub-code
 * @param[out]   res        response message
 *
 * @return operation status
 *    @retval SYNC_OK  success
 */
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

/**
 * verify that the fileserver still thinks we have a volume checked out.
 *
 * In DAFS, a non-fileserver program accesses a volume by checking it out from
 * the fileserver (FSYNC_VOL_OFF or FSYNC_VOL_NEEDVOLUME), and then locks the
 * volume. There is a possibility that the fileserver crashes or restarts for
 * some reason between volume checkout and locking; if this happens, the
 * fileserver could attach the volume before we had a chance to lock it. This
 * function serves to detect if this has happened; it must be called after
 * volume checkout and locking to make sure the fileserver still thinks we
 * have the volume. (If it doesn't, we should try to check it out again.)
 *
 * @param[in] volume    volume ID
 * @param[in] partition partition name string
 * @param[in] command   the command that was used to checkout the volume
 * @param[in] reason    the reason code used to checkout the volume
 *
 * @return operation status
 *  @retval SYNC_OK the fileserver could not have attached the volume since
 *                  it was checked out (either it thinks it is still checked
 *                  out, or it doesn't know about the volume)
 *  @retval SYNC_DENIED fileserver may have restarted since checkout; checkout
 *                      should be reattempted
 *  @retval SYNC_COM_ERROR internal/fatal error
 */
afs_int32
FSYNC_VerifyCheckout(VolumeId volume, char * partition,
                     afs_int32 command, afs_int32 reason)
{
    SYNC_response res;
    FSSYNC_VolOp_info vop;
    afs_int32 code;
    afs_int32 pid;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = &vop;
    res.payload.len = sizeof(vop);

    code = FSYNC_VolOp(volume, partition, FSYNC_VOL_QUERY_VOP, FSYNC_WHATEVER, &res);
    if (code != SYNC_OK) {
	if (res.hdr.reason == FSYNC_NO_PENDING_VOL_OP) {
	    Log("FSYNC_VerifyCheckout: fileserver claims no vop for vol %lu "
	        "part %s; fileserver may have restarted since checkout\n",
	        afs_printable_uint32_lu(volume), partition);
	    return SYNC_DENIED;
	}

	if (res.hdr.reason == FSYNC_UNKNOWN_VOLID ||
	    res.hdr.reason == FSYNC_WRONG_PART) {
	    /* if the fileserver does not know about this volume on this
	     * partition, there's no way it could have attached it, so we're
	     * fine */
	    return SYNC_OK;
	}

	Log("FSYNC_VerifyCheckout: FSYNC_VOL_QUERY_VOP failed for vol %lu "
	    "part %s with code %ld reason %ld\n",
	    afs_printable_uint32_lu(volume), partition,
	    afs_printable_int32_ld(code),
	    afs_printable_int32_ld(res.hdr.reason));
	return SYNC_COM_ERROR;
    }

    pid = getpid();

    /* Check if the current vol op is us. Checking pid is probably enough, but
     * be a little bit paranoid. We could also probably check tid, but I'm not
     * completely confident of its reliability on all platforms (on pthread
     * envs, we coerce a pthread_t to an afs_int32, which is not guaranteed
     * to mean anything significant). */

    if (vop.com.programType == programType && vop.com.pid == pid &&
        vop.com.command == command && vop.com.reason == reason) {

	/* looks like the current pending vol op is the same one as the one
	 * with which we checked it out. success. */
	return SYNC_OK;
    }

    Log("FSYNC_VerifyCheckout: vop for vol %lu part %s does not match "
        "expectations (got pt %ld pid %ld cmd %ld reason %ld, but expected "
        "pt %ld pid %ld cmd %ld reason %ld); fileserver may have restarted "
        "since checkout\n", afs_printable_uint32_lu(volume), partition,
	afs_printable_int32_ld(vop.com.programType),
	afs_printable_int32_ld(vop.com.pid),
	afs_printable_int32_ld(vop.com.command),
	afs_printable_int32_ld(vop.com.reason),
	afs_printable_int32_ld(programType),
	afs_printable_int32_ld(pid),
	afs_printable_int32_ld(command),
	afs_printable_int32_ld(reason));

    return SYNC_DENIED;
}

afs_int32
FSYNC_StatsOp(FSSYNC_StatsOp_hdr * scom, int command, int reason,
	      SYNC_response * res)
{
    return FSYNC_GenericOp(scom, sizeof(*scom), command, reason, res);
}

/**
 * query the volume group cache.
 *
 * @param[in]  part     vice partition path
 * @param[in]  volid    volume id
 * @param[out] qry      query response object
 * @param[out] res      SYNC response message
 *
 * @return operation status
 *    @retval SYNC_OK success
 */
afs_int32
FSYNC_VGCQuery(char * part,
	     VolumeId volid,
	     FSSYNC_VGQry_response_t * qry,
	     SYNC_response *res)
{
    SYNC_response lres;

    if (!res) {
	res = &lres;
    }

    res->hdr.response_len = sizeof(res->hdr);
    res->payload.buf = qry;
    res->payload.len = sizeof(*qry);

    return FSYNC_VolOp(volid, part, FSYNC_VG_QUERY, 0, res);
}

/**
 * perform an update operation on the VGC.
 *
 * @param[in] parent    rw volume
 * @param[in] child     volume id to add
 * @param[in] partition name of vice partition on which this VG resides
 * @param[in] opcode    FSSYNC VG cache opcode
 * @param[in] reason    FSSYNC reason code
 * @param[out] res      SYNC response message
 *
 * @return operation status
 *    @retval SYNC_OK success
 *
 * @internal
 */
static afs_int32
_FSYNC_VGCUpdate(char * partition,
		 VolumeId parent,
		 VolumeId child,
		 int opcode,
		 int reason,
		 SYNC_response *res)
{
    FSSYNC_VGUpdate_command_t vcom;

    memset(&vcom, 0, sizeof(vcom));

    vcom.parent = parent;
    vcom.child = child;
    if (partition)
	strlcpy(vcom.partName, partition, sizeof(vcom.partName));

    return FSYNC_GenericOp(&vcom, sizeof(vcom), opcode, reason, res);
}

/**
 * Add volume to volume group cache.
 *
 * @param[in] parent    rw volume
 * @param[in] child     volume id to add
 * @param[in] partition name of vice partition on which this VG resides
 * @param[in] reason    FSSYNC reason code
 * @param[out] res      SYNC response message
 *
 * @return operation status
 *    @retval SYNC_OK success
 */
afs_int32
FSYNC_VGCAdd(char * partition,
	     VolumeId parent,
	     VolumeId child,
	     int reason,
	     SYNC_response *res)
{
    return _FSYNC_VGCUpdate(partition, parent, child, FSYNC_VG_ADD, reason, res);
}

/**
 * Delete volume from volume group cache.
 *
 * @param[in] parent    rw volume
 * @param[in] child     volume id to add
 * @param[in] partition name of vice partition on which this VG resides
 * @param[in] reason    FSSYNC reason code
 * @param[out] res      SYNC response message
 *
 * @return operation status
 *    @retval SYNC_OK success
 */
afs_int32
FSYNC_VGCDel(char * partition,
	     VolumeId parent,
	     VolumeId child,
	     int reason,
	     SYNC_response *res)
{
    return _FSYNC_VGCUpdate(partition, parent, child, FSYNC_VG_DEL, reason, res);
}

/**
 * perform an asynchronous volume group scan.
 *
 * @param[in] partition   vice partition string
 * @param[in] reason      FSSYNC reason code
 *
 * @note if partition is NULL, all vice partitions will be scanned.
 *
 * @return operation status
 *    @retval SYNC_OK success
 */
afs_int32
FSYNC_VGCScan(char * partition, int reason)
{
    int command;

    if (partition == NULL) {
	command = FSYNC_VG_SCAN_ALL;
	partition = "";
    } else {
	command = FSYNC_VG_SCAN;
    }

    return FSYNC_VolOp(0, partition, command, reason, NULL);
}

#endif /* FSSYNC_BUILD_CLIENT */
