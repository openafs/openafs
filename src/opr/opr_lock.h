/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef OPENAFS_OPR_LOCK_H
#define OPENAFS_OPR_LOCK_H 1

#include <pthread.h>
#include <errno.h>

/* Mutexes */

typedef pthread_mutex_t opr_mutex_t;

# ifdef OPR_DEBUG_LOCKS
static_inline void
opr_mutex_init(opr_mutex_t *mutex)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    opr_Verify(pthread_mutex_init(mutex, &attr) == 0);
    pthread_mutexattr_destroy(&attr);
}

#  define opr_mutex_assert(mutex) \
    opr_Verify(pthread_mutex_lock(mutex) == EDEADLK)

# else

#  define opr_mutex_init(mutex) \
    opr_Verify(pthread_mutex_init(mutex, NULL) == 0)

#  define opr_mutex_assert(mutex)

# endif

# define opr_mutex_destroy(mutex) \
    opr_Verify(pthread_mutex_destroy(mutex) == 0)

# define opr_mutex_enter(mutex) \
    opr_Verify(pthread_mutex_lock(mutex) == 0)

# define opr_mutex_exit(mutex) \
    opr_Verify(pthread_mutex_unlock(mutex) == 0)

# define opr_mutex_tryenter(mutex) \
    (pthread_mutex_trylock(mutex) ? 0: 1)

typedef pthread_cond_t opr_cv_t;

# define opr_cv_init(condvar) \
    opr_Verify(pthread_cond_init(condvar, NULL) == 0)

# define opr_cv_destroy(condvar) \
    opr_Verify(pthread_cond_destroy(condvar) == 0)

# define opr_cv_wait(condvar, mutex) \
    opr_Verify(pthread_cond_wait(condvar, mutex) == 0)

static_inline int
opr_cv_timedwait(opr_cv_t *condvar, opr_mutex_t *mutex,
		 const struct timespec *abstime)
{
    int code = pthread_cond_timedwait(condvar, mutex, abstime);
    opr_Assert(code == 0 || code == ETIMEDOUT);
    return code;
}

# define opr_cv_signal(condvar) \
    opr_Verify(pthread_cond_signal(condvar) == 0)

# define opr_cv_broadcast(condvar) \
    opr_Verify(pthread_cond_broadcast(condvar) == 0)

#endif /* OPENAFS_OPR_LOCK_H */
