/*
 * Copyright 2006-2008, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * salvsync-client.c
 *
 * OpenAFS demand attach fileserver
 * Salvage server synchronization with fileserver.
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
#include <assert.h>
#include <signal.h>
#include <string.h>

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "salvsync.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include <rx/rx_queue.h>

/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)


#ifdef AFS_DEMAND_ATTACH_FS
/*
 * SALVSYNC is a feature specific to the demand attach fileserver
 */

extern int LogLevel;
extern int VInit;
extern pthread_mutex_t vol_salvsync_mutex;

static SYNC_client_state salvsync_client_state = 
    { -1,                     /* file descriptor */
      SALVSYNC_ENDPOINT_DECL, /* server endpoint */
      SALVSYNC_PROTO_VERSION, /* protocol version */
      5,                      /* connect retry limit */
      120,                    /* hard timeout */
      "SALVSYNC",             /* protocol name string */
    };

/*
 * client-side routines
 */

int
SALVSYNC_clientInit(void)
{
    return SYNC_connect(&salvsync_client_state);
}

int
SALVSYNC_clientFinis(void)
{
    SYNC_closeChannel(&salvsync_client_state);
    return 1;
}

int
SALVSYNC_clientReconnect(void)
{
    return SYNC_reconnect(&salvsync_client_state);
}

afs_int32
SALVSYNC_askSalv(SYNC_command * com, SYNC_response * res)
{
    afs_int32 code;
    SALVSYNC_command_hdr * scom = com->payload.buf;

    scom->hdr_version = SALVSYNC_PROTO_VERSION;

    VSALVSYNC_LOCK;
    code = SYNC_ask(&salvsync_client_state, com, res);
    VSALVSYNC_UNLOCK;

    switch (code) {
    case SYNC_OK:
    case SYNC_FAILED:
      break;
    case SYNC_COM_ERROR:
    case SYNC_BAD_COMMAND:
	Log("SALVSYNC_askSalv: fatal SALVSYNC protocol error; online salvager functionality disabled until next fileserver restart\n");
	break;
    case SYNC_DENIED:
	Log("SALVSYNC_askSalv: SALVSYNC request denied for reason=%d\n", res->hdr.reason);
	break;
    default:
	Log("SALVSYNC_askSalv: unknown protocol response %d\n", code);
	break;
    }

    return code;
}

afs_int32
SALVSYNC_SalvageVolume(VolumeId volume, char *partName, int command, int reason, 
		       afs_uint32 prio, SYNC_response * res_in)
{
    SYNC_command com;
    SYNC_response res_l, *res;
    SALVSYNC_command_hdr scom;
    SALVSYNC_response_hdr sres;
    int n, tot;

    memset(&com, 0, sizeof(com));
    memset(&scom, 0, sizeof(scom));

    if (res_in) {
	res = res_in;
    } else {
	memset(&res_l, 0, sizeof(res_l));
	memset(&sres, 0, sizeof(sres));
	res_l.payload.buf = (void *) &sres;
	res_l.payload.len = sizeof(sres);
	res = &res_l;
    }

    com.payload.buf = (void *) &scom;
    com.payload.len = sizeof(scom);
    com.hdr.command = command;
    com.hdr.reason = reason;
    com.hdr.command_len = sizeof(com.hdr) + sizeof(scom);
    scom.volume = volume;
    scom.parent = volume;
    scom.prio = prio;

    if (partName) {
	strlcpy(scom.partName, partName, sizeof(scom.partName));
    } else {
	scom.partName[0] = '\0';
    }

    return SALVSYNC_askSalv(&com, res);
}

afs_int32
SALVSYNC_LinkVolume(VolumeId parent, 
		    VolumeId clone,
		    char * partName,
		    SYNC_response * res_in)
{
    SYNC_command com;
    SYNC_response res_l, *res;
    SALVSYNC_command_hdr scom;
    SALVSYNC_response_hdr sres;
    int n, tot;

    memset(&com, 0, sizeof(com));
    memset(&scom, 0, sizeof(scom));

    if (res_in) {
	res = res_in;
    } else {
	memset(&res_l, 0, sizeof(res_l));
	memset(&sres, 0, sizeof(sres));
	res_l.payload.buf = (void *) &sres;
	res_l.payload.len = sizeof(sres);
	res = &res_l;
    }

    com.payload.buf = (void *) &scom;
    com.payload.len = sizeof(scom);
    com.hdr.command = SALVSYNC_OP_LINK;
    com.hdr.reason = SALVSYNC_REASON_WHATEVER;
    com.hdr.command_len = sizeof(com.hdr) + sizeof(scom);
    scom.volume = clone;
    scom.parent = parent;

    if (partName) {
	strlcpy(scom.partName, partName, sizeof(scom.partName));
    } else {
	scom.partName[0] = '\0';
    }

    return SALVSYNC_askSalv(&com, res);
}

#endif /* AFS_DEMAND_ATTACH_FS */
