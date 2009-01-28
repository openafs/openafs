/*
 * Copyright (c) 2008, Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the company nor the names of its contributors may 
 *    be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_RX_INTERNAL_H_
#define _RX_INTERNAL_H_

#ifdef AFS_DARWIN80_ENV
#include <libkern/OSAtomic.h>
#endif
#ifdef AFS_SUN58_ENV
#include <sys/atomic.h>
#endif

#ifdef AFS_NT40_ENV
#ifndef _WIN64
#ifndef __cplusplus
#include <intrin.h>
#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedAnd)
#define rx_AtomicOr(object, operand, mutex) _InterlockedOr(&object, operand)
#define rx_AtomicAnd(object, operand, mutex) _InterlockedAnd(&object, operand)
#endif /* __cplusplus */
#else /* !WIN64 */
#define rx_AtomicOr(object, operand, mutex) InterlockedOr(&object, operand)
#define rx_AtomicAnd(object, operand, mutex) InterlockedAnd(&object, operand)
#endif /* WIN64 */
#define rx_AtomicIncrement_NL(object) InterlockedIncrement(&object)
#define rx_AtomicIncrement(object, mutex) InterlockedIncrement(&object)
#define rx_AtomicXor(object, operand, mutex) InterlockedXor(&object, operand)
#define rx_AtomicAdd_NL(object, addend) InterlockedExchangeAdd(&object, addend)
#define rx_AtomicAdd(object, addend, mutex) InterlockedExchangeAdd(&object, addend)
#define rx_AtomicDecrement_NL(object) InterlockedDecrement(&object)
#define rx_AtomicDecrement(object, mutex) InterlockedDecrement(&object)
#define rx_AtomicSwap_NL(object1, object2) InterlockedExchange ((volatile LONG *) object1, object2);
#define rx_AtomicSwap(object1, object2, mutex) InterlockedExchange ((volatile LONG *) object1, object2);
#elif defined(AFS_DARWIN80_ENV)
#define rx_AtomicIncrement_NL(object) OSAtomicIncrement32(&object)
#define rx_AtomicIncrement(object, mutex) OSAtomicIncrement32(&object)
#define rx_AtomicOr(object, operand, mutex) OSAtomicOr32(operand, &object)
#define rx_AtomicAnd(object, operand, mutex) OSAtomicAnd32(operand, &object)
#define rx_AtomicXor(object, operand, mutex) OSAtomicXor32(operand, &object)
#define rx_AtomicAdd_NL(object, addend) OSAtomicAdd32(addend, &object)
#define rx_AtomicAdd(object, addend, mutex) OSAtomicAdd32(addend, &object)
#define rx_AtomicDecrement_NL(object) OSAtomicDecrement32(&object)
#define rx_AtomicDecrement(object, mutex) OSAtomicDecrement32(&object)
#define rx_AtomicSwap_NL(oldval, newval) rx_AtomicSwap_int(oldval, newval)
#define rx_AtomicSwap(oldval, newval, mutex) rx_AtomicSwap_int(oldval, newval)
static inline afs_int32 rx_AtomicSwap_int(afs_int32 *oldval, afs_int32 newval) {
    afs_int32 ret = *oldval;
    OSAtomicCompareAndSwap32 ((afs_int32) *oldval,(afs_int32) newval,
			      (afs_int32*) oldval);
    return ret;
}
#elif defined(AFS_SUN58_ENV)
#define rx_AtomicIncrement_NL(object) atomic_inc_32(&object)
#define rx_AtomicIncrement(object, mutex) atomic_inc_32(&object)
#define rx_AtomicOr(object, operand, mutex) atomic_or_32(&object, operand)
#define rx_AtomicAnd(object, operand, mutex) atomic_and_32(&object, operand)
#define rx_AtomicAdd_NL(object, addend) atomic_add_32(&object, addend)
#define rx_AtomicAdd(object, addend, mutex) atomic_add_32(&object, addend)
#define rx_AtomicDecrement_NL(object) atomic_dec_32(&object)
#define rx_AtomicDecrement(object, mutex) atomic_dec_32(&object)
#define rx_AtomicSwap_NL(oldval, newval) rx_AtomicSwap_int(oldval, newval)
#define rx_AtomicSwap(oldval, newval, mutex) rx_AtomicSwap_int(oldval, newval)
static inline afs_int32 rx_AtomicSwap_int(afs_int32 *oldval, afs_int32 newval) {
    afs_int32 ret = *oldval;
    atomic_cas_32((afs_int32) *oldval,(afs_int32) newval,
		  (afs_int32*) oldval);
    return ret;
}
#else
#define rx_AtomicIncrement_NL(object) (object)++
#define rx_AtomicIncrement(object, mutex) rx_MutexIncrement(object, mutex)
#define rx_AtomicOr(object, operand, mutex) rx_MutexOr(object, operand, mutex)
#define rx_AtomicAnd(object, operand, mutex) rx_MutexAnd(object, operand, mutex)
#define rx_AtomicAdd_NL(object, addend) object += addend
#define rx_AtomicAdd(object, addend, mutex) rx_MutexAdd(object, addand, mutex)
#define rx_AtomicDecrement_NL(object) (object)--
#define rx_AtomicDecrement(object, mutex) rx_MutexDecrement(object, mutex)
#define rx_AtomicSwap_NL(oldval, newval) rx_AtomicSwap_int(oldval, newval)
#define rx_AtomicSwap(oldval, newval, mutex) rx_AtomicSwap_int(oldval, newval)
static inline afs_int32 rx_AtomicSwap_int(afs_int32 *oldval, afs_int32 newval) {
    afs_int32 ret = *oldval;
    *oldval = newval;
    return ret;
}
#endif
#define rx_AtomicPeek_NL(object) rx_AtomicAdd_NL(object, 0)
#define rx_AtomicPeek(object, mutex) rx_AtomicAdd(object, 0, mutex)
#define rx_MutexIncrement(object, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object++; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexOr(object, operand, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object |= operand; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexAnd(object, operand, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object &= operand; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexXor(object, operand, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object ^= operand; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexAdd(object, addend, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object += addend; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexDecrement(object, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object--; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexAdd1Increment2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        object2++; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#define rx_MutexAdd1Decrement2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        object2--; \
        MUTEX_EXIT(&mutex); \
    } while(0)

#define rx_MutexAdd1AtomicIncrement2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        rx_AtomicIncrement(&object2); \
        MUTEX_EXIT(&mutex); \
    } while (0)
#define rx_MutexAdd1AtomicDecrement2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        rx_AtomicDecrement(&object2); \
        MUTEX_EXIT(&mutex); \
    } while (0)
#endif /* _RX_INTERNAL_H */
