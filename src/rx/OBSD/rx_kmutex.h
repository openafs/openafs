/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * OpenBSD implementation by Jim Rees.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#include <sys/lock.h>

/* You can't have AFS_GLOBAL_SUNLOCK and not RX_ENABLE_LOCKS */
#define RX_ENABLE_LOCKS 1
#define AFS_GLOBAL_RXLOCK_KERNEL

/* This is incomplete and probably wouldn't work with NCPUS > 1 */

typedef int afs_kcondvar_t;

#define CV_INIT(cv, a, b, c)
#define CV_DESTROY(cv)
#define CV_WAIT(cv, lck)    { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        MUTEX_EXIT(lck);        \
	                        tsleep(cv, PSOCK, "afs_rx_cv_wait", 0);  \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    }
#define CV_SIGNAL(cv)		wakeup_one(cv)
#define CV_BROADCAST(cv)	wakeup(cv)

typedef struct {
    struct proc *owner;
} afs_kmutex_t;

#define	MUTEX_DEFAULT	0

#define MUTEX_INIT(a,b,c,d) \
    do { \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct proc *)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curproc; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == 0), (a)->owner = curproc, 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curproc); \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == curproc)

#endif /* _RX_KMUTEX_H_ */
