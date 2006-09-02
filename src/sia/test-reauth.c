/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* test-reauth.c - test SIA reauthorization code. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/sia/Attic/test-reauth.c,v 1.5 2003/07/15 23:16:52 shadow Exp $");

#include <afs/stds.h>
#include <stdio.h>
#include <sgtty.h>
#include <utmp.h>
#include <signal.h>
#include <errno.h>
#include <ttyent.h>
#include <syslog.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdio.h>
#include <strings.h>
#include <lastlog.h>
#include <paths.h>

#include <sia.h>
#include <siad.h>


char *
sia_code_string(int code)
{
    static char err_string[64];

    switch (code) {
    case SIADSUCCESS:
	return "SIADSUCCESS";
    case SIAFAIL:
	return "SIAFAIL";
    case SIASTOP:
	return "SIASTOP";
    default:
	(void)sprintf(err_string, "Unknown error %d\n", code);
	return err_string;
    }
}

main(int ac, char **av)
{
    char *username;
    SIAENTITY *entity = NULL;
    int (*sia_collect) () = sia_collect_trm;
    int code;


    if (ac != 2) {
	printf("Usage: test-reauth user-name\n");
	exit(1);
    }
    username = av[1];

    code = sia_ses_init(&entity, ac, av, NULL, username, NULL, 1, NULL);
    if (code != SIASUCCESS) {
	printf("sia_ses_init failed with code %s\n", sia_code_string(code));
	sia_ses_release(&entity);
	exit(1);
    }

    code = sia_ses_reauthent(sia_collect, entity);
    if (code != SIASUCCESS) {
	printf("sia_ses_reauthent failed with code %s\n",
	       sia_code_string(code));
	sia_ses_release(&entity);
	exit(1);
    }

    code = sia_ses_release(&entity);
    if (code != SIASUCCESS) {
	printf("sia_ses_release failed with code %s\n",
	       sia_code_string(code));
	exit(1);
    }

    printf("Password verified.\n");

    exit(0);

}
