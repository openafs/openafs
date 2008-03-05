/*
 * Copyright 2006-2008, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * salvsync-server.c
 *
 * OpenAFS demand attach fileserver
 * Salvage server synchronization with fileserver.
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
#include <afs/procmgmt.h>

#if !defined(offsetof)
#include <stddef.h>
#endif

#ifdef USE_UNIX_SOCKETS
#include <afs/afsutil.h>
#include <sys/un.h>
#endif


/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)

#define MAXHANDLERS	4	/* Up to 4 clients; must be at least 2, so that
				 * move = dump+restore can run on single server */

/* Forward declarations */
static void * SALVSYNC_syncThread(void *);
static void SALVSYNC_newconnection(int fd);
static void SALVSYNC_com(int fd);
static void SALVSYNC_Drop(int fd);
static void AcceptOn(void);
static void AcceptOff(void);
static void InitHandler(void);
static void CallHandler(fd_set * fdsetp);
static int AddHandler(int afd, void (*aproc) (int));
static int FindHandler(register int afd);
static int FindHandler_r(register int afd);
static int RemoveHandler(register int afd);
static void GetHandler(fd_set * fdsetp, int *maxfdp);


/*
 * This lock controls access to the handler array.
 */
struct Lock SALVSYNC_handler_lock;


#ifdef AFS_DEMAND_ATTACH_FS
/*
 * SALVSYNC is a feature specific to the demand attach fileserver
 */

static int AllocNode(struct SalvageQueueNode ** node);

static int AddToSalvageQueue(struct SalvageQueueNode * node);
static void DeleteFromSalvageQueue(struct SalvageQueueNode * node);
static void AddToPendingQueue(struct SalvageQueueNode * node);
static void DeleteFromPendingQueue(struct SalvageQueueNode * node);
static struct SalvageQueueNode * LookupPendingCommand(SALVSYNC_command_hdr * qry);
static struct SalvageQueueNode * LookupPendingCommandByPid(int pid);
static void UpdateCommandPrio(struct SalvageQueueNode * node);
static void HandlePrio(struct SalvageQueueNode * clone, 
		       struct SalvageQueueNode * parent,
		       afs_uint32 new_prio);

static int LinkNode(struct SalvageQueueNode * parent,
		    struct SalvageQueueNode * clone);

static struct SalvageQueueNode * LookupNode(VolumeId vid, char * partName, 
					    struct SalvageQueueNode ** parent);
static struct SalvageQueueNode * LookupNodeByCommand(SALVSYNC_command_hdr * qry,
						     struct SalvageQueueNode ** parent);
static void AddNodeToHash(struct SalvageQueueNode * node);
static void DeleteNodeFromHash(struct SalvageQueueNode * node);

static afs_int32 SALVSYNC_com_Salvage(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_Cancel(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_Query(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_CancelAll(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_Link(SALVSYNC_command * com, SALVSYNC_response * res);


extern int LogLevel;
extern int VInit;
extern pthread_mutex_t vol_salvsync_mutex;

/**
 * salvsync server socket handle.
 */
static SYNC_server_state_t salvsync_server_state = 
    { -1,                       /* file descriptor */
      SALVSYNC_ENDPOINT_DECL,   /* server endpoint */
      SALVSYNC_PROTO_VERSION,   /* protocol version */
      5,                        /* bind() retry limit */
      100,                      /* listen() queue depth */
      "SALVSYNC",               /* protocol name string */
    };


/**
 * queue of all volumes waiting to be salvaged.
 */
struct SalvageQueue {
    volatile int total_len;
    volatile afs_int32 last_insert;    /**< id of last partition to have a salvage node inserted */
    volatile int len[VOLMAXPARTS+1];
    volatile struct rx_queue part[VOLMAXPARTS+1]; /**< per-partition queues of pending salvages */
    pthread_cond_t cv;
};
static struct SalvageQueue salvageQueue;  /* volumes waiting to be salvaged */

/**
 * queue of all volumes currently being salvaged.
 */
struct QueueHead {
    volatile struct rx_queue q;  /**< queue of salvages in progress */
    volatile int len;            /**< length of in-progress queue */
    pthread_cond_t queue_change_cv;
};
static struct QueueHead pendingQueue;  /* volumes being salvaged */

/* XXX
 * whether a partition has a salvage in progress
 *
 * the salvager code only permits one salvage per partition at a time
 *
 * the following hack tries to keep salvaged parallelism high by
 * only permitting one salvage dispatch per partition at a time
 *
 * unfortunately, the parallel salvager currently
 * has a rather braindead routine that won't permit
 * multiple salvages on the same "device".  this
 * function happens to break pretty badly on lvm, raid luns, etc.
 *
 * this hack isn't good enough to stop the device limiting code from
 * crippling performance.  someday that code needs to be rewritten
 */
static int partition_salvaging[VOLMAXPARTS+1];

#define VSHASH_SIZE 64
#define VSHASH_MASK (VSHASH_SIZE-1)
#define VSHASH(vid) ((vid)&VSHASH_MASK)

static struct QueueHead  SalvageHashTable[VSHASH_SIZE];

static struct SalvageQueueNode *
LookupNode(afs_uint32 vid, char * partName,
	   struct SalvageQueueNode ** parent)
{
    struct rx_queue *qp, *nqp;
    struct SalvageQueueNode *vsp;
    int idx = VSHASH(vid);

    for (queue_Scan(&SalvageHashTable[idx], qp, nqp, rx_queue)) {
	vsp = (struct SalvageQueueNode *)((char *)qp - offsetof(struct SalvageQueueNode, hash_chain));
	if ((vsp->command.sop.volume == vid) &&
	    !strncmp(vsp->command.sop.partName, partName, sizeof(vsp->command.sop.partName))) {
	    break;
	}
    }

    if (queue_IsEnd(&SalvageHashTable[idx], qp)) {
	vsp = NULL;
    }

    if (parent) {
	if (vsp) {
	    *parent = (vsp->type == SALVSYNC_VOLGROUP_CLONE) ?
		vsp->volgroup.parent : vsp;
	} else {
	    *parent = NULL;
	}
    }

    return vsp;
}

static struct SalvageQueueNode *
LookupNodeByCommand(SALVSYNC_command_hdr * qry,
		    struct SalvageQueueNode ** parent)
{
    return LookupNode(qry->volume, qry->partName, parent);
}

static void
AddNodeToHash(struct SalvageQueueNode * node)
{
    int idx = VSHASH(node->command.sop.volume);

    if (queue_IsOnQueue(&node->hash_chain)) {
	return;
    }

    queue_Append(&SalvageHashTable[idx], &node->hash_chain);
    SalvageHashTable[idx].len++;
}

static void
DeleteNodeFromHash(struct SalvageQueueNode * node)
{
    int idx = VSHASH(node->command.sop.volume);

    if (queue_IsNotOnQueue(&node->hash_chain)) {
	return;
    }

    queue_Remove(&node->hash_chain);
    SalvageHashTable[idx].len--;
}

void
SALVSYNC_salvInit(void)
{
    int i;
    pthread_t tid;
    pthread_attr_t tattr;

    /* initialize the queues */
    assert(pthread_cond_init(&salvageQueue.cv, NULL) == 0);
    for (i = 0; i <= VOLMAXPARTS; i++) {
	queue_Init(&salvageQueue.part[i]);
	salvageQueue.len[i] = 0;
    }
    assert(pthread_cond_init(&pendingQueue.queue_change_cv, NULL) == 0);
    queue_Init(&pendingQueue);
    salvageQueue.total_len = pendingQueue.len = 0;
    salvageQueue.last_insert = -1;
    memset(partition_salvaging, 0, sizeof(partition_salvaging));

    for (i = 0; i < VSHASH_SIZE; i++) {
	assert(pthread_cond_init(&SalvageHashTable[i].queue_change_cv, NULL) == 0);
	SalvageHashTable[i].len = 0;
	queue_Init(&SalvageHashTable[i]);
    }

    /* start the salvsync thread */
    assert(pthread_attr_init(&tattr) == 0);
    assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(&tid, &tattr, SALVSYNC_syncThread, NULL) == 0);
}


static fd_set SALVSYNC_readfds;

static void *
SALVSYNC_syncThread(void * args)
{
    int on = 1;
    int code;
    int numTries;
    int tid;
    SYNC_server_state_t * state = &salvsync_server_state;

    SYNC_getAddr(&state->endpoint, &state->addr);
    SYNC_cleanupSock(state);

#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /* set our 'thread-id' so that the host hold table works */
    MUTEX_ENTER(&rx_stats_mutex);	/* protects rxi_pthread_hinum */
    tid = ++rxi_pthread_hinum;
    MUTEX_EXIT(&rx_stats_mutex);
    pthread_setspecific(rx_thread_id_key, (void *)tid);
    Log("Set thread id %d for SALVSYNC_syncThread\n", tid);

    state->fd = SYNC_getSock(&state->endpoint);
    code = SYNC_bindSock(state);
    assert(!code);

    InitHandler();
    AcceptOn();

    for (;;) {
	int maxfd;
	GetHandler(&SALVSYNC_readfds, &maxfd);
	/* Note: check for >= 1 below is essential since IOMGR_select
	 * doesn't have exactly same semantics as select.
	 */
	if (select(maxfd + 1, &SALVSYNC_readfds, NULL, NULL, NULL) >= 1)
	    CallHandler(&SALVSYNC_readfds);
    }

    return NULL;
}

static void
SALVSYNC_newconnection(int afd)
{
#ifdef USE_UNIX_SOCKETS
    struct sockaddr_un other;
#else  /* USE_UNIX_SOCKETS */
    struct sockaddr_in other;
#endif
    int junk, fd;
    junk = sizeof(other);
    fd = accept(afd, (struct sockaddr *)&other, &junk);
    if (fd == -1) {
	Log("SALVSYNC_newconnection:  accept failed, errno==%d\n", errno);
	assert(1 == 2);
    } else if (!AddHandler(fd, SALVSYNC_com)) {
	AcceptOff();
	assert(AddHandler(fd, SALVSYNC_com));
    }
}

/* this function processes commands from an salvsync file descriptor (fd) */
static afs_int32 SALV_cnt = 0;
static void
SALVSYNC_com(int fd)
{
    SYNC_command com;
    SYNC_response res;
    SALVSYNC_response_hdr sres_hdr;
    SALVSYNC_command scom;
    SALVSYNC_response sres;
    SYNC_PROTO_BUF_DECL(buf);
    
    com.payload.buf = (void *)buf;
    com.payload.len = SYNC_PROTO_MAX_LEN;
    res.payload.buf = (void *) &sres_hdr;
    res.payload.len = sizeof(sres_hdr);
    res.hdr.response_len = sizeof(res.hdr) + sizeof(sres_hdr);
    res.hdr.proto_version = SALVSYNC_PROTO_VERSION;

    scom.hdr = &com.hdr;
    scom.sop = (SALVSYNC_command_hdr *) buf;
    scom.com = &com;
    sres.hdr = &res.hdr;
    sres.sop = &sres_hdr;
    sres.res = &res;

    SALV_cnt++;
    if (SYNC_getCom(fd, &com)) {
	Log("SALVSYNC_com:  read failed; dropping connection (cnt=%d)\n", SALV_cnt);
	SALVSYNC_Drop(fd);
	return;
    }

    if (com.recv_len < sizeof(com.hdr)) {
	Log("SALVSYNC_com:  invalid protocol message length (%u)\n", com.recv_len);
	res.hdr.response = SYNC_COM_ERROR;
	res.hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    if (com.hdr.proto_version != SALVSYNC_PROTO_VERSION) {
	Log("SALVSYNC_com:  invalid protocol version (%u)\n", com.hdr.proto_version);
	res.hdr.response = SYNC_COM_ERROR;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    if (com.hdr.command == SYNC_COM_CHANNEL_CLOSE) {
	res.hdr.response = SYNC_OK;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    if (com.recv_len != (sizeof(com.hdr) + sizeof(SALVSYNC_command_hdr))) {
	Log("SALVSYNC_com:  invalid protocol message length (%u)\n", com.recv_len);
	res.hdr.response = SYNC_COM_ERROR;
	res.hdr.reason = SYNC_REASON_MALFORMED_PACKET;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
	goto respond;
    }

    VOL_LOCK;
    switch (com.hdr.command) {
    case SALVSYNC_NOP:
	break;
    case SALVSYNC_SALVAGE:
    case SALVSYNC_RAISEPRIO:
	res.hdr.response = SALVSYNC_com_Salvage(&scom, &sres);
	break;
    case SALVSYNC_CANCEL:
	/* cancel a salvage */
	res.hdr.response = SALVSYNC_com_Cancel(&scom, &sres);
	break;
    case SALVSYNC_CANCELALL:
	/* cancel all queued salvages */
	res.hdr.response = SALVSYNC_com_CancelAll(&scom, &sres);
	break;
    case SALVSYNC_QUERY:
	/* query whether a volume is done salvaging */
	res.hdr.response = SALVSYNC_com_Query(&scom, &sres);
	break;
    case SALVSYNC_OP_LINK:
	/* link a clone to its parent in the scheduler */
	res.hdr.response = SALVSYNC_com_Link(&scom, &sres);
	break;
    default:
	res.hdr.response = SYNC_BAD_COMMAND;
	break;
    }

    sres_hdr.sq_len = salvageQueue.total_len;
    sres_hdr.pq_len = pendingQueue.len;
    VOL_UNLOCK;

 respond:
    SYNC_putRes(fd, &res);
    if (res.hdr.flags & SYNC_FLAG_CHANNEL_SHUTDOWN) {
	SALVSYNC_Drop(fd);
    }
}

/**
 * request that a volume be salvaged.
 *
 * @param[in]  com  inbound command object
 * @param[out] res  outbound response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_DENIED failed to enqueue request
 *    @retval SYNC_FAILED malformed command packet
 *
 * @note this is a SALVSYNC protocol rpc handler
 *
 * @internal
 *
 * @post the volume is enqueued in the to-be-salvaged queue.  
 *       if the volume was already in the salvage queue, its 
 *       priority (and thus its location in the queue) are 
 *       updated.
 */
static afs_int32
SALVSYNC_com_Salvage(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * node, * clone;
    int hash = 0;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    clone = LookupNodeByCommand(com->sop, &node);

    if (node == NULL) {
	if (AllocNode(&node)) {
	    code = SYNC_DENIED;
	    res->hdr->reason = SYNC_REASON_NOMEM;
	    goto done;
	}
	clone = node;
	hash = 1;
    }

    HandlePrio(clone, node, com->sop->prio);

    switch (node->state) {
    case SALVSYNC_STATE_QUEUED:
	UpdateCommandPrio(node);
	break;

    case SALVSYNC_STATE_ERROR:
    case SALVSYNC_STATE_DONE:
    case SALVSYNC_STATE_UNKNOWN:
	memcpy(&clone->command.com, com->hdr, sizeof(SYNC_command_hdr));
	memcpy(&clone->command.sop, com->sop, sizeof(SALVSYNC_command_hdr));

	/* 
	 * make sure volgroup parent partition path is kept coherent
	 *
	 * If we ever want to support non-COW clones on a machine holding
	 * the RW site, please note that this code does not work under the
	 * conditions where someone zaps a COW clone on partition X, and
	 * subsequently creates a full clone on partition Y -- we'd need
	 * an inverse to SALVSYNC_com_Link.
	 *  -- tkeiser 11/28/2007
	 */
	strcpy(node->command.sop.partName, com->sop->partName);

	if (AddToSalvageQueue(node)) {
	    code = SYNC_DENIED;
	}
	break;

    default:
	break;
    }

    if (hash) {
	AddNodeToHash(node);
    }

    res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
    res->sop->state = node->state;
    res->sop->prio = node->command.sop.prio;

 done:
    return code;
}

/**
 * cancel a pending salvage request.
 *
 * @param[in]  com  inbound command object
 * @param[out] res  outbound response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_FAILED malformed command packet
 *
 * @note this is a SALVSYNC protocol rpc handler
 *
 * @internal
 */
static afs_int32
SALVSYNC_com_Cancel(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * node;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    node = LookupNodeByCommand(com->sop, NULL);

    if (node == NULL) {
	res->sop->state = SALVSYNC_STATE_UNKNOWN;
	res->sop->prio = 0;
    } else {
	res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
	res->sop->prio = node->command.sop.prio;
	res->sop->state = node->state;
	if ((node->type == SALVSYNC_VOLGROUP_PARENT) && 
	    (node->state == SALVSYNC_STATE_QUEUED)) {
	    DeleteFromSalvageQueue(node);
	}
    }

 done:
    return code;
}

/**
 * cancel all pending salvage requests.
 *
 * @param[in]  com  incoming command object
 * @param[out] res  outbound response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *
 * @note this is a SALVSYNC protocol rpc handler
 *
 * @internal
 */
static afs_int32
SALVSYNC_com_CancelAll(SALVSYNC_command * com, SALVSYNC_response * res)
{
    struct SalvageQueueNode * np, *nnp;
    struct DiskPartition64 * dp;

    for (dp = DiskPartitionList ; dp ; dp = dp->next) {
	for (queue_Scan(&salvageQueue.part[dp->index], np, nnp, SalvageQueueNode)) {
	    DeleteFromSalvageQueue(np);
	}
    }

    return SYNC_OK;
}

/**
 * link a queue node for a clone to its parent volume.
 *
 * @param[in]  com   inbound command object
 * @param[out] res   outbound response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_FAILED malformed command packet
 *    @retval SYNC_DENIED the request could not be completed
 *
 * @note this is a SALVSYNC protocol rpc handler
 *
 * @post the requested volume is marked as a child of another volume.
 *       thus, future salvage requests for this volume will result in the
 *       parent of the volume group being scheduled for salvage instead
 *       of this clone.
 *
 * @internal
 */
static afs_int32
SALVSYNC_com_Link(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * clone, * parent;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    /* lookup clone's salvage scheduling node */
    clone = LookupNodeByCommand(com->sop, NULL);
    if (clone == NULL) {
	code = SYNC_DENIED;
	res->hdr->reason = SALVSYNC_REASON_ERROR;
	goto done;
    }

    /* lookup parent's salvage scheduling node */
    parent = LookupNode(com->sop->parent, com->sop->partName, NULL);
    if (parent == NULL) {
	if (AllocNode(&parent)) {
	    code = SYNC_DENIED;
	    res->hdr->reason = SYNC_REASON_NOMEM;
	    goto done;
	}
	memcpy(&parent->command.com, com->hdr, sizeof(SYNC_command_hdr));
	memcpy(&parent->command.sop, com->sop, sizeof(SALVSYNC_command_hdr));
	parent->command.sop.volume = parent->command.sop.parent = com->sop->parent;
	AddNodeToHash(parent);
    }

    if (LinkNode(parent, clone)) {
	code = SYNC_DENIED;
	goto done;
    }

 done:
    return code;
}

/**
 * query the status of a volume salvage request.
 *
 * @param[in]  com   inbound command object
 * @param[out] res   outbound response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_FAILED malformed command packet
 *
 * @note this is a SALVSYNC protocol rpc handler
 *
 * @internal
 */
static afs_int32
SALVSYNC_com_Query(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * node;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    LookupNodeByCommand(com->sop, &node);

    /* query whether a volume is done salvaging */
    if (node == NULL) {
	res->sop->state = SALVSYNC_STATE_UNKNOWN;
	res->sop->prio = 0;
    } else {
	res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
	res->sop->state = node->state;
	res->sop->prio = node->command.sop.prio;
    }

 done:
    return code;
}

static void
SALVSYNC_Drop(int fd)
{
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
	assert(AddHandler(salvsync_server_state.fd, SALVSYNC_newconnection));
	AcceptHandler = FindHandler(salvsync_server_state.fd);
    }
}

static void
AcceptOff(void)
{
    if (AcceptHandler != -1) {
	assert(RemoveHandler(salvsync_server_state.fd));
	AcceptHandler = -1;
    }
}

/* The multiple FD handling code. */

static int HandlerFD[MAXHANDLERS];
static void (*HandlerProc[MAXHANDLERS]) (int);

static void
InitHandler(void)
{
    register int i;
    ObtainWriteLock(&SALVSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++) {
	HandlerFD[i] = -1;
	HandlerProc[i] = NULL;
    }
    ReleaseWriteLock(&SALVSYNC_handler_lock);
}

static void
CallHandler(fd_set * fdsetp)
{
    register int i;
    ObtainReadLock(&SALVSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++) {
	if (HandlerFD[i] >= 0 && FD_ISSET(HandlerFD[i], fdsetp)) {
	    ReleaseReadLock(&SALVSYNC_handler_lock);
	    (*HandlerProc[i]) (HandlerFD[i]);
	    ObtainReadLock(&SALVSYNC_handler_lock);
	}
    }
    ReleaseReadLock(&SALVSYNC_handler_lock);
}

static int
AddHandler(int afd, void (*aproc) (int))
{
    register int i;
    ObtainWriteLock(&SALVSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == -1)
	    break;
    if (i >= MAXHANDLERS) {
	ReleaseWriteLock(&SALVSYNC_handler_lock);
	return 0;
    }
    HandlerFD[i] = afd;
    HandlerProc[i] = aproc;
    ReleaseWriteLock(&SALVSYNC_handler_lock);
    return 1;
}

static int
FindHandler(register int afd)
{
    register int i;
    ObtainReadLock(&SALVSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == afd) {
	    ReleaseReadLock(&SALVSYNC_handler_lock);
	    return i;
	}
    ReleaseReadLock(&SALVSYNC_handler_lock);	/* just in case */
    assert(1 == 2);
    return -1;			/* satisfy compiler */
}

static int
FindHandler_r(register int afd)
{
    register int i;
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == afd) {
	    return i;
	}
    assert(1 == 2);
    return -1;			/* satisfy compiler */
}

static int
RemoveHandler(register int afd)
{
    ObtainWriteLock(&SALVSYNC_handler_lock);
    HandlerFD[FindHandler_r(afd)] = -1;
    ReleaseWriteLock(&SALVSYNC_handler_lock);
    return 1;
}

static void
GetHandler(fd_set * fdsetp, int *maxfdp)
{
    register int i;
    register int maxfd = -1;
    FD_ZERO(fdsetp);
    ObtainReadLock(&SALVSYNC_handler_lock);	/* just in case */
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] != -1) {
	    FD_SET(HandlerFD[i], fdsetp);
	    if (maxfd < HandlerFD[i])
		maxfd = HandlerFD[i];
	}
    *maxfdp = maxfd;
    ReleaseReadLock(&SALVSYNC_handler_lock);	/* just in case */
}

/**
 * allocate a salvage queue node.
 *
 * @param[out] node_out  address in which to store new node pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval 1 failed to allocate node
 *
 * @internal
 */
static int
AllocNode(struct SalvageQueueNode ** node_out)
{
    int code = 0;
    struct SalvageQueueNode * node;

    *node_out = node = (struct SalvageQueueNode *) 
	malloc(sizeof(struct SalvageQueueNode));
    if (node == NULL) {
	code = 1;
	goto done;
    }

    memset(node, 0, sizeof(struct SalvageQueueNode));
    node->type = SALVSYNC_VOLGROUP_PARENT;
    node->state = SALVSYNC_STATE_UNKNOWN;

 done:
    return code;
}

/**
 * link a salvage queue node to its parent.
 *
 * @param[in] parent  pointer to queue node for parent of volume group
 * @param[in] clone   pointer to queue node for a clone
 *
 * @return operation status
 *    @retval 0 success
 *    @retval 1 failure
 *
 * @internal
 */
static int
LinkNode(struct SalvageQueueNode * parent,
	 struct SalvageQueueNode * clone)
{
    int code = 0;
    int idx;

    /* check for attaching a clone to a clone */
    if (parent->type != SALVSYNC_VOLGROUP_PARENT) {
	code = 1;
	goto done;
    }

    /* check for pre-existing registration and openings */
    for (idx = 0; idx < VOLMAXTYPES; idx++) {
	if (parent->volgroup.children[idx] == clone) {
	    goto linked;
	}
	if (parent->volgroup.children[idx] == NULL) {
	    break;
	}
    }
    if (idx == VOLMAXTYPES) {
	code = 1;
	goto done;
    }

    /* link parent and child */
    parent->volgroup.children[idx] = clone;
    clone->type = SALVSYNC_VOLGROUP_CLONE;
    clone->volgroup.parent = parent;


 linked:
    switch (clone->state) {
    case SALVSYNC_STATE_QUEUED:
	DeleteFromSalvageQueue(clone);

    case SALVSYNC_STATE_SALVAGING:
	switch (parent->state) {
	case SALVSYNC_STATE_UNKNOWN:
	case SALVSYNC_STATE_ERROR:
	case SALVSYNC_STATE_DONE:
	    parent->command.sop.prio = clone->command.sop.prio;
	    AddToSalvageQueue(parent);
	    break;

	case SALVSYNC_STATE_QUEUED:
	    if (clone->command.sop.prio) {
		parent->command.sop.prio += clone->command.sop.prio;
		UpdateCommandPrio(parent);
	    }
	    break;

	default:
	    break;
	}
	break;

    default:
	break;
    }

 done:
    return code;
}

static void
HandlePrio(struct SalvageQueueNode * clone, 
	   struct SalvageQueueNode * node,
	   afs_uint32 new_prio)
{
    afs_uint32 delta;

    switch (node->state) {
    case SALVSYNC_STATE_ERROR:
    case SALVSYNC_STATE_DONE:
    case SALVSYNC_STATE_UNKNOWN:
	node->command.sop.prio = 0;
	break;
    }

    if (new_prio < clone->command.sop.prio) {
	/* strange. let's just set our delta to 1 */
	delta = 1;
    } else {
	delta = new_prio - clone->command.sop.prio;
    }

    if (clone->type == SALVSYNC_VOLGROUP_CLONE) {
	clone->command.sop.prio = new_prio;
    }

    node->command.sop.prio += delta;
}

static int
AddToSalvageQueue(struct SalvageQueueNode * node)
{
    afs_int32 id;
    struct SalvageQueueNode * last = NULL;

    id = volutil_GetPartitionID(node->command.sop.partName);
    if (id < 0 || id > VOLMAXPARTS) {
	return 1;
    }
    if (!VGetPartitionById_r(id, 0)) {
	/* don't enqueue salvage requests for unmounted partitions */
	return 1;
    }
    if (queue_IsOnQueue(node)) {
	return 0;
    }

    if (queue_IsNotEmpty(&salvageQueue.part[id])) {
	last = queue_Last(&salvageQueue.part[id], SalvageQueueNode);
    }
    queue_Append(&salvageQueue.part[id], node);
    salvageQueue.len[id]++;
    salvageQueue.total_len++;
    salvageQueue.last_insert = id;
    node->partition_id = id;
    node->state = SALVSYNC_STATE_QUEUED;

    /* reorder, if necessary */
    if (last && last->command.sop.prio < node->command.sop.prio) {
	UpdateCommandPrio(node);
    }

    assert(pthread_cond_broadcast(&salvageQueue.cv) == 0);
    return 0;
}

static void
DeleteFromSalvageQueue(struct SalvageQueueNode * node)
{
    if (queue_IsOnQueue(node)) {
	queue_Remove(node);
	salvageQueue.len[node->partition_id]--;
	salvageQueue.total_len--;
	node->state = SALVSYNC_STATE_UNKNOWN;
	assert(pthread_cond_broadcast(&salvageQueue.cv) == 0);
    }
}

static void
AddToPendingQueue(struct SalvageQueueNode * node)
{
    queue_Append(&pendingQueue, node);
    pendingQueue.len++;
    node->state = SALVSYNC_STATE_SALVAGING;
    assert(pthread_cond_broadcast(&pendingQueue.queue_change_cv) == 0);
}

static void
DeleteFromPendingQueue(struct SalvageQueueNode * node)
{
    if (queue_IsOnQueue(node)) {
	queue_Remove(node);
	pendingQueue.len--;
	node->state = SALVSYNC_STATE_UNKNOWN;
	assert(pthread_cond_broadcast(&pendingQueue.queue_change_cv) == 0);
    }
}

static struct SalvageQueueNode *
LookupPendingCommand(SALVSYNC_command_hdr * qry)
{
    struct SalvageQueueNode * np, * nnp;

    for (queue_Scan(&pendingQueue, np, nnp, SalvageQueueNode)) {
	if ((np->command.sop.volume == qry->volume) && 
	    !strncmp(np->command.sop.partName, qry->partName,
		     sizeof(qry->partName)))
	    break;
    }

    if (queue_IsEnd(&pendingQueue, np))
	np = NULL;
    return np;
}

static struct SalvageQueueNode *
LookupPendingCommandByPid(int pid)
{
    struct SalvageQueueNode * np, * nnp;

    for (queue_Scan(&pendingQueue, np, nnp, SalvageQueueNode)) {
	if (np->pid == pid)
	    break;
    }

    if (queue_IsEnd(&pendingQueue, np))
	np = NULL;
    return np;
}


/* raise the priority of a previously scheduled salvage */
static void
UpdateCommandPrio(struct SalvageQueueNode * node)
{
    struct SalvageQueueNode *np, *nnp;
    afs_int32 id;
    afs_uint32 prio;

    assert(queue_IsOnQueue(node));

    prio = node->command.sop.prio;
    id = node->partition_id;
    if (queue_First(&salvageQueue.part[id], SalvageQueueNode)->command.sop.prio < prio) {
	queue_Remove(node);
	queue_Prepend(&salvageQueue.part[id], node);
    } else {
	for (queue_ScanBackwardsFrom(&salvageQueue.part[id], node, np, nnp, SalvageQueueNode)) {
	    if (np->command.sop.prio > prio)
		break;
	}
	if (queue_IsEnd(&salvageQueue.part[id], np)) {
	    queue_Remove(node);
	    queue_Prepend(&salvageQueue.part[id], node);
	} else if (node != np) {
	    queue_Remove(node);
	    queue_InsertAfter(np, node);
	}
    }
}

/* this will need to be rearchitected if we ever want more than one thread
 * to wait for new salvage nodes */
struct SalvageQueueNode * 
SALVSYNC_getWork(void)
{
    int i, ret;
    struct DiskPartition64 * dp = NULL, * fdp;
    static afs_int32 next_part_sched = 0;
    struct SalvageQueueNode *node = NULL, *np;

    VOL_LOCK;

    /*
     * wait for work to be scheduled
     * if there are no disk partitions, just sit in this wait loop forever
     */
    while (!salvageQueue.total_len || !DiskPartitionList) {
	VOL_CV_WAIT(&salvageQueue.cv);
    }

    /* 
     * short circuit for simple case where only one partition has
     * scheduled salvages
     */
    if (salvageQueue.last_insert >= 0 && salvageQueue.last_insert <= VOLMAXPARTS &&
	(salvageQueue.total_len == salvageQueue.len[salvageQueue.last_insert])) {
	node = queue_First(&salvageQueue.part[salvageQueue.last_insert], SalvageQueueNode);
	goto have_node;
    }


    /* 
     * ok, more than one partition has scheduled salvages.
     * now search for partitions with scheduled salvages, but no pending salvages. 
     */
    dp = VGetPartitionById_r(next_part_sched, 0);
    if (!dp) {
	dp = DiskPartitionList;
    }
    fdp = dp;

    for (i=0 ; 
	 !i || dp != fdp ; 
	 dp = (dp->next) ? dp->next : DiskPartitionList, i++ ) {
	if (!partition_salvaging[dp->index] && salvageQueue.len[dp->index]) {
	    node = queue_First(&salvageQueue.part[dp->index], SalvageQueueNode);
	    goto have_node;
	}
    }


    /*
     * all partitions with scheduled salvages have at least one pending.
     * now do an exhaustive search for a scheduled salvage.
     */
    dp = fdp;

    for (i=0 ; 
	 !i || dp != fdp ; 
	 dp = (dp->next) ? dp->next : DiskPartitionList, i++ ) {
	if (salvageQueue.len[dp->index]) {
	    node = queue_First(&salvageQueue.part[dp->index], SalvageQueueNode);
	    goto have_node;
	}
    }

    /* we should never reach this line */
    assert(1==2);

 have_node:
    assert(node != NULL);
    node->pid = 0;
    partition_salvaging[node->partition_id]++;
    DeleteFromSalvageQueue(node);
    AddToPendingQueue(node);

    if (dp) {
	/* update next_part_sched field */
	if (dp->next) {
	    next_part_sched = dp->next->index;
	} else if (DiskPartitionList) {
	    next_part_sched = DiskPartitionList->index;
	} else {
	    next_part_sched = -1;
	}
    }

 bail:
    VOL_UNLOCK;
    return node;
}

/**
 * update internal scheduler state to reflect completion of a work unit.
 *
 * @param[in]  node    salvage queue node object pointer
 * @param[in]  result  worker process result code
 *
 * @post scheduler state is updated.
 *
 * @internal
 */
static void
SALVSYNC_doneWork_r(struct SalvageQueueNode * node, int result)
{
    afs_int32 partid;
    int idx;

    DeleteFromPendingQueue(node);
    partid = node->partition_id;
    if (partid >=0 && partid <= VOLMAXPARTS) {
	partition_salvaging[partid]--;
    }
    if (result == 0) {
	node->state = SALVSYNC_STATE_DONE;
    } else if (result != SALSRV_EXIT_VOLGROUP_LINK) {
	node->state = SALVSYNC_STATE_ERROR;
    }

    if (node->type == SALVSYNC_VOLGROUP_PARENT) {
	for (idx = 0; idx < VOLMAXTYPES; idx++) {
	    if (node->volgroup.children[idx]) {
		node->volgroup.children[idx]->state = node->state;
	    }
	}
    }
}

/**
 * check whether worker child failed.
 *
 * @param[in] status  status bitfield return by wait()
 *
 * @return boolean failure code
 *    @retval 0 child succeeded
 *    @retval 1 child failed
 *
 * @internal
 */
static int
ChildFailed(int status)
{
    return (WCOREDUMP(status) || 
	    WIFSIGNALED(status) || 
	    ((WEXITSTATUS(status) != 0) && 
	     (WEXITSTATUS(status) != SALSRV_EXIT_VOLGROUP_LINK)));
}


/**
 * notify salvsync scheduler of node completion, by child pid.
 *
 * @param[in]  pid     pid of worker child
 * @param[in]  status  worker status bitfield from wait()
 *
 * @post scheduler state is updated.
 *       if status code is a failure, fileserver notification was attempted
 *
 * @see SALVSYNC_doneWork_r
 */
void
SALVSYNC_doneWorkByPid(int pid, int status)
{
    struct SalvageQueueNode * node;
    char partName[16];
    afs_uint32 volids[VOLMAXTYPES+1];
    unsigned int idx;

    memset(volids, 0, sizeof(volids));

    VOL_LOCK;
    node = LookupPendingCommandByPid(pid);
    if (node != NULL) {
	SALVSYNC_doneWork_r(node, status);

	if (ChildFailed(status)) {
	    /* populate volume id list for later processing outside the glock */
	    volids[0] = node->command.sop.volume;
	    strcpy(partName, node->command.sop.partName);
	    if (node->type == SALVSYNC_VOLGROUP_PARENT) {
		for (idx = 0; idx < VOLMAXTYPES; idx++) {
		    if (node->volgroup.children[idx]) {
			volids[idx+1] = node->volgroup.children[idx]->command.sop.volume;
		    }
		}
	    }
	}
    }
    VOL_UNLOCK;

    /*
     * if necessary, notify fileserver of
     * failure to salvage volume group
     * [we cannot guarantee that the child made the
     *  appropriate notifications (e.g. SIGSEGV)]
     *  -- tkeiser 11/28/2007
     */
    if (ChildFailed(status)) {
	for (idx = 0; idx <= VOLMAXTYPES; idx++) {
	    if (volids[idx]) {
		FSYNC_VolOp(volids[idx],
			    partName,
			    FSYNC_VOL_FORCE_ERROR,
			    FSYNC_WHATEVER,
			    NULL);
	    }
	}
    }
}

#endif /* AFS_DEMAND_ATTACH_FS */
