/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * dpass
 *
 * This program allows a user to discover the password generated for him
 * by the migration toolkit when migrating his password information
 * to the DCE.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/dauth/Attic/dpass.c,v 1.7.2.1 2006/10/22 02:08:33 jaltman Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <ubik.h>

#include <stdio.h>
#include <pwd.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/cmd.h>
#include "adkint.h"
#include "assert.h"
#include <des.h>


char *msg[] = {
    "",
    "Please read the following message before entering your password.",
    "",
    "This program will display your new, temporary DCE password on your",
    "terminal, and you should change the assigned password as soon as",
    "possible (from a DCE client). The program assumes that your site uses",
    "the standard AFS authentication service provided by Transarc and that",
    "your initial account was created from the AFS authentication",
    "information by Transarc-supplied migration software.  If this is not",
    "the case, you should consult your system administrator.  The password",
    "you enter should be the AFS password that was in effect when your DCE",
    "account was created; this is not necessarily the same password you",
    "have at the moment. The cell name (which you may override with a",
    "command line option), must be the name of the AFS cell from which the",
    "authentication information was taken.",
    0
};

CommandProc(as, arock)
     char *arock;
     struct cmd_syndesc *as;
{
    int i;
    afs_int32 code;
    struct ktc_encryptionKey key;
    char cell[MAXKTCREALMLEN];
    char *cell_p;
    char prompt[1000];

    /*
     * We suppress the message, above, if this environment variable
     * is set. This allows the administrator to wrap dpass in a shell
     * script, if desired, in order to display more information
     * about registry conversion dates, etc.
     */
    if (!getenv("DPASS_NO_MESSAGE")) {
	for (i = 0; msg[i]; i++)
	    printf("%s\n", msg[i]);
    }

    if (as->parms[0].items) {
	struct afsconf_cell cellinfo;
	struct afsconf_dir *cdir;

	cell_p = as->parms[0].items->data;
	cdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	if (!cdir) {
	    fprintf(stderr,
		    "\nUnable to verify that \"%s\" is a valid cell name\nProceeding on the assumption that it is correct.\n",
		    cell_p);
	    exit(1);
	}
	code = afsconf_GetCellInfo(cdir, cell_p, 0, &cellinfo);
	if (code) {
	    fprintf(stderr,
		    "\nUnable to find information about cell \"%s\"\nProceeding on the assumption that this is a valid cell name.\n",
		    cell_p);
	} else {
	    strncpy(cell, cellinfo.name, sizeof(cell) - 1);
	    cell[sizeof(cell)-1] = '\0';
	    cell_p = cell;
	}
    } else {
	struct afsconf_dir *cdir;

	cdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	if (!cdir) {
	    fprintf(stderr,
		    "\nUnable to read the AFS client configuration file to get local cell name.\nTry specifying the cell with the -cell switch.\n");
	    exit(1);
	}
	afsconf_GetLocalCell(cdir, cell, sizeof(cell));
	cell_p = cell;
    }

    printf("\n");
    sprintf(prompt, "Original password for AFS cell %s: ", cell_p);
    code = ka_ReadPassword(prompt, 1, cell_p, &key);
    if (code) {
	fprintf(stderr, "Error reading password\n");
	exit(1);
    }
#define k(i) (key.data[i] & 0xff)
#define s(n) ((k(n) << 8) | k(n+1))
    printf("\nThe new DCE password is: %0.4x-%0.4x-%0.4x-%0.4x\n", s(0), s(2),
	   s(4), s(6));
}

main(argc, argv)
     int argc;
     char *argv[];
{
    struct cmd_syndesc *ts;
    afs_int32 code;
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
    ts = cmd_CreateSyntax(NULL, CommandProc, 0, "show new DCE passord");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL,
		"original AFS cell name");
    code = cmd_Dispatch(argc, argv);
    exit(code);
}
