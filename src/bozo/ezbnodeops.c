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

RCSID
    ("$Header$");

#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <lwp.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <afs/afsutil.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include "bnode.h"
#include "bosprototypes.h"

static int ez_timeout(), ez_getstat(), ez_setstat(), ez_delete();
static int ez_procexit(), ez_getstring(), ez_getparm(), ez_restartp();
static int ez_hascore();
struct bnode *ez_create();

#define	SDTIME		60	/* time in seconds given to a process to evaporate */

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
};

static int
ez_hascore(register struct ezbnode *abnode)
{
    char tbuffer[256];

    bnode_CoreName(abnode, NULL, tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;
    else
	return 0;
}

static int
ez_restartp(register struct ezbnode *abnode)
{
    struct bnode_token *tt;
    register afs_int32 code;
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
ez_delete(struct ezbnode *abnode)
{
    free(abnode->command);
    free(abnode);
    return 0;
}

struct bnode *
ez_create(char *ainstance, char *acommand)
{
    struct ezbnode *te;
    char *cmdpath;

    if (ConstructLocalBinPath(acommand, &cmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", acommand);
	return NULL;
    }

    te = (struct ezbnode *)malloc(sizeof(struct ezbnode));
    memset(te, 0, sizeof(struct ezbnode));
    if (bnode_InitBnode(te, &ezbnode_ops, ainstance) != 0) {
	free(te);
	return NULL;
    }
    te->command = cmdpath;
    return (struct bnode *)te;
}

/* called to SIGKILL a process if it doesn't terminate normally */
static int
ez_timeout(struct ezbnode *abnode)
{
    if (!abnode->waitingForShutdown)
	return 0;		/* spurious */
    /* send kill and turn off timer */
    bnode_StopProc(abnode->proc, SIGKILL);
    abnode->killSent = 1;
    bnode_SetTimeout(abnode, 0);
    return 0;
}

static int
ez_getstat(struct ezbnode *abnode, afs_int32 * astatus)
{
    register afs_int32 temp;
    if (abnode->waitingForShutdown)
	temp = BSTAT_SHUTTINGDOWN;
    else if (abnode->running)
	temp = BSTAT_NORMAL;
    else
	temp = BSTAT_SHUTDOWN;
    *astatus = temp;
    return 0;
}

static int
ez_setstat(register struct ezbnode *abnode, afs_int32 astatus)
{
    struct bnode_proc *tp;
    register afs_int32 code;

    if (abnode->waitingForShutdown)
	return BZBUSY;
    if (astatus == BSTAT_NORMAL && !abnode->running) {
	/* start up */
	abnode->lastStart = FT_ApproxTime();
	code = bnode_NewProc(abnode, abnode->command, NULL, &tp);
	if (code)
	    return code;
	abnode->running = 1;
	abnode->proc = tp;
	return 0;
    } else if (astatus == BSTAT_SHUTDOWN && abnode->running) {
	/* start shutdown */
	bnode_StopProc(abnode->proc, SIGTERM);
	abnode->waitingForShutdown = 1;
	bnode_SetTimeout(abnode, SDTIME);
	return 0;
    }
    return 0;
}

static int
ez_procexit(struct ezbnode *abnode, struct bnode_proc *aproc)
{
    /* process has exited */
    register afs_int32 code;

    abnode->waitingForShutdown = 0;
    abnode->running = 0;
    abnode->killSent = 0;
    abnode->proc = (struct bnode_proc *)0;
    bnode_SetTimeout(abnode, 0);	/* clear timer */
    if (abnode->b.goal)
	code = ez_setstat(abnode, BSTAT_NORMAL);
    else
	code = 0;
    return code;
}

static int
ez_getstring(struct ezbnode *abnode, char *abuffer, afs_int32 alen)
{
    return -1;			/* don't have much to add */
}

static
ez_getparm(struct ezbnode *abnode, afs_int32 aindex, char *abuffer,
	   afs_int32 alen)
{
    if (aindex > 0)
	return BZDOM;
    strcpy(abuffer, abnode->command);
    return 0;
}
