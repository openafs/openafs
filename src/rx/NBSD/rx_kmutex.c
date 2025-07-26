/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kmutex.c - mutex and condition variable macros for kernel environment.
 *
 * NetBSD implementation.
 */

#include <afsconfig.h>
#include "afs/param.h"

#if defined(AFS_NBSD50_ENV)

#include "rx/rx_kcommon.h"
#include "rx_kmutex.h"
#include "rx/rx_kernel.h"

int
afs_cv_wait(afs_kcondvar_t *cv, afs_kmutex_t *m, int sigok)
{
    int haveGlock = ISAFS_GLOCK();
    int retval = 0;

    if (haveGlock)
	AFS_GUNLOCK();
    if (sigok) {
	if (cv_wait_sig(cv, m) == 0)
	    retval = EINTR;
    } else {
	cv_wait(cv, m);
    }
    if (haveGlock) {
	MUTEX_EXIT(m);
	AFS_GLOCK();
	MUTEX_ENTER(m);
    }
    return retval;
}

#endif /* AFS_NBSD50_ENV */
