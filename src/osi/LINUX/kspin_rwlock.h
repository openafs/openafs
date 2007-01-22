/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KSPIN_RWLOCK_H
#define	_OSI_LINUX_KSPIN_RWLOCK_H

#define OSI_IMPLEMENTS_SPIN_RWLOCK 1
#define OSI_IMPLEMENTS_SPIN_RWLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_SPIN_RWLOCK_NBWRLOCK 1

#include <linux/spinlock.h>

typedef struct {
    rwlock_t lock;
    int osi_volatile level;
    osi_spin_rwlock_options_t opts;
} osi_spin_rwlock_t;

osi_extern void osi_spin_rwlock_Init(osi_spin_rwlock_t *, osi_spin_rwlock_options_t *);
#define osi_spin_rwlock_Destroy(x)
#define osi_spin_rwlock_RdLock(x) read_lock(&((x)->lock))

#if defined(OSI_IMPLEMENTS_SPIN_RWLOCK_NBRDLOCK)
#define osi_spin_rwlock_NBRdLock(x) (!(read_trylock(&((x)->lock))))
#endif

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LINUX/kspin_rwlock_inline.h>
#endif

#endif /* _OSI_LINUX_KSPIN_RWLOCK_H */
