/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* For NT, LWP is implemented on top of fibers. The design attempts to
 * follow the current LWP implementation so that we are not using 2 LWP
 * models. The parts of the Unix LWP model that are not actually used in
 * AFS have not been implemented at all. With the exception of preemption,
 * adding those parts to this NT base would be a matter of adding routines.
 *
 * required lib is kernel32.lib, header is winbase.h
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header$");

#ifdef AFS_NT40_ENV
#include <stdio.h>
#include <stdlib.h>
#include <afs/afsutil.h>
#include "lwp.h"


static struct lwp_ctl *lwp_init;
#define LWPANCHOR (*lwp_init)

PROCESS lwp_cpptr;

char lwp_debug = 0;

#define  READY		2
#define  WAITING		3
#define  DESTROYED	4
#define QWAITING		5

/* Debugging macro */
#ifdef DEBUG
#define Debug(level, msg)\
	 if (lwp_debug && lwp_debug >= level) {\
	     printf("***LWP (0x%p): ", lwp_cpptr);\
	     printf msg;\
	     putchar('\n');\
	 }

#else
#define Debug(level, msg)
#endif


/* Forward declarations */
static void Dispatcher(void);
static void Exit_LWP(void);
static void Abort_LWP(char *msg);
static VOID WINAPI Enter_LWP(PVOID fiberData);
static void Initialize_PCB(PROCESS pcb, int priority, int stacksize,
			   int (*funP)(), void *argP, char *name);

static void Dispose_of_Dead_PCB();
static void Free_PCB();
static int Internal_Signal();
static void purge_dead_pcbs(void);

static void lwp_remove(PROCESS p, struct QUEUE *q);
static void insert(PROCESS p, struct QUEUE *q);
static void move(PROCESS p, struct QUEUE *from, struct QUEUE *to);

/* biggest LWP stack created so far - used by IOMGR */
int lwp_MaxStackSeen = 0;

#ifdef DEBUG
static void Dump_One_Process(PROCESS pid);
static void Dump_Processes(void);
#endif

int lwp_nextindex;

#define MAX_PRIORITIES	(LWP_MAX_PRIORITY+1)

/* Invariant for runnable queues: The head of each queue points to the
 * currently running process if it is in that queue, or it points to the
 * next process in that queue that should run.
 */
struct QUEUE {
    PROCESS	head;
    int		count;
} runnable[MAX_PRIORITIES], blocked;

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

#ifdef AFS_WIN95_ENV

LPVOID ConvertThreadToFiber(PROCESS x)
{
	return NULL;
}
LPVOID CreateFiber(DWORD x ,LPVOID y,PROCESS z)
{
	return NULL;
}

VOID SwitchToFiber(LPVOID p)
{
}

VOID DeleteFiber(LPVOID p)
{
}
#endif


int lwp_MinStackSize = 0;

/* LWP_InitializeProcessSupport - setup base support for fibers.
 *
 * Arguments:
 *	priority - priority of main thread.
 *
 * Returns:
 *	pid - return this process
 *	value:
 *	LWP_SUCCESS (else aborts)
 *	
 */

int LWP_InitializeProcessSupport(int priority, PROCESS *pid)
{
    PROCESS pcb;
    register int i;
    char* value;

    Debug(0, ("Entered LWP_InitializeProcessSupport"))
    if (lwp_init != NULL) return LWP_SUCCESS;

    if (priority >= MAX_PRIORITIES) return LWP_EBADPRI;

    pcb = (PROCESS)malloc(sizeof(*pcb));
    if (pcb == NULL)
	Abort_LWP("Insufficient Storage to Initialize LWP PCB");
    (void) memset((void*)pcb, 0, sizeof(*pcb));
    pcb->fiber = ConvertThreadToFiber(pcb);
    if (pcb == NULL) 
	Abort_LWP("Cannot convert main thread to LWP fiber");

    lwp_init = (struct lwp_ctl *) malloc(sizeof(struct lwp_ctl));
    if (lwp_init == NULL)
	Abort_LWP("Insufficient Storage to Initialize LWP CTL");
    (void) memset((void*)lwp_init, 0, sizeof(struct lwp_ctl));

    for (i=0; i<MAX_PRIORITIES; i++) {
	runnable[i].head = NULL;
	runnable[i].count = 0;
    }
    blocked.head = NULL;
    blocked.count = 0;

    LWPANCHOR.processcnt = 1;
    LWPANCHOR.outerpid = pcb;
    LWPANCHOR.outersp = NULL;


    Initialize_PCB(pcb, priority, 0, NULL, NULL,
		"Main Process [created by LWP]");   

    lwp_cpptr = pcb;
    Debug(10, ("Init: Insert 0x%p into runnable at priority %d\n", pcb, priority))
    insert(pcb, &runnable[priority]);

    if ( ( value = getenv("AFS_LWP_STACK_SIZE")) == NULL )
	lwp_MinStackSize = AFS_LWP_MINSTACKSIZE;	
    else
	lwp_MinStackSize = (AFS_LWP_MINSTACKSIZE>atoi(value)?
				AFS_LWP_MINSTACKSIZE : atoi(value));
   
    *pid = pcb;

    return LWP_SUCCESS;
}

/* LWP_CreateProcess - create a new fiber and start executing it.
 *
 * Arguments:
 *	funP - start function
 *	stacksize - size of 
 *	priority - LWP priority
 *	argP - initial parameter for start function
 *	name - name of LWP
 *
 * Return:
 *	pid - handle of created LWP
 *	value:
 *	0 - success
 *	LWP_EINIT - error in intialization.
 *
 */
int LWP_CreateProcess(int (*funP)(), int stacksize, int priority, void *argP,
		      char *name, PROCESS *pid)
{
    PROCESS pcb;
    
    purge_dead_pcbs();

    pcb = (PROCESS)malloc(sizeof(*pcb));
    if (pcb == NULL)
	return LWP_ENOMEM;
    (void) memset((void*)pcb, 0, sizeof(*pcb));

    /*
     * on some systems (e.g. hpux), a minimum usable stack size has
     * been discovered
     */
    if (stacksize < lwp_MinStackSize) {
      stacksize = lwp_MinStackSize;
    }
    /* more stack size computations; keep track of for IOMGR */
    if (lwp_MaxStackSeen < stacksize)
	lwp_MaxStackSeen = stacksize;

    pcb->fiber = CreateFiber(stacksize, Enter_LWP, pcb);
    if (pcb->fiber == NULL) {
	free((void*)pcb);
	return LWP_EINIT;
    }
    Debug(0, ("Create: pcb=0x%p, funP=0x%p, argP=0x%p\n", pcb, funP, argP))
    /* Fiber is now created, so fill in PCB */
    Initialize_PCB(pcb, priority, stacksize, funP, argP, name);
    Debug(10, ("Create: Insert 0x%p into runnable at priority %d\n", pcb, priority))
    insert(pcb, &runnable[priority]);

    LWPANCHOR.processcnt++;

    /* And hand off execution. */
    SwitchToFiber(pcb->fiber);

    *pid = pcb;

    return LWP_SUCCESS;
}

int LWP_DestroyProcess(PROCESS pid)
{
    Debug(0, ("Entered Destroy_Process"))
    if (!lwp_init)
	return LWP_EINIT;

    
    if (lwp_cpptr != pid) {
	Dispose_of_Dead_PCB(pid);
    } else {
	pid->status = DESTROYED;
	move(pid, &runnable[pid->priority], &blocked);
    }

    Dispatcher();

    return LWP_SUCCESS;
}

int LWP_QWait(void)
{
    PROCESS tp;
    (tp=lwp_cpptr) -> status = QWAITING;
    lwp_remove(tp, &runnable[tp->priority]);
    Dispatcher();
    return LWP_SUCCESS;
}

int LWP_QSignal(PROCESS pid)
{
    if (pid->status == QWAITING) {
	pid->status = READY;
	Debug(10, ("QSignal: Insert 0x%p into runnable at priority %d\n", pid, pid->priority))
	insert(pid, &runnable[pid->priority]);
	return LWP_SUCCESS;
    }
    else return LWP_ENOWAIT;
}

int LWP_CurrentProcess(PROCESS *pid)
{
    Debug(0, ("Entered Current_Process"))
    if (lwp_init) {
	    *pid = lwp_cpptr;
	    return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

PROCESS LWP_ThreadId()
{
    Debug(0, ("Entered ThreadId"))
    if (lwp_init)
        return lwp_cpptr;
    else
        return (PROCESS) 0;
}

int LWP_DispatchProcess(void)		/* explicit voluntary preemption */
{
    Debug(2, ("Entered Dispatch_Process"))
    if (lwp_init) {
	Dispatcher();
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}


#ifdef DEBUG
static void Dump_Processes(void)
{
    if (lwp_init) {
	register int i;
	for (i=0; i<MAX_PRIORITIES; i++)
	    for_all_elts(x, runnable[i], {
		printf("[Priority %d]\n", i);
		Dump_One_Process(x);
	    })
	for_all_elts(x, blocked, { Dump_One_Process(x); })
    } else
	printf("***LWP: LWP support not initialized\n");
}
#endif


int LWP_GetProcessPriority(pid, priority)	/* returns process priority */
    PROCESS pid;
    int *priority;
{
    Debug(0, ("Entered Get_Process_Priority"))
    if (lwp_init) {
	*priority = pid->priority;
	return 0;
    } else
	return LWP_EINIT;
}

static int Internal_Signal(char *event)
{
    int rc = LWP_ENOWAIT;
    int i;

    Debug(0, ("Entered Internal_Signal [event id 0x%p]", event))
    if (!lwp_init) return LWP_EINIT;
    if (event == NULL) return LWP_EBADEVENT;
    for_all_elts(temp, blocked, {
	if (temp->status == WAITING)
	    for (i=0; i < temp->eventcnt; i++) {
		if (temp -> eventlist[i] == event) {
		    temp -> eventlist[i] = NULL;
		    rc = LWP_SUCCESS;
		    Debug(0, ("Signal satisfied for PCB 0x%p", temp))
		    if (--temp->waitcnt == 0) {
			temp -> status = READY;
			temp -> wakevent = i+1;
			move(temp, &blocked, &runnable[temp->priority]);
			break;
		    }
		}
	    }
    })
    return rc;
}

/* signal the occurence of an event */
int LWP_INTERNALSIGNAL(void *event, int yield)
{
    Debug(2, ("Entered LWP_SignalProcess"))
    if (lwp_init) {
	int rc;
	rc = Internal_Signal(event);
	if (yield)
	    Dispatcher();
	return rc;
    } else
	return LWP_EINIT;
}

int LWP_TerminateProcessSupport()	/* terminate all LWP support */
{
    register int i;

    Debug(0, ("Entered Terminate_Process_Support"))
    if (lwp_init == NULL) return LWP_EINIT;
    if (lwp_cpptr != LWPANCHOR.outerpid)
	Abort_LWP("Terminate_Process_Support invoked from wrong process!");
    /* Is this safe??? @@@ */
    for (i=0; i<MAX_PRIORITIES; i++)
	for_all_elts(cur, runnable[i], { Free_PCB(cur); })
    for_all_elts(cur, blocked, { Free_PCB(cur); })
    free(lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}

int LWP_MwaitProcess(int wcount, void **evlist)
{
    int ecount, i;

    Debug(0, ("Entered Mwait_Process [waitcnt = %d]", wcount))
    if (evlist == NULL) {
	Dispatcher();
	return LWP_EBADCOUNT;
    }

    for (ecount = 0; evlist[ecount] != NULL; ecount++) ;

    if (ecount == 0) {
	Dispatcher();
	return LWP_EBADCOUNT;
    }

    if (!lwp_init)
	return LWP_EINIT;

    if (wcount>ecount || wcount<0) {
	Dispatcher();
	return LWP_EBADCOUNT;
    }
    if (ecount > lwp_cpptr->eventlistsize) {

	void **save_eventlist = lwp_cpptr->eventlist;
	lwp_cpptr->eventlist = (char **)realloc(lwp_cpptr->eventlist,
						ecount*sizeof(char *));
	if (lwp_cpptr->eventlist == NULL) {
	    lwp_cpptr->eventlist = save_eventlist;
	    Dispatcher();
	    return LWP_ENOMEM;
	}
	lwp_cpptr->eventlistsize = ecount;
    }
    for (i=0; i<ecount; i++) lwp_cpptr -> eventlist[i] = evlist[i];
    if (wcount > 0) {
	lwp_cpptr->status = WAITING;

	move(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);

    }
    lwp_cpptr->wakevent = 0;
    lwp_cpptr->waitcnt = wcount;
    lwp_cpptr->eventcnt = ecount;

    Dispatcher();

    return LWP_SUCCESS;
}

int LWP_WaitProcess(void *event)		/* wait on a single event */
{
    void *tempev[2];

    Debug(2, ("Entered Wait_Process"))
    if (event == NULL) return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, tempev);
}


/* Internal Support Routines */
static void Initialize_PCB(PROCESS pcb, int priority, int stacksize,
			   int (*funP)(), void *argP, char *name)
{
    int i = 0;

    Debug(4, ("Entered Initialize_PCB"))
    if (name != NULL)
	while (((pcb->name[i] = name[i]) != '\0') && (i < 31)) i++;
    pcb->name[31] = '\0';
    pcb->priority = priority;
    pcb->stacksize = stacksize;
    pcb->funP = funP;
    pcb->argP = argP;
    pcb->status = READY;
    pcb->eventlist = (void**)malloc(EVINITSIZE*sizeof(void*));
    pcb->eventlistsize =  pcb->eventlist ? EVINITSIZE : 0;
    pcb->eventcnt = 0;
    pcb->wakevent = 0;
    pcb->waitcnt = 0;
    pcb->next = pcb->prev = (PROCESS)NULL;
    pcb->iomgrRequest = (struct IoRequest*)NULL;
    pcb->index = lwp_nextindex ++;
}


static VOID WINAPI Enter_LWP(PVOID fiberData)
{
    PROCESS pcb = (PROCESS)fiberData;

/* next lines are new..... */
    lwp_cpptr = pcb;

    Debug(2, ("Enter_LWP: pcb=0x%p, funP=0x%p, argP=0x%p\n", pcb, pcb->funP, pcb->argP))

    (*pcb->funP)(pcb->argP);

    LWP_DestroyProcess(pcb);
    Dispatcher();
}

static void Abort_LWP(char *msg)
{
    Debug(0, ("Entered Abort_LWP"))
    printf("***LWP: %s\n",msg);
#ifdef DEBUG
    printf("***LWP: Abort --- dumping PCBs ...\n");
    Dump_Processes();
#endif
    Exit_LWP();
}

#ifdef DEBUG
static void Dump_One_Process(PROCESS pid)
{
    int i;

    printf("***LWP: Process Control Block at 0x%p\n", pid);
    printf("***LWP: Name: %s\n", pid->name);
    if (pid->funP != NULL)
	printf("***LWP: Initial entry point: 0x%p\n", pid->funP);
    switch (pid->status) {
	case READY:	printf("READY");     break;
	case WAITING:	printf("WAITING");   break;
	case DESTROYED:	printf("DESTROYED"); break;
	default:	printf("unknown");
    }
    putchar('\n');
    printf("***LWP: Priority: %d \tInitial parameter: 0x%p\n",
	    pid->priority, pid->argP);
    if (pid->stacksize != 0) {
	printf("***LWP:  Stacksize: %d\n", pid->stacksize);
    }
    if (pid->eventcnt > 0) {
	printf("***LWP: Number of events outstanding: %d\n", pid->waitcnt);
	printf("***LWP: Event id list:");
	for (i=0;i<pid->eventcnt;i++)
	    printf(" 0x%p", pid->eventlist[i]);
	putchar('\n');
    }
    if (pid->wakevent>0)
	printf("***LWP: Number of last wakeup event: %d\n", pid->wakevent);
}
#endif


static void purge_dead_pcbs(void)
{
    for_all_elts(cur, blocked, { if (cur->status == DESTROYED) Dispose_of_Dead_PCB(cur); })
}

int LWP_TraceProcesses = 0;

static void Dispatcher(void)
{
    register int i;
#ifdef DEBUG
    static int dispatch_count = 0;

    if (LWP_TraceProcesses > 0) {
	for (i=0; i<MAX_PRIORITIES; i++) {
	    printf("[Priority %d, runnable (%d):", i, runnable[i].count);
	    for_all_elts(p, runnable[i], {
		printf(" \"%s\"", p->name);
	    })
	    puts("]");
	}
	printf("[Blocked (%d):", blocked.count);
	for_all_elts(p, blocked, {
	    printf(" \"%s\"", p->name);
	})
	puts("]");
    }
#endif
    /* Move head of current runnable queue forward if current LWP is still
     *in it.
     */
    if (lwp_cpptr != NULL && lwp_cpptr == runnable[lwp_cpptr->priority].head)
	runnable[lwp_cpptr->priority].head =
	    runnable[lwp_cpptr->priority].head -> next;
    /* Find highest priority with runnable processes. */
    for (i=MAX_PRIORITIES-1; i>=0; i--)
	if (runnable[i].head != NULL) break;

    if (i < 0) Abort_LWP("No READY processes");

#ifdef DEBUG
    if (LWP_TraceProcesses > 0)
	printf("Dispatch %d [PCB at 0x%p] \"%s\"\n", ++dispatch_count,
	       runnable[i].head, runnable[i].head->name);
#endif

    lwp_cpptr = runnable[i].head;
    SwitchToFiber(lwp_cpptr->fiber);
}

void lwp_abort(void)
{
    afs_NTAbort();
}

static void Exit_LWP(void)
{

    lwp_abort();
}

static void Delete_PCB(PROCESS pid)
{
    Debug(4, ("Entered Delete_PCB"))
    lwp_remove(pid, (pid->status==WAITING || pid->status==DESTROYED
		     ? &blocked
		     : &runnable[pid->priority]));
    LWPANCHOR.processcnt--;
}


static void Free_PCB(PROCESS pid)
{
    Debug(4, ("Entered Free_PCB"))
    if (pid->fiber != NULL) {
	DeleteFiber(pid->fiber);
    }
    if (pid->eventlist != NULL)  free((void*)pid->eventlist);
    free((void*)pid);
}

static void Dispose_of_Dead_PCB(PROCESS cur)
{
    Debug(4, ("Entered Dispose_of_Dead_PCB"))
    Delete_PCB(cur);
    Free_PCB(cur);
}

/* Queue manipulation. */
static void lwp_remove(PROCESS p, struct QUEUE *q)
{
    /* Special test for only element on queue */
    if (q->count == 1)
	q -> head = NULL;
    else {
	/* Not only element, do normal remove */
	p -> next -> prev = p -> prev;
	p -> prev -> next = p -> next;
    }
    /* See if head pointing to this element */
    if (q->head == p) q -> head = p -> next;
    q->count--;
    p -> next = p -> prev = NULL;
}

static void insert(PROCESS p, struct QUEUE *q)
{
    if (q->head == NULL) {	/* Queue is empty */
	q -> head = p;
	p -> next = p -> prev = p;
    } else {			/* Regular insert */
	p -> prev = q -> head -> prev;
	q -> head -> prev -> next = p;
	q -> head -> prev = p;
	p -> next = q -> head;
    }
    q->count++;
}

static void move(PROCESS p, struct QUEUE *from, struct QUEUE *to)
{
    lwp_remove(p, from);
    insert(p, to);
}
#endif /* AFS_NT40_ENV */
