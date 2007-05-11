/*
 * Copyright 2005-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SHLOCK_H
#define _OSI_OSI_SHLOCK_H 1


/*
 * WARNING -- shared lock semantics are not supported
 * natively on most platforms!  Use of this interface
 * will likely reduce the performance of your
 * algorithm substantially!
 *
 */


/*
 * platform-independent osi_shlock API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_SHLOCK)
 *
 *  struct {
 *    ....
 *    osi_shlock_t t_shlock;
 *  } mystruct;
 *
 *
 *  void osi_shlock_Init(osi_shlock_t *, osi_shlock_options_t *);
 *    -- initializes the shlock
 *
 *  void osi_shlock_Destroy(osi_shlock_t *);
 *    -- destroys the shlock
 *
 *  void osi_shlock_options_Init(osi_shlock_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_shlock_options_Destroy(osi_shlock_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_shlock_options_Set(osi_shlock_options_t *, 
 *                              osi_shlock_options_param_t, 
 *                              long val);
 *    -- set the value of a parameter
 *
 *  void osi_shlock_RdLock(osi_shlock_t *);
 *    -- read locks the shlock
 *
 *  void osi_shlock_ShLock(osi_shlock_t *);
 *    -- shared locks the shlock
 *
 *  void osi_shlock_WrLock(osi_shlock_t *);
 *    -- write locks the shlock
 *
 *  void osi_shlock_RdUnlock(osi_shlock_t *);
 *    -- drop read lock on the shlock
 *
 *  void osi_shlock_ShUnlock(osi_shlock_t *);
 *    -- drop shared lock on the shlock
 *
 *  void osi_shlock_WrUnlock(osi_shlock_t *);
 *    -- drop write lock on the shlock
 *
 *  void osi_shlock_SToW(osi_shlock_t *);
 *    -- upgrade a shared lock to a write lock
 *
 *  void osi_shlock_WToS(osi_shlock_t *);
 *    -- downgrade a write lock to a shared lock
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_WTOR)
 *  void osi_shlock_WToR(osi_shlock_t *);
 *    -- downgrade a write lock to a read lock
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_STOR)
 *  void osi_shlock_SToR(osi_shlock_t *);
 *    -- downgrade a shared lock to a read lock
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_NBRDLOCK)
 *  int osi_shlock_NBRdLock(osi_shlock_t *);
 *    -- try to acquire a read lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_NBSHLOCK)
 *  int osi_shlock_NBShLock(osi_shlock_t *);
 *    -- try to acquire a shared lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_NBWRLOCK)
 *  int osi_shlock_NBWrLock(osi_shlock_t *);
 *    -- try to acquire a write lock, returns non-zero on success
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_LOCKED)
 *  int osi_shlock_IsLocked(osi_shlock_t *);
 *    -- determine if the shlock is locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_RDLOCKED)
 *  int osi_shlock_IsRdLocked(osi_shlock_t *);
 *    -- determine if the shlock is read locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_SHLOCKED)
 *  int osi_shlock_IsShLocked(osi_shlock_t *);
 *    -- determine if the shlock is shared locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_WRLOCKED)
 *  int osi_shlock_IsWrLocked(osi_shlock_t *);
 *    -- determine if the shlock is write locked by someone
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_SHLOCKHELD)
 *  int osi_shlock_IsShLockHeld(osi_shlock_t *);
 *    -- determine if this thread has the shared lock, returns non-zero if held
 *
 * for convenience, the following interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_SHLOCK_QUERY_SHLOCKHELD)
 *  void osi_shlock_AssertShLockHeld(osi_shlock_t *);
 *    -- panic if this thread doesn't hold the shared lock
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SHLOCK_QUERY_WRLOCKHELD)
 *  int osi_shlock_IsWrLockHeld(osi_shlock_t *);
 *    -- determine if this thread has the write lock, returns non-zero if held
 *
 * for convenience, the following interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_SHLOCK_QUERY_WRLOCKHELD)
 *  void osi_shlock_AssertWrLockHeld(osi_shlock_t *);
 *    -- panic if this thread doesn't hold the write lock
 *
 */

#include <osi/COMMON/shlock_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#include <osi/LEGACY/kshlock.h>

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/shlock.h>
#elif defined(OSI_ENV_LWP)
#include <osi/LWP/shlock.h>
#endif /* OSI_ENV_LWP */

#endif /* !OSI_ENV_KERNELSPACE */

#if !defined(OSI_IMPLEMENTS_SHLOCK_QUERY_SHLOCKHELD)
#define osi_shlock_AssertShLockHeld(x)
#endif
#if !defined(OSI_IMPLEMENTS_SHLOCK_QUERY_WRLOCKHELD)
#define osi_shlock_AssertWrLockHeld(x)
#endif

#endif /* _OSI_OSI_SHLOCK_H */
