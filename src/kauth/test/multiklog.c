/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* this is a version of klog which attempts to authenticate many times
 * in a loop.  It is a test program, and isn't really intended for general
 * use. 
 */

/* These two needed for rxgen output to work */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/kauth/test/multiklog.c,v 1.7.2.1 2007/04/10 18:43:43 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>

#include <lock.h>
#include <ubik.h>

#include <stdio.h>
#include <pwd.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "kauth.h"
#include "kautils.h"

/* Current Usage:
     mutliklog [principal [password]] [-t] [-c cellname] [-servers <hostlist>] [-number n]

     where:
       principal is of the form 'name' or 'name@cell' which provides the
	  cellname.  See the -c option below.
       password is the user's password.  This form is NOT recommended for
	  interactive users.
       -t advises klog to write a Kerberos style ticket file in /tmp.
       -c identifies cellname as the cell in which authentication is to take
	  place.
       -servers allows the explicit specification of the hosts providing
	  authentication services for the cell being used for authentication.
	  This is a debugging option and will disappear.
       -repeat is the number of times to iterate over the authentication
 */

int CommandProc();

static int zero_argc;
static char **zero_argv;

int
osi_audit()
{
/* this sucks but it works for now.
*/
    return 0;
}

main(argc, argv)
     int argc;
     char *argv[];
{
    struct cmd_syndesc *ts;
    long code;

    zero_argc = argc;
    zero_argv = argv;

    ts = cmd_CreateSyntax(NULL, CommandProc, 0,
			  "obtain Kerberos authentication");

#define aXFLAG 0
#define aPRINCIPAL 1
#define aPASSWORD 2
#define aTMP 3
#define aCELL 4
#define aSERVERS 5
#define aPIPE 6
#define aSILENT 7
#define aLIFETIME 8
#define aREPCOUNT 9

    cmd_AddParm(ts, "-x", CMD_FLAG, CMD_OPTIONAL, "(obsolete, noop)");
    cmd_Seek(ts, aPRINCIPAL);
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-tmp", CMD_FLAG, CMD_OPTIONAL,
		"write Kerberos-style ticket file in /tmp");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
		"explicit list of servers");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL,
		"read password from stdin");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL,
		"ticket lifetime in hh[:mm[:ss]]");
    cmd_AddParm(ts, "-repeat", CMD_SINGLE, CMD_OPTIONAL,
		"number of times to repeat authentication");

    code = cmd_Dispatch(argc, argv);
    exit(code);
}

extern struct passwd *getpwuid();

static char *
getpipepass()
{
    static char gpbuf[BUFSIZ];
    /* read a password from stdin, stop on \n or eof */
    register int i, tc;
    memset(gpbuf, 0, sizeof(gpbuf));
    for (i = 0; i < (sizeof(gpbuf) - 1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF)
	    break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}

CommandProc(as, arock)
     char *arock;
     struct cmd_syndesc *as;
{
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    char realm[MAXKTCREALMLEN];
    long serverList[MAXSERVERS];
    char *lcell;		/* local cellname */
    char lrealm[MAXKTCREALMLEN];	/* uppercase copy of local cellname */
    int code;
    int i;
    Date lifetime;		/* requested ticket lifetime */

    struct passwd pwent;
    struct passwd *pw = &pwent;
    struct passwd *lclpw = &pwent;
    char passwd[BUFSIZ];

    static char rn[] = "klog";	/*Routine name */
    static int Pipe = 0;	/* reading from a pipe */
    static int Silent = 0;	/* Don't want error messages */
    static int reps = 1;
    static long storecode = 0;	/* hold a non-zero error code, if any */

    int explicit;		/* servers specified explicitly */
    int local;			/* explicit cell is same a local one */
    int foundPassword = 0;	/*Not yet, anyway */
    int foundExplicitCell = 0;	/*Not yet, anyway */
    int writeTicketFile = 0;	/* write ticket file to /tmp */
    long password_expires = -1;

    char *reason;		/* string describing errors */

    /* blow away command line arguments */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);
    Pipe = (as->parms[aPIPE].items ? 1 : 0);

    code = ka_Init(0);
    if (code || !(lcell = ka_LocalCell())) {
      nocell:
	if (!Silent)
	    afs_com_err(rn, code, "Can't get local cell name!");
	exit(code);
    }
    if (code = ka_CellToRealm(lcell, lrealm, 0))
	goto nocell;

    strcpy(instance, "");

    /* Parse our arguments. */

    if (as->parms[aTMP].items) {
	writeTicketFile = 1;
    }

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
	    if (!Silent) {
		afs_com_err(rn, code, "could not parse server list");
	    }
	    return code;
	}
	explicit = 1;
    } else
	explicit = 0;

    if (as->parms[aPRINCIPAL].items) {
	ka_ParseLoginName(as->parms[aPRINCIPAL].items->data, name, instance,
			  cell);
	if (strlen(instance) > 0)
	    if (!Silent) {
		fprintf(stderr,
			"Non-null instance (%s) may cause strange behavior.\n",
			instance);
	    }
	if (strlen(cell) > 0) {
	    if (foundExplicitCell) {
		if (!Silent) {
		    fprintf(stderr,
			    "%s: May not specify an explicit cell twice.\n",
			    rn);
		}
		return -1;
	    }
	    foundExplicitCell = 1;
	    strncpy(realm, cell, sizeof(realm));
	}
	lclpw->pw_name = name;
    } else {
	/* No explicit name provided: use Unix uid. */
	pw = getpwuid(getuid());
	if (pw == 0) {
	    if (!Silent) {
		fprintf(stderr,
			"Can't figure out your name in local cell %s from your user id.\n",
			lcell);
		fprintf(stderr, "Try providing the user name.\n");
	    }
	    exit(1);
	}
	lclpw = pw;
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

    if (as->parms[aLIFETIME].items) {
	char *life = as->parms[aLIFETIME].items->data;
	char *sp;		/* string ptr to rest of life */
	lifetime = 3600 * strtol(life, &sp, 0);	/* hours */
	if (sp == life) {
	  bad_lifetime:
	    if (!Silent)
		fprintf(stderr, "%s: translating '%s' to lifetime\n", rn,
			life);
	    return -1;
	}
	if (*sp == ':') {
	    life = sp + 1;	/* skip the colon */
	    lifetime += 60 * strtol(life, &sp, 0);	/* minutes */
	    if (sp == life)
		goto bad_lifetime;
	    if (*sp == ':') {
		life = sp + 1;
		lifetime += strtol(life, &sp, 0);	/* seconds */
		if (sp == life)
		    goto bad_lifetime;
		if (*sp)
		    goto bad_lifetime;
	    } else if (*sp)
		goto bad_lifetime;
	} else if (*sp)
	    goto bad_lifetime;
	if (lifetime > MAXKTCTICKETLIFETIME) {
	    if (!Silent)
		fprintf(stderr,
			"%s: a lifetime of %.2f hours is too long, must be less than %d.\n",
			rn, (double)lifetime / 3600.0,
			MAXKTCTICKETLIFETIME / 3600);
	    exit(1);
	}
    } else
	lifetime = 0;

    if (as->parms[aREPCOUNT].items) {
	reps = atoi(as->parms[aREPCOUNT].items->data);
    }

    if (!foundExplicitCell)
	strcpy(realm, lcell);
    if (code = ka_CellToRealm(realm, realm, &local)) {
	if (!Silent)
	    afs_com_err(rn, code, "Can't convert cell to realm");
	exit(code);
    }

    /* Get the password if it wasn't provided. */
    if (!foundPassword) {
	if (Pipe) {
	    strncpy(passwd, getpipepass(), sizeof(passwd));
	} else {
	    if (ka_UserReadPassword
		("Password:", passwd, sizeof(passwd), &reason)) {
		fprintf(stderr, "Unable to login because %s.\n", reason);
		exit(1);
	    }
	}
    }

    if (explicit)
	ka_ExplicitCell(realm, serverList);

    /* we really want this to fail repeatedly, though we only check one return
     * code.  I hope it's representative...
     */
    for (i = 0; i < reps; i++) {
	code =
	    ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION, pw->pw_name,
				       instance, realm, passwd, lifetime,
				       &password_expires, 0, &reason);
	if (code)
	    storecode = code;
    }
    code = storecode;

    memset(passwd, 0, sizeof(passwd));
    if (code) {
	if (!Silent) {
	    fprintf(stderr, "Unable to authenticate to AFS because %s.\n",
		    reason);
	}
	exit(code);
    }

    if (writeTicketFile) {
	code = krb_write_ticket_file(realm);
	if (!Silent) {
	    if (code)
		afs_com_err(rn, code, "writing Kerberos ticket file");
	    else
		fprintf(stderr, "Wrote ticket file to /tmp\n");
	}
    }
#ifdef DEBUGEXPIRES
    if (password_expires >= 0) {
	printf("password expires at %ld\n", password_expires);
    }
#endif /* DEBUGEXPIRES */

    exit(0);
}
