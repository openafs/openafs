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
 * Solaris implementation.
 */

#include <afsconfig.h>
#include "afs/param.h"


#if defined(AFS_SUN5_ENV) && defined(KERNEL)

#include "rx/rx_kcommon.h"
#include "rx_kmutex.h"
#include "rx/rx_kernel.h"

#include <errno.h>
#include <sys/tiuser.h>
#include <sys/t_lock.h>
#include <sys/mutex.h>

#ifdef RX_LOCKS_DB
int
afs_cv_wait(cv, m, sigok, fileid, line)
     int fileid;
     int line;
#else
int
afs_cv_wait(cv, m, sigok)
#endif
     afs_kcondvar_t *cv;
     afs_kmutex_t *m;
     int sigok;
{
    int haveGlock = ISAFS_GLOCK();
    int retval = 0;

    if (haveGlock)
	AFS_GUNLOCK();
#ifdef RX_LOCKS_DB
    rxdb_droplock(m, osi_ThreadUnique(), fileid, line);
#endif
    if (sigok) {
	if (cv_wait_sig(cv, m) == 0)
	    retval = EINTR;
    } else {
	cv_wait(cv, m);
    }
#ifdef RX_LOCKS_DB
    rxdb_grablock(m, osi_ThreadUnique(), fileid, line);
#endif
    if (haveGlock) {
	MUTEX_EXIT(m);
	AFS_GLOCK();
	MUTEX_ENTER(m);
    }
    return retval;
}

#ifdef RX_LOCKS_DB
int
afs_cv_timedwait(cv, m, t, sigok, fileid, line)
     int fileid;
     int line;
#else
int
afs_cv_timedwait(cv, m, t, sigok)
#endif
     afs_kcondvar_t *cv;
     afs_kmutex_t *m;
     clock_t t;
     int sigok;
{
    int haveGlock = ISAFS_GLOCK();
    int retval = 0;

    if (haveGlock)
	AFS_GUNLOCK();
#ifdef RX_LOCKS_DB
    rxdb_droplock(m, osi_ThreadUnique(), fileid, line);
#endif
    if (sigok) {
	if (cv_timedwait_sig(cv, m, t) == 0)
	    retval = EINTR;
    } else {
	cv_timedwait(cv, m, t);
    }
#ifdef RX_LOCKS_DB
    rxdb_grablock(m, osi_ThreadUnique(), fileid, line);
#endif
    if (haveGlock) {
	MUTEX_EXIT(m);
	AFS_GLOCK();
	MUTEX_ENTER(m);
    }
    return retval;
}

#endif /* SUN5 && KERNEL */
