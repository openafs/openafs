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


#include <afs/stds.h>
#include <stdio.h>

#include <afs/afs_Admin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_clientAdmin.h>


int
main(int argc, char **argv)
{
    int i;
    afs_status_t status;

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

    if (argc < 2) {
	fprintf(stderr, "Usage is: %s [<code>]+\n", argv[0]);
	exit(1);
    }

    if (!afsclient_Init(&status)) {
	fprintf(stderr, "Unable to initialize error tables\n");
	exit(1);
    }

    for (i = 1; i < argc; i++) {
	const char *errText;
	afs_status_t errStatus;

	status = (afs_status_t) atoi(argv[i]);
	util_AdminErrorCodeTranslate(status, 0, &errText, &errStatus);
	printf("%d = %s\n", status, errText);
    }
    return 0;
}
