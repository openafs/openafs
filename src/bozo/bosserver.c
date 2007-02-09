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

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <rx/rxkad.h>
#include <afs/keys.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#if defined(AFS_SGI_ENV)
#include <afs/afs_args.h>
#endif


#define BOZO_LWP_STACKSIZE	16000
extern int BOZO_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();
extern struct bnode_ops fsbnode_ops, dafsbnode_ops, ezbnode_ops, cronbnode_ops;

void bozo_Log();

struct afsconf_dir *bozo_confdir = 0;	/* bozo configuration dir */
static PROCESS bozo_pid;
struct rx_securityClass *bozo_rxsc[3];
const char *bozo_fileName;
FILE *bozo_logFile;

int DoLogging = 0;
int DoSyslog = 0;
#ifndef AFS_NT40_ENV
int DoSyslogFacility = LOG_DAEMON;
#endif
static afs_int32 nextRestart;
static afs_int32 nextDay;

struct ktime bozo_nextRestartKT, bozo_nextDayKT;
int bozo_newKTs;
int rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

#ifdef BOS_RESTRICTED_MODE
int bozo_isrestricted = 0;
int bozo_restdisable = 0;

void
bozo_insecureme(int sig)
{
    signal(SIGFPE, bozo_insecureme);
    bozo_isrestricted = 0;
    bozo_restdisable = 1;
}
#endif

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
bozo_ReBozo()
{
#ifdef AFS_NT40_ENV
    /* exit with restart code; SCM integrator process will restart bosserver */
    int status = BOSEXIT_RESTART;

    /* if noauth flag is set, pass "-noauth" to new bosserver */
    if (afsconf_GetNoAuthFlag(bozo_confdir)) {
	status |= BOSEXIT_NOAUTH_FLAG;
    }
    /* if logging is on, pass "-log" to new bosserver */
    if (DoLogging) {
	status |= BOSEXIT_LOGGING_FLAG;
    }
    exit(status);
#else
    /* exec new bosserver process */
    char *argv[4];
    int i = 0;

    argv[i] = (char *)AFSDIR_SERVER_BOSVR_FILEPATH;
    i++;

    /* if noauth flag is set, pass "-noauth" to new bosserver */
    if (afsconf_GetNoAuthFlag(bozo_confdir)) {
	argv[i] = "-noauth";
	i++;
    }
    /* if logging is on, pass "-log" to new bosserver */
    if (DoLogging) {
	argv[i] = "-log";
	i++;
    }
#ifndef AFS_NT40_ENV
    /* if syslog logging is on, pass "-syslog" to new bosserver */
    if (DoSyslog) {
	char *arg = (char *)malloc(40);	/* enough for -syslog=# */
	if (DoSyslogFacility != LOG_DAEMON) {
	    snprintf(arg, 40, "-syslog=%d", DoSyslogFacility);
	} else {
	    strcpy(arg, "-syslog");
	}
	argv[i] = arg;
	i++;
    }
#endif

    /* null-terminate argument list */
    argv[i] = NULL;

    /* close random fd's */
    for (i = 3; i < 64; i++) {
	close(i);
    }

    execv(argv[0], argv);	/* should not return */
    _exit(1);
#endif /* AFS_NT40_ENV */
}

/* make sure a dir exists */
static int
MakeDir(const char *adir)
{
    struct stat tstat;
    register afs_int32 code;
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
CreateDirs()
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
    return 0;
}

/* strip the \\n from the end of the line, if it is present */
static int
StripLine(register char *abuffer)
{
    register char *tp;

    tp = abuffer + strlen(abuffer);	/* starts off pointing at the null  */
    if (tp == abuffer)
	return 0;		/* null string, no last character to check */
    tp--;			/* aim at last character */
    if (*tp == '\n')
	*tp = 0;
    return 0;
}

/* write one bnode's worth of entry into the file */
static
bzwrite(register struct bnode *abnode, register struct bztemp *at)
{
    register int i;
    char tbuffer[BOZO_BSSIZE];
    register afs_int32 code;

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
    register FILE *tfile;
    char tbuffer[BOZO_BSSIZE];
    register char *tp;
    char *instp, *typep, *notifier, *notp;
    register afs_int32 code;
    afs_int32 ktmask, ktday, kthour, ktmin, ktsec;
    afs_int32 i, goal;
    struct bnode *tb;
    char *parms[MAXPARMS];
#ifdef BOS_RESTRICTED_MODE
    int rmode;
#endif

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
#ifdef BOS_NEW_CONFIG
	if (access(AFSDIR_SERVER_BOZCONFNEW_FILEPATH, 0) == 0) {
	    code =
		renamefile(AFSDIR_SERVER_BOZCONFNEW_FILEPATH,
			   AFSDIR_SERVER_BOZCONF_FILEPATH);
	    if (code < 0)
		perror("bosconfig rename");
	}
#endif
    }

    /* setup default times we want to do restarts */
    bozo_nextRestartKT.mask = KTIME_HOUR | KTIME_MIN | KTIME_DAY;
    bozo_nextRestartKT.hour = 4;	/* 4 am */
    bozo_nextRestartKT.min = 0;
    bozo_nextRestartKT.day = 0;	/* Sunday */
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
#ifdef BOS_RESTRICTED_MODE
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
#endif

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
	}

	/* ok, we have the type and parms, now create the object */
	code =
	    bnode_Create(typep, instp, &tb, parms[0], parms[1], parms[2],
			 parms[3], parms[4], notifier,
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
    register FILE *tfile;
    char tbuffer[AFSDIR_PATH_MAX];
    register afs_int32 code;
    struct bztemp btemp;

    if (!aname)
	aname = (char *)bozo_fileName;
    strcpy(tbuffer, aname);
    strcat(tbuffer, ".NBZ");
    tfile = fopen(tbuffer, "w");
    if (!tfile)
	return -1;
    btemp.file = tfile;
#ifdef BOS_RESTRICTED_MODE
    fprintf(tfile, "restrictmode %d\n", bozo_isrestricted);
#endif
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
bdrestart(register struct bnode *abnode, char *arock)
{
    register afs_int32 code;

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
static int
BozoDaemon()
{
    register afs_int32 now;

    /* now initialize the values */
    bozo_newKTs = 1;
    while (1) {
	IOMGR_Sleep(60);
	now = FT_ApproxTime();

#ifdef BOS_RESTRICTED_MODE
	if (bozo_restdisable) {
	    bozo_Log("Restricted mode disabled by signal\n");
	    bozo_restdisable = 0;
	}
#endif
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
}

#ifdef AFS_AIX32_ENV
static int
tweak_config()
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

#if 0
/*
 * This routine causes the calling process to go into the background and
 * to lose its controlling tty.
 *
 * It does not close or otherwise alter the standard file descriptors.
 *
 * It writes warning messages to the standard error output if certain
 * fundamental errors occur.
 *
 * This routine requires
 * 
 * #include <sys/types.h>
 * #include <sys/stat.h>
 * #include <fcntl.h>
 * #include <unistd.h>
 * #include <stdlib.h>
 *
 * and has been tested on:
 *
 * AIX 4.2
 * Digital Unix 4.0D
 * HP-UX 11.0
 * IRIX 6.5
 * Linux 2.1.125
 * Solaris 2.5
 * Solaris 2.6
 */

#ifndef AFS_NT40_ENV
static void
background(void)
{
    /* 
     * A process is a process group leader if its process ID
     * (getpid()) and its process group ID (getpgrp()) are the same.
     */

    /*
     * To create a new session (and thereby lose our controlling
     * terminal) we cannot be a process group leader.
     *
     * To guarantee we are not a process group leader, we fork and
     * let the parent process exit.
     */

    if (getpid() == getpgrp()) {
	pid_t pid;
	pid = fork();
	switch (pid) {
	case -1:
	    abort();		/* leave footprints */
	    break;
	case 0:		/* child */
	    break;
	default:		/* parent */
	    exit(0);
	    break;
	}
    }

    /*
     * By here, we are not a process group leader, so we can make a
     * new session and become the session leader.
     */

    {
	pid_t sid = setsid();

	if (sid == -1) {
	    static char err[] = "bosserver: WARNING: setsid() failed\n";
	    write(STDERR_FILENO, err, sizeof err - 1);
	}
    }

    /*
     * Once we create a new session, the current process is a
     * session leader without a controlling tty.
     *
     * On some systems, the first tty device the session leader
     * opens automatically becomes the controlling tty for the
     * session.
     *
     * So, to guarantee we do not acquire a controlling tty, we fork
     * and let the parent process exit.  The child process is not a
     * session leader, and so it will not acquire a controlling tty
     * even if it should happen to open a tty device.
     */

    if (getpid() == getpgrp()) {
	pid_t pid;
	pid = fork();
	switch (pid) {
	case -1:
	    abort();		/* leave footprints */
	    break;
	case 0:		/* child */
	    break;
	default:		/* parent */
	    exit(0);
	    break;
	}
    }

    /*
     * check that we no longer have a controlling tty
     */

    {
	int fd;

	fd = open("/dev/tty", O_RDONLY);

	if (fd >= 0) {
	    static char err[] =
		"bosserver: WARNING: /dev/tty still attached\n";
	    close(fd);
	    write(STDERR_FILENO, err, sizeof err - 1);
	}
    }
}
#endif /* ! AFS_NT40_ENV */
#endif

/* start a process and monitor it */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv, char **envp)
{
    struct rx_service *tservice;
    register afs_int32 code;
    struct afsconf_dir *tdir;
    int noAuth = 0;
    int i;
    char namebuf[AFSDIR_PATH_MAX];
    int rxMaxMTU = -1;
    afs_uint32 host = htonl(INADDR_ANY);
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
#ifdef BOS_RESTRICTED_MODE
    signal(SIGFPE, bozo_insecureme);
#endif

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

    /* parse cmd line */
    for (code = 1; code < argc; code++) {
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
	} else if (strcmp(argv[code], "-nofork") == 0) {
	    nofork = 1;
	}
#endif
	else if (strcmp(argv[code], "-enable_peer_stats") == 0) {
	    rx_enablePeerRPCStats();
	} else if (strcmp(argv[code], "-enable_process_stats") == 0) {
	    rx_enableProcessRPCStats();
	}
#ifdef BOS_RESTRICTED_MODE
	else if (strcmp(argv[code], "-restricted") == 0) {
	    bozo_isrestricted = 1;
	}
#endif
	else if (strcmp(argv[code], "-rxbind") == 0) {
	    rxBind = 1;
	}
	else if (!strcmp(argv[i], "-rxmaxmtu")) {
	    if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n"); 
		exit(1); 
	    }
	    rxMaxMTU = atoi(argv[++i]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) || 
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d% invalid; must be between %d-%d\n",
			rxMaxMTU, RX_MIN_PACKET_SIZE, 
			RX_MAX_PACKET_DATA_SIZE);
		exit(1);
	    }
	}
	else if (strcmp(argv[code], "-auditlog") == 0) {
	    int tempfd, flags;
	    FILE *auditout;
	    char oldName[MAXPATHLEN];
	    char *fileName = argv[++code];

#ifndef AFS_NT40_ENV
	    struct stat statbuf;
	    
	    if ((lstat(fileName, &statbuf) == 0) 
		&& (S_ISFIFO(statbuf.st_mode))) {
		flags = O_WRONLY | O_NONBLOCK;
	    } else 
#endif
	    {
		strcpy(oldName, fileName);
		strcat(oldName, ".old");
		renamefile(fileName, oldName);
		flags = O_WRONLY | O_TRUNC | O_CREAT;
	    }
	    tempfd = open(fileName, flags, 0666);
	    if (tempfd > -1) {
		auditout = fdopen(tempfd, "a");
		if (auditout) {
		    osi_audit_file(auditout);
		} else
		    printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	    } else
		printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	}
	else {

	    /* hack to support help flag */

#ifndef AFS_NT40_ENV
	    printf("Usage: bosserver [-noauth] [-log] "
		   "[-auditlog <log path>] "
		   "[-rxmaxmtu <bytes>] [-rxbind] "
		   "[-syslog[=FACILITY]] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-nofork] " "[-help]\n");
#else
	    printf("Usage: bosserver [-noauth] [-log] "
		   "[-auditlog <log path>] "
		   "[-rxmaxmtu <bytes>] [-rxbind] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#endif
	    fflush(stdout);

	    exit(0);
	}
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
    CreateDirs();

    /* chdir to AFS log directory */
    chdir(AFSDIR_SERVER_LOGS_DIRPATH);

#if 0
    fputs(AFS_GOVERNMENT_MESSAGE, stdout);
    fflush(stdout);
#endif

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

    /* Write current state of directory permissions to log file */
    DirAccessOK();

    for (i = 0; i < 10; i++) {
	code = rx_Init(htons(AFSCONF_NANNYPORT));
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

    code = LWP_CreateProcess(BozoDaemon, BOZO_LWP_STACKSIZE, /* priority */ 1,
			     (void *) /*parm */ 0, "bozo-the-clown",
			     &bozo_pid);

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
    if (code = ReadBozoFile(0)) {
	bozo_Log
	    ("bosserver: Something is wrong (%d) with the bos configuration file %s; aborting\n",
	     code, AFSDIR_SERVER_BOZCONF_FILEPATH);
	exit(code);
    }

    /* opened the cell databse */
    bozo_confdir = tdir;

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(bozo_rxstat_userok);

    /* have bcrypt key now */

    afsconf_SetNoAuthFlag(tdir, noAuth);

    bozo_rxsc[0] = rxnull_NewServerSecurityObject();
    bozo_rxsc[1] = (struct rx_securityClass *)0;
    bozo_rxsc[2] =
	rxkad_NewServerSecurityObject(0, tdir, afsconf_GetKey, NULL);

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }

    if (rxBind) {
	afs_int32 ccode;
#ifndef AFS_NT40_ENV
        if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || 
            AFSDIR_SERVER_NETINFO_FILEPATH) {
            char reason[1024];
            ccode = parseNetFiles(SHostAddrs, NULL, NULL,
                                           ADDRSPERSITE, reason,
                                           AFSDIR_SERVER_NETINFO_FILEPATH,
                                           AFSDIR_SERVER_NETRESTRICT_FILEPATH);
        } else 
#endif	
	{
            ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
        }
        if (ccode == 1) 
            host = SHostAddrs[0];
    }

    tservice = rx_NewServiceHost(host,  /* port */ 0, /* service id */ 1,
			     /*service name */ "bozo",
			     /* security classes */
			     bozo_rxsc,
			     /* numb sec classes */ 3, BOZO_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_SetStackSize(tservice, BOZO_LWP_STACKSIZE);	/* so gethostbyname works (in cell stuff) */

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats", bozo_rxsc,
			  3, RXSTATS_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_StartServer(1);		/* donate this process */
}

void
bozo_Log(char *a, char *b, char *c, char *d, char *e, char *f)
{
    char tdate[27];
    time_t myTime;

    if (DoSyslog) {
#ifndef AFS_NT40_ENV
	syslog(LOG_INFO, a, b, c, d, e, f);
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
	    printf(a, b, c, d, e, f);
	    fflush(stdout);
	} else {
	    fprintf(bozo_logFile, "%s ", tdate);
	    fprintf(bozo_logFile, a, b, c, d, e, f);

	    /* close so rm BosLog works */
	    fclose(bozo_logFile);
	}
    }
}
