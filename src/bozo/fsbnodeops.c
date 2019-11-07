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
#include <afs/opr.h>

#include <lwp.h>
#include <rx/rx.h>
#include <afs/afsutil.h>
#include <opr/queue.h>

#include "bnode.h"
#include "bnode_internal.h"
#include "bosprototypes.h"

extern char *DoPidFiles;
static int emergency = 0;

/* if this file exists, then we have to salvage the file system */
#define	SALFILE	    "SALVAGE."

#define	POLLTIME	20	/* for handling below */
#define	SDTIME		60	/* time in seconds given to a process to evaporate */

/*  basic rules:
    Normal operation involves having the file server and the vol server both running.

    If the vol server terminates, it can simply be restarted.

    If the file server terminates, the disk must salvaged before the file server
    can be restarted.  In order to restart either the file server or the salvager,
    the vol server must be shut down.

    If the file server terminates *normally* (exits after receiving a SIGQUIT)
    then we don't have to salvage it.

    The needsSalvage flag is set when the file server is started.  It is cleared
    if the file server exits when fileSDW is true but fileKillSent is false,
    indicating that it exited after receiving a quit, but before we sent it a kill.

    The needsSalvage flag is cleared when the salvager exits.
*/

struct fsbnode {
    struct bnode b;
    afs_int32 timeSDStarted;	/* time shutdown operation started */
    char *filecmd;		/* command to start primary file server */
    char *volcmd;		/* command to start secondary vol server */
    char *salsrvcmd;            /* command to start salvageserver (demand attach fs) */
    char *salcmd;		/* command to start salvager */
    char *scancmd;		/* command to start scanner (MR-AFS) */
    struct bnode_proc *fileProc;	/* process for file server */
    struct bnode_proc *volProc;	/* process for vol server */
    struct bnode_proc *salsrvProc;	/* process for salvageserver (demand attach fs) */
    struct bnode_proc *salProc;	/* process for salvager */
    struct bnode_proc *scanProc;	/* process for scanner (MR-AFS) */
    afs_int32 lastFileStart;	/* last start for file */
    afs_int32 lastVolStart;	/* last start for vol */
    afs_int32 lastSalsrvStart;	/* last start for salvageserver (demand attach fs) */
    afs_int32 lastScanStart;	/* last start for scanner (MR-AFS) */
    char fileRunning;		/* file process is running */
    char volRunning;		/* volser is running */
    char salsrvRunning;		/* salvageserver is running (demand attach fs) */
    char salRunning;		/* salvager is running */
    char scanRunning;		/* scanner is running (MR_AFS) */
    char fileSDW;		/* file shutdown wait */
    char volSDW;		/* vol shutdown wait */
    char salsrvSDW;		/* salvageserver shutdown wait (demand attach fs) */
    char salSDW;		/* waiting for the salvager to shutdown */
    char scanSDW;		/* scanner shutdown wait (MR_AFS) */
    char fileKillSent;		/* kill signal has been sent */
    char volKillSent;
    char salsrvKillSent;        /* kill signal has been sent (demand attach fs) */
    char salKillSent;
    char scanKillSent;		/* kill signal has been sent (MR_AFS) */
    char needsSalvage;		/* salvage before running */
    char needsClock;		/* do we need clock ticks */
};

struct bnode * fs_create(char *ainstance, char *afilecmd, char *avolcmd,
			 char *asalcmd, char *ascancmd, char *dummy);
struct bnode * dafs_create(char *ainstance, char *afilecmd, char *avolcmd,
			   char * asalsrvcmd, char *asalcmd, char *ascancmd);

static int fs_hascore(struct bnode *abnode);
static int fs_restartp(struct bnode *abnode);
static int fs_delete(struct bnode *abnode);
static int fs_timeout(struct bnode *abnode);
static int fs_getstat(struct bnode *abnode, afs_int32 * astatus);
static int fs_setstat(struct bnode *abnode, afs_int32 astatus);
static int fs_procstarted(struct bnode *abnode, struct bnode_proc *aproc);
static int fs_procexit(struct bnode *abnode, struct bnode_proc *aproc);
static int fs_getstring(struct bnode *abnode, char **adesc);
static int fs_getparm(struct bnode *abnode, afs_int32 aindex, char **aparm);
static int dafs_getparm(struct bnode *abnode, afs_int32 aindex, char **aparm);

static int SetSalFlag(struct fsbnode *abnode, int aflag);
static int RestoreSalFlag(struct fsbnode *abnode);
static void SetNeedsClock(struct fsbnode *);
static int NudgeProcs(struct fsbnode *);

static char *PathToExecutable(char *cmd);

struct bnode_ops fsbnode_ops = {
    fs_create,
    fs_timeout,
    fs_getstat,
    fs_setstat,
    fs_delete,
    fs_procexit,
    fs_getstring,
    fs_getparm,
    fs_restartp,
    fs_hascore,
    fs_procstarted,
};

/* demand attach fs bnode ops */
struct bnode_ops dafsbnode_ops = {
    dafs_create,
    fs_timeout,
    fs_getstat,
    fs_setstat,
    fs_delete,
    fs_procexit,
    fs_getstring,
    dafs_getparm,
    fs_restartp,
    fs_hascore,
    fs_procstarted,
};

/* Quick inline function to safely convert a fsbnode to a bnode without
 * dropping type information
 */

static_inline struct bnode *
fsbnode2bnode(struct fsbnode *abnode) {
    return (struct bnode *) abnode;
}

/* Function to tell whether this bnode has a core file or not.  You might
 * think that this could be in bnode.c, and decide what core files to check
 * for based on the bnode's coreName property, but that doesn't work because
 * there may not be an active process for a bnode that dumped core at the
 * time the query is done.
 */
static int
fs_hascore(struct bnode *abnode)
{
    char tbuffer[256];

    /* see if file server has a core file */
    bnode_CoreName(abnode, "file", tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;

    /* see if volserver has a core file */
    bnode_CoreName(abnode, "vol", tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;

    /* see if salvageserver left a core file */
    bnode_CoreName(abnode, "salsrv", tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;

    /* see if salvager left a core file */
    bnode_CoreName(abnode, "salv", tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;

    /* see if scanner left a core file (MR-AFS) */
    bnode_CoreName(abnode, "scan", tbuffer);
    if (access(tbuffer, 0) == 0)
	return 1;

    /* no one left a core file */
    return 0;
}

static int
fs_restartp(struct bnode *bn)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;
    struct bnode_token *tt;
    afs_int32 code;
    struct stat tstat;

    code = bnode_ParseLine(abnode->filecmd, &tt);
    if (code)
	return 0;
    if (!tt)
	return 0;
    code = stat(tt->key, &tstat);
    if (code) {
	bnode_FreeTokens(tt);
	return 0;
    }
    if (tstat.st_ctime > abnode->lastFileStart)
	code = 1;
    else
	code = 0;
    bnode_FreeTokens(tt);
    if (code)
	return code;

    /* now do same for volcmd */
    code = bnode_ParseLine(abnode->volcmd, &tt);
    if (code)
	return 0;
    if (!tt)
	return 0;
    code = stat(tt->key, &tstat);
    if (code) {
	bnode_FreeTokens(tt);
	return 0;
    }
    if (tstat.st_ctime > abnode->lastVolStart)
	code = 1;
    else
	code = 0;
    bnode_FreeTokens(tt);
    if (code)
	return code;

    if (abnode->salsrvcmd) {    /* only in demand attach fs */
	/* now do same for salsrvcmd (demand attach fs) */
	code = bnode_ParseLine(abnode->salsrvcmd, &tt);
	if (code)
	    return 0;
	if (!tt)
	    return 0;
	code = stat(tt->key, &tstat);
	if (code) {
	    bnode_FreeTokens(tt);
	    return 0;
	}
	if (tstat.st_ctime > abnode->lastSalsrvStart)
	    code = 1;
	else
	    code = 0;
	bnode_FreeTokens(tt);
    }

    if (abnode->scancmd) {	/* Only in MR-AFS */
	/* now do same for scancmd (MR-AFS) */
	code = bnode_ParseLine(abnode->scancmd, &tt);
	if (code)
	    return 0;
	if (!tt)
	    return 0;
	code = stat(tt->key, &tstat);
	if (code) {
	    bnode_FreeTokens(tt);
	    return 0;
	}
	if (tstat.st_ctime > abnode->lastScanStart)
	    code = 1;
	else
	    code = 0;
	bnode_FreeTokens(tt);
    }

    return code;
}

/* set needsSalvage flag, creating file SALVAGE.<instancename> if
    we need to salvage the file system (so we can tell over panic reboots */
static int
SetSalFlag(struct fsbnode *abnode, int aflag)
{
    char *filepath;
    int fd;

    /* don't use the salvage flag for demand attach fs */
    if (abnode->salsrvcmd == NULL) {
	abnode->needsSalvage = aflag;
	if (asprintf(&filepath, "%s/%s%s", AFSDIR_SERVER_LOCAL_DIRPATH,
		   SALFILE, abnode->b.name) < 0)
	    return ENOMEM;
	if (aflag) {
	    fd = open(filepath, O_CREAT | O_TRUNC | O_RDWR, 0666);
	    close(fd);
	} else {
	    unlink(filepath);
	}
	free(filepath);
    }
    return 0;
}

/* set the needsSalvage flag according to the existence of the salvage file */
static int
RestoreSalFlag(struct fsbnode *abnode)
{
    char *filepath;

    /* never set needs salvage flag for demand attach fs */
    if (abnode->salsrvcmd != NULL) {
	abnode->needsSalvage = 0;
    } else {
	if (asprintf(&filepath, "%s/%s%s", AFSDIR_SERVER_LOCAL_DIRPATH,
		     SALFILE, abnode->b.name) < 0)
	    return ENOMEM;
	if (access(filepath, 0) == 0) {
	    /* file exists, so need to salvage */
	    abnode->needsSalvage = 1;
	} else {
	    abnode->needsSalvage = 0;
	}
	free(filepath);
    }
    return 0;
}

static int
fs_delete(struct bnode *bn)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;

    free(abnode->filecmd);
    free(abnode->volcmd);
    free(abnode->salcmd);
    if (abnode->salsrvcmd)
	free(abnode->salsrvcmd);
    if (abnode->scancmd)
	free(abnode->scancmd);
    free(abnode);
    return 0;
}

/*! PathToExecutable() - for both Unix and Windows, accept a full bnode
 * command, including any arguments, and return only the path to the
 * binary executable, with arguments stripped.
 *
 * \notes The caller will stat() the returned path.
 *
 * \param cmd - full bnode command string with arguments
 *
 * \return - string path to the binary executable, to be freed by the caller
 */
#ifdef AFS_NT40_ENV
/* The Windows implementation must also ensure that an extension is
 * specified in the path.
 */

static char *
PathToExecutable(char *cmd)
{
    char cmdext[_MAX_EXT];
    char *cmdexe, *cmdcopy, *cmdname;
    size_t cmdend;

    cmdcopy = strdup(cmd);
    if (cmdcopy == NULL) {
	return NULL;
    }
    /* strip off any arguments */
    cmdname = strsep(&cmdcopy, " \t");    /* roken, I'm hopin' */
    if (*cmdname == '\0') {
	free(cmdname);
	return NULL;
    }
    /* Is there an extension specified? */
    _splitpath(cmdname, NULL, NULL, NULL, cmdext);
    if (*cmdext == '\0') {
	/* No, supply one. */
	if (asprintf(&cmdexe, "%s.exe", cmdname) < 0) {
	    free(cmdname);
	    return NULL;
	}
	free(cmdname);
	return cmdexe;
    }
    return cmdname;
}
#else /* AFS_NT40_ENV */
/* Unix implementation is extension-agnostic. */
static char *
PathToExecutable(char *cmd)
{
    char *cmdcopy, *cmdname;
    cmdcopy = strdup(cmd);
    if (cmdcopy == NULL) {
	return NULL;
    }
    cmdname = strsep(&cmdcopy, " ");
    if (*cmdname == '\0') {
	free(cmdname);
	return NULL;
    }
    return cmdname;
}
#endif /* AFS_NT40_ENV */


struct bnode *
fs_create(char *ainstance, char *afilecmd, char *avolcmd, char *asalcmd,
	  char *ascancmd, char *dummy)
{
    struct stat tstat;
    struct fsbnode *te;
    char *cmdname = NULL;
    char *fileCmdpath, *volCmdpath, *salCmdpath, *scanCmdpath;
    int bailout = 0;

    fileCmdpath = volCmdpath = salCmdpath = scanCmdpath = NULL;
    te = NULL;

    /* construct local paths from canonical (wire-format) paths */
    if (ConstructLocalBinPath(afilecmd, &fileCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", afilecmd));
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(avolcmd, &volCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", avolcmd));
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalcmd, &salCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", asalcmd));
	bailout = 1;
	goto done;
    }

    if (ascancmd && strlen(ascancmd)) {
	if (ConstructLocalBinPath(ascancmd, &scanCmdpath)) {
	    ViceLog(0, ("BNODE: command path invalid '%s'\n", ascancmd));
	    bailout = 1;
	    goto done;
	}
    }

    if (!bailout) {
	cmdname = PathToExecutable(fileCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: file server binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}
	free(cmdname);

	cmdname = PathToExecutable(volCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: volume server binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}
	free(cmdname);

	cmdname = PathToExecutable(salCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: salvager binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}

	if (ascancmd && strlen(ascancmd)) {
	    free(cmdname);
	    cmdname = PathToExecutable(scanCmdpath);
	    if (cmdname == NULL) {
		ViceLog(0, ("Out of memory constructing binary filename\n"));
		bailout = 1;
		goto done;
	    }
	    if (stat(cmdname, &tstat)) {
		ViceLog(0, ("BNODE: scanner binary '%s' not found\n", cmdname));
		bailout = 1;
		goto done;
	    }
	}
    }

    te = calloc(1, sizeof(struct fsbnode));
    if (te == NULL) {
	bailout = 1;
	goto done;
    }
    te->filecmd = fileCmdpath;
    te->volcmd = volCmdpath;
    te->salsrvcmd = NULL;
    te->salcmd = salCmdpath;
    if (ascancmd && strlen(ascancmd))
	te->scancmd = scanCmdpath;
    else
	te->scancmd = NULL;
    if (bnode_InitBnode(fsbnode2bnode(te), &fsbnode_ops, ainstance) != 0) {
	bailout = 1;
	goto done;
    }
    bnode_SetTimeout(fsbnode2bnode(te), POLLTIME);
		/* ask for timeout activations every 20 seconds */
    RestoreSalFlag(te);		/* restore needsSalvage flag based on file's existence */
    SetNeedsClock(te);		/* compute needsClock field */

 done:
    free(cmdname);
    if (bailout) {
	if (te)
	    free(te);
	if (fileCmdpath)
	    free(fileCmdpath);
	if (volCmdpath)
	    free(volCmdpath);
	if (salCmdpath)
	    free(salCmdpath);
	if (scanCmdpath)
	    free(scanCmdpath);
	return NULL;
    }

    return fsbnode2bnode(te);
}

/* create a demand attach fs bnode */
struct bnode *
dafs_create(char *ainstance, char *afilecmd, char *avolcmd,
	    char * asalsrvcmd, char *asalcmd, char *ascancmd)
{
    struct stat tstat;
    struct fsbnode *te;
    char *cmdname = NULL;
    char *fileCmdpath, *volCmdpath, *salsrvCmdpath, *salCmdpath, *scanCmdpath;
    int bailout = 0;

    fileCmdpath = volCmdpath = salsrvCmdpath = salCmdpath = scanCmdpath = NULL;
    te = NULL;

    /* construct local paths from canonical (wire-format) paths */
    if (ConstructLocalBinPath(afilecmd, &fileCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", afilecmd));
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(avolcmd, &volCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", avolcmd));
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalsrvcmd, &salsrvCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", asalsrvcmd));
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalcmd, &salCmdpath)) {
	ViceLog(0, ("BNODE: command path invalid '%s'\n", asalcmd));
	bailout = 1;
	goto done;
    }

    if (ascancmd && strlen(ascancmd)) {
	if (ConstructLocalBinPath(ascancmd, &scanCmdpath)) {
	    ViceLog(0, ("BNODE: command path invalid '%s'\n", ascancmd));
	    bailout = 1;
	    goto done;
	}
    }

    if (!bailout) {
	cmdname = PathToExecutable(fileCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: file server binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}
	free(cmdname);

	cmdname = PathToExecutable(volCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: volume server binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}
	free(cmdname);

	cmdname = PathToExecutable(salsrvCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: salvageserver binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}
	free(cmdname);

	cmdname = PathToExecutable(salCmdpath);
	if (cmdname == NULL) {
	    ViceLog(0, ("Out of memory constructing binary filename\n"));
	    bailout = 1;
	    goto done;
	}
	if (stat(cmdname, &tstat)) {
	    ViceLog(0, ("BNODE: salvager binary '%s' not found\n", cmdname));
	    bailout = 1;
	    goto done;
	}

	if (ascancmd && strlen(ascancmd)) {
	    free(cmdname);
	    cmdname = PathToExecutable(scanCmdpath);
	    if (cmdname == NULL) {
		ViceLog(0, ("Out of memory constructing binary filename\n"));
		bailout = 1;
		goto done;
	    }
	    if (stat(cmdname, &tstat)) {
		ViceLog(0, ("BNODE: scanner binary '%s' not found\n", cmdname));
		bailout = 1;
		goto done;
	    }
	}
    }

    te = calloc(1, sizeof(struct fsbnode));
    if (te == NULL) {
	bailout = 1;
	goto done;
    }
    te->filecmd = fileCmdpath;
    te->volcmd = volCmdpath;
    te->salsrvcmd = salsrvCmdpath;
    te->salcmd = salCmdpath;
    if (ascancmd && strlen(ascancmd))
	te->scancmd = scanCmdpath;
    else
	te->scancmd = NULL;
    if (bnode_InitBnode(fsbnode2bnode(te), &dafsbnode_ops, ainstance) != 0) {
	bailout = 1;
	goto done;
    }
    bnode_SetTimeout(fsbnode2bnode(te), POLLTIME);
		/* ask for timeout activations every 20 seconds */
    RestoreSalFlag(te);		/* restore needsSalvage flag based on file's existence */
    SetNeedsClock(te);		/* compute needsClock field */

 done:
    free(cmdname);
    if (bailout) {
	if (te)
	    free(te);
	if (fileCmdpath)
	    free(fileCmdpath);
	if (volCmdpath)
	    free(volCmdpath);
	if (salsrvCmdpath)
	    free(salsrvCmdpath);
	if (salCmdpath)
	    free(salCmdpath);
	if (scanCmdpath)
	    free(scanCmdpath);
	return NULL;
    }

    return fsbnode2bnode(te);
}

/* called to SIGKILL a process if it doesn't terminate normally */
static int
fs_timeout(struct bnode *bn)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;

    afs_int32 now;

    now = FT_ApproxTime();
    /* shutting down */
    if (abnode->volSDW) {
	if (!abnode->volKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->volProc, SIGKILL);
	    abnode->volKillSent = 1;
	    ViceLog(0,
		("bos shutdown: volserver failed to shutdown within %d seconds\n",
		 SDTIME));
	}
    }
    if (abnode->salSDW) {
	if (!abnode->salKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->salProc, SIGKILL);
	    abnode->salKillSent = 1;
	    ViceLog(0,
		("bos shutdown: salvager failed to shutdown within %d seconds\n",
		 SDTIME));
	}
    }
    if (abnode->fileSDW) {
	if (!abnode->fileKillSent && now - abnode->timeSDStarted > FSSDTIME) {
	    bnode_StopProc(abnode->fileProc, SIGKILL);
	    abnode->fileKillSent = 1;
	    ViceLog(0,
		("bos shutdown: fileserver failed to shutdown within %d seconds\n",
		 FSSDTIME));
	}
    }
    if (abnode->salsrvSDW) {
	if (!abnode->salsrvKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->salsrvProc, SIGKILL);
	    abnode->salsrvKillSent = 1;
	    ViceLog(0,
		("bos shutdown: salvageserver failed to shutdown within %d seconds\n",
		 SDTIME));
	}
    }
    if (abnode->scanSDW) {
	if (!abnode->scanKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->scanProc, SIGKILL);
	    abnode->scanKillSent = 1;
	    ViceLog(0,
		("bos shutdown: scanner failed to shutdown within %d seconds\n",
		 SDTIME));
	}
    }

    if ((abnode->b.flags & BNODE_ERRORSTOP) && !abnode->salRunning
	&& !abnode->volRunning && !abnode->fileRunning && !abnode->scanRunning
	&& !abnode->salsrvRunning) {
	bnode_SetStat(bn, BSTAT_NORMAL);
    }
    else {
	bnode_ResetErrorCount(bn);
    }

    SetNeedsClock(abnode);
    return 0;
}

static int
fs_getstat(struct bnode *bn, afs_int32 * astatus)
{
    struct fsbnode *abnode = (struct fsbnode *) bn;

    afs_int32 temp;
    if (abnode->volSDW || abnode->fileSDW || abnode->salSDW
	|| abnode->scanSDW || abnode->salsrvSDW)
	temp = BSTAT_SHUTTINGDOWN;
    else if (abnode->salRunning)
	temp = BSTAT_NORMAL;
    else if (abnode->volRunning && abnode->fileRunning
	     && (!abnode->scancmd || abnode->scanRunning)
	     && (!abnode->salsrvcmd || abnode->salsrvRunning))
	temp = BSTAT_NORMAL;
    else if (!abnode->salRunning && !abnode->volRunning
	     && !abnode->fileRunning && !abnode->scanRunning
	     && !abnode->salsrvRunning)
	temp = BSTAT_SHUTDOWN;
    else
	temp = BSTAT_STARTINGUP;
    *astatus = temp;
    return 0;
}

static int
fs_setstat(struct bnode *abnode, afs_int32 astatus)
{
    return NudgeProcs((struct fsbnode *) abnode);
}

static int
fs_procstarted(struct bnode *bn, struct bnode_proc *aproc)
{
    int code = 0;

    if (DoPidFiles) {
	code = bozo_CreatePidFile(bn->name, aproc->coreName, aproc->pid);
    }
    return code;
}

static int
fs_procexit(struct bnode *bn, struct bnode_proc *aproc)
{
   struct fsbnode *abnode = (struct fsbnode *)bn;

    /* process has exited */

    if (DoPidFiles) {
	bozo_DeletePidFile(bn->name, aproc->coreName);
    }

    if (aproc == abnode->volProc) {
	abnode->volProc = 0;
	abnode->volRunning = 0;
	abnode->volSDW = 0;
	abnode->volKillSent = 0;
    } else if (aproc == abnode->fileProc) {
	/* if we were expecting a shutdown and we didn't send a kill signal
	 * and exited (didn't have a signal termination), then we assume that
	 * the file server exited after putting the appropriate volumes safely
	 * offline, and don't salvage next time.
	 */
	if (abnode->fileSDW && !abnode->fileKillSent
	    && aproc->lastSignal == 0)
	    SetSalFlag(abnode, 0);	/* shut down normally */
	abnode->fileProc = 0;
	abnode->fileRunning = 0;
	abnode->fileSDW = 0;
	abnode->fileKillSent = 0;
    } else if (aproc == abnode->salProc) {
	/* if we didn't shutdown the salvager, then assume it exited ok, and thus
	 * that we don't have to salvage again */
	if (!abnode->salSDW)
	    SetSalFlag(abnode, 0);	/* salvage just completed */
	abnode->salProc = 0;
	abnode->salRunning = 0;
	abnode->salSDW = 0;
	abnode->salKillSent = 0;
    } else if (aproc == abnode->scanProc) {
	abnode->scanProc = 0;
	abnode->scanRunning = 0;
	abnode->scanSDW = 0;
	abnode->scanKillSent = 0;
    } else if (aproc == abnode->salsrvProc) {
	abnode->salsrvProc = 0;
	abnode->salsrvRunning = 0;
	abnode->salsrvSDW = 0;
	abnode->salsrvKillSent = 0;
    }

    /* now restart anyone who needs to restart */
    return NudgeProcs(abnode);
}

/* make sure we're periodically checking the state if we need to */
static void
SetNeedsClock(struct fsbnode *ab)
{
    afs_int32 timeout = POLLTIME;

    if ((ab->fileSDW && !ab->fileKillSent) || (ab->volSDW && !ab->volKillSent)
	|| (ab->scanSDW && !ab->scanKillSent) || (ab->salSDW && !ab->salKillSent)
	|| (ab->salsrvSDW && !ab->salsrvKillSent)) {
	/* SIGQUIT sent, will send SIGKILL if process does not exit */
	ab->needsClock = 1;
    } else if (ab->b.goal == 1 && ab->fileRunning && ab->volRunning
	&& (!ab->scancmd || ab->scanRunning)
	&& (!ab->salsrvcmd || ab->salsrvRunning)) {
	if (ab->b.errorStopCount) {
	    /* reset error count after running for a bit */
	    ab->needsClock = 1;
	} else {
	    ab->needsClock = 0;	/* running normally */
	}
    } else if ((ab->b.goal == 0) && !ab->fileRunning && !ab->volRunning
	       && !ab->salRunning && !ab->scanRunning && !ab->salsrvRunning) {
	if (ab->b.flags & BNODE_ERRORSTOP && ab->b.errorStopDelay) {
	    ViceLog(0, ("%s will retry start in %d seconds\n", ab->b.name,
			ab->b.errorStopDelay));
	    ab->needsClock = 1;	/* halted for errors, retry later */
	    timeout = ab->b.errorStopDelay;
	} else {
	    ab->needsClock = 0;	/* halted normally */
	}
    } else
	ab->needsClock = 1;	/* other */

    if (ab->needsClock && (!bnode_PendingTimeout(fsbnode2bnode(ab))
			   || ab->b.period != timeout))
	bnode_SetTimeout(fsbnode2bnode(ab), timeout);
    if (!ab->needsClock)
	bnode_SetTimeout(fsbnode2bnode(ab), 0);
}

static int
NudgeProcs(struct fsbnode *abnode)
{
    struct bnode_proc *tp;	/* not register */
    afs_int32 code;
    afs_int32 now;

    now = FT_ApproxTime();
    if (abnode->b.goal == 1) {
	/* we're trying to run the system. If the file server is running, then we
	 * are trying to start up the system.  If it is not running, then needsSalvage
	 * tells us if we need to run the salvager or not */
	if (abnode->fileRunning) {
	    if (abnode->salRunning) {
		ViceLog(0, ("Salvager running along with file server!\n"));
		ViceLog(0, ("Emergency shutdown\n"));
		emergency = 1;
		bnode_SetGoal(fsbnode2bnode(abnode), BSTAT_SHUTDOWN);
		bnode_StopProc(abnode->salProc, SIGKILL);
		SetNeedsClock(abnode);
		return -1;
	    }
	    if (!abnode->volRunning) {
		abnode->lastVolStart = FT_ApproxTime();
		code = bnode_NewProc(fsbnode2bnode(abnode), abnode->volcmd, "vol", &tp);
		if (code == 0) {
		    abnode->volProc = tp;
		    abnode->volRunning = 1;
		}
	    }
	    if (abnode->salsrvcmd) {
		if (!abnode->salsrvRunning) {
		    abnode->lastSalsrvStart = FT_ApproxTime();
		    code =
			bnode_NewProc(fsbnode2bnode(abnode), abnode->salsrvcmd, "salsrv",
				      &tp);
		    if (code == 0) {
			abnode->salsrvProc = tp;
			abnode->salsrvRunning = 1;
		    }
		}
	    }
	    if (abnode->scancmd) {
		if (!abnode->scanRunning) {
		    abnode->lastScanStart = FT_ApproxTime();
		    code =
			bnode_NewProc(fsbnode2bnode(abnode), abnode->scancmd, "scanner",
				      &tp);
		    if (code == 0) {
			abnode->scanProc = tp;
			abnode->scanRunning = 1;
		    }
		}
	    }
	} else {		/* file is not running */
	    /* see how to start */
	    /* for demand attach fs, needsSalvage flag is ignored */
	    if (!abnode->needsSalvage || abnode->salsrvcmd) {
		/* no crash apparent, just start up normally */
		if (!abnode->fileRunning) {
		    abnode->lastFileStart = FT_ApproxTime();
		    code =
			bnode_NewProc(fsbnode2bnode(abnode), abnode->filecmd, "file", &tp);
		    if (code == 0) {
			abnode->fileProc = tp;
			abnode->fileRunning = 1;
			SetSalFlag(abnode, 1);
		    }
		}
		if (!abnode->volRunning) {
		    abnode->lastVolStart = FT_ApproxTime();
		    code = bnode_NewProc(fsbnode2bnode(abnode), abnode->volcmd, "vol", &tp);
		    if (code == 0) {
			abnode->volProc = tp;
			abnode->volRunning = 1;
		    }
		}
		if (abnode->salsrvcmd && !abnode->salsrvRunning) {
		    abnode->lastSalsrvStart = FT_ApproxTime();
		    code =
			bnode_NewProc(fsbnode2bnode(abnode), abnode->salsrvcmd, "salsrv",
				      &tp);
		    if (code == 0) {
			abnode->salsrvProc = tp;
			abnode->salsrvRunning = 1;
		    }
		}
		if (abnode->scancmd && !abnode->scanRunning) {
		    abnode->lastScanStart = FT_ApproxTime();
		    code =
			bnode_NewProc(fsbnode2bnode(abnode), abnode->scancmd, "scanner",
				      &tp);
		    if (code == 0) {
			abnode->scanProc = tp;
			abnode->scanRunning = 1;
		    }
		}
	    } else {		/* needs to be salvaged */
		/* make sure file server and volser are gone */
		if (abnode->volRunning) {
		    bnode_StopProc(abnode->volProc, SIGTERM);
		    if (!abnode->volSDW)
			abnode->timeSDStarted = now;
		    abnode->volSDW = 1;
		}
		if (abnode->fileRunning) {
		    bnode_StopProc(abnode->fileProc, SIGQUIT);
		    if (!abnode->fileSDW)
			abnode->timeSDStarted = now;
		    abnode->fileSDW = 1;
		}
		if (abnode->scanRunning) {
		    bnode_StopProc(abnode->scanProc, SIGTERM);
		    if (!abnode->scanSDW)
			abnode->timeSDStarted = now;
		    abnode->scanSDW = 1;
		}
		if (abnode->volRunning || abnode->fileRunning
		    || abnode->scanRunning)
		    return 0;
		/* otherwise, it is safe to start salvager */
		if (!abnode->salRunning) {
		    code = bnode_NewProc(fsbnode2bnode(abnode), abnode->salcmd, "salv", &tp);
		    if (code == 0) {
			abnode->salProc = tp;
			abnode->salRunning = 1;
		    }
		}
	    }
	}
    } else {			/* goal is 0, we're shutting down */
	/* trying to shutdown */
	if (abnode->salRunning && !abnode->salSDW) {
	    bnode_StopProc(abnode->salProc, SIGTERM);
	    abnode->salSDW = 1;
	    abnode->timeSDStarted = now;
	}
	if (abnode->fileRunning && !abnode->fileSDW) {
	    bnode_StopProc(abnode->fileProc, SIGQUIT);
	    abnode->fileSDW = 1;
	    abnode->timeSDStarted = now;
	}
	if (abnode->volRunning && !abnode->volSDW) {
	    bnode_StopProc(abnode->volProc, SIGTERM);
	    abnode->volSDW = 1;
	    abnode->timeSDStarted = now;
	}
	if (abnode->salsrvRunning && !abnode->salsrvSDW) {
	    bnode_StopProc(abnode->salsrvProc, SIGTERM);
	    abnode->salsrvSDW = 1;
	    abnode->timeSDStarted = now;
	}
	if (abnode->scanRunning && !abnode->scanSDW) {
	    bnode_StopProc(abnode->scanProc, SIGTERM);
	    abnode->scanSDW = 1;
	    abnode->timeSDStarted = now;
	}
    }
    SetNeedsClock(abnode);
    return 0;
}

static int
fs_getstring(struct bnode *bn, char **adesc)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;
    const char *desc;

    if (abnode->b.goal == 1) {
	if (abnode->fileRunning) {
	    if (abnode->fileSDW)
		desc = "file server shutting down";
	    else if (abnode->scancmd) {
		if (!abnode->volRunning && !abnode->scanRunning)
		    desc = "file server up; volser and scanner down";
		else if (abnode->volRunning && !abnode->scanRunning)
		    desc = "file server up; volser up; scanner down";
		else if (!abnode->volRunning && abnode->scanRunning)
		    desc = "file server up; volser down; scanner up";
		else
		    desc = "file server running";
	    } else if (!abnode->volRunning)
		desc = "file server up; volser down";
	    else
		desc = "file server running";
	} else if (abnode->salRunning) {
	    desc = "salvaging file system";
	} else
	    desc = "starting file server";
    } else {
	/* shutting down */
	if (abnode->fileRunning || abnode->volRunning || abnode->scanRunning) {
	    desc = "file server shutting down";
	} else if (abnode->salRunning)
	    desc = "salvager shutting down";
	else
	    desc = "file server shut down";
    }
    *adesc = strdup(desc);
    if (*adesc == NULL)
	return BZIO;
    return 0;
}

static int
fs_getparm(struct bnode *bn, afs_int32 aindex, char **aparm)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;
    char *parm;

    if (aindex == 0)
	parm = abnode->filecmd;
    else if (aindex == 1)
	parm = abnode->volcmd;
    else if (aindex == 2)
	parm = abnode->salcmd;
    else if (aindex == 3 && abnode->scancmd)
	parm = abnode->scancmd;
    else
	return BZDOM;
    *aparm = strdup(parm);
    if (*aparm == NULL)
	return BZIO;
    return 0;
}

static int
dafs_getparm(struct bnode *bn, afs_int32 aindex, char **aparm)
{
    struct fsbnode *abnode = (struct fsbnode *)bn;
    char *parm;

    if (aindex == 0)
	parm = abnode->filecmd;
    else if (aindex == 1)
	parm = abnode->volcmd;
    else if (aindex == 2)
	parm = abnode->salsrvcmd;
    else if (aindex == 3)
	parm = abnode->salcmd;
    else if (aindex == 4 && abnode->scancmd)
	parm = abnode->scancmd;
    else
	return BZDOM;
    *aparm = strdup(parm);
    if (*aparm == NULL)
	return BZIO;
    return 0;
}
