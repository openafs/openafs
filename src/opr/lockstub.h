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

/*!
 * This is a set of stub defines that can be included by LWP processes to
 * disable the pthread locking macros, and typedefs
 */

#ifndef OPENAFS_OPR_LOCKSTUB_H
#define OPENAFS_OPR_LOCKSTUB_H 1

# ifdef AFS_PTHREAD_ENV
#  error "Do not use the opr/lockstub.h header with pthreaded code"
# endif

typedef int opr_mutex_t;
typedef int opr_cv_t;

# define opr_mutex_init(mutex)
# define opr_mutex_assert(mutex)
# define opr_mutex_destroy(mutex)
# define opr_mutex_enter(mutex)
# define opr_mutex_exit(mutex)
# define opr_mutex_tryenter(mutex) (1)
# define opr_cv_init(condvar)
# define opr_cv_destroy(condvar)
# define opr_cv_wait(condvar, mutex)
# define opr_cv_timedwait(condvar, mutex, timeout)
# define opr_cv_signal(condvar)
# define opr_cv_broadcast(condvar)
#endif
