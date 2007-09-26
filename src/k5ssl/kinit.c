/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

int k5k4;	/* -5 -4 */
char *Qflag;	/* -Q preauths */
char *lflag;	/* lifetime */
char *rflag;	/* renewable lifetime */
char *sflag;	/* start time */
char *Sflag;	/* service name */
char *tflag;	/* keytab */
char *cflag;	/* cache name */
int action;	/* -k -R -v */
int Vflag;	/* verbose */
int fflag;	/* forwardable */
int Fflag;	/* not forwardable */
int pflag;	/* proxiable */
int Pflag;	/* not proxiable */
int aflag;	/* addresses */
int Aflag;	/* no addresses */

krb5_context k5context;
#if USING_HEIMDAL
#define HM(h,m)	h
#else
#define HM(h,m)	m
#endif

process(char *name)
{
    krb5_ccache ccache = 0;
    int code;
    char *what;
    krb5_creds outcreds[1];
    char *p = 0;
    char *name_unparsed;
    krb5_enctype enctype = 0;
    krb5_principal client = 0;
    krb5_get_init_creds_opt gic_opts[1];
    krb5_address **localaddrs = 0;
    krb5_deltat deltat, stime;
    krb5_keytab keytab = 0;

    memset(outcreds, 0, sizeof *outcreds);
    stime = 0;
    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    krb5_get_init_creds_opt_init(gic_opts);
    if (fflag || Fflag)
	krb5_get_init_creds_opt_set_forwardable(gic_opts, fflag);
    if (pflag || Pflag)
	krb5_get_init_creds_opt_set_proxiable(gic_opts, pflag);
    if (aflag) {
	what = "krb5_os_localaddr";
	code = krb5_os_localaddr(k5context, &localaddrs);
	if (code)
	    goto Failed;
	
	krb5_get_init_creds_opt_set_address_list(gic_opts, localaddrs);
    } else if (Aflag) {
	krb5_get_init_creds_opt_set_address_list(gic_opts, NULL);
    }
    if (lflag) {
	what = "krb5_string_to_deltat";
	code = krb5_string_to_deltat(lflag, &deltat);
	if (code) goto Failed;
	krb5_get_init_creds_opt_set_tkt_life(gic_opts,	deltat);
    }
    if (rflag) {
	what = "krb5_string_to_deltat";
	code = krb5_string_to_deltat(rflag, &deltat);
	if (code) goto Failed;
	krb5_get_init_creds_opt_set_renew_life(gic_opts,  deltat);
    }
    if (sflag) {
	what = "krb5_string_to_deltat";
	code = krb5_string_to_deltat(sflag, &deltat);
	if (code) goto Failed;
	    /* XXX what about krb5_string_to_timestamp ??? */
	stime = deltat;
    }
    if (cflag) {
	what = "krb5_cc_resolve";
	if ((code = krb5_cc_resolve(k5context, cflag, &ccache)))
	    goto Failed;
    } else {
	what = "krb5_cc_default";
	if ((code = krb5_cc_default(k5context, &ccache)))
	    goto Failed;
    }
    if (name) {
	what = "krb5_parse_name";
	code = krb5_parse_name(k5context, name, &client);
	if (code) goto Failed;
#if 0
    } else if (action == 'k') {
	what = "krb5_sname_to_principal";
	if ((code = krb5_sname_to_principal(k5context, NULL,
		NULL, KRB5_NT_SRV_HST, &client)))
	    goto Failed;
#endif
    } else {
	what = "krb5_cc_get_principal";
	code = krb5_cc_get_principal(k5context, ccache, &client);
	if (code) {
	    struct passwd *pw;
	    what = "getlogin";
	    name = getlogin();
	    if (!name) {
		what = "getpwuid";
		pw = getpwuid((int)getuid());
		if (!pw) {
		    code = errno;
		    goto Failed;
		}
		name = pw->pw_name;
	    }
	    if (name) {
		what = "krb5_parse_name";
		code = krb5_parse_name(k5context, name, &client);
		if (code) goto Failed;
	    }
	}
    }
    switch(action) {
    case 0:
	what = "krb5_get_init_creds_password";
	code = krb5_get_init_creds_password(k5context,
	    outcreds, client, 0, krb5_prompter_posix,
	    0, stime,
	    Sflag, gic_opts);
	break;
    case 'k':
	what = "krb5_kt_resolve";
	code = krb5_kt_resolve(k5context, tflag, &keytab);
	if (code) goto Failed;
	what = "krb5_get_init_creds_keytab";
	code = krb5_get_init_creds_keytab(k5context, outcreds,
	    client, keytab, stime, Sflag, gic_opts);
	break;
    case 'r':
	what = "krb5_get_renewed_creds";
	code = krb5_get_renewed_creds(k5context, outcreds,
	    client, ccache, Sflag);
    case 'v':
	what = "krb5_get_validated_creds";
	code = krb5_get_validated_creds(k5context, outcreds,
	    client, ccache, Sflag);
	break;
    }
    if (code) goto Failed;
    what = "krb5_cc_initialize";
    code = krb5_cc_initialize(k5context, ccache, client);
    if (code) goto Failed;
    what = "krb5_cc_store_cred";
    code = krb5_cc_store_cred(k5context, ccache, outcreds);
    if (code) goto Failed;
    goto Done;
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, afs_error_message(code));
Done:
    if (outcreds->client == client)
	outcreds->client = 0;
    krb5_free_cred_contents(k5context, outcreds);
    krb5_free_addresses(k5context, localaddrs);
    if (ccache) krb5_cc_close(k5context, ccache);
    if (client) krb5_free_principal(k5context, client);
    if (keytab) krb5_kt_close(k5context, keytab);
    if (p) free(p);
    if (k5context) {
	krb5_free_context(k5context);
	k5context = 0;
    }
    return !!code;
}

int
main(int argc, char **argv)
{
    int f;
    char *argp;
    char *name;

    f = 0;
    name = 0;
    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case '5':
	k5k4 |= 1;
	break;
    case '4':
	fprintf(stderr,"No k4 support\n");
	goto Usage;
    case 'Q':	/* preauth */
	if (argc <= 1) {
	    fprintf(stderr,"-Q: missing argument\n");
	    goto Usage;
	}
	--argc;
	Qflag = *++argv;
	break;
    case 'l':	/* lifetime */
	if (argc <= 1) {
	    fprintf(stderr,"-l: missing argument\n");
	    goto Usage;
	}
	--argc;
	lflag = *++argv;
	break;
    case 'r':	/* renew */
	if (argc <= 1) {
	    fprintf(stderr,"-r: missing argument\n");
	    goto Usage;
	}
	--argc;
	rflag = *++argv;
	break;
    case 's':	/* start */
	if (argc <= 1) {
	    fprintf(stderr,"-s: missing argument\n");
	    goto Usage;
	}
	--argc;
	sflag = *++argv;
	break;
    case 'S':	/* service */
	if (argc <= 1) {
	    fprintf(stderr,"-S: missing argument\n");
	    goto Usage;
	}
	--argc;
	Sflag = *++argv;
	break;
    case 't':	/* keytab */
	if (argc <= 1) {
	    fprintf(stderr,"-t: missing argument\n");
	    goto Usage;
	}
	--argc;
	tflag = *++argv;
	break;
    case 'c':	/* cache */
	if (argc <= 1) {
	    fprintf(stderr,"-c: missing argument\n");
	    goto Usage;
	}
	--argc;
	cflag = *++argv;
	break;
    case 'V':	/* verbose */
	++Vflag;
	break;
    case 'f':	/* forwardable */
	++fflag;
	break;
    case 'F':	/* not forwardable */
	++Fflag;
	break;
    case 'p':	/* proxiable */
	++pflag;
	break;
    case 'P':	/* not proxiable */
	++Pflag;
	break;
    case 'a':	/* addresses */
	++aflag;
	break;
    case 'A':	/* no addresses */
	++Aflag;
	break;
    case 'k':
    case 'R':
    case 'v':
	action = *argp;
	break;
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr, "Usage: kinit [-5VfFpPaA] [|-k|-R|-v] [-c cache] [-t keytab] [-S service] [-s start] [-r renew] [-l lifetime] [-Q preauth] [principal]\n");
	exit(1);
    }
    else if (name) {
	fprintf(stderr,"More than one principal specified - %s\n", argp);
	goto Usage;
    }
    else name = argp;
    f = 0;
    if (fflag && Fflag) {
	fprintf(stderr,"incompatible options: -f and -F specified");
	f = 1;
    }
    if (pflag && Pflag) {
	fprintf(stderr,"incompatible options: -p and -P specified");
	f = 1;
    }
    if (aflag && Aflag) {
	fprintf(stderr,"incompatible options: -a and -A specified");
	f = 1;
    }
    if (f) goto Usage;

    exit(process(name));
}
