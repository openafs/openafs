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

/*
 * test host realm stuff.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef USING_K5SSL
#include <sys/time.h>
#include "k5ssl.h"
#else
#include "krb5.h"
#endif

int exitrc;
int nflag;

krb5_context k5context;

void process(char *host)
{
    int code;
    char **realms = 0;
    int i;

    if (!k5context) {
	code = krb5_init_context(&k5context);
	if (code) {
	    fprintf (stderr, "krb5_init_context failed - %d\n", code);
	    exit(2);
	}
    }
    code = krb5_get_host_realm(k5context, host, &realms);
    if (code) {
	if (host)
	    fprintf(stderr, "Cannot get realms for host <%s> - %d\n", host, code);
	else
	    fprintf(stderr, "Cannot get realms for default host - %d\n", code);
	exitrc = 1;
    }
    if (realms) {
	if (host)
	    printf ("host <%s> realms", host);
	else
	    printf ("default host realms");
	for (i = 0; realms[i]; ++i) {
	    printf (" <%s>", realms[i]);
	}
	printf ("\n");
	fflush(stdout);
    }
    krb5_free_host_realm(k5context, realms);
}

int
main(int argc, char **argv)
{
    char *argp;
    char *progname = argv[0];

    while (--argc > 0) if (*(argp = *++argv) == '-')
    while (*++argp) switch(*argp) {
    case 'n':
	process(NULL);
	break;
    case '-':
	break;
    default:
    Usage:
	fprintf(stderr,"Usage: %s [-n] hosts\n", progname);
	exit(1);
    } else process(argp);

    if (k5context)
	krb5_free_context(k5context);
    exit(exitrc);
}
