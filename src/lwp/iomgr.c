/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/


/*
	IO Manager routines & server process for VICE server.
*/

/* This controls the size of an fd_set; it must be defined early before
 * the system headers define that type and the macros that operate on it.
 * Its value should be as large as the maximum file descriptor limit we
 * are likely to run into on any platform.  Right now, that is 65536
 * which is the default hard fd limit on Solaris 9 */
/* We don't do this on Windows because on that platform there is code
 * which allocates fd_set's on the stack (IOMGR_Sleep on Win9x, and
 * FDSetAnd on WinNT) */
#ifndef _WIN32
#define FD_SETSIZE 65536
#endif

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <malloc.h>
extern void lwp_abort(void);
#else
#include <unistd.h>		/* select() prototype */
#include <sys/types.h>		/* fd_set on older platforms */
#include <sys/time.h>		/* struct timeval, select() prototype */
#ifndef FD_SET
# include <sys/select.h>	/* fd_set on newer platforms */
#endif
#include <sys/file.h>
#endif /* AFS_NT40_ENV */
#include "lwp.h"
#include "timer.h"
#include <signal.h>
#include <errno.h>
#ifdef AFS_SUN5_ENV
#include <fcntl.h>
#endif
#ifdef AFS_DJGPP_ENV
#include "dosdefs95.h"
#include "netbios95.h"
#include <sys/socket.h>
#include <sys/farptr.h>
#include <dpmi.h>
#include <go32.h>
#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;
#endif /* AFS_DJGPP_ENV */

#if	defined(USE_PTHREADS) || defined(USE_SOLARIS_THREADS)

void IOMGR_Initialize()	/* noop */
{ }

void IOMGR_Sleep (seconds)
  unsigned seconds;
{
    struct timespec itv;

    itv.tv_sec = seconds;
    itv.tv_nsec = 0;
    assert(pthread_mutex_unlock(&lwp_mutex) == 0);
    assert(pthread_delay_np(&itv) == 0);
    assert(pthread_mutex_lock(&lwp_mutex) == 0);
}

#else

#ifdef	AFS_DECOSF_ENV
extern void *malloc();
#endif	/* AFS_DECOSF_ENV */

typedef unsigned char bool;
#define FALSE	0
#define TRUE	1

#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif

#ifndef NSIG
#define NSIG 8*sizeof(sigset_t)
#endif

static int SignalSignals();

/********************************\
* 				 *
*  Stuff for managing IoRequests *
* 				 *
\********************************/

struct IoRequest {

    /* Pid of process making request (for use in IOMGR_Cancel */
    PROCESS		pid;

    /* Descriptor masks for requests */
    int			nfds;
    fd_set		*readfds;
    fd_set		*writefds;
    fd_set		*exceptfds;

    struct TM_Elem	timeout;

    /* Result of select call */
    int			result;

    struct IoRequest    *next;	/* for iorFreeList */

#ifdef AFS_DJGPP_ENV
    NCB                  *ncbp;
    dos_ptr              dos_ncb;
#endif /* AFS_DJGPP_ENV */

};

/********************************\
* 				 *
*  Stuff for managing signals    *
* 				 *
\********************************/

#define badsig(signo)		(((signo) <= 0) || ((signo) >= NSIG))
#define mysigmask(signo)		(1 << ((signo)-1))


fd_set openMask;		/* mask of open files on an IOMGR abort */
static afs_int32 sigsHandled;	/* sigmask(signo) is on if we handle signo */
static int anySigsDelivered;		/* true if any have been delivered. */
#ifndef AFS_NT40_ENV
static struct sigaction oldActions[NSIG];	/* the old signal vectors */
#endif
static char *sigEvents[NSIG];		/* the event to do an LWP signal on */
static int sigDelivered[NSIG];		/* True for signals delivered so far.
					   This is an int array to make sure
					   there are no conflicts when trying
					   to write it */
/* software 'signals' */
#define NSOFTSIG		4
static int (*sigProc[NSOFTSIG])();
static char *sigRock[NSOFTSIG];


static struct IoRequest *iorFreeList = 0;

static struct TM_Elem *Requests;	/* List of requests */
static struct timeval iomgr_timeout;	/* global so signal handler can zap it */

/* stuff for debugging */
static int iomgr_errno;
static struct timeval iomgr_badtv;
static PROCESS iomgr_badpid;
static void SignalIO(int fds, fd_set *rfds, fd_set *wfds, fd_set *efs,
		    int code);
static void SignalTimeout(int code, struct timeval *timeout);

#ifdef AFS_DJGPP_ENV
/* handle Netbios NCB completion */
static int NCB_fd;
int anyNCBComplete = FALSE;
int handler_seg, handler_off;  /* seg:off of NCB completion handler */
static __dpmi_regs        callback_regs;
static _go32_dpmi_seginfo callback_info;
#endif /* AFS_DJGPP_ENV */

/* fd_set pool managment. 
 * Use the pool instead of creating fd_set's on the stack. fd_set's can be
 * 8K in size, so making three could put 24K in the limited space of an LWP
 * stack.
 */
struct IOMGR_fd_set {
    struct IOMGR_fd_set *next;
} *iomgrFreeFDSets = (struct IOMGR_fd_set*)0;

/* IOMGR_FreeFDSet
 * Return fd_set to the free list.
 */
void IOMGR_FreeFDSet(fd_set *s)
{
    struct IOMGR_fd_set *t = (struct IOMGR_fd_set *)s;

    t->next = iomgrFreeFDSets;
    iomgrFreeFDSets = t;
}

/* IOMGR_AllocFDSet
 * returns a zeroed fd_set or null if could not malloc one.
 */
fd_set *IOMGR_AllocFDSet(void)
{
    struct IOMGR_fd_set *t;
    if (iomgrFreeFDSets) {
	t =  iomgrFreeFDSets;
	iomgrFreeFDSets = iomgrFreeFDSets->next;
    }
    else {
	t = (struct IOMGR_fd_set *)malloc(sizeof(fd_set));
    }
    if (!t)
	return (fd_set*)0;
    else {
	FD_ZERO((fd_set*)t);
	return (fd_set*)t;
    }
}

#define FreeRequest(x) ((x)->next = iorFreeList, iorFreeList = (x))

static struct IoRequest *NewRequest()
{
    struct IoRequest *request;

    if ((request=iorFreeList))
	iorFreeList = (struct IoRequest *) (request->next);
    else request = (struct IoRequest *) malloc(sizeof(struct IoRequest));

    memset((char*)request, 0, sizeof(struct IoRequest));
    return request;
}

#define Purge(list) FOR_ALL_ELTS(req, list, { free(req->BackPointer); })


/* FD_SET support routines. All assume the fd_set size is a multiple of an int
 * so we can at least do logical operations on ints instead of chars.
 *
 * For each routine, nfds is the highest bit set in either fd_set, starting
 * with no bits == 0.
 */
#ifdef AFS_NT40_ENV
#define FD_N_ZERO(A, x) FD_ZERO(x)
#else
#define FDS_P_POS (sizeof(int)*8)
#define INTS_PER_FDS(x) (((x)+(FDS_P_POS-1)) / FDS_P_POS)
#define FD_N_ZERO(nfds, x) memset((char*)(x), 0, (INTS_PER_FDS(nfds))*sizeof(int))
#endif

#if defined(AFS_LINUX22_ENV) && (__GLIBC_MINOR__ > 0)
/* Build for both glibc 2.0.x and 2.1.x */
#define FDS_BITS __fds_bits
#else
#define FDS_BITS fds_bits
#endif

/* FDSetCmp - returns 1 if any bits in fd_set1 are also set in fd_set2.
 * If nfds is 0, or one of the fd_sets is null return 0 (since there is no bit
 * set in both fd_sets).
 */
static int FDSetCmp(int nfds, fd_set *fd_set1, fd_set *fd_set2)
{
    unsigned int i, j;

    if (fd_set1 == (fd_set*)0 || fd_set2 == (fd_set*)0)
	return 0;

#ifdef AFS_NT40_ENV
    if (fd_set1->fd_count == 0 || fd_set2->fd_count == 0)
	return 0;

    for (i=0; i<fd_set1->fd_count; i++) {
	for (j=0; j<fd_set2->fd_count; j++) {
	if (fd_set1->fd_array[i] == fd_set2->fd_array[j])
	    return 1;
	}
    }
#else
    if (nfds == 0)
	return 0;

    j = INTS_PER_FDS(nfds);
    for (i=0; i<j; i++) {
	if (fd_set1->FDS_BITS[i] & fd_set2->FDS_BITS[i])
	    return 1;
    }
#endif
    return 0;
}

/* FDSetSet - set bits from fd_set2 into fd_set1
 */
static void FDSetSet(int nfds, fd_set *fd_set1, fd_set *fd_set2)
{
    unsigned int i;
#ifndef AFS_NT40_ENV
    unsigned int n;
#endif

    if (fd_set1 == (fd_set*)0 || fd_set2 == (fd_set*)0)
	return;

#ifdef AFS_NT40_ENV
    if (fd_set2->fd_count==0)
	return;

    for (i=0; i<fd_set2->fd_count; i++)
	FD_SET(fd_set2->fd_array[i], fd_set1);
#else
    if (nfds == 0)
	return;

    for (i = 0, n = INTS_PER_FDS(nfds); i < n; i++) {
	fd_set1->FDS_BITS[i] |= fd_set2->FDS_BITS[i];
    }
#endif
}

/* FDSetAnd - fd_set1  <- fd_set1 & fd_set2. 
 */
#ifdef AFS_NT40_ENV
static void FDSetAnd(int nfds, fd_set *fd_set1, fd_set *fd_set2)
{
    unsigned int i;
    fd_set tmpset;

    if (fd_set1 == NULL || fd_set1->fd_count == 0)
	return;
    
    if (fd_set2 == NULL || fd_set2->fd_count == 0) {
	FD_ZERO(fd_set1);
    }
    else {
	FD_ZERO(&tmpset);
	for (i=0; i<fd_set2->fd_count; i++) {
	    if (FD_ISSET(fd_set2->fd_array[i], fd_set1))
		FD_SET(fd_set2->fd_array[i], &tmpset);
	}
	*fd_set1 = tmpset;
    }
}
#else
static void FDSetAnd(int nfds, fd_set *fd_set1, fd_set *fd_set2)
{
    int i, n;

    if (nfds == 0 || fd_set1 == (fd_set*)0 || fd_set2 == (fd_set*)0)
	return;

    n = INTS_PER_FDS(nfds);
    for (i=0; i<n; i++) {
	fd_set1->FDS_BITS[i] &= fd_set2->FDS_BITS[i];
    }
}
#endif
	    
/* FDSetEmpty - return true if fd_set is empty 
 */
static int FDSetEmpty(int nfds, fd_set *fds)
{
#ifndef AFS_NT40_ENV
    int i, n;

    if (nfds == 0)
	return 1;
#endif
    if (fds == (fd_set*)0)
	return 1;

#ifdef AFS_NT40_ENV
    if (fds->fd_count == 0)
	return 1;
    else
	return 0;
#else
    n = INTS_PER_FDS(nfds);

    for (i=n-1; i>=0; i--) {
	if (fds->FDS_BITS[i])
	    break;
    }

    if (i>=0)
	return 0;
    else
	return 1;
#endif
}

/* The IOMGR process */

/*
 * Important invariant: process->iomgrRequest is null iff request not in timer
 * queue.
 * also, request->pid is valid while request is in queue,
 * also, don't signal selector while request in queue, since selector frees
 *  request.
 */

/* These are not declared in IOMGR so that they don't use up 6K of stack. */
static fd_set IOMGR_readfds, IOMGR_writefds, IOMGR_exceptfds;
static int IOMGR_nfds = 0;

static int IOMGR(void *dummy)
{
    for (;;) {
	int code;
	struct TM_Elem *earliest;
	struct timeval timeout, junk;
	bool woke_someone;

	FD_ZERO(&IOMGR_readfds);
	FD_ZERO(&IOMGR_writefds);
	FD_ZERO(&IOMGR_exceptfds);
	IOMGR_nfds = 0;

	/* Wake up anyone who has expired or who has received a
	   Unix signal between executions.  Keep going until we
	   run out. */
	do {
	    woke_someone = FALSE;
	    /* Wake up anyone waiting on signals. */
	    /* Note: SignalSignals() may yield! */
	    if (anySigsDelivered && SignalSignals ())
		woke_someone = TRUE;
#ifndef AFS_DJGPP_ENV
	    FT_GetTimeOfDay(&junk, 0);    /* force accurate time check */
#endif
	    TM_Rescan(Requests);
	    for (;;) {
		register struct IoRequest *req;
		struct TM_Elem *expired;
		expired = TM_GetExpired(Requests);
		if (expired == NULL) break;
		woke_someone = TRUE;
		req = (struct IoRequest *) expired -> BackPointer;
#ifdef DEBUG
		if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */
		/* no data ready */
		if (req->readfds)   FD_N_ZERO(req->nfds, req->readfds);
		if (req->writefds)  FD_N_ZERO(req->nfds, req->writefds);
		if (req->exceptfds) FD_N_ZERO(req->nfds, req->exceptfds);
		req->nfds = 0;
		req->result = 0; /* no fds ready */
		TM_Remove(Requests, &req->timeout);
#ifdef DEBUG
		req -> timeout.Next = (struct TM_Elem *) 2;
		req -> timeout.Prev = (struct TM_Elem *) 2;
#endif /* DEBUG */
		LWP_QSignal(req->pid);
		req->pid->iomgrRequest = 0;
	    }

#ifdef AFS_DJGPP_ENV
            if (IOMGR_CheckNCB())    /* check for completed netbios requests */
              woke_someone = TRUE;
#endif /* AFS_DJGPP_ENV */

	    if (woke_someone) LWP_DispatchProcess();
	} while (woke_someone);

	/* Collect requests & update times */
	FD_ZERO(&IOMGR_readfds);
	FD_ZERO(&IOMGR_writefds);
	FD_ZERO(&IOMGR_exceptfds);
	IOMGR_nfds = 0;

	FOR_ALL_ELTS(r, Requests, {
	    register struct IoRequest *req;
	    req = (struct IoRequest *) r -> BackPointer;
	    FDSetSet(req->nfds, &IOMGR_readfds,   req->readfds);
	    FDSetSet(req->nfds, &IOMGR_writefds,  req->writefds);
	    FDSetSet(req->nfds, &IOMGR_exceptfds, req->exceptfds);
	    if (req->nfds > IOMGR_nfds)
		IOMGR_nfds = req->nfds;
	})
	earliest = TM_GetEarliest(Requests);
	if (earliest != NULL) {
	    timeout = earliest -> TimeLeft;


	    /* Do select */
#ifdef DEBUG
	    if (lwp_debug != 0) {
#ifdef AFS_NT40_ENV
		int idbg;
		printf("[Read Select:");
		if (IOMGR_readfds.fd_count == 0)
		    printf(" none]\n");
		else {
		    for (idbg=0; idbg<IOMGR_readfds.fd_count; idbg++)
			printf(" %d", IOMGR_readfds.fd_array[idbg]);
		    printf("]\n");
		}
		printf("[Write Select:");
		if (IOMGR_writefds.fd_count == 0)
		    printf(" none]\n");
		else {
		    for (idbg=0; idbg<IOMGR_writefds.fd_count; idbg++)
			printf(" %d", IOMGR_writefds.fd_array[idbg]);
		    printf("]\n");
		}
		printf("[Except Select:");
		if (IOMGR_exceptfds.fd_count == 0)
		    printf(" none]\n");
		else {
		    for (idbg=0; idbg<IOMGR_exceptfds.fd_count; idbg++)
			printf(" %d", IOMGR_exceptfds.fd_array[idbg]);
		    printf("]\n");
		}
#else
		/* Only prints first 32. */
		printf("[select(%d, 0x%x, 0x%x, 0x%x, ", IOMGR_nfds,
		       *(int*)&IOMGR_readfds, *(int*)&IOMGR_writefds,
		       *(int*)&IOMGR_exceptfds);
#endif /* AFS_NT40_ENV */
		if (timeout.tv_sec == -1 && timeout.tv_usec == -1)
		    puts("INFINITE)]");
		else
		    printf("<%d, %d>)]\n", timeout.tv_sec, timeout.tv_usec);
	    }
#endif /* DEBUG */
	    iomgr_timeout = timeout;
	    if (timeout.tv_sec == -1 && timeout.tv_usec == -1) {
		/* infinite, sort of */
		iomgr_timeout.tv_sec = 100000000;
		iomgr_timeout.tv_usec = 0;
	    }
#if defined(AFS_NT40_ENV) || defined(AFS_LINUX24_ENV)
	    /* On NT, signals don't interrupt a select call. So this can potentially
	     * lead to long wait times before a signal is honored. To avoid this we
	     * dont do select() for longer than IOMGR_MAXWAITTIME (5 secs) */
	    /* Whereas Linux seems to sometimes "lose" signals */
	    if (iomgr_timeout.tv_sec > (IOMGR_MAXWAITTIME - 1)) {
	      iomgr_timeout.tv_sec = IOMGR_MAXWAITTIME;
	      iomgr_timeout.tv_usec = 0;
	    }
#endif /* NT40 */

#ifdef AFS_DJGPP_ENV
            /* We do this also for the DOS-box Win95 client, since
               NCB calls don't interrupt a select, but we want to catch them
               in a reasonable amount of time (say, half a second). */
            iomgr_timeout.tv_sec = 0;
            iomgr_timeout.tv_usec = IOMGR_WIN95WAITTIME;
#endif /* DJGPP */

	    /* Check one last time for a signal delivery.  If one comes after
	       this, the signal handler will set iomgr_timeout to zero, causing
	       the select to return immediately.  The timer package won't return
	       a zero timeval because all of those guys were handled above.

	       I'm assuming that the kernel masks signals while it's picking up
	       the parameters to select.  This may a bad assumption.  -DN */
	    if (anySigsDelivered)
		continue;	/* go to the top and handle them. */

#ifdef AFS_DJGPP_ENV
            if (IOMGR_CheckNCB())    /* check for completed netbios requests */
              LWP_DispatchProcess();
#endif /* AFS_DJGPP_ENV */

#ifdef AFS_NT40_ENV
	    if (IOMGR_readfds.fd_count == 0 && IOMGR_writefds.fd_count == 0
		&& IOMGR_exceptfds.fd_count == 0) {
		DWORD stime;
		code = 0;
		if (iomgr_timeout.tv_sec || iomgr_timeout.tv_usec) {
		    stime = iomgr_timeout.tv_sec * 1000
			+ iomgr_timeout.tv_usec/1000;
		    if (!stime)
			stime = 1;
		    Sleep(stime);
		}
	    }
	    else
#endif
		{    /* select runs much faster if 0's are passed instead of &0s */
		    code = select(IOMGR_nfds,
				  (FDSetEmpty(IOMGR_nfds, &IOMGR_readfds)) ?
				  (fd_set*)0 : &IOMGR_readfds,
				  (FDSetEmpty(IOMGR_nfds, &IOMGR_writefds)) ?
				  (fd_set*)0 : &IOMGR_writefds,
				  (FDSetEmpty(IOMGR_nfds, &IOMGR_exceptfds)) ?
				  (fd_set*)0 : &IOMGR_exceptfds,
				  &iomgr_timeout);
		}

	    if (code < 0) {
	       int e=1;

#if defined(AFS_SUN_ENV)
	       /* Tape drives on Sun boxes do not support select and return ENXIO */
	       if (errno == ENXIO) e=0, code=1;
#endif
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_AIX32_ENV)
	       /* For SGI and SVR4 - poll & select can return EAGAIN ... */
	       if (errno == EAGAIN) e=0;
#endif
#if defined(AFS_SUN5_ENV)
               /* On sun4x_55, select doesn't block signal. It could be
                  interupted by a signal that changes iomgr_timeout, and
                  then select returns with EINVAL. In this case, we need
                  to retry.
                */
               if (errno==EINVAL && anySigsDelivered)
                   e = 0;
#endif /* AFS_SUN5_ENV */

	       if ((errno != EINTR) && e) {
#ifndef AFS_NT40_ENV
		  int i;
		  for(i=0; i<FD_SETSIZE; i++) {
		     if (fcntl(i, F_GETFD, 0) < 0 && errno == EBADF)
			 FD_SET(i, &openMask);
		  }
#endif
		  iomgr_errno = errno;
		  lwp_abort();
	       }
	    }

	    /* See what happened */
	    if (code > 0) {
		/* Action -- wake up everyone involved */
		SignalIO(IOMGR_nfds, &IOMGR_readfds, &IOMGR_writefds,
			 &IOMGR_exceptfds, code);
	    }
	    else if (code == 0
		&& (iomgr_timeout.tv_sec != 0 || iomgr_timeout.tv_usec != 0)) {
		/* Real timeout only if signal handler hasn't set
		   iomgr_timeout to zero. */

#if defined(AFS_NT40_ENV) || defined(AFS_LINUX24_ENV)
		/* On NT, real timeout only if above and if iomgr_timeout
		 * interval is equal to timeout interval (i.e., not adjusted
		 * to check for pseudo-signals).
		 */
		/* And also for Linux as above */
		if (iomgr_timeout.tv_sec  != timeout.tv_sec ||
		    iomgr_timeout.tv_usec != timeout.tv_usec) {
		    /* signal check interval timed out; not real timeout */
		    continue;
		}
#endif /* AFS_NT40_ENV */
#ifndef AFS_DJGPP_ENV
	    	FT_GetTimeOfDay(&junk, 0);
#endif
		SignalTimeout(code, &timeout);
	    }
#ifdef AFS_DJGPP_ENV
            IOMGR_CheckNCB();
#endif /* AFS_DJGPP_ENV */
	}
	LWP_DispatchProcess();
    }
    return -1; /* keeps compilers happy. */
}

/************************\
* 			 *
*  Signalling routines 	 *
* 			 *
\************************/

static void SignalIO(int fds, fd_set *readfds, fd_set *writefds,
		     fd_set *exceptfds, int code)
{
    int nfds;
    /* Look at everyone who's bit mask was affected */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	req = (struct IoRequest *) r -> BackPointer;
	nfds = MIN(fds, req->nfds);
	if (FDSetCmp(nfds, req->readfds, readfds) ||
	    FDSetCmp(nfds, req->writefds, writefds) ||
	    FDSetCmp(nfds, req->exceptfds, exceptfds)) {
	    /* put ready fd's into request. */
	    FDSetAnd(nfds, req->readfds, readfds);
	    FDSetAnd(nfds, req->writefds, writefds);
	    FDSetAnd(nfds, req->exceptfds, exceptfds);
	    req -> result = code;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;
	}
    })
}

static void SignalTimeout(int code, struct timeval *timeout)
{
    /* Find everyone who has specified timeout */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	req = (struct IoRequest *) r -> BackPointer;
	if (TM_eql(&r->TimeLeft, timeout)) {
	    req -> result = code;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;
	} else
	    return;
    })
}

/*****************************************************\
*						      *
*  Signal handling routine (not to be confused with   *
*  signalling routines, above).			      *
*						      *
\*****************************************************/
static void SigHandler (signo)
    int signo;
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return;		/* can't happen. */
    sigDelivered[signo] = TRUE;
    anySigsDelivered = TRUE;
    /* Make sure that the IOMGR process doesn't pause on the select. */
    iomgr_timeout.tv_sec = 0;
    iomgr_timeout.tv_usec = 0;
}

/* Alright, this is the signal signalling routine.  It delivers LWP signals
   to LWPs waiting on Unix signals. NOW ALSO CAN YIELD!! */
static int SignalSignals (void)
{
    bool gotone = FALSE;
    register int i;
    register int (*p)();
    afs_int32 stackSize;

    anySigsDelivered = FALSE;

    /* handle software signals */
    stackSize = (AFS_LWP_MINSTACKSIZE < lwp_MaxStackSeen? lwp_MaxStackSeen : AFS_LWP_MINSTACKSIZE);
    for (i=0; i < NSOFTSIG; i++) {
	PROCESS pid;
	if (p=sigProc[i]) /* This yields!!! */
	    LWP_CreateProcess2(p, stackSize, LWP_NORMAL_PRIORITY, 
			       (void *) sigRock[i], "SignalHandler", &pid);
	sigProc[i] = 0;
    }

    for (i = 1; i <= NSIG; ++i)  /* forall !badsig(i) */
	if ((sigsHandled & mysigmask(i)) && sigDelivered[i] == TRUE) {
	    sigDelivered[i] = FALSE;
	    LWP_NoYieldSignal (sigEvents[i]);
	    gotone = TRUE;
	}
    return gotone;
}


/***************************\
* 			    *
*  User-callable routines   *
* 			    *
\***************************/


/* Keep IOMGR process id */
static PROCESS IOMGR_Id = NULL;

int IOMGR_SoftSig(aproc, arock)
int (*aproc)();
char *arock; {
    register int i;
    for (i=0;i<NSOFTSIG;i++) {
	if (sigProc[i] == 0) {
	    /* a free entry */
	    sigProc[i] = aproc;
	    sigRock[i] = arock;
	    anySigsDelivered = TRUE;
	    iomgr_timeout.tv_sec = 0;
	    iomgr_timeout.tv_usec = 0;
	    return 0;
	}
    }
    return -1;
}


unsigned char allOnes[100];

int IOMGR_Initialize(void)
{
    extern int TM_Init();
    PROCESS pid;

    /* If lready initialized, just return */
    if (IOMGR_Id != NULL) return LWP_SUCCESS;

    /* Init LWP if someone hasn't yet. */
    if (LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid) != LWP_SUCCESS)
	return -1;

    /* Initialize request lists */
    if (TM_Init(&Requests) < 0) return -1;

    /* Initialize signal handling stuff. */
    sigsHandled = 0;
    anySigsDelivered = TRUE; /* A soft signal may have happened before
	IOMGR_Initialize:  so force a check for signals regardless */
    memset(allOnes, 0xff, sizeof(allOnes));

#ifdef AFS_DJGPP_ENV
    install_ncb_handler();
#endif /* AFS_DJGPP_ENV */

    return LWP_CreateProcess(IOMGR, AFS_LWP_MINSTACKSIZE, 0, (void *) 0, 
			     "IO MANAGER", &IOMGR_Id);
}

int IOMGR_Finalize()
{
    int status;

    Purge(Requests)
    TM_Final(&Requests);
    status = LWP_DestroyProcess(IOMGR_Id);
    IOMGR_Id = NULL;
    return status;
}

/* signal I/O for anyone who is waiting for a FD or a timeout; not too cheap,
 * since forces select and timeofday check */
int IOMGR_Poll(void) {
    fd_set *readfds, *writefds, *exceptfds;
    afs_int32 code;
    struct timeval tv;
    int fds;

    FT_GetTimeOfDay(&tv, 0);    /* force accurate time check */
    TM_Rescan(Requests);
    for (;;) {
	register struct IoRequest *req;
	struct TM_Elem *expired;
	expired = TM_GetExpired(Requests);
	if (expired == NULL) break;
	req = (struct IoRequest *) expired -> BackPointer;
#ifdef DEBUG
	if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */
	/* no data ready */
	if (req->readfds)   FD_N_ZERO(req->nfds, req->readfds);
	if (req->writefds)  FD_N_ZERO(req->nfds, req->writefds);
	if (req->exceptfds) FD_N_ZERO(req->nfds, req->exceptfds);
	req->nfds = 0;
	req->result = 0; /* no fds ready */
	TM_Remove(Requests, &req->timeout);
#ifdef DEBUG
	req -> timeout.Next = (struct TM_Elem *) 2;
	req -> timeout.Prev = (struct TM_Elem *) 2;
#endif /* DEBUG */
	LWP_QSignal(req->pid);
	req->pid->iomgrRequest = 0;
    }

    /* Collect requests & update times */
    readfds = IOMGR_AllocFDSet();
    writefds = IOMGR_AllocFDSet();
    exceptfds = IOMGR_AllocFDSet();
    if (!(readfds && writefds && exceptfds)) {
	fprintf(stderr, "IOMGR_Poll: Could not malloc space for fd_sets.\n");
	fflush(stderr);
    }

    fds = 0;

    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	req = (struct IoRequest *) r -> BackPointer;
	FDSetSet(req->nfds, readfds,   req->readfds);
	FDSetSet(req->nfds, writefds,  req->writefds);
	FDSetSet(req->nfds, exceptfds, req->exceptfds);
	if (fds < req->nfds)
	    fds = req->nfds;
    })
	
    tv.tv_sec = 0;
    tv.tv_usec = 0;
#ifdef AFS_NT40_ENV
    code = -1;
    if (readfds->fd_count == 0 && writefds->fd_count == 0
	&& exceptfds->fd_count == 0)
#endif
	code = select(fds, readfds, writefds, exceptfds, &tv);
    if (code > 0) {
	SignalIO(fds, readfds, writefds, exceptfds, code);
    }

    if (readfds) IOMGR_FreeFDSet(readfds);
    if (writefds) IOMGR_FreeFDSet(writefds);
    if (exceptfds) IOMGR_FreeFDSet(exceptfds);


    LWP_DispatchProcess();  /* make sure others run */
    LWP_DispatchProcess();
    return 0;
}

int IOMGR_Select(fds, readfds, writefds, exceptfds, timeout)
     int fds;
     fd_set *readfds, *writefds, *exceptfds;
     struct timeval *timeout;
{
    register struct IoRequest *request;
    int result;

#ifndef AFS_NT40_ENV
    if(fds > FD_SETSIZE) {
	fprintf(stderr, "IOMGR_Select: fds=%d, more than max %d\n",
		fds, FD_SETSIZE);
	fflush(stderr);
	lwp_abort();
    }
#endif

    /* See if polling request. If so, handle right here */
    if (timeout != NULL) {
	if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
	    int code;
#ifdef DEBUG
	    if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */
again:
	    code = select(fds, readfds, writefds, exceptfds, timeout);
#if	defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_AIX32_ENV)
	    /*
	     * For SGI and SVR4 - poll & select can return EAGAIN ...
	     */
	    /*
	     * this is basically for SGI, but I believe stock SVR4 (Solaris?)
	     * can also get this error return
	     */
	    if (code < 0 && errno == EAGAIN)
		goto again;	
#endif
#ifdef AFS_NT40_ENV
	    if (code == SOCKET_ERROR) {
		if (WSAGetLastError() == WSAEINPROGRESS)
		    goto again;

		code = -1;
	    }
#endif
	    return (code > 1 ? 1 : code);
	}
    }

    /* Construct request block & insert */
    request = NewRequest(); /* zeroes fd_set's */
    if (readfds && !FDSetEmpty(fds, readfds))
	request->readfds = readfds;
    if (writefds && !FDSetEmpty(fds, writefds))
	request->writefds = writefds;
    if (exceptfds && !FDSetEmpty(fds, exceptfds))
	request->exceptfds = exceptfds;
    request->nfds = fds;

    if (timeout == NULL) {
	request -> timeout.TotalTime.tv_sec = -1;
	request -> timeout.TotalTime.tv_usec = -1;
    } else {
	request -> timeout.TotalTime = *timeout;
	/* check for bad request */
	if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec > 999999) {
	    /* invalid arg */
	    iomgr_badtv = *timeout;
	    iomgr_badpid = LWP_ActiveProcess;
	    /* now fixup request */
	    if(request->timeout.TotalTime.tv_sec < 0)
		request->timeout.TotalTime.tv_sec = 1;
	    request->timeout.TotalTime.tv_usec = 100000;
	}
    }

    request -> timeout.BackPointer = (char *) request;

    /* Insert my PID in case of IOMGR_Cancel */
    request -> pid = LWP_ActiveProcess;
    LWP_ActiveProcess -> iomgrRequest = request;

#ifdef DEBUG
    request -> timeout.Next = (struct TM_Elem *) 1;
    request -> timeout.Prev = (struct TM_Elem *) 1;
#endif /* DEBUG */
    TM_Insert(Requests, &request->timeout);

    /* Wait for action */
    LWP_QWait();

    /* Update parameters & return */
    result = request -> result;

    FreeRequest(request);
    return (result > 1 ? 1 : result);
}

int IOMGR_Cancel(PROCESS pid)
{
    register struct IoRequest *request;

    if ((request = pid->iomgrRequest) == 0) return -1;	/* Pid not found */

    if (request->readfds)   FD_N_ZERO(request->nfds, request->readfds);
    if (request->writefds)  FD_N_ZERO(request->nfds, request->writefds);
    if (request->exceptfds) FD_N_ZERO(request->nfds, request->exceptfds);
    request->nfds = 0;

    request -> result = -2;
    TM_Remove(Requests, &request->timeout);
#ifdef DEBUG
    request -> timeout.Next = (struct TM_Elem *) 5;
    request -> timeout.Prev = (struct TM_Elem *) 5;
#endif /* DEBUG */
    LWP_QSignal(request->pid);
    pid->iomgrRequest = 0;

    return 0;
}

#ifndef AFS_NT40_ENV
/* Cause delivery of signal signo to result in a LWP_SignalProcess of
   event. */
int IOMGR_Signal (int signo, char *event)
{
    struct sigaction sa;

    if (badsig(signo))
	return LWP_EBADSIG;
    if (event == NULL)
	return LWP_EBADEVENT;
    sa.sa_handler = SigHandler;
    sa.sa_mask = *((sigset_t *) allOnes);	/* mask all signals */
    sa.sa_flags = 0;
    sigsHandled |= mysigmask(signo);
    sigEvents[signo] = event;
    sigDelivered[signo] = FALSE;
    if (sigaction (signo, &sa, &oldActions[signo]) == -1)
	return LWP_ESYSTEM;
    return LWP_SUCCESS;
}

/* Stop handling occurrences of signo. */
int IOMGR_CancelSignal (int signo)
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return LWP_EBADSIG;
    sigaction (signo, &oldActions[signo], NULL);
    sigsHandled &= ~mysigmask(signo);
    return LWP_SUCCESS;
}
#endif /* AFS_NT40_ENV */
/* This routine calls select is a fashion that simulates the standard sleep routine */
void IOMGR_Sleep (int seconds)
{
#ifndef AFS_DJGPP_ENV
    struct timeval timeout;

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    IOMGR_Select(0, 0, 0, 0, &timeout);
#else
    struct timeval timeout;
    int s;
    fd_set set, empty;
    FD_ZERO(&empty);
    FD_ZERO(&set);
    s = socket(AF_INET,SOCK_STREAM,0);
    FD_SET(s,&set);

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    IOMGR_Select(1,&set,&empty,&empty,&timeout);
    close(s);
#endif            /* DJGPP  */
}
#endif	/* USE_PTHREADS */


#ifdef AFS_DJGPP_ENV

/* Netbios code for djgpp port */

int IOMGR_NCBSelect(NCB *ncbp, dos_ptr dos_ncb, struct timeval *timeout)
{
  struct IoRequest *request;
  int result;
  
  if (timeout != NULL && timeout->tv_sec == 0 && timeout->tv_usec == 0)
  {
    /* Poll */
    if (ncbp->ncb_event != NULL)
    {
      /* error */
      return -1;
    }
    
    if (get_dos_member_b(NCB, dos_ncb, ncb_cmd_cplt) != 0xff)
    {
      return 1;
    }
    else {
      return 0;
    }
  }

  /* Construct request block & insert */
  request = NewRequest();
  request->ncbp = ncbp;
  request->dos_ncb = dos_ncb;
  
  if (timeout == NULL)
  {
    request->timeout.TotalTime.tv_sec = -1;
    request->timeout.TotalTime.tv_usec = -1;
  }
  else
  {
    request -> timeout.TotalTime = *timeout;
    /* check for bad request */
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec > 999999)
    {
      /* invalid arg */
      iomgr_badtv = *timeout;
      iomgr_badpid = LWP_ActiveProcess;
      /* now fixup request */
      if(request->timeout.TotalTime.tv_sec < 0)
        request->timeout.TotalTime.tv_sec = 1;
      request->timeout.TotalTime.tv_usec = 100000;
    }
  }

  request->timeout.BackPointer = (char *)request;

  /* Insert my PID in case of IOMGR_Cancel */
  request -> pid = LWP_ActiveProcess;
  LWP_ActiveProcess -> iomgrRequest = request;
  
#ifdef DEBUG
  request -> timeout.Next = (struct TM_Elem *) 1;
  request -> timeout.Prev = (struct TM_Elem *) 1;
#endif /* DEBUG */
  TM_Insert(Requests, &request->timeout);

  if (ncbp->ncb_event != NULL)
  {
    /* since we were given an event, we can return immediately and just
       signal the event once the request completes. */
    return 0;
  }
  else
  {
    /* Wait for action */
    
    LWP_QWait();
    
    /* Update parameters & return */
    result = request -> result;

    FreeRequest(request);
    return (result > 1 ? 1 : result);
  }
}
      
int IOMGR_CheckNCB(void)
{
  int woke_someone = FALSE;
  EVENT_HANDLE ev;
  PROCESS pid;
  
  anyNCBComplete = FALSE;
  FOR_ALL_ELTS(r, Requests, {
    register struct IoRequest *req;
    req = (struct IoRequest *) r -> BackPointer;

    if (req->dos_ncb && get_dos_member_b(NCB, req->dos_ncb, ncb_cmd_cplt) != 0xff)
    {
      /* this NCB has completed */
      TM_Remove(Requests, &req->timeout);

      /* copy out NCB from DOS to virtual space */
      dosmemget(req->dos_ncb, sizeof(NCB), (char *) req->ncbp);

      if (ev = req->ncbp->ncb_event)
      {
        thrd_SetEvent(ev);
      }
      else
      {
        woke_someone = TRUE;
        LWP_QSignal(pid=req->pid);
        pid->iomgrRequest = 0;
      }
    }
  })
  return woke_someone;
}

int ncb_handler(__dpmi_regs *r)
{
  anyNCBComplete = TRUE;  /* NCB completed */
  /* Make sure that the IOMGR process doesn't pause on the select. */
  iomgr_timeout.tv_sec = 0;
  iomgr_timeout.tv_usec = 0;
  return;
}

int install_ncb_handler(void)
{
  callback_info.pm_offset = (long) ncb_handler;
  if (_go32_dpmi_allocate_real_mode_callback_retf(&callback_info,
                                                  &callback_regs))
 {
      fprintf(stderr, "error, allocate_real_mode_callback_retf failed\n");
      return -1;
 }

 handler_seg = callback_info.rm_segment;
 handler_off = callback_info.rm_offset;
 
 /*printf("NCB handler_seg=0x%x, off=0x%x\n", handler_seg, handler_off);*/
}
#endif /* AFS_DJGPP_ENV */
