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
 * User-space implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#define RX_ENABLE_LOCKS		1

#define	afs_kmutex_t		usr_mutex_t
#define	afs_kcondvar_t		usr_cond_t
#define MUTEX_INIT(A,B,C,D)	usr_mutex_init(A)
#define MUTEX_ENTER(A)		usr_mutex_lock(A)
#define MUTEX_TRYENTER(A)	usr_mutex_trylock(A)
#define MUTEX_EXIT(A)		usr_mutex_unlock(A)
#define MUTEX_DESTROY(A)	usr_mutex_destroy(A)
#define MUTEX_ASSERT(A)
#define CV_INIT(A,B,C,D)	usr_cond_init(A)
#define CV_SIGNAL(A)		usr_cond_signal(A)
#define CV_BROADCAST(A)		usr_cond_broadcast(A)
#define CV_DESTROY(A)		usr_cond_destroy(A)
#define CV_WAIT(_cv, _lck)	{ \
				    int isGlockOwner = ISAFS_GLOCK(); \
				    if (isGlockOwner) { \
					AFS_GUNLOCK(); \
				    } \
				    usr_cond_wait(_cv, _lck); \
				    if (isGlockOwner) { \
					MUTEX_EXIT(_lck); \
					AFS_GLOCK(); \
					MUTEX_ENTER(_lck); \
				    } \
				}

#define SPLVAR
#define NETPRI
#define USERPRI

#endif /* _RX_KMUTEX_H_ */
