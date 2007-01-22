/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
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

typedef struct osi_condvar_options {
    osi_uint8 preemptive_only;     /* only activate in pre-emptive environments (e.g. no-op for LWP) */
    osi_uint8 trace_allowed;       /* whether or not lock tracing is allowed */
    osi_uint8 trace_enabled;       /* enable lock tracing */
} osi_condvar_options_t;
/* defaults:  { 0, 1, 0 } */

typedef enum {
    OSI_CONDVAR_OPTION_PREEMPTIVE_ONLY,
    OSI_CONDVAR_OPTION_TRACE_ALLOWED,
    OSI_CONDVAR_OPTION_TRACE_ENABLED,
    OSI_CONDVAR_OPTION_MAX_ID
} osi_condvar_options_param_t;


/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

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

#if defined(OSI_PTHREAD_ENV)
#include <osi/PTHREAD/condvar.h>
#else /* !OSI_PTHREAD_ENV */
#include <osi/LWP/condvar.h>
#endif /* !OSI_PTHREAD_ENV */
#endif /* !KERNEL */

#include <osi/COMMON/condvar_options.h>

#endif /* _OSI_OSI_CONDVAR_H */
