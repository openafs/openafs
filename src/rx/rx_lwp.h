/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_lwp.h - mutexes and condition variables do not exist for LWP.
 */

#ifndef _RX_LWP_H_
#define _RX_LWP_H_

typedef int afs_kmutex_t;
typedef int afs_kcondvar_t;

#define CALL_HOLD(call, type)
#define CALL_HOLD_R(call, type)
#define CALL_RELE(call, type)
#define CALL_RELE_R(call, type)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)
#define MUTEX_INIT(a,b,c,d)
#define MUTEX_ASSERT(a)
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#define CV_WAIT(cv, l)
#define CV_SIGNAL(cv)
#define CV_BROADCAST(cv)

#endif /* _RX_LWP_H_ */
