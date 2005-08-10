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

RCSID
    ("$Header: /cvs/openafs/src/log/tokens.c,v 1.6.2.1 2005/07/11 19:46:16 shadow Exp $");

#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/file.h>
#include <rx/xdr.h>
#include <errno.h>
#include <sys/types.h>
#include <afs/auth.h>
#include <time.h>		/*time(), ctime() */
#include <pwd.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#define VIRTUE
#define VICE

#ifdef CMUWP_ENV
#include <afs/afsutil.h>	/*getv*(), getc*() routine family */
#endif /* CMUWP_ENV */

#undef VIRTUE
#undef VICE

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{				/*Main program */
    int cellNum;		/*Cell entry number */
    int rc;			/*Return value from U_CellGetLocalTokens */
    time_t current_time;	/*Current time of day */
    time_t tokenExpireTime;	/*When token expires */
    char *expireString;		/*Char string of expiration time */
    char UserName[16];		/*Printable user name */
    struct ktc_principal serviceName, clientName;	/* service name for ticket */
    struct ktc_token token;	/* the token we're printing */

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

    /* has no args ... support for help flag */

    if (argc > 1) {
	/* syntax from AFS Com Ref Man p9-39 */

	printf("Usage: tokens [-help]\n");
	fflush(stdout);
	exit(0);
    }

    printf("\nTokens held by the Cache Manager:\n\n");
    cellNum = 0;
    current_time = time(0);
    while (1) {
	rc = ktc_ListTokens(cellNum, &cellNum, &serviceName);
	if (rc) {
	    /* only error is now end of list */
	    printf("   --End of list--\n");
	    break;
	} else {
	    /* get the ticket info itself */
	    rc = ktc_GetToken(&serviceName, &token, sizeof(token),
			      &clientName);
	    if (rc) {
		printf
		    ("tokens: failed to get token info for service %s.%s.%s (code %d)\n",
		     serviceName.name, serviceName.instance, serviceName.cell,
		     rc);
		continue;
	    }
	    tokenExpireTime = token.endTime;
	    strcpy(UserName, clientName.name);
	    if (clientName.instance[0] != 0) {
		strcat(UserName, ".");
		strcat(UserName, clientName.instance);
	    }
	    if (UserName[0] == 0)
		printf("Tokens");
	    else if (strncmp(UserName, "AFS ID", 6) == 0) {
		printf("User's (%s) tokens", UserName);
	    } else if (strncmp(UserName, "Unix UID", 8) == 0) {
		printf("Tokens");
	    } else
		printf("User %s's tokens", UserName);
	    printf(" for %s%s%s@%s ", serviceName.name,
		   serviceName.instance[0] ? "." : "", serviceName.instance,
		   serviceName.cell);
	    if (tokenExpireTime <= current_time)
		printf("[>> Expired <<]\n");
	    else {
		expireString = ctime(&tokenExpireTime);
		expireString += 4;	/*Move past the day of week */
		expireString[12] = '\0';
		printf("[Expires %s]\n", expireString);
	    }
	}
    }
    exit(0);
}				/*Main program */
