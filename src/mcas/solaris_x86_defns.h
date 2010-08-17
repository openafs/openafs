/*
Copyright (c) 2003, Keir Fraser All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Keir Fraser
    * nor the names of its contributors may be used to endorse or
    * promote products derived from this software without specific
    * prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __SOLARIS_X86_DEFNS_H__
#define __SOLARIS_X86_DEFNS_H__

#ifndef SOLARIS_X86_686
#define SOLARIS_X86_686
#endif

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/atomic.h>
#include <sched.h>
#include <alloca.h>

#define CACHE_LINE_SIZE 64

#if 0
#include <thread.h>
#define pthread_mutex_t mutex_t
#define pthread_cond_t  cond_t
#define pthread_t       thread_t
#define pthread_key_t   thread_key_t
#define pthread_create(_a,_b,_c,_d) thr_create(NULL,0,_c,_d,THR_BOUND|THR_NEW_LWP,_a)
#define pthread_join(_a,_b) thr_join(_a,NULL,NULL)
#define pthread_key_create(_a,_b) thr_keycreate(_a,_b)
#define pthread_setspecific(_a,_b) thr_setspecific(_a,_b)
static void *
pthread_getspecific(pthread_key_t _a)
{
    void *__x;
    thr_getspecific(_a, &__x);
    return __x;
}

#define pthread_setconcurrency(_x) thr_setconcurrency(_x)
#define pthread_mutex_init(_a,_b) mutex_init(_a,USYNC_THREAD,NULL)
#define pthread_mutex_lock(_a) mutex_lock(_a)
#define pthread_mutex_unlock(_a) mutex_unlock(_a)
#define pthread_cond_init(_a,_b) cond_init(_a,USYNC_THREAD,NULL)
#define pthread_cond_wait(_a,_b) cond_wait(_a,_b)
#define pthread_cond_broadcast(_a) cond_broadcast(_a)
#else
#include <pthread.h>
#endif


/*
 * I. Compare-and-swap.
 */

#define CAS(_a, _o, _n)\
atomic_cas_32((_a), (_o), (_n))

#define FAS(_a, _n)\
atomic_swap_32((_a), (uint32_t)(_n))

/* Update Pointer location, return Old value. */
#define FASPO(_a, _n)\
atomic_swap_ptr((_a), (_n))

#define CASPO(_a, _o, _n)\
atomic_cas_ptr((_a), (void *) (_o), (void *) (_n))

#define CAS64(_a, _o, _n)\
(_u64) atomic_cas_64((volatile uint64_t *)(_a), (_o), (_n))

/* Update Integer location, return Old value. */
#define CASIO(_a, _o, _n)\
atomic_cas_32((volatile uint32_t *)(_a), (_o), (_n))

#define FASIO(_a, _n)\
atomic_swap_32((volatile uint32_t *)(_a), (_n))

/* Update 32/64-bit location, return Old value. */
#define CAS32O CAS
#define CAS64O CAS64


/*
 * II. Memory barriers.
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 *
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */

#define WMB() membar_producer
#define RMB() membar_consumer
#define MB()\
(membar_enter(),membar_exit(),membar_producer(),membar_consumer())

#define VOLATILE		volatile

/* On Intel, CAS is a strong barrier, but not a compile barrier. */
#define RMB_NEAR_CAS() WMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS()  WMB()

/*
 * III. Cycle counter access.
 */

#if 1 /* Sun Studio 12 */
typedef unsigned long long tick_t;
#define RDTICK() \
    ({ tick_t __t; __asm__ __volatile__ ("rdtsc" : "=A" (__t)); __t; })
#else
typedef unsigned long tick_t;
extern tick_t RDTICK(void);
#endif


/*
 * IV. Types.
 */

typedef unsigned char _u8;
typedef unsigned short _u16;
typedef unsigned int _u32;
typedef unsigned long long _u64;

#endif /* __SOLARIS_X86_DEFNS_H__ */
