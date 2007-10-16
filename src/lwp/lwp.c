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
\*******************************************************************/

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header: /cvs/openafs/src/lwp/lwp.c,v 1.27.2.8 2007/06/06 17:23:24 shadow Exp $");

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* allocate externs here */
#define  LWP_KERNEL
#include "lwp.h"
#ifdef	AFS_AIX32_ENV
#include <ulimit.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/pseg.h>
#include <sys/core.h>
#pragma alloca
int setlim(int limcon, uchar_t hard, int limit);
#endif
#ifdef AFS_SGI64_ENV
extern char *getenv();
#include <time.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#if	!defined(USE_PTHREADS) && !defined(USE_SOLARIS_THREADS)

#ifdef	AFS_OSF_ENV
extern void *malloc(int size);
extern void *realloc(void *ptr, int size);
#endif
#if defined(AFS_OSF_ENV) || defined(AFS_S390_LINUX20_ENV)
extern int PRE_Block;		/* from preempt.c */
#else
extern char PRE_Block;		/* from preempt.c */
#endif

#define ON		1
#define OFF		0
#define TRUE		1
#define FALSE		0
#define READY		2
#define WAITING		3
#define DESTROYED	4
#define QWAITING	5
#define MAXINT     (~(1<<((sizeof(int)*8)-1)))
#define MINSTACK   44

#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
#define MINFRAME 128
#define STACK_ALIGN 8
#else
#ifdef __s390x__
#define MINFRAME    160
#define STACK_ALIGN 8
#else
#ifdef __s390__
#define MINFRAME    96
#define STACK_ALIGN 8
#elif defined(AFS_DARWIN_ENV)
#define STACK_ALIGN 16
#else
#define STACK_ALIGN 4
#endif
#endif
#endif

/* Debugging macro */
#ifdef DEBUG
#define Debug(level, msg) do {						\
    if (lwp_debug && lwp_debug >= level) {				\
	printf("***LWP (0x%x): ", lwp_cpptr);				\
	printf msg;							\
	putchar('\n');							\
    }									\
} while (0)
#else
#define Debug(level, msg) do {						\
    ;									\
} while (0)
#endif

static int Dispatcher();
static int Create_Process_Part2();
static int Exit_LWP();
static afs_int32 Initialize_Stack();
static int Stack_Used();
char (*RC_to_ASCII());

static void Abort_LWP();
static void Overflow_Complain();
static void Initialize_PCB();
static void Dispose_of_Dead_PCB();
static void Free_PCB();
static int Internal_Signal();
static purge_dead_pcbs();

#define MAX_PRIORITIES	(LWP_MAX_PRIORITY+1)

struct QUEUE {
    PROCESS head;
    int count;
} runnable[MAX_PRIORITIES], blocked, qwaiting;
/* Invariant for runnable queues: The head of each queue points to the currently running process if it is in that queue, or it points to the next process in that queue that should run. */

/* Offset of stack field within pcb -- used by stack checking stuff */
int stack_offset;

/* special user-tweakable option for AIX */
int lwp_MaxStackSize = 32768;

/* biggest LWP stack created so far */
int lwp_MaxStackSeen = 0;

/* Stack checking action */
int lwp_overflowAction = LWP_SOABORT;

/* Controls stack size counting. */
int lwp_stackUseEnabled = TRUE;	/* pay the price */

int lwp_nextindex;

/* Minimum stack size */
int lwp_MinStackSize = 0;

static int
lwp_remove(register PROCESS p, register struct QUEUE *q)
{
    /* Special test for only element on queue */
    if (q->count == 1)
	q->head = NULL;
    else {
	/* Not only element, do normal remove */
	p->next->prev = p->prev;
	p->prev->next = p->next;
    }
    /* See if head pointing to this element */
    if (q->head == p)
	q->head = p->next;
    q->count--;
    p->next = p->prev = NULL;
    return 0;
}

static int
insert(register PROCESS p, register struct QUEUE *q)
{
    if (q->head == NULL) {	/* Queue is empty */
	q->head = p;
	p->next = p->prev = p;
    } else {			/* Regular insert */
	p->prev = q->head->prev;
	q->head->prev->next = p;
	q->head->prev = p;
	p->next = q->head;
    }
    q->count++;
    return 0;
}

static int
move(PROCESS p, struct QUEUE *from, struct QUEUE *to)
{

    lwp_remove(p, from);

    insert(p, to);
    return 0;
}

/* Iterator macro */
#define for_all_elts(var, q, body)\
	{\
	    register PROCESS var, _NEXT_;\
	    register int _I_;\
	    for (_I_=q.count, var = q.head; _I_>0; _I_--, var=_NEXT_) {\
		_NEXT_ = var -> next;\
		body\
	    }\
	}

/*									    */
/*****************************************************************************\
* 									      *
*  Following section documents the Assembler interfaces used by LWP code      *
* 									      *
\*****************************************************************************/

/*
	savecontext(int (*ep)(), struct lwp_context *savearea, char *sp);

Stub for Assembler routine that will
save the current SP value in the passed
context savearea and call the function
whose entry point is in ep.  If the sp
parameter is NULL, the current stack is
used, otherwise sp becomes the new stack
pointer.

	returnto(struct lwp_context *savearea);

Stub for Assembler routine that will
restore context from a passed savearea
and return to the restored C frame.

*/

/* Macro to force a re-schedule.  Strange name is historical */
#define Set_LWP_RC() savecontext(Dispatcher, &lwp_cpptr->context, NULL)

static struct lwp_ctl *lwp_init = 0;

int
LWP_QWait(void)
{
    register PROCESS tp;
    (tp = lwp_cpptr)->status = QWAITING;
    move(tp, &runnable[tp->priority], &qwaiting);
    Set_LWP_RC();
    return LWP_SUCCESS;
}

int
LWP_QSignal(register PROCESS pid)
{
    if (pid->status == QWAITING) {
	pid->status = READY;
	move(pid, &qwaiting, &runnable[pid->priority]);
	return LWP_SUCCESS;
    } else
	return LWP_ENOWAIT;
}

#ifdef	AFS_AIX32_ENV
char *
reserveFromStack(register afs_int32 size)
{
    char *x;
    x = alloca(size);
    return x;
}
#endif

int
LWP_CreateProcess(int (*ep) (), int stacksize, int priority, void *parm,
		  char *name, PROCESS * pid)
{
    PROCESS temp, temp2;
#ifdef	AFS_AIX32_ENV
    static char *stackptr = 0;
#else
    char *stackptr;
#endif
    char *stackmemory;

#if defined(AFS_LWP_MINSTACKSIZE)
    /*
     * on some systems (e.g. hpux), a minimum usable stack size has
     * been discovered
     */
    if (stacksize < lwp_MinStackSize) {
	stacksize = lwp_MinStackSize;
    }
#endif /* defined(AFS_LWP_MINSTACKSIZE) */
    /* more stack size computations; keep track of for IOMGR */
    if (lwp_MaxStackSeen < stacksize)
	lwp_MaxStackSeen = stacksize;

    Debug(0, ("Entered LWP_CreateProcess"));
    /* Throw away all dead process control blocks */
    purge_dead_pcbs();
    if (lwp_init) {
	temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
	if (temp == NULL) {
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	if (stacksize < MINSTACK)
#ifdef AFS_DARWIN_ENV
	    stacksize = 1008;
#else /* !AFS_DARWIN_ENV */
	    stacksize = 1000;
#endif /* !AFS_DARWIN_ENV */
	else
	    stacksize =
		STACK_ALIGN * ((stacksize + STACK_ALIGN - 1) / STACK_ALIGN);
#ifdef	AFS_AIX32_ENV
	if (!stackptr) {
	    /*
	     * The following signal action for AIX is necessary so that in case of a 
	     * crash (i.e. core is generated) we can include the user's data section 
	     * in the core dump. Unfortunately, by default, only a partial core is
	     * generated which, in many cases, isn't too useful.
	     *
	     * We also do it here in case the main program forgets to do it.
	     */
	    struct sigaction nsa;
	    extern uid_t geteuid();

	    sigemptyset(&nsa.sa_mask);
	    nsa.sa_handler = SIG_DFL;
	    nsa.sa_flags = SA_FULLDUMP;
	    sigaction(SIGABRT, &nsa, NULL);
	    sigaction(SIGSEGV, &nsa, NULL);

	    /*
	     * First we need to increase the default resource limits,
	     * if necessary, so that we can guarantee that we have the
	     * resources to create the core file, but we can't always 
	     * do it as an ordinary user.
	     */
	    if (!geteuid()) {
		/* vos dump causes problems */
		/* setlim(RLIMIT_FSIZE, 0, 1048575); * 1 Gig */
		setlim(RLIMIT_STACK, 0, 65536);	/* 65 Meg */
		setlim(RLIMIT_CORE, 0, 131072);	/* 131 Meg */
	    }
	    /*
	     * Now reserve in one scoop all the stack space that will be used
	     * by the particular application's main (i.e. non-lwp) body. This
	     * is plenty space for any of our applications.
	     */
	    stackptr = reserveFromStack(lwp_MaxStackSize);
	}
	stackptr -= stacksize;
	stackmemory = stackptr;
#else
#ifdef AFS_DARWIN_ENV
	if ((stackmemory = (char *)malloc(stacksize + STACK_ALIGN - 1)) == NULL)
#else /* !AFS_DARWIN_ENV */
	if ((stackmemory = (char *)malloc(stacksize + 7)) == NULL)
#endif /* !AFS_DARWIN_ENV */
	{
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	/* Round stack pointer to byte boundary */
#ifdef AFS_DARWIN_ENV
	stackptr = (char *)(STACK_ALIGN * (((long)stackmemory + STACK_ALIGN - 1) / STACK_ALIGN));
#else /* !AFS_DARWIN_ENV */
	stackptr = (char *)(8 * (((long)stackmemory + 7) / 8));
#endif /* !AFS_DARWIN_ENV */
#endif
	if (priority < 0 || priority >= MAX_PRIORITIES) {
	    Set_LWP_RC();
	    return LWP_EBADPRI;
	}
	Initialize_Stack(stackptr, stacksize);
	Initialize_PCB(temp, priority, stackmemory, stacksize, ep, parm, name);
	insert(temp, &runnable[priority]);
	temp2 = lwp_cpptr;
	if (PRE_Block != 0)
	    Abort_LWP("PRE_Block not 0");

	/* Gross hack: beware! */
	PRE_Block = 1;
	lwp_cpptr = temp;
#if defined(AFS_PARISC_LINUX24_ENV)
	savecontext(Create_Process_Part2, &temp2->context,
		    stackptr + MINFRAME);
#else
#ifdef __hp9000s800
	savecontext(Create_Process_Part2, &temp2->context,
		    stackptr + MINFRAME);
#else
#if defined(AFS_SGI62_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#ifdef sys_x86_darwin_80
	savecontext(Create_Process_Part2, &temp2->context, stackptr + stacksize - 16 - sizeof(void *));	/* 16 = 2 * jmp_buf_type */
#else /* !sys_x86_darwin_80 */
	/* Need to have the sp on an 8-byte boundary for storing doubles. */
	savecontext(Create_Process_Part2, &temp2->context, stackptr + stacksize - 16);	/* 16 = 2 * jmp_buf_type */
#endif /* !sys_x86_darwin_80 */
#else
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV)
	savecontext(Create_Process_Part2, &temp2->context, stackptr + stacksize - 0x40);	/* lomgjmp does something
												 * with %fp + 0x38 */
#else
#if defined(AFS_S390_LINUX20_ENV)
	savecontext(Create_Process_Part2, &temp2->context,
		    stackptr + stacksize - MINFRAME);
#else /* !AFS_S390_LINUX20_ENV */
	savecontext(Create_Process_Part2, &temp2->context,
		    stackptr + stacksize - sizeof(void *));
#endif /* AFS_S390_LINUX20_ENV */
#endif /* AFS_SPARC64_LINUX20_ENV || AFS_SPARC_LINUX20_ENV */
#endif /* AFS_SGI62_ENV */
#endif
#endif
	/* End of gross hack */

	Set_LWP_RC();
	*pid = temp;
	return 0;
    } else
	return LWP_EINIT;
}

#ifdef	AFS_AIX32_ENV
int
LWP_CreateProcess2(int (*ep) (), int stacksize, int priority, void *parm,
		   char *name, PROCESS * pid)
{
    PROCESS temp, temp2;
    char *stackptr;

#if defined(AFS_LWP_MINSTACKSIZE)
    /*
     * on some systems (e.g. hpux), a minimum usable stack size has
     * been discovered
     */
    if (stacksize < lwp_MinStackSize) {
	stacksize = lwp_MinStackSize;
    }
#endif /* defined(AFS_LWP_MINSTACKSIZE) */
    /* more stack size computations; keep track of for IOMGR */
    if (lwp_MaxStackSeen < stacksize)
	lwp_MaxStackSeen = stacksize;

    Debug(0, ("Entered LWP_CreateProcess"));
    /* Throw away all dead process control blocks */
    purge_dead_pcbs();
    if (lwp_init) {
	temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
	if (temp == NULL) {
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	if (stacksize < MINSTACK)
	    stacksize = 1000;
	else
	    stacksize =
		STACK_ALIGN * ((stacksize + STACK_ALIGN - 1) / STACK_ALIGN);
	if ((stackptr = (char *)malloc(stacksize)) == NULL) {
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	if (priority < 0 || priority >= MAX_PRIORITIES) {
	    Set_LWP_RC();
	    return LWP_EBADPRI;
	}
	Initialize_Stack(stackptr, stacksize);
	Initialize_PCB(temp, priority, stackptr, stacksize, ep, parm, name);
	insert(temp, &runnable[priority]);
	temp2 = lwp_cpptr;
	if (PRE_Block != 0)
	    Abort_LWP("PRE_Block not 0");

	/* Gross hack: beware! */
	PRE_Block = 1;
	lwp_cpptr = temp;
	savecontext(Create_Process_Part2, &temp2->context,
		    stackptr + stacksize - sizeof(void *));
	/* End of gross hack */

	Set_LWP_RC();
	*pid = temp;
	return 0;
    } else
	return LWP_EINIT;
}
#endif

int
LWP_CurrentProcess(PROCESS * pid)
{				/* returns pid of current process */
    Debug(0, ("Entered Current_Process"));
    if (lwp_init) {
	*pid = lwp_cpptr;
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

PROCESS
LWP_ThreadId(void)
{
    Debug(0, ("Entered ThreadId"));
    if (lwp_init)
	return lwp_cpptr;
    else
	return (PROCESS) 0;
}

#define LWPANCHOR (*lwp_init)

int
LWP_DestroyProcess(PROCESS pid)
{				/* destroy a lightweight process */
    PROCESS temp;

    Debug(0, ("Entered Destroy_Process"));
    if (lwp_init) {
	if (lwp_cpptr != pid) {
	    Dispose_of_Dead_PCB(pid);
	    Set_LWP_RC();
	} else {
	    pid->status = DESTROYED;
	    move(pid, &runnable[pid->priority], &blocked);
	    temp = lwp_cpptr;
#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
	    savecontext(Dispatcher, &(temp->context),
			&(LWPANCHOR.dsptchstack[MINFRAME]));
#elif defined(AFS_SGI62_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	    savecontext(Dispatcher, &(temp->context),
			&(LWPANCHOR.
			  dsptchstack[(sizeof LWPANCHOR.dsptchstack) - 8]));
#elif defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV)
	    savecontext(Dispatcher, &(temp->context),
			&(LWPANCHOR.
			  dsptchstack[(sizeof LWPANCHOR.dsptchstack) -
				      0x40]));
#elif defined(AFS_S390_LINUX20_ENV)
	    savecontext(Dispatcher, &(temp->context),
			&(LWPANCHOR.
			  dsptchstack[(sizeof LWPANCHOR.dsptchstack) -
				      MINFRAME]));
#else
	    savecontext(Dispatcher, &(temp->context),
			&(LWPANCHOR.
			  dsptchstack[(sizeof LWPANCHOR.dsptchstack) -
				      sizeof(void *)]));
#endif
	}
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

int
LWP_DispatchProcess(void)
{				/* explicit voluntary preemption */
    Debug(2, ("Entered Dispatch_Process"));
    if (lwp_init) {
	Set_LWP_RC();
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

#ifdef DEBUG
int
Dump_Processes(void)
{
    if (lwp_init) {
	register int i;
	for (i = 0; i < MAX_PRIORITIES; i++)
	    for_all_elts(x, runnable[i], {
			 printf("[Priority %d]\n", i);
			 Dump_One_Process(x);
			 }
	)
	    for_all_elts(x, blocked, {
			 Dump_One_Process(x);}
	)
	    for_all_elts(x, qwaiting, {
			 Dump_One_Process(x);}
	)
    } else
	printf("***LWP: LWP support not initialized\n");
    return 0;
}
#endif

int
LWP_GetProcessPriority(PROCESS pid, int *priority)
{				/* returns process priority */
    Debug(0, ("Entered Get_Process_Priority"));
    if (lwp_init) {
	*priority = pid->priority;
	return 0;
    } else
	return LWP_EINIT;
}

int
LWP_InitializeProcessSupport(int priority, PROCESS * pid)
{
    PROCESS temp;
    struct lwp_pcb dummy;
    register int i;
    char *value;

    Debug(0, ("Entered LWP_InitializeProcessSupport"));
    if (lwp_init != NULL)
	return LWP_SUCCESS;

    /* Set up offset for stack checking -- do this as soon as possible */
    stack_offset = (char *)&dummy.stack - (char *)&dummy;

    if (priority >= MAX_PRIORITIES)
	return LWP_EBADPRI;
    for (i = 0; i < MAX_PRIORITIES; i++) {
	runnable[i].head = NULL;
	runnable[i].count = 0;
    }
    blocked.head = NULL;
    blocked.count = 0;
    qwaiting.head = NULL;
    qwaiting.count = 0;
    lwp_init = (struct lwp_ctl *)malloc(sizeof(struct lwp_ctl));
    temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
    if (lwp_init == NULL || temp == NULL)
	Abort_LWP("Insufficient Storage to Initialize LWP Support");
    LWPANCHOR.processcnt = 1;
    LWPANCHOR.outerpid = temp;
    LWPANCHOR.outersp = NULL;
    Initialize_PCB(temp, priority, NULL, 0, NULL, NULL,
		   "Main Process [created by LWP]");
    insert(temp, &runnable[priority]);
    savecontext(Dispatcher, &temp->context, NULL);
    LWPANCHOR.outersp = temp->context.topstack;
    Set_LWP_RC();
    *pid = temp;

    /* get minimum stack size from the environment. this allows the  administrator
     * to change the lwp stack dynamically without getting a new binary version.
     */
    if ((value = getenv("AFS_LWP_STACK_SIZE")) == NULL)
	lwp_MinStackSize = AFS_LWP_MINSTACKSIZE;
    else
	lwp_MinStackSize =
	    (AFS_LWP_MINSTACKSIZE >
	     atoi(value) ? AFS_LWP_MINSTACKSIZE : atoi(value));

    return LWP_SUCCESS;
}

int
LWP_INTERNALSIGNAL(char *event, int yield)
{				/* signal the occurence of an event */
    Debug(2, ("Entered LWP_SignalProcess"));
    if (lwp_init) {
	int rc;
	rc = Internal_Signal(event);
	if (yield)
	    Set_LWP_RC();
	return rc;
    } else
	return LWP_EINIT;
}

int
LWP_TerminateProcessSupport(void)
{				/* terminate all LWP support */
    register int i;

    Debug(0, ("Entered Terminate_Process_Support"));
    if (lwp_init == NULL)
	return LWP_EINIT;
    if (lwp_cpptr != LWPANCHOR.outerpid)
	Abort_LWP("Terminate_Process_Support invoked from wrong process!");
    for (i = 0; i < MAX_PRIORITIES; i++)
	for_all_elts(cur, runnable[i], {
		     Free_PCB(cur);}
    )
	for_all_elts(cur, blocked, {
		     Free_PCB(cur);}
    )
	for_all_elts(cur, qwaiting, {
		     Free_PCB(cur);}
    )
	free(lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}

int
LWP_WaitProcess(char *event)
{				/* wait on a single event */
    char *tempev[2];

    Debug(2, ("Entered Wait_Process"));
    if (event == NULL)
	return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, tempev);
}

int
LWP_MwaitProcess(int wcount, char *evlist[])
{				/* wait on m of n events */
    register int ecount, i;


    Debug(0, ("Entered Mwait_Process [waitcnt = %d]", wcount));

    if (evlist == NULL) {
	Set_LWP_RC();
	return LWP_EBADCOUNT;
    }

    for (ecount = 0; evlist[ecount] != NULL; ecount++);

    if (ecount == 0) {
	Set_LWP_RC();
	return LWP_EBADCOUNT;
    }

    if (lwp_init) {

	if (wcount > ecount || wcount < 0) {
	    Set_LWP_RC();
	    return LWP_EBADCOUNT;
	}
	if (ecount > lwp_cpptr->eventlistsize) {

	    lwp_cpptr->eventlist =
		(char **)realloc(lwp_cpptr->eventlist,
				 ecount * sizeof(char *));
	    lwp_cpptr->eventlistsize = ecount;
	}
	for (i = 0; i < ecount; i++)
	    lwp_cpptr->eventlist[i] = evlist[i];
	if (wcount > 0) {
	    lwp_cpptr->status = WAITING;

	    move(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);

	}
	lwp_cpptr->wakevent = 0;
	lwp_cpptr->waitcnt = wcount;
	lwp_cpptr->eventcnt = ecount;

	Set_LWP_RC();

	return LWP_SUCCESS;
    }

    return LWP_EINIT;
}

int
LWP_StackUsed(PROCESS pid, int *maxa, int *used)
{
    *maxa = pid->stacksize;
    *used = Stack_Used(pid->stack, *maxa);
    if (*used == 0)
	return LWP_NO_STACK;
    return LWP_SUCCESS;
}

/*
 *  The following functions are strictly
 *  INTERNAL to the LWP support package.
 */

static void
Abort_LWP(char *msg)
{
    struct lwp_context tempcontext;

    Debug(0, ("Entered Abort_LWP"));
    printf("***LWP: %s\n", msg);
    printf("***LWP: Abort --- dumping PCBs ...\n");
#ifdef DEBUG
    Dump_Processes();
#endif
    if (LWPANCHOR.outersp == NULL)
	Exit_LWP();
    else
	savecontext(Exit_LWP, &tempcontext, LWPANCHOR.outersp);
    return;
}

static int
Create_Process_Part2(void)
{				/* creates a context for the new process */
    PROCESS temp;

    Debug(2, ("Entered Create_Process_Part2"));
    temp = lwp_cpptr;		/* Get current process id */
    savecontext(Dispatcher, &temp->context, NULL);
    (*temp->ep) (temp->parm);
    LWP_DestroyProcess(temp);
    return 0;
}

static int
Delete_PCB(register PROCESS pid)
{				/* remove a PCB from the process list */
    Debug(4, ("Entered Delete_PCB"));
    lwp_remove(pid,
	       (pid->blockflag || pid->status == WAITING
		|| pid->status ==
		DESTROYED ? &blocked : 
		(pid->status == QWAITING) ? &qwaiting :
		&runnable[pid->priority]));
    LWPANCHOR.processcnt--;
    return 0;
}

#ifdef DEBUG
static int
Dump_One_Process(PROCESS pid)
{
    int i;

    printf("***LWP: Process Control Block at 0x%x\n", pid);
    printf("***LWP: Name: %s\n", pid->name);
    if (pid->ep != NULL)
	printf("***LWP: Initial entry point: 0x%x\n", pid->ep);
    if (pid->blockflag)
	printf("BLOCKED and ");
    switch (pid->status) {
    case READY:
	printf("READY");
	break;
    case WAITING:
	printf("WAITING");
	break;
    case DESTROYED:
	printf("DESTROYED");
	break;
    case QWAITING:
	printf("QWAITING");
	break;
    default:
	printf("unknown");
    }
    putchar('\n');
    printf("***LWP: Priority: %d \tInitial parameter: 0x%x\n", pid->priority,
	   pid->parm);
    if (pid->stacksize != 0) {
	printf("***LWP:  Stacksize: %d \tStack base address: 0x%x\n",
	       pid->stacksize, pid->stack);
	printf("***LWP: HWM stack usage: ");
	printf("%d\n", Stack_Used(pid->stack, pid->stacksize));
    }
    printf("***LWP: Current Stack Pointer: 0x%x\n", pid->context.topstack);
    if (pid->eventcnt > 0) {
	printf("***LWP: Number of events outstanding: %d\n", pid->waitcnt);
	printf("***LWP: Event id list:");
	for (i = 0; i < pid->eventcnt; i++)
	    printf(" 0x%x", pid->eventlist[i]);
	putchar('\n');
    }
    if (pid->wakevent > 0)
	printf("***LWP: Number of last wakeup event: %d\n", pid->wakevent);
    return 0;
}
#endif

static int
purge_dead_pcbs(void)
{
    for_all_elts(cur, blocked, {
		 if (cur->status == DESTROYED) Dispose_of_Dead_PCB(cur);}
    )
	return 0;
}

int LWP_TraceProcesses = 0;

static int
Dispatcher(void)
{				/* Lightweight process dispatcher */
    register int i;
#ifdef DEBUG
    static int dispatch_count = 0;

    if (LWP_TraceProcesses > 0) {
	for (i = 0; i < MAX_PRIORITIES; i++) {
	    printf("[Priority %d, runnable (%d):", i, runnable[i].count);
	    for_all_elts(p, runnable[i], {
			 printf(" \"%s\"", p->name);
			 }
	    )
		puts("]");
	}
	printf("[Blocked (%d):", blocked.count);
	for_all_elts(p, blocked, {
		     printf(" \"%s\"", p->name);
		     }
	)
	puts("]");
	printf("[Qwaiting (%d):", qwaiting.count);
	for_all_elts(p, qwaiting, {
		     printf(" \"%s\"", p->name);
		     }
	)
	puts("]");
    }
#endif

    /* Check for stack overflowif this lwp has a stack.  Check for
     * the guard word at the front of the stack being damaged and
     * for the stack pointer being below the front of the stack.
     * WARNING!  This code assumes that stacks grow downward. */
#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
    /* Fix this (stackcheck at other end of stack?) */
    if (lwp_cpptr != NULL && lwp_cpptr->stack != NULL
	&& (lwp_cpptr->stackcheck !=
	    *(afs_int32 *) ((lwp_cpptr->stack) + lwp_cpptr->stacksize - 4)
	    || lwp_cpptr->context.topstack >
	    lwp_cpptr->stack + lwp_cpptr->stacksize - 4)) {
#else
    if (lwp_cpptr && lwp_cpptr->stack
	&& (lwp_cpptr->stackcheck != *(int *)(lwp_cpptr->stack)
	    || lwp_cpptr->context.topstack < lwp_cpptr->stack
	    || lwp_cpptr->context.topstack >
	    (lwp_cpptr->stack + lwp_cpptr->stacksize))) {
#endif
	printf("stackcheck = %u: stack = %u \n", lwp_cpptr->stackcheck,
	       *(int *)lwp_cpptr->stack);
	printf("topstack = 0x%x: stackptr = 0x%x: stacksize = 0x%x\n",
	       lwp_cpptr->context.topstack, lwp_cpptr->stack,
	       lwp_cpptr->stacksize);

	switch (lwp_overflowAction) {
	case LWP_SOQUIET:
	    break;
	case LWP_SOABORT:
	    Overflow_Complain();
	    abort();
	case LWP_SOMESSAGE:
	default:
	    Overflow_Complain();
	    lwp_overflowAction = LWP_SOQUIET;
	    break;
	}
    }

    /* Move head of current runnable queue forward if current LWP is still in it. */
    if (lwp_cpptr != NULL && lwp_cpptr == runnable[lwp_cpptr->priority].head)
	runnable[lwp_cpptr->priority].head =
	    runnable[lwp_cpptr->priority].head->next;
    /* Find highest priority with runnable processes. */
    for (i = MAX_PRIORITIES - 1; i >= 0; i--)
	if (runnable[i].head != NULL)
	    break;

    if (i < 0)
	Abort_LWP("No READY processes");

#ifdef DEBUG
    if (LWP_TraceProcesses > 0)
	printf("Dispatch %d [PCB at 0x%x] \"%s\"\n", ++dispatch_count,
	       runnable[i].head, runnable[i].head->name);
#endif
    if (PRE_Block != 1)
	Abort_LWP("PRE_Block not 1");
    lwp_cpptr = runnable[i].head;

    returnto(&lwp_cpptr->context);
}

/* Complain of a stack overflow to stderr without using stdio. */
static void
Overflow_Complain(void)
{
    time_t currenttime;
    char *timeStamp;
    char *msg1 = " LWP: stack overflow in process ";
    char *msg2 = "!\n";

    currenttime = time(0);
    timeStamp = ctime(&currenttime);
    timeStamp[24] = 0;
    write(2, timeStamp, strlen(timeStamp));

    write(2, msg1, strlen(msg1));
    write(2, lwp_cpptr->name, strlen(lwp_cpptr->name));
    write(2, msg2, strlen(msg2));
}

static void
Dispose_of_Dead_PCB(PROCESS cur)
{
    Debug(4, ("Entered Dispose_of_Dead_PCB"));
    Delete_PCB(cur);
    Free_PCB(cur);
/*
    Internal_Signal(cur);
*/
}

static int
Exit_LWP(void)
{
    abort();
}

static void
Free_PCB(PROCESS pid)
{
    Debug(4, ("Entered Free_PCB"));
    if (pid->stack != NULL) {
	Debug(0,
	      ("HWM stack usage: %d, [PCB at 0x%x]",
	       Stack_Used(pid->stack, pid->stacksize), pid));
#ifndef AFS_AIX32_ENV
	free(pid->stack);
#endif
    }
    if (pid->eventlist != NULL)
	free(pid->eventlist);
    free(pid);
}

static void
Initialize_PCB(PROCESS temp, int priority, char *stack, int stacksize,
	       int (*ep) (), void *parm, char *name)
{
    register int i = 0;

    Debug(4, ("Entered Initialize_PCB"));
    if (name != NULL)
	while (((temp->name[i] = name[i]) != '\0') && (i < 31))
	    i++;
    temp->name[31] = '\0';
    temp->status = READY;
    temp->eventlist = (char **)malloc(EVINITSIZE * sizeof(char *));
    temp->eventlistsize = EVINITSIZE;
    temp->eventcnt = 0;
    temp->wakevent = 0;
    temp->waitcnt = 0;
    temp->blockflag = 0;
    temp->iomgrRequest = 0;
    temp->priority = priority;
    temp->index = lwp_nextindex++;
    temp->stack = stack;
    temp->stacksize = stacksize;
#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
    if (temp->stack != NULL)
	temp->stackcheck = *(int *)((temp->stack) + stacksize - 4);
#else
    if (temp->stack != NULL)
	temp->stackcheck = *(int *)(temp->stack);
#endif
    temp->ep = ep;
    temp->parm = parm;
    temp->misc = NULL;		/* currently unused */
    temp->next = NULL;
    temp->prev = NULL;
    temp->lwp_rused = 0;
    temp->level = 1;		/* non-preemptable */
}

static int
Internal_Signal(register char *event)
{
    int rc = LWP_ENOWAIT;
    register int i;

    Debug(0, ("Entered Internal_Signal [event id 0x%x]", event));
    if (!lwp_init)
	return LWP_EINIT;
    if (event == NULL)
	return LWP_EBADEVENT;
    for_all_elts(temp, blocked, {
		 if (temp->status == WAITING)
		 for (i = 0; i < temp->eventcnt; i++) {
		 if (temp->eventlist[i] == event) {
		 temp->eventlist[i] = NULL; rc = LWP_SUCCESS;
		 Debug(0, ("Signal satisfied for PCB 0x%x", temp));
		 if (--temp->waitcnt == 0) {
		 temp->status = READY; temp->wakevent = i + 1;
		 move(temp, &blocked, &runnable[temp->priority]); break;}
		 }
		 }
		 }
    )
	return rc;
}

/* This can be any unlikely pattern except 0x00010203 or the reverse. */
#define STACKMAGIC	0xBADBADBA
static afs_int32
Initialize_Stack(char *stackptr, int stacksize)
{
    register int i;

    Debug(4, ("Entered Initialize_Stack"));
    if (lwp_stackUseEnabled)
	for (i = 0; i < stacksize; i++)
	    stackptr[i] = i & 0xff;
    else
#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
	*(afs_int32 *) (stackptr + stacksize - 4) = STACKMAGIC;
#else
	*(afs_int32 *) stackptr = STACKMAGIC;
#endif
    return 0;
}

static int
Stack_Used(register char *stackptr, int stacksize)
{
    register int i;

#if defined(__hp9000s800) || defined(AFS_PARISC_LINUX24_ENV)
    if (*(afs_int32 *) (stackptr + stacksize - 4) == STACKMAGIC)
	return 0;
    else {
	for (i = stacksize - 1; i >= 0; i--)
	    if ((unsigned char)stackptr[i] != (i & 0xff))
		return (i);
	return 0;
    }
#else
    if (*(afs_int32 *) stackptr == STACKMAGIC)
	return 0;
    else {
	for (i = 0; i < stacksize; i++)
	    if ((unsigned char)stackptr[i] != (i & 0xff))
		return (stacksize - i);
	return 0;
    }
#endif
}


int
LWP_NewRock(int Tag, char *Value)
    /* Finds a free rock and sets its value to Value.
     * Return codes:
     * LWP_SUCCESS      Rock did not exist and a new one was used
     * LWP_EBADROCK     Rock already exists.
     * LWP_ENOROCKS     All rocks are in use.
     * 
     * From the above semantics, you can only set a rock value once.  This is specifically
     * to prevent multiple users of the LWP package from accidentally using the same Tag
     * value and clobbering others.  You can always use one level of indirection to obtain
     * a rock whose contents can change.
     */
{
    register int i;
    register struct rock *ra;	/* rock array */

    ra = lwp_cpptr->lwp_rlist;

    for (i = 0; i < lwp_cpptr->lwp_rused; i++)
	if (ra[i].tag == Tag)
	    return (LWP_EBADROCK);

    if (lwp_cpptr->lwp_rused < MAXROCKS) {
	ra[lwp_cpptr->lwp_rused].tag = Tag;
	ra[lwp_cpptr->lwp_rused].value = Value;
	lwp_cpptr->lwp_rused++;
	return (LWP_SUCCESS);
    } else
	return (LWP_ENOROCKS);
}


int
LWP_GetRock(int Tag, char **Value)
    /* Obtains the pointer Value associated with the rock Tag of this LWP.
     * Returns:
     * LWP_SUCCESS              if specified rock exists and Value has been filled
     * LWP_EBADROCK     rock specified does not exist
     */
{
    register int i;
    register struct rock *ra;

    ra = lwp_cpptr->lwp_rlist;

    for (i = 0; i < lwp_cpptr->lwp_rused; i++)
	if (ra[i].tag == Tag) {
	    *Value = ra[i].value;
	    return (LWP_SUCCESS);
	}
    return (LWP_EBADROCK);
}


#ifdef	AFS_AIX32_ENV
int
setlim(int limcon, uchar_t hard, int limit)
{
    struct rlimit rlim;

    (void)getrlimit(limcon, &rlim);

    limit = limit * 1024;
    if (hard)
	rlim.rlim_max = limit;
    else if (limit == RLIM_INFINITY && geteuid() != 0)
	rlim.rlim_cur = rlim.rlim_max;
    else
	rlim.rlim_cur = limit;

    /* Must use ulimit() due to Posix constraints */
    if (limcon == RLIMIT_FSIZE) {
	if (ulimit
	    (UL_SETFSIZE,
	     ((hard ? rlim.rlim_max : rlim.rlim_cur) / 512)) < 0) {
	    printf("Can't %s%s limit\n",
		   limit == RLIM_INFINITY ? "remove" : "set",
		   hard ? " hard" : "");
	    return (-1);
	}
    } else {
	if (setrlimit(limcon, &rlim) < 0) {
	    perror("");
	    printf("Can't %s%s limit\n",
		   limit == RLIM_INFINITY ? "remove" : "set",
		   hard ? " hard" : "");
	    return (-1);
	}
    }
    return (0);
}


#ifdef	notdef
/*
 * Print the specific limit out
 */
int
plim(char *name, afs_int32 lc, uchar_t hard)
{
    struct rlimit rlim;
    int lim;

    printf("%s \t", name);
    (void)getrlimit(lc, &rlim);
    lim = hard ? rlim.rlim_max : rlim.rlim_cur;
    if (lim == RLIM_INFINITY)
	printf("unlimited");
    printf("%d %s", lim / 1024, "kbytes");
    printf("\n");
}
#endif
#endif

#ifdef	AFS_SUN5_ENV
int
LWP_NoYieldSignal(char *event)
{
    return (LWP_INTERNALSIGNAL(event, 0));
}

int
LWP_SignalProcess(char *event)
{
    return (LWP_INTERNALSIGNAL(event, 1));
}

#endif
#else
#ifdef	USE_SOLARIS_THREADS
#include <thread.h>
#else
#include "pthread.h"
#endif
#include <stdio.h>
#include <assert.h>

pthread_mutex_t lwp_mutex;	/* Mutex to ensure mutual exclusion of all LWP threads */

PROCESS lwp_process_list;	/* List of LWP initiated threads */

pthread_key_t lwp_process_key;	/* Key associating lwp pid with thread */

#define CHECK check(__LINE__);

typedef struct event {
    struct event *next;		/* next in hash chain */
    char *event;		/* lwp event: an address */
    int refcount;		/* Is it in use? */
    pthread_cond_t cond;	/* Currently associated condition variable */
    int seq;			/* Sequence number: this is incremented
				 * by wakeup calls; wait will not return until
				 * it changes */
} event_t;

#define HASHSIZE 127
event_t *hashtable[HASHSIZE];	/* Hash table for events */
#define hash(event)	((unsigned long) (event) % HASHSIZE);

#if CMA_DEBUG || DEBUGF
char *
lwp_process_string(void)
{
    static char id[200];
    PROCESS p;
    LWP_CurrentProcess(&p);
    sprintf(id, "PID %x <%s>", p, p->name);
    return id;
}
#endif

void
lwp_unimplemented(char *interface)
{
    fprintf(stderr,
	    "cmalwp: %s is not currently implemented: program aborted\n",
	    interface);
    exit(1);
}

static void
lwpabort(char *interface)
{
    fprintf(stderr, "cmalwp: %s failed unexpectedly\n", interface);
    abort();
}

int
LWP_QWait(void)
{
    lwp_unimplemented("LWP_QWait");
}

int
LWP_QSignal(int pid)
{
    lwp_unimplemented("LWP_QSignal");
}

/* Allocate and initialize an LWP process handle. The associated pthread handle
 * must be added by the caller, and the structure threaded onto the LWP active
 * process list by lwp_thread_process */
static PROCESS
lwp_alloc_process(char *name, pthread_startroutine_t ep, pthread_addr_t arg)
{
    PROCESS lp;
    assert(lp = (PROCESS) malloc(sizeof(*lp)));
    memset((char *)lp, 0, sizeof(*lp));
    if (!name) {
	char temp[100];
	static procnum;
	sprintf(temp, "unnamed_process_%04d", ++procnum);
	assert(name = (char *)malloc(strlen(temp) + 1));
	strcpy(name, temp);
    }
    lp->name = name;
    lp->ep = ep;
    lp->arg = arg;
    return lp;
}

/* Thread the LWP process descriptor *lp onto the lwp active process list
 * and associate a back pointer to the process descriptor from the associated
 * thread */
static
lwp_thread_process(PROCESS lp)
{
    lp->next = lwp_process_list;
    lwp_process_list = lp;
    assert(!pthread_setspecific(lwp_process_key, (pthread_addr_t) lp));
}

/* The top-level routine used as entry point to explicitly created LWP
 * processes. This completes a few details of process creation left
 * out by LWP_CreateProcess and calls the user-specified entry point */
static int
lwp_top_level(pthread_addr_t argp)
{
    PROCESS lp = (PROCESS) argp;

    assert(!pthread_mutex_lock(&lwp_mutex));
    lwp_thread_process(lp);
    (lp->ep) (lp->arg);
    assert(!pthread_mutex_unlock(&lwp_mutex));
    /* Should cleanup state */
}

int
LWP_CreateProcess(pthread_startroutine_t ep, int stacksize, int priority,
		  void *parm, char *name, PROCESS * pid)
{
    int status;
    pthread_attr_t attr;
    pthread_t handle;
    PROCESS lp;

#ifndef LWP_NO_PRIORITIES
    if (!cmalwp_pri_inrange(priority))
	return LWP_EBADPRI;
#endif
    assert(!pthread_attr_create(&attr));
    assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
    if (stacksize)
	assert(!pthread_attr_setstacksize(&attr, stacksize));
#ifndef BDE_THREADS
    (void)pthread_attr_setinheritsched(&attr, PTHREAD_DEFAULT_SCHED);
    (void)pthread_attr_setsched(&attr, SCHED_FIFO);
#ifndef LWP_NO_PRIORITIES
    (void)pthread_attr_setprio(&attr, cmalwp_lwppri_to_cmapri(priority));
#endif
#endif
    lp = lwp_alloc_process(name, (pthread_startroutine_t) ep,
			   (pthread_addr_t) parm);

    /* allow new thread to run if higher priority */
    assert(!pthread_mutex_unlock(&lwp_mutex));
    /* process is only added to active list after first time it runs (it adds itself) */
    status =
	pthread_create(&lp->handle, attr,
		       (pthread_startroutine_t) lwp_top_level,
		       (pthread_addr_t) lp);
    assert(!pthread_attr_delete(&attr));
    assert(!pthread_mutex_lock(&lwp_mutex));
    if (status != 0) {
	free(lp);
	return LWP_ENOMEM;
    }
    *pid = lp;
    return LWP_SUCCESS;
}

PROCESS
LWP_ActiveProcess(void)
{				/* returns pid of current process */
    PROCESS pid;
    assert(!pthread_getspecific(lwp_process_key, (pthread_addr_t *) & pid));
    return pid;
}

int
LWP_CurrentProcess(PROCESS * pid)
{				/* get pid of current process */
    assert(!pthread_getspecific(lwp_process_key, (pthread_addr_t *) pid));
    return LWP_SUCCESS;
}

int
LWP_DestroyProcess(PROCESS pid)
{				/* destroy a lightweight process */
    lwp_unimplemented("LWP_DestroyProcess");
}

int
LWP_DispatchProcess(void)
{				/* explicit voluntary preemption */
    assert(!pthread_mutex_unlock(&lwp_mutex));
    pthread_yield();
    assert(!pthread_mutex_lock(&lwp_mutex));
    return LWP_SUCCESS;
}

static int
lwp_process_key_destructor(void)
{
}

int
LWP_InitializeProcessSupport(int priority, PROCESS * pid)
{
    static int initialized = 0;
    int status;
    static PROCESS lp;
    extern main;
    int state;

    if (initialized) {
	*pid = lp;
	return LWP_SUCCESS;
    }
#ifndef LWP_NO_PRIORITIES
    if (priority < 0 || priority > LWP_MAX_PRIORITY)
	return LWP_EBADPRI;
#endif

    /* Create pthread key to associate LWP process descriptor with each
     * LWP-created thread */
    assert(!pthread_keycreate(&lwp_process_key, (pthread_destructor_t)
			      lwp_process_key_destructor));

    lp = lwp_alloc_process("main process", main, 0);
    lp->handle = pthread_self();
    lwp_thread_process(lp);
#ifndef LWP_NO_PRIORITIES
    (void)pthread_setscheduler(pthread_self(), SCHED_FIFO,
			       cmalwp_lwppri_to_cmapri(priority));

#endif
    assert(pthread_mutex_init(&lwp_mutex, MUTEX_FAST_NP) == 0);
    assert(pthread_mutex_lock(&lwp_mutex) == 0);
    initialized = 1;
    *pid = lp;
    return LWP_SUCCESS;
}

int
LWP_TerminateProcessSupport(void)
{				/* terminate all LWP support */
    lwp_unimplemented("LWP_TerminateProcessSupport");
}

/* Get and initialize event structure corresponding to lwp event (i.e. address) */
static event_t *
getevent(char *event)
{
    event_t *evp, *newp;
    int hashcode;

    hashcode = hash(event);
    evp = hashtable[hashcode];
    newp = 0;
    while (evp) {
	if (evp->event == event) {
	    evp->refcount++;
	    return evp;
	}
	if (evp->refcount == 0)
	    newp = evp;
	evp = evp->next;
    }
    if (!newp) {
	newp = (event_t *) malloc(sizeof(event_t));
	assert(newp);
	newp->next = hashtable[hashcode];
	hashtable[hashcode] = newp;
	assert(!pthread_cond_init(&newp->cond, ((pthread_condattr_t) 0)));
	newp->seq = 0;
    }
    newp->event = event;
    newp->refcount = 1;
    return newp;
}

/* Release the specified event */
#define relevent(evp) ((evp)->refcount--)

int
LWP_WaitProcess(char *event)
{				/* wait on a single event */
    struct event *ev;
    int seq;
    debugf(("%s: wait process (%x)\n", lwp_process_string(), event));
    if (event == NULL)
	return LWP_EBADEVENT;
    ev = getevent(event);
    seq = ev->seq;
    while (seq == ev->seq) {
	assert(pthread_cond_wait(&ev->cond, &lwp_mutex) == 0);
    }
    debugf(("%s: Woken up (%x)\n", lwp_process_string(), event));
    relevent(ev);
    return LWP_SUCCESS;
}

int
LWP_MwaitProcess(int wcount, char *evlist[])
{				/* wait on m of n events */
    lwp_unimplemented("LWP_MWaitProcess");
}

int
LWP_NoYieldSignal(char *event)
{
    struct event *ev;
    debugf(("%s: no yield signal (%x)\n", lwp_process_string(), event));
    if (event == NULL)
	return LWP_EBADEVENT;
    ev = getevent(event);
    if (ev->refcount > 1) {
	ev->seq++;
	assert(pthread_cond_broadcast(&ev->cond) == 0);
    }
    relevent(ev);
    return LWP_SUCCESS;
}

int
LWP_SignalProcess(char *event)
{
    struct event *ev;
    debugf(("%s: signal process (%x)\n", lwp_process_string(), event));
    if (event == NULL)
	return LWP_EBADEVENT;
    ev = getevent(event);
    if (ev->refcount > 1) {
	ev->seq++;
	assert(!pthread_mutex_unlock(&lwp_mutex));
	assert(!pthread_cond_broadcast(&ev->cond));
	pthread_yield();
	assert(!pthread_mutex_lock(&lwp_mutex));
    }
    relevent(ev);
    return LWP_SUCCESS;
}

int
LWP_StackUsed(PROCESS pid, int *maxa, int *used)
{
    lwp_unimplemented("LWP_StackUsed");
}

int
LWP_NewRock(int Tag, char *Value)
{
    lwp_unimplemented("LWP_NewRock");
}

int
LWP_GetRock(int Tag, char **Value)
{
    lwp_unimplemented("LWP_GetRock");
}

int
LWP_GetProcessPriority(PROCESS pid, int *priority)
{				/* returns process priority */
    lwp_unimplemented("LWP_GetProcessPriority");
}

#endif /* USE_PTHREADS */
