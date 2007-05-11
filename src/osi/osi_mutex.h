/*
 * Copyright 2005-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_MUTEX_H
#define _OSI_OSI_MUTEX_H 1


/*
 * platform-independent osi_mutex API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_MUTEX)
 *
 *  struct {
 *    ....
 *    osi_mutex_t t_lock;
 *  } mystruct;
 *
 *
 *  void osi_mutex_Init(osi_mutex_t *, osi_mutex_options_t *);
 *    -- initializes the mutex
 *
 *  void osi_mutex_Destroy(osi_mutex_t *);
 *    -- destroys the mutex
 *
 *  void osi_mutex_options_Init(osi_mutex_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_mutex_options_Destroy(osi_mutex_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_mutex_options_Set(osi_mutex_options_t *, 
 *                             osi_mutex_options_param_t, 
 *                             long val);
 *    -- set the value of a parameter
 *
 *  void osi_mutex_Lock(osi_mutex_t *);
 *    -- locks the mutex
 *
 *  void osi_mutex_Unlock(osi_mutex_t *);
 *    -- unlock the mutex
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_MUTEX_NBLOCK)
 *  int osi_mutex_NBLock(osi_mutex_t *);
 *    -- non-blocking mutex lock, returns non-zero if lock succeeded
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_MUTEX_QUERY_LOCKHELD)
 *  int osi_mutex_IsLockHeld(osi_mutex_t *);
 *    -- determine if this thread has the mutex locked
 *
 * for convenience, this interface is defined as a no-op when
 * !defined(OSI_IMPLEMENTS_MUTEX_QUERY_LOCKHELD)
 *  void osi_mutex_AssertLockHeld(osi_mutex_t *);
 *    -- panic if this thread doesn't hold the lock
 *
 */

#include <osi/COMMON/mutex_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmutex.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/kmutex.h>
#elif defined(OSI_LINUX_ENV)
#include <osi/LINUX/kmutex.h>
#else
#include <osi/LEGACY/kmutex.h>
#endif

#elif defined(UKERNEL) /* UKERNEL */

#include <osi/LEGACY/kmutex.h>

#else /* !KERNEL */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/mutex.h>
#elif defined(OSI_ENV_LWP)
#include <osi/LWP/mutex.h>
#endif /* OSI_ENV_LWP */
#endif /* !KERNEL */

#if !defined(OSI_IMPLEMENTS_MUTEX_QUERY_LOCKHELD)
#define osi_mutex_AssertLockHeld(x)
#endif

#endif /* _OSI_OSI_MUTEX_H */
