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
\*******************************************************************/

#ifndef __LWP_INCLUDE_
#define	__LWP_INCLUDE_	1

#if !defined(KERNEL) && !defined(_KMEMUSER) && !defined(AFS_PTHREAD_ENV)
#include <afs/param.h>
#if defined(USE_UCONTEXT) && defined(HAVE_UCONTEXT_H)
#include <ucontext.h>
#else
#include <setjmp.h>
#endif

#define LWP_SUCCESS	0
#define LWP_EBADPID	-1
#define LWP_EBLOCKED	-2
#define LWP_EINIT	-3
#define LWP_EMAXPROC	-4
#define LWP_ENOBLOCK	-5
#define LWP_ENOMEM	-6
#define LWP_ENOPROCESS	-7
#define LWP_ENOWAIT	-8
#define LWP_EBADCOUNT	-9
#define LWP_EBADEVENT	-10
#define LWP_EBADPRI	-11
#define LWP_NO_STACK	-12
/* These two are for the signal mechanism. */
#define LWP_EBADSIG	-13	/* bad signal number */
#define LWP_ESYSTEM	-14	/* system call failed */
/* These are for the rock mechanism */
#define LWP_ENOROCKS	-15	/* all rocks are in use */
#define LWP_EBADROCK	-16	/* the specified rock does not exist */

#if	defined(USE_PTHREADS) || defined(USE_SOLARIS_THREADS)
#ifdef	USE_SOLARIS_THREADS
#include <thread.h>
typedef int pthread_t;
typedef void *pthread_addr_t;
typedef void *pthread_condattr_t;
typedef void (*pthread_destructor_t) (void *);
typedef pthread_addr_t(*pthread_startroutine_t) (pthread_addr_t);
#define	pthread_mutex_lock	mutex_lock
#define	pthread_mutex_unlock	mutex_unlock
#define	pthread_getspecific	thr_getspecific
#define	pthread_setspecific	thr_setspecific
#define	pthread_yield		thr_yield
/*typedef mutex_t pthread_mutex_t;*/
typedef thread_key_t pthread_key_t;
typedef cond_t PTHREAD_COND, *pthread_cond_t;

#define PTHREAD_DEFAULT_SCHED 1
#define SCHED_FIFO  2
#define MUTEX_FAST_NP              0
#define PTHREAD_DEFAULT_STACK           65536	/* 64 K */
#define PRI_OTHER_MIN   1
#define PRI_OTHER_MAX   127
#define PRI_OTHER_MID   ((PRI_OTHER_MIN + PRI_OTHER_MAX)/2)
#define DESTRUCTOR_TAB_CHUNK 20
#define maskAllSignals(o) thr_sigsetmask(SIG_BLOCK,&pthread_allSignals,&o)
#define restoreAllSignals(o) thr_sigsetmask(SIG_SETMASK,&o,NULL)

typedef struct PTHREAD_MUTEX {
    int kind;
    pthread_t ownerId;
    mutex_t fastMutex;
    int timesInside;
} PTHREAD_MUTEX, *pthread_mutex_t;


typedef struct PTHREAD_ATTR {
    long stackSize;
    int prio;
} PTHREAD_ATTR, *pthread_attr_t;

typedef struct {
    void (*destructor) (void *);
    thread_key_t key;
} dest_slot_t;

typedef struct {
    int nentries;		/* size allocated (in terms of number of slots) */
    int next_free;		/* next free slot */
    dest_slot_t slot[1];
} pthread_destructor_tab_t;
define DTAB_SIZE(size) (sizeof(pthread_destructor_tab_t) +
			(size) * sizeof(dest_slot_t))
#else
#include "pthread.h"
#endif
#include <assert.h>

#define LWP_MAX_PRIORITY	0
#define LWP_NORMAL_PRIORITY	0
#define LWP_NO_PRIORITIES
/*
 * We define PROCESS as a pointer to this struct, rather than simply as
 * a pthread_t  since some applications test for a process handle being
 * non-zero. This can't be done on a pthread_t.
 */
typedef struct lwp_process {
    pthread_t handle;		/* The pthreads handle */
    struct lwp_process *next;	/* Next LWP process */
    char *name;			/* LWP name of the process */
    pthread_startroutine_t ep;	/* Process entry point */
    pthread_addr_t arg;		/* Initial parameter */
} *PROCESS;

struct rock {			/* to hide things associated with this LWP under */
    int tag;			/* unique identifier for this rock */
    char *value;		/* pointer to some arbitrary data structure */
};

#define MAXROCKS	4	/* max no. of rocks per LWP */

#define DEBUGF 		0

#ifndef BDE_THREADS
/*#define CMA_DEBUG 1*/
#endif

#ifdef CMA_DEBUG
#define LWP_CHECKSTUFF(msg)	lwp_checkstuff(msg)
#else
#define LWP_CHECKSTUFF(msg)
#endif

#if DEBUGF
#define debugf(m) printf m
#else
#define debugf(m)
#endif

#define IOMGR_Poll() LWP_DispatchProcess()

/*
 * These two macros can be used to enter/exit the LWP context in a CMA
 * program. They simply acquire/release the global LWP mutex .
 */
extern pthread_mutex_t lwp_mutex;
#define LWP_EXIT_LWP_CONTEXT() pthread_mutex_unlock(&lwp_mutex)
#define LWP_ENTER_LWP_CONTEXT() pthread_mutex_lock(&lwp_mutex)
#else
#ifndef AFS_NT40_ENV
#define lwp_abort() abort()
#endif
/* Maximum priority permissible (minimum is always 0) */
#define LWP_MAX_PRIORITY 4	/* changed from 1 */

/* Usual priority used by user LWPs */
#define LWP_NORMAL_PRIORITY (LWP_MAX_PRIORITY-2)

/* Initial size of eventlist in a PCB; grows dynamically  */
#define EVINITSIZE  5

typedef struct lwp_pcb *PROCESS;

#ifdef AFS_NT40_ENV
#include <windef.h>
typedef struct lwp_pcb {
    char name[32];		/* name of LWP */
    LPVOID fiber;
    int (*funP) ();		/* function to execute on this LWP */
    void *argP;			/* argument for function */
    int priority;		/* LWP priority */
    int stacksize;		/* Just for reference. */
    /* the following are used for scheduling */
    int status:8;
    int eventlistsize:8;
    int eventcnt:8;
    int padding:8;
    void **eventlist;
    int wakevent;
    int waitcnt;
    struct lwp_pcb *next, *prev;
    struct IoRequest *iomgrRequest;
    int index;			/* new number (++) for each process created. */
} lwp_pcb_t;

#else
struct lwp_context {		/* saved context for dispatcher */
    char *topstack;		/* ptr to top of process stack */
#if defined(USE_UCONTEXT) && defined(HAVE_UCONTEXT_H)
    ucontext_t ucontext;
    int state;
#else				/* !HAVE_UCONTEXT_H */
# if defined(sparc) && !defined(__linux__)
# ifdef	save_allregs
    int globals[7 + 1 + 32 + 2 + 32 + 2];	/* g1-g7, y reg, f0-f31, fsr, fq, c0-c31, csr, cq. */
# else
    int globals[8];		/* g1-g7 and y registers. */
# endif
# endif
    jmp_buf setjmp_buffer;
#endif				/* HAVE_UCONTEXT_H */
};

struct rock {			/* to hide things associated with this LWP under */
    int tag;			/* unique identifier for this rock */
    char *value;		/* pointer to some arbitrary data structure */
};

#define MAXROCKS	4	/* max no. of rocks per LWP */

struct lwp_pcb {		/* process control block */
    char name[32];		/* ASCII name */
    int rc;			/* most recent return code */
    char status;		/* status flags */
    char blockflag;		/* if (blockflag), process blocked */
    char eventlistsize;		/* size of eventlist array */
    char padding;		/* force 32-bit alignment */
    void **eventlist;		/* ptr to array of eventids */
    int eventcnt;		/* no. of events currently in eventlist array */
    int wakevent;		/* index of eventid causing wakeup */
    int waitcnt;		/* min number of events awaited */
    int priority;		/* dispatching priority */
    struct lwp_pcb *misc;	/* for LWP internal use only */
    char *stack;		/* ptr to process stack */
    int stacksize;		/* size of stack */
    int stackcheck;		/* first word of stack for overflow checking */
    int (*ep) ();		/* initial entry point */
    char *parm;			/* initial parm for process */
    struct lwp_context
      context;			/* saved context for next dispatch */
    int lwp_rused;		/* no of rocks presently in use */
    struct rock lwp_rlist[MAXROCKS];	/* set of rocks to hide things under */
    struct lwp_pcb *next, *prev;	/* ptrs to next and previous pcb */
    int level;			/* nesting level of critical sections */
    struct IoRequest *iomgrRequest;	/* request we're waiting for */
    int index;			/* LWP index: should be small index; actually is
				 * incremented on each lwp_create_process */
};
#endif /* AFS_NT40_ENV */

extern int lwp_nextindex;	/* Next lwp index to assign */


#ifndef LWP_KERNEL
#define LWP_ActiveProcess	(lwp_cpptr+0)
#define LWP_Index() (LWP_ActiveProcess->index)
#define LWP_HighestIndex() (lwp_nextindex - 1)
#ifndef	AFS_SUN5_ENV		/* Actual functions for solaris */
#define LWP_SignalProcess(event)	LWP_INTERNALSIGNAL(event, 1)
#define LWP_NoYieldSignal(event)	LWP_INTERNALSIGNAL(event, 0)
#endif

extern
#endif
struct lwp_pcb *lwp_cpptr;	/* pointer to current process pcb */

struct lwp_ctl {		/* LWP control structure */
    int processcnt;		/* number of lightweight processes */
    char *outersp;		/* outermost stack pointer */
    struct lwp_pcb *outerpid;	/* process carved by Initialize */
    struct lwp_pcb *first, last;	/* ptrs to first and last pcbs */
#ifdef __hp9000s800
    double dsptchstack[200];	/* stack for dispatcher use only */
    /* force 8 byte alignment        */
#else
    char dsptchstack[800];	/* stack for dispatcher use only */
#endif
};

#ifndef LWP_KERNEL
extern
#endif
char lwp_debug;			/* ON = show LWP debugging trace */

/* 
 * Under hpux, any stack size smaller than 16K seems prone to
 * overflow problems.
 *
 * On Solaris 2.5, gethostbyname() can use up to 21920 bytes of stack
 * space.  Note: when measuring this, it is important to check the
 * amount of stack space it uses for hosts that are known as well as
 * for hosts that are unknown; the stack usage can differ between these
 * cases, and also between machines apparently running the same OS
 * version.
 */
/*
 * On ia64 where the ucontext is used, it can be an extra 48K
 * Need to account for this.  There might be two of these on the
 * stack too. This needs to be checked.
 */
#if defined(USE_UCONTEXT) && defined(HAVE_UCONTEXT_H)
#define AFS_LWP_MINSTACKSIZE  (288 * 1024)
#elif defined(AFS_LINUX22_ENV)
#define AFS_LWP_MINSTACKSIZE	(192 * 1024)
#else
#define AFS_LWP_MINSTACKSIZE	(48 * 1024)
#endif

/* Action to take on stack overflow. */
#define LWP_SOQUIET	1	/* do nothing */
#define LWP_SOABORT	2	/* abort the program */
#define LWP_SOMESSAGE	3	/* print a message and be quiet */
extern int lwp_overflowAction;

/* Tells if stack size counting is enabled. */
extern int lwp_stackUseEnabled;
extern int lwp_MaxStackSeen;

#ifndef	AFS_AIX32_ENV
#define	LWP_CreateProcess2(a, b, c, d, e, f)	\
	LWP_CreateProcess((a), (b), (c), (d), (e), (f))
#endif

/* External function declarations. */
#ifdef AFS_NT40_ENV
#ifndef _MFC_VER		/*skip if doing Microsoft foundation class */
#include <winsock2.h>
#endif
#elif defined(AFS_LINUX20_ENV)
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#else
# include <unistd.h>		/* select() prototype */
# include <sys/types.h>		/* fd_set on older platforms */
# include <sys/time.h>		/* struct timeval, select() prototype */
# ifndef FD_SET
#  include <sys/select.h>	/* fd_set on newer platforms */
# endif
#endif

#endif /* USE_PTHREADS */

/* iomgr.c */
extern fd_set *IOMGR_AllocFDSet(void);
extern int IOMGR_Select(int nfds, fd_set * rfds, fd_set * wfds, fd_set * efds,
			struct timeval *tvp);
extern int IOMGR_Poll(void);
extern void IOMGR_Sleep(int seconds);
extern int IOMGR_Cancel(PROCESS pid);
extern int IOMGR_Initialize(void);
extern void IOMGR_FreeFDSet(fd_set * fds);
extern int IOMGR_SoftSig(int (*aproc) (), char *arock);


/* fasttime.c */
extern int FT_GetTimeOfDay(struct timeval *tv, struct timezone *tz);
extern int FT_Init(int printErrors, int notReally);
extern int FT_AGetTimeOfDay(struct timeval *tv, struct timezone *tz);
extern unsigned int FT_ApproxTime(void);


extern int LWP_WaitForKeystroke(int seconds);	/* -1 => forever */
extern int LWP_GetResponseKey(int seconds, char *key);
extern int LWP_GetLine(char *linebuf, int len);
#ifdef AFS_NT40_ENV
/* lwp.c */
extern int LWP_DispatchProcess(void);
extern int LWP_QWait(void);
extern int LWP_QSignal(PROCESS pid);
#else
extern int LWP_CurrentProcess(PROCESS * pid);
extern PROCESS LWP_ThreadId(void);
#endif
extern int LWP_InitializeProcessSupport(int priority, PROCESS * pid);
extern int LWP_CreateProcess(int (*ep) (), int stacksize, int priority,
			     void *parm, char *name, PROCESS * pid);
extern int LWP_DestroyProcess(PROCESS pid);
extern int LWP_WaitProcess(void *event);
extern int LWP_INTERNALSIGNAL(void *event, int yield);

#ifdef AFS_LINUX24_ENV
/* max time we are allowed to spend in a select call on Linux to avoid 
 lost signal issues */
#define IOMGR_MAXWAITTIME        60	/* seconds */
#else
/* max time we are allowed to spend in a select call on NT */
#define IOMGR_MAXWAITTIME        5	/* seconds */
#endif

/* max time we spend on a select in a Win95 DOS box */
#define IOMGR_WIN95WAITTIME 5000	/* microseconds */

#endif /* __LWP_INCLUDE_ */

#endif /* !KERNEL && !_KMEMUSER */
