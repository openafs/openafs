/* 
 * $Id$
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid =
	"$Id$";
#endif /* lint || SABER */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#ifndef WINDOWS
#include <sys/param.h>
#include <sys/errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#endif /* WINDOWS */

/* on AIX AFS has an unresolved reference to osi_audit. We will define
 * it here as extern. It also trys to call the ntohl and htonl routines
 * as routines rather then macros. We need a real routine here. 
 * We do this before the ntohl and htonl macros are defined in net/in.h
 */
int osi_audit()
    { return(0);}

#if 0
#ifdef _AIX
u_long htonl(u_long x)
    { return(x);}

u_long ntohl(u_long x)
    { return(x);}
#endif

#include <netinet/in.h>
/* #include <krb.h> */
#endif /* 0 */

#include <krb5.h>

#ifdef WINDOWS

#ifdef PRE_AFS35
#include "afs_tokens.h"
#include "rxkad.h"
#else /* !PRE_AFS35 */
#include <afs/stds.h>
#include <afs/auth.h>
#include <rx/rxkad.h>
#include <afs/dirpath.h>
#endif /* PRE_AFS35 */

#else /* !WINDOWS */
#include <afs/stds.h>
#include <afs/com_err.h>

#include <afs/param.h>
#ifdef AFS_SUN5_ENV
#include <sys/ioccom.h>
#endif
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/ptserver.h>
#include <afs/dirpath.h>
#endif /* WINDOWS */

#include "aklog.h"
#include "linked_list.h"

#define AFSKEY "afs"
#define AFSINST ""

#ifndef AFS_TRY_FULL_PRINC
#define AFS_TRY_FULL_PRINC 0
#endif /* AFS_TRY_FULL_PRINC */

#define AKLOG_SUCCESS 0
#define AKLOG_USAGE 1
#define AKLOG_SOMETHINGSWRONG 2
#define AKLOG_AFS 3
#define AKLOG_KERBEROS 4
#define AKLOG_TOKEN 5
#define AKLOG_BADPATH 6
#define AKLOG_MISC 7

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAXSYMLINKS
/* RedHat 4.x doesn't seem to define this */
#define MAXSYMLINKS	5
#endif

#define DIR '/'			/* Character that divides directories */
#define DIRSTRING "/"		/* String form of above */
#define VOLMARKER ':'		/* Character separating cellname from mntpt */
#define VOLMARKERSTRING ":"	/* String form of above */

typedef struct {
    char cell[BUFSIZ];
    char realm[REALM_SZ];
} cellinfo_t;

struct afsconf_cell ak_cellconfig; /* General information about the cell */
static char linkedcell[MAXCELLCHARS+1];
static char linkedcell2[MAXCELLCHARS+1];

#ifdef WINDOWS

/* libafsconf.dll */
extern long cm_GetRootCellName();
extern long cm_SearchCellFile();

static long cm_SearchCellFile_CallBack();

#else /* !WINDOWS */

/*
 * Why doesn't AFS provide these prototypes?
 */

#ifdef AFS_INT32
typedef afs_int32 int32 ;
#endif

extern int afsconf_GetLocalCell(struct afsconf_dir *, char *, afs_int32);
extern int afsconf_GetCellInfo(struct afsconf_dir *, char *, char *,
			       struct afsconf_cell *);
extern int afsconf_Close(struct afsconf_dir *);
extern int ktc_GetToken(struct ktc_principal *, struct ktc_token *, int,
			struct ktc_principal *);
extern int ktc_SetToken(struct ktc_principal *, struct ktc_token *,
			struct ktc_principal *, int);
extern afs_int32 pr_Initialize(afs_int32, char *, char *, afs_int32);
extern int pr_SNameToId(char *, afs_int32 *);
extern int pr_CreateUser(char *, afs_int32 *);
extern int pr_End();
extern int pioctl(char *, afs_int32, struct ViceIoctl *, afs_int32);

/*
 * Other prototypes
 */

extern char *afs_realm_of_cell(krb5_context, struct afsconf_cell *);

#endif /* WINDOWS */

/*
 * Provide a replacement for strerror if we don't have it
 */

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(x) sys_errlist[x]
#endif /* HAVE_STRERROR */

static aklog_params params;	/* Various aklog functions */
static char msgbuf[BUFSIZ];	/* String for constructing error messages */
static char *progname = NULL;	/* Name of this program */
static int dflag = FALSE;	/* Give debugging information */
static int noauth = FALSE;	/* If true, don't try to get tokens */
static int zsubs = FALSE;	/* Are we keeping track of zephyr subs? */
static int hosts = FALSE;	/* Are we keeping track of hosts? */
static int noprdb = FALSE;	/* Skip resolving name to id? */
static int linked = FALSE;  /* try for both AFS nodes */
static int afssetpag = FALSE; /* setpag for AFS */
static int force = FALSE;	/* Bash identical tokens? */
static linked_list zsublist;	/* List of zephyr subscriptions */
static linked_list hostlist;	/* List of host addresses */
static linked_list authedcells;	/* List of cells already logged to */

/* ANL - CMU lifetime convert routine */
/* for K5.4.1 don't use this for now. Need to see if it is needed */
/* maybe needed in the krb524d module as well */
/* extern unsigned long krb_life_to_time(); */

#ifdef __STDC__
static char *copy_cellinfo(cellinfo_t *cellinfo)
#else
static char *copy_cellinfo(cellinfo)
  cellinfo_t *cellinfo;
#endif /* __STDC__ */
{
    cellinfo_t *new_cellinfo;

    if ((new_cellinfo = (cellinfo_t *)malloc(sizeof(cellinfo_t))))
	memcpy(new_cellinfo, cellinfo, sizeof(cellinfo_t));
    
    return ((char *)new_cellinfo);
}


#ifdef __STDC__
static char *copy_string(char *string)    
#else
static char *copy_string(string)
  char *string;
#endif /* __STDC__ */
{
    char *new_string;

    if ((new_string = (char *)calloc(strlen(string) + 1, sizeof(char))))
	(void) strcpy(new_string, string);

    return (new_string);
}


#ifdef __STDC__
static int get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char *local_cell, char *linkedcell)
#else
static int get_cellconfig(cell, cellconfig, local_cell, linkedcell)
    char *cell;
    struct afsconf_cell *cellconfig;
    char *local_cell;
	char *linkedcell;
#endif /* __STDC__ */
{
    int status = AKLOG_SUCCESS;
    struct afsconf_dir *configdir;
#ifndef PRE_AFS35
    char *dirpath;
#endif /* ! PRE_AFS35 */

    memset(local_cell, 0, sizeof(local_cell));
    memset((char *)cellconfig, 0, sizeof(*cellconfig));

#ifndef WINDOWS

    if (!(configdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	sprintf(msgbuf, 
		"%s: can't get afs configuration (afsconf_Open(%s))\n",
		progname, AFSDIR_CLIENT_ETC_DIRPATH);
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_AFS);
    }

    if (afsconf_GetLocalCell(configdir, local_cell, MAXCELLCHARS)) {
	sprintf(msgbuf, "%s: can't determine local cell.\n", progname);
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_AFS);
    }

    if ((cell == NULL) || (cell[0] == 0))
	cell = local_cell;

	linkedcell[0] = '\0';
    if (afsconf_GetCellInfo(configdir, cell, NULL, cellconfig)) {
	sprintf(msgbuf, "%s: Can't get information about cell %s.\n",
		progname, cell);
	params.pstderr(msgbuf);
	status = AKLOG_AFS;
    }
	if (cellconfig->linkedCell) 
		strncpy(linkedcell,cellconfig->linkedCell,MAXCELLCHARS);

    (void) afsconf_Close(configdir);

#else /* WINDOWS */
    /*
     * We'll try to mimic the GetCellInfo call here and fill in as much
     * of the afsconf_cell structure as we can.
     */
    if (cm_GetRootCellName(local_cell)) {
	sprintf(msgbuf, "%s: can't get local cellname\n", progname);
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_AFS);
    }

    if ((cell == NULL) || (cell[0] == 0))
	cell = local_cell;

    strcpy(cellconfig->name, cell);

    /* No way of figuring this out as far as I can tell */
    linkedcell[0] = '\0';

    /* Initialize server info */
    cellconfig->numServers = 0;
    cellconfig->hostName[0][0] = "\0";

    /*
     * Get servers of cell. cm_SearchCellFile_CallBack() gets call with
     * each server.
     */
#ifdef PRE_AFS35
    status = (int) cm_SearchCellFile(cell, &cm_SearchCellFile_CallBack,
#else
    status = (int) cm_SearchCellFile(cell, NULL, &cm_SearchCellFile_CallBack,
#endif
				     cellconfig /* rock */);

    switch(status) {
    case 0:
	break;

    case -1:
	sprintf(msgbuf, "%s: GetWindowsDirectory() failed.\n", progname);
	break;

    case -2:
	sprintf(msgbuf, "%s: Couldn't open afsdcells.ini for reading\n",
		progname);
	break;

    case -3:
	sprintf(msgbuf, "%s: Couldn't find any servers for cell %s\n",
		progname, cell);
	break;

    case -4:
	sprintf(msgbuf, "%s: Badly formatted line in afsdcells.ini (does not begin with a \">\" or contain \"#\"\n",
		progname);
	break;

    default:
	sprintf(msgbuf, "%s cm_SearchCellFile returned unknown error %d\n",
		status);
    }

    if (status) {
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_AFS);
    }

    status = AKLOG_SUCCESS;

    
#endif /* WINDOWS */

    return(status);
}


#ifdef WINDOWS
/*
 * Callback function for cm_SearchCellFile() in get_cellconfig() above.
 * This function gets called once for each server that is found for the cell.
 */
static long
cm_SearchCellFile_CallBack(void *rock /* cellconfig */,
			   struct sockaddr_in *addr, /* Not used */
			   char *server)
{
    struct afsconf_cell *cellconfig = rock;


    /*
     * Save server name and increment count of servers
     */
    strcpy(cellconfig->hostName[cellconfig->numServers++], server);
    
    return (long) 0;
}

    
#endif /* WINDOWS */


/* 
 * Log to a cell.  If the cell has already been logged to, return without
 * doing anything.  Otherwise, log to it and mark that it has been logged
 * to.
 */
#ifdef __STDC__
static int auth_to_cell(krb5_context context, char *cell, char *realm)
#else
static int auth_to_cell(context, cell, realm)

  krb5_context context;
  char *cell;
  char *realm;
#endif /* __STDC__ */
{
    int status = AKLOG_SUCCESS;
    char username[BUFSIZ];	/* To hold client username structure */
    long viceId;		/* AFS uid of user */

    char name[ANAME_SZ];	/* Name of afs key */
    char primary_instance[INST_SZ];	/* Instance of afs key */
    char secondary_instance[INST_SZ];	/* Backup instance to try */
    int try_secondary = 0;		/* Flag to indicate if we try second */
    char realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char local_cell[MAXCELLCHARS+1];
    char cell_to_use[MAXCELLCHARS+1]; /* Cell to authenticate to */
    static char lastcell[MAXCELLCHARS+1] = { 0 };
#ifndef WINDOWS
    static char confname[512] = { 0 };
#endif
    krb5_creds *v5cred = NULL;
    CREDENTIALS c;
    struct ktc_principal aserver;
    struct ktc_principal aclient;
    struct ktc_token atoken, btoken;

#ifdef ALLOW_REGISTER
    afs_int32 id;
#endif /* ALLOW_REGISTER */

    memset(name, 0, sizeof(name));
    memset(primary_instance, 0, sizeof(primary_instance));
    memset(secondary_instance, 0, sizeof(secondary_instance));
    memset(realm_of_user, 0, sizeof(realm_of_user));
    memset(realm_of_cell, 0, sizeof(realm_of_cell));

#ifndef WINDOWS
    if (confname[0] == '\0') {
	strncpy(confname, AFSDIR_CLIENT_ETC_DIRPATH, sizeof(confname));
	confname[sizeof(confname) - 2] = '\0';
    }
#endif /* WINDOWS */

    /* NULL or empty cell returns information on local cell */
    if ((status = get_cellconfig(cell, &ak_cellconfig,
			 local_cell, linkedcell)))
	return(status);

    strncpy(cell_to_use, ak_cellconfig.name, MAXCELLCHARS);
    cell_to_use[MAXCELLCHARS] = 0;

    if (ll_string(&authedcells, ll_s_check, cell_to_use)) {
	if (dflag) {
	    sprintf(msgbuf, "Already authenticated to %s (or tried to)\n", 
		    cell_to_use);
	    params.pstdout(msgbuf);
	}
	return(AKLOG_SUCCESS);
    }

    /* 
     * Record that we have attempted to log to this cell.  We do this
     * before we try rather than after so that we will not try
     * and fail repeatedly for one cell.
     */
    (void)ll_string(&authedcells, ll_s_add, cell_to_use);

    /* 
     * Record this cell in the list of zephyr subscriptions.  We may
     * want zephyr subscriptions even if authentication fails.
     * If this is done after we attempt to get tokens, aklog -zsubs
     * can return something different depending on whether or not we
     * are in -noauth mode.
     */
    if (ll_string(&zsublist, ll_s_add, cell_to_use) == LL_FAILURE) {
	sprintf(msgbuf, 
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, cell_to_use);
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_MISC);
    }
    if (ll_string(&zsublist, ll_s_add, local_cell) == LL_FAILURE) {
	sprintf(msgbuf, 
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, local_cell);
	params.pstderr(msgbuf);
	params.exitprog(AKLOG_MISC);
    }

    if (!noauth) {
	if (dflag) {
	    sprintf(msgbuf, "Authenticating to cell %s (server %s).\n",
		    cell_to_use, ak_cellconfig.hostName[0]);
	    params.pstdout(msgbuf);
	}

	/*
	 * Find out which realm we're supposed to authenticate to.  If one
	 * is not included, use the kerberos realm found in the credentials
	 * cache.
	 */

	if (realm && realm[0]) {
	    strcpy(realm_of_cell, realm);
	    if (dflag) {
		sprintf(msgbuf, "We were told to authenticate to realm %s.\n",
			realm);
		params.pstdout(msgbuf);
	    }
	}
	else {
	    char *realm = afs_realm_of_cell(context, &ak_cellconfig);

	    if (!realm) {
		sprintf(msgbuf, 
			"%s: Couldn't figure out realm for cell %s.\n",
			progname, cell_to_use);
		params.pstderr(msgbuf);
		params.exitprog(AKLOG_MISC);
	    }

	    strcpy(realm_of_cell, realm);

	    if (dflag) {
		sprintf(msgbuf, "We've deduced that we need to authenticate to"
			" realm %s.\n", realm_of_cell);
		params.pstdout(msgbuf);
	    }
	}

	/* We use the afs.<cellname> convention here... 
	 *
	 * Doug Engert's original code had principals of the form:
	 *
	 * "afsx/cell@realm"
	 *
	 * in the KDC, so the name wouldn't conflict with DFS.  Since we're
	 * not using DFS, I changed it just to look for the following
	 * principals:
	 *
	 * afs/<cell>@<realm>
	 * afs@<realm>
	 *
	 * Because people are transitioning from afs@realm to afs/cell,
	 * we configure things so that if the first one isn't found, we
	 * try the second one.  You can select which one you prefer with
	 * a configure option.
	 */

	strcpy(name, AFSKEY);

	if (AFS_TRY_FULL_PRINC || strcasecmp(cell_to_use, realm_of_cell) != 0) {
	    strncpy(primary_instance, cell_to_use, sizeof(primary_instance));
	    primary_instance[sizeof(primary_instance)-1] = '\0';
	    if (strcasecmp(cell_to_use, realm_of_cell) == 0) {
		try_secondary = 1;
		secondary_instance[0] = '\0';
	    }
	} else {
	    primary_instance[0] = '\0';
	    try_secondary = 1;
	    strncpy(secondary_instance, cell_to_use,
		    sizeof(secondary_instance));
	    secondary_instance[sizeof(secondary_instance)-1] = '\0';
	}

	/* 
	 * Extract the session key from the ticket file and hand-frob an
	 * afs style authenticator.
	 */

	/*
	 * Try to obtain AFS tickets.  Because there are two valid service
	 * names, we will try both, but trying the more specific first.
	 *
	 *	afs/<cell>@<realm> i.e. allow for single name with "."
	 * 	afs@<realm>
	 */
#if 0
	if (dflag) {
		dee_gettokens(); /* DEBUG */
	}
#endif

	if (dflag) {
	    sprintf(msgbuf, "Getting tickets: %s/%s@%s\n", name,
		    primary_instance, realm_of_cell);
	    params.pstdout(msgbuf);
	}

	status = params.get_cred(context, name, primary_instance, realm_of_cell,
			 &c, &v5cred);

	if (status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
	    if (try_secondary) {
		if (dflag) {
		    sprintf(msgbuf, "Principal not found, trying alternate "
			    "service name: %s/%s@%s\n", name,
			    secondary_instance, realm_of_cell);
		    params.pstdout(msgbuf);
		}
		status = params.get_cred(context, name, secondary_instance,
					 realm_of_cell, &c, &v5cred);
	    }
	}

	if (status != KSUCCESS) {
	    if (dflag) {
		sprintf(msgbuf, 
			"Kerberos error code returned by get_cred: %d\n",
			status);
		params.pstdout(msgbuf);
	    }
	    sprintf(msgbuf, "%s: Couldn't get %s AFS tickets:\n",
		    progname, cell_to_use);
	    params.pstderr(msgbuf);
		com_err(progname, status, "while getting AFS tickets");
	    return(AKLOG_KERBEROS);
	}

	strncpy(aserver.name, AFSKEY, MAXKTCNAMELEN - 1);
	strncpy(aserver.instance, AFSINST, MAXKTCNAMELEN - 1);
	strncpy(aserver.cell, cell_to_use, MAXKTCREALMLEN - 1);

	strcpy (username, c.pname);
	if (c.pinst[0]) {
	    strcat (username, ".");
	    strcat (username, c.pinst);
	}

	atoken.kvno = c.kvno;
	atoken.startTime = c.issue_date;
	/*
	 * It seems silly to go through a bunch of contortions to
	 * extract the expiration time, when the v5 credentials already
	 * has the exact time!  Let's use that instead.
	 *
	 * Note that this isn't a security hole, as the expiration time
	 * is also contained in the encrypted token
	 */
	atoken.endTime = v5cred->times.endtime;
	memcpy(&atoken.sessionKey, c.session, 8);
	atoken.ticketLen = c.ticket_st.length;
	memcpy(atoken.ticket, c.ticket_st.dat, atoken.ticketLen);
	
	if (!force &&
	    !ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient) &&
	    atoken.kvno == btoken.kvno &&
	    atoken.ticketLen == btoken.ticketLen &&
	    !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
	    !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) {

	    if (dflag) {
		sprintf(msgbuf, "Identical tokens already exist; skipping.\n");
		params.pstdout(msgbuf);
	    }
	    return 0;
	}

#ifdef FORCE_NOPRDB
	noprdb = 1;
#endif

#ifndef WINDOWS
	if (noprdb) {
#endif
	    if (dflag) {
		sprintf(msgbuf, "Not resolving name %s to id (-noprdb set)\n",
			username);
		params.pstdout(msgbuf);
	    }
#ifndef WINDOWS
	}
	else {
	    if ((status = params.get_user_realm(context, realm_of_user)) != KSUCCESS) {
		sprintf(msgbuf, "%s: Couldn't determine realm of user:)",
			progname);
		params.pstderr(msgbuf);
		com_err(progname, status, " while getting realm");
		return(AKLOG_KERBEROS);
	    }
	    if (strcmp(realm_of_user, realm_of_cell)) {
		strcat(username, "@");
		strcat(username, realm_of_user);
	    }

	    if (dflag) {
		sprintf(msgbuf, "About to resolve name %s to id in cell %s.\n", 
			username, aserver.cell);
		params.pstdout(msgbuf);
	    }

	    /*
	     * Talk about DUMB!  It turns out that there is a bug in
	     * pr_Initialize -- even if you give a different cell name
	     * to it, it still uses a connection to a previous AFS server
	     * if one exists.  The way to fix this is to change the
	     * _filename_ argument to pr_Initialize - that forces it to
	     * re-initialize the connection.  We do this by adding and
	     * removing a "/" on the end of the configuration directory name.
	     */

	    if (lastcell[0] != '\0' && (strcmp(lastcell, aserver.cell) != 0)) {
		int i = strlen(confname);
		if (confname[i - 1] == '/') {
		    confname[i - 1] = '\0';
		} else {
		    confname[i] = '/';
		    confname[i + 1] = '\0';
		}
	    }

	    strcpy(lastcell, aserver.cell);

	    if (!pr_Initialize (0, confname, aserver.cell, 0))
		    status = pr_SNameToId (username, &viceId);
	    
	    if (dflag) {
		if (status) 
		    sprintf(msgbuf, "Error %d\n", status);
		else
		    sprintf(msgbuf, "Id %d\n", (int) viceId);
		params.pstdout(msgbuf);
	    }
	    
		/*
		 * This is a crock, but it is Transarc's crock, so
		 * we have to play along in order to get the
		 * functionality.  The way the afs id is stored is
		 * as a string in the username field of the token.
		 * Contrary to what you may think by looking at
		 * the code for tokens, this hack (AFS ID %d) will
		 * not work if you change %d to something else.
		 */

		/*
		 * This code is taken from cklog -- it lets people
		 * automatically register with the ptserver in foreign cells
		 */

#ifdef ALLOW_REGISTER
	if (status == 0) {
	    if (viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
	    if ((status == 0) && (viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
		sprintf (username, "AFS ID %d", (int) viceId);
#ifdef ALLOW_REGISTER
	    } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
		if (dflag) {
		    sprintf(msgbuf, "doing first-time registration of %s "
			    "at %s\n", username, cell_to_use);
		    params.pstdout(msgbuf);
		}
		id = 0;
		strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
		strcpy(aclient.instance, "");
		strncpy(aclient.cell, c.realm, MAXKTCREALMLEN - 1);
		if ((status = ktc_SetToken(&aserver, &atoken, &aclient, 0))) {
		    sprintf(msgbuf, "%s: unable to obtain tokens for cell %s "
			    "(status: %d).\n", progname, cell_to_use, status);
		    params.pstderr(msgbuf);
		    status = AKLOG_TOKEN;
		}

		/*
		 * In case you're wondering, we don't need to change the
		 * filename here because we're still connecting to the
		 * same cell -- we're just using a different authentication
		 * level
		 */

		if ((status = pr_Initialize(1L, confname, aserver.cell, 0))) {
		    sprintf(msgbuf, "Error %d\n", status);
		    params.pstdout(msgbuf);
		}

		if ((status = pr_CreateUser(username, &id))) {
		    sprintf(msgbuf, "%s: %s so unable to create remote PTS "
			    "user %s in cell %s (status: %d).\n", progname,
			    error_message(status), username, cell_to_use,
			    status);
		    params.pstdout(msgbuf);
		} else {
		    sprintf(msgbuf, "created cross-cell entry for %s at %s\n",
			    username, cell_to_use);
		    params.pstdout(msgbuf);
		    sprintf(username, "AFS ID %d", (int) id);
		}
	    }
	}
#endif /* ALLOW_REGISTER */

	}
#endif /* !WINDOWS */

	if (dflag) {
	    sprintf(msgbuf, "Set username to %s\n", username);
	    params.pstdout(msgbuf);
	}

	/* Reset the "aclient" structure before we call ktc_SetToken.
	 * This structure was first set by the ktc_GetToken call when
	 * we were comparing whether identical tokens already existed.
	 */
	strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
	strcpy(aclient.instance, "");
	strncpy(aclient.cell, c.realm, MAXKTCREALMLEN - 1);

	if (dflag) {
	    sprintf(msgbuf, "Setting tokens. %s / %s @ %s \n",
			aclient.name, aclient.instance, aclient.cell );
	    params.pstdout(msgbuf);
	}
	/* on AIX 4.1.4 with AFS 3.4a+ if a write is not done before 
	 * this routine, it will not add the token. It is not clear what 
	 * is going on here! So we will do the following operation
	 */
	write(2,"",0); /* dummy write */
#ifndef WINDOWS
	if ((status = ktc_SetToken(&aserver, &atoken, &aclient, afssetpag))) {
	    sprintf(msgbuf, 
		    "%s: unable to obtain tokens for cell %s (status: %d).\n",
		    progname, cell_to_use, status);
	    params.pstderr(msgbuf);
	    status = AKLOG_TOKEN;
	}
#else /* WINDOWS */
	/* Note switched 2nd and 3rd args */
#ifdef PRE_AFS35
	if ((status = ktc_SetToken(&aserver, &aclient, &atoken, afssetpag))) {
#else
	if ((status = ktc_SetToken(&aserver, &atoken, &aclient, afssetpag))) {
#endif
	    switch(status) {
	    case KTC_INVAL:
		sprintf(msgbuf, "%s: Bad ticket length", progname);
		break;
	    case KTC_PIOCTLFAIL:
		sprintf(msgbuf, "%s: Unknown error contacting AFS service",
			progname);
		break;
	    case KTC_NOCELL:
		sprintf(msgbuf, "%s: Cell name (%s) not recognized by AFS service",
			progname, realm_of_cell);
		break;
	    case KTC_NOCM:
		sprintf(msgbuf, "%s: AFS service is unavailable", progname);
		break;
	    default:
		sprintf(msgbuf, "%s: Undocumented error (%d) contacting AFS service", progname, status);
		break;	
	    }
	    params.pstderr(msgbuf);
	    status = AKLOG_TOKEN;	    
	}
#endif /* !WINDOWS */
    }
    else
	if (dflag) {
	    sprintf(msgbuf, "Noauth mode; not authenticating.\n");
	    params.pstdout(msgbuf);
	}
	
    return(status);
}

#ifndef WINDOWS /* struct ViceIoctl missing */

#ifdef __STDC__
static int get_afs_mountpoint(char *file, char *mountpoint, int size)
#else
static int get_afs_mountpoint(file, mountpoint, size)
  char *file;
  char *mountpoint;
  int size;
#endif /* __STDC__ */
{
#ifdef AFS_SUN_ENV
	char V ='V'; /* AFS has problem on Sun with pioctl */
#endif
    char our_file[MAXPATHLEN + 1];
    char *parent_dir;
    char *last_component;
    struct ViceIoctl vio;
    char cellname[BUFSIZ];

    memset(our_file, 0, sizeof(our_file));
    strcpy(our_file, file);

    if ((last_component = strrchr(our_file, DIR))) {
	*last_component++ = 0;
	parent_dir = our_file;
    }
    else {
	last_component = our_file;
	parent_dir = ".";
    }    
    
    memset(cellname, 0, sizeof(cellname));

    vio.in = last_component;
    vio.in_size = strlen(last_component)+1;
    vio.out_size = size;
    vio.out = mountpoint;

    if (!pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &vio, 0)) {
	if (strchr(mountpoint, VOLMARKER) == NULL) {
	    vio.in = file;
	    vio.in_size = strlen(file) + 1;
	    vio.out_size = sizeof(cellname);
	    vio.out = cellname;
	    
	    if (!pioctl(file, VIOC_FILE_CELL_NAME, &vio, 1)) {
		strcat(cellname, VOLMARKERSTRING);
		strcat(cellname, mountpoint + 1);
		memset(mountpoint + 1, 0, size - 1);
		strcpy(mountpoint + 1, cellname);
	    }
	}
	return(TRUE);
    }
    else
	return(FALSE);
}

/* 
 * This routine each time it is called returns the next directory 
 * down a pathname.  It resolves all symbolic links.  The first time
 * it is called, it should be called with the name of the path
 * to be descended.  After that, it should be called with the arguemnt
 * NULL.
 */
#ifdef __STDC__
static char *next_path(char *origpath)
#else
static char *next_path(origpath)
  char *origpath;
#endif /* __STDC__ */
{
    static char path[MAXPATHLEN + 1];
    static char pathtocheck[MAXPATHLEN + 1];

    int link = FALSE;		/* Is this a symbolic link? */
    char linkbuf[MAXPATHLEN + 1];
    char tmpbuf[MAXPATHLEN + 1];

    static char *last_comp;	/* last component of directory name */
    static char *elast_comp;	/* End of last component */
    char *t;
    int len;
    
    static int symlinkcount = 0; /* We can't exceed MAXSYMLINKS */
    
    /* If we are given something for origpath, we are initializing only. */
    if (origpath) {
	memset(path, 0, sizeof(path));
	memset(pathtocheck, 0, sizeof(pathtocheck));
	strcpy(path, origpath);
	last_comp = path;
	symlinkcount = 0;
	return(NULL);
    }

    /* We were not given origpath; find then next path to check */
    
    /* If we've gotten all the way through already, return NULL */
    if (last_comp == NULL)
	return(NULL);

    do {
	while (*last_comp == DIR)
	    strncat(pathtocheck, last_comp++, 1);
	len = (elast_comp = strchr(last_comp, DIR)) 
	    ? elast_comp - last_comp : strlen(last_comp);
	strncat(pathtocheck, last_comp, len);
	memset(linkbuf, 0, sizeof(linkbuf));
	if ((link = (params.readlink(pathtocheck, linkbuf, 
				    sizeof(linkbuf)) > 0))) {
	    if (++symlinkcount > MAXSYMLINKS) {
		sprintf(msgbuf, "%s: %s\n", progname, strerror(ELOOP));
		params.pstderr(msgbuf);
		params.exitprog(AKLOG_BADPATH);
	    }
	    memset(tmpbuf, 0, sizeof(tmpbuf));
	    if (elast_comp)
		strcpy(tmpbuf, elast_comp);
	    if (linkbuf[0] == DIR) {
		/* 
		 * If this is a symbolic link to an absolute path, 
		 * replace what we have by the absolute path.
		 */
		memset(path, 0, strlen(path));
		memcpy(path, linkbuf, sizeof(linkbuf));
		strcat(path, tmpbuf);
		last_comp = path;
		elast_comp = NULL;
		memset(pathtocheck, 0, sizeof(pathtocheck));
	    }
	    else {
		/* 
		 * If this is a symbolic link to a relative path, 
		 * replace only the last component with the link name.
		 */
		strncpy(last_comp, linkbuf, strlen(linkbuf) + 1);
		strcat(path, tmpbuf);
		elast_comp = NULL;
		if ((t = strrchr(pathtocheck, DIR))) {
		    t++;
		    memset(t, 0, strlen(t));
		}
		else
		    memset(pathtocheck, 0, sizeof(pathtocheck));
	    }
	}
	else
	    last_comp = elast_comp;
    }
    while(link);

    return(pathtocheck);
}

#endif /* WINDOWS */

#if 0
/*****************************************/
int dee_gettokens()
{
#ifdef AFS_SUN_ENV
	char V = 'V'; /* AFS has problem on SunOS */
#endif
   struct ViceIoctl vio;
   char outbuf[BUFSIZ];
   long ind;
   int fd;

   memset(outbuf, 0, sizeof(outbuf));

   vio.out_size = sizeof(outbuf);
   vio.in_size = sizeof(ind);
   vio.out = outbuf;
   vio.in = &ind;

   ind = 0;
   fd = open("dee.tok",O_WRONLY);
   while(!pioctl(0,VIOCGETTOK,&vio,0)) {
	write(fd,&outbuf,sizeof(outbuf)); 
       ind++;
   }
   close(fd);
}
/*****************************************/
#endif

#ifndef WINDOWS /* struct ViceIoctl missing */

#ifdef __STDC__
static void add_hosts(char *file)
#else
static void add_hosts(file)
  char *file;
#endif /* __STDC__ */
{
#ifdef AFS_SUN_ENV
	char V = 'V'; /* AFS has problem on SunOS */
#endif
    struct ViceIoctl vio;
    char outbuf[BUFSIZ];
    long *phosts;
    int i;
    struct hostent *hp;
    struct in_addr in;
    
    memset(outbuf, 0, sizeof(outbuf));

    vio.out_size = sizeof(outbuf);
    vio.in_size = 0;
    vio.out = outbuf;

    if (dflag) {
	sprintf(msgbuf, "Getting list of hosts for %s\n", file);
	params.pstdout(msgbuf);
    }
    /* Don't worry about errors. */
    if (!pioctl(file, VIOCWHEREIS, &vio, 1)) {
	phosts = (long *) outbuf;

	/*
	 * Lists hosts that we care about.  If ALLHOSTS is defined,
	 * then all hosts that you ever may possible go through are
	 * included in this list.  If not, then only hosts that are
	 * the only ones appear.  That is, if a volume you must use
	 * is replaced on only one server, that server is included.
	 * If it is replicated on many servers, then none are included.
	 * This is not perfect, but the result is that people don't
	 * get subscribed to a lot of instances of FILSRV that they
	 * probably won't need which reduces the instances of 
	 * people getting messages that don't apply to them.
	 */
#ifndef ALLHOSTS
	if (phosts[1] != '\0')
	    return;
#endif
	for (i = 0; phosts[i]; i++) {
	    if (hosts) {
		in.s_addr = phosts[i];
		if (dflag) {
		    sprintf(msgbuf, "Got host %s\n", inet_ntoa(in));
		    params.pstdout(msgbuf);
		}
		ll_string(&hostlist, ll_s_add, (char *)inet_ntoa(in));
	    }
	    if (zsubs && (hp=gethostbyaddr((char *) &phosts[i],sizeof(long),AF_INET))) {
		if (dflag) {
		    sprintf(msgbuf, "Got host %s\n", hp->h_name);
		    params.pstdout(msgbuf);
		}
		ll_string(&zsublist, ll_s_add, hp->h_name);
	    }
	}
    }
}

#endif /* WINDOWS */

#ifndef WINDOWS /* next_path(), get_afs_mountpoint() */

/*
 * This routine descends through a path to a directory, logging to 
 * every cell it encounters along the way.
 */
#ifdef __STDC__
static int auth_to_path(krb5_context context, char *path)
#else
static int auth_to_path(context, path)
  krb5_context context;
  char *path;			/* The path to which we try to authenticate */
#endif /* __STDC__ */
{
    int status = AKLOG_SUCCESS;
    int auth_to_cell_status = AKLOG_SUCCESS;

    char *nextpath;
    char pathtocheck[MAXPATHLEN + 1];
    char mountpoint[MAXPATHLEN + 1];

    char *cell;
    char *endofcell;

    u_char isdir;

    /* Initialize */
    if (path[0] == DIR)
	strcpy(pathtocheck, path);
    else {
	if (params.getwd(pathtocheck) == NULL) {
	    sprintf(msgbuf, "Unable to find current working directory:\n");
	    params.pstderr(msgbuf);
	    sprintf(msgbuf, "%s\n", pathtocheck);
	    params.pstderr(msgbuf);
	    sprintf(msgbuf, "Try an absolute pathname.\n");
	    params.pstderr(msgbuf);
	    params.exitprog(AKLOG_BADPATH);
	}
	else {
	    strcat(pathtocheck, DIRSTRING);
	    strcat(pathtocheck, path);
	}
    }
    next_path(pathtocheck);

    /* Go on to the next level down the path */
    while ((nextpath = next_path(NULL))) {
	strcpy(pathtocheck, nextpath);
	if (dflag) {
	    sprintf(msgbuf, "Checking directory %s\n", pathtocheck);
	    params.pstdout(msgbuf);
	}
	/* 
	 * If this is an afs mountpoint, determine what cell from 
	 * the mountpoint name which is of the form 
	 * #cellname:volumename or %cellname:volumename.
	 */
	if (get_afs_mountpoint(pathtocheck, mountpoint, sizeof(mountpoint))) {
	    /* skip over the '#' or '%' */
	    cell = mountpoint + 1;
	    /* Add this (cell:volumename) to the list of zsubs */
	    if (zsubs)
		ll_string(&zsublist, ll_s_add, cell);
	    if (zsubs || hosts)
		add_hosts(pathtocheck);
	    if ((endofcell = strchr(mountpoint, VOLMARKER))) {
		*endofcell = '\0';
		if ((auth_to_cell_status = auth_to_cell(context, cell, NULL))) {
		    if (status == AKLOG_SUCCESS)
			status = auth_to_cell_status;
		    else if (status != auth_to_cell_status)
			status = AKLOG_SOMETHINGSWRONG;
		}
	    }
	}
	else {
	    if (params.isdir(pathtocheck, &isdir) < 0) {
		/*
		 * If we've logged and still can't stat, there's
		 * a problem... 
		 */
		sprintf(msgbuf, "%s: stat(%s): %s\n", progname, 
			pathtocheck, strerror(errno));
		params.pstderr(msgbuf);
		return(AKLOG_BADPATH);
	    }
	    else if (! isdir) {
		/* Allow only directories */
		sprintf(msgbuf, "%s: %s: %s\n", progname, pathtocheck,
			strerror(ENOTDIR));
		params.pstderr(msgbuf);
		return(AKLOG_BADPATH);
	    }
	}
    }
    

    return(status);
}

#endif /* WINDOWS */


/* Print usage message and exit */
#ifdef __STDC__
static void usage(void)
#else
static void usage()
#endif /* __STDC__ */
{
    sprintf(msgbuf, "\nUsage: %s %s%s%s\n", progname,
	    "[-d] [[-cell | -c] cell [-k krb_realm]] ",
	    "[[-p | -path] pathname]\n",
	    "    [-zsubs] [-hosts] [-noauth] [-noprdb] [-force] [-setpag] [-linked]\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -d gives debugging information.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    krb_realm is the kerberos realm of a cell.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    pathname is the name of a directory to which ");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "you wish to authenticate.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -zsubs gives zephyr subscription information.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -hosts gives host address information.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -noauth does not attempt to get tokens.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -noprdb means don't try to determine AFS ID.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -force means replace identical tickets. \n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -linked means if AFS node is linked, try both. \n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    -setpag set the AFS process authentication group.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "    No commandline arguments means ");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "authenticate to the local cell.\n");
    params.pstderr(msgbuf);
    sprintf(msgbuf, "\n");
    params.pstderr(msgbuf);
    params.exitprog(AKLOG_USAGE);
}

#ifdef __STDC__
void aklog(int argc, char *argv[], aklog_params *a_params)
#else
void aklog(argc, argv, a_params)
  int argc;
  char *argv[];
  aklog_params *a_params;
#endif /* __STDC__ */
{
	krb5_context context;
    int status = AKLOG_SUCCESS;
    int i;
    int somethingswrong = FALSE;

    cellinfo_t cellinfo;

    extern char *progname;	/* Name of this program */

    extern int dflag;		/* Debug mode */

    int cmode = FALSE;		/* Cellname mode */
    int pmode = FALSE;		/* Path name mode */

    char realm[REALM_SZ];	/* Kerberos realm of afs server */
    char cell[BUFSIZ];		/* Cell to which we are authenticating */
    char path[MAXPATHLEN + 1];		/* Path length for path mode */

    linked_list cells;		/* List of cells to log to */
    linked_list paths;		/* List of paths to log to */
    ll_node *cur_node;

    memset(&cellinfo, 0, sizeof(cellinfo));

    memset(realm, 0, sizeof(realm));
    memset(cell, 0, sizeof(cell));
    memset(path, 0, sizeof(path));

    ll_init(&cells);
    ll_init(&paths);

    ll_init(&zsublist);
    ll_init(&hostlist);

    /* Store the program name here for error messages */
    if ((progname = strrchr(argv[0], DIR)))
	progname++;
    else
	progname = argv[0];

    krb5_init_context(&context);
#ifndef WINDOWS
	initialize_ktc_error_table ();
#endif

    memcpy((char *)&params, (char *)a_params, sizeof(aklog_params));

    /* Initialize list of cells to which we have authenticated */
    (void)ll_init(&authedcells);

    /* Parse commandline arguments and make list of what to do. */
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-d") == 0)
	    dflag++;
	else if (strcmp(argv[i], "-noauth") == 0) 
	    noauth++;
	else if (strcmp(argv[i], "-zsubs") == 0)
	    zsubs++;
	else if (strcmp(argv[i], "-hosts") == 0)
	    hosts++;
	else if (strcmp(argv[i], "-noprdb") == 0)
	    noprdb++;
	else if (strcmp(argv[i], "-linked") == 0)
		linked++;
	else if (strcmp(argv[i], "-force") == 0)
	    force++;
    else if (strcmp(argv[i], "-setpag") == 0)
	    afssetpag++;
	else if (((strcmp(argv[i], "-cell") == 0) ||
		  (strcmp(argv[i], "-c") == 0)) && !pmode)
	    if (++i < argc) {
		cmode++;
		strcpy(cell, argv[i]);
	    }
	    else
		usage();
	else if (((strcmp(argv[i], "-path") == 0) ||
		  (strcmp(argv[i], "-p") == 0)) && !cmode)
#ifndef WINDOWS
	    if (++i < argc) {
		pmode++;
		strcpy(path, argv[i]);
	    }
	    else
		usage();
#else /* WINDOWS */
	{
	    sprintf(msgbuf, "%s: path mode not supported.\n", progname);
	    params.pstderr(msgbuf);
	    params.exitprog(AKLOG_MISC);
	}
#endif /* WINDOWS */
	    
	else if (argv[i][0] == '-')
	    usage();
	else if (!pmode && !cmode) {
	    if (strchr(argv[i], DIR) || (strcmp(argv[i], ".") == 0) ||
		(strcmp(argv[i], "..") == 0)) {
#ifndef WINDOWS
		pmode++;
		strcpy(path, argv[i]);
#else /* WINDOWS */
		sprintf(msgbuf, "%s: path mode not supported.\n", progname);
		params.pstderr(msgbuf);
		params.exitprog(AKLOG_MISC);
#endif /* WINDOWS */
	    }
	    else { 
		cmode++;
		strcpy(cell, argv[i]);
	    }
	}
	else
	    usage();

	if (cmode) {
	    if (((i + 1) < argc) && (strcmp(argv[i + 1], "-k") == 0)) {
		i+=2;
		if (i < argc)
		    strcpy(realm, argv[i]);
		else
		    usage();
	    }
	    /* Add this cell to list of cells */
	    strcpy(cellinfo.cell, cell);
	    strcpy(cellinfo.realm, realm);
	    if ((cur_node = ll_add_node(&cells, ll_tail))) {
		char *new_cellinfo;
		if ((new_cellinfo = copy_cellinfo(&cellinfo)))
		    ll_add_data(cur_node, new_cellinfo);
		else {
		    sprintf(msgbuf, 
			    "%s: failure copying cellinfo.\n", progname);
		    params.pstderr(msgbuf);
		    params.exitprog(AKLOG_MISC);
		}
	    }
	    else {
		sprintf(msgbuf, "%s: failure adding cell to cells list.\n",
			progname);
		params.pstderr(msgbuf);
		params.exitprog(AKLOG_MISC);
	    }
	    memset(&cellinfo, 0, sizeof(cellinfo));
	    cmode = FALSE;
	    memset(cell, 0, sizeof(cell));
	    memset(realm, 0, sizeof(realm));
	}
#ifndef WINDOWS
	else if (pmode) {
	    /* Add this path to list of paths */
	    if ((cur_node = ll_add_node(&paths, ll_tail))) {
		char *new_path; 
		if ((new_path = copy_string(path)))
		    ll_add_data(cur_node, new_path);
		else {
		    sprintf(msgbuf, "%s: failure copying path name.\n",
			    progname);
		    params.pstderr(msgbuf);
		    params.exitprog(AKLOG_MISC);
		}
	    }
	    else {
		sprintf(msgbuf, "%s: failure adding path to paths list.\n",
			progname);
		params.pstderr(msgbuf);
		params.exitprog(AKLOG_MISC);
	    }
	    pmode = FALSE;
	    memset(path, 0, sizeof(path));
	}
#endif /* WINDOWS */
    }

    /*
     * The code that _used_ to be here called setpag().  When you think
     * about this, doing this makes no sense!  setpag() allocates a PAG
     * only for the current process, so the token installed would have
     * not be usable in the parent!  Since ktc_SetToken() now takes a
     * 4th argument to control whether or not we're going to allocate
     * a PAG (and since when you do it _that_ way, it modifies the cred
     * structure of your parent)), why don't we use that instead?
     */

#if 0
    if (afssetpag) {
	   status = setpag();
	   if (dflag) { 
	     int i,j;
		 int gidsetlen = 50;
         int gidset[50];

		 printf("setpag %d\n",status);
	     j = getgroups(gidsetlen,gidset);
         printf("Groups(%d):",j);
         for (i = 0; i<j; i++) {
           printf("%d",gidset[i]);
           if((i+1)<j) printf(",");
         }
         printf("\n");
	   }
	}
#endif
    /* If nothing was given, log to the local cell. */
    if ((cells.nelements + paths.nelements) == 0) {
		struct passwd *pwd;

		status = auth_to_cell(context, NULL, NULL);
	
	 	/* If this cell is linked to a DCE cell, and user 
		 * requested -linked, get tokens for both 
		 * This is very usefull when the AFS cell is linked to a DFS 
	         * cell and this system does not also have DFS. 
	         */

		if (!status && linked && linkedcell[0]) {
				strncpy(linkedcell2,linkedcell,MAXCELLCHARS);
			    if (dflag) {
			        sprintf(msgbuf, "Linked cell: %s\n",
			            linkedcell);
			        params.pstdout(msgbuf);
			    }
				status = auth_to_cell(context, linkedcell2, NULL);
		}

#ifndef WINDOWS
		/*
		 * Local hack - if the person has a file in their home
		 * directory called ".xlog", read that for a list of
		 * extra cells to authenticate to
		 */

		if ((pwd = getpwuid(getuid())) != NULL) {
		    struct stat sbuf;
		    FILE *f;
		    char fcell[100], xlog_path[512];

		    strcpy(xlog_path, pwd->pw_dir);
		    strcat(xlog_path, "/.xlog");

		    if ((stat(xlog_path, &sbuf) == 0) &&
			((f = fopen(xlog_path, "r")) != NULL)) {

			if (dflag) {
			    sprintf(msgbuf, "Reading %s for cells to "
				    "authenticate to.\n", xlog_path);
			    params.pstdout(msgbuf);
			}

			while (fgets(fcell, 100, f) != NULL) {
			    int auth_status;

			    fcell[strlen(fcell) - 1] = '\0';

			    if (dflag) {
				sprintf(msgbuf, "Found cell %s in %s.\n",
					fcell, xlog_path);
				params.pstdout(msgbuf);
			    }

			    auth_status = auth_to_cell(context, fcell, NULL);
			    if (status == AKLOG_SUCCESS)
				status = auth_status;
			    else
				status = AKLOG_SOMETHINGSWRONG;
			}
		    }
		}
#endif /* WINDOWS */
	}
    else {
	/* Log to all cells in the cells list first */
	for (cur_node = cells.first; cur_node; cur_node = cur_node->next) {
	    memcpy((char *)&cellinfo, cur_node->data, sizeof(cellinfo));
	    if ((status = auth_to_cell(context, cellinfo.cell, cellinfo.realm)))
		somethingswrong++;
		else {
			if (linked && linkedcell[0]) {
				strncpy(linkedcell2,linkedcell,MAXCELLCHARS);
                if (dflag) {
                    sprintf(msgbuf, "Linked cell: %s\n",
                        linkedcell);
                    params.pstdout(msgbuf);
                }
				if ((status = auth_to_cell(context,linkedcell2,
							 cellinfo.realm)))
				somethingswrong++;
			}
		}
	}
	
#ifndef WINDOWS
	/* Then, log to all paths in the paths list */
	for (cur_node = paths.first; cur_node; cur_node = cur_node->next) {
	    if ((status = auth_to_path(context, cur_node->data)))
		somethingswrong++;
	}
#endif /* WINDOWS */
	
	/* 
	 * If only one thing was logged to, we'll return the status 
	 * of the single call.  Otherwise, we'll return a generic
	 * something failed status.
	 */
	if (somethingswrong && ((cells.nelements + paths.nelements) > 1))
	    status = AKLOG_SOMETHINGSWRONG;
    }

    /* If we are keeping track of zephyr subscriptions, print them. */
    if (zsubs) 
	for (cur_node = zsublist.first; cur_node; cur_node = cur_node->next) {
	    sprintf(msgbuf, "zsub: %s\n", cur_node->data);
	    params.pstdout(msgbuf);
	}

    /* If we are keeping track of host information, print it. */
    if (hosts)
	for (cur_node = hostlist.first; cur_node; cur_node = cur_node->next) {
	    sprintf(msgbuf, "host: %s\n", cur_node->data);
	    params.pstdout(msgbuf);
	}

    params.exitprog(status);
}

#ifndef HAVE_ADD_TO_ERROR_TABLE
#include <afs/error_table.h>

void add_error_table (const struct error_table *);

void
add_to_error_table(struct et_list *new_table)
{
	add_error_table(new_table->table);
}
#endif /* HAVE_ADD_TO_ERROR_TABLE */
