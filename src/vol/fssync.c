/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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
   fsync.c
   File server synchronization with external volume utilities.
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
#include "fssync.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"

/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)

int (*V_BreakVolumeCallbacks) ();

#define MAXHANDLERS	4	/* Up to 4 clients; must be at least 2, so that
				 * move = dump+restore can run on single server */
#define MAXOFFLINEVOLUMES 128	/* This needs to be as big as the maximum
				 * number that would be offline for 1 operation.
				 * Current winner is salvage, which needs all
				 * cloned read-only copies offline when salvaging
				 * a single read-write volume */

#define MAX_BIND_TRIES	5	/* Number of times to retry socket bind */


struct offlineInfo {
    VolumeId volumeID;
    char partName[16];
};

static struct offlineInfo OfflineVolumes[MAXHANDLERS][MAXOFFLINEVOLUMES];

static FS_sd = -1;		/* Client socket for talking to file server */
static AcceptSd = -1;		/* Socket used by server for accepting connections */

static int getport();

struct command {
    bit32 command;
    bit32 reason;
    VolumeId volume;
    char partName[16];		/* partition name, e.g. /vicepa */
};

/* Forward declarations */
static void FSYNC_sync();
static void FSYNC_newconnection();
static void FSYNC_com();
static void FSYNC_Drop();
static void AcceptOn();
static void AcceptOff();
static void InitHandler();
static void CallHandler(fd_set * fdsetp);
static int AddHandler();
static int FindHandler();
static int FindHandler_r();
static int RemoveHandler();
static void GetHandler(fd_set * fdsetp, int *maxfdp);

extern int LogLevel;

/*
 * This lock controls access to the handler array. The overhead
 * is minimal in non-preemptive environments.
 */
struct Lock FSYNC_handler_lock;

int
FSYNC_clientInit(void)
{
    struct sockaddr_in addr;
    /* I can't believe the following is needed for localhost connections!! */
    static time_t backoff[] =
	{ 3, 3, 3, 5, 5, 5, 7, 15, 16, 24, 32, 40, 48, 0 };
    time_t *timeout = &backoff[0];

    for (;;) {
	FS_sd = getport(&addr);
	if (connect(FS_sd, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
	    return 1;
	if (!*timeout)
	    break;
	if (!(*timeout & 1))
	    Log("FSYNC_clientInit temporary failure (will retry)");
	FSYNC_clientFinis();
	sleep(*timeout++);
    }
    perror("FSYNC_clientInit failed (giving up!)");
    return 0;
}

void
FSYNC_clientFinis(void)
{
#ifdef AFS_NT40_ENV
    closesocket(FS_sd);
#else
    close(FS_sd);
#endif
    FS_sd = -1;
}

int
FSYNC_askfs(VolumeId volume, char *partName, int com, int reason)
{
    byte response;
    struct command command;
    int n;
    command.volume = volume;
    command.command = com;
    command.reason = reason;
    if (partName)
	strcpy(command.partName, partName);
    else
	command.partName[0] = 0;
    assert(FS_sd != -1);
    VFSYNC_LOCK;
#ifdef AFS_NT40_ENV
    if (send(FS_sd, (char *)&command, sizeof(command), 0) != sizeof(command)) {
	printf("FSYNC_askfs: write to file server failed\n");
	response = FSYNC_DENIED;
	goto done;
    }
    while ((n = recv(FS_sd, &response, 1, 0)) != 1) {
	if (n == 0 || WSAEINTR != WSAGetLastError()) {
	    printf("FSYNC_askfs: No response from file server\n");
	    response = FSYNC_DENIED;
	    goto done;
	}
    }
#else
    if (write(FS_sd, &command, sizeof(command)) != sizeof(command)) {
	printf("FSYNC_askfs: write to file server failed\n");
	response = FSYNC_DENIED;
	goto done;
    }
    while ((n = read(FS_sd, &response, 1)) != 1) {
	if (n == 0 || errno != EINTR) {
	    printf("FSYNC_askfs: No response from file server\n");
	    response = FSYNC_DENIED;
	    goto done;
	}
    }
#endif
    if (response == 0) {
	printf
	    ("FSYNC_askfs: negative response from file server; volume %u, command %d\n",
	     command.volume, (int)command.command);
    }
  done:
    VFSYNC_UNLOCK;
    return response;
}

void
FSYNC_fsInit(void)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t tid;
    pthread_attr_t tattr;
    assert(pthread_attr_init(&tattr) == 0);
    assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(&tid, &tattr, FSYNC_sync, NULL) == 0);
#else /* AFS_PTHREAD_ENV */
    PROCESS pid;
    assert(LWP_CreateProcess
	   (FSYNC_sync, USUAL_STACK_SIZE, USUAL_PRIORITY, (void *)0,
	    "FSYNC_sync", &pid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */
}

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
    addr->sin_port = htons(2040);	/* XXXX htons not _really_ neccessary */

    return sd;
}

static fd_set FSYNC_readfds;

static void
FSYNC_sync()
{
    struct sockaddr_in addr;
    int on = 1;
    extern VInit;
    int code;
    int numTries;
#ifdef AFS_PTHREAD_ENV
    int tid;
#endif

#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif

#ifdef AFS_PTHREAD_ENV
    /* set our 'thread-id' so that the host hold table works */
    MUTEX_ENTER(&rx_stats_mutex);	/* protects rxi_pthread_hinum */
    tid = ++rxi_pthread_hinum;
    MUTEX_EXIT(&rx_stats_mutex);
    pthread_setspecific(rx_thread_id_key, (void *)tid);
    Log("Set thread id %d for FSYNC_sync\n", tid);
#endif /* AFS_PTHREAD_ENV */

    while (!VInit) {
	/* Let somebody else run until level > 0.  That doesn't mean that 
	 * all volumes have been attached. */
#ifdef AFS_PTHREAD_ENV
	pthread_yield();
#else /* AFS_PTHREAD_ENV */
	LWP_DispatchProcess();
#endif /* AFS_PTHREAD_ENV */
    }
    AcceptSd = getport(&addr);
    /* Reuseaddr needed because system inexplicably leaves crud lying around */
    code =
	setsockopt(AcceptSd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
		   sizeof(on));
    if (code)
	Log("FSYNC_sync: setsockopt failed with (%d)\n", errno);

    for (numTries = 0; numTries < MAX_BIND_TRIES; numTries++) {
	if ((code =
	     bind(AcceptSd, (struct sockaddr *)&addr, sizeof(addr))) == 0)
	    break;
	Log("FSYNC_sync: bind failed with (%d), will sleep and retry\n",
	    errno);
	sleep(5);
    }
    assert(!code);
    listen(AcceptSd, 100);
    InitHandler();
    AcceptOn();
    for (;;) {
	int maxfd;
	GetHandler(&FSYNC_readfds, &maxfd);
	/* Note: check for >= 1 below is essential since IOMGR_select
	 * doesn't have exactly same semantics as select.
	 */
#ifdef AFS_PTHREAD_ENV
	if (select(maxfd + 1, &FSYNC_readfds, NULL, NULL, NULL) >= 1)
#else /* AFS_PTHREAD_ENV */
	if (IOMGR_Select(maxfd + 1, &FSYNC_readfds, NULL, NULL, NULL) >= 1)
#endif /* AFS_PTHREAD_ENV */
	    CallHandler(&FSYNC_readfds);
    }
}

static void
FSYNC_newconnection(int afd)
{
    struct sockaddr_in other;
    int junk, fd;
    junk = sizeof(other);
    fd = accept(afd, (struct sockaddr *)&other, &junk);
    if (fd == -1) {
	Log("FSYNC_newconnection:  accept failed, errno==%d\n", errno);
	assert(1 == 2);
    } else if (!AddHandler(fd, FSYNC_com)) {
	AcceptOff();
	assert(AddHandler(fd, FSYNC_com));
    }
}

/*
#define TEST2081
*/

afs_int32 FS_cnt = 0;
static void
FSYNC_com(int fd)
{
    byte rc = FSYNC_OK;
    int n, i;
    Error error;
    struct command command;
    int leaveonline;
    register struct offlineInfo *volumes, *v;
    Volume *vp;
    char tvolName[VMAXPATHLEN];

    FS_cnt++;
#ifdef AFS_NT40_ENV
    n = recv(fd, &command, sizeof(command), 0);
#else
    n = read(fd, &command, sizeof(command));
#endif
    if (n <= 0) {
	FSYNC_Drop(fd);
	return;
    }
    if (n < sizeof(command)) {
	Log("FSYNC_com:  partial read (%d instead of %d); dropping connection (cnt=%d)\n", n, sizeof(command), FS_cnt);
	FSYNC_Drop(fd);
	return;
    }
    VATTACH_LOCK;
    VOL_LOCK;
    volumes = OfflineVolumes[FindHandler(fd)];
    for (v = 0, i = 0; i < MAXOFFLINEVOLUMES; i++) {
	if (volumes[i].volumeID == command.volume
	    && strcmp(volumes[i].partName, command.partName) == 0) {
	    v = &volumes[i];
	    break;
	}
    }
    switch (command.command) {
    case FSYNC_DONE:
	/* don't try to put online, this call is made only after deleting
	 * a volume, in which case we want to remove the vol # from the
	 * OfflineVolumes array only */
	if (v)
	    v->volumeID = 0;
	break;
    case FSYNC_ON:

/*
This is where a detatched volume gets reattached. However in the
special case where the volume is merely busy, it is already
attatched and it is only necessary to clear the busy flag. See
defect #2080 for details.
*/

	/* is the volume already attatched? */
#ifdef	notdef
/*
 * XXX With the following enabled we had bizarre problems where the backup id would
 * be reset to 0; that was due to the interaction between fileserver/volserver in that they
 * both keep volumes in memory and the changes wouldn't be made to the fileserver. Some of
 * the problems were due to refcnt changes as result of VGetVolume/VPutVolume which would call
 * VOffline, etc. when we don't want to; someday the whole #2080 issue should be revisited to
 * be done right XXX
 */
	vp = VGetVolume_r(&error, command.volume);
	if (vp) {
	    /* yep, is the BUSY flag set? */
	    if (vp->specialStatus == VBUSY) {
/* test harness for defect #2081 */

#ifdef TEST2081
		/*
		 * test #2081 by releasing TEST.2081,
		 * so leave it alone here, zap it after
		 */

		if (strcmp(vp->header->diskstuff.name, "TEST.2081") == 0)
		    break;
#endif
		/* yep, clear BUSY flag */

		vp->specialStatus = 0;
		/* make sure vol is online */
		if (v) {
		    v->volumeID = 0;
		    V_inUse(vp) = 1;	/* online */
		}
		VPutVolume_r(vp);
		break;
	    }
	    VPutVolume_r(vp);
	}
#endif

	/* so, we need to attach the volume */

	if (v)
	    v->volumeID = 0;
	tvolName[0] = '/';
	sprintf(&tvolName[1], VFORMAT, command.volume);

	vp = VAttachVolumeByName_r(&error, command.partName, tvolName,
				   V_VOLUPD);
	if (vp)
	    VPutVolume_r(vp);
	break;
    case FSYNC_OFF:
    case FSYNC_NEEDVOLUME:{
	    leaveonline = 0;
	    /* not already offline, we need to find a slot for newly offline volume */
	    if (!v) {
		for (i = 0; i < MAXOFFLINEVOLUMES; i++) {
		    if (volumes[i].volumeID == 0) {
			v = &volumes[i];
			break;
		    }
		}
	    }
	    if (!v) {
		rc = FSYNC_DENIED;
		break;
	    }
	    vp = VGetVolume_r(&error, command.volume);
	    if (vp) {
		if (command.partName[0] != 0
		    && strcmp(command.partName, vp->partition->name) != 0) {
		    /* volume on desired partition is not online, so we
		     * should treat this as an offline volume.
		     */
		    VPutVolume_r(vp);
		    vp = (Volume *) 0;
		}
	    }
	    if (vp) {
		leaveonline = (command.command == FSYNC_NEEDVOLUME
			       && (command.reason == V_READONLY
				   || (!VolumeWriteable(vp)
				       && (command.reason == V_CLONE
					   || command.reason == V_DUMP))
			       )
		    );
		if (!leaveonline) {
		    if (command.command == FSYNC_NEEDVOLUME
			&& (command.reason == V_CLONE
			    || command.reason == V_DUMP)) {
			vp->specialStatus = VBUSY;
		    }
		    /* remember what volume we got, so we can keep track of how
		     * many volumes the volserver or whatever is using.  Note that
		     * vp is valid since leaveonline is only set when vp is valid.
		     */
		    v->volumeID = command.volume;
		    strcpy(v->partName, vp->partition->name);
		    if (!V_inUse(vp)) {
			/* in this case, VOffline just returns sans decrementing
			 * ref count.  We could try to fix it, but it has lots of
			 * weird callers.
			 */
			VPutVolume_r(vp);
		    } else {
			VOffline_r(vp, "A volume utility is running.");
		    }
		    vp = 0;
		} else {
		    VUpdateVolume_r(&error, vp);	/* At least get volume stats right */
		    if (LogLevel) {
			Log("FSYNC: Volume %u (%s) was left on line for an external %s request\n", V_id(vp), V_name(vp), command.reason == V_CLONE ? "clone" : command.reason == V_READONLY ? "readonly" : command.reason == V_DUMP ? "dump" : "UNKNOWN");
		    }
		}
		if (vp)
		    VPutVolume_r(vp);
	    }
	    rc = FSYNC_OK;
	    break;
	}
    case FSYNC_MOVEVOLUME:
	/* Yuch:  the "reason" for the move is the site it got moved to... */
	/* still set specialStatus so we stop sending back VBUSY.
	 * also should still break callbacks.  Note that I don't know
	 * how to tell if we should break all or not, so we just do it
	 * since it doesn't matter much if we do an extra break
	 * volume callbacks on a volume move within the same server */
	vp = VGetVolume_r(&error, command.volume);
	if (vp) {
	    vp->specialStatus = VMOVED;
	    VPutVolume_r(vp);
	}

	if (V_BreakVolumeCallbacks) {
	    Log("fssync: volume %u moved to %x; breaking all call backs\n",
		command.volume, command.reason);
	    VOL_UNLOCK;
	    VATTACH_UNLOCK;
	    (*V_BreakVolumeCallbacks) (command.volume);
	    VATTACH_LOCK;
	    VOL_LOCK;
	}
	break;
    case FSYNC_RESTOREVOLUME:
	/* if the volume is being restored, break all callbacks on it */
	if (V_BreakVolumeCallbacks) {
	    Log("fssync: volume %u restored; breaking all call backs\n",
		command.volume);
	    VOL_UNLOCK;
	    VATTACH_UNLOCK;
	    (*V_BreakVolumeCallbacks) (command.volume);
	    VATTACH_LOCK;
	    VOL_LOCK;
	}
	break;
    default:
	rc = FSYNC_DENIED;
	break;
    }
    VOL_UNLOCK;
    VATTACH_UNLOCK;
#ifdef AFS_NT40_ENV
    (void)send(fd, &rc, 1, 0);
#else
    (void)write(fd, &rc, 1);
#endif
}

static void
FSYNC_Drop(int fd)
{
    struct offlineInfo *p;
    register i;
    Error error;
    char tvolName[VMAXPATHLEN];

    VATTACH_LOCK;
    VOL_LOCK;
    p = OfflineVolumes[FindHandler(fd)];
    for (i = 0; i < MAXOFFLINEVOLUMES; i++) {
	if (p[i].volumeID) {
	    Volume *vp;

	    tvolName[0] = '/';
	    sprintf(&tvolName[1], VFORMAT, p[i].volumeID);
	    vp = VAttachVolumeByName_r(&error, p[i].partName, tvolName,
				       V_VOLUPD);
	    if (vp)
		VPutVolume_r(vp);
	    p[i].volumeID = 0;
	}
    }
    VOL_UNLOCK;
    VATTACH_UNLOCK;
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
AcceptOn()
{
    if (AcceptHandler == -1) {
	assert(AddHandler(AcceptSd, FSYNC_newconnection));
	AcceptHandler = FindHandler(AcceptSd);
    }
}

static void
AcceptOff()
{
    if (AcceptHandler != -1) {
	assert(RemoveHandler(AcceptSd));
	AcceptHandler = -1;
    }
}

/* The multiple FD handling code. */

static int HandlerFD[MAXHANDLERS];
static int (*HandlerProc[MAXHANDLERS]) ();

static void
InitHandler()
{
    register int i;
    ObtainWriteLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++) {
	HandlerFD[i] = -1;
	HandlerProc[i] = 0;
    }
    ReleaseWriteLock(&FSYNC_handler_lock);
}

static void
CallHandler(fd_set * fdsetp)
{
    register int i;
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

static int
AddHandler(int afd, int (*aproc) ())
{
    register int i;
    ObtainWriteLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == -1)
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
FindHandler(register int afd)
{
    register int i;
    ObtainReadLock(&FSYNC_handler_lock);
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] == afd) {
	    ReleaseReadLock(&FSYNC_handler_lock);
	    return i;
	}
    ReleaseReadLock(&FSYNC_handler_lock);	/* just in case */
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
    ObtainWriteLock(&FSYNC_handler_lock);
    HandlerFD[FindHandler_r(afd)] = -1;
    ReleaseWriteLock(&FSYNC_handler_lock);
    return 1;
}

static void
GetHandler(fd_set * fdsetp, int *maxfdp)
{
    register int i;
    register int maxfd = -1;
    FD_ZERO(fdsetp);
    ObtainReadLock(&FSYNC_handler_lock);	/* just in case */
    for (i = 0; i < MAXHANDLERS; i++)
	if (HandlerFD[i] != -1) {
	    FD_SET(HandlerFD[i], fdsetp);
	    if (maxfd < HandlerFD[i])
		maxfd = HandlerFD[i];
	}
    *maxfdp = maxfd;
    ReleaseReadLock(&FSYNC_handler_lock);	/* just in case */
}
