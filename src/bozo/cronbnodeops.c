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


#include <sys/types.h>
#include <sys/stat.h>
#include <lwp.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include "bnode.h"
#include "bosprototypes.h"

struct bnode *cron_create(char *, char *, char *, char *, char *, char *);
static int cron_hascore(struct bnode *bnode);
static int cron_restartp(struct bnode *bnode);
static int cron_delete(struct bnode *bnode);
static int cron_timeout(struct bnode *bnode);
static int cron_getstat(struct bnode *bnode, afs_int32 *status);
static int cron_setstat(struct bnode *bnode, afs_int32 status);
static int cron_procstarted(struct bnode *bnode, struct bnode_proc *proc);
static int cron_procexit(struct bnode *bnode, struct bnode_proc *proc);
static int cron_getstring(struct bnode *bnode, char *abuffer, afs_int32 alen);
static int cron_getparm(struct bnode *bnode, afs_int32, char *, afs_int32);

#define	SDTIME		60	/* time in seconds given to a process to evaporate */

struct bnode_ops cronbnode_ops = {
    cron_create,
    cron_timeout,
    cron_getstat,
    cron_setstat,
    cron_delete,
    cron_procexit,
    cron_getstring,
    cron_getparm,
    cron_restartp,
    cron_hascore,
    cron_procstarted,
};

struct cronbnode {
    struct bnode b;
    afs_int32 zapTime;		/* time we sent a sigterm */
    char *command;
    char *whenString;		/* string rep. of when to run */
    struct bnode_proc *proc;
    afs_int32 lastStart;	/* time last started process */
    afs_int32 nextRun;		/* next time to run, if no proc running */
    struct ktime whenToRun;	/* high-level rep of when should we run this guy */
    afs_int32 when;		/* computed at creation time and procexit time */
    char everRun;		/* true if ever ran */
    char waitingForShutdown;	/* have we started any shutdown procedure? */
    char running;		/* is process running? */
    char killSent;		/* have we tried sigkill signal? */
};

static int
cron_hascore(struct bnode *abnode)
{
    char tbuffer[256];

    bnode_CoreName(abnode, NULL, tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;
    else
	return 0;
}

/* run at creation or after process exit.  figures out if we're all done (if a
    one shot run) or when we should run again.  Sleeps until we should run again.
    Note that the computation of when we should run again is made in procexit
    and/or create procs.  This guy only schedules the sleep */
int
ScheduleCronBnode(struct cronbnode *abnode)
{
    afs_int32 code;
    afs_int32 temp;
    struct bnode_proc *tp;

    /* If this proc is shutdown, tell bproc() to no longer run this job */
    if (abnode->b.goal == BSTAT_SHUTDOWN) {
	bnode_SetTimeout((struct bnode *)abnode, 0);
	return 0;
    }

    /* otherwise we're supposed to be running, figure out when */
    if (abnode->when == 0) {
	/* one shot */
	if (abnode->everRun) {
	    /* once is enough */
	    bnode_Delete((struct bnode *)abnode);
	    return 0;
	}
	/* otherwise start it */
	if (!abnode->running) {
	    /* start up */
	    abnode->lastStart = FT_ApproxTime();
	    code = bnode_NewProc((struct bnode *)abnode, abnode->command, NULL, &tp);
	    if (code) {
		bozo_Log("cron bnode %s failed to start (code %d)\n",
			 abnode->b.name, code);
		return code;
	    }
	    abnode->everRun = 1;
	    abnode->running = 1;
	    abnode->proc = tp;
	    return 0;
	}
    } else {
	/* run periodically */
	if (abnode->running)
	    return 0;
	/* otherwise find out when to run it, and do it then */
	temp = abnode->when - FT_ApproxTime();
	if (temp < 1)
	    temp = 1;		/* temp is when to start dude */
	bnode_SetTimeout((struct bnode *)abnode, temp);
    }
    return 0;
}

static int
cron_restartp(struct bnode *abnode)
{
    return 0;
}

static int
cron_delete(struct bnode *bn)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    free(abnode->command);
    free(abnode->whenString);
    free(abnode);
    return 0;
}

struct bnode *
cron_create(char *ainstance, char *acommand, char *awhen,
	    char *unused1, char *unused2, char *unused3)
{
    struct cronbnode *te;
    afs_int32 code;
    char *cmdpath;

    /* construct local path from canonical (wire-format) path */
    if (ConstructLocalBinPath(acommand, &cmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", acommand);
	return NULL;
    }

    te = (struct cronbnode *)malloc(sizeof(struct cronbnode));
    memset(te, 0, sizeof(struct cronbnode));
    code = ktime_ParsePeriodic(awhen, &te->whenToRun);
    if ((code < 0) || (bnode_InitBnode((struct bnode *)te, &cronbnode_ops, ainstance) != 0)) {
	free(te);
	free(cmdpath);
	return NULL;
    }
    te->when = ktime_next(&te->whenToRun, 0);
    te->command = cmdpath;
    te->whenString = copystr(awhen);
    return (struct bnode *)te;
}

/* called to SIGKILL a process if it doesn't terminate normally.  In cron, also
    start up a process if it is time and not already running */
static int
cron_timeout(struct bnode *bn)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    afs_int32 temp;
    afs_int32 code;
    struct bnode_proc *tp;

    if (!abnode->running) {
	if (abnode->when == 0)
	    return 0;		/* spurious timeout activation */
	/* not running, perhaps we should start it */
	if (FT_ApproxTime() >= abnode->when) {
	    abnode->lastStart = FT_ApproxTime();
	    bnode_SetTimeout((struct bnode *)abnode, 0);
	    code = bnode_NewProc((struct bnode *)abnode, abnode->command, NULL, &tp);
	    if (code) {
		bozo_Log("cron failed to start bnode %s (code %d)\n",
			 abnode->b.name, code);
		return code;
	    }
	    abnode->everRun = 1;
	    abnode->running = 1;
	    abnode->proc = tp;
	} else {
	    /* woke up too early, try again */
	    temp = abnode->when - FT_ApproxTime();
	    if (temp < 1)
		temp = 1;
	    bnode_SetTimeout((struct bnode *)abnode, temp);
	}
    } else {
	if (!abnode->waitingForShutdown)
	    return 0;		/* spurious */
	/* send kill and turn off timer */
	bnode_StopProc(abnode->proc, SIGKILL);
	abnode->killSent = 1;
	bnode_SetTimeout((struct bnode *)abnode, 0);
    }
    return 0;
}

static int
cron_getstat(struct bnode *bn, afs_int32 * astatus)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    afs_int32 temp;
    if (abnode->waitingForShutdown)
	temp = BSTAT_SHUTTINGDOWN;
    else if (abnode->b.goal == 0)
	temp = BSTAT_SHUTDOWN;
    else if (abnode->everRun && abnode->when == 0 && !abnode->running) {
	/* special hack: bnode deletion won't happen if bnode is active, so
	 * we make bnodes that are ready to be deleted automatically appear
	 * as BSTAT_SHUTDOWN so bnode_Delete is happy. */
	temp = BSTAT_SHUTDOWN;
    } else
	temp = BSTAT_NORMAL;
    *astatus = temp;
    return 0;
}

static int
cron_setstat(struct bnode *bn, afs_int32 astatus)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    if (abnode->waitingForShutdown)
	return BZBUSY;
    if (astatus == BSTAT_SHUTDOWN) {
	if (abnode->running) {
	    /* start shutdown */
	    bnode_StopProc(abnode->proc, SIGTERM);
	    abnode->waitingForShutdown = 1;
	    bnode_SetTimeout((struct bnode *)abnode, SDTIME);
	    /* When shutdown is complete, bproc() calls BOP_PROCEXIT()
	     * [cron_procexit()] which will tell bproc() to no longer
	     * run this cron job.
	     */
	} else {
	    /* Tell bproc() to no longer run this cron job */
	    bnode_SetTimeout((struct bnode *)abnode, 0);
	}
    } else if (astatus == BSTAT_NORMAL) {
	/* start the cron job
	 * Figure out when to run next and schedule it
	 */
	abnode->when = ktime_next(&abnode->whenToRun, 0);
	ScheduleCronBnode(abnode);
    }
    return 0;
}

static int
cron_procstarted(struct bnode *bn, struct bnode_proc *aproc)
{
    return 0;	/* no op */
}

static int
cron_procexit(struct bnode *bn, struct bnode_proc *aproc)
{
    struct cronbnode *abnode = (struct cronbnode *) bn;
    /* process has exited */

    /* log interesting errors for folks */
    if (aproc->lastSignal)
	bozo_Log("cron job %s exited due to signal %d\n", abnode->b.name,
		 aproc->lastSignal);
    else if (aproc->lastExit)
	bozo_Log("cron job %s exited with non-zero code %d\n", abnode->b.name,
		 aproc->lastExit);

    abnode->waitingForShutdown = 0;
    abnode->running = 0;
    abnode->killSent = 0;
    abnode->proc = (struct bnode_proc *)0;

    /* Figure out when to run next and schedule it */
    abnode->when = ktime_next(&abnode->whenToRun, 0);
    ScheduleCronBnode(abnode);
    return 0;
}

static int
cron_getstring(struct bnode *bn, char *abuffer, afs_int32 alen)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    if (abnode->running)
	strcpy(abuffer, "running now");
    else if (abnode->when == 0)
	strcpy(abuffer, "waiting to run once");
    else
	sprintf(abuffer, "run next at %s", ktime_DateOf(abnode->when));
    return 0;
}

static int
cron_getparm(struct bnode *bn, afs_int32 aindex, char *abuffer,
	     afs_int32 alen)
{
    struct cronbnode *abnode = (struct cronbnode *)bn;
    if (aindex == 0)
	strcpy(abuffer, abnode->command);
    else if (aindex == 1) {
	strcpy(abuffer, abnode->whenString);
    } else
	return BZDOM;
    return 0;
}
