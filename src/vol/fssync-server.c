/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006-2010 Sine Nomine Associates
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
   fssync-server.c
   File server synchronization with external volume utilities.
   server-side implementation
 */

/* This controls the size of an fd_set; it must be defined early before
 * the system headers define that type and the macros that operate on it.
 * Its value should be as large as the maximum file descriptor limit we
 * are likely to run into on any platform.  Right now, that is 65536
 * which is the default hard fd limit on Solaris 9 */
#ifndef _WIN32
#define FD_SETSIZE 65536
#endif

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <time.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>
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
#include "daemon_com_inline.h"
#include "fssync.h"
#include "fssync_inline.h"
#include "salvsync.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "volume_inline.h"
#include "partition.h"
#include "vg_cache.h"
#include "common.h"

#ifdef HAVE_POLL
#include <sys/poll.h>
#endif /* HAVE_POLL */

#ifdef USE_UNIX_SOCKETS
#include <sys/un.h>
#include <afs/afsutil.h>
#endif /* USE_UNIX_SOCKETS */

#ifdef FSSYNC_BUILD_SERVER

int (*V_BreakVolumeCallbacks) (VolumeId volume);

#define MAXHANDLERS	4	/* Up to 4 clients; must be at least 2, so that
				 * move = dump+restore can run on single server */
#define MAXOFFLINEVOLUMES 128	/* This needs to be as big as the maximum
				 * number that would be offline for 1 operation.
				 * Current winner is salvage, which needs all
				 * cloned read-only copies offline when salvaging
				 * a single read-write volume */



static struct offlineInfo OfflineVolumes[MAXHANDLERS][MAXOFFLINEVOLUMES];

/**
 * fssync server socket handle.
 */
static SYNC_server_state_t fssync_server_state =
    { OSI_NULLSOCKET,                       /* file descriptor */
      FSSYNC_ENDPOINT_DECL,     /* server endpoint */
      FSYNC_PROTO_VERSION,      /* protocol version */
      5,                        /* bind() retry limit */
      100,                      /* listen() queue depth */
      "FSSYNC",                 /* protocol name string */
    };

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * a queue of volume pointers to salvage in the background.
 */
struct fsync_salv_node {
    struct rx_queue q;
    Volume *vp;                     /**< volume to salvage */
    unsigned char update_salv_prio; /**< whether we should update the salvage priority or not */
};
static struct {
    struct rx_queue head;
    pthread_cond_t cv;
} fsync_salv;

static void * FSYNC_salvageThread(void *);
static void FSYNC_backgroundSalvage(Volume *vp);
#endif /* AFS_DEMAND_ATTACH_FS */

/* Forward declarations */
static void * FSYNC_sync(void *);
static void FSYNC_newconnection(osi_socket afd);
static void FSYNC_com(osi_socket fd);
static void FSYNC_Drop(osi_socket fd);
static void AcceptOn(void);
static void AcceptOff(void);
static void InitHandler(void);
static int AddHandler(osi_socket fd, void (*aproc)(osi_socket));
static int FindHandler(osi_socket afd);
static int FindHandler_r(osi_socket afd);
static int RemoveHandler(osi_socket afd);
#if defined(HAVE_POLL) && defined (AFS_PTHREAD_ENV)
static void CallHandler(struct pollfd *fds, int nfds, int mask);
static void GetHandler(struct pollfd *fds, int maxfds, int events, int *nfds);
#else
static void CallHandler(fd_set * fdsetp);
static void GetHandler(fd_set * fdsetp, int *maxfdp);
#endif
extern int LogLevel;

static afs_int32 FSYNC_com_VolOp(osi_socket fd, SYNC_command * com, SYNC_response * res);

#ifdef AFS_DEMAND_ATTACH_FS
static afs_int32 FSYNC_com_VolError(FSSYNC_VolOp_command * com, SYNC_response * res);
#endif
static afs_int32 FSYNC_com_VolOn(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolOff(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolMove(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolBreakCBKs(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolDone(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolQuery(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolHdrQuery(FSSYNC_VolOp_command * com, SYNC_response * res);
#ifdef AFS_DEMAND_ATTACH_FS
static afs_int32 FSYNC_com_VGUpdate(osi_socket fd, SYNC_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VolOpQuery(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VGQuery(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VGScan(FSSYNC_VolOp_command * com, SYNC_response * res);
static afs_int32 FSYNC_com_VGScanAll(FSSYNC_VolOp_command * com, SYNC_response * res);
#endif /* AFS_DEMAND_ATTACH_FS */

static afs_int32 FSYNC_com_VnQry(osi_socket fd, SYNC_command * com, SYNC_response * res);

static afs_int32 FSYNC_com_StatsOp(osi_socket fd, SYNC_command * com, SYNC_response * res);

static afs_int32 FSYNC_com_StatsOpGeneral(FSSYNC_StatsOp_command * scom, SYNC_response * res);

#ifdef AFS_DEMAND_ATTACH_FS
static afs_int32 FSYNC_com_StatsOpViceP(FSSYNC_StatsOp_command * scom, SYNC_response * res);
static afs_int32 FSYNC_com_StatsOpHash(FSSYNC_StatsOp_command * scom, SYNC_response * res);
static afs_int32 FSYNC_com_StatsOpHdr(FSSYNC_StatsOp_command * scom, SYNC_response * res);
static afs_int32 FSYNC_com_StatsOpVLRU(FSSYNC_StatsOp_command * scom, SYNC_response * res);
#endif

static void FSYNC_com_to_info(FSSYNC_VolOp_command * vcom, FSSYNC_VolOp_info * info);

static int FSYNC_partMatch(FSSYNC_VolOp_command * vcom, Volume * vp, int match_anon);


/*
 * This lock controls access to the handler array. The overhead
 * is minimal in non-preemptive environments.
 */
struct Lock FSYNC_handler_lock;

void
FSYNC_fsInit(void)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t tid;
    pthread_attr_t tattr;
#else /* AFS_PTHREAD_ENV */
    PROCESS pid;
#endif /* AFS_PTHREAD_ENV */

    Lock_Init(&FSYNC_handler_lock);

#ifdef AFS_PTHREAD_ENV
    osi_Assert(pthread_attr_init(&tattr) == 0);
    osi_Assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    osi_Assert(pthread_create(&tid, &tattr, FSYNC_sync, NULL) == 0);
#else /* AFS_PTHREAD_ENV */
    osi_Assert(LWP_CreateProcess
	   (FSYNC_sync, USUAL_STACK_SIZE, USUAL_PRIORITY, (void *)0,
	    "FSYNC_sync", &pid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_DEMAND_ATTACH_FS
    queue_Init(&fsync_salv.head);
    CV_INIT(&fsync_salv.cv, "fsync salv", CV_DEFAULT, 0);
    osi_Assert(pthread_create(&tid, &tattr, FSYNC_salvageThread, NULL) == 0);
#endif /* AFS_DEMAND_ATTACH_FS */
}

#if defined(HAVE_POLL) && defined(AFS_PTHREAD_ENV)
static struct pollfd FSYNC_readfds[MAXHANDLERS];
#else
static fd_set FSYNC_readfds;
#endif


static void *
FSYNC_sync(void * args)
{
    extern int VInit;
    int code;
#ifdef AFS_PTHREAD_ENV
    int tid;
#endif
    SYNC_server_state_t * state = &fssync_server_state;
#ifdef AFS_DEMAND_ATTACH_FS
    VThreadOptions_t * thread_opts;
    int min_vinit = 2;
#else
    /*
     * For non-DAFS, only wait until we begin attaching volumes (instead
     * of waiting until all volumes are attached), since it can take
     * awhile until VInit == 2.
     */
    int min_vinit = 1;
#endif /* AFS_DEMAND_ATTACH_FS */

    /* we must not be called before vol package initialization, since we use
     * vol package mutexes and conds etc */
    osi_Assert(VInit);

    SYNC_getAddr(&state->endpoint, &state->addr);
    SYNC_cleanupSock(state);

#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif

#ifdef AFS_PTHREAD_ENV
    /* set our 'thread-id' so that the host hold table works */
    MUTEX_ENTER(&rx_stats_mutex);	/* protects rxi_pthread_hinum */
    tid = ++rxi_pthread_hinum;
    MUTEX_EXIT(&rx_stats_mutex);
    pthread_setspecific(rx_thread_id_key, (void *)(intptr_t)tid);
    Log("Set thread id %d for FSYNC_sync\n", tid);
#endif /* AFS_PTHREAD_ENV */

    VOL_LOCK;

    while (VInit < min_vinit) {
	/* Let somebody else run until all volumes have been preattached
	 * (DAFS), or we have started attaching volumes (non-DAFS). This
	 * doesn't mean that all volumes have been attached.
	 */
#ifdef AFS_PTHREAD_ENV
	VOL_CV_WAIT(&vol_vinit_cond);
#else /* AFS_PTHREAD_ENV */
	LWP_DispatchProcess();
#endif /* AFS_PTHREAD_ENV */
    }

    VOL_UNLOCK;

    state->fd = SYNC_getSock(&state->endpoint);
    code = SYNC_bindSock(state);
    osi_Assert(!code);

#ifdef AFS_DEMAND_ATTACH_FS
    /*
     * make sure the volume package is incapable of recursively executing
     * salvsync calls on this thread, since there is a possibility of
     * deadlock.
     */
    thread_opts = malloc(sizeof(VThreadOptions_t));
    if (thread_opts == NULL) {
	Log("failed to allocate memory for thread-specific volume package options structure\n");
	return NULL;
    }
    memcpy(thread_opts, &VThread_defaults, sizeof(VThread_defaults));
    thread_opts->disallow_salvsync = 1;
    osi_Assert(pthread_setspecific(VThread_key, thread_opts) == 0);

    code = VVGCache_PkgInit();
    osi_Assert(code == 0);
#endif

    InitHandler();
    AcceptOn();

    for (;;) {
#if defined(HAVE_POLL) && defined(AFS_PTHREAD_ENV)
        int nfds;
        GetHandler(FSYNC_readfds, MAXHANDLERS, POLLIN|POLLPRI, &nfds);
        if (poll(FSYNC_readfds, nfds, -1) >=1)
	    CallHandler(FSYNC_readfds, nfds, POLLIN|POLLPRI);
#else
	int maxfd;
#ifdef AFS_PTHREAD_ENV
	struct timeval s_timeout;
#endif
	GetHandler(&FSYNC_readfds, &maxfd);
	/* Note: check for >= 1 below is essential since IOMGR_select
	 * doesn't have exactly same semantics as select.
	 */
#ifdef AFS_PTHREAD_ENV
	s_timeout.tv_sec = SYNC_SELECT_TIMEOUT;
	s_timeout.tv_usec = 0;
	if (select(maxfd + 1, &FSYNC_readfds, NULL, NULL, &s_timeout) >= 1)
#else /* AFS_PTHREAD_ENV */
	if (IOMGR_Select(maxfd + 1, &FSYNC_readfds, NULL, NULL, NULL) >= 1)
#endif /* AFS_PTHREAD_ENV */
	    CallHandler(&FSYNC_readfds);
#endif
    }
    return NULL; /* hush now, little gcc */
}

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * thread for salvaging volumes in the background.
 *
 * Since FSSYNC handlers cannot issue SALVSYNC requests in order to avoid
 * deadlock issues, this thread exists so code in the FSSYNC handler thread
 * can hand off volumes to be salvaged in the background.
 *
 * @param[in] args  unused
 *
 * @note DEMAND_ATTACH_FS only
 */
static void *
FSYNC_salvageThread(void * args)
{
    Volume *vp;
    struct fsync_salv_node *node;

    VOL_LOCK;

    for (;;) {
	while (queue_IsEmpty(&fsync_salv.head)) {
	    VOL_CV_WAIT(&fsync_salv.cv);
	}

	node = queue_First(&fsync_salv.head, fsync_salv_node);
	queue_Remove(node);

	vp = node->vp;
	if (node->update_salv_prio) {
	    if (VUpdateSalvagePriority_r(vp)) {
		ViceLog(0, ("FSYNC_salvageThread: unable to raise salvage priority "
		            "for volume %lu\n", afs_printable_uint32_lu(vp->hashid)));
	    }
	}

	free(node);
	node = NULL;

	VCancelReservation_r(vp);
    }

    VOL_UNLOCK;

    return NULL;
}

/**
 * salvage a volume in the background.
 *
 * Salvages cannot be scheduled directly from the main FSYNC thread, so
 * instead call this function to schedule a salvage asynchronously in the
 * FSYNC_salvageThread thread.
 *
 * @param[in] vp  volume to pointer to salvage
 *
 * @pre VOL_LOCK held
 *
 * @note DEMAND_ATTACH_FS only
 */
static void
FSYNC_backgroundSalvage(Volume *vp)
{
    struct fsync_salv_node *node;
    Error ec;

    VCreateReservation_r(vp);

    node = malloc(sizeof(struct fsync_salv_node));
    node->vp = vp;

    /* Save this value, to know if we should VUpdateSalvagePriority_r.
     * We need to save it here, snce VRequestSalvage_r will change it. */
    node->update_salv_prio = vp->salvage.requested;

    if (VRequestSalvage_r(&ec, vp, SALVSYNC_ERROR, 0)) {
	ViceLog(0, ("FSYNC_backgroundSalvage: unable to request salvage for volume %lu\n",
	            afs_printable_uint32_lu(vp->hashid)));
    }

    queue_Append(&fsync_salv.head, node);
    CV_BROADCAST(&fsync_salv.cv);
}
#endif /* AFS_DEMAND_ATTACH_FS */

static void
FSYNC_newconnection(osi_socket afd)
{
#ifdef USE_UNIX_SOCKETS
    struct sockaddr_un other;
#else  /* USE_UNIX_SOCKETS */
    struct sockaddr_in other;
#endif
    osi_socket fd;
    socklen_t junk;
    junk = sizeof(other);
    fd = accept(afd, (struct sockaddr *)&other, &junk);
    if (fd == OSI_NULLSOCKET) {
	Log("FSYNC_newconnection:  accept failed, errno==%d\n", errno);
	osi_Assert(1 == 2);
    } else if (!AddHandler(fd, FSYNC_com)) {
	AcceptOff();
	osi_Assert(AddHandler(fd, FSYNC_com));
    }
}

/* this function processes commands from an fssync file descriptor (fd) */
afs_int32 FS_cnt = 0;
static void
FSYNC_com(osi_socket fd)
{
    SYNC_command com;
    SYNC_response res;
    SYNC_PROTO_BUF_DECL(com_buf);
    SYNC_PROTO_BUF_DECL(res_buf);

    memset(&res.hdr, 0, sizeof(res.hdr));

    com.payload.buf = (void *)com_buf;
    com.payload.len = SYNC_PROTO_MAX_LEN;
    res.hdr.response_len = sizeof(res.hdr);
    res.payload.len = SYNC_PROTO_MAX_LEN;
    res.payload.buf = (void *)res_buf;

    FS_cnt++;
    if (SYNC_getCom(&fssync_server_state, fd, &com)) {
	Log("FSYNC_com:  read failed; dropping connection (cnt=%d)\n", FS_cnt);
	FSYNC_Drop(fd);
	return;
    }

    if (com.recv_len < sizeof(com.hdr)) {
	Log("FSSYNC_com:  invalid protocol message length (%u)\n", com.recv_len);
	res.hdr.response = SYNC_COM_ERROR;
	res.hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    if (com.hdr.proto_version != FSYNC_PROTO_VERSION) {
	Log("FSYNC_com:  invalid protocol version (%u)\n", com.hdr.proto_version);
	res.hdr.response = SYNC_COM_ERROR;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    if (com.hdr.command == SYNC_COM_CHANNEL_CLOSE) {
	res.hdr.response = SYNC_OK;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;

	/* don't respond, just drop; senders of SYNC_COM_CHANNEL_CLOSE
	 * never wait for a response. */
	goto done;
    }

    ViceLog(125, ("FSYNC_com: from fd %d got command %ld (%s) reason %ld (%s) "
                  "pt %ld (%s) pid %ld\n", (int)fd,
                  afs_printable_int32_ld(com.hdr.command),
                  FSYNC_com2string(com.hdr.command),
                  afs_printable_int32_ld(com.hdr.reason),
                  FSYNC_reason2string(com.hdr.reason),
                  afs_printable_int32_ld(com.hdr.programType),
                  VPTypeToString(com.hdr.programType),
                  afs_printable_int32_ld(com.hdr.pid)));

    res.hdr.com_seq = com.hdr.com_seq;

    VOL_LOCK;
    switch (com.hdr.command) {
    case FSYNC_VOL_ON:
    case FSYNC_VOL_ATTACH:
    case FSYNC_VOL_LEAVE_OFF:
    case FSYNC_VOL_OFF:
    case FSYNC_VOL_FORCE_ERROR:
    case FSYNC_VOL_LISTVOLUMES:
    case FSYNC_VOL_NEEDVOLUME:
    case FSYNC_VOL_MOVE:
    case FSYNC_VOL_BREAKCBKS:
    case FSYNC_VOL_DONE:
    case FSYNC_VOL_QUERY:
    case FSYNC_VOL_QUERY_HDR:
#ifdef AFS_DEMAND_ATTACH_FS
    case FSYNC_VOL_QUERY_VOP:
    case FSYNC_VG_QUERY:
    case FSYNC_VG_SCAN:
    case FSYNC_VG_SCAN_ALL:
#endif
	res.hdr.response = FSYNC_com_VolOp(fd, &com, &res);
	break;
    case FSYNC_VOL_STATS_GENERAL:
    case FSYNC_VOL_STATS_VICEP:
    case FSYNC_VOL_STATS_HASH:
    case FSYNC_VOL_STATS_HDR:
    case FSYNC_VOL_STATS_VLRU:
	res.hdr.response = FSYNC_com_StatsOp(fd, &com, &res);
	break;
    case FSYNC_VOL_QUERY_VNODE:
	res.hdr.response = FSYNC_com_VnQry(fd, &com, &res);
	break;
#ifdef AFS_DEMAND_ATTACH_FS
    case FSYNC_VG_ADD:
    case FSYNC_VG_DEL:
	res.hdr.response = FSYNC_com_VGUpdate(fd, &com, &res);
	break;
#endif
    default:
	res.hdr.response = SYNC_BAD_COMMAND;
	break;
    }
    VOL_UNLOCK;

    ViceLog(125, ("FSYNC_com: fd %d responding with code %ld (%s) reason %ld "
                  "(%s)\n", (int)fd,
                  afs_printable_int32_ld(res.hdr.response),
                  SYNC_res2string(res.hdr.response),
                  afs_printable_int32_ld(res.hdr.reason),
                  FSYNC_reason2string(res.hdr.reason)));

 respond:
    SYNC_putRes(&fssync_server_state, fd, &res);

 done:
    if (res.hdr.flags & SYNC_FLAG_CHANNEL_SHUTDOWN) {
	FSYNC_Drop(fd);
    }
}

static afs_int32
FSYNC_com_VolOp(osi_socket fd, SYNC_command * com, SYNC_response * res)
{
    int i;
    afs_int32 code = SYNC_OK;
    FSSYNC_VolOp_command vcom;

    if (com->recv_len != (sizeof(com->hdr) + sizeof(FSSYNC_VolOp_hdr))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res->hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	return SYNC_COM_ERROR;
    }

    vcom.hdr = &com->hdr;
    vcom.vop = (FSSYNC_VolOp_hdr *) com->payload.buf;
    vcom.com = com;

    vcom.volumes = OfflineVolumes[FindHandler(fd)];
    for (vcom.v = NULL, i = 0; i < MAXOFFLINEVOLUMES; i++) {
	if ((vcom.volumes[i].volumeID == vcom.vop->volume) &&
	    (strncmp(vcom.volumes[i].partName, vcom.vop->partName,
		     sizeof(vcom.volumes[i].partName)) == 0)) {
	    vcom.v = &vcom.volumes[i];
	    break;
	}
    }

    ViceLog(125, ("FSYNC_com_VolOp: fd %d got command for vol %lu part %.16s\n",
                  (int)fd, afs_printable_uint32_lu(vcom.vop->volume),
                  vcom.vop->partName));

    switch (com->hdr.command) {
    case FSYNC_VOL_ON:
    case FSYNC_VOL_ATTACH:
    case FSYNC_VOL_LEAVE_OFF:
	code = FSYNC_com_VolOn(&vcom, res);
	break;
    case FSYNC_VOL_OFF:
    case FSYNC_VOL_NEEDVOLUME:
	code = FSYNC_com_VolOff(&vcom, res);
	break;
    case FSYNC_VOL_LISTVOLUMES:
	code = SYNC_OK;
	break;
    case FSYNC_VOL_MOVE:
	code = FSYNC_com_VolMove(&vcom, res);
	break;
    case FSYNC_VOL_BREAKCBKS:
	code = FSYNC_com_VolBreakCBKs(&vcom, res);
	break;
    case FSYNC_VOL_DONE:
	code = FSYNC_com_VolDone(&vcom, res);
	break;
    case FSYNC_VOL_QUERY:
	code = FSYNC_com_VolQuery(&vcom, res);
	break;
    case FSYNC_VOL_QUERY_HDR:
	code = FSYNC_com_VolHdrQuery(&vcom, res);
	break;
#ifdef AFS_DEMAND_ATTACH_FS
    case FSYNC_VOL_FORCE_ERROR:
	code = FSYNC_com_VolError(&vcom, res);
	break;
    case FSYNC_VOL_QUERY_VOP:
	code = FSYNC_com_VolOpQuery(&vcom, res);
	break;
    case FSYNC_VG_QUERY:
	code = FSYNC_com_VGQuery(&vcom, res);
	break;
    case FSYNC_VG_SCAN:
	code = FSYNC_com_VGScan(&vcom, res);
	break;
    case FSYNC_VG_SCAN_ALL:
	code = FSYNC_com_VGScanAll(&vcom, res);
	break;
#endif /* AFS_DEMAND_ATTACH_FS */
    default:
	code = SYNC_BAD_COMMAND;
    }

    return code;
}

/**
 * service an FSYNC request to bring a volume online.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume transitioned online
 *   @retval SYNC_FAILED invalid command protocol message
 *   @retval SYNC_DENIED operation could not be completed
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_ON
 *       - FSYNC_VOL_ATTACH
 *       - FSYNC_VOL_LEAVE_OFF
 *
 * @note the supplementary reason code contains additional details.
 *       When SYNC_DENIED is returned, the specific reason is
 *       placed in the response packet reason field.
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolOn(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
#ifndef AFS_DEMAND_ATTACH_FS
    char tvolName[VMAXPATHLEN];
#endif
    Volume * vp;
    Error error;

    /* Verify the partition name is null terminated. */
    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	code = SYNC_FAILED;
	goto done;
    }

    /* so, we need to attach the volume */

#ifdef AFS_DEMAND_ATTACH_FS
    /* Verify the partition name is not empty. */
    if (*vcom->vop->partName == 0) {
	res->hdr.reason = FSYNC_BAD_PART;
	code = SYNC_FAILED;
	goto done;
    }

    /* check DAFS permissions */
    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
    if (vp &&
	FSYNC_partMatch(vcom, vp, 1) &&
	vp->pending_vol_op &&
	(vcom->hdr->programType != vp->pending_vol_op->com.programType)) {
	/* a different program has this volume checked out. deny. */
	Log("FSYNC_VolOn: WARNING: program type %u has attempted to manipulate "
	    "state for volume %u using command code %u while the volume is "
	    "checked out by program type %u for command code %u.\n",
	    vcom->hdr->programType,
	    vcom->vop->volume,
	    vcom->hdr->command,
	    vp->pending_vol_op->com.programType,
	    vp->pending_vol_op->com.command);
	code = SYNC_DENIED;
	res->hdr.reason = FSYNC_EXCLUSIVE;
	goto done;
    }
#endif

    if (vcom->v)
	vcom->v->volumeID = 0;


    if (vcom->hdr->command == FSYNC_VOL_LEAVE_OFF) {
	/* nothing much to do if we're leaving the volume offline */
#ifdef AFS_DEMAND_ATTACH_FS
	if (vp) {
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);
	}
	if (vp && V_attachState(vp) != VOL_STATE_DELETED) {
	    if (FSYNC_partMatch(vcom, vp, 1)) {
		if ((V_attachState(vp) == VOL_STATE_UNATTACHED) ||
		    (V_attachState(vp) == VOL_STATE_PREATTACHED)) {
		    VChangeState_r(vp, VOL_STATE_UNATTACHED);
		    VDeregisterVolOp_r(vp);
		} else {
		    code = SYNC_DENIED;
		    res->hdr.reason = FSYNC_BAD_STATE;
		}
	    } else {
		code = SYNC_DENIED;
		res->hdr.reason = FSYNC_WRONG_PART;
	    }
	} else {
	    code = SYNC_FAILED;
	    res->hdr.reason = FSYNC_UNKNOWN_VOLID;
	}

	if (vp) {
	    VCancelReservation_r(vp);
	    vp = NULL;
	}
#endif
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS

    if (vp &&
        FSYNC_partMatch(vcom, vp, 0) &&
        vp->pending_vol_op &&
        vp->pending_vol_op->vol_op_state == FSSYNC_VolOpRunningOnline &&
        V_attachState(vp) == VOL_STATE_ATTACHED) {

	/* noop; the volume stayed online for the volume operation and we were
	 * simply told that the vol op is done. The vp we already have is fine,
	 * so avoid confusing volume routines with trying to preattach an
	 * attached volume. */

    } else {
	/* first, check to see whether we have such a volume defined */
	vp = VPreAttachVolumeById_r(&error,
				    vcom->vop->partName,
				    vcom->vop->volume);
    }

    if (vp) {
	VCreateReservation_r(vp);
	VWaitExclusiveState_r(vp);
	VDeregisterVolOp_r(vp);
	VCancelReservation_r(vp);
	vp = NULL;
    }
#else /* !AFS_DEMAND_ATTACH_FS */
    tvolName[0] = OS_DIRSEPC;
    snprintf(&tvolName[1], sizeof(tvolName)-1, VFORMAT, afs_printable_uint32_lu(vcom->vop->volume));
    tvolName[sizeof(tvolName)-1] = '\0';

    vp = VAttachVolumeByName_r(&error, vcom->vop->partName, tvolName,
			       V_VOLUPD);
    if (vp)
	VPutVolume_r(vp);
#endif /* !AFS_DEMAND_ATTACH_FS */
    if (error) {
	code = SYNC_DENIED;
	res->hdr.reason = error;
    }

 done:
    return code;
}

/**
 * service an FSYNC request to take a volume offline.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume transitioned offline
 *   @retval SYNC_FAILED invalid command protocol message
 *   @retval SYNC_DENIED operation could not be completed
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_OFF
 *       - FSYNC_VOL_NEEDVOLUME
 *
 * @note the supplementary reason code contains additional details.
 *       When SYNC_DENIED is returned, the specific reason is
 *       placed in the response packet reason field.
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolOff(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    FSSYNC_VolOp_info info;
    afs_int32 code = SYNC_OK;
    int i;
    Volume * vp;
    Error error;
#ifdef AFS_DEMAND_ATTACH_FS
    Volume *nvp, *rvp = NULL;
#endif

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	code = SYNC_FAILED;
	goto done;
    }

    /* not already offline, we need to find a slot for newly offline volume */
    if (vcom->hdr->programType == debugUtility) {
	/* debug utilities do not have their operations tracked */
	vcom->v = NULL;
    } else {
	if (!vcom->v) {
	    for (i = 0; i < MAXOFFLINEVOLUMES; i++) {
		if (vcom->volumes[i].volumeID == 0) {
		    vcom->v = &vcom->volumes[i];
		    break;
		}
	    }
	}
	if (!vcom->v) {
	    goto deny;
	}
    }

    FSYNC_com_to_info(vcom, &info);

#ifdef AFS_DEMAND_ATTACH_FS
    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
#else
    vp = VGetVolume_r(&error, vcom->vop->volume);
#endif

    if (vp) {
	    if (!FSYNC_partMatch(vcom, vp, 1)) {
	    /* volume on desired partition is not online, so we
	     * should treat this as an offline volume.
	     */
#ifndef AFS_DEMAND_ATTACH_FS
	    VPutVolume_r(vp);
#endif
	    vp = NULL;
	    goto done;
	}
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (vp) {
	ProgramType type = (ProgramType) vcom->hdr->programType;

	/* do initial filtering of requests */

	/* enforce mutual exclusion for volume ops */
	if (vp->pending_vol_op) {
	    if (vp->pending_vol_op->com.programType != type) {
                if (vp->pending_vol_op->com.command == FSYNC_VOL_OFF &&
                    vp->pending_vol_op->com.reason == FSYNC_SALVAGE) {

                    Log("denying offline request for volume %lu; volume is salvaging\n",
		        afs_printable_uint32_lu(vp->hashid));

                    res->hdr.reason = FSYNC_SALVAGE;
                    goto deny;
                }
		Log("volume %u already checked out\n", vp->hashid);
		/* XXX debug */
		Log("vp->vop = { com = { ver=%u, prog=%d, com=%d, reason=%d, len=%u, flags=0x%x }, vop = { vol=%u, part='%s' } }\n",
		    vp->pending_vol_op->com.proto_version,
		    vp->pending_vol_op->com.programType,
		    vp->pending_vol_op->com.command,
		    vp->pending_vol_op->com.reason,
		    vp->pending_vol_op->com.command_len,
		    vp->pending_vol_op->com.flags,
		    vp->pending_vol_op->vop.volume,
		    vp->pending_vol_op->vop.partName );
		Log("vcom = { com = { ver=%u, prog=%d, com=%d, reason=%d, len=%u, flags=0x%x } , vop = { vol=%u, part='%s' } }\n",
		    vcom->hdr->proto_version,
		    vcom->hdr->programType,
		    vcom->hdr->command,
		    vcom->hdr->reason,
		    vcom->hdr->command_len,
		    vcom->hdr->flags,
		    vcom->vop->volume,
		    vcom->vop->partName);
		res->hdr.reason = FSYNC_EXCLUSIVE;
		goto deny;
	    } else {
		Log("warning: volume %u recursively checked out by programType id %d\n",
		    vp->hashid, vcom->hdr->programType);
	    }
	}

	/* wait for exclusive ops, so we have an accurate picture of the
	 * vol attach state */
	VCreateReservation_r(vp);
	VWaitExclusiveState_r(vp);
	rvp = vp;

	/* filter based upon requestor
	 *
	 * volume utilities / volserver are not allowed to check out
	 * volumes which are in an error state
	 *
	 * unknown utility programs will be denied on principal
	 */
	switch (type) {
	case salvageServer:
	case volumeSalvager:
	    /* it is possible for the salvageserver to checkout a
	     * volume for salvage before its scheduling request
	     * has been sent to the salvageserver */
	    if (vp->salvage.requested && !vp->salvage.scheduled) {
		vp->salvage.scheduled = 1;
	    }

	    /* If the volume is in VOL_STATE_SALVAGE_REQ, we need to wait
	     * for the vol to go offline before we can give it away. Also
	     * make sure we don't come out with vp in an excl state. */
	    while (V_attachState(vp) == VOL_STATE_SALVAGE_REQ ||
	           VIsExclusiveState(V_attachState(vp))) {

		VOL_CV_WAIT(&V_attachCV(vp));
	    }

	case debugUtility:
	    break;

	case volumeUtility:
	case volumeServer:
            if (VIsSalvaging(vp)) {
                Log("denying offline request for volume %lu; volume is in salvaging state\n",
		    afs_printable_uint32_lu(vp->hashid));
                res->hdr.reason = FSYNC_SALVAGE;

		/* the volume hasn't been checked out yet by the salvager,
		 * but we think the volume is salvaging; schedule a
		 * a salvage to update the salvage priority */
		FSYNC_backgroundSalvage(vp);

		goto deny;
            }
	    if (VIsErrorState(V_attachState(vp))) {
		goto deny;
	    }
	    break;

	default:
	    Log("bad program type passed to FSSYNC\n");
	    goto deny;
	}

	/* short circuit for offline volume states
	 * so we can avoid I/O penalty of attachment */
	switch (V_attachState(vp)) {
	case VOL_STATE_UNATTACHED:
	case VOL_STATE_PREATTACHED:
	case VOL_STATE_SALVAGING:
	case VOL_STATE_ERROR:
	    /* register the volume operation metadata with the volume
	     *
	     * if the volume is currently pre-attached, attach2()
	     * will evaluate the vol op metadata to determine whether
	     * attaching the volume would be safe */
	    VRegisterVolOp_r(vp, &info);
	    vp->pending_vol_op->vol_op_state = FSSYNC_VolOpRunningUnknown;
	    /* fall through */

	case VOL_STATE_DELETED:
	    goto done;
	default:
	    break;
	}

	/* convert to heavyweight ref */
	nvp = VGetVolumeByVp_r(&error, vp);
	if (!nvp) {
            /*
             * It's possible for VGetVolumeByVp_r to have dropped and
             * re-acquired VOL_LOCK, so volume state may have changed
             * back to one of the states we tested for above. Since
             * GetVolume can return NULL in some of those states, just
             * test for the states again here.
             */
            switch (V_attachState(vp)) {
            case VOL_STATE_UNATTACHED:
            case VOL_STATE_PREATTACHED:
            case VOL_STATE_SALVAGING:
            case VOL_STATE_ERROR:
                /* register the volume operation metadata with the volume
                 *
                 * if the volume is currently pre-attached, attach2()
                 * will evaluate the vol op metadata to determine whether
                 * attaching the volume would be safe */
                VRegisterVolOp_r(vp, &info);
                vp->pending_vol_op->vol_op_state = FSSYNC_VolOpRunningUnknown;
		/* fall through */

            case VOL_STATE_DELETED:
                goto done;
            default:
                break;
            }

	    Log("FSYNC_com_VolOff: failed to get heavyweight reference to volume %u (state=%u, flags=0x%x)\n",
		vcom->vop->volume, V_attachState(vp), V_attachFlags(vp));
	    res->hdr.reason = FSYNC_VOL_PKG_ERROR;
	    goto deny;
	} else if (nvp != vp) {
	    /* i don't think this should ever happen, but just in case... */
	    Log("FSYNC_com_VolOff: warning: potentially dangerous race detected\n");
	    vp = nvp;
	}

	/* kill off lightweight ref to ensure we can't deadlock against ourselves later... */
	VCancelReservation_r(rvp);
	rvp = NULL;

	/* register the volume operation metadata with the volume */
	VRegisterVolOp_r(vp, &info);

    }
#endif /* AFS_DEMAND_ATTACH_FS */

    if (vp) {
	if (VVolOpLeaveOnline_r(vp, &info)) {
	    VUpdateVolume_r(&error, vp, VOL_UPDATE_WAIT);	/* At least get volume stats right */
	    if (LogLevel) {
		Log("FSYNC: Volume %u (%s) was left on line for an external %s request\n",
		    V_id(vp), V_name(vp),
		    vcom->hdr->reason == V_CLONE ? "clone" :
		    vcom->hdr->reason == V_READONLY ? "readonly" :
		    vcom->hdr->reason == V_DUMP ? "dump" :
		    vcom->hdr->reason == FSYNC_SALVAGE ? "salvage" :
		    "UNKNOWN");
	    }
#ifdef AFS_DEMAND_ATTACH_FS
            vp->pending_vol_op->vol_op_state = FSSYNC_VolOpRunningOnline;
#endif
	    VPutVolume_r(vp);
	} else {
	    if (VVolOpSetVBusy_r(vp, &info)) {
		vp->specialStatus = VBUSY;
	    }

	    /* remember what volume we got, so we can keep track of how
	     * many volumes the volserver or whatever is using.  Note that
	     * vp is valid since leaveonline is only set when vp is valid.
	     */
	    if (vcom->v) {
		vcom->v->volumeID = vcom->vop->volume;
		strlcpy(vcom->v->partName, vp->partition->name, sizeof(vcom->v->partName));
	    }

#ifdef AFS_DEMAND_ATTACH_FS
	    VCreateReservation_r(vp);
            VOfflineForVolOp_r(&error, vp, "A volume utility is running.");
            if (error==0) {
                osi_Assert(vp->nUsers==0);
                vp->pending_vol_op->vol_op_state = FSSYNC_VolOpRunningOffline;
            }
            else {
		VWaitExclusiveState_r(vp);
		VDeregisterVolOp_r(vp);
                code = SYNC_DENIED;
            }
	    VCancelReservation_r(vp);
#else
	    VOffline_r(vp, "A volume utility is running.");
#endif
	    vp = NULL;
	}
    }
    goto done;

 deny:
    code = SYNC_DENIED;

 done:
#ifdef AFS_DEMAND_ATTACH_FS
    if (rvp) {
        VCancelReservation_r(rvp);
    }
#endif
    return code;
}

/**
 * service an FSYNC request to mark a volume as moved.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume marked as moved to a remote server
 *   @retval SYNC_FAILED invalid command protocol message
 *   @retval SYNC_DENIED current volume state does not permit this operation
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this operation also breaks all callbacks for the given volume
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_MOVE
 *
 * @note the supplementary reason code contains additional details.  For
 *       instance, SYNC_OK is still returned when the partition specified
 *       does not match the one registered in the volume object -- reason
 *       will be FSYNC_WRONG_PART in this case.
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolMove(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_DENIED;
    Error error;
    Volume * vp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	code = SYNC_FAILED;
	goto done;
    }

    /* Yuch:  the "reason" for the move is the site it got moved to... */
    /* still set specialStatus so we stop sending back VBUSY.
     * also should still break callbacks.  Note that I don't know
     * how to tell if we should break all or not, so we just do it
     * since it doesn't matter much if we do an extra break
     * volume callbacks on a volume move within the same server */
#ifdef AFS_DEMAND_ATTACH_FS
    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
#else
    vp = VGetVolume_r(&error, vcom->vop->volume);
#endif
    if (vp) {
	if (FSYNC_partMatch(vcom, vp, 1)) {
#ifdef AFS_DEMAND_ATTACH_FS
	    if ((V_attachState(vp) == VOL_STATE_UNATTACHED) ||
		(V_attachState(vp) == VOL_STATE_PREATTACHED)) {
#endif
		code = SYNC_OK;
		vp->specialStatus = VMOVED;
#ifdef AFS_DEMAND_ATTACH_FS
	    } else {
		res->hdr.reason = FSYNC_BAD_STATE;
	    }
#endif
	} else {
	    res->hdr.reason = FSYNC_WRONG_PART;
	}
#ifndef AFS_DEMAND_ATTACH_FS
	VPutVolume_r(vp);
#endif /* !AFS_DEMAND_ATTACH_FS */
    } else {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
    }

    if ((code == SYNC_OK) && (V_BreakVolumeCallbacks != NULL)) {
	Log("fssync: volume %u moved to %x; breaking all call backs\n",
	    vcom->vop->volume, vcom->hdr->reason);
	VOL_UNLOCK;
	(*V_BreakVolumeCallbacks) (vcom->vop->volume);
	VOL_LOCK;
    }


 done:
    return code;
}

/**
 * service an FSYNC request to mark a volume as destroyed.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume marked as destroyed
 *   @retval SYNC_FAILED invalid command protocol message
 *   @retval SYNC_DENIED current volume state does not permit this operation
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_DONE
 *
 * @note the supplementary reason code contains additional details.  For
 *       instance, SYNC_OK is still returned when the partition specified
 *       does not match the one registered in the volume object -- reason
 *       will be FSYNC_WRONG_PART in this case.
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolDone(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    Error error;
    Volume * vp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    /* don't try to put online, this call is made only after deleting
     * a volume, in which case we want to remove the vol # from the
     * OfflineVolumes array only */
    if (vcom->v)
	vcom->v->volumeID = 0;

    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
    if (vp) {
	if (FSYNC_partMatch(vcom, vp, 1)) {
#ifdef AFS_DEMAND_ATTACH_FS
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);

	    if ((V_attachState(vp) == VOL_STATE_UNATTACHED) ||
		(V_attachState(vp) == VOL_STATE_PREATTACHED) ||
		VIsErrorState(V_attachState(vp))) {

		/* Change state to DELETED, not UNATTACHED, so clients get
		 * a VNOVOL error when they try to access from now on. */

		VChangeState_r(vp, VOL_STATE_DELETED);
		VDeregisterVolOp_r(vp);

		/* Volume is gone; clear out old salvage stats */
		memset(&vp->salvage, 0, sizeof(vp->salvage));

		/* Someday we should free the vp, too, after about 2 hours,
		 * possibly by putting the vp back on the VLRU. */

		code = SYNC_OK;
	    } else if (V_attachState(vp) == VOL_STATE_DELETED) {
		VDeregisterVolOp_r(vp);
		res->hdr.reason = FSYNC_UNKNOWN_VOLID;

	    } else {
		code = SYNC_DENIED;
		res->hdr.reason = FSYNC_BAD_STATE;
	    }

	    VCancelReservation_r(vp);
	    vp = NULL;
#else /* AFS_DEMAND_ATTACH_FS */
	    if (!vp->specialStatus) {
		vp->specialStatus = VNOVOL;
	    }
	    code = SYNC_OK;
#endif /* !AFS_DEMAND_ATTACH_FS */
	} else {
	    code = SYNC_OK; /* XXX is this really a good idea? */
	    res->hdr.reason = FSYNC_WRONG_PART;
	}
    } else {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
    }

 done:
    return code;
}

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * service an FSYNC request to transition a volume to the hard error state.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume transitioned to hard error state
 *   @retval SYNC_FAILED invalid command protocol message
 *   @retval SYNC_DENIED (see note)
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_FORCE_ERROR
 *
 * @note SYNC_DENIED is returned in the following cases:
 *        - no partition name is specified (reason field set to
 *          FSYNC_WRONG_PART).
 *        - volume id not known to fileserver (reason field set
 *          to FSYNC_UNKNOWN_VOLID).
 *
 * @note demand attach fileserver only
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolError(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    Error error;
    Volume * vp;
    afs_int32 code = SYNC_FAILED;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);

    if (!vp && vcom->hdr->reason == FSYNC_SALVAGE) {
	/* The requested volume doesn't seem to exist. However, it is possible
	 * that this is triggered by trying to create or clone a volume that
	 * was prevented from succeeding by a half-created volume in the way.
	 * (e.g. we tried to create volume X, but volume X exists except that
	 * its .vol header was deleted for some reason) So, still try to
	 * a salvage for that volume ID. */

	Log("FSYNC_com_VolError: attempting to schedule salvage for unknown "
	    "volume %lu part %s\n", afs_printable_uint32_lu(vcom->vop->volume),
	    vcom->vop->partName);
	vp = VPreAttachVolumeById_r(&error, vcom->vop->partName,
	                            vcom->vop->volume);
    }

    if (vp) {
	if (FSYNC_partMatch(vcom, vp, 0)) {
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);
            VDeregisterVolOp_r(vp);

            if (vcom->hdr->reason == FSYNC_SALVAGE) {
		FSYNC_backgroundSalvage(vp);
            } else {
		/* null out salvsync control state, as it's no longer relevant */
		memset(&vp->salvage, 0, sizeof(vp->salvage));
	        VChangeState_r(vp, VOL_STATE_ERROR);
            }

	    VCancelReservation_r(vp);
	    vp = NULL;

	    code = SYNC_OK;
	} else {
	    res->hdr.reason = FSYNC_WRONG_PART;
	}
    } else {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
    }

 done:
    return code;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/**
 * service an FSYNC request to break all callbacks for this volume.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK callback breaks scheduled for volume
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_BREAKCBKS
 *
 * @note demand attach fileserver only
 *
 * @todo should do partition matching
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolBreakCBKs(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    /* if the volume is being restored, break all callbacks on it */
    if (V_BreakVolumeCallbacks) {
	Log("fssync: breaking all call backs for volume %u\n",
	    vcom->vop->volume);
	VOL_UNLOCK;
	(*V_BreakVolumeCallbacks) (vcom->vop->volume);
	VOL_LOCK;
    }
    return SYNC_OK;
}

/**
 * service an FSYNC request to return the Volume object.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK      volume object returned to caller
 *   @retval SYNC_FAILED  bad command packet, or failed to locate volume object
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_QUERY
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolQuery(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    Error error;
    Volume * vp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
#else /* !AFS_DEMAND_ATTACH_FS */
    vp = VGetVolume_r(&error, vcom->vop->volume);
#endif /* !AFS_DEMAND_ATTACH_FS */

    if (vp) {
	if (FSYNC_partMatch(vcom, vp, 1)) {
	    if (res->payload.len >= sizeof(Volume)) {
		memcpy(res->payload.buf, vp, sizeof(Volume));
		res->hdr.response_len += sizeof(Volume);
		code = SYNC_OK;
	    } else {
		res->hdr.reason = SYNC_REASON_PAYLOAD_TOO_BIG;
	    }
	} else {
	    res->hdr.reason = FSYNC_WRONG_PART;
	}
#ifndef AFS_DEMAND_ATTACH_FS
	VPutVolume_r(vp);
#endif
    } else {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
    }

 done:
    return code;
}

/**
 * service an FSYNC request to return the Volume header.
 *
 * @param[in]   vcom  pointer command object
 * @param[out]  res   object in which to store response packet
 *
 * @return operation status
 *   @retval SYNC_OK volume header returned to caller
 *   @retval SYNC_FAILED  bad command packet, or failed to locate volume header
 *
 * @note this is an FSYNC RPC server stub
 *
 * @note this procedure handles the following FSSYNC command codes:
 *       - FSYNC_VOL_QUERY_HDR
 *
 * @internal
 */
static afs_int32
FSYNC_com_VolHdrQuery(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    Error error;
    Volume * vp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }
    if (res->payload.len < sizeof(VolumeDiskData)) {
	res->hdr.reason = SYNC_REASON_PAYLOAD_TOO_BIG;
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);
#else /* !AFS_DEMAND_ATTACH_FS */
    vp = VGetVolume_r(&error, vcom->vop->volume);
#endif

    if (vp) {
	if (FSYNC_partMatch(vcom, vp, 1)) {
#ifdef AFS_DEMAND_ATTACH_FS
	    if ((vp->header == NULL) ||
		!(V_attachFlags(vp) & VOL_HDR_ATTACHED) ||
		!(V_attachFlags(vp) & VOL_HDR_LOADED)) {
		res->hdr.reason = FSYNC_HDR_NOT_ATTACHED;
		goto cleanup;
	    }
#else /* !AFS_DEMAND_ATTACH_FS */
	    if (!vp || !vp->header) {
		res->hdr.reason = FSYNC_HDR_NOT_ATTACHED;
		goto cleanup;
	    }
#endif /* !AFS_DEMAND_ATTACH_FS */
	} else {
	    res->hdr.reason = FSYNC_WRONG_PART;
	    goto cleanup;
	}
    } else {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
	goto done;
    }

    memcpy(res->payload.buf, &V_disk(vp), sizeof(VolumeDiskData));
    res->hdr.response_len += sizeof(VolumeDiskData);
    code = SYNC_OK;

 cleanup:
#ifndef AFS_DEMAND_ATTACH_FS
    VPutVolume_r(vp);
#endif

 done:
    return code;
}

#ifdef AFS_DEMAND_ATTACH_FS
static afs_int32
FSYNC_com_VolOpQuery(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    Error error;
    Volume * vp;

    vp = VLookupVolume_r(&error, vcom->vop->volume, NULL);

    if (vp) {
	VCreateReservation_r(vp);
	VWaitExclusiveState_r(vp);
    }

    if (vp && vp->pending_vol_op) {
	if (!FSYNC_partMatch(vcom, vp, 1)) {
	    res->hdr.reason = FSYNC_WRONG_PART;
	    code = SYNC_FAILED;
	} else {
	    osi_Assert(sizeof(FSSYNC_VolOp_info) <= res->payload.len);
	    memcpy(res->payload.buf, vp->pending_vol_op, sizeof(FSSYNC_VolOp_info));
	    res->hdr.response_len += sizeof(FSSYNC_VolOp_info);
	}
    } else {
	if (!vp || V_attachState(vp) == VOL_STATE_DELETED) {
	    res->hdr.reason = FSYNC_UNKNOWN_VOLID;
	} else if (!FSYNC_partMatch(vcom, vp, 1)) {
	    res->hdr.reason = FSYNC_WRONG_PART;
	} else {
	    res->hdr.reason = FSYNC_NO_PENDING_VOL_OP;
	}
	code = SYNC_FAILED;
    }

    if (vp) {
	VCancelReservation_r(vp);
    }
    return code;
}

static afs_int32
FSYNC_com_VGQuery(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    int rc;
    struct DiskPartition64 * dp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    dp = VGetPartition_r(vcom->vop->partName, 0);
    if (dp == NULL) {
	res->hdr.reason = FSYNC_BAD_PART;
	goto done;
    }

    osi_Assert(sizeof(FSSYNC_VGQry_response_t) <= res->payload.len);

    rc = VVGCache_query_r(dp, vcom->vop->volume, res->payload.buf);
    switch (rc) {
    case 0:
	res->hdr.response_len += sizeof(FSSYNC_VGQry_response_t);
	code = SYNC_OK;
	break;
    case EAGAIN:
	res->hdr.reason = FSYNC_PART_SCANNING;
	break;
    case ENOENT:
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
	break;
    default:
	break;
    }

 done:
    return code;
}

static afs_int32
FSYNC_com_VGUpdate(osi_socket fd, SYNC_command * com, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    struct DiskPartition64 * dp;
    FSSYNC_VGUpdate_command_t * vgucom;
    int rc;

    if (com->recv_len != (sizeof(com->hdr) + sizeof(*vgucom))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res->hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	code = SYNC_COM_ERROR;
	goto done;
    }

    vgucom = com->payload.buf;

    ViceLog(125, ("FSYNC_com_VGUpdate: fd %d got command for parent %lu child "
                  "%lu partName %.16s\n", (int)fd,
                  afs_printable_uint32_lu(vgucom->parent),
                  afs_printable_uint32_lu(vgucom->child),
                  vgucom->partName));

    if (SYNC_verifyProtocolString(vgucom->partName, sizeof(vgucom->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    dp = VGetPartition_r(vgucom->partName, 0);
    if (dp == NULL) {
	res->hdr.reason = FSYNC_BAD_PART;
	goto done;
    }

    switch(com->hdr.command) {
    case FSYNC_VG_ADD:
	rc = VVGCache_entry_add_r(dp, vgucom->parent, vgucom->child, NULL);
	break;

    case FSYNC_VG_DEL:
	rc = VVGCache_entry_del_r(dp, vgucom->parent, vgucom->child);
	break;

    default:
	Log("FSYNC_com_VGUpdate called improperly\n");
	rc = -1;
	break;
    }

    /* EINVAL means the partition VGC doesn't exist at all; not really
     * an error */
    if (rc == 0 || rc == EINVAL) {
	code = SYNC_OK;
    }

    if (rc == ENOENT) {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
    } else {
	res->hdr.reason = FSYNC_WHATEVER;
    }

 done:
    return code;
}

static afs_int32
FSYNC_com_VGScan(FSSYNC_VolOp_command * vcom, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;
    struct DiskPartition64 * dp;

    if (SYNC_verifyProtocolString(vcom->vop->partName, sizeof(vcom->vop->partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    dp = VGetPartition_r(vcom->vop->partName, 0);
    if (dp == NULL) {
	res->hdr.reason = FSYNC_BAD_PART;
	goto done;
    }

    if (VVGCache_scanStart_r(dp) == 0) {
	code = SYNC_OK;
    }

 done:
    return code;
}

static afs_int32
FSYNC_com_VGScanAll(FSSYNC_VolOp_command * com, SYNC_response * res)
{
    afs_int32 code = SYNC_FAILED;

    if (VVGCache_scanStart_r(NULL) == 0) {
	code = SYNC_OK;
    }

    return code;
}
#endif /* AFS_DEMAND_ATTACH_FS */

static afs_int32
FSYNC_com_VnQry(osi_socket fd, SYNC_command * com, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    FSSYNC_VnQry_hdr * qry = com->payload.buf;
    Volume * vp;
    Vnode * vnp;
    Error error;

    if (com->recv_len != (sizeof(com->hdr) + sizeof(FSSYNC_VnQry_hdr))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res->hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	return SYNC_COM_ERROR;
    }

    ViceLog(125, ("FSYNC_com_VnQry: fd %d got command for vol %lu vnode %lu "
                  "uniq %lu spare %lu partName %.16s\n", (int)fd,
                  afs_printable_uint32_lu(qry->volume),
                  afs_printable_uint32_lu(qry->vnode),
                  afs_printable_uint32_lu(qry->unique),
                  afs_printable_uint32_lu(qry->spare),
                  qry->partName));

#ifdef AFS_DEMAND_ATTACH_FS
    vp = VLookupVolume_r(&error, qry->volume, NULL);
#else /* !AFS_DEMAND_ATTACH_FS */
    vp = VGetVolume_r(&error, qry->volume);
#endif /* !AFS_DEMAND_ATTACH_FS */

    if (!vp) {
	res->hdr.reason = FSYNC_UNKNOWN_VOLID;
	code = SYNC_FAILED;
	goto done;
    }

    vnp = VLookupVnode(vp, qry->vnode);
    if (!vnp) {
	res->hdr.reason = FSYNC_UNKNOWN_VNID;
	code = SYNC_FAILED;
	goto cleanup;
    }

    if (Vn_class(vnp)->residentSize > res->payload.len) {
	res->hdr.reason = SYNC_REASON_ENCODING_ERROR;
	code = SYNC_FAILED;
	goto cleanup;
    }

    memcpy(res->payload.buf, vnp, Vn_class(vnp)->residentSize);
    res->hdr.response_len += Vn_class(vnp)->residentSize;

 cleanup:
#ifndef AFS_DEMAND_ATTACH_FS
    VPutVolume_r(vp);
#endif

 done:
    return code;
}

static afs_int32
FSYNC_com_StatsOp(osi_socket fd, SYNC_command * com, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    FSSYNC_StatsOp_command scom;

    if (com->recv_len != (sizeof(com->hdr) + sizeof(FSSYNC_StatsOp_hdr))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res->hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	return SYNC_COM_ERROR;
    }

    scom.hdr = &com->hdr;
    scom.sop = (FSSYNC_StatsOp_hdr *) com->payload.buf;
    scom.com = com;

    ViceLog(125, ("FSYNC_com_StatsOp: fd %d got command for stats: "
                  "{vlru_generation = %lu, hash_bucket = %lu, partName = "
                  "%.16s}\n", (int)fd,
                  afs_printable_uint32_lu(scom.sop->args.vlru_generation),
                  afs_printable_uint32_lu(scom.sop->args.hash_bucket),
                  scom.sop->args.partName));

    switch (com->hdr.command) {
    case FSYNC_VOL_STATS_GENERAL:
	code = FSYNC_com_StatsOpGeneral(&scom, res);
	break;
#ifdef AFS_DEMAND_ATTACH_FS
	/* statistics for the following subsystems are only tracked
	 * for demand attach fileservers */
    case FSYNC_VOL_STATS_VICEP:
	code = FSYNC_com_StatsOpViceP(&scom, res);
	break;
    case FSYNC_VOL_STATS_HASH:
	code = FSYNC_com_StatsOpHash(&scom, res);
	break;
    case FSYNC_VOL_STATS_HDR:
	code = FSYNC_com_StatsOpHdr(&scom, res);
	break;
    case FSYNC_VOL_STATS_VLRU:
	code = FSYNC_com_StatsOpVLRU(&scom, res);
	break;
#endif /* AFS_DEMAND_ATTACH_FS */
    default:
	code = SYNC_BAD_COMMAND;
    }

    return code;
}

static afs_int32
FSYNC_com_StatsOpGeneral(FSSYNC_StatsOp_command * scom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;

    memcpy(res->payload.buf, &VStats, sizeof(VStats));
    res->hdr.response_len += sizeof(VStats);

    return code;
}

#ifdef AFS_DEMAND_ATTACH_FS
static afs_int32
FSYNC_com_StatsOpViceP(FSSYNC_StatsOp_command * scom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct DiskPartition64 * dp;
    struct DiskPartitionStats64 * stats;

    if (SYNC_verifyProtocolString(scom->sop->args.partName, sizeof(scom->sop->args.partName))) {
	res->hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	code = SYNC_FAILED;
	goto done;
    }

    dp = VGetPartition_r(scom->sop->args.partName, 0);
    if (!dp) {
	code = SYNC_FAILED;
    } else {
	stats = (struct DiskPartitionStats64 *) res->payload.buf;
	stats->free = dp->free;
	stats->totalUsable = dp->totalUsable;
	stats->minFree = dp->minFree;
	stats->f_files = dp->f_files;
	stats->vol_list_len = dp->vol_list.len;

	res->hdr.response_len += sizeof(struct DiskPartitionStats64);
    }

 done:
    return code;
}

static afs_int32
FSYNC_com_StatsOpHash(FSSYNC_StatsOp_command * scom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct VolumeHashChainStats * stats;
    struct VolumeHashChainHead * head;

    if (scom->sop->args.hash_bucket >= VolumeHashTable.Size) {
	return SYNC_FAILED;
    }

    head = &VolumeHashTable.Table[scom->sop->args.hash_bucket];
    stats = (struct VolumeHashChainStats *) res->payload.buf;
    stats->table_size = VolumeHashTable.Size;
    stats->chain_len = head->len;
    stats->chain_cacheCheck = head->cacheCheck;
    stats->chain_busy = head->busy;
    AssignInt64(head->looks, &stats->chain_looks);
    AssignInt64(head->gets, &stats->chain_gets);
    AssignInt64(head->reorders, &stats->chain_reorders);

    res->hdr.response_len += sizeof(struct VolumeHashChainStats);

    return code;
}

static afs_int32
FSYNC_com_StatsOpHdr(FSSYNC_StatsOp_command * scom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;

    memcpy(res->payload.buf, &volume_hdr_LRU.stats, sizeof(volume_hdr_LRU.stats));
    res->hdr.response_len += sizeof(volume_hdr_LRU.stats);

    return code;
}

static afs_int32
FSYNC_com_StatsOpVLRU(FSSYNC_StatsOp_command * scom, SYNC_response * res)
{
    afs_int32 code = SYNC_OK;

    code = SYNC_BAD_COMMAND;

    return code;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/**
 * populate an FSSYNC_VolOp_info object from a command packet object.
 *
 * @param[in]   vcom  pointer to command packet
 * @param[out]  info  pointer to info object which will be populated
 *
 * @note FSSYNC_VolOp_info objects are attached to Volume objects when
 *       a volume operation is commenced.
 *
 * @internal
 */
static void
FSYNC_com_to_info(FSSYNC_VolOp_command * vcom, FSSYNC_VolOp_info * info)
{
    memcpy(&info->com, vcom->hdr, sizeof(SYNC_command_hdr));
    memcpy(&info->vop, vcom->vop, sizeof(FSSYNC_VolOp_hdr));
    info->vol_op_state = FSSYNC_VolOpPending;
}

/**
 * check whether command packet partition name matches volume
 * object's partition name.
 *
 * @param[in] vcom        pointer to command packet
 * @param[in] vp          pointer to volume object
 * @param[in] match_anon  anon matching control flag (see note below)
 *
 * @return whether partitions match
 *   @retval 0  partitions do NOT match
 *   @retval 1  partitions match
 *
 * @note if match_anon is non-zero, then this function will return a
 *       positive match for a zero-length partition string in the
 *       command packet.
 *
 * @internal
 */
static int
FSYNC_partMatch(FSSYNC_VolOp_command * vcom, Volume * vp, int match_anon)
{
    return ((match_anon && vcom->vop->partName[0] == 0) ||
	    (strncmp(vcom->vop->partName, V_partition(vp)->name,
		     sizeof(vcom->vop->partName)) == 0));
}


static void
FSYNC_Drop(osi_socket fd)
{
    struct offlineInfo *p;
    int i;
    Error error;
#ifndef AFS_DEMAND_ATTACH_FS
    char tvolName[VMAXPATHLEN];
#endif

    VOL_LOCK;
    p = OfflineVolumes[FindHandler(fd)];
    for (i = 0; i < MAXOFFLINEVOLUMES; i++) {
	if (p[i].volumeID) {
	    Volume *vp;

#ifdef AFS_DEMAND_ATTACH_FS
	    vp = VPreAttachVolumeById_r(&error, p[i].partName, p[i].volumeID);
	    if (vp) {
		VCreateReservation_r(vp);
		VWaitExclusiveState_r(vp);
		VDeregisterVolOp_r(vp);
		VCancelReservation_r(vp);
	    }
#else
	    tvolName[0] = OS_DIRSEPC;
	    sprintf(&tvolName[1], VFORMAT, afs_printable_uint32_lu(p[i].volumeID));
	    vp = VAttachVolumeByName_r(&error, p[i].partName, tvolName,
				       V_VOLUPD);
	    if (vp)
		VPutVolume_r(vp);
#endif /* !AFS_DEMAND_ATTACH_FS */
	    p[i].volumeID = 0;
	}
    }
    VOL_UNLOCK;
    RemoveHandler(fd);
#ifdef AFS_NT40_ENV
    closesocket(fd);
#else
    close(fd);
#endif
    AcceptOn();
}

static int AcceptHandler = -1;	/* handler id for accept, if turned on */

static void
AcceptOn(void)
{
    if (AcceptHandler == -1) {
	osi_Assert(AddHandler(fssync_server_state.fd, FSYNC_newconnection));
	AcceptHandler = FindHandler(fssync_server_state.fd);
    }
}

static void
AcceptOff(void)
{
    if (AcceptHandler != -1) {
	osi_Assert(RemoveHandler(fssync_server_state.fd));
	AcceptHandler = -1;
    }
}

/* The multiple FD handling code. */

static osi_socket HandlerFD[MAXHANDLERS];
static void (*HandlerProc[MAXHANDLERS]) (osi_socket);

static void
InitHandler(void)
{
    int i;
    ObtainWriteLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++) {
	HandlerFD[i] = OSI_NULLSOCKET;
	HandlerProc[i] = 0;
    }
    ReleaseWriteLock(&FSYNC_handler_lock);
}

#if defined(HAVE_POLL) && defined(AFS_PTHREAD_ENV)
static void
CallHandler(struct pollfd *fds, int nfds, int mask)
{
    int i;
    int handler;
    ObtainReadLock(&FSYNC_handler_lock);
    for (i = 0; i < nfds; i++) {
        if (fds[i].revents & mask) {
	    handler = FindHandler_r(fds[i].fd);
            ReleaseReadLock(&FSYNC_handler_lock);
            (*HandlerProc[handler]) (fds[i].fd);
	    ObtainReadLock(&FSYNC_handler_lock);
        }
    }
    ReleaseReadLock(&FSYNC_handler_lock);
}
#else
static void
CallHandler(fd_set * fdsetp)
{
    int i;
    ObtainReadLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++) {
	if (HandlerFD[i] >= 0 && FD_ISSET(HandlerFD[i], fdsetp)) {
	    ReleaseReadLock(&FSYNC_handler_lock);
	    (*HandlerProc[i]) (HandlerFD[i]);
	    ObtainReadLock(&FSYNC_handler_lock);
	}
    }
    ReleaseReadLock(&FSYNC_handler_lock);
}
#endif

static int
AddHandler(osi_socket afd, void (*aproc) (osi_socket))
{
    int i;
    ObtainWriteLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == OSI_NULLSOCKET)
	    break;
    if (i >= MAXHANDLERS) {
	ReleaseWriteLock(&FSYNC_handler_lock);
	return 0;
    }
    HandlerFD[i] = afd;
    HandlerProc[i] = aproc;
    ReleaseWriteLock(&FSYNC_handler_lock);
    return 1;
}

static int
FindHandler(osi_socket afd)
{
    int i;
    ObtainReadLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == afd) {
	    ReleaseReadLock(&FSYNC_handler_lock);
	    return i;
	}
    ReleaseReadLock(&FSYNC_handler_lock);	/* just in case */
    osi_Assert(1 == 2);
    return -1;			/* satisfy compiler */
}

static int
FindHandler_r(osi_socket afd)
{
    int i;
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == afd) {
	    return i;
	}
    osi_Assert(1 == 2);
    return -1;			/* satisfy compiler */
}

static int
RemoveHandler(osi_socket afd)
{
    ObtainWriteLock(&FSYNC_handler_lock);
    HandlerFD[FindHandler_r(afd)] = OSI_NULLSOCKET;
    ReleaseWriteLock(&FSYNC_handler_lock);
    return 1;
}

#if defined(HAVE_POLL) && defined(AFS_PTHREAD_ENV)
static void
GetHandler(struct pollfd *fds, int maxfds, int events, int *nfds)
{
    int i;
    int fdi = 0;
    ObtainReadLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] != OSI_NULLSOCKET) {
	    osi_Assert(fdi<maxfds);
	    fds[fdi].fd = HandlerFD[i];
	    fds[fdi].events = events;
	    fds[fdi].revents = 0;
	    fdi++;
	}
    *nfds = fdi;
    ReleaseReadLock(&FSYNC_handler_lock);
}
#else
static void
GetHandler(fd_set * fdsetp, int *maxfdp)
{
    int i;
    int maxfd = -1;
    FD_ZERO(fdsetp);
    ObtainReadLock(&FSYNC_handler_lock);	/* just in case */
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] != OSI_NULLSOCKET) {
	    FD_SET(HandlerFD[i], fdsetp);
#ifndef AFS_NT40_ENV
            /* On Windows the nfds parameter to select() is ignored */
	    if (maxfd < HandlerFD[i] || maxfd == (int)-1)
		maxfd = HandlerFD[i];
#endif /* AFS_NT40_ENV */
	}
    *maxfdp = maxfd;
    ReleaseReadLock(&FSYNC_handler_lock);	/* just in case */
}
#endif /* HAVE_POLL && AFS_PTHREAD_ENV */

#endif /* FSSYNC_BUILD_SERVER */
