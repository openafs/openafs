/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_ATOMIC_H
#define	_OSI_LINUX_ATOMIC_H


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3)

/*
 * before linux 2.6.3 atomic_t was useless due to
 * limitations on sparcvX, X<9
 * (in order to guarantee atomicity, they
 *  embedded a lock in the 8 msb bits of every
 *  atomic_t, leaving only 24 bits of value)
 *
 * starting in 2.6.3 someone committed a patch
 * which changes it to use a vector of global
 * locks hashed off the atomic_t pointer
 */

/*
 * XXX this is still a work in progress
 * 
 * it is not yet included in osi_atomic.h
 */

#include <asm/atomic.h>

typedef atomic_t osi_atomic32_t;
typedef osi_int32 osi_atomic32_val_t;
typedef osi_int32 osi_atomic32_delta_t;

#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
typedef atomic64_t osi_atomic64_t;
typedef osi_int64 osi_atomic64_val_t;
typedef osi_int64 osi_atomic64_delta_t;
#endif


#define OSI_IMPLEMENTS_ATOMIC_MEMBAR 1
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS 1
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES 1

#define osi_atomic_membar() smp_mb()
#define osi_atomic_membar_order_loads() smp_rmb()
#define osi_atomic_membar_order_stores() smp_wmb()

#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)

#endif /* _OSI_LINUX_ATOMIC_H */
