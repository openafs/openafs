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

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <direct.h>
#include <io.h>
#include <WINNT/afsevent.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#include <syslog.h>
#endif /* AFS_NT40_ENV */
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_globals.h>
#include "bosint.h"
#include "bnode.h"
#include "bosprototypes.h"
#include <rx/rxkad.h>
#include <rx/rxstat.h>
#include <afs/keys.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <afs/audit.h>
#include <afs/cellconfig.h>
#if defined(AFS_SGI_ENV)
#include <afs/afs_args.h>
#endif

#define BOZO_LWP_STACKSIZE	16000
extern struct bnode_ops fsbnode_ops, dafsbnode_ops, ezbnode_ops, cronbnode_ops;

struct afsconf_dir *bozo_confdir = 0;	/* bozo configuration dir */
static PROCESS bozo_pid;
const char *bozo_fileName;
FILE *bozo_logFile;
#ifndef AFS_NT40_ENV
static int bozo_argc = 0;
static char** bozo_argv = NULL;
#endif

const char *DoCore;
int DoLogging = 0;
int DoSyslog = 0;
const char *DoPidFiles = NULL;
#ifndef AFS_NT40_ENV
int DoSyslogFacility = LOG_DAEMON;
#endif
static afs_int32 nextRestart;
static afs_int32 nextDay;

struct ktime bozo_nextRestartKT, bozo_nextDayKT;
int bozo_newKTs;
int rxBind = 0;
int rxkadDisableDotCheck = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

int bozo_isrestricted = 0;
int bozo_restdisable = 0;

void
bozo_insecureme(int sig)
{
    signal(SIGFPE, bozo_insecureme);
    bozo_isrestricted = 0;
    bozo_restdisable = 1;
}

struct bztemp {
    FILE *file;
};

/* check whether caller is authorized to manage RX statistics */
int
bozo_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(bozo_confdir, call, NULL);
}

/* restart bozo process */
int
bozo_ReBozo(void)
{
#ifdef AFS_NT40_ENV
    /* exit with restart code; SCM integrator process will restart bosserver with
       the same arguments */
    exit(BOSEXIT_RESTART);
#else
    /* exec new bosserver process */
    int i = 0;

    /* close random fd's */
    for (i = 3; i < 64; i++) {
	close(i);
    }

    unlink(AFSDIR_SERVER_BOZRXBIND_FILEPATH);

    execv(bozo_argv[0], bozo_argv);	/* should not return */
    _exit(1);
#endif /* AFS_NT40_ENV */
}

/* make sure a dir exists */
static int
MakeDir(const char *adir)
{
    struct stat tstat;
    afs_int32 code;
    if (stat(adir, &tstat) < 0 || (tstat.st_mode & S_IFMT) != S_IFDIR) {
	int reqPerm;
	unlink(adir);
	reqPerm = GetRequiredDirPerm(adir);
	if (reqPerm == -1)
	    reqPerm = 0777;
#ifdef AFS_NT40_ENV
	/* underlying filesystem may not support directory protection */
	code = mkdir(adir);
#else
	code = mkdir(adir, reqPerm);
#endif
	return code;
    }
    return 0;
}

/* create all the bozo dirs */
static int
CreateDirs(const char *coredir)
{
    if ((!strncmp
	 (AFSDIR_USR_DIRPATH, AFSDIR_CLIENT_ETC_DIRPATH,
	  strlen(AFSDIR_USR_DIRPATH)))
	||
	(!strncmp
	 (AFSDIR_USR_DIRPATH, AFSDIR_SERVER_BIN_DIRPATH,
	  strlen(AFSDIR_USR_DIRPATH)))) {
	MakeDir(AFSDIR_USR_DIRPATH);
    }
    if (!strncmp
	(AFSDIR_SERVER_AFS_DIRPATH, AFSDIR_SERVER_BIN_DIRPATH,
	 strlen(AFSDIR_SERVER_AFS_DIRPATH))) {
	MakeDir(AFSDIR_SERVER_AFS_DIRPATH);
    }
    MakeDir(AFSDIR_SERVER_BIN_DIRPATH);
    MakeDir(AFSDIR_SERVER_ETC_DIRPATH);
    MakeDir(AFSDIR_SERVER_LOCAL_DIRPATH);
    MakeDir(AFSDIR_SERVER_DB_DIRPATH);
    MakeDir(AFSDIR_SERVER_LOGS_DIRPATH);
#ifndef AFS_NT40_ENV
    if (!strncmp
	(AFSDIR_CLIENT_VICE_DIRPATH, AFSDIR_CLIENT_ETC_DIRPATH,
	 strlen(AFSDIR_CLIENT_VICE_DIRPATH))) {
	MakeDir(AFSDIR_CLIENT_VICE_DIRPATH);
    }
    MakeDir(AFSDIR_CLIENT_ETC_DIRPATH);

    symlink(AFSDIR_SERVER_THISCELL_FILEPATH, AFSDIR_CLIENT_THISCELL_FILEPATH);
    symlink(AFSDIR_SERVER_CELLSERVDB_FILEPATH,
	    AFSDIR_CLIENT_CELLSERVDB_FILEPATH);
#endif /* AFS_NT40_ENV */
    if (coredir)
	MakeDir(coredir);
    return 0;
}

/* strip the \\n from the end of the line, if it is present */
static int
StripLine(char *abuffer)
{
    char *tp;

    tp = abuffer + strlen(abuffer);	/* starts off pointing at the null  */
    if (tp == abuffer)
	return 0;		/* null string, no last character to check */
    tp--;			/* aim at last character */
    if (*tp == '\n')
	*tp = 0;
    return 0;
}

/* write one bnode's worth of entry into the file */
static int
bzwrite(struct bnode *abnode, void *arock)
{
    struct bztemp *at = (struct bztemp *)arock;
    int i;
    char tbuffer[BOZO_BSSIZE];
    afs_int32 code;

    if (abnode->notifier)
	fprintf(at->file, "bnode %s %s %d %s\n", abnode->type->name,
		abnode->name, abnode->fileGoal, abnode->notifier);
    else
	fprintf(at->file, "bnode %s %s %d\n", abnode->type->name,
		abnode->name, abnode->fileGoal);
    for (i = 0;; i++) {
	code = bnode_GetParm(abnode, i, tbuffer, BOZO_BSSIZE);
	if (code) {
	    if (code != BZDOM)
		return code;
	    break;
	}
	fprintf(at->file, "parm %s\n", tbuffer);
    }
    fprintf(at->file, "end\n");
    return 0;
}

#define	MAXPARMS    20
int
ReadBozoFile(char *aname)
{
    FILE *tfile;
    char tbuffer[BOZO_BSSIZE];
    char *tp;
    char *instp, *typep, *notifier, *notp;
    afs_int32 code;
    afs_int32 ktmask, ktday, kthour, ktmin, ktsec;
    afs_int32 i, goal;
    struct bnode *tb;
    char *parms[MAXPARMS];
    char *thisparms[MAXPARMS];
    int rmode;

    /* rename BozoInit to BosServer for the user */
    if (!aname) {
	/* if BozoInit exists and BosConfig doesn't, try a rename */
	if (access(AFSDIR_SERVER_BOZINIT_FILEPATH, 0) == 0
	    && access(AFSDIR_SERVER_BOZCONF_FILEPATH, 0) != 0) {
	    code =
		renamefile(AFSDIR_SERVER_BOZINIT_FILEPATH,
			   AFSDIR_SERVER_BOZCONF_FILEPATH);
	    if (code < 0)
		perror("bosconfig rename");
	}
	if (access(AFSDIR_SERVER_BOZCONFNEW_FILEPATH, 0) == 0) {
	    code =
		renamefile(AFSDIR_SERVER_BOZCONFNEW_FILEPATH,
			   AFSDIR_SERVER_BOZCONF_FILEPATH);
	    if (code < 0)
		perror("bosconfig rename");
	}
    }

    /* don't do server restarts by default */
    bozo_nextRestartKT.mask = KTIME_NEVER;
    bozo_nextRestartKT.hour = 0;
    bozo_nextRestartKT.min = 0;
    bozo_nextRestartKT.day = 0;

    /* restart processes at 5am if their binaries have changed */
    bozo_nextDayKT.mask = KTIME_HOUR | KTIME_MIN;
    bozo_nextDayKT.hour = 5;
    bozo_nextDayKT.min = 0;

    for (code = 0; code < MAXPARMS; code++)
	parms[code] = NULL;
    instp = typep = notifier = NULL;
    tfile = (FILE *) 0;
    if (!aname)
	aname = (char *)bozo_fileName;
    tfile = fopen(aname, "r");
    if (!tfile)
	return 0;		/* -1 */
    instp = (char *)malloc(BOZO_BSSIZE);
    typep = (char *)malloc(BOZO_BSSIZE);
    notifier = notp = (char *)malloc(BOZO_BSSIZE);
    while (1) {
	/* ok, read lines giving parms and such from the file */
	tp = fgets(tbuffer, sizeof(tbuffer), tfile);
	if (tp == (char *)0)
	    break;		/* all done */

	if (strncmp(tbuffer, "restarttime", 11) == 0) {
	    code =
		sscanf(tbuffer, "restarttime %d %d %d %d %d", &ktmask, &ktday,
		       &kthour, &ktmin, &ktsec);
	    if (code != 5) {
		code = -1;
		goto fail;
	    }
	    /* otherwise we've read in the proper ktime structure; now assign
	     * it and continue processing */
	    bozo_nextRestartKT.mask = ktmask;
	    bozo_nextRestartKT.day = ktday;
	    bozo_nextRestartKT.hour = kthour;
	    bozo_nextRestartKT.min = ktmin;
	    bozo_nextRestartKT.sec = ktsec;
	    continue;
	}

	if (strncmp(tbuffer, "checkbintime", 12) == 0) {
	    code =
		sscanf(tbuffer, "checkbintime %d %d %d %d %d", &ktmask,
		       &ktday, &kthour, &ktmin, &ktsec);
	    if (code != 5) {
		code = -1;
		goto fail;
	    }
	    /* otherwise we've read in the proper ktime structure; now assign
	     * it and continue processing */
	    bozo_nextDayKT.mask = ktmask;	/* time to restart the system */
	    bozo_nextDayKT.day = ktday;
	    bozo_nextDayKT.hour = kthour;
	    bozo_nextDayKT.min = ktmin;
	    bozo_nextDayKT.sec = ktsec;
	    continue;
	}

	if (strncmp(tbuffer, "restrictmode", 12) == 0) {
	    code = sscanf(tbuffer, "restrictmode %d", &rmode);
	    if (code != 1) {
		code = -1;
		goto fail;
	    }
	    if (rmode != 0 && rmode != 1) {
		code = -1;
		goto fail;
	    }
	    bozo_isrestricted = rmode;
	    continue;
	}

	if (strncmp("bnode", tbuffer, 5) != 0) {
	    code = -1;
	    goto fail;
	}
	notifier = notp;
	code =
	    sscanf(tbuffer, "bnode %s %s %d %s", typep, instp, &goal,
		   notifier);
	if (code < 3) {
	    code = -1;
	    goto fail;
	} else if (code == 3)
	    notifier = NULL;

	memset(thisparms, 0, sizeof(thisparms));

	for (i = 0; i < MAXPARMS; i++) {
	    /* now read the parms, until we see an "end" line */
	    tp = fgets(tbuffer, sizeof(tbuffer), tfile);
	    if (!tp) {
		code = -1;
		goto fail;
	    }
	    StripLine(tbuffer);
	    if (!strncmp(tbuffer, "end", 3))
		break;
	    if (strncmp(tbuffer, "parm ", 5)) {
		code = -1;
		goto fail;	/* no "parm " either */
	    }
	    if (!parms[i])	/* make sure there's space */
		parms[i] = (char *)malloc(BOZO_BSSIZE);
	    strcpy(parms[i], tbuffer + 5);	/* remember the parameter for later */
	    thisparms[i] = parms[i];
	}

	/* ok, we have the type and parms, now create the object */
	code =
	    bnode_Create(typep, instp, &tb, thisparms[0], thisparms[1],
			 thisparms[2], thisparms[3], thisparms[4], notifier,
			 goal ? BSTAT_NORMAL : BSTAT_SHUTDOWN, 0);
	if (code)
	    goto fail;

	/* bnode created in 'temporarily shutdown' state;
	 * check to see if we are supposed to run this guy,
	 * and if so, start the process up */
	if (goal) {
	    bnode_SetStat(tb, BSTAT_NORMAL);	/* set goal, taking effect immediately */
	} else {
	    bnode_SetStat(tb, BSTAT_SHUTDOWN);
	}
    }
    /* all done */
    code = 0;

  fail:
    if (instp)
	free(instp);
    if (typep)
	free(typep);
    for (i = 0; i < MAXPARMS; i++)
	if (parms[i])
	    free(parms[i]);
    if (tfile)
	fclose(tfile);
    return code;
}

/* write a new bozo file */
int
WriteBozoFile(char *aname)
{
    FILE *tfile;
    char tbuffer[AFSDIR_PATH_MAX];
    afs_int32 code;
    struct bztemp btemp;

    if (!aname)
	aname = (char *)bozo_fileName;
    strcpy(tbuffer, aname);
    strcat(tbuffer, ".NBZ");
    tfile = fopen(tbuffer, "w");
    if (!tfile)
	return -1;
    btemp.file = tfile;

    fprintf(tfile, "restrictmode %d\n", bozo_isrestricted);
    fprintf(tfile, "restarttime %d %d %d %d %d\n", bozo_nextRestartKT.mask,
	    bozo_nextRestartKT.day, bozo_nextRestartKT.hour,
	    bozo_nextRestartKT.min, bozo_nextRestartKT.sec);
    fprintf(tfile, "checkbintime %d %d %d %d %d\n", bozo_nextDayKT.mask,
	    bozo_nextDayKT.day, bozo_nextDayKT.hour, bozo_nextDayKT.min,
	    bozo_nextDayKT.sec);
    code = bnode_ApplyInstance(bzwrite, &btemp);
    if (code || (code = ferror(tfile))) {	/* something went wrong */
	fclose(tfile);
	unlink(tbuffer);
	return code;
    }
    /* close the file, check for errors and snap new file into place */
    if (fclose(tfile) == EOF) {
	unlink(tbuffer);
	return -1;
    }
    code = renamefile(tbuffer, aname);
    if (code) {
	unlink(tbuffer);
	return -1;
    }
    return 0;
}

static int
bdrestart(struct bnode *abnode, void *arock)
{
    afs_int32 code;

    if (abnode->fileGoal != BSTAT_NORMAL || abnode->goal != BSTAT_NORMAL)
	return 0;		/* don't restart stopped bnodes */
    bnode_Hold(abnode);
    code = bnode_RestartP(abnode);
    if (code) {
	/* restart the dude */
	bnode_SetStat(abnode, BSTAT_SHUTDOWN);
	bnode_WaitStatus(abnode, BSTAT_SHUTDOWN);
	bnode_SetStat(abnode, BSTAT_NORMAL);
    }
    bnode_Release(abnode);
    return 0;			/* keep trying all bnodes */
}

#define	BOZO_MINSKIP 3600	/* minimum to advance clock */
/* lwp to handle system restarts */
static void *
BozoDaemon(void *unused)
{
    afs_int32 now;

    /* now initialize the values */
    bozo_newKTs = 1;
    while (1) {
	IOMGR_Sleep(60);
	now = FT_ApproxTime();

	if (bozo_restdisable) {
	    bozo_Log("Restricted mode disabled by signal\n");
	    bozo_restdisable = 0;
	}

	if (bozo_newKTs) {	/* need to recompute restart times */
	    bozo_newKTs = 0;	/* done for a while */
	    nextRestart = ktime_next(&bozo_nextRestartKT, BOZO_MINSKIP);
	    nextDay = ktime_next(&bozo_nextDayKT, BOZO_MINSKIP);
	}

	/* see if we should do a restart */
	if (now > nextRestart) {
	    SBOZO_ReBozo(0);	/* doesn't come back */
	}

	/* see if we should restart a server */
	if (now > nextDay) {
	    nextDay = ktime_next(&bozo_nextDayKT, BOZO_MINSKIP);

	    /* call the bnode restartp function, and restart all that require it */
	    bnode_ApplyInstance(bdrestart, 0);
	}
    }
    return NULL;
}

#ifdef AFS_AIX32_ENV
static int
tweak_config(void)
{
    FILE *f;
    char c[80];
    int s, sb_max, ipfragttl;

    sb_max = 131072;
    ipfragttl = 20;
    f = popen("/usr/sbin/no -o sb_max", "r");
    s = fscanf(f, "sb_max = %d", &sb_max);
    fclose(f);
    if (s < 1)
	return;
    f = popen("/usr/sbin/no -o ipfragttl", "r");
    s = fscanf(f, "ipfragttl = %d", &ipfragttl);
    fclose(f);
    if (s < 1)
	ipfragttl = 20;

    if (sb_max < 131072)
	sb_max = 131072;
    if (ipfragttl > 20)
	ipfragttl = 20;

    sprintf(c, "/usr/sbin/no -o sb_max=%d -o ipfragttl=%d", sb_max,
	    ipfragttl);
    f = popen(c, "r");
    fclose(f);
}
#endif

static char *
make_pid_filename(char *ainst, char *aname)
{
    char *buffer = NULL;
    int length;

    length = strlen(DoPidFiles) + strlen(ainst) + 6;
    if (aname && *aname) {
	length += strlen(aname) + 1;
    }
    buffer = malloc(length * sizeof(char));
    if (!buffer) {
	if (aname) {
	    bozo_Log("Failed to alloc pid filename buffer for %s.%s.\n",
		     ainst, aname);
	} else {
	    bozo_Log("Failed to alloc pid filename buffer for %s.\n", ainst);
	}
    } else {
	if (aname && *aname) {
	    snprintf(buffer, length, "%s/%s.%s.pid", DoPidFiles, ainst,
		     aname);
	} else {
	    snprintf(buffer, length, "%s/%s.pid", DoPidFiles, ainst);
	}
    }
    return buffer;
}

/**
 * Write a file containing the pid of the named process.
 *
 * @param ainst instance name
 * @param aname sub-process name of the instance, may be null
 * @param apid  process id of the newly started process
 *
 * @returns status
 */
int
bozo_CreatePidFile(char *ainst, char *aname, pid_t apid)
{
    int code = 0;
    char *pidfile = NULL;
    FILE *fp;

    pidfile = make_pid_filename(ainst, aname);
    if (!pidfile) {
	return ENOMEM;
    }
    if ((fp = fopen(pidfile, "w")) == NULL) {
	bozo_Log("Failed to open pidfile %s; errno=%d\n", pidfile, errno);
	free(pidfile);
	return errno;
    }
    if (fprintf(fp, "%ld\n", afs_printable_int32_ld(apid)) < 0) {
	code = errno;
    }
    if (fclose(fp) != 0) {
	code = errno;
    }
    free(pidfile);
    return code;
}

/**
 * Clean a pid file for a process which just exited.
 *
 * @param ainst instance name
 * @param aname sub-process name of the instance, may be null
 *
 * @returns status
 */
int
bozo_DeletePidFile(char *ainst, char *aname)
{
    char *pidfile = NULL;
    pidfile = make_pid_filename(ainst, aname);
    if (pidfile) {
	unlink(pidfile);
	free(pidfile);
    }
    return 0;
}

/**
 * Create the rxbind file of this bosserver.
 *
 * @param host  bind address of this server
 *
 * @returns status
 */
void
bozo_CreateRxBindFile(afs_uint32 host)
{
    char buffer[16];
    FILE *fp;

    afs_inet_ntoa_r(host, buffer);
    bozo_Log("Listening on %s:%d\n", buffer, AFSCONF_NANNYPORT);
    if ((fp = fopen(AFSDIR_SERVER_BOZRXBIND_FILEPATH, "w")) == NULL) {
	bozo_Log("Unable to open rxbind address file: %s, code=%d\n",
		 AFSDIR_SERVER_BOZRXBIND_FILEPATH, errno);
    } else {
	/* If listening on any interface, write the loopback interface
	   to the rxbind file to give local scripts a usable addresss. */
	if (host == htonl(INADDR_ANY)) {
	    afs_inet_ntoa_r(htonl(0x7f000001), buffer);
	}
	fprintf(fp, "%s\n", buffer);
	fclose(fp);
    }
}

/* start a process and monitor it */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv, char **envp)
{
    struct rx_service *tservice;
    afs_int32 code;
    struct afsconf_dir *tdir;
    int noAuth = 0;
    int i;
    char namebuf[AFSDIR_PATH_MAX];
    int rxMaxMTU = -1;
    afs_uint32 host = htonl(INADDR_ANY);
    char *auditFileName = NULL;
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
#ifndef AFS_NT40_ENV
    int nofork = 0;
    struct stat sb;
#endif
#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    /* for some reason, this permits user-mode RX to run a lot faster.
     * we do it here in the bosserver, so we don't have to do it
     * individually in each server.
     */
    tweak_config();

    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
    sigaction(SIGABRT, &nsa, NULL);
#endif
    osi_audit_init();
    signal(SIGFPE, bozo_insecureme);

#ifdef AFS_NT40_ENV
    /* Initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "%s: Couldn't initialize winsock.\n", argv[0]);
	exit(2);
    }
#endif

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    /* some path inits */
    bozo_fileName = AFSDIR_SERVER_BOZCONF_FILEPATH;
    DoCore = AFSDIR_SERVER_LOGS_DIRPATH;

    /* initialize the list of dirpaths that the bosserver has
     * an interest in monitoring */
    initBosEntryStats();

#if defined(AFS_SGI_ENV)
    /* offer some protection if AFS isn't loaded */
    if (syscall(AFS_SYSCALL, AFSOP_ENDLOG) < 0 && errno == ENOPKG) {
	printf("bosserver: AFS doesn't appear to be configured in O.S..\n");
	exit(1);
    }
#endif

#ifndef AFS_NT40_ENV
    /* save args for restart */
    bozo_argc = argc;
    bozo_argv = malloc((argc+1) * sizeof(char*));
    if (!bozo_argv) {
	fprintf(stderr, "%s: Failed to allocate argument list.\n", argv[0]);
	exit(1);
    }
    bozo_argv[0] = (char*)AFSDIR_SERVER_BOSVR_FILEPATH; /* expected path */
    bozo_argv[bozo_argc] = NULL; /* null terminate list */
#endif	/* AFS_NT40_ENV */

    /* parse cmd line */
    for (code = 1; code < argc; code++) {
#ifndef AFS_NT40_ENV
	bozo_argv[code] = argv[code];
#endif	/* AFS_NT40_ENV */
	if (strcmp(argv[code], "-noauth") == 0) {
	    /* set noauth flag */
	    noAuth = 1;
	} else if (strcmp(argv[code], "-log") == 0) {
	    /* set extra logging flag */
	    DoLogging = 1;
	}
#ifndef AFS_NT40_ENV
	else if (strcmp(argv[code], "-syslog") == 0) {
	    /* set syslog logging flag */
	    DoSyslog = 1;
	} else if (strncmp(argv[code], "-syslog=", 8) == 0) {
	    DoSyslog = 1;
	    DoSyslogFacility = atoi(argv[code] + 8);
	} else if (strncmp(argv[code], "-cores=", 7) == 0) {
	    if (strcmp((argv[code]+7), "none") == 0)
		DoCore = 0;
	    else
		DoCore = (argv[code]+7);
	} else if (strcmp(argv[code], "-nofork") == 0) {
	    nofork = 1;
	}
#endif
	else if (strcmp(argv[code], "-enable_peer_stats") == 0) {
	    rx_enablePeerRPCStats();
	} else if (strcmp(argv[code], "-enable_process_stats") == 0) {
	    rx_enableProcessRPCStats();
	}
	else if (strcmp(argv[code], "-restricted") == 0) {
	    bozo_isrestricted = 1;
	}
	else if (strcmp(argv[code], "-rxbind") == 0) {
	    rxBind = 1;
	}
	else if (strcmp(argv[code], "-allow-dotted-principals") == 0) {
	    rxkadDisableDotCheck = 1;
	}
	else if (!strcmp(argv[code], "-rxmaxmtu")) {
	    if ((code + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n");
		exit(1);
	    }
	    rxMaxMTU = atoi(argv[++code]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) ||
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d invalid; must be between %d-%" AFS_SIZET_FMT "\n",
			rxMaxMTU, RX_MIN_PACKET_SIZE,
			RX_MAX_PACKET_DATA_SIZE);
		exit(1);
	    }
	}
	else if (strcmp(argv[code], "-auditlog") == 0) {
	    auditFileName = argv[++code];

	} else if (strcmp(argv[code], "-audit-interface") == 0) {
	    char *interface = argv[++code];

	    if (osi_audit_interface(interface)) {
		printf("Invalid audit interface '%s'\n", interface);
		exit(1);
	    }
	} else if (strncmp(argv[code], "-pidfiles=", 10) == 0) {
	    DoPidFiles = (argv[code]+10);
	} else if (strncmp(argv[code], "-pidfiles", 9) == 0) {
	    DoPidFiles = AFSDIR_LOCAL_DIR;
	}
	else {

	    /* hack to support help flag */

#ifndef AFS_NT40_ENV
	    printf("Usage: bosserver [-noauth] [-log] "
		   "[-auditlog <log path>] "
		   "[-audit-interface <file|sysvmq> (default is file)] "
		   "[-rxmaxmtu <bytes>] [-rxbind] [-allow-dotted-principals]"
		   "[-syslog[=FACILITY]] "
		   "[-restricted] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-cores=<none|path>] \n"
		   "[-pidfiles[=path]] "
		   "[-nofork] " "[-help]\n");
#else
	    printf("Usage: bosserver [-noauth] [-log] "
		   "[-auditlog <log path>] "
		   "[-audit-interface <file|sysvmq> (default is file)] "
		   "[-rxmaxmtu <bytes>] [-rxbind] [-allow-dotted-principals]"
		   "[-restricted] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-cores=<none|path>] \n"
		   "[-pidfiles[=path]] "
		   "[-help]\n");
#endif
	    fflush(stdout);

	    exit(0);
	}
    }
    if (auditFileName) {
	osi_audit_file(auditFileName);
    }

#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	printf("bosserver: must be run as root.\n");
	exit(1);
    }
#endif

    code = bnode_Init();
    if (code) {
	printf("bosserver: could not init bnode package, code %d\n", code);
	exit(1);
    }

    bnode_Register("fs", &fsbnode_ops, 3);
    bnode_Register("dafs", &dafsbnode_ops, 4);
    bnode_Register("simple", &ezbnode_ops, 1);
    bnode_Register("cron", &cronbnode_ops, 2);

    /* create useful dirs */
    CreateDirs(DoCore);

    /* chdir to AFS log directory */
    if (DoCore)
	chdir(DoCore);
    else
	chdir(AFSDIR_SERVER_LOGS_DIRPATH);

    /* go into the background and remove our controlling tty, close open
       file desriptors
     */

#ifndef AFS_NT40_ENV
    if (!nofork)
	daemon(1, 0);
#endif /* ! AFS_NT40_ENV */

    if ((!DoSyslog)
#ifndef AFS_NT40_ENV
	&& ((lstat(AFSDIR_BOZLOG_FILE, &sb) == 0) &&
	!(S_ISFIFO(sb.st_mode)))
#endif
	) {
	strcpy(namebuf, AFSDIR_BOZLOG_FILE);
	strcat(namebuf, ".old");
	renamefile(AFSDIR_BOZLOG_FILE, namebuf);	/* try rename first */
	bozo_logFile = fopen(AFSDIR_BOZLOG_FILE, "a");
	if (!bozo_logFile) {
	    printf("bosserver: can't initialize log file (%s).\n",
		   AFSDIR_SERVER_BOZLOG_FILEPATH);
	    exit(1);
	}
	/* keep log closed normally, so can be removed */
	fclose(bozo_logFile);
    } else {
#ifndef AFS_NT40_ENV
	openlog("bosserver", LOG_PID, DoSyslogFacility);
#endif
    }

#if defined(RLIMIT_CORE) && defined(HAVE_GETRLIMIT)
    {
      struct rlimit rlp;
      getrlimit(RLIMIT_CORE, &rlp);
      if (!DoCore)
	  rlp.rlim_cur = 0;
      else
	  rlp.rlim_max = rlp.rlim_cur = RLIM_INFINITY;
      setrlimit(RLIMIT_CORE, &rlp);
      getrlimit(RLIMIT_CORE, &rlp);
      bozo_Log("Core limits now %d %d\n",(int)rlp.rlim_cur,(int)rlp.rlim_max);
    }
#endif

    /* Write current state of directory permissions to log file */
    DirAccessOK();

    if (rxBind) {
	afs_int32 ccode;
	if (AFSDIR_SERVER_NETRESTRICT_FILEPATH ||
	    AFSDIR_SERVER_NETINFO_FILEPATH) {
	    char reason[1024];
	    ccode = parseNetFiles(SHostAddrs, NULL, NULL,
	                          ADDRSPERSITE, reason,
	                          AFSDIR_SERVER_NETINFO_FILEPATH,
	                          AFSDIR_SERVER_NETRESTRICT_FILEPATH);
        } else {
            ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
        }
        if (ccode == 1)
            host = SHostAddrs[0];
    }

    for (i = 0; i < 10; i++) {
	if (rxBind) {
	    code = rx_InitHost(host, htons(AFSCONF_NANNYPORT));
	} else {
	    code = rx_Init(htons(AFSCONF_NANNYPORT));
	}
	if (code) {
	    bozo_Log("can't initialize rx: code=%d\n", code);
	    sleep(3);
	} else
	    break;
    }
    if (i >= 10) {
	bozo_Log("Bos giving up, can't initialize rx\n");
	exit(code);
    }

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }

    code = LWP_CreateProcess(BozoDaemon, BOZO_LWP_STACKSIZE, /* priority */ 1,
			     /* param */ NULL , "bozo-the-clown",
			     &bozo_pid);
    if (code) {
	bozo_Log("Failed to create daemon thread\n");
        exit(1);
    }

    /* try to read the key from the config file */
    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	/* try to create local cell config file */
	struct afsconf_cell tcell;
	strcpy(tcell.name, "localcell");
	tcell.numServers = 1;
	code = gethostname(tcell.hostName[0], MAXHOSTCHARS);
	if (code) {
	    bozo_Log("failed to get hostname, code %d\n", errno);
	    exit(1);
	}
	if (tcell.hostName[0][0] == 0) {
	    bozo_Log("host name not set, can't start\n");
	    bozo_Log("try the 'hostname' command\n");
	    exit(1);
	}
	memset(tcell.hostAddr, 0, sizeof(tcell.hostAddr));	/* not computed */
	code =
	    afsconf_SetCellInfo(bozo_confdir, AFSDIR_SERVER_ETC_DIRPATH,
				&tcell);
	if (code) {
	    bozo_Log
		("could not create cell database in '%s' (code %d), quitting\n",
		 AFSDIR_SERVER_ETC_DIRPATH, code);
	    exit(1);
	}
	tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
	if (!tdir) {
	    bozo_Log
		("failed to open newly-created cell database, quitting\n");
	    exit(1);
	}
    }

    /* read init file, starting up programs */
    if ((code = ReadBozoFile(0))) {
	bozo_Log
	    ("bosserver: Something is wrong (%d) with the bos configuration file %s; aborting\n",
	     code, AFSDIR_SERVER_BOZCONF_FILEPATH);
	exit(code);
    }

    bozo_CreateRxBindFile(host);	/* for local scripts */

    /* opened the cell databse */
    bozo_confdir = tdir;

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(bozo_rxstat_userok);

    afsconf_SetNoAuthFlag(tdir, noAuth);
    afsconf_BuildServerSecurityObjects(tdir, 0, &securityClasses, &numClasses);

    if (DoPidFiles) {
	bozo_CreatePidFile("bosserver", NULL, getpid());
    }

    tservice = rx_NewServiceHost(host, 0, /* service id */ 1,
			         "bozo", securityClasses, numClasses,
				 BOZO_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_SetStackSize(tservice, BOZO_LWP_STACKSIZE);	/* so gethostbyname works (in cell stuff) */
    if (rxkadDisableDotCheck) {
        rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
                                    (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
    }

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats",
			  securityClasses, numClasses, RXSTATS_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_StartServer(1);		/* donate this process */
    return 0;
}

void
bozo_Log(const char *format, ...)
{
    char tdate[27];
    time_t myTime;
    va_list ap;

    va_start(ap, format);

    if (DoSyslog) {
#ifndef AFS_NT40_ENV
        vsyslog(LOG_INFO, format, ap);
#endif
    } else {
	myTime = time(0);
	strcpy(tdate, ctime(&myTime));	/* copy out of static area asap */
	tdate[24] = ':';

	/* log normally closed, so can be removed */

	bozo_logFile = fopen(AFSDIR_SERVER_BOZLOG_FILEPATH, "a");
	if (bozo_logFile == NULL) {
	    printf("bosserver: WARNING: problem with %s\n",
		   AFSDIR_SERVER_BOZLOG_FILEPATH);
	    printf("%s ", tdate);
	    vprintf(format, ap);
	    fflush(stdout);
	} else {
	    fprintf(bozo_logFile, "%s ", tdate);
	    vfprintf(bozo_logFile, format, ap);

	    /* close so rm BosLog works */
	    fclose(bozo_logFile);
	}
    }
}
