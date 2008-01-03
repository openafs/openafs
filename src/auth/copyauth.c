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
    ("$Header$");

#include <string.h>
#include <sys/types.h>
#include "auth.h"
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include "cellconfig.h"
#include <afs/afsutil.h>

#include "AFS_component_version_number.c"

char whoami[256];

int
main(int argc, char **argv)
{
    char localName[64];
    register afs_int32 code;
    register char *cname;
    struct afsconf_dir *tdir;
    struct ktc_principal tserver;
    struct ktc_token token;

    strcpy(whoami, argv[0]);

    if (argc <= 1) {
	printf
	    ("%s: copies a file system ticket from the local cell to another cell\n",
	     whoami);
	printf("%s: usage is 'setauth <new-cell>\n", whoami);
	exit(1);
    }

    cname = argv[1];

    /* lookup the name of the local cell */
    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!tdir) {
	printf("copyauth: can't open dir %s\n", AFSDIR_CLIENT_ETC_DIRPATH);
	exit(1);
    }
    code = afsconf_GetLocalCell(tdir, localName, sizeof(localName));
    if (code) {
	printf("%s: can't determine local cell name\n", whoami);
	exit(1);
    }
    /* done with configuration stuff now */
    afsconf_Close(tdir);


    /* get ticket in local cell */
    strcpy(tserver.cell, localName);
    strcpy(tserver.name, "afs");
    tserver.instance[0] = 0;
    code = ktc_GetToken(&tserver, &token, sizeof(token), NULL);
    if (code) {
	printf
	    ("%s: failed to get '%s' service ticket in cell '%s' (code %d)\n",
	     whoami, tserver.name, tserver.cell, code);
	exit(1);
    }

    /* and now set the ticket in the new cell */
    strcpy(tserver.cell, argv[1]);
    code = ktc_SetToken(&tserver, &token, NULL, 0);
    if (code) {
	printf
	    ("%s: failed to set ticket (code %d), are you sure you're authenticated?\n",
	     whoami, code);
	exit(1);
    }

    /* all done */
    printf("Authentication established for cell %s.\n", cname);
    exit(0);
}
