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

#ifdef AFS_NT40_ENV
#ifndef _WIN64
#ifndef __cplusplus
#include <intrin.h>
#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedAnd)
#define rx_MutexOr(object, operand, mutex) _InterlockedOr(&object, operand)
#define rx_MutexAnd(object, operand, mutex) _InterlockedAnd(&object, operand)
#endif
#else
#define rx_MutexOr(object, operand, mutex) InterlockedOr(&object, operand)
#define rx_MutexAnd(object, operand, mutex) InterlockedAnd(&object, operand)
#endif
#define rx_MutexIncrement(object, mutex) InterlockedIncrement(&object)
#define rx_MutexXor(object, operand, mutex) InterlockedXor(&object, operand)
#define rx_MutexAdd(object, addend, mutex) InterlockedExchangeAdd(&object, addend)
#define rx_MutexDecrement(object, mutex) InterlockedDecrement(&object)
#define rx_MutexAdd1Increment2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        InterlockedIncrement(&object2); \
        MUTEX_EXIT(&mutex); \
    } while (0)
#define rx_MutexAdd1Decrement2(object1, addend, object2, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object1 += addend; \
        InterlockedDecrement(&object2); \
        MUTEX_EXIT(&mutex); \
    } while (0)
#else
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
#define rx_MutexDecrement(object, mutex) \
    do { \
        MUTEX_ENTER(&mutex); \
        object--; \
        MUTEX_EXIT(&mutex); \
    } while(0)
#endif 

#endif /* _RX_INTERNAL_H */
