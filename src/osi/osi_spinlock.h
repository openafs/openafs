/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SPINLOCK_H
#define _OSI_OSI_SPINLOCK_H 1


/*
 * platform-independent osi_spinlock API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_SPINLOCK)
 *
 *  struct {
 *    ....
 *    osi_spinlock_t t_lock;
 *  } mystruct;
 *
 *
 *  void osi_spinlock_Init(osi_spinlock_t *, osi_spinlock_options_t *);
 *    -- initializes the spinlock
 *
 *  void osi_spinlock_Destroy(osi_spinlock_t *);
 *    -- destroys the spinlock
 *
 *  void osi_spinlock_options_Init(osi_spinlock_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_spinlock_options_Destroy(osi_spinlock_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_spinlock_options_Set(osi_spinlock_options_t *, osi_spinlock_options_param_t, long val);
 *    -- set the value of a parameter
 *
 *  void osi_spinlock_Lock(osi_spinlock_t *);
 *    -- locks the spinlock
 *
 *  void osi_spinlock_Unlock(osi_spinlock_t *);
 *    -- unlock the spinlock
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_SPINLOCK_NBLOCK)
 *  int osi_spinlock_NBLock(osi_spinlock_t *);
 *    -- non-blocking spinlock lock, returns non-zero if lock succeeded
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_SPINLOCK_QUERY_LOCKHELD)
 *  int osi_spinlock_IsLockHeld(osi_spinlock_t *);
 *    -- determine if this thread has the spinlock locked
 *
 * for convenience, this interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_SPINLOCK_QUERY_LOCKHELD)
 *  void osi_spinlock_AssertLockHeld(osi_spinlock_t *);
 *    -- panic if this thread doesn't hold the lock
 *
 */

#include <osi/COMMON/spinlock_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_LINUX_ENV)
#include <osi/LINUX/kspinlock.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/spinlock.h>
#endif /* !OSI_ENV_PTHREAD */

#endif /* !OSI_ENV_KERNELSPACE */

/* automatically builds on top of mutexes 
 * wherever we don't have a native impl */
#include <osi/LEGACY/spinlock.h>


#if !defined(OSI_IMPLEMENTS_SPINLOCK_QUERY_LOCKHELD)
#define osi_spinlock_AssertLockHeld(x)
#endif

#endif /* _OSI_OSI_SPINLOCK_H */
