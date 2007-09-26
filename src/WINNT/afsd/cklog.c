/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <afs/kautils.h>
#include <afs/dirpath.h>
#include "cm_config.h"
#include "cmd.h"
#include <winsock2.h>

#define AFS_KERBEROS_ENV

#define BAD_ARGUMENT 1
#define KLOGEXIT(code) exit(code)

int CommandProc();

static int zero_argc;
static char **zero_argv;

void main (argc, argv)
  int   argc;
  char *argv[];
{   struct cmd_syndesc *ts;
    int code;
    WSADATA WSAjunk;

    zero_argc = argc;
    zero_argv = argv;

    /* Start up sockets */
    WSAStartup(0x0101, &WSAjunk);

    ts = cmd_CreateSyntax((char *) 0, CommandProc, 0, "obtain Kerberos authentication");

#define aXFLAG 0
#define aPRINCIPAL 1
#define aPASSWORD 2
#define aCELL 3
#define aSERVERS 4
#define aPIPE 5
#define aSILENT 6
#define aLIFETIME 7
#define aSETPAG 8
#define aTMP 9


    cmd_AddParm(ts, "-x", CMD_FLAG, CMD_OPTIONAL, "(obsolete, noop)");
    cmd_Seek(ts, aPRINCIPAL);
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL, "explicit list of servers");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL, "read password from stdin");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL, "ticket lifetime in hh[:mm[:ss]]");
    cmd_AddParm(ts, "-setpag", CMD_FLAG, CMD_OPTIONAL, "Create a new setpag before authenticating");
    cmd_AddParm(ts, "-tmp", CMD_FLAG, CMD_OPTIONAL, "write Kerberos-style ticket file in /tmp");

    code = cmd_Dispatch(argc, argv);
    KLOGEXIT(code);
}

static char *getpipepass() {
    static char gpbuf[BUFSIZ];
    /* read a password from stdin, stop on \n or eof */
    register int i, tc;
    memset(gpbuf, 0, sizeof(gpbuf));
    for(i=0; i<(sizeof(gpbuf)-1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF) break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}

/* good_gets is like gets except that it take a max string length and won't
 * write past the end of its input buffer.  It returns a variety of negative
 * numbers in case of errors and zero if there was no characters read (a blank
 * line for instance).  Otherwise it returns the length of the string read in.
 */

static int good_gets (s, max)
  char *s;
  int   max;
{   int l;				/* length of string read */
    if (!fgets (s, max, stdin)) {
	if (feof(stdin)) return EOF;	/* EOF on input, nothing read */
	else return -2;			/* I don't think this can happen */
    }
    l = (int)strlen (s);
    if (l && (s[l-1] == '\n')) s[--l] = 0;
    return l;
}

static int read_pw_string(char *s, int max)
{
    int ok = 0;
    HANDLE h;
    int md;

    /* set no echo */
    h = GetStdHandle (STD_INPUT_HANDLE);
    GetConsoleMode (h, &md);
    SetConsoleMode (h, md & ~ENABLE_ECHO_INPUT);

    while (!ok) {
	printf("Password:");
	fflush(stdout);
	if (good_gets(s, max) <= 0) {
	    printf("\n"); fflush(stdout);
	    if (feof (stdin)) break;	/* just give up */
	    else continue;		/* try again: blank line */
	}
	ok = 1;
    }

    if (!ok)
	memset(s, 0, max);

    /* reset echo */
    SetConsoleMode (h, md);
    printf("\n"); fflush(stdout);

    s[max-1] = 0;			/* force termination */
    return !ok;
}

CommandProc (as, arock)
  char *arock;
  struct cmd_syndesc *as;
{
    char  name[MAXKTCNAMELEN];
    char  instance[MAXKTCNAMELEN];
    char  cell[MAXKTCREALMLEN];
    char  defaultCell[256];
    char  realm[MAXKTCREALMLEN];
    int	  code;
    int   i, dosetpag;
    int   lifetime;			/* requested ticket lifetime */

    char passwd[BUFSIZ];

    static char	rn[] = "klog";		/*Routine name*/
    static int Pipe = 0;		/* reading from a pipe */
    static int Silent = 0;		/* Don't want error messages */

    int	foundPassword =	0;		/*Not yet, anyway*/
    int	foundExplicitCell = 0;		/*Not yet, anyway*/
    int writeTicketFile = 0;          /* write ticket file to /tmp */
    int password_expires = -1;

    char *reason;			/* string describing errors */

    name[0] = '\0';
    instance[0] = '\0';
    cell[0] = '\0';


    /* blow away command line arguments */
    for (i=1; i<zero_argc; i++) memset (zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);
    Pipe = (as->parms[aPIPE].items ? 1 : 0);

    /* Determine if we should also do a setpag based on -setpag switch */
    dosetpag = (as->parms[aSETPAG].items ? 1 : 0);

    if (as->parms[aTMP].items) {
       writeTicketFile = 1;
    }

    cm_GetRootCellName(defaultCell);
    ka_Init(0);

    /* Parse our arguments. */

    if (as->parms[aCELL].items) {
	/*
	 * cell name explicitly mentioned; take it in if no other cell name
	 * has already been specified and if the name actually appears.  If
	 * the given cell name differs from our own, we don't do a lookup.
	 */
	foundExplicitCell = 1;
	strncpy (realm, as->parms[aCELL].items->data, sizeof(realm));
    }

    if (as->parms[aSERVERS].items) {
	fprintf (stderr, "SERVERS option not available.\n");
    }

    if (as->parms[aPRINCIPAL].items) {
	ka_ParseLoginName (as->parms[aPRINCIPAL].items->data,
			   name, instance, cell);
	if (strlen (instance) > 0)
	    if (!Silent) {
		fprintf (stderr,
			 "Non-null instance (%s) may cause strange behavior.\n",
			 instance);
	    }
	if (strlen(cell) > 0) {
	    if (foundExplicitCell) {
		if (!Silent) {
		    fprintf (stderr,
			  "%s: May not specify an explicit cell twice.\n", rn);
		}
		return -1;
	    }
	    foundExplicitCell = 1;
	    strncpy (realm, cell, sizeof(realm));
	}
    } else {
	/* No explicit name provided. */
	DWORD size = GetEnvironmentVariable("USERNAME", name, sizeof(name) - 1);
	if (size <= 0 || size >= sizeof(name)) {
	    size = sizeof(name) - 1;
	    if (!GetUserName(name, &size)) {
		KLOGEXIT( BAD_ARGUMENT );
	    }
	}
    }

    if (as->parms[aPASSWORD].items) {
	/*
	 * Current argument is the desired password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	foundPassword = 1;
	strncpy (passwd, as->parms[aPASSWORD].items->data, sizeof(passwd));
	memset (as->parms[aPASSWORD].items->data, 0,
	       strlen(as->parms[aPASSWORD].items->data));
    }

    if (as->parms[aLIFETIME].items) {
	char *life = as->parms[aLIFETIME].items->data;
	char *sp;			/* string ptr to rest of life */
	lifetime = 3600*strtol (life, &sp, 0); /* hours */
	if (sp == life) {
bad_lifetime:
	    if (!Silent) fprintf (stderr, "%s: translating '%s' to lifetime failed\n",
			       rn, life);
	    return BAD_ARGUMENT;
	}
	if (*sp == ':') {
	    life = sp+1;		/* skip the colon */
	    lifetime += 60*strtol (life, &sp, 0); /* minutes */
	    if (sp == life) goto bad_lifetime;
	    if (*sp == ':') {
		life = sp+1;
		lifetime += strtol (life, &sp, 0); /* seconds */
		if (sp == life) goto bad_lifetime;
		if (*sp) goto bad_lifetime;
	    } else if (*sp) goto bad_lifetime;
	} else if (*sp) goto bad_lifetime;
	if (lifetime > MAXKTCTICKETLIFETIME) {
	    if (!Silent)
		fprintf (stderr,
		"%s: a lifetime of %.2f hours is too long, must be less than %d.\n",
		rn, (double)lifetime/3600.0, MAXKTCTICKETLIFETIME/3600);
	    KLOGEXIT( BAD_ARGUMENT );
	}
    } else lifetime = 0;

    if (!foundExplicitCell) strcpy (realm, defaultCell);

    /* Get the password if it wasn't provided. */
    if (!foundPassword) {
	if (Pipe) {
	    strncpy(passwd, getpipepass(), sizeof(passwd));
	}
	else {
	    if (read_pw_string(passwd, sizeof(passwd)))
		reason = "can't read password from terminal";
	    else if (strlen(passwd) == 0)
		reason = "zero length password is illegal";
	    else
		reason = NULL;
	    if (reason) {
		fprintf (stderr, "Unable to login because %s.\n", reason);
		KLOGEXIT( BAD_ARGUMENT );
	    }
	}
    }

    code = ka_UserAuthenticateGeneral (KA_USERAUTH_VERSION,
				       name, instance, realm, passwd, lifetime,
				       &password_expires, 0, &reason);
    memset (passwd, 0, sizeof(passwd));
    if (code) {
	if (!Silent) {
	    fprintf (stderr,
		     "Unable to authenticate to AFS because %s.\n", reason);
	  }
	KLOGEXIT( code );
      }

#ifndef AFS_KERBEROS_ENV
    if (writeTicketFile) {
       code = krb_write_ticket_file (realm);
       if (!Silent) {
          if (code) 
              afs_com_err (rn, code, "writing Kerberos ticket file");
          else fprintf (stderr, "Wrote ticket file to /tmp\n");
      }
   }
#endif
 
#ifdef DEBUGEXPIRES
       if (password_expires >= 0) {
	 printf ("password expires at %ld\n", password_expires);
       }
#endif /* DEBUGEXPIRES */

    return 0;
}
