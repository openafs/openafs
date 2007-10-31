/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* These two needed for rxgen output to work */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <rx/xdr.h>

#include <lock.h>
#include <ubik.h>

#include <stdio.h>
#ifndef AFS_NT40_ENV
#include <pwd.h>
#endif
#include <string.h>
#include <signal.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "kauth.h"
#include "kautils.h"
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif
#include <limits.h>


/* This code borrowed heavily from the log program.  Here is the intro comment
 * for that program: */

/*
	log -- tell the Andrew Cache Manager your password
	5 June 1985
	modified February 1986

	Further modified in August 1987 to understand cell IDs.
 */

/* Current Usage:
     kpasswd [user [password] [newpassword]] [-c cellname] [-servers <hostlist>]

     where:
       principal is of the form 'name' or 'name@cell' which provides the
	  cellname.  See the -c option below.
       password is the user's password.  This form is NOT recommended for
	  interactive users.
       newpassword is the new password.  This form is NOT recommended for
	  interactive users.
       -c identifies cellname as the cell in which authentication is to take
	  place.
       -servers allows the explicit specification of the hosts providing
	  authentication services for the cell being used for authentication.
	  This is a debugging option and will disappear.
 */

/* The following code to make use of libcmd.a also stolen from klog.c. */

int CommandProc(struct cmd_syndesc *, void *);

static int zero_argc;
static char **zero_argv;
extern int init_child(), give_to_child(), terminate_child();

#ifdef AFS_NT40_ENV
struct passwd {
    char *pw_name;
};
char userName[128];
DWORD userNameLen;
#endif

main(argc, argv, envp)
     int argc;
     char *argv[];
     char **envp;
{
    struct cmd_syndesc *ts;
    afs_int32 code;

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
#endif

    zero_argc = argc;
    zero_argv = argv;

    init_child(*argv);
    ts = cmd_CreateSyntax(NULL, CommandProc, 0, "change user's password");

#define aXFLAG 0
#define aPRINCIPAL 1
#define aPASSWORD 2
#define aNEWPASSWORD 3
#define aCELL 4
#define aSERVERS 5
#define aPIPE 6

    cmd_AddParm(ts, "-x", CMD_FLAG, CMD_OPTIONAL, "(obsolete, noop)");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-newpassword", CMD_SINGLE, CMD_OPTIONAL,
		"user's new password");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
		"explicit list of servers");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL, "silent operation");

    code = cmd_Dispatch(argc, argv);
    exit(code != 0);
}


static void
getpipepass(gpbuf, len)
     char *gpbuf;
     int len;
{
    /* read a password from stdin, stop on \n or eof */
    register int i, tc;
    memset(gpbuf, 0, len);
    for (i = 0; i < len; i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF)
	    break;
	gpbuf[i] = tc;
    }
    return;
}

static afs_int32
read_pass(passwd, len, prompt, verify)
     char *passwd;
     int len;
     char *prompt;
     int verify;
{
    afs_int32 code;
    code = read_pw_string(passwd, len, prompt, verify);
    if (code == -1) {
	getpipepass(passwd, len);
	return 0;
    }
    return code;
}

static int
password_ok(newpw, insist)
     char *newpw;
     int *insist;
{
    if (insist == 0) {
	/* see if it is reasonable, but don't get so obnoxious */
	/* FIXME: null pointer derefence!!! */
	(*insist)++;		/* so we don't get called again */
	if (strlen(newpw) < 6)
	    return 0;
    }
    return 1;			/* lie about it */
}

static char rn[] = "kpasswd";	/* Routine name */
static int Pipe = 0;		/* reading from a pipe */

#if TIMEOUT
int
timedout()
{
    if (!Pipe)
	fprintf(stderr, "%s: timed out\n", rn);
    exit(1);
}
#endif

char passwd[BUFSIZ], npasswd[BUFSIZ], verify[BUFSIZ];
CommandProc(struct cmd_syndesc *as, void *arock)
{
    char name[MAXKTCNAMELEN] = "";
    char instance[MAXKTCNAMELEN] = "";
    char cell[MAXKTCREALMLEN] = "";
    char realm[MAXKTCREALMLEN] = "";
    afs_int32 serverList[MAXSERVERS];
    char *lcell;		/* local cellname */
    int code;
    int i;

    struct ubik_client *conn = 0;
    struct ktc_encryptionKey key;
    struct ktc_encryptionKey mitkey;
    struct ktc_encryptionKey newkey;
    struct ktc_encryptionKey newmitkey;

    struct ktc_token token;

    struct passwd pwent;
    struct passwd *pw = &pwent;

    int insist;			/* insist on good password quality */
    int lexplicit = 0;		/* servers specified explicitly */
    int local;			/* explicit cell is same a local cell */
    int foundPassword = 0;	/*Not yet, anyway */
    int foundNewPassword = 0;	/*Not yet, anyway */
    int foundExplicitCell = 0;	/*Not yet, anyway */
#ifdef DEFAULT_MITV4_STRINGTOKEY
    int dess2k = 1;
#elif DEFAULT_AFS_STRINGTOKEY
    int dess2k = 0;
#else
    int dess2k = -1;
#endif

    /* blow away command line arguments */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -pipe switch */
    Pipe = (as->parms[aPIPE].items ? 1 : 0);

#if TIMEOUT
    signal(SIGALRM, timedout);
    alarm(30);
#endif

    code = ka_Init(0);
    if (code || !(lcell = ka_LocalCell())) {
#ifndef AFS_FREELANCE_CLIENT
	if (!Pipe)
	    afs_com_err(rn, code, "Can't get local cell name!");
	exit(1);
#endif
    }

    code = rx_Init(0);
    if (code) {
	if (!Pipe)
	    afs_com_err(rn, code, "Failed to initialize Rx");
	exit(1);
    }

    strcpy(instance, "");

    /* Parse our arguments. */

    if (as->parms[aCELL].items) {
	/*
	 * cell name explicitly mentioned; take it in if no other cell name
	 * has already been specified and if the name actually appears.  If
	 * the given cell name differs from our own, we don't do a lookup.
	 */
	foundExplicitCell = 1;
	strncpy(realm, as->parms[aCELL].items->data, sizeof(realm));
    }

    if (as->parms[aSERVERS].items) {
	/* explicit server list */
	int i;
	struct cmd_item *ip;
	char *ap[MAXSERVERS + 2];

	for (ip = as->parms[aSERVERS].items, i = 2; ip; ip = ip->next, i++)
	    ap[i] = ip->data;
	ap[0] = "";
	ap[1] = "-servers";
	code = ubik_ParseClientList(i, ap, serverList);
	if (code) {
	    if (!Pipe)
		afs_com_err(rn, code, "could not parse server list");
	    return code;
	}
	lexplicit = 1;
    }

    if (as->parms[aPRINCIPAL].items) {
	ka_ParseLoginName(as->parms[aPRINCIPAL].items->data, name, instance,
			  cell);
	if (strlen(instance) > 0)
	    if (!Pipe)
		fprintf(stderr,
			"Non-null instance (%s) may cause strange behavior.\n",
			instance);
	if (strlen(cell) > 0) {
	    if (foundExplicitCell) {
		if (!Pipe)
		    fprintf(stderr,
			    "%s: May not specify an explicit cell twice.\n",
			    rn);
		return -1;
	    }
	    foundExplicitCell = 1;
	    strncpy(realm, cell, sizeof(realm));
	}
	pw->pw_name = name;
    } else {
	/* No explicit name provided: use Unix uid. */
#ifdef AFS_NT40_ENV
	userNameLen = 128;
	if (GetUserName(userName, &userNameLen) == 0) {
	    if (!Pipe) {
		fprintf(stderr,
			"Can't figure out your name in local cell %s from your user id.\n",
			lcell);
		fprintf(stderr, "Try providing the user name.\n");
	    }
	    exit(1);
	}
	pw->pw_name = userName;
#else
	pw = getpwuid(getuid());
	if (pw == 0) {
	    if (!Pipe) {
		fprintf(stderr,
			"Can't figure out your name in local cell %s from your user id.\n",
			lcell);
		fprintf(stderr, "Try providing the user name.\n");
	    }
	    exit(1);
	}
#endif
    }

    if (as->parms[aPASSWORD].items) {
	/*
	 * Current argument is the desired password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	foundPassword = 1;
	strncpy(passwd, as->parms[aPASSWORD].items->data, sizeof(passwd));
	memset(as->parms[aPASSWORD].items->data, 0,
	       strlen(as->parms[aPASSWORD].items->data));
    }

    if (as->parms[aNEWPASSWORD].items) {
	/*
	 * Current argument is the new password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	foundNewPassword = 1;
	strncpy(npasswd, as->parms[aNEWPASSWORD].items->data,
		sizeof(npasswd));
	memset(as->parms[aNEWPASSWORD].items->data, 0,
	       strlen(as->parms[aNEWPASSWORD].items->data));
    }
#ifdef AFS_FREELANCE_CLIENT
    if (!foundExplicitCell && !lcell) {
	if (!Pipe)
	    afs_com_err(rn, code, "no cell name provided");
	exit(1);
    }
#else
    if (!foundExplicitCell)
	strcpy(realm, lcell);
#endif /* freelance */

    if (code = ka_CellToRealm(realm, realm, &local)) {
	if (!Pipe)
	    afs_com_err(rn, code, "Can't convert cell to realm");
	exit(1);
    }
    lcstring(cell, realm, sizeof(cell));

    ka_PrintUserID("Changing password for '", pw->pw_name, instance, "'");
    printf(" in cell '%s'.\n", cell);

    /* Get the password if it wasn't provided. */
    if (!foundPassword) {
	if (Pipe)
	    getpipepass(passwd, sizeof(passwd));
	else {
	    code = read_pass(passwd, sizeof(passwd), "Old password: ", 0);
	    if (code || (strlen(passwd) == 0)) {
		if (code)
		    code = KAREADPW;
		memset(&mitkey, 0, sizeof(mitkey));
		memset(&key, 0, sizeof(key));
		memset(passwd, 0, sizeof(passwd));
		if (code)
		    afs_com_err(rn, code, "reading password");
		exit(1);
	    }
	}
    }
    ka_StringToKey(passwd, realm, &key);
    des_string_to_key(passwd, &mitkey);
    give_to_child(passwd);

    /* Get new password if it wasn't provided. */
    insist = 0;
    if (!foundNewPassword) {
	if (Pipe)
	    getpipepass(npasswd, sizeof(npasswd));
	else {
	    do {
		code =
		    read_pass(npasswd, sizeof(npasswd),
			      "New password (RETURN to abort): ", 0);
		if (code || (strlen(npasswd) == 0)) {
		    if (code)
			code = KAREADPW;
		    goto no_change;

		}
	    } while (password_bad(npasswd));

	    code =
		read_pass(verify, sizeof(verify), "Retype new password: ", 0);
	    if (code) {
		code = KAREADPW;
		goto no_change;
	    }
	    if (strcmp(verify, npasswd) != 0) {
		printf("Mismatch - ");
		goto no_change;
	    }
	    memset(verify, 0, sizeof(verify));
	}
    }
    if (code = password_bad(npasswd)) {	/* assmt here! */
	goto no_change_no_msg;
    }
#if TRUNCATEPASSWORD
    if (strlen(npasswd) > 8) {
	npasswd[8] = 0;
	fprintf(stderr,
		"%s: password too long, only the first 8 chars will be used.\n",
		rn);
    } else
	npasswd[8] = 0;		/* in case the password was exactly 8 chars long */
#endif
    ka_StringToKey(npasswd, realm, &newkey);
    des_string_to_key(npasswd, &newmitkey);
    memset(npasswd, 0, sizeof(npasswd));

    if (lexplicit)
	ka_ExplicitCell(realm, serverList);

    /* Get an connection to kaserver's admin service in desired cell.  Set the
     * lifetime above the time uncertainty so that badly skewed clocks are
     * reported when the ticket is decrypted.  Then give us 10 seconds to
     * actually get our work done if the clocks are skewed by only 14:59.
     * NOTE: Kerberos lifetime encoding will round this up to next 5 minute
     * interval, namely 20 minutes. */

#define ADMIN_LIFETIME (KTC_TIME_UNCERTAINTY+1)

    code =
	ka_GetAdminToken(pw->pw_name, instance, realm, &key, ADMIN_LIFETIME,
			 &token, /*!new */ 0);
    if (code == KABADREQUEST) {
	code =
	    ka_GetAdminToken(pw->pw_name, instance, realm, &mitkey,
			     ADMIN_LIFETIME, &token, /*!new */ 0);
	if ((code == KABADREQUEST) && (strlen(passwd) > 8)) {
	    /* try with only the first 8 characters incase they set their password
	     * with an old style passwd program. */
	    char pass8[9];
	    strncpy(pass8, passwd, 8);
	    pass8[8] = 0;
	    ka_StringToKey(pass8, realm, &key);
	    memset(pass8, 0, sizeof(pass8));
	    memset(passwd, 0, sizeof(passwd));
	    code = ka_GetAdminToken(pw->pw_name, instance, realm, &key, ADMIN_LIFETIME, &token,	/*!new */
				    0);
#ifdef notdef
	    /* the folks in testing really *hate* this message */
	    if (code == 0) {
		fprintf(stderr,
			"Warning: only the first 8 characters of your old password were significant.\n");
	    }
#endif
	    if (code == 0) {
		if (dess2k == -1)
		    dess2k = 0;
	    }
	} else {
	    if (dess2k == -1)
		dess2k = 1;
	}
    } else {
	if (dess2k == -1)
	    dess2k = 0;
    }

    memset(&mitkey, 0, sizeof(mitkey));
    memset(&key, 0, sizeof(key));
    if (code == KAUBIKCALL)
	afs_com_err(rn, code, "(Authentication Server unavailable, try later)");
    else if (code) {
	if (code == KABADREQUEST)
	    fprintf(stderr, "%s: Incorrect old password.\n", rn);
	else
	    afs_com_err(rn, code, "so couldn't change password");
    } else {
	code =
	    ka_AuthServerConn(realm, KA_MAINTENANCE_SERVICE, &token, &conn);
	if (code)
	    afs_com_err(rn, code, "contacting Admin Server");
	else {
	    if (dess2k == 1)
		code =
		    ka_ChangePassword(pw->pw_name, instance, conn, 0,
				      &newmitkey);
	    else
		code =
		    ka_ChangePassword(pw->pw_name, instance, conn, 0,
				      &newkey);
	    memset(&newkey, 0, sizeof(newkey));
	    memset(&newmitkey, 0, sizeof(newmitkey));
	    if (code) {
		char *reason;
		reason = (char *)afs_error_message(code);
		fprintf(stderr, "%s: Password was not changed because %s\n",
			rn, reason);
	    } else
		printf("Password changed.\n\n");
	}
    }
    memset(&newkey, 0, sizeof(newkey));
    memset(&newmitkey, 0, sizeof(newmitkey));

    /* Might need to close down the ubik_Client connection */
    if (conn) {
	ubik_ClientDestroy(conn);
	conn = 0;
    }
    rx_Finalize();
    terminate_child();
    exit(code);

  no_change:			/* yuck, yuck, yuck */
    if (code)
	afs_com_err(rn, code, "getting new password");
  no_change_no_msg:
    memset(&key, 0, sizeof(key));
    memset(npasswd, 0, sizeof(npasswd));
    printf("Password for '%s' in cell '%s' unchanged.\n\n", pw->pw_name,
	   cell);
    terminate_child();
    exit(code ? code : 1);
}
