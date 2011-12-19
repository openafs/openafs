/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Test of the ktc_*Token() routines */

#include <afsconfig.h>
#include <afs/param.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>


#include <afs/stds.h>
#include <afs/afsutil.h>
#include <afs/auth.h>

extern int ktc_SetToken(struct ktc_principal *aserver,
			struct ktc_token *atoken,
			struct ktc_principal *aclient, int flags);

extern int ktc_GetToken(struct ktc_principal *aserver,
			struct ktc_token *atoken, int atokenLen,
			struct ktc_principal *aclient);

extern int ktc_ListTokens(int aprevIndex, int *aindex,
			  struct ktc_principal *aserver);

extern int ktc_ForgetAllTokens(void);


static int SamePrincipal(struct ktc_principal *p1, struct ktc_principal *p2);
static int SameToken(struct ktc_token *t1, struct ktc_token *t2);


#define MAXCELLS  20

int
main(void)
{
    struct ktc_principal oldServer[MAXCELLS], newServer[MAXCELLS];
    struct ktc_principal oldClient[MAXCELLS], newClient[MAXCELLS];
    struct ktc_token oldToken[MAXCELLS], newToken[MAXCELLS];
    int cellCount, cellIndex;
    int i, code;

#ifdef AFS_NT40_ENV
    /* Initialize winsock; required by NT pioctl() */
    if (afs_winsockInit()) {
	printf("\nUnable to initialize winsock (required by NT pioctl()).\n");
	exit(1);
    }
#endif

    /* Get original tokens */

    printf("\nFetching original tokens.\n");

    cellIndex = 0;

    for (i = 0; i < MAXCELLS; i++) {
	/* fetch server principal */
	code = ktc_ListTokens(cellIndex, &cellIndex, &oldServer[i]);

	if (code) {
	    if (code == KTC_NOENT) {
		/* no more tokens */
		break;
	    } else {
		/* some error occured */
		perror("ktc_ListTokens failed fetching original tokens");
		exit(1);
	    }
	}

	/* fetch token and client identity w.r.t. server */
	code =
	    ktc_GetToken(&oldServer[i], &oldToken[i],
			 sizeof(struct ktc_token), &oldClient[i]);

	if (code) {
	    /* some unexpected error occured */
	    perror("ktc_GetToken failed fetching original tokens");
	    exit(1);
	}
    }

    cellCount = i;

    if (cellCount == 0) {
	printf("Obtain one or more tokens prior to executing test.\n");
	exit(0);
    } else if (cellCount == MAXCELLS) {
	printf("Only first %d tokens utilized by test; rest will be lost.\n",
	       MAXCELLS);
    }

    for (i = 0; i < cellCount; i++) {
	printf("Token[%d]: server = %s@%s, client = %s@%s\n", i,
	       oldServer[i].name, oldServer[i].cell, oldClient[i].name,
	       oldClient[i].cell);
    }


    /* Forget original tokens */

    printf("\nClearing original tokens and verifying disposal.\n");

    code = ktc_ForgetAllTokens();

    if (code) {
	perror("ktc_ForgetAllTokens failed on original tokens");
	exit(1);
    }

    for (i = 0; i < cellCount; i++) {
	struct ktc_principal dummyPrincipal;
	struct ktc_token dummyToken;

	code =
	    ktc_GetToken(&oldServer[i], &dummyToken, sizeof(struct ktc_token),
			 &dummyPrincipal);

	if (code != KTC_NOENT) {
	    printf("ktc_ForgetAllTokens did not eliminate all tokens.\n");
	    exit(1);
	}

	cellIndex = 0;

	code = ktc_ListTokens(cellIndex, &cellIndex, &dummyPrincipal);

	if (code != KTC_NOENT) {
	    printf("ktc_ForgetAllTokens did not eliminate all tokens.\n");
	    exit(1);
	}
    }


    /* Reinstall tokens */

    printf("\nReinstalling original tokens.\n");

    for (i = 0; i < cellCount; i++) {
	code = ktc_SetToken(&oldServer[i], &oldToken[i], &oldClient[i], 0);

	if (code) {
	    perror("ktc_SetToken failed reinstalling tokens");
	    exit(1);
	}
    }


    /* Get reinstalled tokens */

    printf("\nFetching reinstalled tokens.\n");

    cellIndex = 0;

    for (i = 0; i < MAXCELLS; i++) {
	/* fetch server principal */
	code = ktc_ListTokens(cellIndex, &cellIndex, &newServer[i]);

	if (code) {
	    if (code == KTC_NOENT) {
		/* no more tokens */
		break;
	    } else {
		/* some error occured */
		perror("ktc_ListTokens failed fetching reinstalled tokens");
		exit(1);
	    }
	}

	/* fetch token and client identity w.r.t. server */
	code =
	    ktc_GetToken(&newServer[i], &newToken[i],
			 sizeof(struct ktc_token), &newClient[i]);

	if (code) {
	    /* some unexpected error occured */
	    perror("ktc_GetToken failed fetching reinstalled tokens");
	    exit(1);
	}
    }


    /* Verify content of reinstalled tokens */

    printf("\nVerifying reinstalled tokens against original tokens.\n");

    if (i != cellCount) {
	printf("Reinstalled token count does not match original count.\n");
	exit(1);
    }

    for (i = 0; i < cellCount; i++) {
	int k, found;
	found = 0;

	for (k = 0; k < cellCount; k++) {
	    if (SamePrincipal(&oldServer[i], &newServer[k])
		&& SamePrincipal(&oldClient[i], &newClient[k])
		&& SameToken(&oldToken[i], &newToken[k])) {
		/* found a matching token */
		found = 1;
		break;
	    }
	}

	if (!found) {
	    printf("Reinstalled token does not match any original token.\n");
	    exit(1);
	}
    }

    /* Test passes */

    printf("\nTest completed without error.\n");
    return 0;
}


static int
SamePrincipal(struct ktc_principal *p1, struct ktc_principal *p2)
{
    if (strcmp(p1->name, p2->name) || strcmp(p1->instance, p2->instance)
	|| strcmp(p1->cell, p2->cell)) {
	/* principals do not match */
	return 0;
    } else {
	/* same principal */
	return 1;
    }
}


static int
SameToken(struct ktc_token *t1, struct ktc_token *t2)
{
    if ((t1->startTime != t2->startTime) || (t1->endTime != t2->endTime)
	|| memcmp(&t1->sessionKey, &t2->sessionKey, sizeof(t1->sessionKey))
	|| (t1->kvno != t2->kvno) || (t1->ticketLen != t2->ticketLen)
	|| memcmp(t1->ticket, t2->ticket, t1->ticketLen)) {
	/* tokens do not match */
	return 0;
    } else {
	/* same token */
	return 1;
    }
}
