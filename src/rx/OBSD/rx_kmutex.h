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

/* Remind me to fix this later... */

#define MUTEX_ISMINE(a)
#define osirx_AssertMine(addr, msg)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)  
#define MUTEX_INIT(a,b,c,d) 
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#define AFS_ASSERT_RXGLOCK()

#endif /* _RX_KMUTEX_H_ */
