/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <lwp.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#endif
#include <afs/afsutil.h>
#include <afs/procmgmt.h>  /* signal(), kill(), wait(), etc. */
#include "bnode.h"

static int ez_timeout(), ez_getstat(), ez_setstat(), ez_delete();
static int ez_procexit(), ez_getstring(), ez_getparm(), ez_restartp();
static int ez_hascore();
struct bnode *ez_create();

#define	SDTIME		60	    /* time in seconds given to a process to evaporate */

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

static int ez_hascore(abnode)
register struct ezbnode *abnode; {
    char tbuffer[256];

    bnode_CoreName(abnode, (char *) 0, tbuffer);
    if (access(tbuffer, 0) == 0) return 1;
    else return 0;
}

static int ez_restartp (abnode)
register struct ezbnode *abnode; {
    struct bnode_token *tt;
    register afs_int32 code;
    struct stat tstat;
    
    code = bnode_ParseLine(abnode->command, &tt);
    if (code) return 0;
    if (!tt) return 0;
    code = stat(tt->key, &tstat);
    if (code) {
	bnode_FreeTokens(tt);
	return 0;
    }
    if (tstat.st_ctime > abnode->lastStart) code = 1;
    else code = 0;
    bnode_FreeTokens(tt);
    return code;
}

static int ez_delete(abnode)
struct ezbnode *abnode; {
    free(abnode->command);
    free(abnode);
    return 0;
}

struct bnode *ez_create(ainstance, acommand)
char *ainstance;
char *acommand; {
    struct ezbnode *te;
    char *cmdpath;

    if (ConstructLocalBinPath(acommand, &cmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", acommand);
	return (struct bnode *)0;
    }

    te = (struct ezbnode *) malloc(sizeof(struct ezbnode));
    bzero(te, sizeof(struct ezbnode));
    bnode_InitBnode(te, &ezbnode_ops, ainstance);
    te->command = cmdpath;
    return (struct bnode *) te;
}

/* called to SIGKILL a process if it doesn't terminate normally */
static int ez_timeout(abnode)
struct ezbnode *abnode; {
    if (!abnode->waitingForShutdown) return 0;	/* spurious */
    /* send kill and turn off timer */
    bnode_StopProc(abnode->proc, SIGKILL);
    abnode->killSent = 1;
    bnode_SetTimeout(abnode, 0);
    return 0;
}

static int ez_getstat(abnode, astatus)
struct ezbnode *abnode;
afs_int32 *astatus; {
    register afs_int32 temp;
    if (abnode->waitingForShutdown) temp = BSTAT_SHUTTINGDOWN;
    else if (abnode->running) temp = BSTAT_NORMAL;
    else temp = BSTAT_SHUTDOWN;
    *astatus = temp;
    return 0;
}

static int ez_setstat(abnode, astatus)
register struct ezbnode *abnode;
afs_int32 astatus; {
    struct bnode_proc *tp;
    register afs_int32 code;

    if (abnode->waitingForShutdown) return BZBUSY;
    if (astatus == BSTAT_NORMAL && !abnode->running) {
	/* start up */
	abnode->lastStart = FT_ApproxTime();
	code = bnode_NewProc(abnode, abnode->command, (char *) 0, &tp);
	if (code) return code;
	abnode->running = 1;
	abnode->proc = tp;
	return 0;
    }
    else if (astatus == BSTAT_SHUTDOWN && abnode->running) {
	/* start shutdown */
	bnode_StopProc(abnode->proc, SIGTERM);
	abnode->waitingForShutdown = 1;
	bnode_SetTimeout(abnode, SDTIME);
	return 0;
    }
    return 0;
}

static int ez_procexit(abnode, aproc)
struct ezbnode *abnode;
struct bnode_proc *aproc; {
    /* process has exited */
    register afs_int32 code;

    abnode->waitingForShutdown = 0;
    abnode->running = 0;
    abnode->killSent = 0;
    abnode->proc = (struct bnode_proc *) 0;
    bnode_SetTimeout(abnode, 0);	/* clear timer */
    if (abnode->b.goal)
	code = ez_setstat(abnode, BSTAT_NORMAL);
    else code = 0;
    return code;
}

static int ez_getstring(abnode, abuffer, alen)
struct ezbnode *abnode;
char *abuffer;
afs_int32 alen;{
    return -1;	    /* don't have much to add */
}

static ez_getparm(abnode, aindex, abuffer, alen)
struct ezbnode *abnode;
afs_int32 aindex;
char *abuffer;
afs_int32 alen; {
    if (aindex > 0) return BZDOM;
    strcpy(abuffer, abnode->command);
    return 0;
}
