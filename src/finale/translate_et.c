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

#include <afs/com_err.h>
#include <rx/rxkad.h>
#include <afs/kautils.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/vlserver.h>
#include <afs/pterror.h>
#include <afs/bnode.h>
#include <afs/volser.h>
#include <ubik.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif


#define ERRCODE_RANGE 8		/* from error_table.h */

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    int i;
    afs_int32 code;
    afs_int32 offset;

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
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_CMD_error_table();
    initialize_VL_error_table();
    initialize_PT_error_table();
    initialize_BZ_error_table();
    initialize_U_error_table();
    initialize_VOLS_error_table();

    if (argc < 2) {
	fprintf(stderr, "Usage is: %s [<code>]+\n", argv[0]);
	exit(1);
    }

    for (i = 1; i < argc; i++) {
	code = atoi(argv[i]);
	offset = code & ((1 << ERRCODE_RANGE) - 1);

	printf("%d (%s).%d = %s\n", (int)code, afs_error_table_name(code),
	       (int)offset, afs_error_message(code));
    }
    return 0;
}
