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
#include <lwp.h>
#include <errno.h>
#include <stdio.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#ifdef AFS_NT40_ENV
#include <io.h>
#include <fcntl.h>
#else
#include <sys/file.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <stdlib.h>

#endif /* AFS_NT40_ENV */
#include <sys/stat.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <afs/afsutil.h>
#include "bnode.h"


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
			 char *asalcmd, char *ascancmd);
struct bnode * dafs_create(char *ainstance, char *afilecmd, char *avolcmd, 
			   char * asalsrvcmd, char *asalcmd, char *ascancmd);

static int fs_hascore(register struct ezbnode *abnode);
static int fs_restartp(register struct fsbnode *abnode);
static int SetSalFlag(register struct fsbnode *abnode, register int aflag);
static int RestoreSalFlag(register struct fsbnode *abnode);
static int fs_delete(struct fsbnode *abnode);
static int fs_timeout(struct fsbnode *abnode);
static int fs_getstat(struct fsbnode *abnode, afs_int32 * astatus);
static int fs_setstat(register struct fsbnode *abnode, afs_int32 astatus);
static int fs_procexit(struct fsbnode *abnode, struct bnode_proc *aproc);
static int fs_getstring(struct fsbnode *abnode, char *abuffer, afs_int32 alen);


static int fs_getparm(struct fsbnode *abnode, afs_int32 aindex, 
		      char *abuffer, afs_int32 alen);
static int dafs_getparm(struct fsbnode *abnode, afs_int32 aindex, 
			char *abuffer, afs_int32 alen);

#ifdef AFS_NT40_ENV
static void AppendExecutableExtension(char *cmd);
#else
#define AppendExecutableExtension(x)
#endif

static void SetNeedsClock(register struct fsbnode *ab);
static int NudgeProcs(register struct fsbnode *abnode);



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
};


/* Function to tell whether this bnode has a core file or not.  You might
 * think that this could be in bnode.c, and decide what core files to check
 * for based on the bnode's coreName property, but that doesn't work because
 * there may not be an active process for a bnode that dumped core at the
 * time the query is done.
 */
static int
fs_hascore(register struct ezbnode *abnode)
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
fs_restartp(register struct fsbnode *abnode)
{
    struct bnode_token *tt;
    register afs_int32 code;
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
	if (tstat.st_ctime > abnode->lastScanStart)
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
SetSalFlag(register struct fsbnode *abnode, register int aflag)
{
    char tbuffer[AFSDIR_PATH_MAX];
    int fd;

    /* don't use the salvage flag for demand attach fs */
    if (abnode->salsrvcmd == NULL) {
	abnode->needsSalvage = aflag;
	strcompose(tbuffer, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH, "/",
		   SALFILE, abnode->b.name, NULL);
	if (aflag) {
	    fd = open(tbuffer, O_CREAT | O_TRUNC | O_RDWR, 0666);
	    close(fd);
	} else {
	    unlink(tbuffer);
	}
    }
    return 0;
}

/* set the needsSalvage flag according to the existence of the salvage file */
static int
RestoreSalFlag(register struct fsbnode *abnode)
{
    char tbuffer[AFSDIR_PATH_MAX];

    /* never set needs salvage flag for demand attach fs */
    if (abnode->salsrvcmd != NULL) {
	abnode->needsSalvage = 0;
    } else {
	strcompose(tbuffer, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH, "/",
		   SALFILE, abnode->b.name, NULL);
	if (access(tbuffer, 0) == 0) {
	    /* file exists, so need to salvage */
	    abnode->needsSalvage = 1;
	} else {
	    abnode->needsSalvage = 0;
	}
    }
    return 0;
}

char *
copystr(register char *a)
{
    register char *b;
    b = (char *)malloc(strlen(a) + 1);
    strcpy(b, a);
    return b;
}

static int
fs_delete(struct fsbnode *abnode)
{
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


#ifdef AFS_NT40_ENV
static void
AppendExecutableExtension(char *cmd)
{
    char cmdext[_MAX_EXT];

    _splitpath(cmd, NULL, NULL, NULL, cmdext);
    if (*cmdext == '\0') {
	/* no filename extension supplied for cmd; append .exe */
	strcat(cmd, ".exe");
    }
}
#endif /* AFS_NT40_ENV */


struct bnode *
fs_create(char *ainstance, char *afilecmd, char *avolcmd, char *asalcmd,
	  char *ascancmd)
{
    struct stat tstat;
    register struct fsbnode *te;
    char cmdname[AFSDIR_PATH_MAX];
    char *fileCmdpath, *volCmdpath, *salCmdpath, *scanCmdpath;
    int bailout = 0;

    te = fileCmdpath = volCmdpath = salCmdpath = scanCmdpath = NULL;

    /* construct local paths from canonical (wire-format) paths */
    if (ConstructLocalBinPath(afilecmd, &fileCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", afilecmd);
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(avolcmd, &volCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", avolcmd);
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalcmd, &salCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", asalcmd);
	bailout = 1;
	goto done;
    }

    if (ascancmd && strlen(ascancmd)) {
	if (ConstructLocalBinPath(ascancmd, &scanCmdpath)) {
	    bozo_Log("BNODE: command path invalid '%s'\n", ascancmd);
	    bailout = 1;
	    goto done;
	}
    }

    if (!bailout) {
	sscanf(fileCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: file server binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	sscanf(volCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: volume server binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	sscanf(salCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: salvager binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	if (ascancmd && strlen(ascancmd)) {
	    sscanf(scanCmdpath, "%s", cmdname);
	    AppendExecutableExtension(cmdname);
	    if (stat(cmdname, &tstat)) {
		bozo_Log("BNODE: scanner binary '%s' not found\n", cmdname);
		bailout = 1;
		goto done;
	    }
	}
    }

    te = (struct fsbnode *)malloc(sizeof(struct fsbnode));
    if (te == NULL) {
	bailout = 1;
	goto done;
    }
    memset(te, 0, sizeof(struct fsbnode));
    te->filecmd = fileCmdpath;
    te->volcmd = volCmdpath;
    te->salsrvcmd = NULL;
    te->salcmd = salCmdpath;
    if (ascancmd && strlen(ascancmd))
	te->scancmd = scanCmdpath;
    else
	te->scancmd = NULL;
    if (bnode_InitBnode(te, &fsbnode_ops, ainstance) != 0) {
	bailout = 1;
	goto done;
    }
    bnode_SetTimeout(te, POLLTIME);	/* ask for timeout activations every 10 seconds */
    RestoreSalFlag(te);		/* restore needsSalvage flag based on file's existence */
    SetNeedsClock(te);		/* compute needsClock field */

 done:
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

    return (struct bnode *)te;
}

/* create a demand attach fs bnode */
struct bnode *
dafs_create(char *ainstance, char *afilecmd, char *avolcmd, 
	    char * asalsrvcmd, char *asalcmd, char *ascancmd)
{
    struct stat tstat;
    register struct fsbnode *te;
    char cmdname[AFSDIR_PATH_MAX];
    char *fileCmdpath, *volCmdpath, *salsrvCmdpath, *salCmdpath, *scanCmdpath;
    int bailout = 0;

    te = fileCmdpath = volCmdpath = salsrvCmdpath = salCmdpath = scanCmdpath = NULL;

    /* construct local paths from canonical (wire-format) paths */
    if (ConstructLocalBinPath(afilecmd, &fileCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", afilecmd);
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(avolcmd, &volCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", avolcmd);
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalsrvcmd, &salsrvCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", asalsrvcmd);
	bailout = 1;
	goto done;
    }
    if (ConstructLocalBinPath(asalcmd, &salCmdpath)) {
	bozo_Log("BNODE: command path invalid '%s'\n", asalcmd);
	bailout = 1;
	goto done;
    }

    if (ascancmd && strlen(ascancmd)) {
	if (ConstructLocalBinPath(ascancmd, &scanCmdpath)) {
	    bozo_Log("BNODE: command path invalid '%s'\n", ascancmd);
	    bailout = 1;
	    goto done;
	}
    }

    if (!bailout) {
	sscanf(fileCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: file server binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	sscanf(volCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: volume server binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	sscanf(salsrvCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: salvageserver binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	sscanf(salCmdpath, "%s", cmdname);
	AppendExecutableExtension(cmdname);
	if (stat(cmdname, &tstat)) {
	    bozo_Log("BNODE: salvager binary '%s' not found\n", cmdname);
	    bailout = 1;
	    goto done;
	}

	if (ascancmd && strlen(ascancmd)) {
	    sscanf(scanCmdpath, "%s", cmdname);
	    AppendExecutableExtension(cmdname);
	    if (stat(cmdname, &tstat)) {
		bozo_Log("BNODE: scanner binary '%s' not found\n", cmdname);
		bailout = 1;
		goto done;
	    }
	}
    }

    te = (struct fsbnode *)malloc(sizeof(struct fsbnode));
    if (te == NULL) {
	bailout = 1;
	goto done;
    }
    memset(te, 0, sizeof(struct fsbnode));
    te->filecmd = fileCmdpath;
    te->volcmd = volCmdpath;
    te->salsrvcmd = salsrvCmdpath;
    te->salcmd = salCmdpath;
    if (ascancmd && strlen(ascancmd))
	te->scancmd = scanCmdpath;
    else
	te->scancmd = NULL;
    if (bnode_InitBnode(te, &dafsbnode_ops, ainstance) != 0) {
	bailout = 1;
	goto done;
    }
    bnode_SetTimeout(te, POLLTIME);	/* ask for timeout activations every 10 seconds */
    RestoreSalFlag(te);		/* restore needsSalvage flag based on file's existence */
    SetNeedsClock(te);		/* compute needsClock field */

 done:
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

    return (struct bnode *)te;
}

/* called to SIGKILL a process if it doesn't terminate normally */
static int
fs_timeout(struct fsbnode *abnode)
{
    register afs_int32 now;

    now = FT_ApproxTime();
    /* shutting down */
    if (abnode->volSDW) {
	if (!abnode->volKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->volProc, SIGKILL);
	    abnode->volKillSent = 1;
	    bozo_Log
		("bos shutdown: volserver failed to shutdown within %d seconds\n",
		 SDTIME);
	}
    }
    if (abnode->salSDW) {
	if (!abnode->salKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->salProc, SIGKILL);
	    abnode->salKillSent = 1;
	    bozo_Log
		("bos shutdown: salvager failed to shutdown within %d seconds\n",
		 SDTIME);
	}
    }
    if (abnode->fileSDW) {
	if (!abnode->fileKillSent && now - abnode->timeSDStarted > FSSDTIME) {
	    bnode_StopProc(abnode->fileProc, SIGKILL);
	    abnode->fileKillSent = 1;
	    bozo_Log
		("bos shutdown: fileserver failed to shutdown within %d seconds\n",
		 FSSDTIME);
	}
    }
    if (abnode->salsrvSDW) {
	if (!abnode->salsrvKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->salsrvProc, SIGKILL);
	    abnode->salsrvKillSent = 1;
	    bozo_Log
		("bos shutdown: salvageserver failed to shutdown within %d seconds\n",
		 SDTIME);
	}
    }
    if (abnode->scanSDW) {
	if (!abnode->scanKillSent && now - abnode->timeSDStarted > SDTIME) {
	    bnode_StopProc(abnode->scanProc, SIGKILL);
	    abnode->scanKillSent = 1;
	    bozo_Log
		("bos shutdown: scanner failed to shutdown within %d seconds\n",
		 SDTIME);
	}
    }
    SetNeedsClock(abnode);
    return 0;
}

static int
fs_getstat(struct fsbnode *abnode, afs_int32 * astatus)
{
    register afs_int32 temp;
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
fs_setstat(register struct fsbnode *abnode, afs_int32 astatus)
{
    return NudgeProcs(abnode);
}

static int
fs_procexit(struct fsbnode *abnode, struct bnode_proc *aproc)
{
    /* process has exited */

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
SetNeedsClock(register struct fsbnode *ab)
{
    if (ab->b.goal == 1 && ab->fileRunning && ab->volRunning
	&& (!ab->scancmd || ab->scanRunning)
	&& (!ab->salsrvcmd || ab->salsrvRunning))
	ab->needsClock = 0;	/* running normally */
    else if (ab->b.goal == 0 && !ab->fileRunning && !ab->volRunning
	     && !ab->salRunning && !ab->scanRunning && !ab->salsrvRunning)
	ab->needsClock = 0;	/* halted normally */
    else
	ab->needsClock = 1;	/* other */
    if (ab->needsClock && !bnode_PendingTimeout(ab))
	bnode_SetTimeout(ab, POLLTIME);
    if (!ab->needsClock)
	bnode_SetTimeout(ab, 0);
}

static int
NudgeProcs(register struct fsbnode *abnode)
{
    struct bnode_proc *tp;	/* not register */
    register afs_int32 code;
    afs_int32 now;

    now = FT_ApproxTime();
    if (abnode->b.goal == 1) {
	/* we're trying to run the system. If the file server is running, then we
	 * are trying to start up the system.  If it is not running, then needsSalvage
	 * tells us if we need to run the salvager or not */
	if (abnode->fileRunning) {
	    if (abnode->salRunning) {
		bozo_Log("Salvager running along with file server!\n");
		bozo_Log("Emergency shutdown\n");
		emergency = 1;
		bnode_SetGoal(abnode, BSTAT_SHUTDOWN);
		bnode_StopProc(abnode->salProc, SIGKILL);
		SetNeedsClock(abnode);
		return -1;
	    }
	    if (!abnode->volRunning) {
		abnode->lastVolStart = FT_ApproxTime();
		code = bnode_NewProc(abnode, abnode->volcmd, "vol", &tp);
		if (code == 0) {
		    abnode->volProc = tp;
		    abnode->volRunning = 1;
		}
	    }
	    if (abnode->salsrvcmd) {
		if (!abnode->salsrvRunning) {
		    abnode->lastSalsrvStart = FT_ApproxTime();
		    code =
			bnode_NewProc(abnode, abnode->salsrvcmd, "salsrv",
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
			bnode_NewProc(abnode, abnode->scancmd, "scanner",
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
			bnode_NewProc(abnode, abnode->filecmd, "file", &tp);
		    if (code == 0) {
			abnode->fileProc = tp;
			abnode->fileRunning = 1;
			SetSalFlag(abnode, 1);
		    }
		}
		if (!abnode->volRunning) {
		    abnode->lastVolStart = FT_ApproxTime();
		    code = bnode_NewProc(abnode, abnode->volcmd, "vol", &tp);
		    if (code == 0) {
			abnode->volProc = tp;
			abnode->volRunning = 1;
		    }
		}
		if (abnode->salsrvcmd && !abnode->salsrvRunning) {
		    abnode->lastSalsrvStart = FT_ApproxTime();
		    code =
			bnode_NewProc(abnode, abnode->salsrvcmd, "salsrv",
				      &tp);
		    if (code == 0) {
			abnode->salsrvProc = tp;
			abnode->salsrvRunning = 1;
		    }
		}
		if (abnode->scancmd && !abnode->scanRunning) {
		    abnode->lastScanStart = FT_ApproxTime();
		    code =
			bnode_NewProc(abnode, abnode->scancmd, "scanner",
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
		    code = bnode_NewProc(abnode, abnode->salcmd, "salv", &tp);
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
fs_getstring(struct fsbnode *abnode, char *abuffer, afs_int32 alen)
{
    if (alen < 40)
	return -1;
    if (abnode->b.goal == 1) {
	if (abnode->fileRunning) {
	    if (abnode->fileSDW)
		strcpy(abuffer, "file server shutting down");
	    else if (abnode->scancmd) {
		if (!abnode->volRunning && !abnode->scanRunning)
		    strcpy(abuffer,
			   "file server up; volser and scanner down");
		else if (abnode->volRunning && !abnode->scanRunning)
		    strcpy(abuffer,
			   "file server up; volser up; scanner down");
		else if (!abnode->volRunning && abnode->scanRunning)
		    strcpy(abuffer,
			   "file server up; volser down; scanner up");

		else
		    strcpy(abuffer, "file server running");
	    } else if (!abnode->volRunning)
		strcpy(abuffer, "file server up; volser down");
	    else
		strcpy(abuffer, "file server running");
	} else if (abnode->salRunning) {
	    strcpy(abuffer, "salvaging file system");
	} else
	    strcpy(abuffer, "starting file server");
    } else {
	/* shutting down */
	if (abnode->fileRunning || abnode->volRunning || abnode->scanRunning) {
	    strcpy(abuffer, "file server shutting down");
	} else if (abnode->salRunning)
	    strcpy(abuffer, "salvager shutting down");
	else
	    strcpy(abuffer, "file server shut down");
    }
    return 0;
}

static int
fs_getparm(struct fsbnode *abnode, afs_int32 aindex, char *abuffer,
	   afs_int32 alen)
{
    if (aindex == 0)
	strcpy(abuffer, abnode->filecmd);
    else if (aindex == 1)
	strcpy(abuffer, abnode->volcmd);
    else if (aindex == 2)
	strcpy(abuffer, abnode->salcmd);
    else if (aindex == 3 && abnode->scancmd)
	strcpy(abuffer, abnode->scancmd);
    else
	return BZDOM;
    return 0;
}

static int
dafs_getparm(struct fsbnode *abnode, afs_int32 aindex, char *abuffer,
	     afs_int32 alen)
{
    if (aindex == 0)
	strcpy(abuffer, abnode->filecmd);
    else if (aindex == 1)
	strcpy(abuffer, abnode->volcmd);
    else if (aindex == 2)
	strcpy(abuffer, abnode->salsrvcmd);
    else if (aindex == 3)
	strcpy(abuffer, abnode->salcmd);
    else if (aindex == 4 && abnode->scancmd)
	strcpy(abuffer, abnode->scancmd);
    else
	return BZDOM;
    return 0;
}
