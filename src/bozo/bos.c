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
#include <afs/opr.h>

#include <hcrypto/ui.h>

#include "bnode.h"
#include <afs/afsutil.h>
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <ubik.h>
#include <afs/ktime.h>
#include <afs/kautils.h>
#include <afs/afsint.h>
#include <afs/volser.h>

static int IStatServer(struct cmd_syndesc *as, int int32p);
static int DoStat(char *aname, struct rx_connection *aconn,
		  int aint32p, int firstTime);

#include "bosint.h"
#include "bnode_internal.h"
#include "bosprototypes.h"

/* command offsets for bos salvage command */
#define ADDPARMOFFSET 11

/* dummy routine for the audit work.  It should do nothing since audits */
/* occur at the server level and bos is not a server. */
int osi_audit(void )
{
    return 0;
}

/* keep those lines small */
static char *
em(afs_int32 acode)
{
    if (acode == -1)
	return "communications failure (-1)";
    else if (acode == -3)
	return "communications timeout (-3)";
    else
	return (char *)afs_error_message(acode);
}

/* make ctime easier to use */
static char *
DateOf(time_t atime)
{
    static char *bad_time = "BAD TIME";
    static char tbuffer[30];
    char *tp;
    tp = ctime(&atime);
    if (tp == NULL)
	return bad_time;
    if (strlcpy(tbuffer, tp, sizeof(tbuffer)) >= sizeof(tbuffer))
	return bad_time;
    tp = strchr(tbuffer, '\n');
    if (tp != NULL)
	*tp = '\0';    /* Trim new line. */
    return tbuffer;
}


/* use the syntax descr to get a connection, authenticated appropriately.
 * aencrypt is set if we want to encrypt the data on the wire.
 */
static struct rx_connection *
GetConn(struct cmd_syndesc *as, int aencrypt)
{
    struct hostent *th;
    char *hostname;
    char *cellname = NULL;
    const char *confdir;
    const char *retry_confdir;
    afs_int32 code;
    struct rx_connection *tconn;
    afs_int32 addr;
    struct afsconf_dir *tdir = NULL;
    afsconf_secflags secFlags;
    struct rx_securityClass *sc;
    afs_int32 scIndex;

    hostname = as->parms[0].items->data;
    th = hostutil_GetHostByName(hostname);
    if (!th) {
	fprintf(stderr, "bos: can't find address for host '%s'\n", hostname);
	exit(1);
    }
    memcpy(&addr, th->h_addr, sizeof(afs_int32));

    if (aencrypt)
	secFlags = AFSCONF_SECOPTS_ALWAYSENCRYPT;
    else
	secFlags = AFSCONF_SECOPTS_FALLBACK_NULL;


    if (as->parms[ADDPARMOFFSET + 2].items) { /* -localauth */
	secFlags |= AFSCONF_SECOPTS_LOCALAUTH;
	confdir = AFSDIR_SERVER_ETC_DIRPATH;
	retry_confdir = NULL;
    } else {
	confdir = AFSDIR_CLIENT_ETC_DIRPATH;
	retry_confdir = AFSDIR_SERVER_ETC_DIRPATH;
    }

    if (as->parms[ADDPARMOFFSET + 1].items) { /* -noauth */
	/* If we're running with -noauth, we don't need a configuration
	 * directory. */
	secFlags |= AFSCONF_SECOPTS_NOAUTH;
    } else {
	tdir = afsconf_Open(confdir);
	if (tdir == NULL && retry_confdir != NULL) {
	    fprintf(stderr, "bos: Retrying initialization with directory %s\n",
		    retry_confdir);
	    tdir = afsconf_Open(retry_confdir);
	}
	if (tdir == NULL) {
	    fprintf(stderr, "bos: can't open cell database (%s)\n", confdir);
	    exit(1);
	}
    }

    if (as->parms[ADDPARMOFFSET].items) /* -cell */
        cellname = as->parms[ADDPARMOFFSET].items->data;

    code = afsconf_PickClientSecObj(tdir, secFlags, NULL, cellname,
				    &sc, &scIndex, NULL);
    if (code) {
	afs_com_err("bos", code, "(configuring connection security)");
	exit(1);
    }

    if (scIndex == RX_SECIDX_NULL && !(secFlags & AFSCONF_SECOPTS_NOAUTH))
	fprintf(stderr, "bos: running unauthenticated\n");

    tconn =
	rx_NewConnection(addr, htons(AFSCONF_NANNYPORT), 1, sc, scIndex);
    if (!tconn) {
	fprintf(stderr, "bos: could not create rx connection\n");
	exit(1);
    }
    rxs_Release(sc);

    return tconn;
}

static int
SetAuth(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct rx_connection *tconn;
    afs_int32 flag;
    char *tp;

    tconn = GetConn(as, 1);
    tp = as->parms[1].items->data;
    if (strcmp(tp, "on") == 0)
	flag = 0;		/* auth req.: noauthflag is false */
    else if (strcmp(tp, "off") == 0)
	flag = 1;
    else {
	fprintf
	    (stderr, "bos: illegal authentication specifier '%s', must be 'off' or 'on'.\n",
	     tp);
	return 1;
    }
    code = BOZO_SetNoAuthFlag(tconn, flag);
    if (code)
	afs_com_err("bos", code, "(failed to set authentication flag)");
    return 0;
}

/**
 * Construct a destination path.
 *
 * Take a name (e.g., foo/bar) and a directory (e.g., /usr/afs/bin), and
 * construct a destination path (e.g., /usr/afs/bin/bar).
 *
 * @param[in]  anam   filename
 * @param[in]  adir   directory path
 * @param[out] apath  constructed filepath output. The caller is resposible
 *                    for freeing 'apath'.
 */
static int
ComputeDestDir(const char *aname, const char *adir, char **apath)
{
    char *tp;

    tp = strrchr(aname, '/');
    if (tp == NULL) {
	/* no '/' in name */
	if (asprintf(apath, "%s/%s", adir, aname) < 0)
	    return ENOMEM;
    } else {
	/* tp points at the / character */
	if (asprintf(apath, "%s/%s", adir, tp + 1) < 0)
	    return ENOMEM;
    }
    return 0;
}

/* copy data from fd afd to rx call acall */
static int
CopyBytes(int afd, struct rx_call *acall)
{
    afs_int32 code;
    afs_int32 len;
    char tbuffer[256];

    while (1) {
	len = read(afd, tbuffer, sizeof(tbuffer));
	if (len < 0)
	    return errno;
	if (len == 0)
	    return 0;		/* all done */
	code = rx_Write(acall, tbuffer, len);
	if (code != len)
	    return -1;
    }
}

static int
Prune(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct rx_connection *tconn;
    afs_int32 flags;

    tconn = GetConn(as, 1);
    flags = 0;
    if (as->parms[1].items)
	flags |= BOZO_PRUNEBAK;
    if (as->parms[2].items)
	flags |= BOZO_PRUNEOLD;
    if (as->parms[3].items)
	flags |= BOZO_PRUNECORE;
    if (as->parms[4].items)
	flags |= 0xff;
    code = BOZO_Prune(tconn, flags);
    if (code)
	afs_com_err("bos", code, "(failed to prune server files)");
    return code;
}

static int
Exec(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;

    tconn = GetConn(as, 1);
    code = BOZO_Exec(tconn, as->parms[1].items->data);
    if (code)
	fprintf(stderr, "bos: failed to execute command (%s)\n", em(code));
    return code;
}

static int
GetDate(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    const char *destDir;
    afs_int32 time, bakTime, oldTime;
    struct rx_connection *tconn;
    struct cmd_item *ti;

    tconn = GetConn(as, 0);
    if (!as->parms[1].items) {
	fprintf(stderr, "bos: no files to check\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items)
	destDir = as->parms[2].items->data;
    else
	destDir = AFSDIR_CANONICAL_SERVER_BIN_DIRPATH;

    for (ti = as->parms[1].items; ti; ti = ti->next) {
	char *path = NULL;

	/* Check date for each file. */
	code = ComputeDestDir(ti->data, destDir, &path);
	if (code != 0) {
	    fprintf(stderr, "bos: failed to format destination path for file %s (%s)\n",
		    ti->data, em(code));
	    return 1;
	}
	code = BOZO_GetDates(tconn, path, &time, &bakTime, &oldTime);
	if (code) {
	    fprintf(stderr, "bos: failed to check date on %s (%s)\n", ti->data,
		   em(code));
	    free(path);
	    return 1;
	} else {
	    printf("File %s ", path);
	    if (time == 0)
		printf("does not exist, ");
	    else
		printf("dated %s, ", DateOf(time));
	    if (bakTime == 0)
		printf("no .BAK file, ");
	    else
		printf(".BAK file dated %s, ", DateOf(bakTime));
	    if (oldTime == 0)
		printf("no .OLD file.");
	    else
		printf(".OLD file dated %s.", DateOf(oldTime));
	    printf("\n");
	}
	free(path);
    }
    return 0;
}

static int
UnInstall(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    const char *destDir;
    struct cmd_item *ti;
    struct rx_connection *tconn;

    tconn = GetConn(as, 1);
    if (!as->parms[1].items) {
	fprintf(stderr, "bos: no files to uninstall\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items)
	destDir = as->parms[2].items->data;
    else
	destDir = AFSDIR_CANONICAL_SERVER_BIN_DIRPATH;

    for (ti = as->parms[1].items; ti; ti = ti->next) {
	char *path = NULL;

	/* Uninstall each file. */
	code = ComputeDestDir(ti->data, destDir, &path);
	if (code) {
	    fprintf(stderr, "bos: failed to format destination path for file %s (%s)\n",
		    ti->data, em(code));
	    return 1;
	}
	code = BOZO_UnInstall(tconn, path);
	if (code) {
	    fprintf(stderr, "bos: failed to uninstall %s (%s)\n", ti->data, em(code));
	    free(path);
	    return 1;
	} else {
	    printf("bos: uninstalled file %s\n", ti->data);
	}
	free(path);
    }
    return 0;
}

static afs_int32
GetServerGoal(struct rx_connection *aconn, char *aname)
{
    char *itype = NULL;
    afs_int32 code;
    struct bozo_status istatus;

    code = BOZO_GetInstanceInfo(aconn, aname, &itype, &istatus);
    xdr_free((xdrproc_t)xdr_string, &itype);
    if (code) {
	fprintf(stderr, "bos: failed to get instance info for '%s' (%s)\n", aname,
	       em(code));
	/* if we can't get the answer, assume its running */
	return BSTAT_NORMAL;
    }
    if (istatus.goal == 0)
	return BSTAT_SHUTDOWN;
    else
	return BSTAT_NORMAL;
}

static int
Install(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;
    struct stat tstat;
    int fd;
    struct rx_call *tcall;
    const char *destDir;

    tconn = GetConn(as, 1);
    if (!as->parms[1].items) {
	fprintf(stderr, "bos: no files to install\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items)
	destDir = as->parms[2].items->data;
    else
	destDir = AFSDIR_CANONICAL_SERVER_BIN_DIRPATH;

    for (ti = as->parms[1].items; ti; ti = ti->next) {
	char *path = NULL;

	/* Install each file. */
	fd = open(ti->data, O_RDONLY);
	if (fd < 0) {
	    /* better to quit on error than continue for install command */
	    fprintf(stderr, "bos: can't find file '%s', quitting\n", ti->data);
	    return 1;
	}
	code = fstat(fd, &tstat);
	if (code) {
	    fprintf(stderr, "bos: failed to stat file %s, errno is %d\n", ti->data,
		   errno);
	    close(fd);
	    return 1;
	}
	/* compute destination dir */
	code = ComputeDestDir(ti->data, destDir, &path);
	if (code != 0) {
	    fprintf(stderr, "bos: failed to format destination path for file %s (%s)\n",
		    ti->data, em(code));
	    close(fd);
	    return 1;
	}
	tcall = rx_NewCall(tconn);
	code =
	    StartBOZO_Install(tcall, path, tstat.st_size,
			      (afs_int32) tstat.st_mode, tstat.st_mtime);
	if (code == 0) {
	    code = CopyBytes(fd, tcall);
	}
	code = rx_EndCall(tcall, code);
	close(fd);
	free(path);
	if (code) {
	    fprintf(stderr, "bos: failed to install %s (%s)\n", ti->data, em(code));
	    return 1;
	} else
	    printf("bos: installed file %s\n", ti->data);
    }
    return 0;
}

static int
Shutdown(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    tconn = GetConn(as, 1);
    if (as->parms[1].items == 0) {
	code = BOZO_ShutdownAll(tconn);
	if (code)
	    fprintf(stderr, "bos: failed to shutdown servers (%s)\n", em(code));
    } else {
	for (ti = as->parms[1].items; ti; ti = ti->next) {
	    code = BOZO_SetTStatus(tconn, ti->data, BSTAT_SHUTDOWN);
	    if (code)
		fprintf(stderr, "bos: failed to shutdown instance %s (%s)\n", ti->data,
		       em(code));
	}
    }
    if (as->parms[8].items) {
	code = BOZO_WaitAll(tconn);
	if (code)
	    fprintf(stderr, "bos: can't wait for processes to shutdown (%s)\n",
		   em(code));
    }
    return 0;
}

static int
GetRestartCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ktime generalTime, newBinaryTime;
    char messageBuffer[256];
    struct rx_connection *tconn;
    char *hostp;

    hostp = as->parms[0].items->data;	/* host name for messages */
    tconn = GetConn(as, 0);

    code = BOZO_GetRestartTime(tconn, 1, (struct bozo_netKTime *) &generalTime);
    if (code) {
	fprintf(stderr, "bos: failed to retrieve restart information (%s)\n",
	       em(code));
	return code;
    }
    code = BOZO_GetRestartTime(tconn, 2, (struct bozo_netKTime *) &newBinaryTime);
    if (code) {
	fprintf(stderr, "bos: failed to retrieve restart information (%s)\n",
	       em(code));
	return code;
    }

    code = ktime_DisplayString(&generalTime, messageBuffer);
    if (code) {
	fprintf(stderr, "bos: failed to decode restart time (%s)\n", em(code));
	return code;
    }
    printf("Server %s restarts %s\n", hostp, messageBuffer);

    code = ktime_DisplayString(&newBinaryTime, messageBuffer);
    if (code) {
	fprintf(stderr, "bos: failed to decode restart time (%s)\n", em(code));
	return code;
    }
    printf("Server %s restarts for new binaries %s\n", hostp, messageBuffer);

    /* all done now */
    return 0;
}

static int
SetRestartCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 count = 0;
    afs_int32 code;
    struct ktime restartTime;
    afs_int32 type = 0 ;
    struct rx_connection *tconn;

    count = 0;
    tconn = GetConn(as, 1);
    if (as->parms[2].items) {
	count++;
	type = 1;
    }
    if (as->parms[3].items) {
	count++;
	type = 2;
    }
    if (count > 1) {
	fprintf(stderr, "bos: can't specify more than one restart time at a time\n");
	return -1;
    }
    if (count == 0)
	type = 1;		/* by default set general restart time */

    if ((code = ktime_ParsePeriodic(as->parms[1].items->data, &restartTime))) {
	fprintf(stderr, "bos: failed to parse '%s' as periodic restart time(%s)\n",
	       as->parms[1].items->data, em(code));
	return code;
    }

    code = BOZO_SetRestartTime(tconn, type, (struct bozo_netKTime *) &restartTime);
    if (code) {
	fprintf(stderr, "bos: failed to set restart time at server (%s)\n", em(code));
	return code;
    }
    return 0;
}

static int
Startup(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    tconn = GetConn(as, 1);
    if (as->parms[1].items == 0) {
	code = BOZO_StartupAll(tconn);
	if (code)
	    fprintf(stderr, "bos: failed to startup servers (%s)\n", em(code));
    } else {
	for (ti = as->parms[1].items; ti; ti = ti->next) {
	    code = BOZO_SetTStatus(tconn, ti->data, BSTAT_NORMAL);
	    if (code)
		fprintf(stderr, "bos: failed to start instance %s (%s)\n", ti->data,
		       em(code));
	}
    }
    return 0;
}

static int
Restart(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    tconn = GetConn(as, 1);
    if (as->parms[2].items) {
	/* this is really a rebozo command */
	if (as->parms[1].items) {
	    /* specified specific things to restart, can't do this at the same
	     * time */
	    fprintf
		(stderr, "bos: can't specify both '-bos' and specific servers to restart.\n");
	    return 1;
	}
	/* otherwise do a rebozo */
	code = BOZO_ReBozo(tconn);
	if (code)
	    fprintf(stderr, "bos: failed to restart bosserver (%s)\n", em(code));
	return code;
    }
    if (as->parms[1].items == 0) {
	if (as->parms[3].items) {	/* '-all' */
	    code = BOZO_RestartAll(tconn);
	    if (code)
		fprintf(stderr, "bos: failed to restart servers (%s)\n", em(code));
	} else
	    fprintf(stderr, "bos: To restart all processes please specify '-all'\n");
    } else {
	if (as->parms[3].items) {
	    fprintf(stderr, "bos: Can't use '-all' along with individual instances\n");
	} else {
	    for (ti = as->parms[1].items; ti; ti = ti->next) {
		code = BOZO_Restart(tconn, ti->data);
		if (code)
		    fprintf(stderr, "bos: failed to restart instance %s (%s)\n",
			   ti->data, em(code));
	    }
	}
    }
    return 0;
}

static int
SetCellName(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;

    tconn = GetConn(as, 1);
    code = BOZO_SetCellName(tconn, as->parms[1].items->data);
    if (code)
	fprintf(stderr, "bos: failed to set cell (%s)\n", em(code));
    return 0;
}

static int
AddHost(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;
    char *name;

    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	if (as->parms[2].items) {
	    code = asprintf(&name, "[%s]", ti->data);
	    if (code < 0) {
		code = ENOMEM;
	    } else {
		code = BOZO_AddCellHost(tconn, name);
		free(name);
	    }
	} else
	    code = BOZO_AddCellHost(tconn, ti->data);
	if (code)
	    fprintf(stderr, "bos: failed to add host %s (%s)\n", ti->data, em(code));
    }
    return 0;
}

static int
RemoveHost(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_DeleteCellHost(tconn, ti->data);
	if (code)
	    fprintf(stderr, "bos: failed to delete host %s (%s)\n", ti->data,
		   em(code));
    }
    return 0;
}

static int
ListHosts(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    char *cellname = NULL;
    char *hostname = NULL;
    afs_int32 i;

    tconn = GetConn(as, 0);
    code = BOZO_GetCellName(tconn, &cellname);
    if (code) {
	fprintf(stderr, "bos: failed to get cell name (%s)\n", em(code));
	exit(1);
    }
    printf("Cell name is %s\n", cellname);
    for (i = 0;; i++) {
	xdr_free((xdrproc_t)xdr_string, &hostname);
	code = BOZO_GetCellHost(tconn, i, &hostname);
	if (code == BZDOM)
	    break;
	if (code != 0) {
	    fprintf(stderr, "bos: failed to get cell host %d (%s)\n", i, em(code));
	    exit(1);
	}
	printf("    Host %d is %s\n", i + 1, hostname);
    }

    xdr_free((xdrproc_t)xdr_string, &cellname);
    xdr_free((xdrproc_t)xdr_string, &hostname);
    return 0;
}

static int
AddKey(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct ktc_encryptionKey tkey;
    afs_int32 temp;
    char buf[BUFSIZ], ver[BUFSIZ];

    tconn = GetConn(as, 1);
    memset(&tkey, 0, sizeof(struct ktc_encryptionKey));

    if (as->parms[1].items) {
	if (strlcpy(buf, as->parms[1].items->data, sizeof(buf)) >= sizeof(buf)) {
	    fprintf(stderr, "Key data too long for buffer\n");
	    exit(1);
	}
    } else {
	/* prompt for key */
	code = UI_UTIL_read_pw_string(buf, sizeof(buf), "input key: ", 0);
	if (code || strlen(buf) == 0) {
	    fprintf(stderr, "Bad key: \n");
	    exit(1);
	}
	code = UI_UTIL_read_pw_string(ver, sizeof(ver), "Retype input key: ", 0);
	if (code || strlen(ver) == 0) {
	    fprintf(stderr, "Bad key: \n");
	    exit(1);
	}
	if (strcmp(ver, buf) != 0) {
	    fprintf(stderr, "\nInput key mismatch\n");
	    exit(1);
	}

    }

    temp = atoi(as->parms[2].items->data);
    if (temp == 999) {
	/* bcrypt key */
	if (strlen(buf) > sizeof(tkey)) {
	    fprintf(stderr, "Key data too long for bcrypt key.\n");
	    exit(1);
	}
	strncpy((char *)&tkey, buf, sizeof(tkey));
    } else {			/* kerberos key */
	char *tcell;
	if (as->parms[ADDPARMOFFSET].items) {
	    tcell = strdup(as->parms[ADDPARMOFFSET].items->data);
	    if (tcell == NULL) {
		fprintf(stderr, "bos: Unable to allocate memory for cellname\n");
		exit(1);
	    }

	    /* string to key needs upper-case cell names */

	    /* I don't believe this is true.  The string to key function
	     * actually expands the cell name, then LOWER-CASES it.  Perhaps it
	     * didn't use to??? */
	    ucstring(tcell, tcell, strlen(tcell));
	} else
	    tcell = NULL;	/* no cell specified, use current */
/*
	ka_StringToKey(as->parms[1].items->data, tcell, &tkey);
*/
	ka_StringToKey(buf, tcell, &tkey);

	if (tcell)
	    free(tcell);
    }
    code = BOZO_AddKey(tconn, temp, ktc_to_bozoptr(&tkey));
    if (code) {
	fprintf(stderr, "bos: failed to set key %d (%s)\n", temp, em(code));
	exit(1);
    }
    return 0;
}

static int
RemoveKey(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    afs_int32 temp;
    struct cmd_item *ti;

    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	temp = atoi(ti->data);
	code = BOZO_DeleteKey(tconn, temp);
	if (code) {
	    fprintf(stderr, "bos: failed to delete key %d (%s)\n", temp, em(code));
	    exit(1);
	}
    }
    return 0;
}

static int
ListKeys(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct ktc_encryptionKey tkey;
    afs_int32 kvno;
    struct bozo_keyInfo keyInfo;
    int everWorked;
    afs_int32 i;

    tconn = GetConn(as, 1);
    everWorked = 0;
    for (i = 0;; i++) {
	code = BOZO_ListKeys(tconn, i, &kvno, ktc_to_bozoptr(&tkey), &keyInfo);
	if (code)
	    break;
	everWorked = 1;
	/* first check if key is returned */
	if ((!ka_KeyIsZero((char *)&tkey, sizeof(tkey)))
	    && (as->parms[1].items)) {
	    printf("key %d is '", kvno);
	    ka_PrintBytes((char *)&tkey, sizeof(tkey));
	    printf("'\n");
	} else {
	    if (keyInfo.keyCheckSum == 0)	/* shouldn't happen */
		printf("key version is %d\n", kvno);
	    else
		printf("key %d has cksum %u\n", kvno, keyInfo.keyCheckSum);
	}
    }
    if (everWorked) {
	printf("Keys last changed on %s.\n", DateOf(keyInfo.mod_sec));
    }
    if (code != BZDOM)
	fprintf(stderr, "bos: %s error encountered while listing keys\n", em(code));
    else
	printf("All done.\n");
    return 0;
}

static int
AddSUser(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    int failed;
    struct cmd_item *ti;

    failed = 0;
    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_AddSUser(tconn, ti->data);
	if (code) {
	    fprintf(stderr, "bos: failed to add user '%s' (%s)\n", ti->data, em(code));
	    failed = 1;
	}
    }
    return failed;
}

static int
RemoveSUser(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    struct cmd_item *ti;
    afs_int32 code;
    int failed;

    failed = 0;
    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_DeleteSUser(tconn, ti->data);
	if (code) {
	    fprintf(stderr, "bos: failed to delete user '%s', ", ti->data);
	    if (code == ENOENT)
		fprintf(stderr, "(no such user)\n");
	    else
		fprintf(stderr, "(%s)\n", em(code));
	    failed = 1;
	}
    }
    return failed;
}

#define	NPERLINE    10		/* dudes to print per line */
static int
ListSUsers(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    int i;
    afs_int32 code;
    char *name = NULL;
    int lastNL, printGreeting;

    tconn = GetConn(as, 0);
    lastNL = 0;
    printGreeting = 1;
    for (i = 0;; i++) {
	xdr_free((xdrproc_t)xdr_string, &name);
	code = BOZO_ListSUsers(tconn, i, &name);
	if (code)
	    break;
	if (printGreeting) {
	    printGreeting = 0;	/* delay until after first call succeeds */
	    printf("SUsers are: ");
	}
	printf("%s ", name);
	if ((i % NPERLINE) == NPERLINE - 1) {
	    printf("\n");
	    lastNL = 1;
	} else
	    lastNL = 0;
    }
    if (code != 1) {
	/* a real error code, instead of scanned past end */
	fprintf(stderr, "bos: failed to retrieve super-user list (%s)\n", em(code));
	goto done;
    }
    if (lastNL == 0)
	printf("\n");
    code = 0;

  done:
    xdr_free((xdrproc_t)xdr_string, &name);
    return code;
}

static int
StatServer(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    int i;
    char *iname = NULL;
    int int32p;

    /* int32p==1 is obsolete, smaller, printout */
    int32p = (as->parms[2].items != 0 ? 2 : 0);

    /* no parms does something pretty different */
    if (as->parms[1].items)
	return IStatServer(as, int32p);

    tconn = GetConn(as, 0);
    for (i = 0;; i++) {
	/* for each instance */
	xdr_free((xdrproc_t)xdr_string, &iname);
	code = BOZO_EnumerateInstance(tconn, i, &iname);
	if (code == BZDOM)
	    break;
	if (code) {
	    fprintf(stderr, "bos: failed to contact host's bosserver (%s).\n",
		   em(code));
	    break;
	}
	DoStat(iname, tconn, int32p, (i == 0));	/* print status line */
    }

    xdr_free((xdrproc_t)xdr_string, &iname);
    return 0;
}

static int
CreateServer(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    char *parms[6];
    struct cmd_item *ti;
    int i;
    char *type, *name, *notifier = NONOTIFIER;

    tconn = GetConn(as, 1);
    for (i = 0; i < 6; i++)
	parms[i] = "";
    for (i = 0, ti = as->parms[3].items; (ti && i < 6); ti = ti->next, i++) {
	parms[i] = ti->data;
    }
    name = as->parms[1].items->data;
    type = as->parms[2].items->data;
    if ((ti = as->parms[4].items)) {
	notifier = ti->data;
    }
    code =
	BOZO_CreateBnode(tconn, type, name, parms[0], parms[1], parms[2],
			 parms[3], parms[4], notifier);
    if (code) {
	fprintf
	    (stderr, "bos: failed to create new server instance %s of type '%s' (%s)\n",
	     name, type, em(code));
    }
    return code;
}

static int
DeleteServer(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_DeleteBnode(tconn, ti->data);
	if (code) {
	    if (code == BZBUSY)
		fprintf(stderr, "bos: can't delete running instance '%s'\n", ti->data);
	    else
		fprintf(stderr, "bos: failed to delete instance '%s' (%s)\n", ti->data,
		       em(code));
	}
    }
    return code;
}

static int
StartServer(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_SetStatus(tconn, ti->data, BSTAT_NORMAL);
	if (code)
	    fprintf(stderr, "bos: failed to start instance '%s' (%s)\n", 
                    ti->data, em(code));
    }
    return code;
}

static int
StopServer(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code;
    struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 1);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	code = BOZO_SetStatus(tconn, ti->data, BSTAT_SHUTDOWN);
	if (code)
	    fprintf(stderr, "bos: failed to change stop instance '%s' (%s)\n",
		   ti->data, em(code));
    }
    if (as->parms[8].items) {
	code = BOZO_WaitAll(tconn);
	if (code)
	    fprintf(stderr, "bos: can't wait for processes to shutdown (%s)\n",
		   em(code));
    }
    return code;
}

static afs_int32
DoSalvage(struct rx_connection * aconn, char * aparm1, char * aparm2,
	  char * aoutName, afs_int32 showlog, char * parallel,
	  char * atmpDir, char * orphans, int dafs, int dodirs)
{
    afs_int32 code;
    char *parms[6];
    char buffer;
    char tbuffer[BOZO_BSSIZE];
    struct bozo_status istatus;
    struct rx_call *tcall;
    char *tp;
    FILE *outFile;
    int closeIt;
    char partName[20];		/* canonical name for partition */
    afs_int32 partNumber;
    char *notifier = NONOTIFIER;
    int count;

    /* if a partition was specified, canonicalize the name, since
     * the salvager has a stupid partition ID parser */
    if (aparm1) {
	partNumber = volutil_GetPartitionID(aparm1);
	if (partNumber < 0) {
	    fprintf(stderr, "bos: could not parse partition ID '%s'\n", aparm1);
	    return EINVAL;
	}
	tp = volutil_PartitionName(partNumber);
	if (!tp) {
	    fprintf(stderr, "bos: internal error parsing partition ID '%s'\n", aparm1);
	    return EINVAL;
	}
	if (strlcpy(partName, tp, sizeof(partName)) >= sizeof(partName)) {
	    fprintf(stderr, "bos: partName buffer too small for partition ID '%s'\n", aparm1);
	    return EINVAL;
	}
    } else
	partName[0] = 0;

    /* open the file name */
    if (aoutName) {
	outFile = fopen(aoutName, "w");
	if (!outFile) {
	    fprintf(stderr, "bos: can't open specified SalvageLog file '%s'\n",
		   aoutName);
	    return ENOENT;
	}
	closeIt = 1;		/* close this file later */
    } else {
	outFile = stdout;
	closeIt = 0;		/* don't close this file later */
    }

    for (code = 2; code < 6; code++)
	parms[code] = "";
    if (!aparm2)
	aparm2 = "";

    /* MUST pass canonical (wire-format) salvager path to bosserver */
    if (*aparm2 != 0) {
	/* single volume salvage */
	if (dafs) {
	    /* for DAFS, we call the salvagserver binary with special options.
	     * in this mode, it simply uses SALVSYNC to tell the currently
	     * running salvageserver to offline and salvage the volume in question */
	    strncpy(tbuffer, AFSDIR_CANONICAL_SERVER_SALSRV_FILEPATH, BOZO_BSSIZE);

	    if ((strlen(tbuffer) + 9 + strlen(partName) + 1 + strlen(aparm2) +
		 1) > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }

	    strcat(tbuffer, " -client ");
	    strcat(tbuffer, partName);
	    strcat(tbuffer, " ");
	    strcat(tbuffer, aparm2);
	} else {
	    strncpy(tbuffer, AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH, BOZO_BSSIZE);

	    if ((strlen(tbuffer) + 1 + strlen(partName) + 1 + strlen(aparm2) +
		 1) > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }

	    strcat(tbuffer, " ");
	    strcat(tbuffer, partName);
	    strcat(tbuffer, " ");
	    strcat(tbuffer, aparm2);
	}
    } else {
	/* partition salvage */
	strncpy(tbuffer, AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH, BOZO_BSSIZE);
	if ((strlen(tbuffer) + 4 + strlen(partName) + 1) > BOZO_BSSIZE) {
	    fprintf(stderr, "bos: command line too big\n");
	    code = E2BIG;
	    goto done;
	}
	strcat(tbuffer, " -force ");
	strcat(tbuffer, partName);
    }

    /* For DAFS, specifying a single volume does not result in a standard
     * salvager call.  Instead, it simply results in a SALVSYNC call to the
     * online salvager daemon.  This interface does not give us the same rich
     * set of call flags.  Thus, we skip these steps for DAFS single-volume
     * calls */
    if (!dafs || (*aparm2 == 0)) {
	/* add the parallel option if given */
	if (parallel != NULL) {
	    if ((strlen(tbuffer) + 11 + strlen(parallel) + 1) > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }
	    strcat(tbuffer, " -parallel ");
	    strcat(tbuffer, parallel);
	}

	/* add the tmpdir option if given */
	if (atmpDir != NULL) {
	    if ((strlen(tbuffer) + 9 + strlen(atmpDir) + 1) > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }
	    strcat(tbuffer, " -tmpdir ");
	    strcat(tbuffer, atmpDir);
	}

	/* add the orphans option if given */
	if (orphans != NULL) {
	    if ((strlen(tbuffer) + 10 + strlen(orphans) + 1) > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }
	    strcat(tbuffer, " -orphans ");
	    strcat(tbuffer, orphans);
	}
	/* add the salvagedirs option if given */
	if (dodirs) {
	    if (strlen(tbuffer) + 14 > BOZO_BSSIZE) {
		fprintf(stderr, "bos: command line too big\n");
		code = E2BIG;
		goto done;
	    }
	    strcat(tbuffer, " -salvagedirs");
	}
    }

    parms[0] = tbuffer;
    parms[1] = "now";		/* when to do it */
    code =
	BOZO_CreateBnode(aconn, "cron", "salvage-tmp", parms[0], parms[1],
			 parms[2], parms[3], parms[4], notifier);
    if (code) {
	fprintf(stderr, "bos: failed to start 'salvager' (%s)\n", em(code));
	goto done;
    }
    /* now wait for bnode to disappear */
    count = 0;
    while (1) {
	char *itype = NULL;
#ifdef AFS_PTHREAD_ENV
	sleep(1);
#else
	IOMGR_Sleep(1);
#endif
	code = BOZO_GetInstanceInfo(aconn, "salvage-tmp", &itype, &istatus);
	xdr_free((xdrproc_t)xdr_string, &itype);
	if (code)
	    break;
	if ((++count % 5) == 0)
	    printf("bos: waiting for salvage to complete.\n");
    }
    if (code != BZNOENT) {
	fprintf(stderr, "bos: salvage failed (%s)\n", em(code));
	goto done;
    }
    code = 0;

    /* now print the log file to the output file */
    printf("bos: salvage completed\n");
    if (aoutName || showlog) {
	fprintf(outFile, "SalvageLog:\n");
	tcall = rx_NewCall(aconn);
	/* MUST pass canonical (wire-format) salvager log path to bosserver */
	code =
	    StartBOZO_GetLog(tcall, AFSDIR_CANONICAL_SERVER_SLVGLOG_FILEPATH);
	if (code) {
	    code = rx_EndCall(tcall, code);
	    goto done;
	}
	/* copy data */
	while (1) {
	    code = rx_Read(tcall, &buffer, 1);
	    if (code != 1)
		break;
	    putc(buffer, outFile);
	    if (buffer == 0)
		break;		/* the end delimeter */
	}
	code = rx_EndCall(tcall, 0);
	/* fall through into cleanup code */
    }

  done:
    if (closeIt && outFile)
	fclose(outFile);
    return code;
}

static int
GetLogCmd(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    struct rx_call *tcall;
    afs_int32 code;
    char buffer;
    int error;

    printf("Fetching log file '%s'...\n", as->parms[1].items->data);
    tconn = GetConn(as, 1);
    tcall = rx_NewCall(tconn);
    code = StartBOZO_GetLog(tcall, as->parms[1].items->data);
    if (code) {
	code = rx_EndCall(tcall, code);
	goto done;
    }
    /* copy data */
    error = 0;
    while (1) {
	code = rx_Read(tcall, &buffer, 1);
	if (code != 1) {
	    error = EIO;
	    break;
	}
	if (buffer == 0)
	    break;		/* the end delimeter */
	putchar(buffer);
    }
    code = rx_EndCall(tcall, error);
    /* fall through into cleanup code */

  done:
    if (code)
	afs_com_err("bos", code, "(while reading log)");
    return code;
}

static int
IsDAFS(struct rx_connection *aconn)
{
    char *itype = NULL;
    struct bozo_status istatus;
    afs_int32 code;

    code = BOZO_GetInstanceInfo(aconn, "dafs", &itype, &istatus);
    xdr_free((xdrproc_t)xdr_string, &itype);
    if (code) {
	/* no dafs bnode; cannot be dafs */
	return 0;
    }
    if (istatus.goal) {
	/* dafs bnode is running; we must be dafs */
	return 1;
    }

    /* dafs bnode exists but is not running; keep checking */
    code = BOZO_GetInstanceInfo(aconn, "fs", &itype, &istatus);
    xdr_free((xdrproc_t)xdr_string, &itype);
    if (code) {
	/* no fs bnode; must be dafs */
	return 1;
    }
    if (istatus.goal) {
	/* fs bnode is running; we are not dafs at the moment */
	return 0;
    }

    /*
     * At this point, we have both dafs and fs bnodes, but neither is running.
     * Therefore, it's not possible to say if we are DAFS or not DAFS.  Just
     * return 0 in that case; it shouldn't much matter what we return, anyway.
     */
    return 0;
}

static int
SalvageCmd(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code, rc;
    char *outName;
    char *volume_name = NULL;
    afs_int32 newID;
    extern struct ubik_client *cstruct;
    afs_int32 curGoal, showlog = 0, dafs = 0;
    int dodirs = 0;
    char *parallel;
    char *tmpDir;
    char *orphans;
    char * serviceName;

    /* parm 0 is machine name, 1 is partition, 2 is volume, 3 is -all flag */
    tconn = GetConn(as, 1);

    /* find out whether fileserver is running demand attach fs */
    if (IsDAFS(tconn)) {
	dafs = 1;
	serviceName = "dafs";
    } else {
	serviceName = "fs";
    }

    /* we can do a volume, a partition or the whole thing, but not mixtures
     * thereof */
    if (!as->parms[1].items && as->parms[2].items) {
	fprintf(stderr, "bos: must specify partition to salvage individual volume.\n");
	return -1;
    }
    if (as->parms[5].items && as->parms[3].items) {
	printf("bos: can not specify both -file and -showlog.\n");
	return -1;
    }
    if (as->parms[4].items && (as->parms[1].items || as->parms[2].items)) {
	fprintf(stderr, "bos: can not specify -all with other flags.\n");
	return -1;
    }

    /* get the output file name, if any */
    if (as->parms[3].items)
	outName = as->parms[3].items->data;
    else
	outName = NULL;

    if (as->parms[5].items)
	showlog = 1;

    /* parallel option */
    parallel = NULL;
    if (as->parms[6].items)
	parallel = as->parms[6].items->data;

    /* get the tmpdir filename if any */
    tmpDir = NULL;
    if (as->parms[7].items)
	tmpDir = as->parms[7].items->data;

    /* -orphans option */
    orphans = NULL;
    if (as->parms[8].items) {
	orphans = as->parms[8].items->data;
    }

    if (dafs) {
	if (!as->parms[9].items) { /* -forceDAFS flag */
	    fprintf(stderr, "This is a demand attach fileserver.  Are you sure you want to proceed with a manual salvage?\n");
	    fprintf(stderr, "must specify -forceDAFS flag in order to proceed.\n");
	    return EINVAL;
	}
    }

    if (as->parms[10].items) { /* -salvagedirs */
	if (as->parms[4].items) { /* -all */
	    dodirs = 1;
	} else {
	    fprintf(stderr, " -salvagedirs only possible with -all.\n");
	    return EINVAL;
	}
    }

    if (as->parms[4].items) {
	/* salvage whole enchilada */
	curGoal = GetServerGoal(tconn, serviceName);
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: shutting down '%s'.\n", serviceName);
	    code = BOZO_SetTStatus(tconn, serviceName, BSTAT_SHUTDOWN);
	    if (code) {
		fprintf(stderr, "bos: failed to stop '%s' (%s)\n", serviceName, em(code));
		return code;
	    }
	    code = BOZO_WaitAll(tconn);	/* wait for shutdown to complete */
	    if (code)
		fprintf
		    (stderr, "bos: failed to wait for file server shutdown, continuing.\n");
	}
	/* now do the salvage operation */
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, NULL, NULL, outName, showlog, parallel, tmpDir,
		       orphans, dafs, dodirs);
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: restarting %s.\n", serviceName);
	    code = BOZO_SetTStatus(tconn, serviceName, BSTAT_NORMAL);
	    if (code) {
		fprintf(stderr, "bos: failed to restart '%s' (%s)\n", serviceName, em(code));
		return code;
	    }
	}
	if (rc)
	    return rc;
    } else if (!as->parms[2].items) {
	if (!as->parms[1].items) {
	    fprintf
		(stderr, "bos: must specify -all switch to salvage all partitions.\n");
	    return -1;
	}
	if (volutil_GetPartitionID(as->parms[1].items->data) < 0) {
	    /* can't parse volume ID, so complain before shutting down
	     * file server.
	     */
	    fprintf(stderr, "bos: can't interpret %s as partition ID.\n",
		   as->parms[1].items->data);
	    return -1;
	}
	curGoal = GetServerGoal(tconn, serviceName);
	/* salvage a whole partition (specified by parms[1]) */
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: shutting down '%s'.\n", serviceName);
	    code = BOZO_SetTStatus(tconn, serviceName, BSTAT_SHUTDOWN);
	    if (code) {
		fprintf(stderr, "bos: can't stop '%s' (%s)\n", serviceName, em(code));
		return code;
	    }
	    code = BOZO_WaitAll(tconn);	/* wait for shutdown to complete */
	    if (code)
		fprintf
		    (stderr, "bos: failed to wait for file server shutdown, continuing.\n");
	}
	/* now do the salvage operation */
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, as->parms[1].items->data, NULL, outName,
		       showlog, parallel, tmpDir, orphans, dafs, 0);
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: restarting '%s'.\n", serviceName);
	    code = BOZO_SetTStatus(tconn, serviceName, BSTAT_NORMAL);
	    if (code) {
		printf("bos: failed to restart '%s' (%s)\n", serviceName, em(code));
		return code;
	    }
	}
	if (rc)
	    return rc;
    } else {
	/* salvage individual volume (don't shutdown fs first), just use
	 * single-shot cron bnode.  Must leave server running when using this
	 * option, since salvager will ask file server for the volume */
	char *tmpname;
	afs_int32 err;
	const char *confdir;
	int localauth;

	if (as->parms[ADDPARMOFFSET].items)
	    tmpname = as->parms[ADDPARMOFFSET].items->data;
	else
	    tmpname = NULL;

	localauth = (as->parms[ADDPARMOFFSET + 2].items != 0);
	confdir =
	    (localauth ? AFSDIR_SERVER_ETC_DIRPATH :
	     AFSDIR_CLIENT_ETC_DIRPATH);

	code = vsu_ClientInit(confdir, tmpname,
			      AFSCONF_SECOPTS_FALLBACK_NULL |
			      AFSCONF_SECOPTS_NOAUTH,
			      NULL, &cstruct);

	if (code == 0) {
	    newID = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);
	    if (newID == 0) {
		fprintf(stderr, "bos: can't interpret %s as volume name or ID\n",
		       as->parms[2].items->data);
		return -1;
	    }
	    if (asprintf(&volume_name, "%u", newID) < 0) {
		fprintf(stderr, "bos: out of memory\n");
		return -1;
	    }
	} else {
	    fprintf
		(stderr, "bos: can't initialize volume system client (code %d), trying anyway.\n",
		 code);
	    volume_name = strdup(as->parms[2].items->data);
	    if (volume_name == NULL) {
		fprintf(stderr, "bos: out of memory\n");
		return -1;
	    }

	}
	if (volutil_GetPartitionID(as->parms[1].items->data) < 0) {
	    /* can't parse volume ID, so complain before shutting down
	     * file server.
	     */
	    fprintf(stderr, "bos: can't interpret %s as partition ID.\n",
		   as->parms[1].items->data);
	    free(volume_name);
	    return -1;
	}
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, as->parms[1].items->data, volume_name, outName,
		       showlog, parallel, tmpDir, orphans, dafs, 0);
	free(volume_name);
	if (rc)
	    return rc;
    }
    return 0;
}

static int
IStatServer(struct cmd_syndesc *as, int int32p)
{
    struct rx_connection *tconn;
    struct cmd_item *ti;
    int firstTime = 1;

    tconn = GetConn(as, 0);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	DoStat(ti->data, tconn, int32p, firstTime);
	firstTime = 0;
    }
    return 0;
}

static int
DoStat(IN char *aname,
       IN struct rx_connection *aconn,
       IN int aint32p,
       IN int firstTime) 	/* true iff first instance in cmd */
{
    afs_int32 temp;
    afs_int32 code;
    afs_int32 i;
    struct bozo_status istatus;
    char *itype = NULL;
    char *is1 = NULL;
    char *is2 = NULL;
    char *is3 = NULL;
    char *is4 = NULL;
    char *desc = NULL;
    char *parm = NULL;
    char *notifier_parm = NULL;

    code = BOZO_GetInstanceInfo(aconn, aname, &itype, &istatus);
    if (code) {
	fprintf(stderr, "bos: failed to get instance info for '%s' (%s)\n", aname,
	       em(code));
	goto done;
    }
    if (firstTime && aint32p && (istatus.flags & BOZO_BADDIRACCESS))
	printf
	    ("Bosserver reports inappropriate access on server directories\n");
    printf("Instance %s, ", aname);
    if (aint32p)
	printf("(type is %s) ", itype);
    if (istatus.fileGoal == istatus.goal) {
	if (!istatus.goal)
	    printf("disabled, ");
    } else {
	if (istatus.fileGoal)
	    printf("temporarily disabled, ");
	else
	    printf("temporarily enabled, ");
    }

    if (istatus.flags & BOZO_ERRORSTOP)
	printf("stopped for too many errors, ");
    if (istatus.flags & BOZO_HASCORE)
	printf("has core file, ");

    code = BOZO_GetStatus(aconn, aname, &temp, &desc);
    if (code)
	fprintf(stderr, "bos: failed to get status for instance '%s' (%s)\n", aname,
	       em(code));
    else {
	printf("currently ");
	if (temp == BSTAT_NORMAL)
	    printf("running normally.\n");
	else if (temp == BSTAT_SHUTDOWN)
	    printf("shutdown.\n");
	else if (temp == BSTAT_STARTINGUP)
	    printf("starting up.\n");
	else if (temp == BSTAT_SHUTTINGDOWN)
	    printf("shutting down.\n");
	if (desc[0] != '\0') {
	    printf("    Auxiliary status is: %s.\n", desc);
	}
    }

    /* are we done yet? */
    if (!aint32p) {
	code = 0;
	goto done;
    }

    if (istatus.procStartTime)
	printf("    Process last started at %s (%d proc starts)\n",
	       DateOf(istatus.procStartTime), istatus.procStarts);
    if (istatus.lastAnyExit) {
	printf("    Last exit at %s\n", DateOf(istatus.lastAnyExit));
    }
    if (istatus.lastErrorExit) {
	printf("    Last error exit at %s, ", DateOf(istatus.lastErrorExit));
	code = BOZO_GetInstanceStrings(aconn, aname, &is1, &is2, &is3, &is4);
	/* don't complain about failing call, since could simply mean
	 * interface mismatch.
	 */
	if (code == 0) {
	    if (*is1 != 0) {
		/* non-null instance string */
		printf("by %s, ", is1);
	    }
	}
	if (istatus.errorSignal) {
	    if (istatus.errorSignal == SIGTERM)
		printf("due to shutdown request\n");
	    else
		printf("due to signal %d\n", istatus.errorSignal);
	} else
	    printf("by exiting with code %d\n", istatus.errorCode);
    }

    if (aint32p > 1) {
	/* try to display all the parms */
	for (i = 0;; i++) {
	    xdr_free((xdrproc_t)xdr_string, &parm);
	    code = BOZO_GetInstanceParm(aconn, aname, i, &parm);
	    if (code)
		break;
	    printf("    Command %d is '%s'\n", i + 1, parm);
	}
	code = BOZO_GetInstanceParm(aconn, aname, 999, &notifier_parm);
	if (!code) {
	    /* Any type of failure is treated as not having a notifier program */
	    printf("    Notifier  is '%s'\n", notifier_parm);
	}
	printf("\n");
    }
    code = 0;

  done:
    xdr_free((xdrproc_t)xdr_string, &itype);
    xdr_free((xdrproc_t)xdr_string, &is1);
    xdr_free((xdrproc_t)xdr_string, &is2);
    xdr_free((xdrproc_t)xdr_string, &is3);
    xdr_free((xdrproc_t)xdr_string, &is4);
    xdr_free((xdrproc_t)xdr_string, &desc);
    xdr_free((xdrproc_t)xdr_string, &parm);
    xdr_free((xdrproc_t)xdr_string, &notifier_parm);
    return code;
}

static int
GetRestrict(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code, val;

    tconn = GetConn(as, 0);
    code = BOZO_GetRestrictedMode(tconn, &val);
    if (code)
	fprintf(stderr, "bos: failed to get restricted mode (%s)\n", em(code));
    else
	printf("Restricted mode is %s\n", val ? "on" : "off");

    return 0;
}

static int
SetRestrict(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *tconn;
    afs_int32 code, val;

    tconn = GetConn(as, 1);
    util_GetInt32(as->parms[1].items->data, &val);
    code = BOZO_SetRestrictedMode(tconn, val);
    if (code)
	fprintf(stderr, "bos: failed to set restricted mode (%s)\n", em(code));
    return 0;
}

static void
add_std_args(struct cmd_syndesc *ts)
{
    cmd_Seek(ts, ADDPARMOFFSET);
    /* + 0 */ cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    /* + 1 */ cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL,
			  "don't authenticate");
    /* + 2 */ cmd_AddParm(ts, "-localauth", CMD_FLAG, CMD_OPTIONAL,
			  "create tickets from KeyFile");
}

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;
#ifdef AFS_NT40_ENV
    __declspec(dllimport)
#endif
    extern int afsconf_SawCell;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
    sigaction(SIGABRT, &nsa, NULL);
#endif

    /* start up rx */
    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "bos: could not initialize rx (%s)\n", em(code));
	exit(1);
    }

    /* To resolve a AFSCELL environment "Note: ..." problem: Because bos calls ka_Init (why?) before any
     * checkup on the existance of a "-cell" option on the particular bos command we don't print the
     * message if a cell is passed in. Luckily the rest of the programs don't call these adhoc routines
     * and things work fine. Note that if the "-cell" isn't passed then we're still ok since later on
     * the proper routine (afsconf_GetCellInfo) is going to be called to do the right thing.
     */
    afsconf_SawCell = 1;	/* Means don't print warning if AFSCELL is set */
    ka_Init(0);
    afsconf_SawCell = 0;	/* Reset it */
    /* don't check error code, since fails sometimes when we're setting up a
     * system */
    initialize_CMD_error_table();
    initialize_BZ_error_table();

    ts = cmd_CreateSyntax("start", StartServer, NULL, 0, "start running a server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("stop", StopServer, NULL, 0, "halt a server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-wait", CMD_FLAG, CMD_OPTIONAL,
		"wait for process to stop");
    add_std_args(ts);

    ts = cmd_CreateSyntax("status", StatServer, NULL, 0,
			  "show server instance status");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL,
		"server process name");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "long status");
    add_std_args(ts);

    ts = cmd_CreateSyntax("shutdown", Shutdown, NULL, 0, "shutdown all processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-wait", CMD_FLAG, CMD_OPTIONAL,
		"wait for process to stop");
    add_std_args(ts);

    ts = cmd_CreateSyntax("startup", Startup, NULL, 0, "start all processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    add_std_args(ts);

    ts = cmd_CreateSyntax("restart", Restart, NULL, 0, "restart processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    cmd_AddParm(ts, "-bosserver", CMD_FLAG, CMD_OPTIONAL,
		"restart bosserver");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "restart all processes");
    add_std_args(ts);

#ifndef OPBOS

    ts = cmd_CreateSyntax("create", CreateServer, NULL, 0,
			  "create a new server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, 0, "server process name");
    cmd_AddParm(ts, "-type", CMD_SINGLE, 0, "server type");
    cmd_AddParm(ts, "-cmd", CMD_LIST, 0, "command lines");
    cmd_AddParm(ts, "-notifier", CMD_SINGLE, CMD_OPTIONAL,
		"Notifier program");
    add_std_args(ts);

    ts = cmd_CreateSyntax("delete", DeleteServer, NULL, 0,
			  "delete a server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("adduser", AddSUser, NULL, 0,
			  "add users to super-user list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user names");
    add_std_args(ts);

    ts = cmd_CreateSyntax("removeuser", RemoveSUser, NULL, 0,
			  "remove users from super-user list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user names");
    add_std_args(ts);

    ts = cmd_CreateSyntax("listusers", ListSUsers, NULL, 0, "list super-users");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("addkey", AddKey, NULL, 0,
			  "add keys to key dbase (kvno 999 is bcrypt)");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-key", CMD_SINGLE, CMD_OPTIONAL, "key");
    cmd_AddParm(ts, "-kvno", CMD_SINGLE, 0, "key version number");
    add_std_args(ts);

    ts = cmd_CreateSyntax("removekey", RemoveKey, NULL, 0,
			  "remove keys from key dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-kvno", CMD_LIST, 0, "key version number");
    add_std_args(ts);

    ts = cmd_CreateSyntax("listkeys", ListKeys, NULL, 0, "list keys");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-showkey", CMD_FLAG, CMD_OPTIONAL,
		"show the actual key rather than the checksum");
    add_std_args(ts);

    ts = cmd_CreateSyntax("listhosts", ListHosts, NULL, 0, "get cell host list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    add_std_args(ts);
    cmd_CreateAlias(ts, "getcell");

    ts = cmd_CreateSyntax("setcellname", SetCellName, NULL, 0, "set cell name");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "cell name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("addhost", AddHost, NULL, 0, "add host to cell dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-host", CMD_LIST, 0, "host name");
    cmd_AddParm(ts, "-clone", CMD_FLAG, CMD_OPTIONAL, "vote doesn't count");
    add_std_args(ts);

    ts = cmd_CreateSyntax("removehost", RemoveHost, NULL, 0,
			  "remove host from cell dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-host", CMD_LIST, 0, "host name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("setauth", SetAuth, NULL, 0,
			  "set authentication required flag");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-authrequired", CMD_SINGLE, 0,
		"on or off: authentication required for admin requests");
    add_std_args(ts);

    ts = cmd_CreateSyntax("install", Install, NULL, 0, "install program");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to install");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args(ts);

    ts = cmd_CreateSyntax("uninstall", UnInstall, NULL, 0, "uninstall program");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to uninstall");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args(ts);

    ts = cmd_CreateSyntax("getlog", GetLogCmd, NULL, 0, "examine log file");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_SINGLE, 0, "log file to examine");
    add_std_args(ts);

    ts = cmd_CreateSyntax("getdate", GetDate, NULL, 0, "get dates for programs");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to check");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args(ts);

    ts = cmd_CreateSyntax("exec", Exec, NULL, 0, "execute shell command on server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-cmd", CMD_SINGLE, 0, "command to execute");
    add_std_args(ts);

    ts = cmd_CreateSyntax("prune", Prune, NULL, 0, "prune server files");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-bak", CMD_FLAG, CMD_OPTIONAL, "delete .BAK files");
    cmd_AddParm(ts, "-old", CMD_FLAG, CMD_OPTIONAL, "delete .OLD files");
    cmd_AddParm(ts, "-core", CMD_FLAG, CMD_OPTIONAL, "delete core files");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "delete all junk files");
    add_std_args(ts);

    ts = cmd_CreateSyntax("setrestart", SetRestartCmd, NULL, 0,
			  "set restart times");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "machine name");
    cmd_AddParm(ts, "-time", CMD_SINGLE, CMD_REQUIRED,
		"time to restart server");
    cmd_AddParm(ts, "-general", CMD_FLAG, CMD_OPTIONAL,
		"set general restart time");
    cmd_AddParm(ts, "-newbinary", CMD_FLAG, CMD_OPTIONAL,
		"set new binary restart time");
    add_std_args(ts);
    cmd_CreateAlias(ts, "setr");

    ts = cmd_CreateSyntax("getrestart", GetRestartCmd, NULL, 0,
			  "get restart times");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "machine name");
    add_std_args(ts);
    cmd_CreateAlias(ts, "getr");

    ts = cmd_CreateSyntax("salvage", SalvageCmd, NULL, 0,
			  "salvage partition or volumes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"salvage partition");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_OPTIONAL,
		"salvage volume number or volume name");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL,
		"salvage log output file");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "salvage whole server");
    cmd_AddParm(ts, "-showlog", CMD_FLAG, CMD_OPTIONAL,
		"display salvage log");
    cmd_AddParm(ts, "-parallel", CMD_SINGLE, CMD_OPTIONAL,
		"# of max parallel partition salvaging");
    cmd_AddParm(ts, "-tmpdir", CMD_SINGLE, CMD_OPTIONAL,
		"directory to place tmp files");
    cmd_AddParm(ts, "-orphans", CMD_SINGLE, CMD_OPTIONAL,
		"ignore | remove | attach");
    cmd_AddParm(ts, "-forceDAFS", CMD_FLAG, CMD_OPTIONAL,
		"(DAFS) force salvage of demand attach fileserver");
    cmd_AddParm(ts, "-salvagedirs", CMD_FLAG, CMD_OPTIONAL,
		"Force rebuild/salvage of all directories");
    add_std_args(ts);

    ts = cmd_CreateSyntax("getrestricted", GetRestrict, NULL, 0,
			  "get restrict mode");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("setrestricted", SetRestrict, NULL, 0,
			  "set restrict mode");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-mode", CMD_SINGLE, 0, "mode to set");
    add_std_args(ts);
#endif

    code = cmd_Dispatch(argc, argv);
    rx_Finalize();
    exit(code);
}
