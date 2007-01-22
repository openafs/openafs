/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KCONDVAR_INLINE_H
#define	_OSI_LINUX_KCONDVAR_INLINE_H

#if defined(COMPLETION_H_EXISTS)

#define OSI_IMPLEMENTS_CONDVAR 1

/*
 * unfortunately, the linux kernel completion API doesn't map very well onto
 * our common use cases.  Completions do not atomically drop any locks, so
 * for what we are doing, they are race-prone.  To avoid this problem, we 
 * wrap completion calls in the BKL.
 *
 * Furthermore, unlike real condvars, linux completions are stateful --
 * it's kinda like a semaphore (but with a condvar-esque api).
 * Thus, every time you want to use them, you need to perform INIT_COMPLETION()
 * to reset state.  We perform this reset under both the mutex and the BKL
 * to ensure maximal safety.  As long as you use condvars properly this should
 * not come back to bite you.
 */

osi_inline_define(
void
osi_condvar_Signal(osi_condvar_t * cv)
{
    lock_kernel();
    complete(&cv->cv);
    unlock_kernel();
}
)
osi_inline_prototype(
void
osi_condvar_Signal(osi_condvar_t * cv)
)

osi_inline_define(
void
osi_condvar_Broadcast(osi_condvar_t * cv)
{
    lock_kernel();
    complete_all(&cv->cv);
    unlock_kernel();
}
)
osi_inline_prototype(
void
osi_condvar_Broadcast(osi_condvar_t * cv)
)

osi_inline_define(
void
osi_condvar_Wait(osi_condvar_t * cv, osi_mutex_t * lock)
{
    lock_kernel();
    INIT_COMPLETION(cv->cv);
    osi_mutex_Unlock(lock);
    wait_for_completion(&cv->cv);
    unlock_kernel();
    osi_mutex_Lock(lock);
}
)
osi_inline_prototype(
void
osi_condvar_Wait(osi_condvar_t * cv, osi_mutex_t * lock)
)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
#define OSI_IMPLEMENTS_CONDVAR_WAIT_SIG 1
osi_inline_define(
int
osi_condvar_WaitSig(osi_condvar_t * cv, osi_mutex_t * lock)
{
    int code;

    lock_kernel();
    INIT_COMPLETION(cv->cv);
    osi_mutex_Unlock(lock);
    code = wait_for_completion_interruptible(&cv->cv);
    unlock_kernel();
    osi_mutex_Lock(lock);

    return code;
}
)
osi_inline_prototype(
int
osi_condvar_WaitSig(osi_condvar_t * cv, osi_mutex_t * lock)
)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10) */

#endif /* COMPLETION_H_EXISTS */

#endif /* _OSI_LINUX_KCONDVAR_INLINE_H */
