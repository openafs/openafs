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

#include <roken.h>

#include <rx/xdr.h>
#include <afs/auth.h>
#include <afs/ktc.h>

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
    char UserName[MAXKTCNAMELEN * 2 + 2]; /*Printable user name */
    char *cellName = NULL;
    struct ktc_principal clientName;	/* service name for ticket */
    struct ktc_token token;	/* the token we're printing */
    struct ktc_setTokenData *tokenSet;

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
	free(cellName);
	cellName = NULL;
	rc = ktc_ListTokensEx(cellNum, &cellNum, &cellName);
	if (rc) {
	    /* only error is now end of list */
	    printf("   --End of list--\n");
	    break;
	} else {
	    /* get the ticket info itself */
	    rc = ktc_GetTokenEx(cellName, &tokenSet);
	    if (rc) {
		printf
		    ("tokens: failed to get token info for cell %s (code %d)\n",
		     cellName, rc);
		continue;
	    }
	    rc = token_extractRxkad(tokenSet, &token, NULL, &clientName);
	    if (rc == 0) {
		tokenExpireTime = token.endTime;
		strcpy(UserName, clientName.name);
		if (clientName.instance[0] != 0) {
		    strcat(UserName, ".");
		    strcat(UserName, clientName.instance);
		}
	        if (UserName[0] == 0)
		    printf("rxkad Tokens");
		else if (strncmp(UserName, "AFS ID", 6) == 0) {
		    printf("User's (%s) rxkad tokens", UserName);
	        } else if (strncmp(UserName, "Unix UID", 8) == 0) {
		    printf("RxkadTokens");
		} else
		    printf("User %s's rxkad tokens", UserName);
		printf(" for %s ", cellName);
		if (tokenExpireTime <= current_time)
		    printf("[>> Expired <<]\n");
		else {
		    expireString = ctime(&tokenExpireTime);
		    expireString += 4;	/*Move past the day of week */
		    expireString[12] = '\0';
		    printf("[Expires %s]\n", expireString);
	        }
	    }
	    token_FreeSet(&tokenSet);
	}
    }
    exit(0);
}				/*Main program */
