/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_user.c contains routines specific to the user space UNIX implementation of rx */

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

# include <sys/types.h>		/* fd_set on older platforms */
# include <errno.h>
# include <signal.h>
#ifdef AFS_NT40_ENV
# include <winsock2.h>
#else
# include <unistd.h>		/* select() prototype */
# include <sys/time.h>		/* struct timeval, select() prototype */
# ifndef FD_SET
#  include <sys/select.h>	/* fd_set on newer platforms */
# endif
# include <sys/socket.h>
# include <sys/file.h>
# include <netdb.h>
# include <sys/stat.h>
# include <netinet/in.h>
# include <net/if.h>
# include <sys/ioctl.h>
# include <sys/time.h>
#endif
# include "rx.h"
# include "rx_globals.h"
# include <lwp.h>

#define MAXTHREADNAMELENGTH 64

extern int (*registerProgram) ();
extern int (*swapNameProgram) ();

int debugSelectFailure;		/* # of times select failed */

/*
 * Sleep on the unique wait channel provided.
 */
void
rxi_Sleep(void *addr)
{
    LWP_WaitProcess(addr);
}

/*
 * Wakeup any threads on the channel provided.
 * They may be woken up spuriously, and must check any conditions.
 */
void
rxi_Wakeup(void *addr)
{
    LWP_NoYieldSignal(addr);
}

PROCESS rx_listenerPid = 0;	/* LWP process id of socket listener process */
static int rx_ListenerProc(void *dummy);

/*
 * Delay the current thread the specified number of seconds.
 */
void
rxi_Delay(int sec)
{
    IOMGR_Sleep(sec);
}

static int quitListening = 0;

/* This routine will kill the listener thread, if it exists. */
void
rxi_StopListener(void)
{
    quitListening = 1;
    rxi_ReScheduleEvents();
}

/* This routine will get called by the event package whenever a new,
   earlier than others, event is posted.  If the Listener process
   is blocked in selects, this will unblock it.  It also can be called
   to force a new trip through the rxi_Listener select loop when the set
   of file descriptors it should be listening to changes... */
void
rxi_ReScheduleEvents(void)
{
    if (rx_listenerPid)
	IOMGR_Cancel(rx_listenerPid);
}

void
rxi_InitializeThreadSupport(void)
{
    PROCESS junk;

    LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &junk);
    IOMGR_Initialize();
    FD_ZERO(&rx_selectMask);
}

void
rxi_StartServerProc(void (*proc) (void), int stacksize)
{
    PROCESS scratchPid;
    static int number = 0;
    char name[32];

    sprintf(name, "srv_%d", ++number);
    LWP_CreateProcess((int (*)(void *))proc, stacksize, RX_PROCESS_PRIORITY, (void *)0,
		      "rx_ServerProc", &scratchPid);
    if (registerProgram)
	(*registerProgram) (scratchPid, name);
}

void
rxi_StartListener(void)
{
    /* Priority of listener should be high, so it can keep conns alive */
#define	RX_LIST_STACK	24000
    LWP_CreateProcess(rx_ListenerProc, RX_LIST_STACK, LWP_MAX_PRIORITY,
		      (void *)0, "rx_Listener", &rx_listenerPid);
    if (registerProgram)
	(*registerProgram) (rx_listenerPid, "listener");
}

/* The main loop which listens to the net for datagrams, and handles timeouts
   and retransmissions, etc.  It also is responsible for scheduling the
   execution of pending events (in conjunction with event.c).

   Note interaction of nextPollTime and lastPollWorked.  The idea is that if rx is not
   keeping up with the incoming stream of packets (because there are threads that
   are interfering with its running sufficiently often), rx does a polling select
   before doing a real IOMGR_Select system call.  Doing a real select means that
   we don't have to let other processes run before processing more packets.

   So, our algorithm is that if the last poll on the file descriptor found useful data, or
   we're at the time nextPollTime (which is advanced so that it occurs every 3 or 4 seconds),
   then we try the polling select before the IOMGR_Select.  If we eventually catch up
   (which we can tell by the polling select returning no input packets ready), then we
   don't do a polling select again until several seconds later (via nextPollTime mechanism).
   */

static void
rxi_ListenerProc(fd_set * rfds, int *tnop, struct rx_call **newcallp)
{
    afs_uint32 host;
    u_short port;
    register struct rx_packet *p = (struct rx_packet *)0;
    osi_socket socket;
    struct clock cv;
    afs_int32 nextPollTime;	/* time to next poll FD before sleeping */
    int lastPollWorked, doingPoll;	/* true iff last poll was useful */
    struct timeval tv, *tvp;
    int code;
#ifdef AFS_NT40_ENV
    int i;
#endif
    PROCESS pid;
    char name[MAXTHREADNAMELENGTH] = "srv_0";

    clock_NewTime();
    lastPollWorked = 0;
    nextPollTime = 0;
    code = LWP_CurrentProcess(&pid);
    if (code) {
	fprintf(stderr, "rxi_Listener: Can't get my pid.\n");
	exit(1);
    }
    rx_listenerPid = pid;
    if (swapNameProgram)
	(*swapNameProgram) (pid, "listener", &name[0]);

    for (;;) {
	/* Grab a new packet only if necessary (otherwise re-use the old one) */
	if (p) {
	    rxi_RestoreDataBufs(p);
	} else {
	    if (!(p = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE)))
		osi_Panic("rxi_ListenerProc: no packets!");	/* Shouldn't happen */
	}
	/* Wait for the next event time or a packet to arrive. */
	/* event_RaiseEvents schedules any events whose time has come and
	 * then atomically computes the time to the next event, guaranteeing
	 * that this is positive.  If there is no next event, it returns 0 */
	clock_NewTime();
	if (!rxevent_RaiseEvents(&cv))
	    tvp = NULL;
	else {
	    /* It's important to copy cv to tv, because the 4.3 documentation
	     * for select threatens that *tv may be updated after a select, in
	     * future editions of the system, to indicate how much of the time
	     * period has elapsed.  So we shouldn't rely on tv not being altered. */
	    tv.tv_sec = cv.sec;	/* Time to next event */
	    tv.tv_usec = cv.usec;
	    tvp = &tv;
	}
	rx_stats.selects++;

	*rfds = rx_selectMask;

	if (lastPollWorked || nextPollTime < clock_Sec()) {
	    /* we're catching up, or haven't tried to for a few seconds */
	    doingPoll = 1;
	    nextPollTime = clock_Sec() + 4;	/* try again in 4 seconds no matter what */
	    tv.tv_sec = tv.tv_usec = 0;	/* make sure we poll */
	    tvp = &tv;
	    code = select((int)(rx_maxSocketNumber + 1), rfds, 0, 0, tvp);
	} else {
	    doingPoll = 0;
	    code = IOMGR_Select((int)(rx_maxSocketNumber + 1), rfds, 0, 0, tvp);
	}
	lastPollWorked = 0;	/* default is that it didn't find anything */

	if (quitListening) {
	    quitListening = 0;
	    LWP_DestroyProcess(pid);
	}

	switch (code) {
	case 0:
	    /* Timer interrupt:
	     * If it was a timer interrupt then we can assume that
	     * the time has advanced by roughly the value of the
	     * previous timeout, and that there is now at least
	     * one pending event.
	     */
	    clock_NewTime();
	    break;
	case -1:
	    /* select or IOMGR_Select returned failure */
	    debugSelectFailure++;	/* update debugging counter */
	    clock_NewTime();
	    break;
	case -2:
	    /* IOMGR_Cancel:
	     * IOMGR_Cancel is invoked whenever a new event is
	     * posted that is earlier than any existing events.
	     * So we re-evaluate the time, and then go back to
	     * reschedule events
	     */
	    clock_NewTime();
	    break;

	default:
	    /* Packets have arrived, presumably:
	     * If it wasn't a timer interrupt, then no event should have
	     * timed out yet (well some event may have, but only just...), so
	     * we don't bother looking to see if any have timed out, but just
	     * go directly to reading the data packets
	     */
	    clock_NewTime();
	    if (doingPoll)
		lastPollWorked = 1;
#ifdef AFS_NT40_ENV
	    for (i = 0; p && i < rfds->fd_count; i++) {
		socket = rfds->fd_array[i];
		if (rxi_ReadPacket(socket, p, &host, &port)) {
		    *newcallp = NULL;
		    p = rxi_ReceivePacket(p, socket, host, port, tnop,
					  newcallp);
		    if (newcallp && *newcallp) {
			if (p) {
			    rxi_FreePacket(p);
			}
			if (swapNameProgram) {
			    (*swapNameProgram) (rx_listenerPid, &name, 0);
			    rx_listenerPid = 0;
			}
			return;
		    }
		}
	    }
#else
	    for (socket = rx_minSocketNumber;
		 p && socket <= rx_maxSocketNumber; socket++) {
		if (!FD_ISSET(socket, rfds))
		    continue;
		if (rxi_ReadPacket(socket, p, &host, &port)) {
		    p = rxi_ReceivePacket(p, socket, host, port, tnop,
					  newcallp);
		    if (newcallp && *newcallp) {
			if (p) {
			    rxi_FreePacket(p);
			}
			if (swapNameProgram) {
			    (*swapNameProgram) (rx_listenerPid, &name, 0);
			    rx_listenerPid = 0;
			}
			return;
		    }
		}
	    }
#endif
	    break;
	}
    }
    /* NOTREACHED */
}

/* This is the listener process request loop. The listener process loop
 * becomes a server thread when rxi_ListenerProc returns, and stays
 * server thread until rxi_ServerProc returns. */
static int
rx_ListenerProc(void *dummy)
{
    int threadID;
    osi_socket sock;
    struct rx_call *newcall;
    fd_set *rfds;

    if (!(rfds = IOMGR_AllocFDSet())) {
	osi_Panic("rx_ListenerProc: no fd_sets!\n");
    }

    while (1) {
	newcall = NULL;
	threadID = -1;
	rxi_ListenerProc(rfds, &threadID, &newcall);
	/* assert(threadID != -1); */
	/* assert(newcall != NULL); */
	sock = OSI_NULLSOCKET;
	rxi_ServerProc(threadID, newcall, &sock);
	/* assert(sock != OSI_NULLSOCKET); */
    }
    /* not reached */
}

/* This is the server process request loop. The server process loop
 * becomes a listener thread when rxi_ServerProc returns, and stays
 * listener thread until rxi_ListenerProc returns. */
void
rx_ServerProc(void)
{
    osi_socket sock;
    int threadID;
    struct rx_call *newcall = NULL;
    fd_set *rfds;

    if (!(rfds = IOMGR_AllocFDSet())) {
	osi_Panic("rxi_ListenerProc: no fd_sets!\n");
    }

    rxi_MorePackets(rx_maxReceiveWindow + 2);	/* alloc more packets */
    rxi_dataQuota += rx_initSendWindow;	/* Reserve some pkts for hard times */
    /* threadID is used for making decisions in GetCall.  Get it by bumping
     * number of threads handling incoming calls */
    threadID = rxi_availProcs++;

    while (1) {
	sock = OSI_NULLSOCKET;
	rxi_ServerProc(threadID, newcall, &sock);
	/* assert(sock != OSI_NULLSOCKET); */
	newcall = NULL;
	rxi_ListenerProc(rfds, &threadID, &newcall);
	/* assert(threadID != -1); */
	/* assert(newcall != NULL); */
    }
    /* not reached */
}

/*
 * Called from GetUDPSocket.
 * Called from a single thread at startup.
 * Returns 0 on success; -1 on failure.
 */
int
rxi_Listen(osi_socket sock)
{
#ifndef AFS_NT40_ENV
    /*
     * Put the socket into non-blocking mode so that rx_Listener
     * can do a polling read before entering select
     */
#ifndef AFS_DJGPP_ENV
    if (fcntl(sock, F_SETFL, FNDELAY) == -1) {
	perror("fcntl");
	(osi_Msg "rxi_Listen: unable to set non-blocking mode on socket\n");
	return -1;
    }
#else
    if (__djgpp_set_socket_blocking_mode(sock, 1) < 0) {
	perror("__djgpp_set_socket_blocking_mode");
	(osi_Msg "rxi_Listen: unable to set non-blocking mode on socket\n");
	return -1;
    }
#endif /* AFS_DJGPP_ENV */

    if (sock > FD_SETSIZE - 1) {
	(osi_Msg "rxi_Listen: socket descriptor > (FD_SETSIZE-1) = %d\n",
	 FD_SETSIZE - 1);
	return -1;
    }
#endif

    FD_SET(sock, &rx_selectMask);
    if (sock > rx_maxSocketNumber)
	rx_maxSocketNumber = sock;
    if (sock < rx_minSocketNumber)
	rx_minSocketNumber = sock;
    return 0;
}

/*
 * Recvmsg
 */
int
rxi_Recvmsg(osi_socket socket, struct msghdr *msg_p, int flags)
{
    return recvmsg(socket, msg_p, flags);
}

/*
 * Simulate a blocking sendmsg on the non-blocking socket.
 * It's non blocking because it was set that way for recvmsg.
 */
int
rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags)
{
    fd_set *sfds = (fd_set *) 0;
    while (sendmsg(socket, msg_p, flags) == -1) {
	int err;
	rx_stats.sendSelects++;

	if (!sfds) {
	    if (!(sfds = IOMGR_AllocFDSet())) {
		(osi_Msg "rx failed to alloc fd_set: ");
		perror("rx_sendmsg");
		return 3;
	    }
	    FD_SET(socket, sfds);
	}
#ifdef AFS_NT40_ENV
	if (errno)
#elif defined(AFS_LINUX22_ENV)
	/* linux unfortunately returns ECONNREFUSED if the target port
	 * is no longer in use */
	/* and EAGAIN if a UDP checksum is incorrect */
	if (errno != EWOULDBLOCK && errno != ENOBUFS && errno != ECONNREFUSED
	    && errno != EAGAIN)
#else
	if (errno != EWOULDBLOCK && errno != ENOBUFS)
#endif
	{
	    (osi_Msg "rx failed to send packet: ");
	    perror("rx_sendmsg");
	    return 3;
	}
	while ((err = select(socket + 1, 0, sfds, 0, 0)) != 1) {
	    if (err >= 0 || errno != EINTR)
		osi_Panic("rxi_sendmsg: select error %d.%d", err, errno);
	}
    }
    if (sfds)
	IOMGR_FreeFDSet(sfds);
    return 0;
}

int
rxi_ListenTcp(osi_socket sock)
{
	return 0;
}
