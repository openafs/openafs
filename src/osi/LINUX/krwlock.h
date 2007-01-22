/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KRWLOCK_H
#define _OSI_LINUX_KRWLOCK_H 1

#define OSI_IMPLEMENTS_RWLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBWRLOCK 1
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define OSI_IMPLEMENTS_RWLOCK_DOWNGRADE 1
#endif

#include <linux/rwsem.h>

typedef struct {
    struct rw_semaphore lock;
    int osi_volatile level;
    osi_rwlock_options_t opts;
} osi_rwlock_t;

osi_extern void osi_rwlock_Init(osi_rwlock_t *, osi_rwlock_options_t *);
#define osi_rwlock_Destroy(x)

#define osi_rwlock_RdLock(x) down_read(&((x)->lock))
#if defined(OSI_IMPLEMENTS_RWLOCK_NBRDLOCK)
#define osi_rwlock_NBRdLock(x) down_read_trylock(&((x)->lock))
#endif


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LINUX/krwlock_inline.h>
#endif

#endif /* _OSI_LINUX_KRWLOCK_H */
