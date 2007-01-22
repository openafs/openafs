/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KCONDVAR_H
#define	_OSI_LINUX_KCONDVAR_H

#if defined(COMPLETION_H_EXISTS)

#define OSI_IMPLEMENTS_CONDVAR 1

#include <linux/completion.h>
#include <linux/smp_lock.h>
#include <osi/osi_mem.h>

typedef struct {
    struct completion cv;
    osi_condvar_options_t opts;
} osi_condvar_t;

/*
 * XXX this is still a work-in-progress. it is not enabled
 */

/*
 * unfortunately, the linux kernel completion API doesn't map very well onto
 * our common use cases.  Completions do not atomically drop any locks, so
 * for what we are doing, they are race-prone.  To avoid this problem, we 
 * wrap completion calls in the BKL.
 */

osi_extern void osi_condvar_Init(osi_condvar_t *, osi_condvar_options_t *);
#define osi_condvar_Destroy(x)

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LINUX/kcondvar_inline.h>
#endif

#else /* !COMPLETION_H_EXISTS */
#include <osi/LEGACY/kcondvar.h>
#endif /* !COMPLETION_H_EXISTS */

#endif /* _OSI_LINUX_KCONDVAR_H */
