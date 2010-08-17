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

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include "procmgmt.h"
#include "pmgtprivate.h"


/* -----------------  Processes  ---------------- */


/*
 * pmgt_ProcessSpawnVE() -- Spawn a process (Unix fork()/execve() semantics)
 *
 *     Returns pid of the child process ((pid_t)-1 on failure with errno set).
 *
 *     Notes: A senvp value of NULL results in Unix fork()/execv() semantics.
 *            Open files are not inherited, except stdin, stdout, and stderr.
 *            If child fails to exec() spath, its exit code is estatus.
 *
 * ASSUMPTIONS: sargv[0] is the same as spath (or its last component).
 */
pid_t
pmgt_ProcessSpawnVE(const char *spath, char *sargv[], char *senvp[],
		    int estatus)
{
    pid_t pid;

    /* create child process to exec spath */
    if ((pid = fork()) == 0) {
	/* child process */
	int i;

	/* close random fd's above stderr */
	for (i = 3; i < 64; i++) {
	    close(i);
	}

	if (senvp) {
	    execve(spath, sargv, senvp);
	} else {
	    execv(spath, sargv);
	}

	/* this point is only reached if exec() failed */
	exit(estatus);
    } else {
	/* parent process */
	return pid;
    }
}
