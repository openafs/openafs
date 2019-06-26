/*
 * Copyright 2011, Garrett Wollman
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <afsconfig.h>
#include <afs/param.h>

#include <afs/opr.h>

#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
# include <pthread.h>
# ifdef HAVE_PTHREAD_NP_H
#  include <pthread_np.h>
# endif

void
opr_threadname_set(const char *threadname)
{
# if defined(HAVE_PTHREAD_SET_NAME_NP)
    /* FreeBSD style */
    pthread_set_name_np(pthread_self(), threadname);
# elif defined(HAVE_PTHREAD_SETNAME_NP)
#  if PTHREAD_SETNAME_NP_ARGS == 3
    /* DECthreads style */
    pthread_setname_np(pthread_self(), threadname, (void *)0);
#  elif PTHREAD_SETNAME_NP_ARGS == 2
    /* GNU libc on Linux style */
    pthread_setname_np(pthread_self(), threadname);
#  elif PTHREAD_SETNAME_NP_ARGS == 1
    /* Mac OS style */
    pthread_setname_np(threadname);
#  else
#   error "Could not identify your pthread_setname_np() implementation"
#  endif /* PTHREAD_SETNAME_NP_ARGS */
# endif /* HAVE_PTHREAD_SETNAME_NP */
}
#endif /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */
