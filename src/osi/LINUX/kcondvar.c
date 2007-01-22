/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_condvar.h>

#if defined(COMPLETION_H_EXISTS)

#include <linux/completion.h>

/*
 * unfortunately, the linux kernel completion API doesn't map very well onto
 * our common use cases.  Completions do not atomically drop any locks, so
 * for what we are doing, they are race-prone.  To avoid this problem, we 
 * wrap completion calls in the BKL.
 *
 * (a completion is stateful; its semantics are much closer to a semaphore
 *  than a condvar)
 */

void
osi_condvar_Init(osi_condvar_t * cv, osi_condvar_options_t * opts)
{
    init_completion(&cv->cv);
    osi_condvar_options_Copy(&cv->opts, opts);
}

#endif /* COMPLETION_H_EXISTS */
