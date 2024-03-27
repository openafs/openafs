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

#include <lwp.h>
#include <rx/rx.h>

#include <afs/afsutil.h>
#include <opr/queue.h>

#include "bnode.h"
#include "bnode_internal.h"
#include "bosprototypes.h"

extern char *DoPidFiles;

struct bnode *ez_create(char *, char *, char *, char *, char *, char *);
static int ez_hascore(struct bnode *bnode);
static int ez_restartp(struct bnode *bnode);
static int ez_delete(struct bnode *bnode);
static int ez_timeout(struct bnode *bnode);
static int ez_getstat(struct bnode *bnode, afs_int32 *status);
static int ez_setstat(struct bnode *bnode, afs_int32 status);
static int ez_procexit(struct bnode *bnode, struct bnode_proc *proc);
static int ez_getstring(struct bnode *bnode, char **adesc);
static int ez_getparm(struct bnode *bnode, afs_int32 aindex, char **parm);
static int ez_procstarted(struct bnode *bnode, struct bnode_proc *proc);

#define	SDTIME		60	/* time in seconds given to a process to evaporate */
#define ERROR_RESET_TIME 60	/* time in seconds to wait before resetting error count state */

struct bnode_ops ezbnode_ops = {
    ez_create,
    ez_timeout,
    ez_getstat,
    ez_setstat,
    ez_delete,
    ez_procexit,
    ez_getstring,
    ez_getparm,
    ez_restartp,
    ez_hascore,
    ez_procstarted
};

static int
ez_hascore(struct bnode *abnode)
{
    char tbuffer[256];

    bnode_CoreName(abnode, NULL, tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;
    else
	return 0;
}

static int
ez_restartp(struct bnode *bn)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;
    struct bnode_token *tt;
    afs_int32 code;
    struct stat tstat;

    code = bnode_ParseLine(abnode->command, &tt);
    if (code)
	return 0;
    if (!tt)
	return 0;
    code = stat(tt->key, &tstat);
    if (code) {
	bnode_FreeTokens(tt);
	return 0;
    }
    if (tstat.st_ctime > abnode->lastStart)
	code = 1;
    else
	code = 0;
    bnode_FreeTokens(tt);
    return code;
}

static int
ez_delete(struct bnode *bn)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;

    free(abnode->command);
    free(abnode);
    return 0;
}

struct bnode *
ez_create(char *ainstance, char *acommand, char *unused1, char *unused2,
	  char *unused3, char *unused4)
{
    struct ezbnode *te;
    char *cmdpath;

    if (ConstructLocalBinPath(acommand, &cmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", acommand);
	return NULL;
    }

    te = calloc(1, sizeof(struct ezbnode));
    if (bnode_InitBnode((struct bnode *)te, &ezbnode_ops, ainstance) != 0) {
	free(te);
	free(cmdpath);
	return NULL;
    }
    te->command = cmdpath;
    return (struct bnode *)te;
}

/* called to SIGKILL a process if it doesn't terminate normally
 * or to retry start after an error stop. */
static int
ez_timeout(struct bnode *bn)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;

    if (abnode->waitingForShutdown) {
	/* send kill and turn off timer */
	bnode_StopProc(abnode->proc, SIGKILL);
	abnode->killSent = 1;
	bnode_SetTimeout((struct bnode *)abnode, 0);
    } else if (!abnode->running && abnode->b.flags & BNODE_ERRORSTOP) {
	/* was stopped for too many errors, retrying */
	/* reset error count after running for a bit */
	bnode_SetTimeout(bn, ERROR_RESET_TIME);
	bnode_SetStat(bn, BSTAT_NORMAL);
    } else {
	bnode_SetTimeout(bn, 0); /* one shot timer */
	bnode_ResetErrorCount(bn);
    }
    return 0;
}

static int
ez_getstat(struct bnode *bn, afs_int32 * astatus)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;

    afs_int32 temp;
    if (abnode->waitingForShutdown)
	temp = BSTAT_SHUTTINGDOWN;
    else if (abnode->running)
	temp = BSTAT_NORMAL;
    else if (abnode->b.flags & BNODE_ERRORSTOP)
	temp = BSTAT_STARTINGUP;
    else
	temp = BSTAT_SHUTDOWN;
    *astatus = temp;
    return 0;
}

static int
ez_setstat(struct bnode *bn, afs_int32 astatus)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;

    struct bnode_proc *tp;
    afs_int32 code;

    if (abnode->waitingForShutdown)
	return BZBUSY;
    if (astatus == BSTAT_NORMAL && !abnode->running) {
	/* start up */
	abnode->lastStart = FT_ApproxTime();
	code = bnode_NewProc((struct bnode *)abnode, abnode->command, NULL, &tp);
	if (code)
	    return code;
	abnode->running = 1;
	abnode->proc = tp;
	return 0;
    } else if (astatus == BSTAT_SHUTDOWN && abnode->running) {
	/* start shutdown */
	bnode_StopProc(abnode->proc, SIGTERM);
	abnode->waitingForShutdown = 1;
	bnode_SetTimeout((struct bnode *)abnode, SDTIME);
	return 0;
    }
    return 0;
}

static int
ez_procstarted(struct bnode *bn, struct bnode_proc *aproc)
{
    int code = 0;

    if (DoPidFiles) {
	code = bozo_CreatePidFile(bn->name, NULL, aproc->pid);
    }
    return code;;
}

static int
ez_procexit(struct bnode *bn, struct bnode_proc *aproc)
{
    struct ezbnode *abnode = (struct ezbnode *)bn;

    /* process has exited */
    afs_int32 code = 0;

    if (DoPidFiles) {
	bozo_DeletePidFile(bn->name, NULL);
    }

    abnode->waitingForShutdown = 0;
    abnode->running = 0;
    abnode->killSent = 0;
    abnode->proc = NULL;
    bnode_SetTimeout((struct bnode *) abnode, 0);	/* clear timer */
    if (abnode->b.goal)
	code = ez_setstat((struct bnode *) abnode, BSTAT_NORMAL);
    else if (abnode->b.flags & BNODE_ERRORSTOP && abnode->b.errorStopDelay) {
	bozo_Log("%s will retry start in %d seconds\n", abnode->b.name,
		 abnode->b.errorStopDelay);
	bnode_SetTimeout(bn, abnode->b.errorStopDelay);
    }
    return code;
}

static int
ez_getstring(struct bnode *abnode, char **adesc)
{
    *adesc = strdup("");  /* Don't have much to add. */
    if (*adesc == NULL)
	return BZIO;
    return 0;
}

static int
ez_getparm(struct bnode *bn, afs_int32 aindex, char **aparm)
{
    struct ezbnode *abnode = (struct ezbnode *) bn;

    if (aindex != 0)
	return BZDOM;

    *aparm = strdup(abnode->command);
    if (*aparm == NULL)
	return BZIO;

    return 0;
}
