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

#ifndef __INTEL_DEFNS_H__
#define __INTEL_DEFNS_H__

#include <pthread.h>
#include <sched.h>

#ifndef X86_64
#define X86_64
#endif

#define CACHE_LINE_SIZE 64

#if 0
#define pthread_mutex_init(_m,_i) \
({ pthread_mutex_init(_m,_i); (_m)->__m_kind = PTHREAD_MUTEX_ADAPTIVE_NP; })
#endif


/*
 * I. Compare-and-swap.
 */

/*
 * This is a strong barrier! Reads cannot be delayed beyond a later store.
 * Reads cannot be hoisted beyond a LOCK prefix. Stores always in-order.
 */
#define CAS32(_a, _o, _n)                                    \
({ __typeof__(_o) __o = _o;                                \
   __asm__ __volatile__(                                   \
       "lock cmpxchg %3,%1"                                \
       : "=a" (__o), "=m" (*(volatile unsigned int *)(_a)) \
       :  "0" (__o), "r" (_n) );                           \
   __o;                                                    \
})

#define FAS32(_a, _n)                                        \
({ __typeof__(_n) __o;                                     \
   __asm__ __volatile__(                                   \
       "lock xchg %0,%1"                                   \
       : "=r" (__o), "=m" (*(volatile unsigned int *)(_a)) \
       :  "0" (_n) );                                      \
   __o;                                                    \
})

#define FAS64(_a, _n)					   \
    ({ __typeof__(_n) __o;				   \
      __asm__ __volatile__(				   \
       "xchgq %0,%1"                                       \
       : "=r" (__o), "=m" (*(volatile unsigned long long *)(_a)) \
       :  "0" (_n)					   \
    );							   \
   __o;                                                    \
})

/* Valid, but not preferred */
#define CAS64_x86_style(_a, _o, _n)                                        \
({ __typeof__(_o) __o = _o;                                      \
   __asm__ __volatile__(                                         \
       "movl %3, %%ecx;"                                         \
       "movl %4, %%ebx;"                                         \
       "lock cmpxchg8b %1"                                       \
       : "=A" (__o), "=m" (*(volatile unsigned long long *)(_a)) \
       : "0" (__o), "m" (_n >> 32), "m" (_n)                     \
       : "ebx", "ecx" );                                         \
   __o;                                                          \
})

#define CAS64(_a, _o, _n)                                        \
    ({ __typeof__(_o) __o = _o;						\
  __asm__ __volatile__ ("lock cmpxchgq %1,%2"				\
			: "=a" (__o)					\
			:"r" (_n),					\
			 "m" (*(volatile unsigned long long *)(_a)),	\
			 "0" (__o)					\
			: "memory");					\
  __o;									\
 })

#define CAS(_x,_o,_n) ((sizeof (*_x) == 4)?CAS32(_x,_o,_n):CAS64(_x,_o,_n))
#define FAS(_x,_n)    ((sizeof (*_x) == 4)?FAS32(_x,_n)   :FAS64(_x,_n))

/* Update Integer location, return Old value. */
#define CASIO CAS
#define FASIO FAS
/* Update Pointer location, return Old value. */
#define CASPO CAS64
#define FASPO FAS64
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

#define MB()  __asm__ __volatile__("" : : : "memory")
#define WMB() MB()
#define RMB() MB()
#define VOLATILE		/*volatile */

/* On Intel, CAS is a strong barrier, but not a compile barrier. */
#define RMB_NEAR_CAS() WMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS()  WMB()


/*
 * III. Cycle counter access.
 */

typedef unsigned long long tick_t;

#define RDTICK() \
  ({ unsigned __a, __d; tick_t __t; \
    __asm__ __volatile__ ("rdtsc" : "=a" (__a), "=d" (__d)); \
    __t=((unsigned long long) __a) | (((unsigned long long) __d) << 32); \
    __t; })


/*
 * IV. Types.
 */

typedef unsigned char _u8;
typedef unsigned short _u16;
typedef unsigned int _u32;
typedef unsigned long long _u64;

#endif /* __INTEL_DEFNS_H__ */
