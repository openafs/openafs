/* test-reauth.c - test SIA reauthorization code. */
/* Copyright (C) 1991, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1988, 1989
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
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


char *sia_code_string(int code)
{
    static char err_string[64];

    switch(code) {
    case SIADSUCCESS:
	return "SIADSUCCESS";
    case SIAFAIL:
	return "SIAFAIL";
    case SIASTOP:
	return "SIASTOP";
    default:
	(void) sprintf(err_string, "Unknown error %d\n", code);
	return err_string;
    }
}

main(int ac, char **av)
{
    char *username;
    SIAENTITY *entity=NULL;
    int (*sia_collect)() = sia_collect_trm;
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
	printf("sia_ses_reauthent failed with code %s\n", sia_code_string(code));
	sia_ses_release(&entity);
	exit(1);
    }
    
    code = sia_ses_release(&entity);
    if (code != SIASUCCESS) {
	printf("sia_ses_release failed with code %s\n", sia_code_string(code));
	exit(1);
    }

    printf("Password verified.\n");

    exit(0);

}
    
