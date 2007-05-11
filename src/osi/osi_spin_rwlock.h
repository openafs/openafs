/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SPIN_RWLOCK_H
#define _OSI_OSI_SPIN_RWLOCK_H 1


/*
 * platform-independent osi_spin_rwlock API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK)
 *
 *  struct {
 *    ....
 *    osi_spin_rwlock_t t_spin_rwlock;
 *  } mystruct;
 *
 *
 *  void osi_spin_rwlock_Init(osi_spin_rwlock_t *, osi_spin_rwlock_options_t *);
 *    -- initializes the spin_rwlock
 *
 *  void osi_spin_rwlock_Destroy(osi_spin_rwlock_t *);
 *    -- destroys the spin_rwlock
 *
 *  void osi_spin_rwlock_RdLock(osi_spin_rwlock_t *);
 *    -- read locks the spin_rwlock
 *
 *  void osi_spin_rwlock_WrLock(osi_spin_rwlock_t *);
 *    -- write locks the spin_rwlock
 *
 *  void osi_spin_rwlock_Unlock(osi_spin_rwlock_t *);
 *    -- unlock the spin_rwlock
 *
 *  void osi_spin_rwlock_options_Init(osi_spin_rwlock_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_spin_rwlock_options_Destroy(osi_spin_rwlock_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_spin_rwlock_options_Set(osi_spin_rwlock_options_t *, 
 *                                   osi_spin_rwlock_options_param_t, 
 *                                   long val);
 *    -- set the value of a parameter
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_NBRDLOCK)
 *  int osi_spin_rwlock_NBRdLock(osi_spin_rwlock_t *);
 *    -- try to acquire a read lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_NBWRLOCK)
 *  int osi_spin_rwlock_NBWrLock(osi_spin_rwlock_t *);
 *    -- try to acquire a write lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_DOWNGRADE)
 *  void osi_spin_rwlock_Downgrade(osi_spin_rwlock_t *);
 *    -- downgrade a lock from write to read
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_TRYUPGRADE)
 *  int osi_spin_rwlock_TryUpgrade(osi_spin_rwlock_t *);
 *    -- tries to upgrade a lock from read to write, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_LOCKED)
 *  int osi_spin_rwlock_IsLocked(osi_spin_rwlock_t *);
 *    -- determine if the spin_rwlock is locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_RDLOCKED)
 *  int osi_spin_rwlock_IsRdLocked(osi_spin_rwlock_t *);
 *    -- determine if the spin_rwlock is read locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_WRLOCKED)
 *  int osi_spin_rwlock_IsWrLocked(osi_spin_rwlock_t *);
 *    -- determine if the spin_rwlock is write locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_WRLOCKHELD)
 *  int osi_spin_rwlock_IsWrLockHeld(osi_spin_rwlock_t *);
 *    -- determine if this thread has the write lock, returns non-zero if held
 *
 * for convenience, the following interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_WRLOCKHELD)
 *  void osi_spin_rwlock_AssertWrLockHeld(osi_spin_rwlock_t *);
 *    -- panic if this thread doesn't hold the write lock
 *
 */

#include <osi/COMMON/spin_rwlock_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_LINUX_ENV)
#include <osi/LINUX/kspin_rwlock.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

/* no known userspace rw spinlock impls */

#endif /* !OSI_ENV_KERNELSPACE */

#include <osi/LEGACY/spin_rwlock.h>

#if !defined(OSI_IMPLEMENTS_SPIN_RWLOCK_QUERY_WRLOCKHELD)
#define osi_spin_rwlock_AssertWrLockHeld(x)
#endif

#endif /* _OSI_OSI_SPIN_RWLOCK_H */
