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

#include <afsconfig.h>
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

/* on AIX AFS trys to call the ntohl and htonl routines as routines
 * rather then macros. We need a real routine here.  We do this before
 * the ntohl and htonl macros are defined in net/in.h
* XXX is this still true?  If so should fix.
 */

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

#include <afs/stds.h>

#ifdef WINDOWS

#include <afs/auth.h>
#include <rx/rxkad.h>
#include <afs/dirpath.h>

#else /* !WINDOWS */
#ifndef HAVE_KERBEROSV_HEIM_ERR_H
#include <afs/com_err.h>
#endif

#include <afs/param.h>
#ifdef AFS_SUN5_ENV
#include <sys/ioccom.h>
#endif
#include <afs/auth.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>
#include <afs/dirpath.h>
#endif /* WINDOWS */

#include <afs/cellconfig.h>	/* XXX does windows have this? */
#ifdef AFS_RXK5
#include "rxk5_utilafs.h"
#else
#include <krb5.h>
#endif

#include "aklog.h"
#include "linked_list.h"

#define AFSKEY "afs"
#define AFS_K5_KEY "afs-k5"
#define AFSINST ""

#ifndef AFS_TRY_FULL_PRINC
#define AFS_TRY_FULL_PRINC 1
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
static krb5_ccache  _krb425_ccache = NULL;

#ifdef WINDOWS

/* libafsconf.dll */
extern long cm_GetRootCellName();
extern long cm_SearchCellFile();

static long cm_SearchCellFile_CallBack();

#else /* !WINDOWS */

/*
 * Why doesn't AFS provide these prototypes?
 */

extern int pioctl(char *, afs_int32, struct ViceIoctl *, afs_int32);

/*
 * Other prototypes
 */

extern char *afs_realm_of_cell(krb5_context, struct afsconf_cell *);
static int isdir(char *, unsigned char *);
static krb5_error_code get_credv5(krb5_context context, char *, char *,
				  char *, krb5_creds **);
static int get_user_realm(krb5_context, char *);

#if defined(HAVE_KRB5_PRINC_SIZE) || defined(krb5_princ_size)

#define get_princ_str(c, p, n) krb5_princ_component(c, p, n)->data
#define get_princ_len(c, p, n) krb5_princ_component(c, p, n)->length
#define second_comp(c, p) (krb5_princ_size(c, p) > 1)
#define realm_data(c, p) krb5_princ_realm(c, p)->data
#define realm_len(c, p) krb5_princ_realm(c, p)->length

#elif defined(HAVE_KRB5_PRINCIPAL_GET_COMP_STRING)

#define get_princ_str(c, p, n) krb5_principal_get_comp_string(c, p, n)
#define get_princ_len(c, p, n) strlen(krb5_principal_get_comp_string(c, p, n))
#define second_comp(c, p) (krb5_principal_get_comp_string(c, p, 1) != NULL)
#define realm_data(c, p) krb5_realm_data(krb5_principal_get_realm(c, p))
#define realm_len(c, p) krb5_realm_length(krb5_principal_get_realm(c, p))

#else
#error "Must have either krb5_princ_size or krb5_principal_get_comp_string"
#endif

#if defined(HAVE_KRB5_CREDS_KEYBLOCK)

#define get_cred_keydata(c) c->keyblock.contents
#define get_cred_keylen(c) c->keyblock.length
#define get_creds_enctype(c) c->keyblock.enctype

#elif defined(HAVE_KRB5_CREDS_SESSION)

#define get_cred_keydata(c) c->session.keyvalue.data
#define get_cred_keylen(c) c->session.keyvalue.length
#define get_creds_enctype(c) c->session.keytype

#else
#error "Must have either keyblock or session member of krb5_creds"
#endif

#if !defined(HAVE_KRB5_524_CONVERT_CREDS) && defined(HAVE_KRB524_CONVERT_CREDS_KDC)
#define krb5_524_convert_creds krb524_convert_creds_kdc
#elif !defined(HAVE_KRB5_524_CONVERT_CREDS) && !defined(HAVE_KRB524_CONVERT_CREDS_KDC)
#if 0
#error "You must have one of krb5_524_convert_creds or krb524_convert_creds_kdc available"
#endif
#endif

#endif /* WINDOWS */

/*
 * Provide a replacement for strerror if we don't have it
 */

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(x) sys_errlist[x]
#endif /* HAVE_STRERROR */

#define DO524_NO 1
#define DO524_YES 2
#define DO524_LOCAL 3

static char *progname = NULL;	/* Name of this program */
static int dflag = FALSE;	/* Give debugging information */
static int noauth = FALSE;	/* If true, don't try to get tokens */
static int zsubs = FALSE;	/* Are we keeping track of zephyr subs? */
static int hosts = FALSE;	/* Are we keeping track of hosts? */
static int noprdb = FALSE;	/* Skip resolving name to id? */
static int linked = FALSE;  /* try for both AFS nodes */
static int afssetpag = FALSE; /* setpag for AFS */
static int force = FALSE;	/* Bash identical tokens? */
static int do524 = DO524_NO;	/* Should we do 524 instead of rxkad2b? */
#ifdef AFS_RXK5
static int rxk5;		/* Use rxk5 enctype selection and settoken behavior */
#endif
static linked_list zsublist;	/* List of zephyr subscriptions */
static linked_list hostlist;	/* List of host addresses */
static linked_list authedcells;	/* List of cells already logged to */

/* ANL - CMU lifetime convert routine */
/* for K5.4.1 don't use this for now. Need to see if it is needed */
/* maybe needed in the krb524d module as well */
/* extern unsigned long krb_life_to_time(); */

static char *copy_cellinfo(cellinfo_t *cellinfo)
{
    cellinfo_t *new_cellinfo;

    if ((new_cellinfo = (cellinfo_t *)malloc(sizeof(cellinfo_t))))
	memcpy(new_cellinfo, cellinfo, sizeof(cellinfo_t));
    
    return ((char *)new_cellinfo);
}


static int get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char *local_cell, char *linkedcell)
{
    int status = AKLOG_SUCCESS;
    struct afsconf_dir *configdir;

    memset(local_cell, 0, sizeof(local_cell));
    memset((char *)cellconfig, 0, sizeof(*cellconfig));

#ifndef WINDOWS

    if (!(configdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	fprintf(stderr, 
		"%s: can't get afs configuration (afsconf_Open(%s))\n",
		progname, AFSDIR_CLIENT_ETC_DIRPATH);
	exit(AKLOG_AFS);
    }

    if (afsconf_GetLocalCell(configdir, local_cell, MAXCELLCHARS)) {
	fprintf(stderr, "%s: can't determine local cell.\n", progname);
	exit(AKLOG_AFS);
    }

    if ((cell == NULL) || (cell[0] == 0))
	cell = local_cell;

	linkedcell[0] = '\0';
    if (afsconf_GetCellInfo(configdir, cell, NULL, cellconfig)) {
	fprintf(stderr, "%s: Can't get information about cell %s.\n",
		progname, cell);
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
	fprintf(stderr, "%s: can't get local cellname\n", progname);
	exit(AKLOG_AFS);
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
    status = (int) cm_SearchCellFile(cell, NULL, &cm_SearchCellFile_CallBack,
				     cellconfig /* rock */);

    switch(status) {
    case 0:
	break;

    case -1:
	fprintf(stderr, "%s: GetWindowsDirectory() failed.\n", progname);
	break;

    case -2:
	fprintf(stderr, "%s: Couldn't open afsdcells.ini for reading\n",
		progname);
	break;

    case -3:
	fprintf(stderr, "%s: Couldn't find any servers for cell %s\n",
		progname, cell);
	break;

    case -4:
	fprintf(stderr, "%s: Badly formatted line in afsdcells.ini (does not begin with a \">\" or contain \"#\"\n",
		progname);
	break;

    default:
	fprintf(stderr, "%s cm_SearchCellFile returned unknown error %d\n",
		status);
    }

    if (status) {
	exit(AKLOG_AFS);
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
static int auth_to_cell(krb5_context context, char *cell, char *realm)
{
    int status = AKLOG_SUCCESS;
    char username[BUFSIZ];	/* To hold client username structure */
    afs_int32 viceId;		/* AFS uid of user */

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
	    printf("Already authenticated to %s (or tried to)\n", 
		   cell_to_use);
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
	fprintf(stderr, 
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, cell_to_use);
	exit(AKLOG_MISC);
    }
    if (ll_string(&zsublist, ll_s_add, local_cell) == LL_FAILURE) {
	fprintf(stderr, 
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, local_cell);
	exit(AKLOG_MISC);
    }

    if (!noauth) {
	if (dflag) {
	    printf("Authenticating to cell %s (server %s).\n",
		   cell_to_use, ak_cellconfig.hostName[0]);
	}

	/*
	 * Find out which realm we're supposed to authenticate to.  If one
	 * is not included, use the kerberos realm found in the credentials
	 * cache.
	 */

	if (realm && realm[0]) {
	    strcpy(realm_of_cell, realm);
	    if (dflag) {
		printf("We were told to authenticate to realm %s.\n", realm);
	    }
	}
	else {
	    char *realm = afs_realm_of_cell(context, &ak_cellconfig);

	    if (!realm) {
		fprintf(stderr, 
			"%s: Couldn't figure out realm for cell %s.\n",
			progname, cell_to_use);
		exit(AKLOG_MISC);
	    }

	    strcpy(realm_of_cell, realm);

	    if (dflag) {
		printf("We've deduced that we need to authenticate to"
		       " realm %s.\n", realm_of_cell);
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

#ifdef AFS_RXK5
	if(rxk5) {
	    strcpy(name, AFS_K5_KEY);
	} else {
#endif	/* AFS_RXK5 */
	    strcpy(name, AFSKEY);
#ifdef AFS_RXK5
	}
#endif

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

	if (dflag) {
	    printf("Getting tickets: %s/%s@%s\n", name,
		   primary_instance, realm_of_cell);
	}

	status = get_credv5(context, name, primary_instance, realm_of_cell,
			    &v5cred);

	if (status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN || status == KRB5KRB_ERR_GENERIC) {
	    if (try_secondary) {
		if (dflag) {
		    printf("Principal not found, trying alternate "
			   "service name: %s/%s@%s\n", name,
			    secondary_instance, realm_of_cell);
		}
		status = get_credv5(context, name, secondary_instance,
				    realm_of_cell, &v5cred);
	    }
	}

	if (status) {
	    if (dflag) {
		printf("Kerberos error code returned by get_cred: %d\n",
			status);
	    }
	    fprintf(stderr, "%s: Couldn't get %s AFS tickets:\n",
		    progname, cell_to_use);
		com_err(progname, status, "while getting AFS tickets");
	    return(AKLOG_KERBEROS);
	}

	strncpy(aserver.name, AFSKEY, MAXKTCNAMELEN - 1);
	strncpy(aserver.instance, AFSINST, MAXKTCNAMELEN - 1);
	strncpy(aserver.cell, cell_to_use, MAXKTCREALMLEN - 1);

	/*
 	 * The default is to use rxkad2b, which means we put in a full
	 * V5 ticket.  If the user specifies -524, we talk to the
	 * 524 ticket converter.  If the user specifies -unwrap, we
	 * construct a encpart only 2b style ticket.
	 */

#if defined(HAVE_KRB5_524_CONVERT_CREDS) || defined(HAVE_KRB524_CONVERT_CREDS_KDC)
	if (do524 != DO524_YES) {
#else
	{
#endif
	    char *p;
	    int len;

	    if (dflag)
	    	printf("Using Kerberos V5 ticket natively\n");

	    len = min(get_princ_len(context, v5cred->client, 0),
	    	      second_comp(context, v5cred->client) ?
					MAXKTCNAMELEN - 2 : MAXKTCNAMELEN - 1);
	    strncpy(username, get_princ_str(context, v5cred->client, 0), len);
	    username[len] = '\0';

	    if (second_comp(context, v5cred->client)) {
	    	strcat(username, ".");
		p = username + strlen(username);
		len = min(get_princ_len(context, v5cred->client, 1),
			  MAXKTCNAMELEN - strlen(username) - 1);
		strncpy(p, get_princ_str(context, v5cred->client, 1), len);
		p[len] = '\0';
	    }

	    memset(&atoken, 0, sizeof(atoken));
	    if (do524 == DO524_NO)
		atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
	    else
		atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY;
	    atoken.startTime = v5cred->times.starttime;;
	    atoken.endTime = v5cred->times.endtime;
	    memcpy(&atoken.sessionKey, get_cred_keydata(v5cred),
		   get_cred_keylen(v5cred));
	    if (do524 == DO524_NO) {
		atoken.ticketLen = v5cred->ticket.length;
		memcpy(atoken.ticket, v5cred->ticket.data, atoken.ticketLen);
	    } else {
		krb5_data enc_part[1];
		if (afs_krb5_skip_ticket_wrapper(v5cred->ticket.data,
			v5cred->ticket.length,
			&enc_part->data, &enc_part->length)) {
		    fprintf(stderr, "%s: Couldn't decode %s AFS tickets:\n",
			    progname, cell_to_use);
		    return(AKLOG_KERBEROS);
		}
		atoken.ticketLen = enc_part->length;
		memcpy(atoken.ticket, enc_part->data, atoken.ticketLen);
	    }
#if !defined(HAVE_KRB5_524_CONVERT_CREDS) && !defined(HAVE_KRB524_CONVERT_CREDS_KDC)
	}
#else
	} else {
    	    CREDENTIALS cred;

	    if (dflag)
	    	printf("Using Kerberos 524 translator service\n");

	    status = krb5_524_convert_creds(context, v5cred, &cred);

	    if (status) {
		com_err(progname, status, "while converting tickets "
			"to Kerberos V4 format");
		return(AKLOG_KERBEROS);
	    }

	    strcpy (username, cred.pname);
	    if (cred.pinst[0]) {
		strcat (username, ".");
		strcat (username, cred.pinst);
	    }

	    atoken.kvno = cred.kvno;
	    atoken.startTime = cred.issue_date;
	    /*
	     * It seems silly to go through a bunch of contortions to
	     * extract the expiration time, when the v5 credentials already
	     * has the exact time!  Let's use that instead.
	     *
	     * Note that this isn't a security hole, as the expiration time
	     * is also contained in the encrypted token
	     */
	    atoken.endTime = v5cred->times.endtime;
	    memcpy(&atoken.sessionKey, cred.session, 8);
	    atoken.ticketLen = cred.ticket_st.length;
	    memcpy(atoken.ticket, cred.ticket_st.dat, atoken.ticketLen);
	}
#endif
	
	if (!force &&
	    !ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient) &&
	    atoken.kvno == btoken.kvno &&
	    atoken.ticketLen == btoken.ticketLen &&
	    !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
	    !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) {

	    if (dflag) {
		printf("Identical tokens already exist; skipping.\n");
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
		printf("Not resolving name %s to id (-noprdb set)\n",
			username);
	    }
#ifndef WINDOWS
	}
	else {
	    if ((status = get_user_realm(context, realm_of_user))) {
		fprintf(stderr, "%s: Couldn't determine realm of user:)",
			progname);
		com_err(progname, status, " while getting realm");
		return(AKLOG_KERBEROS);
	    }
	    if (strcmp(realm_of_user, realm_of_cell)) {
		strcat(username, "@");
		strcat(username, realm_of_user);
	    }

	    if (dflag) {
		printf("About to resolve name %s to id in cell %s.\n", 
			username, aserver.cell);
	    }

	    strcpy(lastcell, aserver.cell);

	    if (!pr_Initialize (0, confname, aserver.cell))
		    status = pr_SNameToId (username, &viceId);
	    
	    if (dflag) {
		if (status) 
		    printf("Error %d\n", status);
		else
		    printf("Id %d\n", (int) viceId);
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
		    printf("doing first-time registration of %s "
			    "at %s\n", username, cell_to_use);
		}
		id = 0;
		strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
		strcpy(aclient.instance, "");
		strncpy(aclient.cell, realm_of_user, MAXKTCREALMLEN - 1);
		if ((status = ktc_SetToken(&aserver, &atoken, &aclient, 0))) {
		    fprintf(stderr, "%s: unable to obtain tokens for cell %s "
			    "(status: %d).\n", progname, cell_to_use, status);
		    status = AKLOG_TOKEN;
		}

		/*
		 * In case you're wondering, we don't need to change the
		 * filename here because we're still connecting to the
		 * same cell -- we're just using a different authentication
		 * level
		 */

		if ((status = pr_Initialize(1L, confname, aserver.cell))) {
		    printf("Error %d\n", status);
		}

		if ((status = pr_CreateUser(username, &id))) {
		    fprintf(stderr, "%s: %s so unable to create remote PTS "
			    "user %s in cell %s (status: %d).\n", progname,
			    error_message(status), username, cell_to_use,
			    status);
		} else {
		    printf("created cross-cell entry for %s at %s\n",
			   username, cell_to_use);
		    sprintf(username, "AFS ID %d", (int) id);
		}
	    }
	}
#endif /* ALLOW_REGISTER */

	}
#endif /* !WINDOWS */

	if (dflag) {
	    fprintf(stdout, "Set username to %s\n", username);
	}

	/* Reset the "aclient" structure before we call ktc_SetToken.
	 * This structure was first set by the ktc_GetToken call when
	 * we were comparing whether identical tokens already existed.
	 */
	strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
	strcpy(aclient.instance, "");
	strncpy(aclient.cell, realm_of_user, MAXKTCREALMLEN - 1);

	if (dflag) {
	    printf("Setting tokens. %s / %s @ %s \n",
		    aclient.name, aclient.instance, aclient.cell );
	}
	/* on AIX 4.1.4 with AFS 3.4a+ if a write is not done before 
	 * this routine, it will not add the token. It is not clear what 
	 * is going on here! So we will do the following operation
	 */
	write(2,"",0); /* dummy write */
#ifndef WINDOWS
#ifdef AFS_RXK5
	if(rxk5) {	
	  if ((status = ktc_SetK5Token(context, &aserver, v5cred, viceId, afssetpag))) {
	    	fprintf(stderr, 
		    "%s: unable to obtain tokens for cell %s (status: %d).\n",
		    progname, cell_to_use, status);
	    	status = AKLOG_TOKEN;
	    }
	} else {
#endif	/* AFS_RXK5 */
	    if ((status = ktc_SetToken(&aserver, &atoken, &aclient, afssetpag))) {
	    	fprintf(stderr, 
		    "%s: unable to obtain tokens for cell %s (status: %d).\n",
		    progname, cell_to_use, status);
	    	status = AKLOG_TOKEN;
	    }
#ifdef AFS_RXK5
	}
#endif	/* AFS_RXK5 */
#else /* WINDOWS */
	/* Note switched 2nd and 3rd args */
	if ((status = ktc_SetToken(&aserver, &atoken, &aclient, afssetpag))) {
	    switch(status) {
	    case KTC_INVAL:
		fprintf(stderr, "%s: Bad ticket length", progname);
		break;
	    case KTC_PIOCTLFAIL:
		fprintf(stderr, "%s: Unknown error contacting AFS service",
			progname);
		break;
	    case KTC_NOCELL:
		fprintf(stderr, "%s: Cell name (%s) not recognized by AFS service",
			progname, realm_of_cell);
		break;
	    case KTC_NOCM:
		fprintf(stderr, "%s: AFS service is unavailable", progname);
		break;
	    default:
		fprintf(stderr, "%s: Undocumented error (%d) contacting AFS service", progname, status);
		break;	
	    }
	    status = AKLOG_TOKEN;	    
	}
#endif /* !WINDOWS */
    }
    else
	if (dflag) {
	    printf("Noauth mode; not authenticating.\n");
	}
	
    return(status);
}

#ifndef WINDOWS /* struct ViceIoctl missing */

static int get_afs_mountpoint(char *file, char *mountpoint, int size)
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
static char *next_path(char *origpath)
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
	if (link = (readlink(pathtocheck, linkbuf, 
				    sizeof(linkbuf)) > 0)) {
	    if (++symlinkcount > MAXSYMLINKS) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ELOOP));
		exit(AKLOG_BADPATH);
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

static void add_hosts(char *file)
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
	printf("Getting list of hosts for %s\n", file);
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
		    printf("Got host %s\n", inet_ntoa(in));
		}
		ll_string(&hostlist, ll_s_add, (char *)inet_ntoa(in));
	    }
	    if (zsubs && (hp=gethostbyaddr((char *) &phosts[i],sizeof(long),AF_INET))) {
		if (dflag) {
		    printf("Got host %s\n", hp->h_name);
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
static int auth_to_path(krb5_context context, char *path)
{
    int status = AKLOG_SUCCESS;
    int auth_to_cell_status = AKLOG_SUCCESS;

    char *nextpath;
    char pathtocheck[MAXPATHLEN + 1];
    char mountpoint[MAXPATHLEN + 1];

    char *cell;
    char *endofcell;

    u_char isdirectory;

    /* Initialize */
    if (path[0] == DIR)
	strcpy(pathtocheck, path);
    else {
	if (getcwd(pathtocheck, sizeof(pathtocheck)) == NULL) {
	    fprintf(stderr, "Unable to find current working directory:\n");
	    fprintf(stderr, "%s\n", pathtocheck);
	    fprintf(stderr, "Try an absolute pathname.\n");
	    exit(AKLOG_BADPATH);
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
	    printf("Checking directory %s\n", pathtocheck);
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
	    if (isdir(pathtocheck, &isdirectory) < 0) {
		/*
		 * If we've logged and still can't stat, there's
		 * a problem... 
		 */
		fprintf(stderr, "%s: stat(%s): %s\n", progname, 
			pathtocheck, strerror(errno));
		return(AKLOG_BADPATH);
	    }
	    else if (! isdirectory) {
		/* Allow only directories */
		fprintf(stderr, "%s: %s: %s\n", progname, pathtocheck,
			strerror(ENOTDIR));
		return(AKLOG_BADPATH);
	    }
	}
    }
    

    return(status);
}

#endif /* WINDOWS */


/* Print usage message and exit */
static void usage(void)
{
    fprintf(stderr, "\nUsage: %s %s%s%s\n", progname,
	    "[-d] [[-cell | -c] cell [-k krb_realm]] ",
	    "[[-p | -path] pathname]\n",
	    "    [-zsubs] [-hosts] [-noauth] [-noprdb] [-force] [-setpag] \n"
	    "    [-linked]"
#if defined(HAVE_KRB5_524_CONVERT_CREDS) || defined(HAVE_KRB524_CONVERT_CREDS_KDC)
	    " [-524]"
#endif
#ifdef AFS_RXK5
	    " [-k5]"
	    " [-k4]"
#endif
	    "\n");
    fprintf(stderr, "    -d gives debugging information.\n");
    fprintf(stderr, "    krb_realm is the kerberos realm of a cell.\n");
    fprintf(stderr, "    pathname is the name of a directory to which ");
    fprintf(stderr, "you wish to authenticate.\n");
    fprintf(stderr, "    -zsubs gives zephyr subscription information.\n");
    fprintf(stderr, "    -hosts gives host address information.\n");
    fprintf(stderr, "    -noauth does not attempt to get tokens.\n");
    fprintf(stderr, "    -noprdb means don't try to determine AFS ID.\n");
    fprintf(stderr, "    -force means replace identical tickets. \n");
    fprintf(stderr, "    -linked means if AFS node is linked, try both. \n");
    fprintf(stderr, "    -setpag set the AFS process authentication group.\n");
#if defined(HAVE_KRB5_524_CONVERT_CREDS) || defined(HAVE_KRB524_CONVERT_CREDS_KDC)
    fprintf(stderr, "    -524 means use the 524 converter instead of V5 directly\n");
#endif
    fprintf(stderr, "    -unwrap means do the 524 conversion locally\n");
#ifdef AFS_RXK5
    fprintf(stderr, "    -k5 means do rxk5 (kernel uses V5 tickets)\n");
    fprintf(stderr, "    -k4 means do rxkad (kernel uses V4 or 2b tickets)\n");
#endif	/* AFS_RXK5 */
    fprintf(stderr, "    No commandline arguments means ");
    fprintf(stderr, "authenticate to the local cell.\n");
    fprintf(stderr, "\n");
    exit(AKLOG_USAGE);
}

void aklog(int argc, char *argv[])
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

#ifdef AFS_RXK5
     /* Select for rxk5 unless AFS_RXK5_DEFAULT envvar is not 1|yes */
    rxk5 = env_afs_rxk5_default() != FORCE_RXKAD;
#endif

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
#if defined(HAVE_KRB5_524_CONVERT_CREDS) || defined(HAVE_KRB524_CONVERT_CREDS_KDC)
	else if (strcmp(argv[i], "-524") == 0)
	    do524 = DO524_YES;
#endif
	else if (strcmp(argv[i], "-unwrap") == 0)
	    do524 = DO524_LOCAL;
#ifdef AFS_RXK5
	else if (strcmp(argv[i], "-k4") == 0)
	    rxk5 = 0;
	else if (strcmp(argv[i], "-k5") == 0)
	    rxk5 = 1;
#endif	/* AFS_RXK5 */
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
	    fprintf(stderr, "%s: path mode not supported.\n", progname);
	    exit(AKLOG_MISC);
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
		fprintf(stderr, "%s: path mode not supported.\n", progname);
		exit(AKLOG_MISC);
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
		    fprintf(stderr, 
			    "%s: failure copying cellinfo.\n", progname);
		    exit(AKLOG_MISC);
		}
	    }
	    else {
		fprintf(stderr, "%s: failure adding cell to cells list.\n",
			progname);
		exit(AKLOG_MISC);
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
		if ((new_path = strdup(path)))
		    ll_add_data(cur_node, new_path);
		else {
		    fprintf(stderr, "%s: failure copying path name.\n",
			    progname);
		    exit(AKLOG_MISC);
		}
	    }
	    else {
		fprintf(stderr, "%s: failure adding path to paths list.\n",
			progname);
		exit(AKLOG_MISC);
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
			        printf("Linked cell: %s\n", linkedcell);
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
			    printf("Reading %s for cells to "
				    "authenticate to.\n", xlog_path);
			}

			while (fgets(fcell, 100, f) != NULL) {
			    int auth_status;

			    fcell[strlen(fcell) - 1] = '\0';

			    if (dflag) {
				printf("Found cell %s in %s.\n",
					fcell, xlog_path);
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
                    printf("Linked cell: %s\n",
                        linkedcell);
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
	    printf("zsub: %s\n", cur_node->data);
	}

    /* If we are keeping track of host information, print it. */
    if (hosts)
	for (cur_node = hostlist.first; cur_node; cur_node = cur_node->next) {
	    printf("host: %s\n", cur_node->data);
	}

    exit(status);
}

static int isdir(char *path, unsigned char *val)
{
    struct stat statbuf;

    if (lstat(path, &statbuf) < 0)
	return (-1);
    else {
	if ((statbuf.st_mode & S_IFMT) == S_IFDIR) 
	    *val = TRUE;
	else
	    *val = FALSE;
	return (0);
    }  
}

static krb5_error_code get_credv5(krb5_context context, 
			char *name, char *inst, char *realm,
			krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));
/* ANL - instance may be ptr to a null string. Pass null then */
    if ((r = krb5_build_principal(context, &increds.server,
                     strlen(realm), realm,
                     name,
           (inst && strlen(inst)) ? inst : (void *) NULL,
                     (void *) NULL))) {
        return r;
    }

    if (!_krb425_ccache) {
        r = krb5_cc_default(context, &_krb425_ccache);
	if (r)
	    return r;
    }
    if (!client_principal) {
        r = krb5_cc_get_principal(context, _krb425_ccache, &client_principal);
	if (r)
	    return r;
    }

    increds.client = client_principal;
    increds.times.endtime = 0;
    
#ifdef AFS_RXK5
    if(rxk5) {
    	/* Get the strongest credentials this KDC can issue for the princ, and the
	   cache manager supports */
	   
	/* Todo: add pioctl GetCapabilities call to fetch the cache-manager supported
	   enctypes at runtime (skipping this for now, because we know which enctypes
	   K5SSL supports */
	   int enc_ix;
	   int enctypes_pref_order[6] = { ENCTYPE_AES256_CTS_HMAC_SHA1_96,
	   				  ENCTYPE_AES128_CTS_HMAC_SHA1_96,
					  ENCTYPE_DES3_CBC_SHA1,
#ifndef USING_HEIMDAL
#define ENCTYPE_ARCFOUR_HMAC_MD5 ENCTYPE_ARCFOUR_HMAC
#define ENCTYPE_ARCFOUR_HMAC_MD5_56 ENCTYPE_ARCFOUR_HMAC_EXP
#endif
					  ENCTYPE_ARCFOUR_HMAC_MD5,
					  ENCTYPE_ARCFOUR_HMAC_MD5_56,
					  ENCTYPE_DES_CBC_CRC };
					  
	    for(enc_ix = 0; enc_ix < 6; ++enc_ix) {
	    	get_creds_enctype((&increds)) = enctypes_pref_order[enc_ix];
		/* odd name for the ccache var, but apparently, just the usual one */
		r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);
		if(!r) {
		    if(dflag) {
		        printf("Successful get_greds_enctype with enctype == %d\n", 
				enctypes_pref_order[enc_ix]);
		    }
		    break;
		}
	    }				  
	   
    } else {
#endif	/* AFS_RXK5 */
    	/* Ask for DES since that is what V4 understands */
    	get_creds_enctype((&increds)) = ENCTYPE_DES_CBC_CRC;
    	r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);
#ifdef AFS_RXK5
    }
#endif	/* AFS_RXK5 */

    return r;
}


static int get_user_realm(krb5_context context, char *realm)
{
    static krb5_principal client_principal = 0;
    int i;

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    i = realm_len(context, client_principal);
    if (i > REALM_SZ-1) i = REALM_SZ-1;
    strncpy(realm,realm_data(context, client_principal), i);
    realm[i] = 0;

    return(0);
}
