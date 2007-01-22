/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_SPINLOCK_H
#define	_OSI_LEGACY_SPINLOCK_H

#if defined(OSI_IMPLEMENTS_SPINLOCK)

#define OSI_IMPLEMENTS_NATIVE_SPINLOCK 1

#else /* !OSI_IMPLEMENTS_SPINLOCK */

#include <osi/osi_mutex.h>

#if defined(OSI_IMPLEMENTS_MUTEX)

#define OSI_IMPLEMENTS_SPINLOCK 1
#define OSI_IMPLEMENTS_LEGACY_SPINLOCK 1

typedef osi_mutex_t osi_spinlock_t;

osi_extern void osi_spinlock_Init(osi_spinlock_t *, osi_spinlock_options_t *);

#define osi_spinlock_Destroy(x) osi_mutex_Destroy(x)
#define osi_spinlock_Lock(x) osi_mutex_Lock(x)
#define osi_spinlock_Unlock(x) osi_mutex_Unlock(x)

#if defined(OSI_IMPLEMENTS_MUTEX_NBLOCK)
#define OSI_IMPLEMENTS_SPINLOCK_NBLOCK 1
#define osi_spinlock_NBLock(x) osi_mutex_NBLock(x)
#endif

#endif /* OSI_IMPLEMENTS_MUTEX */

#endif /* !OSI_IMPLEMENTS_SPINLOCK */

#endif /* _OSI_LEGACY_SPINLOCK_H */
