/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KSPINLOCK_H
#define	_OSI_LINUX_KSPINLOCK_H

#define OSI_IMPLEMENTS_SPINLOCK 1
#define OSI_IMPLEMENTS_SPINLOCK_NBLOCK 1

#include <linux/spinlock.h>

typedef struct {
    spinlock_t lock;
    osi_spinlock_options_t opts;
} osi_spinlock_t;

osi_extern void osi_spinlock_Init(osi_spinlock_t *, osi_spinlock_options_t *);
#define osi_spinlock_Destroy(x)
#define osi_spinlock_Lock(x) spin_lock(&((x)->lock))
#define osi_spinlock_NBLock(x) (!(spin_trylock(&((x)->lock))))
#define osi_spinlock_Unlock(x) spin_unlock(&((x)->lock))

#endif /* _OSI_LINUX_KSPINLOCK_H */
