/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "net/if.h"
#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif


#if defined(AFS_SUN5_ENV) || defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_HPUX_ENV)
#define	AFS_MINBUFFERS	100
#else
#define	AFS_MINBUFFERS	50
#endif

struct afsop_cell {
    afs_int32 hosts[MAXCELLHOSTS];
    char cellName[100];
};

char afs_zeros[AFS_ZEROS];
char afs_rootVolumeName[64] = "";
afs_uint32 rx_bindhost;

afs_int32 afs_initState = 0;
afs_int32 afs_termState = 0;
afs_int32 afs_setTime = 0;
int afs_cold_shutdown = 0;
char afs_SynchronousCloses = '\0';
static int afs_CB_Running = 0;
static int AFS_Running = 0;
static int afs_CacheInit_Done = 0;
static int afs_Go_Done = 0;
extern struct interfaceAddr afs_cb_interface;
static int afs_RX_Running = 0;
static int afs_InitSetup_done = 0;

afs_int32 afs_rx_deadtime = AFS_RXDEADTIME;
afs_int32 afs_rx_harddead = AFS_HARDDEADTIME;

static int afscall_set_rxpck_received = 0;

#if defined(AFS_HPUX_ENV)
extern int afs_vfs_mount();
#endif /* defined(AFS_HPUX_ENV) */

/* This is code which needs to be called once when the first daemon enters
 * the client. A non-zero return means an error and AFS should not start.
 */
static int
afs_InitSetup(int preallocs)
{
    extern void afs_InitStats();
    int code;

    if (afs_InitSetup_done)
	return EAGAIN;

#ifndef AFS_NOSTATS
    /*
     * Set up all the AFS statistics variables.  This should be done
     * exactly once, and it should be done here, the first resource-setting
     * routine to be called by the CM/RX.
     */
    afs_InitStats();
#endif /* AFS_NOSTATS */

    memset(afs_zeros, 0, AFS_ZEROS);

    /* start RX */
    if(!afscall_set_rxpck_received)
    rx_extraPackets = AFS_NRXPACKETS;	/* smaller # of packets */
    code = rx_InitHost(rx_bindhost, htons(7001));
    if (code) {
	printf("AFS: RX failed to initialize %d).\n", code);
	return code;
    }
    rx_SetRxDeadTime(afs_rx_deadtime);
    /* resource init creates the services */
    afs_ResourceInit(preallocs);

    afs_InitSetup_done = 1;
    afs_osi_Wakeup(&afs_InitSetup_done);

    return code;
}
#if defined(AFS_DARWIN80_ENV)
struct afsd_thread_info {
    unsigned long parm;
};
static int
afsd_thread(int *rock)
{
    struct afsd_thread_info *arg = (struct afsd_thread_info *)rock;
    unsigned long parm = arg->parm;

    switch (parm) {
    case AFSOP_START_RXCALLBACK:
	AFS_GLOCK();
	wakeup(arg);
	afs_CB_Running = 1;
	while (afs_RX_Running != 2)
	    afs_osi_Sleep(&afs_RX_Running);
	afs_RXCallBackServer();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_START_AFS:
	AFS_GLOCK();
	wakeup(arg);
	AFS_Running = 1;
	while (afs_initState < AFSOP_START_AFS)
	    afs_osi_Sleep(&afs_initState);
	afs_initState = AFSOP_START_BKG;
	afs_osi_Wakeup(&afs_initState);
	afs_Daemon();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_START_BKG:
	AFS_GLOCK();
	wakeup(arg);
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	if (afs_initState < AFSOP_GO) {
	    afs_initState = AFSOP_GO;
	    afs_osi_Wakeup(&afs_initState);
	}
	afs_BackgroundDaemon();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_START_TRUNCDAEMON:
	AFS_GLOCK();
	wakeup(arg);
	while (afs_initState < AFSOP_GO)
	    afs_osi_Sleep(&afs_initState);
	afs_CacheTruncateDaemon();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_START_CS:
	AFS_GLOCK();
	wakeup(arg);
	afs_CheckServerDaemon();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_RXEVENT_DAEMON:
	AFS_GLOCK();
	wakeup(arg);
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	afs_rxevent_daemon();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    case AFSOP_RXLISTENER_DAEMON:
	AFS_GLOCK();
	wakeup(arg);
	afs_initState = AFSOP_START_AFS;
	afs_osi_Wakeup(&afs_initState);
	afs_RX_Running = 2;
	afs_osi_Wakeup(&afs_RX_Running);
	afs_osi_RxkRegister();
	rxk_Listener();
	AFS_GUNLOCK();
	thread_terminate(current_thread());
	break;
    default:
	printf("Unknown op %ld in StartDaemon()\n", (long)parm);
	break;
    }
}

void
afs_DaemonOp(long parm, long parm2, long parm3, long parm4, long parm5,
	     long parm6)
{
    int code;
    struct afsd_thread_info info;
    thread_t thread;

    if (parm == AFSOP_START_RXCALLBACK) {
	if (afs_CB_Running)
	    return;
    } else if (parm == AFSOP_RXLISTENER_DAEMON) {
	if (afs_RX_Running)
	    return;
	afs_RX_Running = 1;
	code = afs_InitSetup(parm2);
	if (parm3) {
	    rx_enablePeerRPCStats();
	}
	if (parm4) {
	    rx_enableProcessRPCStats();
	}
	if (code)
	    return;
    } else if (parm == AFSOP_START_AFS) {
	if (AFS_Running)
	    return;
    }				/* other functions don't need setup in the parent */
    info.parm = parm;
    kernel_thread_start((thread_continue_t)afsd_thread, &info, &thread);
    AFS_GUNLOCK();
    /* we need to wait cause we passed stack pointers around.... */
    msleep(&info, NULL, PVFS, "afs_DaemonOp", NULL);
    AFS_GLOCK();
    thread_deallocate(thread);
}
#endif


#if defined(AFS_LINUX24_ENV) && defined(COMPLETION_H_EXISTS)
struct afsd_thread_info {
#if defined(AFS_LINUX26_ENV) && !defined(INIT_WORK_HAS_DATA)
    struct work_struct tq;
#endif
    unsigned long parm;
    struct completion *complete;
};

static int
afsd_thread(void *rock)
{
    struct afsd_thread_info *arg = rock;
    unsigned long parm = arg->parm;
#ifdef SYS_SETPRIORITY_EXPORTED
    int (*sys_setpriority) (int, int, int) = sys_call_table[__NR_setpriority];
#endif
#if defined(AFS_LINUX26_ENV)
    daemonize("afsd");
#else
    daemonize();
#endif
				/* doesn't do much, since we were forked from keventd, but
				 * does call mm_release, which wakes up our parent (since it
				 * used CLONE_VFORK) */
#if !defined(AFS_LINUX26_ENV)
    reparent_to_init();
#endif
    afs_osi_MaskSignals();
    switch (parm) {
    case AFSOP_START_RXCALLBACK:
	sprintf(current->comm, "afs_cbstart");
	AFS_GLOCK();
	complete(arg->complete);
	afs_CB_Running = 1;
	while (afs_RX_Running != 2)
	    afs_osi_Sleep(&afs_RX_Running);
	sprintf(current->comm, "afs_callback");
	afs_RXCallBackServer();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_START_AFS:
	sprintf(current->comm, "afs_afsstart");
	AFS_GLOCK();
	complete(arg->complete);
	AFS_Running = 1;
	while (afs_initState < AFSOP_START_AFS)
	    afs_osi_Sleep(&afs_initState);
	afs_initState = AFSOP_START_BKG;
	afs_osi_Wakeup(&afs_initState);
	sprintf(current->comm, "afsd");
	afs_Daemon();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_START_BKG:
	sprintf(current->comm, "afs_bkgstart");
	AFS_GLOCK();
	complete(arg->complete);
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	if (afs_initState < AFSOP_GO) {
	    afs_initState = AFSOP_GO;
	    afs_osi_Wakeup(&afs_initState);
	}
	sprintf(current->comm, "afs_background");
	afs_BackgroundDaemon();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_START_TRUNCDAEMON:
	sprintf(current->comm, "afs_trimstart");
	AFS_GLOCK();
	complete(arg->complete);
	while (afs_initState < AFSOP_GO)
	    afs_osi_Sleep(&afs_initState);
	sprintf(current->comm, "afs_cachetrim");
	afs_CacheTruncateDaemon();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_START_CS:
	sprintf(current->comm, "afs_checkserver");
	AFS_GLOCK();
	complete(arg->complete);
	afs_CheckServerDaemon();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_RXEVENT_DAEMON:
	sprintf(current->comm, "afs_evtstart");
#ifdef SYS_SETPRIORITY_EXPORTED
	sys_setpriority(PRIO_PROCESS, 0, -10);
#else
#ifdef CURRENT_INCLUDES_NICE
	current->nice = -10;
#endif
#endif
	AFS_GLOCK();
	complete(arg->complete);
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	sprintf(current->comm, "afs_rxevent");
	afs_rxevent_daemon();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    case AFSOP_RXLISTENER_DAEMON:
	sprintf(current->comm, "afs_lsnstart");
#ifdef SYS_SETPRIORITY_EXPORTED
	sys_setpriority(PRIO_PROCESS, 0, -10);
#else
#ifdef CURRENT_INCLUDES_NICE
	current->nice = -10;
#endif
#endif
	AFS_GLOCK();
	complete(arg->complete);
	afs_initState = AFSOP_START_AFS;
	afs_osi_Wakeup(&afs_initState);
	afs_RX_Running = 2;
	afs_osi_Wakeup(&afs_RX_Running);
	afs_osi_RxkRegister();
	sprintf(current->comm, "afs_rxlistener");
	rxk_Listener();
	AFS_GUNLOCK();
	complete_and_exit(0, 0);
	break;
    default:
	printf("Unknown op %ld in StartDaemon()\n", (long)parm);
	break;
    }
    return 0;
}

void
#if defined(AFS_LINUX26_ENV) && !defined(INIT_WORK_HAS_DATA)
afsd_launcher(struct work_struct *work)
#else
afsd_launcher(void *rock)
#endif
{
#if defined(AFS_LINUX26_ENV) && !defined(INIT_WORK_HAS_DATA)
    struct afsd_thread_info *rock = container_of(work, struct afsd_thread_info, tq);
#endif

    if (!kernel_thread(afsd_thread, (void *)rock, CLONE_VFORK | SIGCHLD))
	printf("kernel_thread failed. afs startup will not complete\n");
}

void
afs_DaemonOp(long parm, long parm2, long parm3, long parm4, long parm5,
	     long parm6)
{
    int code;
    DECLARE_COMPLETION(c);
#if defined(AFS_LINUX26_ENV)
    struct work_struct tq;
#else
    struct tq_struct tq;
#endif
    struct afsd_thread_info info;
    if (parm == AFSOP_START_RXCALLBACK) {
	if (afs_CB_Running)
	    return;
    } else if (parm == AFSOP_RXLISTENER_DAEMON) {
	if (afs_RX_Running)
	    return;
	afs_RX_Running = 1;
	code = afs_InitSetup(parm2);
	if (parm3) {
	    rx_enablePeerRPCStats();
	}
	if (parm4) {
	    rx_enableProcessRPCStats();
	}
	if (code)
	    return;
    } else if (parm == AFSOP_START_AFS) {
	if (AFS_Running)
	    return;
    }				/* other functions don't need setup in the parent */
    info.complete = &c;
    info.parm = parm;
#if defined(AFS_LINUX26_ENV)
#if !defined(INIT_WORK_HAS_DATA)
    INIT_WORK(&info.tq, afsd_launcher);
    schedule_work(&info.tq);
#else
    INIT_WORK(&tq, afsd_launcher, &info);
    schedule_work(&tq);
#endif
#else
    tq.sync = 0;
    INIT_LIST_HEAD(&tq.list);
    tq.routine = afsd_launcher;
    tq.data = &info;
    schedule_task(&tq);
#endif
    AFS_GUNLOCK();
    /* we need to wait cause we passed stack pointers around.... */
    wait_for_completion(&c);
    AFS_GLOCK();
}
#endif

/* leaving as is, probably will barf if we add prototypes here since it's likely being called
with partial list */
int
afs_syscall_call(parm, parm2, parm3, parm4, parm5, parm6)
     long parm, parm2, parm3, parm4, parm5, parm6;
{
    afs_int32 code = 0;
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    size_t bufferSize;
#else /* AFS_SGI61_ENV */
    u_int bufferSize;
#endif /* AFS_SGI61_ENV */

    AFS_STATCNT(afs_syscall_call);
    if (
#ifdef	AFS_SUN5_ENV
	!afs_suser(CRED())
#else
	!afs_suser(NULL)
#endif
		    && (parm != AFSOP_GETMTU) && (parm != AFSOP_GETMASK)) {
	/* only root can run this code */
#if defined(AFS_OSF_ENV) || defined(AFS_SUN5_ENV) || defined(KERNEL_HAVE_UERROR)
#if defined(KERNEL_HAVE_UERROR)
	setuerror(EACCES);
#endif
	code = EACCES;
#else
	code = EPERM;
#endif
	AFS_GLOCK();
#ifdef AFS_DARWIN80_ENV
	put_vfs_context();
#endif
	goto out;
    }
    AFS_GLOCK();
#ifdef AFS_DARWIN80_ENV
    put_vfs_context();
#endif
#if ((defined(AFS_LINUX24_ENV) && defined(COMPLETION_H_EXISTS)) || defined(AFS_DARWIN80_ENV)) && !defined(UKERNEL)
    if (parm < AFSOP_ADDCELL || parm == AFSOP_RXEVENT_DAEMON
	|| parm == AFSOP_RXLISTENER_DAEMON) {
	afs_DaemonOp(parm, parm2, parm3, parm4, parm5, parm6);
    }
#else /* !(AFS_LINUX24_ENV && !UKERNEL) */
    if (parm == AFSOP_START_RXCALLBACK) {
	if (afs_CB_Running)
	    goto out;
	afs_CB_Running = 1;
#ifndef RXK_LISTENER_ENV
	code = afs_InitSetup(parm2);
	if (!code)
#endif /* !RXK_LISTENER_ENV */
	{
#ifdef RXK_LISTENER_ENV
	    while (afs_RX_Running != 2)
		afs_osi_Sleep(&afs_RX_Running);
#else /* !RXK_LISTENER_ENV */
	    afs_initState = AFSOP_START_AFS;
	    afs_osi_Wakeup(&afs_initState);
#endif /* RXK_LISTENER_ENV */
	    afs_osi_Invisible();
	    afs_RXCallBackServer();
	}
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, code);
#endif /* AFS_SGI_ENV */
    }
#ifdef RXK_LISTENER_ENV
    else if (parm == AFSOP_RXLISTENER_DAEMON) {
	if (afs_RX_Running)
	    goto out;
	afs_RX_Running = 1;
	code = afs_InitSetup(parm2);
	if (parm3) {
	    rx_enablePeerRPCStats();
	}
	if (parm4) {
	    rx_enableProcessRPCStats();
	}
	if (!code) {
	    afs_initState = AFSOP_START_AFS;
	    afs_osi_Wakeup(&afs_initState);
	    afs_osi_Invisible();
	    afs_RX_Running = 2;
	    afs_osi_Wakeup(&afs_RX_Running);
#ifndef UKERNEL
	    afs_osi_RxkRegister();
#endif /* !UKERNEL */
	    rxk_Listener();
	}
#ifdef	AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, code);
#endif /* AFS_SGI_ENV */
    }
#endif /* RXK_LISTENER_ENV */
    else if (parm == AFSOP_START_AFS) {
	/* afs daemon */
	if (AFS_Running)
	    goto out;
	AFS_Running = 1;
	while (afs_initState < AFSOP_START_AFS)
	    afs_osi_Sleep(&afs_initState);

	afs_initState = AFSOP_START_BKG;
	afs_osi_Wakeup(&afs_initState);
	afs_osi_Invisible();
	afs_Daemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif /* AFS_SGI_ENV */
    } else if (parm == AFSOP_START_CS) {
	afs_osi_Invisible();
	afs_CheckServerDaemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif /* AFS_SGI_ENV */
    } else if (parm == AFSOP_START_BKG) {
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	if (afs_initState < AFSOP_GO) {
	    afs_initState = AFSOP_GO;
	    afs_osi_Wakeup(&afs_initState);
	}
	/* start the bkg daemon */
	afs_osi_Invisible();
#ifdef AFS_AIX32_ENV
	if (parm2)
	    afs_BioDaemon(parm2);
	else
#endif /* AFS_AIX32_ENV */
	    afs_BackgroundDaemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif /* AFS_SGI_ENV */
    } else if (parm == AFSOP_START_TRUNCDAEMON) {
	while (afs_initState < AFSOP_GO)
	    afs_osi_Sleep(&afs_initState);
	/* start the bkg daemon */
	afs_osi_Invisible();
	afs_CacheTruncateDaemon();
#ifdef	AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif /* AFS_SGI_ENV */
    }
#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
    else if (parm == AFSOP_RXEVENT_DAEMON) {
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);
	afs_osi_Invisible();
	afs_rxevent_daemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif /* AFS_SGI_ENV */
    }
#endif /* AFS_SUN5_ENV || RXK_LISTENER_ENV */
#endif /* AFS_LINUX24_ENV && !UKERNEL */
    else if (parm == AFSOP_BASIC_INIT) {
	afs_int32 temp;

	while (!afs_InitSetup_done)
	    afs_osi_Sleep(&afs_InitSetup_done);

#if defined(AFS_SGI_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_SUN5_ENV)
	temp = AFS_MINBUFFERS;	/* Should fix this soon */
#else
	/* number of 2k buffers we could get from all of the buffer space */
	temp = ((afs_bufferpages * NBPG) >> 11);
	temp = temp >> 2;	/* don't take more than 25% (our magic parameter) */
	if (temp < AFS_MINBUFFERS)
	    temp = AFS_MINBUFFERS;	/* though we really should have this many */
#endif
	DInit(temp);
	afs_rootFid.Fid.Volume = 0;
	code = 0;
    } else if (parm == AFSOP_BUCKETPCT) {
	/* need to enable this now, will disable again before GO
	   if we don't have 100% */
	splitdcache = 1;
	switch (parm2) {
	case 1:
	    afs_tpct1 = parm3;
	    break;
	case 2:
	    afs_tpct2 = parm3;
	    break;
	}           
    } else if (parm == AFSOP_ADDCELL) {
	/* add a cell.  Parameter 2 is 8 hosts (in net order),  parm 3 is the null-terminated
	 * name.  Parameter 4 is the length of the name, including the null.  Parm 5 is the
	 * home cell flag (0x1 bit) and the nosuid flag (0x2 bit) */
	struct afsop_cell *tcell = afs_osi_Alloc(sizeof(struct afsop_cell));

	code = afs_InitDynroot();
	if (!code) {
	    AFS_COPYIN((char *)parm2, (char *)tcell->hosts, sizeof(tcell->hosts),
		       code);
	}
	if (!code) {
	    if (parm4 > sizeof(tcell->cellName))
		code = EFAULT;
	    else {
		AFS_COPYIN((char *)parm3, tcell->cellName, parm4, code);
		if (!code)
		    afs_NewCell(tcell->cellName, tcell->hosts, parm5, NULL, 0,
				0, 0);
	    }
	}
	afs_osi_Free(tcell, sizeof(struct afsop_cell));
    } else if (parm == AFSOP_ADDCELL2) {
	struct afsop_cell *tcell = afs_osi_Alloc(sizeof(struct afsop_cell));
	char *tbuffer = osi_AllocSmallSpace(AFS_SMALLOCSIZ), *lcnamep = 0;
	char *tbuffer1 = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
	int cflags = parm4;

	code = afs_InitDynroot();
	if (!code) {
#if 0
	    /* wait for basic init - XXX can't find any reason we need this? */
	    while (afs_initState < AFSOP_START_BKG)
		afs_osi_Sleep(&afs_initState);
#endif

	    AFS_COPYIN((char *)parm2, (char *)tcell->hosts, sizeof(tcell->hosts),
		       code);
	}
	if (!code) {
	    AFS_COPYINSTR((char *)parm3, tbuffer1, AFS_SMALLOCSIZ,
			  &bufferSize, code);
	    if (!code) {
		if (parm4 & 4) {
		    AFS_COPYINSTR((char *)parm5, tbuffer, AFS_SMALLOCSIZ,
				  &bufferSize, code);
		    if (!code) {
			lcnamep = tbuffer;
			cflags |= CLinkedCell;
		    }
		}
		if (!code)
		    code =
			afs_NewCell(tbuffer1, tcell->hosts, cflags, lcnamep,
				    0, 0, 0);
	    }
	}
	afs_osi_Free(tcell, sizeof(struct afsop_cell));
	osi_FreeSmallSpace(tbuffer);
	osi_FreeSmallSpace(tbuffer1);
    } else if (parm == AFSOP_ADDCELLALIAS) {
	/*
	 * Call arguments:
	 * parm2 is the alias name
	 * parm3 is the real cell name
	 */
	char *aliasName = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
	char *cellName = osi_AllocSmallSpace(AFS_SMALLOCSIZ);

	code = afs_InitDynroot();
	if (!code) {
	    AFS_COPYINSTR((char *)parm2, aliasName, AFS_SMALLOCSIZ, &bufferSize,
			  code);
	}
	if (!code)
	    AFS_COPYINSTR((char *)parm3, cellName, AFS_SMALLOCSIZ,
			  &bufferSize, code);
	if (!code)
	    afs_NewCellAlias(aliasName, cellName);
	osi_FreeSmallSpace(aliasName);
	osi_FreeSmallSpace(cellName);
    } else if (parm == AFSOP_SET_THISCELL) {
	/*
	 * Call arguments:
	 * parm2 is the primary cell name
	 */
	char *cell = osi_AllocSmallSpace(AFS_SMALLOCSIZ);

	code = afs_InitDynroot();
	if (!code) {
	    AFS_COPYINSTR((char *)parm2, cell, AFS_SMALLOCSIZ, &bufferSize, code);
	}
	if (!code)
	    afs_SetPrimaryCell(cell);
	osi_FreeSmallSpace(cell);
    } else if (parm == AFSOP_CACHEINIT) {
	struct afs_cacheParams cparms;

	if (afs_CacheInit_Done)
	    goto out;

	AFS_COPYIN((char *)parm2, (caddr_t) & cparms, sizeof(cparms), code);
	if (code) {
#if defined(KERNEL_HAVE_UERROR)
	    setuerror(code);
	    code = -1;
#endif
	    goto out;
	}
	afs_CacheInit_Done = 1;
        code = afs_icl_InitLogs();
	afs_setTime = cparms.setTimeFlag;

	code =
	    afs_CacheInit(cparms.cacheScaches, cparms.cacheFiles,
			  cparms.cacheBlocks, cparms.cacheDcaches,
			  cparms.cacheVolumes, cparms.chunkSize,
			  cparms.memCacheFlag, cparms.inodes, cparms.users);

    } else if (parm == AFSOP_CACHEINODE) {
	ino_t ainode = parm2;
	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);

#ifdef AFS_DARWIN80_ENV
	get_vfs_context();
#endif
	/* do it by inode */
#ifdef AFS_SGI62_ENV
	ainode = (ainode << 32) | (parm3 & 0xffffffff);
#endif
	code = afs_InitCacheFile(NULL, ainode);
#ifdef AFS_DARWIN80_ENV
	put_vfs_context();
#endif
    } else if (parm == AFSOP_ROOTVOLUME) {
	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG)
	    afs_osi_Sleep(&afs_initState);

	if (parm2) {
	    AFS_COPYINSTR((char *)parm2, afs_rootVolumeName,
			  sizeof(afs_rootVolumeName), &bufferSize, code);
	    afs_rootVolumeName[sizeof(afs_rootVolumeName) - 1] = 0;
	} else
	    code = 0;
    } else if (parm == AFSOP_CACHEFILE || parm == AFSOP_CACHEINFO
	       || parm == AFSOP_VOLUMEINFO || parm == AFSOP_AFSLOG
	       || parm == AFSOP_CELLINFO) {
	char *tbuffer = osi_AllocSmallSpace(AFS_SMALLOCSIZ);

	code = 0;
	AFS_COPYINSTR((char *)parm2, tbuffer, AFS_SMALLOCSIZ, &bufferSize,
		      code);
	if (code) {
	    osi_FreeSmallSpace(tbuffer);
	    goto out;
	}
	if (!code) {
	    tbuffer[AFS_SMALLOCSIZ - 1] = '\0';	/* null-terminate the name */
	    /* We have the cache dir copied in.  Call the cache init routine */
#ifdef AFS_DARWIN80_ENV
    get_vfs_context();
#endif
	    if (parm == AFSOP_CACHEFILE)
		code = afs_InitCacheFile(tbuffer, 0);
	    else if (parm == AFSOP_CACHEINFO)
		code = afs_InitCacheInfo(tbuffer);
	    else if (parm == AFSOP_VOLUMEINFO)
		code = afs_InitVolumeInfo(tbuffer);
	    else if (parm == AFSOP_CELLINFO)
		code = afs_InitCellInfo(tbuffer);
#ifdef AFS_DARWIN80_ENV
	    put_vfs_context();
#endif
	}
	osi_FreeSmallSpace(tbuffer);
    } else if (parm == AFSOP_GO) {
	/* the generic initialization calls come here.  One parameter: should we do the
	 * set-time operation on this workstation */
	if (afs_Go_Done)
	    goto out;
	afs_Go_Done = 1;
	while (afs_initState < AFSOP_GO)
	    afs_osi_Sleep(&afs_initState);
	afs_initState = 101;
	afs_setTime = parm2;
	if (afs_tpct1 + afs_tpct2 != 100) {
	    afs_tpct1 = 0;
	    afs_tpct2 = 0;
	    splitdcache = 0;
	} else {        
	    splitdcache = 1;
	}
	afs_osi_Wakeup(&afs_initState);
#if	(!defined(AFS_NONFSTRANS)) || defined(AFS_AIX_IAUTH_ENV)
	afs_nfsclient_init();
#endif
	afs_uuid_create(&afs_cb_interface.uuid);
	printf("found %d non-empty cache files (%d%%).\n",
	       afs_stats_cmperf.cacheFilesReused,
	       (100 * afs_stats_cmperf.cacheFilesReused) /
	       (afs_stats_cmperf.cacheNumEntries ? afs_stats_cmperf.
		cacheNumEntries : 1));
    } else if (parm == AFSOP_ADVISEADDR) {
	/* pass in the host address to the rx package */
	int rxbind = 0;
	int refresh = 0;

	afs_int32 count = parm2;
	afs_int32 *buffer =
	    afs_osi_Alloc(sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);
	afs_int32 *maskbuffer =
	    afs_osi_Alloc(sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);
	afs_int32 *mtubuffer =
	    afs_osi_Alloc(sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);
	int i;

	/* This is a refresh */
	if (count & 0x40000000) {
	    count &= ~0x40000000;
	    /* Can't bind after we start. Fix? */
	    count &= ~0x80000000;
	    refresh = 1;
	}

	/* Bind, but only if there's only one address configured */ 
	if ( count & 0x80000000) {
	    count &= ~0x80000000;
	    if (count == 1)
		rxbind=1;
	}

	if (count > AFS_MAX_INTERFACE_ADDR) {
	    code = ENOMEM;
	    count = AFS_MAX_INTERFACE_ADDR;
	}

	AFS_COPYIN((char *)parm3, (char *)buffer, count * sizeof(afs_int32),
		   code);
	if (parm4)
	    AFS_COPYIN((char *)parm4, (char *)maskbuffer,
		       count * sizeof(afs_int32), code);
	if (parm5)
	    AFS_COPYIN((char *)parm5, (char *)mtubuffer,
		       count * sizeof(afs_int32), code);

	afs_cb_interface.numberOfInterfaces = count;
	for (i = 0; i < count; i++) {
	    afs_cb_interface.addr_in[i] = buffer[i];
#ifdef AFS_USERSPACE_IP_ADDR
	    /* AFS_USERSPACE_IP_ADDR means we have no way of finding the
	     * machines IP addresses when in the kernel (the in_ifaddr
	     * struct is not available), so we pass the info in at
	     * startup. We also pass in the subnetmask and mtu size. The
	     * subnetmask is used when setting the rank:
	     * afsi_SetServerIPRank(); and the mtu size is used when
	     * finding the best mtu size. rxi_FindIfnet() is replaced
	     * with rxi_Findcbi().
	     */
	    afs_cb_interface.subnetmask[i] =
		(parm4 ? maskbuffer[i] : 0xffffffff);
	    afs_cb_interface.mtu[i] = (parm5 ? mtubuffer[i] : htonl(1500));
#endif
	}
	rxi_setaddr(buffer[0]);
	if (!refresh) {
	    if (rxbind)
		rx_bindhost = buffer[0];
	    else
		rx_bindhost = htonl(INADDR_ANY);
	}

	afs_osi_Free(buffer, sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);
	afs_osi_Free(maskbuffer, sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);
	afs_osi_Free(mtubuffer, sizeof(afs_int32) * AFS_MAX_INTERFACE_ADDR);

	if (refresh) {
	    afs_CheckServers(1, NULL);     /* check down servers */
	    afs_CheckServers(0, NULL);     /* check down servers */
	}
    }
#ifdef	AFS_SGI53_ENV
    else if (parm == AFSOP_NFSSTATICADDR) {
	extern int (*nfs_rfsdisptab_v2) ();
	nfs_rfsdisptab_v2 = (int (*)())parm2;
    } else if (parm == AFSOP_NFSSTATICADDR2) {
	extern int (*nfs_rfsdisptab_v2) ();
#ifdef _K64U64
	nfs_rfsdisptab_v2 = (int (*)())((parm2 << 32) | (parm3 & 0xffffffff));
#else /* _K64U64 */
	nfs_rfsdisptab_v2 = (int (*)())(parm3 & 0xffffffff);
#endif /* _K64U64 */
    }
#if defined(AFS_SGI62_ENV) && !defined(AFS_SGI65_ENV)
    else if (parm == AFSOP_SBLOCKSTATICADDR2) {
	extern int (*afs_sblockp) ();
	extern void (*afs_sbunlockp) ();
#ifdef _K64U64
	afs_sblockp = (int (*)())((parm2 << 32) | (parm3 & 0xffffffff));
	afs_sbunlockp = (void (*)())((parm4 << 32) | (parm5 & 0xffffffff));
#else
	afs_sblockp = (int (*)())(parm3 & 0xffffffff);
	afs_sbunlockp = (void (*)())(parm5 & 0xffffffff);
#endif /* _K64U64 */
    }
#endif /* AFS_SGI62_ENV && !AFS_SGI65_ENV */
#endif /* AFS_SGI53_ENV */
    else if (parm == AFSOP_SHUTDOWN) {
	afs_cold_shutdown = 0;
	if (parm2 == 1)
	    afs_cold_shutdown = 1;
#ifndef AFS_DARWIN_ENV
	if (afs_globalVFS != 0) {
	    afs_warn("AFS isn't unmounted yet! Call aborted\n");
	    code = EACCES;
	} else
#endif
	    afs_shutdown();
    } else if (parm == AFSOP_AFS_VFSMOUNT) {
#ifdef	AFS_HPUX_ENV
	vfsmount(parm2, parm3, parm4, parm5);
#else /* defined(AFS_HPUX_ENV) */
#if defined(KERNEL_HAVE_UERROR)
	setuerror(EINVAL);
#else
	code = EINVAL;
#endif
#endif /* defined(AFS_HPUX_ENV) */
    } else if (parm == AFSOP_CLOSEWAIT) {
	afs_SynchronousCloses = 'S';
    } else if (parm == AFSOP_GETMTU) {
	afs_uint32 mtu = 0;
#if	!defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV)
#ifdef AFS_USERSPACE_IP_ADDR
	afs_int32 i;
	i = rxi_Findcbi(parm2);
	mtu = ((i == -1) ? htonl(1500) : afs_cb_interface.mtu[i]);
#else /* AFS_USERSPACE_IP_ADDR */
	AFS_IFNET_T tifnp;

	tifnp = rxi_FindIfnet(parm2, NULL);	/*  make iterative */
	mtu = (tifnp ? ifnet_mtu(tifnp) : htonl(1500));
#endif /* else AFS_USERSPACE_IP_ADDR */
#endif /* !AFS_SUN5_ENV */
	if (!code)
	    AFS_COPYOUT((caddr_t) & mtu, (caddr_t) parm3, sizeof(afs_int32),
			code);
#ifdef AFS_AIX32_ENV
/* this is disabled for now because I can't figure out how to get access
 * to these kernel variables.  It's only for supporting user-mode rx
 * programs -- it makes a huge difference on the 220's in my testbed,
 * though I don't know why. The bosserver does this with /etc/no, so it's
 * being handled a different way for the servers right now.  */
/*      {
	static adjusted = 0;
	extern u_long sb_max_dflt;
	if (!adjusted) {
	  adjusted = 1;
	  if (sb_max_dflt < 131072) sb_max_dflt = 131072; 
	  if (sb_max < 131072) sb_max = 131072; 
	}
      } */
#endif /* AFS_AIX32_ENV */
    } else if (parm == AFSOP_GETMASK) {	/* parm2 == addr in net order */
	afs_uint32 mask = 0;
#if	!defined(AFS_SUN5_ENV)
#ifdef AFS_USERSPACE_IP_ADDR
	afs_int32 i;
	i = rxi_Findcbi(parm2);
	if (i != -1) {
	    mask = afs_cb_interface.subnetmask[i];
	} else {
	    code = -1;
	}
#else /* AFS_USERSPACE_IP_ADDR */
	AFS_IFNET_T tifnp;

	tifnp = rxi_FindIfnet(parm2, &mask);	/* make iterative */
	if (!tifnp)
	    code = -1;
#endif /* else AFS_USERSPACE_IP_ADDR */
#endif /* !AFS_SUN5_ENV */
	if (!code)
	    AFS_COPYOUT((caddr_t) & mask, (caddr_t) parm3, sizeof(afs_int32),
			code);
    }
#ifdef AFS_AFSDB_ENV
    else if (parm == AFSOP_AFSDB_HANDLER) {
	int sizeArg = (int)parm4;
	int kmsgLen = sizeArg & 0xffff;
	int cellLen = (sizeArg & 0xffff0000) >> 16;
	afs_int32 *kmsg = afs_osi_Alloc(kmsgLen);
	char *cellname = afs_osi_Alloc(cellLen);

#ifndef UKERNEL
	afs_osi_MaskUserLoop();
#endif
	AFS_COPYIN((afs_int32 *) parm2, cellname, cellLen, code);
	AFS_COPYIN((afs_int32 *) parm3, kmsg, kmsgLen, code);
	if (!code) {
	    code = afs_AFSDBHandler(cellname, cellLen, kmsg);
	    if (*cellname == 1)
		*cellname = 0;
	    if (code == -2) {	/* Shutting down? */
		*cellname = 1;
		code = 0;
	    }
	}
	if (!code)
	    AFS_COPYOUT(cellname, (char *)parm2, cellLen, code);
	afs_osi_Free(kmsg, kmsgLen);
	afs_osi_Free(cellname, cellLen);
    }
#endif
    else if (parm == AFSOP_SET_DYNROOT) {
	code = afs_SetDynrootEnable(parm2);
    } else if (parm == AFSOP_SET_FAKESTAT) {
	afs_fakestat_enable = parm2;
	code = 0;
    } else if (parm == AFSOP_SET_BACKUPTREE) {
	afs_bkvolpref = parm2;
    } else if (parm == AFSOP_SET_RXPCK) {
	rx_extraPackets = parm2;
	afscall_set_rxpck_received = 1;
    } else
	code = EINVAL;

  out:
    AFS_GUNLOCK();
#ifdef AFS_LINUX20_ENV
    return -code;
#else
    return code;
#endif
}

/*
 * Initstate in the range 0 < x < 100 are early initialization states.
 * Initstate of 100 means a AFSOP_START operation has been done.  After this,
 *  the cache may be initialized.
 * Initstate of 101 means a AFSOP_GO operation has been done.  This operation
 *  is done after all the cache initialization has been done.
 * Initstate of 200 means that the volume has been looked up once, possibly
 *  incorrectly.
 * Initstate of 300 means that the volume has been *successfully* looked up.
 */
int
afs_CheckInit(void)
{
    register int code = 0;

    AFS_STATCNT(afs_CheckInit);
    if (afs_initState <= 100)
	code = ENXIO;		/* never finished init phase */
    else if (afs_initState == 101) {	/* init done, wait for afs_daemon */
	while (afs_initState < 200)
	    afs_osi_Sleep(&afs_initState);
    } else if (afs_initState == 200)
	code = ETIMEDOUT;	/* didn't find root volume */
    return code;
}

int afs_shuttingdown = 0;
void
afs_shutdown(void)
{
    extern short afs_brsDaemons;
    extern afs_int32 afs_CheckServerDaemonStarted;
    extern struct afs_osi_WaitHandle AFS_WaitHandler, AFS_CSWaitHandler;
    extern struct osi_file *afs_cacheInodep;

    AFS_STATCNT(afs_shutdown);
    if (afs_initState == 0) {
        afs_warn("AFS not initialized - not shutting down\n");
      return;
    }

    if (afs_shuttingdown)
	return;
    afs_shuttingdown = 1;
    if (afs_cold_shutdown)
	afs_warn("COLD ");
    else
	afs_warn("WARM ");
    afs_warn("shutting down of: CB... ");

    afs_termState = AFSOP_STOP_RXCALLBACK;
    rx_WakeupServerProcs();
#ifdef AFS_AIX51_ENV
    shutdown_rxkernel();
#endif
    /* shutdown_rxkernel(); */
    while (afs_termState == AFSOP_STOP_RXCALLBACK)
	afs_osi_Sleep(&afs_termState);

    afs_warn("afs... ");
    while (afs_termState == AFSOP_STOP_AFS) {
	afs_osi_CancelWait(&AFS_WaitHandler);
	afs_osi_Sleep(&afs_termState);
    }
    if (afs_CheckServerDaemonStarted) {
	while (afs_termState == AFSOP_STOP_CS) {
	    afs_osi_CancelWait(&AFS_CSWaitHandler);
	    afs_osi_Sleep(&afs_termState);
	}
    }
    afs_warn("BkG... ");
    /* Wake-up afs_brsDaemons so that we don't have to wait for a bkg job! */
    while (afs_termState == AFSOP_STOP_BKG) {
	afs_osi_Wakeup(&afs_brsDaemons);
	afs_osi_Sleep(&afs_termState);
    }
    afs_warn("CTrunc... ");
    /* Cancel cache truncate daemon. */
    while (afs_termState == AFSOP_STOP_TRUNCDAEMON) {
	afs_osi_Wakeup((char *)&afs_CacheTruncateDaemon);
	afs_osi_Sleep(&afs_termState);
    }
#ifdef AFS_AFSDB_ENV
    afs_warn("AFSDB... ");
    afs_StopAFSDB();
    while (afs_termState == AFSOP_STOP_AFSDB)
	afs_osi_Sleep(&afs_termState);
#endif
#if	defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
    afs_warn("RxEvent... ");
    /* cancel rx event daemon */
    while (afs_termState == AFSOP_STOP_RXEVENT)
	afs_osi_Sleep(&afs_termState);
#if defined(RXK_LISTENER_ENV)
#ifndef UKERNEL
    afs_warn("UnmaskRxkSignals... ");
    afs_osi_UnmaskRxkSignals();
#endif
    /* cancel rx listener */
    afs_warn("RxListener... ");
    osi_StopListener();		/* This closes rx_socket. */
    while (afs_termState == AFSOP_STOP_RXK_LISTENER) {
	afs_warn("Sleep... ");
	afs_osi_Sleep(&afs_termState);
    }
#endif
#else
    afs_termState = AFSOP_STOP_COMPLETE;
#endif
    afs_warn("\n");

    /* Close file only after daemons which can write to it are stopped. */
    if (afs_cacheInodep) {	/* memcache won't set this */
	osi_UFSClose(afs_cacheInodep);	/* Since we always leave it open */
	afs_cacheInodep = 0;
    }
    return;			/* Just kill daemons for now */
#ifdef notdef
    shutdown_CB();
    shutdown_AFS();
    shutdown_rxkernel();
    shutdown_rxevent();
    shutdown_rx();
    afs_shutdown_BKG();
    shutdown_bufferpackage();
#endif
#ifdef AFS_AIX51_ENV
    shutdown_daemons();
#endif
#ifdef notdef
    shutdown_cache();
    shutdown_osi();
    shutdown_osinet();
    shutdown_osifile();
    shutdown_vnodeops();
    shutdown_vfsops();
    shutdown_exporter();
    shutdown_memcache();
#if (!defined(AFS_NONFSTRANS) || defined(AFS_AIX_IAUTH_ENV)) && !defined(AFS_OSF_ENV)
    shutdown_nfsclnt();
#endif
    shutdown_afstest();
    /* The following hold the cm stats */
/*
    memset(&afs_cmstats, 0, sizeof(struct afs_CMStats));
    memset(&afs_stats_cmperf, 0, sizeof(struct afs_stats_CMPerf));
    memset(&afs_stats_cmfullperf, 0, sizeof(struct afs_stats_CMFullPerf));
*/
    afs_warn(" ALL allocated tables\n");
    afs_shuttingdown = 0;
#endif
}

void
shutdown_afstest(void)
{
    AFS_STATCNT(shutdown_afstest);
    afs_initState = afs_termState = afs_setTime = 0;
    AFS_Running = afs_CB_Running = 0;
    afs_CacheInit_Done = afs_Go_Done = 0;
    if (afs_cold_shutdown) {
	*afs_rootVolumeName = 0;
    }
}


/* In case there is a bunch of dynamically build bkg daemons to free */
void
afs_shutdown_BKG(void)
{
    AFS_STATCNT(shutdown_BKG);
}
