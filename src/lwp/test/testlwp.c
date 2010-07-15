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


/* allocate externs here */
#include <afsconfig.h>
#include <afs/param.h>


#define LWP_KERNEL
#include "lwp.h"

#define  NULL	    0

#define  ON	    1
#define  OFF	    0
#define  TRUE	    1
#define  FALSE	    0
#define  READY	    (1<<1)
#define  WAITING    (1<<2)
#define  DESTROYED  (1<<3)
#define  MAXINT     (~(1<<((sizeof(int)*8)-1)))
#define  MINSTACK   44

/* Debugging macro */
#ifdef DEBUG
#define Debug(level, msg)\
	 if (lwp_debug && lwp_debug >= level) {\
	     printf("***LWP (0x%x): ", lwp_cpptr);\
	     printf msg;\
	     putchar('\n');\
	 }

#else
#define Debug(level, msg)

#endif

int	 Dispatcher();
int	 Create_Process_Part2();
int	 Exit_LWP();
int Initialize_Stack(), Stack_Used();
char   (*RC_to_ASCII());

#define MAX_PRIORITIES	(LWP_MAX_PRIORITY+1)

struct QUEUE {
    PROCESS	head;
    int		count;
} runnable[MAX_PRIORITIES], blocked;

/* Offset of stack field within pcb -- used by stack checking stuff */
int stack_offset;

static remove(p, q)
    PROCESS p;
    struct QUEUE *q;
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

static insert(p, q)
    PROCESS p;
    struct QUEUE *q;
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

static move(p, from, to)
    PROCESS p;
    struct QUEUE *from, *to;
{
    remove(p, from);
    insert(p, to);
}

/* Iterator macro */
#define for_all_elts(var, q, body)\
	{\
	    PROCESS var, _NEXT_;\
	    int _I_;\
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

#define Set_LWP_RC(dummy) savecontext(Dispatcher, &lwp_cpptr->context, NULL)

static struct lwp_ctl *lwp_init;

int LWP_CreateProcess(ep, stacksize, priority, parm, name, pid)
   int   (*ep)();
   int   stacksize, priority;
   char  *parm;
   char  *name;
   PROCESS *pid;
{
    PROCESS temp, temp2;
    char *stackptr;

    Debug(0, ("Entered LWP_CreateProcess"))
    /* Throw away all dead process control blocks */
    purge_dead_pcbs();
    if (lwp_init) {
	temp = (PROCESS) malloc (sizeof (struct lwp_pcb));
	if (temp == NULL) {
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	if (stacksize < MINSTACK)
	    stacksize = 1000;
	else
	    stacksize = 4 * ((stacksize+3) / 4);
	if ((stackptr = (char *) malloc(stacksize)) == NULL) {
	    Set_LWP_RC();
	    return LWP_ENOMEM;
	}
	if (priority < 0 || priority >= MAX_PRIORITIES) {
	    Set_LWP_RC();
	    return LWP_EBADPRI;
	}
#ifdef DEBUG
#ifdef STACK_USED
 	Initialize_Stack(stackptr, stacksize);
#endif
#endif
	Initialize_PCB(temp, priority, stackptr, stacksize, ep, parm, name);
	insert(temp, &runnable[priority]);
	temp2 = lwp_cpptr;
	lwp_cpptr = temp;
	savecontext(Create_Process_Part2, &temp2->context, stackptr+stacksize-4);
	Set_LWP_RC();
	*pid = temp;
	return 0;
    } else
	return LWP_EINIT;
}

int LWP_CurrentProcess(pid)	/* returns pid of current process */
    PROCESS *pid;
{
    Debug(0, ("Entered Current_Process"))
    if (lwp_init) {
	    *pid = lwp_cpptr;
	    return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

#define LWPANCHOR (*lwp_init)

int LWP_DestroyProcess(pid)		/* destroy a lightweight process */
    PROCESS pid;
{
    PROCESS temp;

    Debug(0, ("Entered Destroy_Process"))
    if (lwp_init) {
	if (lwp_cpptr != pid) {
	    Dispose_of_Dead_PCB(pid);
	    Set_LWP_RC();
	} else {
	    pid -> status = DESTROYED;
	    move(pid, &runnable[pid->priority], &blocked);
	    temp = lwp_cpptr;
	    savecontext(Dispatcher, &(temp -> context),
			&(LWPANCHOR.dsptchstack[(sizeof LWPANCHOR.dsptchstack)-4]));
	}
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

int LWP_DispatchProcess()		/* explicit voluntary preemption */
{
    Debug(2, ("Entered Dispatch_Process"))
    if (lwp_init) {
	Set_LWP_RC();
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

#ifdef DEBUG
Dump_Processes()
{
    if (lwp_init) {
	int i;
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
	*priority = pid -> priority;
	return 0;
    } else
	return LWP_EINIT;
}

int LWP_InitializeProcessSupport(priority, pid)
    int priority;
    PROCESS *pid;
{
    PROCESS temp;
    struct lwp_pcb dummy;
    int i;

    Debug(0, ("Entered LWP_InitializeProcessSupport"))
    if (lwp_init != NULL) return LWP_EINIT;

    /* Set up offset for stack checking -- do this as soon as possible */
    stack_offset = (char *) &dummy.stack - (char *) &dummy;

    if (priority >= MAX_PRIORITIES) return LWP_EBADPRI;
    for (i=0; i<MAX_PRIORITIES; i++) {
	runnable[i].head = NULL;
	runnable[i].count = 0;
    }
    blocked.head = NULL;
    blocked.count = 0;
    lwp_init = (struct lwp_ctl *) malloc(sizeof(struct lwp_ctl));
    temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
    if (lwp_init == NULL || temp == NULL)
	Abort_LWP("Insufficient Storage to Initialize LWP Support");
    LWPANCHOR.processcnt = 1;
    LWPANCHOR.outerpid = temp;
    LWPANCHOR.outersp = NULL;
    Initialize_PCB(temp, priority, NULL, 0, NULL, NULL, "Main Process [created by LWP]");
    insert(temp, &runnable[priority]);
    savecontext(Dispatcher, &temp->context, NULL);
    LWPANCHOR.outersp = temp -> context.topstack;
    Set_LWP_RC();
    *pid = temp;
    return LWP_SUCCESS;
}

int LWP_INTERNALSIGNAL(event, yield)		/* signal the occurence of an event */
    char *event;
    int yield;
{
    Debug(2, ("Entered LWP_SignalProcess"))
    if (lwp_init) {
	int rc;
	rc = Internal_Signal(event);
	if (yield) Set_LWP_RC();
	return rc;
    } else
	return LWP_EINIT;
}

int LWP_TerminateProcessSupport()	/* terminate all LWP support */
{
    int pc;
    int i;

    Debug(0, ("Entered Terminate_Process_Support"))
    if (lwp_init == NULL) return LWP_EINIT;
    if (lwp_cpptr != LWPANCHOR.outerpid)
	Abort_LWP("Terminate_Process_Support invoked from wrong process!");
    pc = LWPANCHOR.processcnt-1;
    for (i=0; i<MAX_PRIORITIES; i++)
	for_all_elts(cur, runnable[i], { Free_PCB(cur); })
    for_all_elts(cur, blocked, { Free_PCB(cur); })
    free(lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}

int LWP_WaitProcess(event)		/* wait on a single event */
    char *event;
{
    char *tempev[2];

    Debug(2, ("Entered Wait_Process"))
    if (event == NULL) return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, tempev);
}

int LWP_MwaitProcess(wcount, evlist, ecount)	/* wait on m of n events */
    int wcount, ecount;
    char *evlist[];
{
    Debug(0, ("Entered Mwait_Process [waitcnt = %d]", wcount))

    if (ecount == 0) {
	Set_LWP_RC();
	return LWP_EBADCOUNT;
    }
    if (lwp_init) {
	if (wcount>ecount || wcount<0) {
	    Set_LWP_RC();
	    return LWP_EBADCOUNT;
	}
	if (ecount > LWP_MAX_EVENTS) {
	    Set_LWP_RC();
	    return LWP_EBADCOUNT;
	}
	if (ecount == 1)
	    lwp_cpptr->eventlist[0] = evlist[0];
	else
	    memcpy(lwp_cpptr->eventlist, evlist, ecount*sizeof(char *));
	if (wcount > 0) {
	    lwp_cpptr -> status = WAITING;
	    move(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);
	}
	lwp_cpptr -> wakevent = 0;
	lwp_cpptr -> waitcnt = wcount;
	lwp_cpptr -> eventcnt = ecount;
	Set_LWP_RC();
	return LWP_SUCCESS;
    }
    return LWP_EINIT;
}

int LWP_StackUsed(pid, max, used)
    PROCESS pid;
    int *max, *used;
{
#define NO_STACK_STUFF
#ifdef DEBUG
#ifdef STACK_USED
#undef NO_STACK_STUFF
#endif
#endif

#ifdef NO_STACK_STUFF
    return LWP_NO_STACK;
#else
    *max = pid -> stacksize;
    *used = Stack_Used(pid->stack, *max);
    return LWP_SUCCESS;
#endif
}

/*
 *  The following functions are strictly
 *  INTERNAL to the LWP support package.
 */

static Abort_LWP(msg)
    char *msg;
{
    struct lwp_context tempcontext;

    Debug(0, ("Entered Abort_LWP"))
    printf("***LWP: %s\n",msg);
    printf("***LWP: Abort --- dumping PCBs ...\n");
#ifdef DEBUG
    Dump_Processes();
#endif
    if (LWPANCHOR.outersp == NULL)
	Exit_LWP();
    else
	savecontext(Exit_LWP, &tempcontext, LWPANCHOR.outersp);
}

static Create_Process_Part2 ()	/* creates a context for the new process */
{
    PROCESS temp;

    Debug(2, ("Entered Create_Process_Part2"))
    temp = lwp_cpptr;		/* Get current process id */
    savecontext(Dispatcher, &temp->context, NULL);
    (*temp->ep)(temp->parm);
    LWP_DestroyProcess(temp);
}

static Delete_PCB(pid) 	/* remove a PCB from the process list */
    PROCESS pid;
{
    Debug(4, ("Entered Delete_PCB"))
    remove(pid, (pid->blockflag || pid->status==WAITING || pid->status==DESTROYED
		 ? &blocked
		 : &runnable[pid->priority]));
    LWPANCHOR.processcnt--;
}

#ifdef DEBUG
static Dump_One_Process(pid)
   PROCESS pid;
{
    int i;

    printf("***LWP: Process Control Block at 0x%x\n", pid);
    printf("***LWP: Name: %s\n", pid->name);
    if (pid->ep != NULL)
	printf("***LWP: Initial entry point: 0x%x\n", pid->ep);
    if (pid->blockflag) printf("BLOCKED and ");
    switch (pid->status) {
	case READY:	printf("READY");     break;
	case WAITING:	printf("WAITING");   break;
	case DESTROYED:	printf("DESTROYED"); break;
	default:	printf("unknown");
    }
    putchar('\n');
    printf("***LWP: Priority: %d \tInitial parameter: 0x%x\n",
	    pid->priority, pid->parm);
    if (pid->stacksize != 0) {
	printf("***LWP:  Stacksize: %d \tStack base address: 0x%x\n",
		pid->stacksize, pid->stack);
	printf("***LWP: HWM stack usage: ");
	printf("%d\n", Stack_Used(pid->stack,pid->stacksize));
	free (pid->stack);
    }
    printf("***LWP: Current Stack Pointer: 0x%x\n", pid->context.topstack);
    if (pid->eventcnt > 0) {
	printf("***LWP: Number of events outstanding: %d\n", pid->waitcnt);
	printf("***LWP: Event id list:");
	for (i=0;i<pid->eventcnt;i++)
	    printf(" 0x%x", pid->eventlist[i]);
	putchar('\n');
    }
    if (pid->wakevent>0)
	printf("***LWP: Number of last wakeup event: %d\n", pid->wakevent);
}
#endif

static purge_dead_pcbs()
{
    for_all_elts(cur, blocked, { if (cur->status == DESTROYED) Dispose_of_Dead_PCB(cur); })
}

int LWP_TraceProcesses = 0;

static Dispatcher()		/* Lightweight process dispatcher */
{
    int i;
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

    for (i=MAX_PRIORITIES-1; i>=0; i--)
	if (runnable[i].head != NULL) break;

    if (i < 0) Abort_LWP("No READY processes");

#ifdef DEBUG
    if (LWP_TraceProcesses > 0)
	printf("Dispatch %d [PCB at 0x%x] \"%s\"\n", ++dispatch_count, runnable[i].head, runnable[i].head->name);
#endif
    lwp_cpptr = runnable[i].head;
    runnable[i].head = runnable[i].head -> next;
    returnto(&lwp_cpptr->context);
}

static Dispose_of_Dead_PCB (cur)
    PROCESS cur;
{
    Debug(4, ("Entered Dispose_of_Dead_PCB"))
    Delete_PCB(cur);
    Free_PCB(cur);
/*
    Internal_Signal(cur);
*/
}

static Exit_LWP()
{
    abort();
}

static Free_PCB(pid)
    PROCESS pid;
{
    Debug(4, ("Entered Free_PCB"))
    if (pid -> stack != NULL) {
	Debug(0, ("HWM stack usage: %d, [PCB at 0x%x]",
		   Stack_Used(pid->stack,pid->stacksize), pid))
	free(pid -> stack);
    }
    free(pid);
}

static Initialize_PCB(temp, priority, stack, stacksize, ep, parm, name)
    PROCESS temp;
    int	(*ep)();
    int	stacksize, priority;
    char *parm;
    char *name,*stack;
{
    int i = 0;

    Debug(4, ("Entered Initialize_PCB"))
    if (name != NULL)
	while (((temp -> name[i] = name[i]) != '\0') && (i < 31)) i++;
    temp -> name[31] = '\0';
    temp -> status = READY;
    temp -> eventcnt = 0;
    temp -> wakevent = 0;
    temp -> waitcnt = 0;
    temp -> blockflag = 0;
    temp -> priority = priority;
    temp -> stack = stack;
    temp -> stacksize = stacksize;
    temp -> ep = ep;
    temp -> parm = parm;
    temp -> misc = NULL;	/* currently unused */
    temp -> next = NULL;
    temp -> prev = NULL;
}

static int Internal_Signal(event)
    char *event;
{
    int rc = LWP_ENOWAIT;
    int i;

    Debug(0, ("Entered Internal_Signal [event id 0x%x]", event))
    if (!lwp_init) return LWP_EINIT;
    if (event == NULL) return LWP_EBADEVENT;
    for_all_elts(temp, blocked, {
	if (temp->status == WAITING)
	    for (i=0; i < temp->eventcnt; i++) {
		if (temp -> eventlist[i] == event) {
		    temp -> eventlist[i] = NULL;
		    rc = LWP_SUCCESS;
		    Debug(0, ("Signal satisfied for PCB 0x%x", temp))
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

#ifdef DEBUG
#ifdef STACK_USED
static Initialize_Stack(stackptr, stacksize)
    char *stackptr;
    int stacksize;
{
    int i;

    Debug(4, ("Entered Initialize_Stack"))
    for (i=0; i<stacksize; i++) stackptr[i] = i & 0xff;
}

static int Stack_Used(stackptr, stacksize)
   char *stackptr;
   int stacksize;
{
    int i;

    for (i=0;i<stacksize;i++)
    if ((unsigned char) stackptr[i] != (i & 0xff))
	return (stacksize-i);
    return 0;
}
#endif
#endif
