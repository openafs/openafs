/*
 * Copyright 2005-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_RWLOCK_H
#define _OSI_OSI_RWLOCK_H 1


/*
 * platform-independent osi_rwlock API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_RWLOCK)
 *
 *  struct {
 *    ....
 *    osi_rwlock_t t_rwlock;
 *  } mystruct;
 *
 *
 *  void osi_rwlock_Init(osi_rwlock_t *, osi_rwlock_options_t *);
 *    -- initializes the rwlock
 *
 *  void osi_rwlock_Destroy(osi_rwlock_t *);
 *    -- destroys the rwlock
 *
 *  void osi_rwlock_RdLock(osi_rwlock_t *);
 *    -- read locks the rwlock
 *
 *  void osi_rwlock_WrLock(osi_rwlock_t *);
 *    -- write locks the rwlock
 *
 *  void osi_rwlock_Unlock(osi_rwlock_t *);
 *    -- unlock the rwlock
 *
 *  void osi_rwlock_options_Init(osi_rwlock_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_rwlock_options_Destroy(osi_rwlock_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_rwlock_options_Set(osi_rwlock_options_t *, 
 *                              osi_rwlock_options_param_t, 
 *                              long val);
 *    -- set the value of a parameter
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_NBRDLOCK)
 *  int osi_rwlock_NBRdLock(osi_rwlock_t *);
 *    -- try to acquire a read lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_NBWRLOCK)
 *  int osi_rwlock_NBWrLock(osi_rwlock_t *);
 *    -- try to acquire a write lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_DOWNGRADE)
 *  void osi_rwlock_Downgrade(osi_rwlock_t *);
 *    -- downgrade a lock from write to read
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_TRYUPGRADE)
 *  int osi_rwlock_TryUpgrade(osi_rwlock_t *);
 *    -- tries to upgrade a lock from read to write, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_QUERY_LOCKED)
 *  int osi_rwlock_IsLocked(osi_rwlock_t *);
 *    -- determine if the rwlock is locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_QUERY_RDLOCKED)
 *  int osi_rwlock_IsRdLocked(osi_rwlock_t *);
 *    -- determine if the rwlock is read locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_QUERY_WRLOCKED)
 *  int osi_rwlock_IsWrLocked(osi_rwlock_t *);
 *    -- determine if the rwlock is write locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_RWLOCK_QUERY_WRLOCKHELD)
 *  int osi_rwlock_IsWrLockHeld(osi_rwlock_t *);
 *    -- determine if this thread has the write lock, returns non-zero if held
 *
 * for convenience, the following interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_RWLOCK_QUERY_WRLOCKHELD)
 *  void osi_rwlock_AssertWrLockHeld(osi_rwlock_t *);
 *    -- panic if this thread doesn't hold the write lock
 *
 */

#include <osi/COMMON/rwlock_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/krwlock.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/krwlock.h>
#elif defined(OSI_LINUX_ENV)
#include <osi/LINUX/krwlock.h>
#else
#include <osi/LEGACY/krwlock.h>
#endif

#elif defined(UKERNEL)

#include <osi/LEGACY/krwlock.h>

#else /* !KERNEL */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/rwlock.h>
#elif defined(OSI_ENV_LWP)
#include <osi/LWP/rwlock.h>
#endif /* OSI_ENV_LWP */
#endif /* !KERNEL */

#if !defined(OSI_IMPLEMENTS_RWLOCK_QUERY_WRLOCKHELD)
#define osi_rwlock_AssertWrLockHeld(x)
#endif

#endif /* _OSI_OSI_RWLOCK_H */
