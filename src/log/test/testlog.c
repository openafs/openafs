/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
/*
	testlog -- Test communication with the Andrew Cache Manager.

	Usage:
		testlog [[-x] user [password] [-c cellname]]

		where:
			-x indicates that no password file lookups are to be done.
			-c identifies cellname as the cell in which authentication is to take place.
			    This implies -x, unless the given cellname matches our local one.
*/

#include <afsconfig.h>
#include <afs/param.h>


#include <itc.h>
#include <stdio.h>
#include <pwd.h>
#include <r/xdr.h>
#include <afs/comauth.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <afs/cellconfig.h>
#include <errno.h>

extern int errno;

#define	DB_CELLS    1
#define	DB_ARGPARSE 0



main(argc, argv)
     int argc;
     char *argv[];

{				/*Main routine */

    struct AllTokenInfo {
	SecretToken sTok;
	ClearToken cTok;
	char cellName[MAXCELLCHARS];
	int valid;
    };

    SecretToken sToken, testSTok;
    ClearToken cToken, testCTok;
    struct AllTokenInfo origInfo[100];	/*All original token info */
    struct passwd pwent;
    struct passwd *pw = &pwent;
    struct passwd *lclpw = &pwent;
    static char passwd[100] = { '\0' };

    static char lclCellID[MAXCELLCHARS] = { '\0' };	/*Name of local cell */
    static char cellID[MAXCELLCHARS] = { '\0' };	/*Name of desired cell */
    int doLookup = TRUE;	/*Assume pwd lookup needed */
    int foundUser = FALSE;	/*Not yet, anyway */
    int foundPassword = FALSE;	/*Not yet, anyway */
    int foundExplicitCell = FALSE;	/*Not yet, anyway */
    int currArg = 0;		/*Current (true) arg num */
    int usageError = FALSE;	/*Did user screw up args? */
    int rc;			/*Value of rpc return */
    int notPrimary;		/*Log can't set primary ID */
    int useCellEntry;		/*Do a cellular call? */
    int isPrimary;		/*Were returned tokens primary? */
    int cellNum;		/*Loop var for token gathering */

    /*
     * Get the tokens from the Cache Manager with both the cellular and non-cellular calls.
     */
    fprintf(stderr,
	    "Getting tokens from the Cache Manager (non-cellular call)\n");
    U_GetLocalTokens(&testCTok, &testSTok);

    for (cellNum = 0; cellNum < 100; cellNum++)
	origInfo[cellNum].valid = 0;
    fprintf(stderr,
	    "Getting tokens from the Cache Manager (cellular call)\n");
    useCellEntry = 1;
    cellNum = 0;
    do {
	printf("\t[%3d] ", cellNum);
	rc = U_CellGetLocalTokens(useCellEntry, cellNum,
				  &origInfo[cellNum].cTok,
				  &origInfo[cellNum].sTok,
				  origInfo[cellNum].cellName, &isPrimary);
	if (rc) {
	    /*Something didn't go well.  Print out errno, unless we got an EDOM */
	    if (errno == EDOM)
		printf("--End of list--\n");
	    else
		printf("** Error in call, errno is %d\n", errno);
	} else {
	    origInfo[cellNum].valid = 1;
	    printf("Vice ID %4d in cell '%s'%s\n",
		   origInfo[cellNum].cTok.ViceId, origInfo[cellNum].cellName,
		   (isPrimary ? " [Primary]" : ""));
	}
	cellNum++;
    } while (!(rc && (errno == EDOM)));

    /*
     * Get our local cell's name and copy it into the desired cellID buffers (assume it'll
     * be the target cell, too).
     */
#if DB_CELLS
    fprintf(stderr, "\nGetting local cell name\n");
#endif /* DB_CELLS */
    rc = GetLocalCellName();
    if (rc != CCONF_SUCCESS)
	fprintf(stderr, "\tCan't get local cell name!\n");
    strcpy(lclCellID, LclCellName);
    strcpy(cellID, LclCellName);
    fprintf(stderr, "\tUsing '%s' as the local cell name.\n", lclCellID);

    /*
     * Parse our arguments.  The current arg number is always 0.
     */
    currArg = 1;
    while (currArg < argc) {
	/*
	 * Handle current argument.
	 */
#if DB_ARGPARSE
	fprintf(stderr, "Parsing arg %d.\n", currArg);
#endif /* DB_ARGPARSE */
	if (strcmp(argv[currArg], "-x") == 0) {
	    /*
	     * Found -x flag.  Remember not to do lookups if flag is the
	     * first true argument.
	     */
	    if (currArg != 1) {
		fprintf(stderr,
			"-x switch must appear before all other arguments.\n");
		usageError = TRUE;
		break;
	    }
	    doLookup = FALSE;
	    currArg++;
#if DB_ARGPARSE
	    fprintf(stderr, "Found legal -x flag.\n");
#endif /* DB_ARGPARSE */
	} else if (strcmp(argv[currArg], "-c") == 0) {
	    /*
	     * Cell name explicitly mentioned; take it in if no other cell name has
	     * already been specified and if the name actually appears.  If the
	     * given cell name differs from our own, we don't do a lookup.
	     */
	    if (foundExplicitCell) {
		fprintf(stderr, "Only one -c switch allowed.\n");
		usageError = TRUE;
		break;
	    }
	    if (currArg + 1 >= argc) {
		fprintf(stderr, "Cell name must follow -c switch.\n");
		usageError = TRUE;
		break;
	    }
	    foundExplicitCell = TRUE;
	    if (strcmp(argv[currArg], lclCellID) != 0) {
		doLookup = FALSE;
		strcpy(cellID, argv[currArg + 1]);
	    }
	    currArg += 2;
#if DB_ARGPARSE
	    fprintf(stderr, "Found explicit cell name: '%s'\n", cellID);
#endif /* DB_ARGPARSE */
	} else if (!foundUser) {
	    /*
	     * If it's not a -x or a -c and we haven't found the user name yet, shove it into our
	     * local password entry buffer, remembering we have it.
	     */
	    foundUser = TRUE;
	    lclpw->pw_name = argv[currArg];
	    currArg++;
#if DB_ARGPARSE
	    fprintf(stderr, "Found user name: '%s'\n", lclpw->pw_name);
#endif /* DB_ARGPARSE */
	} else if (!foundPassword) {
	    /*
	     * Current argument is the desired password string.  Remember it in our local
	     * buffer, and zero out the argument string - anyone can see it there with ps!
	     */
	    foundPassword = TRUE;
	    strcpy(passwd, argv[currArg]);
	    memset(argv[currArg], 0, strlen(passwd));
	    currArg++;
#if DB_ARGPARSE
	    fprintf(stderr,
		    "Found password: '%s' (%d chars), erased from arg list (now '%s').\n",
		    passwd, strlen(passwd), argv[currArg - 1]);
#endif /* DB_ARGPARSE */
	} else {
	    /*
	     * No more legal choices here.  Remember to tell the user (constructively)
	     * that he screwed up.
	     */
	    usageError = TRUE;
	    break;
	}
    }				/*end while */

    if (argc < 2) {
	/*
	 * No arguments were provided; our only clue is the uid.
	 */
#if DB_ARGPARSE
	fprintf(stderr, "No arguments, using getpwuid(getuid()).\n");
#endif /* DB_ARGPARSE */
	pw = getpwuid(getuid());
	if (pw == NULL) {
	    fprintf(stderr,
		    "\nCan't figure out your name in local cell '%s' from your user id.\n",
		    lclCellID);
	    fprintf(stderr, "Try providing the user name.\n");
	    fprintf(stderr,
		    "Usage: testlog [[-x] user [password] [-c cellname]]\n\n");
	    exit(1);
	}
	foundUser = TRUE;
	doLookup = FALSE;
	lclpw = pw;
#if DB_ARGPARSE
	fprintf(stderr, "Found it, user name is '%s'.\n", lclpw->pw_name);
#endif /* DB_ARGPARSE */
    }

    /* 
     * Argument parsing is complete.  If the user gave us bad arguments or didn't
     * include a user, try to mend his evil ways.
     */
    if (usageError || !foundUser) {
	fprintf(stderr,
		"Usage: testlog [[-x] user [password] [-c cellname]]\n\n");
	exit(1);
    }

    /*
     * If we need to do a lookup on the user name (no -x flag, wants local cell, not already
     * looked up), then do it.
     */
    if (doLookup) {
	pw = getpwnam(lclpw->pw_name);
	if (pw == NULL) {
	    fprintf(stderr, "'%s' is not a valid user in local cell '%s'.\n",
		    lclpw->pw_name, lclCellID);
	    exit(1);
	}
#if DB_ARGPARSE
	fprintf(stderr, "Lookup on user name '%s' succeeded.\n", pw->pw_name);
#endif /* DB_ARGPARSE */
    }

    /*
     * Having all the info we need, we initialize our RPC connection.
     */
    if (U_InitRPC() != 0) {
	fprintf(stderr, "%s: Problems with RPC (U_InitRPC failed).\n",
		argv[0]);
	exit(1);
    }

    /*
     * Get the password if it was not provided.
     */
    if (passwd[0] == '\0') {
	char buf[128];

	sprintf(buf, "Password for user '%s' in cell '%s': ", lclpw->pw_name,
		cellID);
	strcpy(passwd, getpass(buf));
    }

    /*
     * Get the corresponding set of tokens from an AuthServer.
     */
    fprintf(stderr, "Trying standard cellular authentication.\n");
#if DB_CELLS
    cToken.ViceId = 123;
#endif /* DB_CELLS */
    if ((rc =
	 U_CellAuthenticate(pw->pw_name, passwd, cellID, &cToken, &sToken))
	!= AUTH_SUCCESS)
	fprintf(stderr, "\tInvalid login: code %d ('%s').\n", rc,
		U_Error(rc));
#if DB_CELLS
    else {
	fprintf(stderr, "\tCell authentication successful.\n");
	fprintf(stderr, "\tViceID field of clear token returned: %d\n",
		cToken.ViceId);
    }
#endif /* DB_CELLS */

    /*
     * Give the non-primary tokens to the Cache Manager, along with the cell they're good for.
     */
    fprintf(stderr, "\nGiving tokens to Cache Manager (non-cellular call)\n");
    rc = U_SetLocalTokens(0, &cToken, &sToken);
    if (rc)
	fprintf(stderr, "\tError: code %d ('%s')\n", rc, U_Error(rc));
    fprintf(stderr,
	    "Giving tokens to Cache Manager (cellular call, cell = '%s')\n",
	    cellID);
    notPrimary = 0;
    if (rc =
	U_CellSetLocalTokens(0, &cToken, &sToken, cellID,
			     notPrimary) != AUTH_SUCCESS)
	fprintf(stderr, "\tError: code %d ('%s')\n", rc, U_Error(rc));

    /*
     * Restore all the original tokens.
     */
    fprintf(stderr, "\nRestoring all original tokens.\n");
    for (cellNum = 0; cellNum < 100; cellNum++)
	if (origInfo[cellNum].valid) {
	    fprintf(stderr, "\tuid %4d in cell '%s'\n",
		    origInfo[cellNum].cTok.ViceId,
		    origInfo[cellNum].cellName);
	    if (rc =
		U_CellSetLocalTokens(0, &origInfo[cellNum].cTok,
				     &origInfo[cellNum].sTok,
				     origInfo[cellNum].cellName,
				     notPrimary) != AUTH_SUCCESS)
		fprintf(stderr, "\t** Error: code %d ('%s')\n", rc,
			U_Error(rc));
	}

}				/*Main routine */
