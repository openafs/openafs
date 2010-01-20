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

#ifndef __PPC_DEFNS_H__
#define __PPC_DEFNS_H__

#ifndef PPC
#define PPC
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sched.h>

#define CACHE_LINE_SIZE 64

#include <pthread.h>


/*
 * I. Compare-and-swap.
 */

static int FAS32(void *, int);
static long FAS64(void *, long);
static int CAS32(void *, int, int);
static long CAS64(void *, long, long);

#pragma mc_func FAS32 {               \
    "7c001828" /* 1: lwarx r0,0,r3 */ \
    "7c80192d" /*    stwcx r4,0,r3 */ \
    "4082fff8" /*    bne   1       */ \
    "60030000" /*    ori   r3,0,r0 */ \
}

#pragma mc_func FAS64 {               \
    "7c0018a8" /* 1: ldarx r0,0,r3 */ \
    "7c8019ad" /*    stdcx r4,0,r3 */ \
    "4082fff8" /*    bne   1       */ \
    "60030000" /*    ori   r3,0,r0 */ \
}

#pragma mc_func CAS32 {               \
    "7c001828" /* 1: lwarx r0,0,r3 */ \
    "7c002000" /*    cmpw  r0,r4   */ \
    "4082000c" /*    bne   2       */ \
    "7ca0192d" /*    stwcx r5,0,r3 */ \
    "4082fff0" /*    bne   1       */ \
    "60030000" /* 2: ori   r3,0,r0 */ \
}

#pragma mc_func CAS64 {               \
    "7c0018a8" /* 1: ldarx r0,0,r3 */ \
    "7c202000" /*    cmpd  r0,r4   */ \
    "4082000c" /*    bne   2       */ \
    "7ca019ad" /*    stdcx r5,0,r3 */ \
    "4082fff0" /*    bne   1       */ \
    "60030000" /* 2: ori   r3,0,r0 */ \
}

#define CASIO(_a,_o,_n) ((int)CAS32((int*)(_a),(int)(_o),(int)(_n)))
#define FASIO(_a,_n)    ((int)FAS32((int*)(_a),(int)(_n)))
#define CASPO(_a,_o,_n) ((void *)(CAS64((long*)(_a),(long)(_o),(long)(_n))))
#define FASPO(_a,_n)    ((void *)(FAS64((long*)(_a),(long)(_n))))
#define CAS32O(_a,_o,_n) ((_u32)(CAS32((_u32*)(_a),(_u32)(_o),(_u32)(_n))))
#define CAS64O(_a,_o,_n) ((_u64)(CAS64((long*)(_a),(long)(_o),(long)(_n))))


/*
 * II. Memory barriers.
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 *
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */

static void WMB(void);
static void RMB(void);
static void MB(void);

#pragma mc_func WMB { "7c0004ac" } /* msync (orders memory transactions) */
#pragma mc_func RMB { "4c00012c" } /* isync (orders instruction issue)   */
#pragma mc_func MB  { "7c0004ac" } /* msync (orders memory transactions) */

#define VOLATILE /*volatile*/


/*
 * III. Cycle counter access.
 */

typedef unsigned long tick_t;
static tick_t RDTICK(void);
#pragma mc_func RDTICK { "7c6c42e6" } /* mftb r3 */


/*
 * IV. Types.
 */

typedef unsigned char      _u8;
typedef unsigned short     _u16;
typedef unsigned int       _u32;
typedef unsigned long      _u64;

#endif /* __PPC_DEFNS_H__ */
