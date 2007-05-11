/*
 * Copyright 2005-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_CONDVAR_H
#define _OSI_OSI_CONDVAR_H 1


/*
 * platform-independent osi_condvar API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_CONDVAR)
 *
 *  struct {
 *    ....
 *    osi_condvar_t t_lock;
 *  } mystruct;
 *
 *
 *  void osi_condvar_Init(osi_condvar_t *, osi_condvar_options_t *);
 *    -- initializes the condvar
 *
 *  void osi_condvar_Destroy(osi_condvar_t *);
 *    -- destroys the condvar
 *
 *  void osi_condvar_options_Init(osi_condvar_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_condvar_options_Destroy(osi_condvar_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_condvar_options_Set(osi_condvar_options_t *, 
 *                               osi_condvar_options_param_t, 
 *                               long val);
 *    -- set the value of a parameter
 *
 *  void osi_condvar_Signal(osi_condvar_t *);
 *    -- signals a condvar waiter to wake up
 *
 *  void osi_condvar_Broadcast(osi_condvar_t *);
 *    -- wakes up all condvar waiters
 *
 *  void osi_condvar_Wait(osi_condvar_t *, osi_mutex_t *);
 *    -- waits on condition
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_CONDVAR_WAIT_SIG)
 *  int osi_condvar_WaitSig(osi_condvar_t *, osi_mutex_t *);
 *    -- waits, but returns immediately if a signal is delivered
 *
 */

#include <osi/COMMON/condvar_options.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kcondvar.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/kcondvar.h>
#else
#include <osi/LEGACY/kcondvar.h>
#endif

#elif defined(UKERNEL) /* UKERNEL */

#include <osi/LEGACY/kcondvar.h>

#else /* !KERNEL */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/condvar.h>
#elif defined(OSI_ENV_LWP)
#include <osi/LWP/condvar.h>
#endif /* OSI_ENV_LWP */
#endif /* !KERNEL */

#endif /* _OSI_OSI_CONDVAR_H */
