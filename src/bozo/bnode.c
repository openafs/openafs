/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <afs/procmgmt.h>
#include <roken.h>

#include <stddef.h>

#include <lwp.h>
#include <rx/rx.h>
#include <afs/audit.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <opr/queue.h>

#include "bnode.h"
#include "bnode_internal.h"
#include "bosprototypes.h"

#ifndef WCOREDUMP
#define WCOREDUMP(x) ((x) & 0200)
#endif

#define BNODE_LWP_STACKSIZE	(16 * 1024)
#define BNODE_ERROR_COUNT_MAX   16   /* maximum number of retries */
#define BNODE_ERROR_DELAY_MAX   60   /* maximum retry delay (seconds) */

static PROCESS bproc_pid;	/* pid of waker-upper */
static struct opr_queue allBnodes;	/**< List of all bnodes */
static struct opr_queue allProcs;	/**< List of all processes for which we're waiting */
static struct opr_queue allTypes;	/**< List of all registered type handlers */

static struct bnode_stats {
    int weirdPids;
} bnode_stats;

extern const char *DoCore;
extern const char *DoPidFiles;
#ifndef AFS_NT40_ENV
extern char **environ;		/* env structure */
#endif

int hdl_notifier(struct bnode_proc *tp);

/* Remember the name of the process, if any, that failed last */
static void
RememberProcName(struct bnode_proc *ap)
{
    struct bnode *tbnodep;

    tbnodep = ap->bnode;
    if (tbnodep->lastErrorName) {
	free(tbnodep->lastErrorName);
	tbnodep->lastErrorName = NULL;
    }
    if (ap->coreName)
	tbnodep->lastErrorName = strdup(ap->coreName);
}

/* utility for use by BOP_HASCORE functions to determine where a core file might
 * be stored.
 */
int
bnode_CoreName(struct bnode *abnode, char *acoreName, char *abuffer)
{
    if (DoCore) {
	strcpy(abuffer, DoCore);
	strcat(abuffer, "/");
	strcat(abuffer, AFSDIR_CORE_FILE);
    } else
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
SaveCore(struct bnode *abnode, struct bnode_proc
	 *aproc)
{
    char tbuffer[256];
    struct stat tstat;
    afs_int32 code = 0;
    char *corefile = NULL;
#ifdef BOZO_SAVE_CORES
    struct timeval Start;
    struct tm *TimeFields;
    char FileName[256];
#endif

    /* Linux always appends the PID to core dumps from threaded processes, so
     * we have to scan the directory to find core files under another name. */
    if (DoCore) {
	strcpy(tbuffer, DoCore);
	strcat(tbuffer, "/");
	strcat(tbuffer, AFSDIR_CORE_FILE);
    } else
	code = stat(AFSDIR_SERVER_CORELOG_FILEPATH, &tstat);
    if (code) {
        DIR *logdir;
        struct dirent *file;
        unsigned long pid;
	const char *coredir = AFSDIR_LOGS_DIR;

	if (DoCore)
	  coredir = DoCore;

	logdir = opendir(coredir);
        if (logdir == NULL)
            return;
        while ((file = readdir(logdir)) != NULL) {
            if (strncmp(file->d_name, "core.", 5) != 0)
                continue;
            pid = atol(file->d_name + 5);
            if (pid == aproc->pid) {
                int r;

                r = asprintf(&corefile, "%s/%s", coredir, file->d_name);
                if (r < 0 || corefile == NULL) {
                    closedir(logdir);
                    return;
                }
                code = 0;
                break;
            }
        }
        closedir(logdir);
    } else {
	corefile = strdup(tbuffer);
    }
    if (code)
	return;

    bnode_CoreName(abnode, aproc->coreName, tbuffer);
#ifdef BOZO_SAVE_CORES
    FT_GetTimeOfDay(&Start, 0);
    TimeFields = localtime(&Start.tv_sec);
    sprintf(FileName, "%s.%d%02d%02d%02d%02d%02d", tbuffer,
	    TimeFields->tm_year + 1900, TimeFields->tm_mon + 1, TimeFields->tm_mday,
	    TimeFields->tm_hour, TimeFields->tm_min, TimeFields->tm_sec);
    strcpy(tbuffer, FileName);
#endif
    rk_rename(corefile, tbuffer);
    free(corefile);
}

int
bnode_GetString(struct bnode *abnode, char **adesc)
{
    return BOP_GETSTRING(abnode, adesc);
}

int
bnode_GetParm(struct bnode *abnode, afs_int32 aindex, char **aparm)
{
    return BOP_GETPARM(abnode, aindex, aparm);
}

int
bnode_GetStat(struct bnode *abnode, afs_int32 * astatus)
{
    return BOP_GETSTAT(abnode, astatus);
}

int
bnode_GetNotifier(struct bnode *abnode, char **anotifier)
{
    if (abnode->notifier == NULL)
	return BZNOENT;
    *anotifier = strdup(abnode->notifier);
    if (*anotifier == NULL)
	return BZIO;
    return 0;
}

int
bnode_RestartP(struct bnode *abnode)
{
    return BOP_RESTARTP(abnode);
}

static int
bnode_Check(struct bnode *abnode)
{
    if (abnode->flags & BNODE_WAIT) {
	abnode->flags &= ~BNODE_WAIT;
	LWP_NoYieldSignal(abnode);
    }
    return 0;
}

/* tell if an instance has a core file */
int
bnode_HasCore(struct bnode *abnode)
{
    return BOP_HASCORE(abnode);
}

/* wait until bnode_Check() gets called for this bnode */
static void
bnode_Wait(struct bnode *abnode)
{
    abnode->flags |= BNODE_WAIT;
    LWP_WaitProcess(abnode);
}

/* wait for all bnodes to stabilize */
int
bnode_WaitAll(void)
{
    struct opr_queue *cursor;
    afs_int32 code;
    afs_int32 stat;

  retry:
    for (opr_queue_Scan(&allBnodes, cursor)) {
	struct bnode *tb = opr_queue_Entry(cursor, struct bnode, q);

	bnode_Hold(tb);
	code = BOP_GETSTAT(tb, &stat);
	if (code) {
	    bnode_Release(tb);
	    return code;
	}
	if (stat != tb->goal) {
	    bnode_Wait(tb);
	    bnode_Release(tb);
	    goto retry;
	}
	bnode_Release(tb);
    }
    return 0;
}

/* wait until bnode status is correct */
int
bnode_WaitStatus(struct bnode *abnode, int astatus)
{
    afs_int32 code;
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
	bnode_Wait(abnode);
    }
}

int
bnode_ResetErrorCount(struct bnode *abnode)
{
    abnode->errorStopCount = 0;
    abnode->errorStopDelay = 0;
    return 0;
}

int
bnode_SetStat(struct bnode *abnode, int agoal)
{
    abnode->goal = agoal;
    bnode_Check(abnode);
    BOP_SETSTAT(abnode, agoal);
    abnode->flags &= ~BNODE_ERRORSTOP;
    return 0;
}

int
bnode_SetGoal(struct bnode *abnode, int agoal)
{
    abnode->goal = agoal;
    bnode_Check(abnode);
    return 0;
}

int
bnode_SetFileGoal(struct bnode *abnode, int agoal)
{
    if (abnode->fileGoal == agoal)
	return 0;		/* already done */
    abnode->fileGoal = agoal;
    WriteBozoFile(0);
    return 0;
}

/* apply a function to all bnodes in the system */
int
bnode_ApplyInstance(int (*aproc) (struct bnode *tb, void *), void *arock)
{
    struct opr_queue *cursor, *store;
    afs_int32 code;

    for (opr_queue_ScanSafe(&allBnodes, cursor, store)) {
	struct bnode *tb = opr_queue_Entry(cursor, struct bnode, q);
	code = (*aproc) (tb, arock);
	if (code)
	    return code;
    }
    return 0;
}

struct bnode *
bnode_FindInstance(char *aname)
{
    struct opr_queue *cursor;

    for (opr_queue_Scan(&allBnodes, cursor)) {
	struct bnode *tb = opr_queue_Entry(cursor, struct bnode, q);

	if (!strcmp(tb->name, aname))
	    return tb;
    }
    return NULL;
}

static struct bnode_type *
FindType(char *aname)
{
    struct opr_queue *cursor;

    for (opr_queue_Scan(&allTypes, cursor)) {
	struct bnode_type *tt = opr_queue_Entry(cursor, struct bnode_type, q);

	if (!strcmp(tt->name, aname))
	    return tt;
    }
    return NULL;
}

int
bnode_Register(char *atype, struct bnode_ops *aprocs, int anparms)
{
    struct opr_queue *cursor;
    struct bnode_type *tt = NULL;

    for (opr_queue_Scan(&allTypes, cursor), tt = NULL) {
	tt = opr_queue_Entry(cursor, struct bnode_type, q);
	if (!strcmp(tt->name, atype))
	    break;
    }
    if (!tt) {
	tt = calloc(1, sizeof(struct bnode_type));
        opr_queue_Init(&tt->q);
	opr_queue_Prepend(&allTypes, &tt->q);
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
    struct bnode *tb;

    tb = bnode_FindInstance(ainstance);
    if (!tb)
	return BZNOENT;

    return bnode_Delete(tb);
}

int
bnode_Hold(struct bnode *abnode)
{
    abnode->refCount++;
    return 0;
}

int
bnode_Release(struct bnode *abnode)
{
    abnode->refCount--;
    if (abnode->refCount == 0 && abnode->flags & BNODE_DELETE) {
	abnode->flags &= ~BNODE_DELETE;	/* we're going for it */
	bnode_Delete(abnode);
    }
    return 0;
}

int
bnode_Delete(struct bnode *abnode)
{
    afs_int32 code;
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
    opr_queue_Remove(&abnode->q);
    free(abnode->name);		/* do this first, since bnode fields may be bad after BOP_DELETE */
    code = BOP_DELETE(abnode);	/* don't play games like holding over this one */
    WriteBozoFile(0);
    return code;
}

/* function to tell if there's a timeout coming up */
int
bnode_PendingTimeout(struct bnode *abnode)
{
    return (abnode->flags & BNODE_NEEDTIMEOUT);
}

/* function called to set / clear periodic bnode wakeup times */
int
bnode_SetTimeout(struct bnode *abnode, afs_int32 atimeout)
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
bnode_InitBnode(struct bnode *abnode, struct bnode_ops *abnodeops,
		char *aname)
{
    /* format the bnode properly */
    memset(abnode, 0, sizeof(struct bnode));
    opr_queue_Init(&abnode->q);
    abnode->ops = abnodeops;
    abnode->name = strdup(aname);
    if (!abnode->name)
	return ENOMEM;
    abnode->flags = BNODE_ACTIVE;
    abnode->fileGoal = BSTAT_NORMAL;
    abnode->goal = BSTAT_SHUTDOWN;

    /* put the bnode at the end of the list so we write bnode file in same order */
    opr_queue_Append(&allBnodes, &abnode->q);

    return 0;
}

/* bnode lwp executes this code repeatedly */
static void *
bproc(void *unused)
{
    afs_int32 code;
    struct bnode *tb;
    afs_int32 temp;
    struct opr_queue *cursor, *store;
    struct bnode_proc *tp;
    int options;		/* must not be register */
    struct timeval tv;
    int setAny;
    int status;

    while (1) {
	/* first figure out how long to sleep for */
	temp = 0x7fffffff;	/* afs_int32 time; maxint doesn't work in select */
	setAny = 0;
	for (opr_queue_Scan(&allBnodes, cursor)) {
	    tb = opr_queue_Entry(cursor, struct bnode, q);
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
	for (opr_queue_ScanSafe(&allBnodes, cursor, store)) {
	    tb = opr_queue_Entry(cursor, struct bnode, q);
	    if ((tb->flags & BNODE_NEEDTIMEOUT) && temp > tb->nextTimeout) {
		bnode_Hold(tb);
		BOP_TIMEOUT(tb);
		bnode_Check(tb);
		if (tb->flags & BNODE_NEEDTIMEOUT) {	/* check again, BOP_TIMEOUT could change */
		    tb->nextTimeout = FT_ApproxTime() + tb->period;
		}
		bnode_Release(tb);	/* delete may occur here */
	    }
	}

	if (code < 0) {
	    /* signalled, probably by incoming signal */
	    while (1) {
		options = WNOHANG;
		code = waitpid((pid_t) - 1, &status, options);
		if (code == 0 || code == -1)
		    break;	/* all done */
		/* otherwise code has a process id, which we now search for */
		for (tp = NULL, opr_queue_Scan(&allProcs, cursor), tp = NULL) {
		    tp = opr_queue_Entry(cursor, struct bnode_proc, q);

		    if (tp->pid == code)
			break;
		}
		if (tp) {
		    /* found the pid */
		    tb = tp->bnode;
		    bnode_Hold(tb);

		    /* count restarts in last 30 seconds */
		    if (temp > tb->rsTime + 30) {
			/* it's been 30 seconds we've been counting */
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

		    if (tb->notifier) {
			bozo_Log("BNODE: Notifier %s will be called\n",
				 tb->notifier);
			hdl_notifier(tp);
		    }

		    if (tb->goal && tb->rsCount++ > 10) {
			/* 10 in 30 seconds */
			if (tb->errorStopCount >= BNODE_ERROR_COUNT_MAX) {
			    tb->errorStopDelay = 0;	/* max reached, give up. */
			} else {
			    tb->errorStopCount++;
			    if (!tb->errorStopDelay) {
				tb->errorStopDelay = 1;   /* wait a second, then retry */
			    } else {
			        tb->errorStopDelay *= 2;  /* ramp up the retry delays */
			    }
			    if (tb->errorStopDelay > BNODE_ERROR_DELAY_MAX) {
			        tb->errorStopDelay = BNODE_ERROR_DELAY_MAX; /* cap the delay */
			    }
			}
			tb->flags |= BNODE_ERRORSTOP;
			bnode_SetGoal(tb, BSTAT_SHUTDOWN);
			bozo_Log
			    ("BNODE '%s' repeatedly failed to start, perhaps missing executable.\n",
			     tb->name);
		    }
		    BOP_PROCEXIT(tb, tp);
		    bnode_Check(tb);
		    bnode_Release(tb);	/* bnode delete can happen here */
		    opr_queue_Remove(&tp->q);
		    free(tp);
		} else
		    bnode_stats.weirdPids++;
	    }
	}
    }
    AFS_UNREACHED(return(NULL));
}

static afs_int32
SendNotifierData(FILE *fp, struct bnode_proc *tp)
{
    struct bnode *tb = tp->bnode;

    /*
     * First sent out the bnode_proc struct
     */
    fprintf(fp, "BEGIN bnode_proc\n");
    fprintf(fp, "comLine: %s\n", tp->comLine);
    fprintf(fp, "coreName: %s\n", (tp->coreName == NULL ? "(null)" : tp->coreName));
    fprintf(fp, "pid: %ld\n", afs_printable_int32_ld(tp->pid));
    fprintf(fp, "lastExit: %ld\n", afs_printable_int32_ld(tp->lastExit));
    fprintf(fp, "flags: %ld\n", afs_printable_int32_ld(tp->flags));
    fprintf(fp, "END bnode_proc\n");
    if (ferror(fp) != 0)
	return -1;

    /*
     * Now sent out the bnode struct
     */
    fprintf(fp, "BEGIN bnode\n");
    fprintf(fp, "name: %s\n", tb->name);
    fprintf(fp, "rsTime: %ld\n", afs_printable_int32_ld(tb->rsTime));
    fprintf(fp, "rsCount: %ld\n", afs_printable_int32_ld(tb->rsCount));
    fprintf(fp, "procStartTime: %ld\n", afs_printable_int32_ld(tb->procStartTime));
    fprintf(fp, "procStarts: %ld\n", afs_printable_int32_ld(tb->procStarts));
    fprintf(fp, "lastAnyExit: %ld\n", afs_printable_int32_ld(tb->lastAnyExit));
    fprintf(fp, "lastErrorExit: %ld\n", afs_printable_int32_ld(tb->lastErrorExit));
    fprintf(fp, "errorCode: %ld\n", afs_printable_int32_ld(tb->errorCode));
    fprintf(fp, "errorSignal: %ld\n", afs_printable_int32_ld(tb->errorSignal));
    fprintf(fp, "goal: %d\n", tb->goal);
    fprintf(fp, "END bnode\n");
    if (ferror(fp) != 0)
	return -1;

    return 0;
}

int
hdl_notifier(struct bnode_proc *tp)
{
#ifndef AFS_NT40_ENV		/* NT notifier callout not yet implemented */
    int pid;
    struct stat tstat;

    if (stat(tp->bnode->notifier, &tstat)) {
	bozo_Log("BNODE: Failed to find notifier '%s'; ignored\n",
		 tp->bnode->notifier);
	return (1);
    }
    if ((pid = fork()) == 0) {
	FILE *fout;
	struct bnode *tb = tp->bnode;

#if defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
	setsid();
#elif defined(AFS_DARWIN90_ENV)
	setpgid(0, 0);
#elif defined(AFS_LINUX_ENV) || defined(AFS_AIX_ENV)
	setpgrp();
#else
	setpgrp(0, 0);
#endif
	fout = popen(tb->notifier, "w");
	if (fout == NULL) {
	    bozo_Log("BNODE: Failed to find notifier '%s'; ignored\n",
		     tb->notifier);
	    perror(tb->notifier);
	    exit(1);
	}
	if (SendNotifierData(fout, tp) != 0)
	    bozo_Log("BNODE: Failed to send notifier data to '%s'\n",
		     tb->notifier);
	if (pclose(fout) < 0)
	    bozo_Log("BNODE: Failed to close notifier pipe to '%s', %d\n",
		     tb->notifier, errno);
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
void *
bnode_SoftInt(void *param)
{
    /* int asignal = (int) param; */

    IOMGR_Cancel(bproc_pid);
    return NULL;
}

/* Called at signal interrupt level; queues function to be called
 * when IOMGR runs again.
 */
void
bnode_Int(int asignal)
{
    if (asignal == SIGQUIT || asignal == SIGTERM) {
	IOMGR_SoftSig(bozo_ShutdownAndExit, (void *)(intptr_t)asignal);
    } else {
	IOMGR_SoftSig(bnode_SoftInt, (void *)(intptr_t)asignal);
    }
}


/* intialize the whole system */
int
bnode_Init(void)
{
    PROCESS junk;
    afs_int32 code;
    struct sigaction newaction;
    static int initDone = 0;

    if (initDone)
	return 0;
    initDone = 1;
    opr_queue_Init(&allTypes);
    opr_queue_Init(&allProcs);
    opr_queue_Init(&allBnodes);
    memset(&bnode_stats, 0, sizeof(bnode_stats));
    LWP_InitializeProcessSupport(1, &junk);	/* just in case */
    IOMGR_Initialize();
    code = LWP_CreateProcess(bproc, BNODE_LWP_STACKSIZE,
			     /* priority */ 1, (void *) /* parm */ 0,
			     "bnode-manager", &bproc_pid);
    if (code)
	return code;
    memset(&newaction, 0, sizeof(newaction));
    newaction.sa_handler = bnode_Int;
    code = sigaction(SIGCHLD, &newaction, NULL);
    if (code)
	return errno;
    code = sigaction(SIGQUIT, &newaction, NULL);
    if (code)
	return errno;
    code = sigaction(SIGTERM, &newaction, NULL);
    if (code)
	return errno;
    return code;
}

/* free token list returned by parseLine */
int
bnode_FreeTokens(struct bnode_token *alist)
{
    struct bnode_token *nlist;
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
    char *tptr = NULL;
    int inToken;
    struct bnode_token *first, *last;
    struct bnode_token *ttok;
    int tc;

    inToken = 0;		/* not copying token chars at start */
    first = (struct bnode_token *)0;
    last = (struct bnode_token *)0;
    while (1) {
	tc = *aline++;
	if (tc == 0 || space(tc)) {	/* terminating null gets us in here, too */
	    if (inToken) {
		inToken = 0;	/* end of this token */
		*tptr++ = 0;
		ttok = malloc(sizeof(struct bnode_token));
		ttok->next = (struct bnode_token *)0;
		ttok->key = strdup(tbuffer);
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
    tp = calloc(1, sizeof(struct bnode_proc));
    opr_queue_Init(&tp->q);
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

    if (cpid == (pid_t) - 1) {
	bozo_Log("Failed to spawn process for bnode '%s'\n", abnode->name);
	bnode_FreeTokens(tlist);
	free(tp);
	return errno;
    }
    bozo_Log("%s started pid %ld: %s\n", abnode->name, cpid, aexecString);

    bnode_FreeTokens(tlist);
    opr_queue_Prepend(&allProcs, &tp->q);
    *aproc = tp;
    tp->pid = cpid;
    tp->flags = BPROC_STARTED;
    tp->flags &= ~BPROC_EXITED;
    BOP_PROCSTARTED(abnode, tp);
    bnode_Check(abnode);
    return 0;
}

int
bnode_StopProc(struct bnode_proc *aproc, int asignal)
{
    int code;
    if (!(aproc->flags & BPROC_STARTED) || (aproc->flags & BPROC_EXITED))
	return BZNOTACTIVE;

    osi_audit(BOSStopProcEvent, 0, AUD_STR, (aproc ? aproc->comLine : NULL),
	      AUD_END);

    code = kill(aproc->pid, asignal);
    bnode_Check(aproc->bnode);
    return code;
}
