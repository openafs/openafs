/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006-2007 Sine Nomine Associates
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#endif
#ifdef BOZO_SAVE_CORES
#include <time.h>
#endif
#include <sys/stat.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <lwp.h>
#include <afs/audit.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include "bnode.h"

#include <osi/osi_includes.h>
#include <osi/osi_trace.h>
#include "tracepoint_table.h"

#if defined(OSI_TRACE_ENABLED)
/*
 * for trace enabled build system,
 * turn on asynchronous messaging
 * for important bozo events
 */
#include <trace/mail.h>
#include <trace/USERSPACE/mail.h>
#include <trace/mail/msg.h>

osi_static osi_result
bnode_start_mail_msg(struct bnode_proc * aproc);
osi_static osi_result
bnode_stop_mail_msg(struct bnode_proc * aproc);
osi_static osi_result
bnode_crash_mail_msg(struct bnode_proc * aproc);
#else /* !OSI_TRACE_ENABLED */
#define bnode_start_mail_msg(x)   (OSI_OK)
#define bnode_stop_mail_msg(x)    (OSI_OK)
#define bnode_crash_mail_msg(x)   (OSI_OK)
#endif /* !OSI_TRACE_ENABLED */

#if defined(AFS_AIX_ENV) || defined(AFS_SUN4_ENV)
/* All known versions of AIX lack WCOREDUMP but this works */
#define WCOREDUMP(x) ((x) & 0x80)
#endif

#define BNODE_LWP_STACKSIZE	(16 * 1024)

int bnode_waiting = 0;
static PROCESS bproc_pid;	/* pid of waker-upper */
static struct bnode *allBnodes = 0;	/* list of all bnodes */
static struct bnode_proc *allProcs = 0;	/* list of all processes for which we're waiting */
static struct bnode_type *allTypes = 0;	/* list of registered type handlers */

static struct bnode_stats {
    int weirdPids;
} bnode_stats;

#ifndef AFS_NT40_ENV
extern char **environ;		/* env structure */
#endif

/* Remember the name of the process, if any, that failed last */
static void
RememberProcName(register struct bnode_proc *ap)
{
    register struct bnode *tbnodep;

    tbnodep = ap->bnode;
    if (tbnodep->lastErrorName) {
	free(tbnodep->lastErrorName);
	tbnodep->lastErrorName = NULL;
    }
    if (ap->coreName) {
	tbnodep->lastErrorName = (char *)malloc(strlen(ap->coreName) + 1);
	strcpy(tbnodep->lastErrorName, ap->coreName);
    }
}

/* utility for use by BOP_HASCORE functions to determine where a core file might
 * be stored.
 */
int
bnode_CoreName(register struct bnode *abnode, char *acoreName, char *abuffer)
{
    strcpy(abuffer, AFSDIR_SERVER_CORELOG_FILEPATH);
    if (acoreName) {
	strcat(abuffer, acoreName);
	strcat(abuffer, ".");
    }
    strcat(abuffer, abnode->name);
    return 0;
}

/* save core file, if any */
static void
SaveCore(register struct bnode *abnode, register struct bnode_proc
	 *aproc)
{
    char tbuffer[256];
    struct stat tstat;
    register afs_int32 code;
#ifdef BOZO_SAVE_CORES
    struct timeval Start;
    struct tm *TimeFields;
    char FileName[256];
#endif

    code = stat(AFSDIR_SERVER_CORELOG_FILEPATH, &tstat);
    if (code)
	return;

    bnode_CoreName(abnode, aproc->coreName, tbuffer);
#ifdef BOZO_SAVE_CORES
    TM_GetTimeOfDay(&Start, 0);
    TimeFields = localtime(&Start.tv_sec);
    sprintf(FileName, "%s.%d%02d%02d%02d%02d%02d", tbuffer,
	    TimeFields->tm_year, TimeFields->tm_mon + 1, TimeFields->tm_mday,
	    TimeFields->tm_hour, TimeFields->tm_min, TimeFields->tm_sec);
    strcpy(tbuffer, FileName);
#endif
    code = renamefile(AFSDIR_SERVER_CORELOG_FILEPATH, tbuffer);
}

int
bnode_GetString(register struct bnode *abnode, register char *abuffer,
		register afs_int32 alen)
{
    return BOP_GETSTRING(abnode, abuffer, alen);
}

int
bnode_GetParm(register struct bnode *abnode, register afs_int32 aindex,
	      register char *abuffer, afs_int32 alen)
{
    return BOP_GETPARM(abnode, aindex, abuffer, alen);
}

int
bnode_GetStat(register struct bnode *abnode, register afs_int32 * astatus)
{
    return BOP_GETSTAT(abnode, astatus);
}

int
bnode_RestartP(register struct bnode *abnode)
{
    return BOP_RESTARTP(abnode);
}

static int
bnode_Check(register struct bnode *abnode)
{
    if (abnode->flags & BNODE_WAIT) {
	abnode->flags &= ~BNODE_WAIT;
	LWP_NoYieldSignal(abnode);
    }
    return 0;
}

/* tell if an instance has a core file */
int
bnode_HasCore(register struct bnode *abnode)
{
    return BOP_HASCORE(abnode);
}

/* wait for all bnodes to stabilize */
int
bnode_WaitAll()
{
    register struct bnode *tb;
    register afs_int32 code;
    afs_int32 stat;

  retry:
    for (tb = allBnodes; tb; tb = tb->next) {
	bnode_Hold(tb);
	code = BOP_GETSTAT(tb, &stat);
	if (code) {
	    bnode_Release(tb);
	    return code;
	}
	if (stat != tb->goal) {
	    tb->flags |= BNODE_WAIT;
	    LWP_WaitProcess(tb);
	    bnode_Release(tb);
	    goto retry;
	}
	bnode_Release(tb);
    }
    return 0;
}

/* wait until bnode status is correct */
int
bnode_WaitStatus(register struct bnode *abnode, int astatus)
{
    register afs_int32 code;
    afs_int32 stat;

    bnode_Hold(abnode);
    while (1) {
	/* get the status */
	code = BOP_GETSTAT(abnode, &stat);
	if (code)
	    return code;

	/* otherwise, check if we're done */
	if (stat == astatus) {
	    bnode_Release(abnode);
	    return 0;		/* done */
	}
	if (astatus != abnode->goal) {
	    bnode_Release(abnode);
	    return -1;		/* no longer our goal, don't keep waiting */
	}
	/* otherwise, block */
	abnode->flags |= BNODE_WAIT;
	LWP_WaitProcess(abnode);
    }
}

int
bnode_SetStat(register struct bnode *abnode, register int agoal)
{
    abnode->goal = agoal;
    bnode_Check(abnode);
    BOP_SETSTAT(abnode, agoal);
    abnode->flags &= ~BNODE_ERRORSTOP;
    return 0;
}

int
bnode_SetGoal(register struct bnode *abnode, register int agoal)
{
    abnode->goal = agoal;
    bnode_Check(abnode);
    return 0;
}

int
bnode_SetFileGoal(register struct bnode *abnode, register int agoal)
{
    if (abnode->fileGoal == agoal)
	return 0;		/* already done */
    abnode->fileGoal = agoal;
    WriteBozoFile(0);
    return 0;
}

/* apply a function to all bnodes in the system */
int
bnode_ApplyInstance(int (*aproc) (), char *arock)
{
    register struct bnode *tb, *nb;
    register afs_int32 code;

    for (tb = allBnodes; tb; tb = nb) {
	nb = tb->next;
	code = (*aproc) (tb, arock);
	if (code)
	    return code;
    }
    return 0;
}

struct bnode *
bnode_FindInstance(register char *aname)
{
    register struct bnode *tb;

    for (tb = allBnodes; tb; tb = tb->next) {
	if (!strcmp(tb->name, aname))
	    return tb;
    }
    return NULL;
}

static struct bnode_type *
FindType(register char *aname)
{
    register struct bnode_type *tt;

    for (tt = allTypes; tt; tt = tt->next) {
	if (!strcmp(tt->name, aname))
	    return tt;
    }
    return (struct bnode_type *)0;
}

int
bnode_Register(char *atype, struct bnode_ops *aprocs, int anparms)
{
    register struct bnode_type *tt;

    for (tt = allTypes; tt; tt = tt->next) {
	if (!strcmp(tt->name, atype))
	    break;
    }
    if (!tt) {
	tt = (struct bnode_type *)malloc(sizeof(struct bnode_type));
	memset(tt, 0, sizeof(struct bnode_type));
	tt->next = allTypes;
	allTypes = tt;
	tt->name = atype;
    }
    tt->ops = aprocs;
    return 0;
}

afs_int32
bnode_Create(char *atype, char *ainstance, struct bnode ** abp, char *ap1,
	     char *ap2, char *ap3, char *ap4, char *ap5, char *notifier,
	     int fileGoal, int rewritefile)
{
    struct bnode_type *type;
    struct bnode *tb;
    char *notifierpath = NULL;
    struct stat tstat;

    if (bnode_FindInstance(ainstance))
	return BZEXISTS;
    type = FindType(atype);
    if (!type)
	return BZBADTYPE;

    if (notifier && strcmp(notifier, NONOTIFIER)) {
	/* construct local path from canonical (wire-format) path */
	if (ConstructLocalBinPath(notifier, &notifierpath)) {
	    bozo_Log("BNODE-Create: Notifier program path invalid '%s'\n",
		     notifier);
	    return BZNOCREATE;
	}

	if (stat(notifierpath, &tstat)) {
	    bozo_Log("BNODE-Create: Notifier program '%s' not found\n",
		     notifierpath);
	    free(notifierpath);
	    return BZNOCREATE;
	}
    }
    tb = (*type->ops->create) (ainstance, ap1, ap2, ap3, ap4, ap5);
    if (!tb) {
	free(notifierpath);
	return BZNOCREATE;
    }
    tb->notifier = notifierpath;
    *abp = tb;
    tb->type = type;

    /* The fs_create above calls bnode_InitBnode() which always sets the 
     ** fileGoal to BSTAT_NORMAL .... overwrite it with whatever is passed into
     ** this function as a parameter... */
    tb->fileGoal = fileGoal;

    bnode_SetStat(tb, tb->goal);	/* nudge it once */

    if (rewritefile != 0)
	WriteBozoFile(0);

    return 0;
}

int
bnode_DeleteName(char *ainstance)
{
    register struct bnode *tb;

    tb = bnode_FindInstance(ainstance);
    if (!tb)
	return BZNOENT;

    return bnode_Delete(tb);
}

int
bnode_Hold(register struct bnode *abnode)
{
    abnode->refCount++;
    return 0;
}

int
bnode_Release(register struct bnode *abnode)
{
    abnode->refCount--;
    if (abnode->refCount == 0 && abnode->flags & BNODE_DELETE) {
	abnode->flags &= ~BNODE_DELETE;	/* we're going for it */
	bnode_Delete(abnode);
    }
    return 0;
}

int
bnode_Delete(register struct bnode *abnode)
{
    register afs_int32 code;
    register struct bnode **lb, *ub;
    afs_int32 temp;

    if (abnode->refCount != 0) {
	abnode->flags |= BNODE_DELETE;
	return 0;
    }

    /* make sure the bnode is idle before zapping */
    bnode_Hold(abnode);
    code = BOP_GETSTAT(abnode, &temp);
    bnode_Release(abnode);
    if (code)
	return code;
    if (temp != BSTAT_SHUTDOWN)
	return BZBUSY;

    /* all clear to zap */
    for (lb = &allBnodes, ub = *lb; ub; lb = &ub->next, ub = *lb) {
	if (ub == abnode) {
	    /* unthread it from the list */
	    *lb = ub->next;
	    break;
	}
    }
    free(abnode->name);		/* do this first, since bnode fields may be bad after BOP_DELETE */
    code = BOP_DELETE(abnode);	/* don't play games like holding over this one */
    WriteBozoFile(0);
    return code;
}

/* function to tell if there's a timeout coming up */
int
bnode_PendingTimeout(register struct bnode *abnode)
{
    return (abnode->flags & BNODE_NEEDTIMEOUT);
}

/* function called to set / clear periodic bnode wakeup times */
int
bnode_SetTimeout(register struct bnode *abnode, afs_int32 atimeout)
{
    if (atimeout != 0) {
	abnode->nextTimeout = FT_ApproxTime() + atimeout;
	abnode->flags |= BNODE_NEEDTIMEOUT;
	abnode->period = atimeout;
	IOMGR_Cancel(bproc_pid);
    } else {
	abnode->flags &= ~BNODE_NEEDTIMEOUT;
    }
    return 0;
}

/* used by new bnode creation code to format bnode header */
int
bnode_InitBnode(register struct bnode *abnode, struct bnode_ops *abnodeops,
		char *aname)
{
    struct bnode **lb, *nb;

    /* format the bnode properly */
    memset(abnode, 0, sizeof(struct bnode));
    abnode->ops = abnodeops;
    abnode->name = (char *)malloc(strlen(aname) + 1);
    if (!abnode->name)
	return ENOMEM;
    strcpy(abnode->name, aname);
    abnode->flags = BNODE_ACTIVE;
    abnode->fileGoal = BSTAT_NORMAL;
    abnode->goal = BSTAT_SHUTDOWN;

    /* put the bnode at the end of the list so we write bnode file in same order */
    for (lb = &allBnodes, nb = *lb; nb; lb = &nb->next, nb = *lb);
    *lb = abnode;

    return 0;
}

static int
DeleteProc(register struct bnode_proc *abproc)
{
    register struct bnode_proc **pb, *tb;
    struct bnode_proc *nb;

    for (pb = &allProcs, tb = *pb; tb; pb = &tb->next, tb = nb) {
	nb = tb->next;
	if (tb == abproc) {
	    *pb = nb;
	    free(tb);
	    return 0;
	}
    }
    return BZNOENT;
}

/* bnode lwp executes this code repeatedly */
static int
bproc()
{
    register afs_int32 code;
    register struct bnode *tb;
    register afs_int32 temp;
    register struct bnode_proc *tp;
    struct bnode *nb;
    int options;		/* must not be register */
    struct timeval tv;
    int setAny;
    int status;

    while (1) {
	/* first figure out how long to sleep for */
	temp = 0x7fffffff;	/* afs_int32 time; maxint doesn't work in select */
	setAny = 0;
	for (tb = allBnodes; tb; tb = tb->next) {
	    if (tb->flags & BNODE_NEEDTIMEOUT) {
		if (tb->nextTimeout < temp) {
		    setAny = 1;
		    temp = tb->nextTimeout;
		}
	    }
	}
	/* now temp has the time at which we should wakeup next */

	/* sleep */
	if (setAny)
	    temp -= FT_ApproxTime();	/* how many seconds until next event */
	else
	    temp = 999999;
	if (temp > 0) {
	    tv.tv_sec = temp;
	    tv.tv_usec = 0;
	    code = IOMGR_Select(0, 0, 0, 0, &tv);
	} else
	    code = 0;		/* fake timeout code */

	/* figure out why we woke up; child exit or timeouts */
	FT_GetTimeOfDay(&tv, 0);	/* must do the real gettimeofday once and a while */
	temp = tv.tv_sec;

	/* check all bnodes to see which ones need timeout events */
	for (tb = allBnodes; tb; tb = nb) {
	    if ((tb->flags & BNODE_NEEDTIMEOUT) && temp > tb->nextTimeout) {
		bnode_Hold(tb);
		BOP_TIMEOUT(tb);
		bnode_Check(tb);
		if (tb->flags & BNODE_NEEDTIMEOUT) {	/* check again, BOP_TIMEOUT could change */
		    tb->nextTimeout = FT_ApproxTime() + tb->period;
		}
		nb = tb->next;
		bnode_Release(tb);	/* delete may occur here */
	    } else
		nb = tb->next;
	}

	if (code < 0) {
	    /* signalled, probably by incoming signal */
	    while (1) {
		options = WNOHANG;
		bnode_waiting = options | 0x800000;
		code = waitpid((pid_t) - 1, &status, options);
		bnode_waiting = 0;
		if (code == 0 || code == -1)
		    break;	/* all done */
		/* otherwise code has a process id, which we now search for */
		for (tp = allProcs; tp; tp = tp->next)
		    if (tp->pid == code)
			break;
		if (tp) {
		    /* found the pid */
		    tb = tp->bnode;
		    bnode_Hold(tb);

		    /* count restarts in last 10 seconds */
		    if (temp > tb->rsTime + 30) {
			/* it's been 10 seconds we've been counting */
			tb->rsTime = temp;
			tb->rsCount = 0;
		    }

		    if (WIFSIGNALED(status) == 0) {
			/* exited, not signalled */
			tp->lastExit = WEXITSTATUS(status);
			tp->lastSignal = 0;
			if (tp->lastExit) {
			    tb->errorCode = tp->lastExit;
			    tb->lastErrorExit = FT_ApproxTime();
			    RememberProcName(tp);
			    tb->errorSignal = 0;
			}
			if (tp->coreName)
			    bozo_Log("%s:%s exited with code %d\n", tb->name,
				     tp->coreName, tp->lastExit);
			else
			    bozo_Log("%s exited with code %d\n", tb->name,
				     tp->lastExit);
		    } else {
			/* Signal occurred, perhaps spurious due to shutdown request.
			 * If due to a shutdown request, don't overwrite last error
			 * information.
			 */
			tp->lastSignal = WTERMSIG(status);
			tp->lastExit = 0;
			if (tp->lastSignal != SIGQUIT
			    && tp->lastSignal != SIGTERM
			    && tp->lastSignal != SIGKILL) {
			    tb->errorSignal = tp->lastSignal;
			    tb->lastErrorExit = FT_ApproxTime();
			    RememberProcName(tp);

			    osi_Trace_Bozo_Event(osi_Trace_Bozo_ProbeId(BnodeProcEvents_crash),
						 osi_Trace_Args3(tp->pid, 
								 tp->lastSignal,
								 WCOREDUMP(status)));
			    (void)bnode_crash_mail_msg(tp);
			}
			if (tp->coreName)
			    bozo_Log("%s:%s exited on signal %d%s\n",
				     tb->name, tp->coreName, tp->lastSignal,
				     WCOREDUMP(status) ? " (core dumped)" :
				     "");
			else
			    bozo_Log("%s exited on signal %d%s\n", tb->name,
				     tp->lastSignal,
				     WCOREDUMP(status) ? " (core dumped)" :
				     "");
			SaveCore(tb, tp);
		    }
		    tb->lastAnyExit = FT_ApproxTime();


#if defined(OSI_TRACE_ENABLED)
		    /*
		     * attempt to unregister any kernel state
		     * for the dead process
		     */
		    {
			osi_result res;
			osi_trace_gen_id_t gen_id;
			osi_trace_generator_address_t gen_addr;

			gen_addr.pid = tp->pid;
			gen_addr.programType = 0;

			res = osi_trace_gen_rgy_get_by_addr(&gen_addr,
							    &gen_id);
			if (OSI_RESULT_OK(res)) {
			    osi_trace_gen_rgy_unregister(gen_id);
			    osi_trace_gen_rgy_put(gen_id);
			}
		    }
#endif /* OSI_TRACE_ENABLED */


		    if (tb->notifier) {
			bozo_Log("BNODE: Notifier %s will be called\n",
				 tb->notifier);
			hdl_notifier(tp);
		    }
		    BOP_PROCEXIT(tb, tp);

		    bnode_Check(tb);
		    if (tb->rsCount++ > 10) {
			/* 10 in 10 seconds */
			tb->flags |= BNODE_ERRORSTOP;
			bnode_SetGoal(tb, BSTAT_SHUTDOWN);
			bozo_Log
			    ("BNODE '%s' repeatedly failed to start, perhaps missing executable.\n",
			     tb->name);
		    }
		    bnode_Release(tb);	/* bnode delete can happen here */
		    DeleteProc(tp);
		} else
		    bnode_stats.weirdPids++;
	    }
	}
    }
}

static afs_int32
SendNotifierData(register int fd, register struct bnode_proc *tp)
{
    register struct bnode *tb = tp->bnode;
    char buffer[1000], *bufp = buffer, *buf1;
    register int len;

    /*
     * First sent out the bnode_proc struct
     */
    (void)sprintf(bufp, "BEGIN bnode_proc\n");
    bufp += strlen(bufp);
    (void)sprintf(bufp, "comLine: %s\n", tp->comLine);
    bufp += strlen(bufp);
    if (!(buf1 = tp->coreName))
	buf1 = "(null)";
    (void)sprintf(bufp, "coreName: %s\n", buf1);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "pid: %ld\n", tp->pid);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "lastExit: %ld\n", tp->lastExit);
    bufp += strlen(bufp);
#ifdef notdef
    (void)sprintf(bufp, "lastSignal: %ld\n", tp->lastSignal);
    bufp += strlen(bufp);
#endif
    (void)sprintf(bufp, "flags: %ld\n", tp->flags);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "END bnode_proc\n");
    bufp += strlen(bufp);
    len = (int)(bufp - buffer);
    if (write(fd, buffer, len) < 0) {
	return -1;
    }

    /*
     * Now sent out the bnode struct
     */
    bufp = buffer;
    (void)sprintf(bufp, "BEGIN bnode\n");
    bufp += strlen(bufp);
    (void)sprintf(bufp, "name: %s\n", tb->name);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "rsTime: %ld\n", tb->rsTime);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "rsCount: %ld\n", tb->rsCount);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "procStartTime: %ld\n", tb->procStartTime);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "procStarts: %ld\n", tb->procStarts);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "lastAnyExit: %ld\n", tb->lastAnyExit);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "lastErrorExit: %ld\n", tb->lastErrorExit);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "errorCode: %ld\n", tb->errorCode);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "errorSignal: %ld\n", tb->errorSignal);
    bufp += strlen(bufp);
/*
    (void) sprintf(bufp, "lastErrorName: %s\n", tb->lastErrorName);
    bufp += strlen(bufp);
*/
    (void)sprintf(bufp, "goal: %d\n", tb->goal);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "END bnode\n");
    bufp += strlen(bufp);
    len = (int)(bufp - buffer);
    if (write(fd, buffer, len) < 0) {
	return -1;
    }
}

int
hdl_notifier(struct bnode_proc *tp)
{
#ifndef AFS_NT40_ENV		/* NT notifier callout not yet implemented */
    int code, pid, status;
    struct stat tstat;

    if (stat(tp->bnode->notifier, &tstat)) {
	bozo_Log("BNODE: Failed to find notifier '%s'; ignored\n",
		 tp->bnode->notifier);
	return (1);
    }
    if ((pid = fork()) == 0) {
	FILE *fout;
	struct bnode *tb = tp->bnode;
	int ec;

#if defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_SGI51_ENV)
	ec = setsid();
#elif defined(AFS_DARWIN90_ENV)
	ec = setpgid(0, 0);
#elif defined(AFS_LINUX20_ENV) || defined(AFS_AIX_ENV)  
	ec = setpgrp();
#else
	ec = setpgrp(0, 0);
#endif
	fout = popen(tb->notifier, "w");
	if (fout == NULL) {
	    bozo_Log("BNODE: Failed to find notifier '%s'; ignored\n",
		     tb->notifier);
	    perror(tb->notifier);
	    exit(1);
	}
	code = SendNotifierData(fileno(fout), tp);
	pclose(fout);
	exit(0);
    } else if (pid < 0) {
	bozo_Log("Failed to fork creating process to handle notifier '%s'\n",
		 tp->bnode->notifier);
	return -1;
    }
#endif /* AFS_NT40_ENV */
    return (0);
}

/* Called by IOMGR at low priority on IOMGR's stack shortly after a SIGCHLD
 * occurs.  Wakes up bproc do redo things */
int
bnode_SoftInt(int asignal)
{
    IOMGR_Cancel(bproc_pid);
    return 0;
}

/* Called at signal interrupt level; queues function to be called
 * when IOMGR runs again.
 */
void
bnode_Int(int asignal)
{
    extern void bozo_ShutdownAndExit();

    if (asignal == SIGQUIT) {
	IOMGR_SoftSig(bozo_ShutdownAndExit, (char *)asignal);
    } else {
	IOMGR_SoftSig(bnode_SoftInt, (char *)asignal);
    }
}


/* intialize the whole system */
int
bnode_Init()
{
    PROCESS junk;
    register afs_int32 code;
    struct sigaction newaction;
    static initDone = 0;

    if (initDone)
	return 0;
    initDone = 1;
    memset(&bnode_stats, 0, sizeof(bnode_stats));
    LWP_InitializeProcessSupport(1, &junk);	/* just in case */
    IOMGR_Initialize();
    code = LWP_CreateProcess(bproc, BNODE_LWP_STACKSIZE,
			     /* priority */ 1, (void *) /* parm */ 0,
			     "bnode-manager", &bproc_pid);
    if (code)
	return code;
    memset((char *)&newaction, 0, sizeof(newaction));
    newaction.sa_handler = bnode_Int;
    code = sigaction(SIGCHLD, &newaction, NULL);
    if (code)
	return errno;
    code = sigaction(SIGQUIT, &newaction, NULL);
    if (code)
	return errno;
    return code;
}

/* free token list returned by parseLine */
int
bnode_FreeTokens(register struct bnode_token *alist)
{
    register struct bnode_token *nlist;
    for (; alist; alist = nlist) {
	nlist = alist->next;
	free(alist->key);
	free(alist);
    }
    return 0;
}

static int
space(int x)
{
    if (x == 0 || x == ' ' || x == '\t' || x == '\n')
	return 1;
    else
	return 0;
}

int
bnode_ParseLine(char *aline, struct bnode_token **alist)
{
    char tbuffer[256];
    register char *tptr;
    int inToken;
    struct bnode_token *first, *last;
    register struct bnode_token *ttok;
    register int tc;

    inToken = 0;		/* not copying token chars at start */
    first = (struct bnode_token *)0;
    last = (struct bnode_token *)0;
    while (1) {
	tc = *aline++;
	if (tc == 0 || space(tc)) {	/* terminating null gets us in here, too */
	    if (inToken) {
		inToken = 0;	/* end of this token */
		*tptr++ = 0;
		ttok =
		    (struct bnode_token *)malloc(sizeof(struct bnode_token));
		ttok->next = (struct bnode_token *)0;
		ttok->key = (char *)malloc(strlen(tbuffer) + 1);
		strcpy(ttok->key, tbuffer);
		if (last) {
		    last->next = ttok;
		    last = ttok;
		} else
		    last = ttok;
		if (!first)
		    first = ttok;
	    }
	} else {
	    /* an alpha character */
	    if (!inToken) {
		tptr = tbuffer;
		inToken = 1;
	    }
	    if (tptr - tbuffer >= sizeof(tbuffer))
		return -1;	/* token too long */
	    *tptr++ = tc;
	}
	if (tc == 0) {
	    /* last token flushed 'cause space(0) --> true */
	    if (last)
		last->next = (struct bnode_token *)0;
	    *alist = first;
	    return 0;
	}
    }
}

#define	MAXVARGS	    128
int
bnode_NewProc(struct bnode *abnode, char *aexecString, char *coreName,
	      struct bnode_proc **aproc)
{
    struct bnode_token *tlist, *tt;
    afs_int32 code;
    struct bnode_proc *tp;
    pid_t cpid;
    char *argv[MAXVARGS];
    int i;

    code = bnode_ParseLine(aexecString, &tlist);	/* try parsing first */
    if (code)
	return code;
    tp = (struct bnode_proc *)malloc(sizeof(struct bnode_proc));
    memset(tp, 0, sizeof(struct bnode_proc));
    tp->next = allProcs;
    tp->bnode = abnode;
    tp->comLine = aexecString;
    tp->coreName = coreName;	/* may be null */
    abnode->procStartTime = FT_ApproxTime();
    abnode->procStarts++;

    /* convert linked list of tokens into argv structure */
    for (tt = tlist, i = 0; i < (MAXVARGS - 1) && tt; tt = tt->next, i++) {
	argv[i] = tt->key;
    }
    argv[i] = NULL;		/* null-terminated */

    cpid = spawnprocve(argv[0], argv, environ, -1);

    osi_audit(BOSSpawnProcEvent, 0, AUD_STR, aexecString, AUD_END);
    osi_Trace_Bozo_Event(osi_Trace_Bozo_ProbeId(BnodeProcEvents_start),
			 osi_Trace_Args1(cpid));

    if (cpid == (pid_t) - 1) {
	bozo_Log("Failed to spawn process for bnode '%s'\n", abnode->name);
	bnode_FreeTokens(tlist);
	free(tp);
	return errno;
    }

    bnode_FreeTokens(tlist);
    allProcs = tp;
    *aproc = tp;
    tp->pid = cpid;
    tp->flags = BPROC_STARTED;
    tp->flags &= ~BPROC_EXITED;

    (void)bnode_start_mail_msg(tp);

    bnode_Check(abnode);
    return 0;
}

int
bnode_StopProc(register struct bnode_proc *aproc, int asignal)
{
    register int code;
    if (!(aproc->flags & BPROC_STARTED) || (aproc->flags & BPROC_EXITED))
	return BZNOTACTIVE;

    osi_audit(BOSStopProcEvent, 0, AUD_STR, (aproc ? aproc->comLine : NULL),
	      AUD_END);
    osi_Trace_Bozo_Event(osi_Trace_Bozo_ProbeId(BnodeProcEvents_stop),
			 osi_Trace_Args2(aproc->pid, asignal));

    (void)bnode_stop_mail_msg(aproc);

    code = kill(aproc->pid, asignal);
    bnode_Check(aproc->bnode);
    return code;
}

int
bnode_Deactivate(register struct bnode *abnode)
{
    register struct bnode **pb, *tb;
    struct bnode *nb;
    if (!(abnode->flags & BNODE_ACTIVE))
	return BZNOTACTIVE;
    for (pb = &allBnodes, tb = *pb; tb; tb = nb) {
	nb = tb->next;
	if (tb == abnode) {
	    *pb = nb;
	    tb->flags &= ~BNODE_ACTIVE;
	    return 0;
	}
    }
    return BZNOENT;
}

#if defined(OSI_TRACE_ENABLED)
osi_static osi_result
bnode_start_mail_msg(struct bnode_proc * aproc)
{
    osi_trace_mail_msg_bozo_proc_start_t * req;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_bozo_proc_start_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_bozo_proc_start_t * ) msg->body;
    req->pid = aproc->pid;
    req->spare = 0;

    code = osi_trace_mail_prepare_send(msg, 
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER, 
				       xid,
				       0,
				       OSI_TRACE_MAIL_MSG_BOZO_PROC_START);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

osi_static osi_result
bnode_stop_mail_msg(struct bnode_proc * aproc)
{
    osi_trace_mail_msg_bozo_proc_stop_t * req;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_bozo_proc_stop_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_bozo_proc_stop_t * ) msg->body;
    req->pid = aproc->pid;
    req->spare = 0;

    code = osi_trace_mail_prepare_send(msg, 
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER, 
				       xid,
				       0,
				       OSI_TRACE_MAIL_MSG_BOZO_PROC_STOP);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

osi_static osi_result
bnode_crash_mail_msg(struct bnode_proc * aproc)
{
    osi_trace_mail_msg_bozo_proc_crash_t * req;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_bozo_proc_crash_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_bozo_proc_crash_t * ) msg->body;
    req->pid = aproc->pid;
    req->signal = aproc->lastSignal;
    req->reason = 0;
    req->spare = 0;

    code = osi_trace_mail_prepare_send(msg, 
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER, 
				       xid,
				       0,
				       OSI_TRACE_MAIL_MSG_BOZO_PROC_CRASH);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

#endif /* OSI_TRACE_ENABLED */
