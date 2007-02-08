/*
 * Copyright 2006, Sine Nomine Associates and others.
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
#include "salvsync.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include <rx/rx_queue.h>

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

#define MAX_BIND_TRIES	5	/* Number of times to retry socket bind */



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

static int AddToSalvageQueue(struct SalvageQueueNode * node);
static void DeleteFromSalvageQueue(struct SalvageQueueNode * node);
static void AddToPendingQueue(struct SalvageQueueNode * node);
static void DeleteFromPendingQueue(struct SalvageQueueNode * node);
static struct SalvageQueueNode * LookupPendingCommand(SALVSYNC_command_hdr * qry);
static struct SalvageQueueNode * LookupPendingCommandByPid(int pid);
static void RaiseCommandPrio(struct SalvageQueueNode * node, SALVSYNC_command_hdr * com);

static struct SalvageQueueNode * LookupNode(VolumeId vid, char * partName);
static struct SalvageQueueNode * LookupNodeByCommand(SALVSYNC_command_hdr * qry);
static void AddNodeToHash(struct SalvageQueueNode * node);
static void DeleteNodeFromHash(struct SalvageQueueNode * node);

static afs_int32 SALVSYNC_com_Salvage(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_Cancel(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_RaisePrio(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_Query(SALVSYNC_command * com, SALVSYNC_response * res);
static afs_int32 SALVSYNC_com_CancelAll(SALVSYNC_command * com, SALVSYNC_response * res);


extern int LogLevel;
extern int VInit;
extern pthread_mutex_t vol_salvsync_mutex;

static int AcceptSd = -1;		/* Socket used by server for accepting connections */


/* be careful about rearranging elements in this structure.
 * element placement has been optimized for locality of reference
 * in SALVSYNC_getWork() */
struct SalvageQueue {
    volatile int total_len;
    volatile afs_int32 last_insert;    /* id of last partition to have a salvage node insert */
    volatile int len[VOLMAXPARTS+1];
    volatile struct rx_queue part[VOLMAXPARTS+1];
    pthread_cond_t cv;
};
static struct SalvageQueue salvageQueue;  /* volumes waiting to be salvaged */

struct QueueHead {
    volatile struct rx_queue q;
    volatile int len;
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
LookupNode(afs_uint32 vid, char * partName)
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
    return vsp;
}

static struct SalvageQueueNode *
LookupNodeByCommand(SALVSYNC_command_hdr * qry)
{
    return LookupNode(qry->volume, qry->partName);
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

#ifdef USE_UNIX_SOCKETS
static int
getport(struct sockaddr_un *addr)
{
    int sd;
    char tbuffer[AFSDIR_PATH_MAX]; 
    
    strcompose(tbuffer, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH, "/",
               "fssync.sock", NULL);
    
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, tbuffer, (sizeof(struct sockaddr_un) - sizeof(short)));
    assert((sd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0);
    return sd;
}
#else
static int
getport(struct sockaddr_in *addr)
{
    int sd;

    memset(addr, 0, sizeof(*addr));
    assert((sd = socket(AF_INET, SOCK_STREAM, 0)) >= 0);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_addr.s_addr = htonl(0x7f000001);
    addr->sin_family = AF_INET;	/* was localhost->h_addrtype */
    addr->sin_port = htons(2041);	/* XXXX htons not _really_ neccessary */

    return sd;
}
#endif

static fd_set SALVSYNC_readfds;

static void *
SALVSYNC_syncThread(void * args)
{
    struct sockaddr_in addr;
    int on = 1;
    int code;
    int numTries;
    int tid;
#ifdef USE_UNIX_SOCKETS
    char tbuffer[AFSDIR_PATH_MAX]; 
#endif

#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /* set our 'thread-id' so that the host hold table works */
    MUTEX_ENTER(&rx_stats_mutex);	/* protects rxi_pthread_hinum */
    tid = ++rxi_pthread_hinum;
    MUTEX_EXIT(&rx_stats_mutex);
    pthread_setspecific(rx_thread_id_key, (void *)tid);
    Log("Set thread id %d for SALVSYNC_syncThread\n", tid);

#ifdef USE_UNIX_SOCKETS
    strcompose(tbuffer, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH, "/",
               "fssync.sock", NULL);
    /* ignore errors */
    remove(tbuffer);
#endif /* USE_UNIX_SOCKETS */

    AcceptSd = getport(&addr);
    /* Reuseaddr needed because system inexplicably leaves crud lying around */
    code =
	setsockopt(AcceptSd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
		   sizeof(on));
    if (code)
	Log("SALVSYNC_sync: setsockopt failed with (%d)\n", errno);

    for (numTries = 0; numTries < MAX_BIND_TRIES; numTries++) {
	if ((code =
	     bind(AcceptSd, (struct sockaddr *)&addr, sizeof(addr))) == 0)
	    break;
	Log("SALVSYNC_sync: bind failed with (%d), will sleep and retry\n",
	    errno);
	sleep(5);
    }
    assert(!code);
    listen(AcceptSd, 100);
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

    if (com.hdr.proto_version != SALVSYNC_PROTO_VERSION) {
	Log("SALVSYNC_com:  invalid protocol version (%u)\n", com.hdr.proto_version);
	res.hdr.response = SYNC_COM_ERROR;
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
    case SALVSYNC_RAISEPRIO:
	/* raise the priority of a salvage */
	res.hdr.response = SALVSYNC_com_RaisePrio(&scom, &sres);
	break;
    case SALVSYNC_QUERY:
	/* query whether a volume is done salvaging */
	res.hdr.response = SALVSYNC_com_Query(&scom, &sres);
	break;
    case SYNC_COM_CHANNEL_CLOSE:
	res.hdr.response = SYNC_OK;
	res.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;
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

static afs_int32
SALVSYNC_com_Salvage(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * node;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    node = LookupNodeByCommand(com->sop);

    /* schedule a salvage for this volume */
    if (node != NULL) {
	switch (node->state) {
	case SALVSYNC_STATE_ERROR:
	case SALVSYNC_STATE_DONE:
	    memcpy(&node->command.com, com->hdr, sizeof(SYNC_command_hdr));
	    memcpy(&node->command.sop, com->sop, sizeof(SALVSYNC_command_hdr));
	    node->command.sop.prio = 0;
	    if (AddToSalvageQueue(node)) {
		code = SYNC_DENIED;
	    }
	    break;
	default:
	    break;
	}
    } else {
	node = (struct SalvageQueueNode *) malloc(sizeof(struct SalvageQueueNode));
	if (node == NULL) {
	    code = SYNC_DENIED;
	    goto done;
	}
	memset(node, 0, sizeof(struct SalvageQueueNode));
	memcpy(&node->command.com, com->hdr, sizeof(SYNC_command_hdr));
	memcpy(&node->command.sop, com->sop, sizeof(SALVSYNC_command_hdr));
	AddNodeToHash(node);
	if (AddToSalvageQueue(node)) {
	    /* roll back */
	    DeleteNodeFromHash(node);
	    free(node);
	    node = NULL;
	    code = SYNC_DENIED;
	    goto done;
	}
    }

    res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
    res->sop->state = node->state;
    res->sop->prio = node->command.sop.prio;

 done:
    return code;
}

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

    node = LookupNodeByCommand(com->sop);

    if (node == NULL) {
	res->sop->state = SALVSYNC_STATE_UNKNOWN;
	res->sop->prio = 0;
    } else {
	res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
	res->sop->prio = node->command.sop.prio;
	res->sop->state = node->state;
	if (node->state == SALVSYNC_STATE_QUEUED) {
	    DeleteFromSalvageQueue(node);
	}
    }

 done:
    return code;
}

static afs_int32
SALVSYNC_com_CancelAll(SALVSYNC_command * com, SALVSYNC_response * res)
{
    struct SalvageQueueNode * np, *nnp;
    struct DiskPartition * dp;

    for (dp = DiskPartitionList ; dp ; dp = dp->next) {
	for (queue_Scan(&salvageQueue.part[dp->index], np, nnp, SalvageQueueNode)) {
	    DeleteFromSalvageQueue(np);
	}
    }

    return SYNC_OK;
}

static afs_int32
SALVSYNC_com_RaisePrio(SALVSYNC_command * com, SALVSYNC_response * res)
{
    afs_int32 code = SYNC_OK;
    struct SalvageQueueNode * node;

    if (SYNC_verifyProtocolString(com->sop->partName, sizeof(com->sop->partName))) {
	code = SYNC_FAILED;
	res->hdr->reason = SYNC_REASON_MALFORMED_PACKET;
	goto done;
    }

    node = LookupNodeByCommand(com->sop);

    /* raise the priority of a salvage */
    if (node == NULL) {
	code = SALVSYNC_com_Salvage(com, res);
	node = LookupNodeByCommand(com->sop);
    } else {
	switch (node->state) {
	case SALVSYNC_STATE_QUEUED:
	    RaiseCommandPrio(node, com->sop);
	    break;
	case SALVSYNC_STATE_SALVAGING:
	    break;
	case SALVSYNC_STATE_ERROR:
	case SALVSYNC_STATE_DONE:
	    code = SALVSYNC_com_Salvage(com, res);
	    break;
	default:
	    break;
	}
    }

    if (node == NULL) {
	res->sop->prio = 0;
	res->sop->state = SALVSYNC_STATE_UNKNOWN;
    } else {
	res->hdr->flags |= SALVSYNC_FLAG_VOL_STATS_VALID;
	res->sop->prio = node->command.sop.prio;
	res->sop->state = node->state;
    }

 done:
    return code;
}

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

    node = LookupNodeByCommand(com->sop);

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
	assert(AddHandler(AcceptSd, SALVSYNC_newconnection));
	AcceptHandler = FindHandler(AcceptSd);
    }
}

static void
AcceptOff(void)
{
    if (AcceptHandler != -1) {
	assert(RemoveHandler(AcceptSd));
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

static int
AddToSalvageQueue(struct SalvageQueueNode * node)
{
    afs_int32 id;

    id = volutil_GetPartitionID(node->command.sop.partName);
    if (id < 0 || id > VOLMAXPARTS) {
	return 1;
    }
    if (!VGetPartitionById_r(id, 0)) {
	/* don't enqueue salvage requests for unmounted partitions */
	return 1;
    }
    queue_Append(&salvageQueue.part[id], node);
    salvageQueue.len[id]++;
    salvageQueue.total_len++;
    salvageQueue.last_insert = id;
    node->partition_id = id;
    node->state = SALVSYNC_STATE_QUEUED;
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
RaiseCommandPrio(struct SalvageQueueNode * node, SALVSYNC_command_hdr * com)
{
    struct SalvageQueueNode *np, *nnp;
    afs_int32 id;

    assert(queue_IsOnQueue(node));

    node->command.sop.prio = com->prio;
    id = node->partition_id;
    if (queue_First(&salvageQueue.part[id], SalvageQueueNode)->command.sop.prio < com->prio) {
	queue_Remove(node);
	queue_Prepend(&salvageQueue.part[id], node);
    } else {
	for (queue_ScanBackwardsFrom(&salvageQueue.part[id], node, np, nnp, SalvageQueueNode)) {
	    if (np->command.sop.prio > com->prio)
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
    struct DiskPartition * dp = NULL, * fdp;
    static afs_int32 next_part_sched = 0;
    struct SalvageQueueNode *node = NULL, *np;

    VOL_LOCK;

    /*
     * wait for work to be scheduled
     * if there are no disk partitions, just sit in this wait loop forever
     */
    while (!salvageQueue.total_len || !DiskPartitionList) {
      assert(pthread_cond_wait(&salvageQueue.cv, &vol_glock_mutex) == 0);
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

static void
SALVSYNC_doneWork_r(struct SalvageQueueNode * node, int result)
{
    afs_int32 partid;
    DeleteFromPendingQueue(node);
    partid = node->partition_id;
    if (partid >=0 && partid <= VOLMAXPARTS) {
	partition_salvaging[partid]--;
    }
    if (result == 0) {
	node->state = SALVSYNC_STATE_DONE;
    } else {
	node->state = SALVSYNC_STATE_ERROR;
    }
}

void 
SALVSYNC_doneWork(struct SalvageQueueNode * node, int result)
{
    VOL_LOCK;
    SALVSYNC_doneWork_r(node, result);
    VOL_UNLOCK;
}

void
SALVSYNC_doneWorkByPid(int pid, int result)
{
    struct SalvageQueueNode * node;

    VOL_LOCK;
    node = LookupPendingCommandByPid(pid);
    if (node != NULL) {
	SALVSYNC_doneWork_r(node, result);
    }
    VOL_UNLOCK;
}

#endif /* AFS_DEMAND_ATTACH_FS */
