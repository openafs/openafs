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

#include <opr/lock.h>

#define RX_ENABLE_LOCKS		1

#define	afs_kmutex_t		opr_mutex_t
#define	afs_kcondvar_t		opr_cv_t
#define MUTEX_INIT(A,B,C,D)	opr_mutex_init(A)
#define MUTEX_ENTER(A)		opr_mutex_enter(A)
#define MUTEX_TRYENTER(A)	opr_mutex_tryenter(A)
#define MUTEX_EXIT(A)		opr_mutex_exit(A)
#define MUTEX_DESTROY(A)	opr_mutex_destroy(A)
#define MUTEX_ASSERT(A)
#define CV_INIT(A,B,C,D)	opr_cv_init(A)
#define CV_SIGNAL(A)		opr_cv_signal(A)
#define CV_BROADCAST(A)		opr_cv_broadcast(A)
#define CV_DESTROY(A)		opr_cv_destroy(A)
#define CV_WAIT(_cv, _lck)	{ \
				    int isGlockOwner = ISAFS_GLOCK(); \
				    if (isGlockOwner) { \
					AFS_GUNLOCK(); \
				    } \
				    opr_cv_wait((_cv), (_lck)); \
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
