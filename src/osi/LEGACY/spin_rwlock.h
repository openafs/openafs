/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_SPIN_RWLOCK_H
#define	_OSI_LEGACY_SPIN_RWLOCK_H

#if defined(OSI_IMPLEMENTS_SPIN_RWLOCK)

#define OSI_IMPLEMENTS_NATIVE_SPIN_RWLOCK 1

#else /* !OSI_IMPLEMENTS_SPIN_RWLOCK */

#include <osi/osi_rwlock.h>

#if defined(OSI_IMPLEMENTS_RWLOCK)

#define OSI_IMPLEMENTS_SPIN_RWLOCK 1
#define OSI_IMPLEMENTS_LEGACY_SPIN_RWLOCK 1

typedef osi_rwlock_t osi_spin_rwlock_t;

osi_extern void osi_spin_rwlock_Init(osi_spin_rwlock_t *, osi_spin_rwlock_options_t *);

#define osi_spin_rwlock_Destroy(x) osi_rwlock_Destroy(x)
#define osi_spin_rwlock_RdLock(x) osi_rwlock_RdLock(x)
#define osi_spin_rwlock_WrLock(x) osi_rwlock_WrLock(x)
#define osi_spin_rwlock_Unlock(x) osi_rwlock_Unlock(x)

#if defined(OSI_IMPLEMENTS_RWLOCK_NBRDLOCK)
#define OSI_IMPLEMENTS_SPIN_RWLOCK_NBRDLOCK 1
#define osi_spin_rwlock_NBRdLock(x) osi_rwlock_NBRdLock(x)
#endif
#if defined(OSI_IMPLEMENTS_RWLOCK_NBWRLOCK)
#define OSI_IMPLEMENTS_SPIN_RWLOCK_NBWRLOCK 1
#define osi_spin_rwlock_NBWrLock(x) osi_rwlock_NBWrLock(x)
#endif

#endif /* OSI_IMPLEMENTS_MUTEX */

#endif /* !OSI_IMPLEMENTS_SPIN_RWLOCK */

#endif /* _OSI_LEGACY_SPIN_RWLOCK_H */
