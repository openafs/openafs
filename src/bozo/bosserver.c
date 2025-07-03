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
#include <afs/stds.h>

#include <afs/procmgmt.h>
#include <roken.h>
#include <ctype.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# ifdef __clang__
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# else
#  pragma GCC diagnostic warning "-Wdeprecated-declarations"
# endif
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef AFS_NT40_ENV
#define PATH_DELIM '\\'
#include <direct.h>
#include <WINNT/afsevent.h>
#endif /* AFS_NT40_ENV */

#define PATH_DELIM '/'
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_globals.h>
#include <rx/rxkad.h>
#include <rx/rxstat.h>
#include <afs/keys.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/audit.h>
#include <afs/authcon.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>

#if defined(AFS_SGI_ENV)
#include <afs/afs_args.h>
#endif

#include "bosint.h"
#include "bnode.h"
#include "bnode_internal.h"
#include "bosprototypes.h"

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

char *DoCore;
int DoLogging = 0;
int DoSyslog = 0;
char *DoPidFiles = NULL;
#ifndef AFS_NT40_ENV
int DoSyslogFacility = LOG_DAEMON;
#endif
int DoTransarcLogs = 0;
static afs_int32 nextRestart;
static afs_int32 nextDay;

struct ktime bozo_nextRestartKT, bozo_nextDayKT;
int bozo_newKTs;
int rxBind = 0;
int rxkadDisableDotCheck = 0;

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

/**
 * Return true if this name is a member of the local realm.
 */
int
bozo_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	bozo_Log("Failed local realm check; code=%d, name=%s, inst=%s, cell=%s\n",
		 code, name, inst, cell);
    }
    return islocal;
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

/*!
 * Make directory with parents.
 *
 * \param[in] adir      directory path to create
 * \param[in] areqPerm  permissions to set on the last component of adir
 * \return              0 on success
 */
static int
MakeDirParents(const char *adir, int areqPerm)
{
    struct stat stats;
    int error = 0;
    char *tdir;
    char *p;
    int parent_perm = 0777;	/* use umask for parent perms */
    size_t len;

    tdir = strdup(adir);
    if (!tdir) {
	return ENOMEM;
    }

    /* strip trailing slashes */
    len = strlen(tdir);
    if (!len) {
	return 0;
    }
    p = tdir + len - 1;
    while (p != tdir && *p == PATH_DELIM) {
	*p-- = '\0';
    }

    p = tdir;
#ifdef AFS_NT40_ENV
    /* skip drive letter */
    if (isalpha(p[0]) && p[1] == ':') {
        p += 2;
    }
#endif
    /* skip leading slashes */
    while (*p == PATH_DELIM) {
	p++;
    }

    /* create parent directories with default perms */
    p = strchr(p, PATH_DELIM);
    while (p) {
	*p = '\0';
	if (stat(tdir, &stats) != 0 || !S_ISDIR(stats.st_mode)) {
	    if (mkdir(tdir, parent_perm) != 0) {
		error = errno;
		goto done;
	    }
	}
	*p++ = PATH_DELIM;

	/* skip back to back slashes */
	while (*p == PATH_DELIM) {
	    p++;
	}
	p = strchr(p, PATH_DELIM);
    }

    /* set required perms on the last path component */
    if (stat(tdir, &stats) != 0 || !S_ISDIR(stats.st_mode)) {
	if (mkdir(tdir, areqPerm) != 0) {
	    error = errno;
	}
    }

  done:
    free(tdir);
    return error;
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
	code = MakeDirParents(adir, reqPerm);
	return code;
    }
    return 0;
}

/* create all the bozo dirs */
static int
CreateDirs(const char *coredir)
{
    if (!strncmp
	 (AFSDIR_USR_DIRPATH, AFSDIR_SERVER_BIN_DIRPATH,
	  strlen(AFSDIR_USR_DIRPATH))) {
	if (MakeDir(AFSDIR_USR_DIRPATH))
	    return errno;
    }
    if (!strncmp
	(AFSDIR_SERVER_AFS_DIRPATH, AFSDIR_SERVER_BIN_DIRPATH,
	 strlen(AFSDIR_SERVER_AFS_DIRPATH))) {
	if (MakeDir(AFSDIR_SERVER_AFS_DIRPATH))
	    return errno;
    }
    if (MakeDir(AFSDIR_SERVER_BIN_DIRPATH))
	return errno;
    if (MakeDir(AFSDIR_SERVER_ETC_DIRPATH))
	return errno;
    if (MakeDir(AFSDIR_SERVER_LOCAL_DIRPATH))
	return errno;
    if (MakeDir(AFSDIR_SERVER_DB_DIRPATH))
	return errno;
    if (MakeDir(AFSDIR_SERVER_LOGS_DIRPATH))
	return errno;
    if (coredir) {
	if (MakeDir(coredir))
	    return errno;
    }
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
	    code = rk_rename(AFSDIR_SERVER_BOZINIT_FILEPATH,
			     AFSDIR_SERVER_BOZCONF_FILEPATH);
	    if (code < 0)
		perror("bosconfig rename");
	}
	if (access(AFSDIR_SERVER_BOZCONFNEW_FILEPATH, 0) == 0) {
	    code = rk_rename(AFSDIR_SERVER_BOZCONFNEW_FILEPATH,
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
    if (!aname)
	aname = (char *)bozo_fileName;
    tfile = fopen(aname, "r");
    if (!tfile)
	return 0;		/* -1 */
    instp = malloc(BOZO_BSSIZE);
    typep = malloc(BOZO_BSSIZE);
    notp = malloc(BOZO_BSSIZE);
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
		parms[i] = malloc(BOZO_BSSIZE);
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
    char *tbuffer = NULL;
    afs_int32 code;
    struct bztemp btemp;
    int ret = 0;

    if (!aname)
	aname = (char *)bozo_fileName;
    if (asprintf(&tbuffer, "%s.NBZ", aname) < 0)
	return -1;

    tfile = fopen(tbuffer, "w");
    if (!tfile) {
	ret = -1;
	goto out;
    }
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
	ret = code;
	goto out;
    }
    /* close the file, check for errors and snap new file into place */
    if (fclose(tfile) == EOF) {
	unlink(tbuffer);
	ret = -1;
	goto out;
    }
    code = rk_rename(tbuffer, aname);
    if (code) {
	unlink(tbuffer);
	ret = -1;
	goto out;
    }
    ret = 0;
out:
    free(tbuffer);
    return ret;
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
    AFS_UNREACHED(return(NULL));
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
    int r;

    if (aname && *aname) {
	r = asprintf(&buffer, "%s/%s.%s.pid", DoPidFiles, ainst, aname);
	if (r < 0 || buffer == NULL)
	    bozo_Log("Failed to alloc pid filename buffer for %s.%s.\n",
		     ainst, aname);
    } else {
	r = asprintf(&buffer, "%s/%s.pid", DoPidFiles, ainst);
	if (r < 0 || buffer == NULL)
	    bozo_Log("Failed to alloc pid filename buffer for %s.\n", ainst);
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

/**
 * Get an interface address in network byte order, modulo the
 * NetInfo/NetRestrict configuration files. Return the INADDR_ANY if no
 * interface address is found.
 */
static afs_uint32
GetRxBindAddress(void)
{
    afs_uint32 addr;
    afs_int32 ccode; /* number of addresses found */

    if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || AFSDIR_SERVER_NETINFO_FILEPATH) {
	char reason[1024];
	ccode = afsconf_ParseNetFiles(&addr, NULL, NULL, 1, reason,
				      AFSDIR_SERVER_NETINFO_FILEPATH,
				      AFSDIR_SERVER_NETRESTRICT_FILEPATH);
    } else {
	/* Get the first non-loopback address from the kernel. */
	ccode = rx_getAllAddr(&addr, 1);
    }

    if (ccode != 1) {
	addr = htonl(INADDR_ANY);
    }
    return addr;
}

/**
 * Try to create local cell config file.
 */
static struct afsconf_dir *
CreateLocalCellConfig(void)
{
    int code;
    struct afsconf_dir *tdir = NULL;
    struct afsconf_cell tcell;

    memset(&tcell, 0, sizeof(tcell));
    strcpy(tcell.name, "localcell");  /* assume name is big enough for the default value */
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
    code = afsconf_SetCellInfo(NULL, AFSDIR_SERVER_ETC_DIRPATH, &tcell);
    if (code) {
	bozo_Log
	    ("could not create cell database in '%s' (code %d), quitting\n",
	     AFSDIR_SERVER_ETC_DIRPATH, code);
	exit(1);
    }
    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	bozo_Log("failed to open newly-created cell database, quitting\n");
	exit(1);
    }
    return tdir;
}

/* start a process and monitor it */

#include "AFS_component_version_number.c"

enum optionsList {
    OPT_noauth,
    OPT_log,
    OPT_restricted,
    OPT_pidfiles,
    OPT_auditinterface,
    OPT_auditlog,
    OPT_transarc_logs,
    OPT_peer_stats,
    OPT_process_stats,
    OPT_rxbind,
    OPT_rxmaxmtu,
    OPT_dotted,
#ifndef AFS_NT40_ENV
    OPT_nofork,
    OPT_cores,
    OPT_syslog
#endif /* AFS_NT40_ENV */
};

int
main(int argc, char **argv, char **envp)
{
    struct cmd_syndesc *opts;

    struct rx_service *tservice;
    afs_int32 code;
    struct afsconf_dir *tdir;
    int noAuth = 0;
    int i;
    char *oldlog;
    int rxMaxMTU = -1;
    afs_uint32 host = htonl(INADDR_ANY);
    char *auditIface = NULL;
    char *auditFileName = NULL;
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
    int DoPeerRPCStats = 0;
    int DoProcessRPCStats = 0;
    struct stat sb;
    struct afsconf_bsso_info bsso;
#ifndef AFS_NT40_ENV
    int nofork = 0;
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

    memset(&bsso, 0, sizeof(bsso));

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
    DoCore = strdup(AFSDIR_SERVER_LOGS_DIRPATH);
    if (!DoCore) {
	fprintf(stderr, "bosserver: Failed to allocate memory.\n");
	exit(1);
    }

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
    for (i = 1; i < argc; i++) {
	bozo_argv[i] = argv[i];
    }
#endif	/* AFS_NT40_ENV */

    /* parse cmd line */
    opts = cmd_CreateSyntax(NULL, NULL, NULL, 0, NULL);

    /* bosserver specific options */
    cmd_AddParmAtOffset(opts, OPT_noauth, "-noauth", CMD_FLAG,
			CMD_OPTIONAL, "disable authentication");
    cmd_AddParmAtOffset(opts, OPT_log, "-log", CMD_FLAG,
			CMD_OPTIONAL, "enable logging of privileged commands");
    cmd_AddParmAtOffset(opts, OPT_restricted, "-restricted", CMD_FLAG,
			CMD_OPTIONAL, "enable restricted mode");
    cmd_AddParmAtOffset(opts, OPT_pidfiles, "-pidfiles", CMD_SINGLE_OR_FLAG,
			CMD_OPTIONAL, "enable creating pid files");
#ifndef AFS_NT40_ENV
    cmd_AddParmAtOffset(opts, OPT_nofork, "-nofork", CMD_FLAG,
			CMD_OPTIONAL, "run in the foreground");
    cmd_AddParmAtOffset(opts, OPT_cores, "-cores", CMD_SINGLE,
			CMD_OPTIONAL, "none | path for core files");
#endif /* AFS_NT40_ENV */

    /* general server options */
    cmd_AddParmAtOffset(opts, OPT_auditinterface, "-audit-interface", CMD_SINGLE,
			CMD_OPTIONAL, "audit interface (file or sysvmq)");
    cmd_AddParmAtOffset(opts, OPT_auditlog, "-auditlog", CMD_SINGLE,
			CMD_OPTIONAL, "audit log path");
    cmd_AddParmAtOffset(opts, OPT_transarc_logs, "-transarc-logs", CMD_FLAG,
			CMD_OPTIONAL, "enable Transarc style logging");

#ifndef AFS_NT40_ENV
    cmd_AddParmAtOffset(opts, OPT_syslog, "-syslog", CMD_SINGLE_OR_FLAG,
			CMD_OPTIONAL, "log to syslog");
#endif

    /* rx options */
    cmd_AddParmAtOffset(opts, OPT_peer_stats, "-enable_peer_stats", CMD_FLAG,
			CMD_OPTIONAL, "enable RX RPC statistics by peer");
    cmd_AddParmAtOffset(opts, OPT_process_stats, "-enable_process_stats", CMD_FLAG,
			CMD_OPTIONAL, "enable RX RPC statistics");
    cmd_AddParmAtOffset(opts, OPT_rxbind, "-rxbind", CMD_FLAG,
			CMD_OPTIONAL, "bind only to the primary interface");
    cmd_AddParmAtOffset(opts, OPT_rxmaxmtu, "-rxmaxmtu", CMD_SINGLE,
			CMD_OPTIONAL, "maximum MTU for RX");
    /* rxkad options */
    cmd_AddParmAtOffset(opts, OPT_dotted, "-allow-dotted-principals", CMD_FLAG,
			CMD_OPTIONAL, "permit Kerberos 5 principals with dots");

    code = cmd_Parse(argc, argv, &opts);
    if (code == CMD_HELP) {
	exit(0);
    }
    if (code)
	exit(1);

    /* bosserver options */
    cmd_OptionAsFlag(opts, OPT_noauth, &noAuth);
    cmd_OptionAsFlag(opts, OPT_log, &DoLogging);
    cmd_OptionAsFlag(opts, OPT_restricted, &bozo_isrestricted);

    if (cmd_OptionPresent(opts, OPT_pidfiles)) {
	if (cmd_OptionAsString(opts, OPT_pidfiles, &DoPidFiles) != 0) {
	    DoPidFiles = strdup(AFSDIR_LOCAL_DIR);
	    if (!DoPidFiles) {
		fprintf(stderr, "bosserver: Failed to allocate memory\n");
		exit(1);
	    }
	}
    }

#ifndef AFS_NT40_ENV
    cmd_OptionAsFlag(opts, OPT_nofork, &nofork);

    if (cmd_OptionAsString(opts, OPT_cores, &DoCore) == 0) {
	if (strcmp(DoCore, "none") == 0) {
	    free(DoCore);
	    DoCore = NULL;
	}
    }
#endif

    /* general server options */
    cmd_OptionAsString(opts, OPT_auditlog, &auditFileName);

    if (cmd_OptionAsString(opts, OPT_auditinterface, &auditIface) == 0) {
	if (osi_audit_interface(auditIface)) {
	    printf("Invalid audit interface '%s'\n", auditIface);
	    free(auditIface);
	    exit(1);
	}
	free(auditIface);
    }

    cmd_OptionAsFlag(opts, OPT_transarc_logs, &DoTransarcLogs);

#ifndef AFS_NT40_ENV
    if (cmd_OptionPresent(opts, OPT_syslog)) {
	DoSyslog = 1;
	cmd_OptionAsInt(opts, OPT_syslog, &DoSyslogFacility);
    }
#endif

    /* rx options */
    cmd_OptionAsFlag(opts, OPT_peer_stats, &DoPeerRPCStats);
    cmd_OptionAsFlag(opts, OPT_process_stats, &DoProcessRPCStats);
    cmd_OptionAsFlag(opts, OPT_rxbind, &rxBind);
    cmd_OptionAsInt(opts, OPT_rxmaxmtu, &rxMaxMTU);

    /* rxkad options */
    cmd_OptionAsFlag(opts, OPT_dotted, &rxkadDisableDotCheck);

#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	printf("bosserver: must be run as root.\n");
	exit(1);
    }
#endif

    /* create useful dirs */
    i = CreateDirs(DoCore);
    if (i) {
	printf("bosserver: could not set up directories, code %d\n", i);
	exit(1);
    }

    if (!DoSyslog) {
	/* Support logging to named pipes by not renaming. */
	if (DoTransarcLogs
	    && (lstat(AFSDIR_SERVER_BOZLOG_FILEPATH, &sb) == 0)
	    && !(S_ISFIFO(sb.st_mode))) {
	    if (asprintf(&oldlog, "%s.old", AFSDIR_SERVER_BOZLOG_FILEPATH) < 0) {
		printf("bosserver: out of memory\n");
		exit(1);
	    }
	    rk_rename(AFSDIR_SERVER_BOZLOG_FILEPATH, oldlog);
	    free(oldlog);
	}
	bozo_logFile = fopen(AFSDIR_SERVER_BOZLOG_FILEPATH, "a");
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

    /*
     * go into the background and remove our controlling tty, close open
     * file desriptors
     */

#ifndef AFS_NT40_ENV
    if (!nofork) {
	if (daemon(1, 0))
	    printf("bosserver: warning - daemon() returned code %d\n", errno);
    }
#endif /* ! AFS_NT40_ENV */

    /* Write current state of directory permissions to log file */
    DirAccessOK();

    /* chdir to AFS log directory */
    if (DoCore)
	i = chdir(DoCore);
    else
	i = chdir(AFSDIR_SERVER_LOGS_DIRPATH);
    if (i) {
	printf("bosserver: could not change to %s, code %d\n",
	       DoCore ? DoCore : AFSDIR_SERVER_LOGS_DIRPATH, errno);
	exit(1);
    }

    if (auditFileName != NULL)
	osi_audit_file(auditFileName);

    /* try to read the key from the config file */
    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	tdir = CreateLocalCellConfig();
    }
    /* opened the cell databse */
    bozo_confdir = tdir;

    code = bnode_Init();
    if (code) {
	printf("bosserver: could not init bnode package, code %d\n", code);
	exit(1);
    }

    bnode_Register("fs", &fsbnode_ops, 3);
    bnode_Register("dafs", &dafsbnode_ops, 4);
    bnode_Register("simple", &ezbnode_ops, 1);
    bnode_Register("cron", &cronbnode_ops, 2);

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

    /* Read init file, starting up programs. Also starts watcher threads. */
    if ((code = ReadBozoFile(0))) {
	bozo_Log
	    ("bosserver: Something is wrong (%d) with the bos configuration file %s; aborting\n",
	     code, AFSDIR_SERVER_BOZCONF_FILEPATH);
	exit(code);
    }

    if (bozo_isrestricted) {
	bozo_Log("NOTICE: bosserver is running in restricted mode.\n");
    } else {
	bozo_Log("WARNING: bosserver is not running in restricted mode.\n");
	bozo_Log("WARNING: Superusers have unrestricted access to this host via bos.\n");
	bozo_Log("WARNING: Use 'bos setrestricted' or restart with the -restricted option\n");
	bozo_Log("WARNING: to enable restricted mode.\n");
    }

    if (rxBind) {
	host = GetRxBindAddress();
    }
    for (i = 0; i < 10; i++) {
	code = rx_InitHost(host, htons(AFSCONF_NANNYPORT));
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

    /* Set some rx config */
    if (DoPeerRPCStats)
	rx_enablePeerRPCStats();
    if (DoProcessRPCStats)
	rx_enableProcessRPCStats();

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (rxMaxMTU != -1) {
	if (rx_SetMaxMTU(rxMaxMTU) != 0) {
	    bozo_Log("bosserver: rxMaxMTU %d is invalid\n", rxMaxMTU);
	    exit(1);
	}
    }

    code = LWP_CreateProcess(BozoDaemon, BOZO_LWP_STACKSIZE, /* priority */ 1,
			     /* param */ NULL , "bozo-the-clown", &bozo_pid);
    if (code) {
	bozo_Log("Failed to create daemon thread\n");
        exit(1);
    }

    /* initialize audit user check */
    osi_audit_set_user_check(bozo_confdir, bozo_IsLocalRealmMatch);

    bozo_CreateRxBindFile(host);	/* for local scripts */

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(bozo_rxstat_userok);

    afsconf_SetNoAuthFlag(tdir, noAuth);

    bsso.dir = tdir;
    bsso.logger = bozo_Log;
    afsconf_BuildServerSecurityObjects_int(&bsso, &securityClasses, &numClasses);

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
	code = rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
					   (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
	if (code) {
	    bozo_Log("Failed to allow dotted principals: code %d\n", code);
	    exit(1);
	}
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
    va_end(ap);
}
