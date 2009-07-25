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

#ifndef __ALPHA_DEFNS_H__
#define __ALPHA_DEFNS_H__

#include <c_asm.h>
#include <alpha/builtins.h>
#include <pthread.h>

#ifndef ALPHA
#define ALPHA
#endif

#define CACHE_LINE_SIZE 64


/*
 * I. Compare-and-swap, fetch-and-store.
 */

#define FAS32(_x,_n)  asm ( \
                "1:     ldl_l   %v0, 0(%a0);" \
                "       bis     %a1, 0, %t0;" \
                "       stl_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;", (_x), (_n))
#define FAS64(_x,_n)  asm ( \
                "1:     ldq_l   %v0, 0(%a0);" \
                "       bis     %a1, 0, %t0;" \
                "       stq_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;", (_x), (_n))
#define CAS32(_x,_o,_n)  asm ( \
                "1:     ldl_l   %v0, 0(%a0);" \
                "       cmpeq   %v0, %a1, %t0;" \
                "       beq     %t0, 3f;" \
                "       bis     %a2, 0, %t0;" \
                "       stl_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;" \
                "3:", (_x), (_o), (_n))
#define CAS64(_x,_o,_n)  asm ( \
                "1:     ldq_l   %v0, 0(%a0);" \
                "       cmpeq   %v0, %a1, %t0;" \
                "       beq     %t0, 3f;" \
                "       bis     %a2, 0, %t0;" \
                "       stq_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;" \
                "3:", (_x), (_o), (_n))
#define CAS(_x,_o,_n) ((sizeof (*_x) == 4)?CAS32(_x,_o,_n):CAS64(_x,_o,_n))
#define FAS(_x,_n)    ((sizeof (*_x) == 4)?FAS32(_x,_n)   :FAS64(_x,_n))
/* Update Integer location, return Old value. */
#define CASIO(_x,_o,_n) CAS(_x,_o,_n)
#define FASIO(_x,_n)    FAS(_x,_n)
/* Update Pointer location, return Old value. */
#define CASPO(_x,_o,_n) (void*)CAS((_x),(void*)(_o),(void*)(_n))
#define FASPO(_x,_n)    (void*)FAS((_x),(void*)(_n))
#define CAS32O CAS32
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

#define MB()  asm("mb")
#define WMB() asm("wmb")
#define RMB() (MB())
#define VOLATILE /*volatile*/


/*
 * III. Cycle counter access.
 */

#include <sys/time.h>
typedef unsigned long tick_t;
#define RDTICK() asm("rpcc %v0")


/*
 * IV. Types.
 */

typedef unsigned char  _u8;
typedef unsigned short _u16;
typedef unsigned int   _u32;
typedef unsigned long  _u64;

#endif /* __ALPHA_DEFNS_H__ */
