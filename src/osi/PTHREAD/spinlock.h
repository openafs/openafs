/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_SPINLOCK_H
#define	_OSI_PTHREAD_SPINLOCK_H

#if defined(PTHREAD_HAVE_SPINLOCK)

#define OSI_IMPLEMENTS_SPINLOCK 1
#define OSI_IMPLEMENTS_SPINLOCK_NBLOCK 1

#include <pthread.h>

typedef struct {
    pthread_spinlock_t lock;
    osi_spinlock_options_t opts;
} osi_spinlock_t;

osi_extern void osi_spinlock_Init(osi_spinlock_t *, osi_spinlock_options_t *);
osi_extern void osi_spinlock_Destroy(osi_spinlock_t *);

#define osi_spinlock_Lock(x) osi_Assert(pthread_spin_lock(x)==0)
#define osi_spinlock_NBLock(x) ((pthread_spin_trylock(x)==0)?1:0)
#define osi_spinlock_Unlock(x) osi_Assert(pthread_spin_unlock(x)==0)

#endif /* PTHREAD_HAVE_SPINLOCK */

#endif /* _OSI_PTHREAD_SPINLOCK_H */
