/*
 * Copyright (c) 2010 Your Filesystem Inc. All rights reserved.
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

#ifndef OPENAFS_RX_ATOMIC_H
#define OPENAFS_RX_ATOMIC_H 1

#define RX_ATOMIC_INIT(i) { (i) }

#ifdef AFS_NT40_ENV
typedef struct {
    volatile int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    atomic->var = val;
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    return atomic->var;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
    InterlockedIncrement(&atomic->var);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    return InterlockedIncrement(&atomic->var);
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    InterlockedExchangeAdd(&atomic->var, change);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    return InterlockedExchangeAdd(&atomic->var, change) + change;
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    InterlockedDecrement(&atomic->var);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    return InterlockedDecrement(&atomic->var);
}

static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    InterlockedExchangeAdd(&atomic->var, 0 - change);
}

#elif defined(AFS_AIX61_ENV) || defined(AFS_USR_AIX61_ENV)
#include <sys/atomic_op.h>

typedef struct {
   volatile int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    atomic->var = val;
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    return atomic->var;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
    fetch_and_add(&atomic->var, 1);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    return (fetch_and_add(&atomic->var, 1) + 1);
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    fetch_and_add(&atomic->var, change);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    return (fetch_and_add(&atomic->var, change) + change);
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    fetch_and_add(&atomic->var, -1);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    return (fetch_and_add(&atomic->var, -1) - 1);
}

static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    fetch_and_add(&atomic->var, -change);
}

#elif defined(AFS_DARWIN80_ENV) || defined(AFS_USR_DARWIN80_ENV)

# if (defined(AFS_DARWIN160_ENV) || defined(AFS_USR_DARWIN160_ENV)) && !defined(KERNEL)
#  define OSATOMIC_USE_INLINED 1
# endif

# include <libkern/OSAtomic.h>

# if defined(KERNEL) && !defined(UKERNEL)
static_inline int
OSAtomicIncrement32(volatile int *value)
{
    return OSIncrementAtomic(value) + 1;
}

static_inline int
OSAtomicAdd32(int amount, volatile int *value)
{
    return OSAddAtomic(amount, value) + amount;
}

static_inline int
OSAtomicDecrement32(volatile int *value)
{
    return OSDecrementAtomic(value) - 1;
}

# endif

typedef struct {
    volatile int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    atomic->var = val;
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    return atomic->var;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
    OSAtomicIncrement32(&atomic->var);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    return OSAtomicIncrement32(&atomic->var);
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    OSAtomicAdd32(change, &atomic->var);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    return OSAtomicAdd32(change, &atomic->var);
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    OSAtomicDecrement32(&atomic->var);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    return OSAtomicDecrement32(&atomic->var);
}

static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    OSAtomicAdd32(0 - change, &atomic->var);
}
#elif defined(AFS_LINUX_ENV) && defined(KERNEL)
#include <asm/atomic.h>

typedef atomic_t rx_atomic_t;

#define rx_atomic_set(X, V)	  atomic_set(X, V)
#define rx_atomic_read(X)	  atomic_read(X)
#define rx_atomic_inc(X)	  atomic_inc(X)
#define rx_atomic_inc_and_read(X) atomic_inc_return(X)
#define rx_atomic_add(X, V)	  atomic_add(V, X)
#define rx_atomic_add_and_read(X, V) atomic_add_return(V, X)
#define rx_atomic_dec(X)	  atomic_dec(X)
#define rx_atomic_dec_and_read(X) atomic_dec_return(X)
#define rx_atomic_sub(X, V)	  atomic_sub(V, X)

#elif defined(AFS_SUN510_ENV) || (defined(AFS_SUN5_ENV) && defined(KERNEL) && !defined(UKERNEL))

# if defined(KERNEL) && !defined(UKERNEL)
#  include <sys/atomic.h>
# else
#  include <atomic.h>
# endif

#ifndef AFS_SUN510_ENV
# define atomic_inc_32(X)    atomic_add_32((X), 1)
# define atomic_inc_32_nv(X) atomic_add_32_nv((X), 1)
# define atomic_dec_32(X)    atomic_add_32((X), -1)
# define atomic_dec_32_nv(X) atomic_add_32_nv((X), -1)
#endif

typedef struct {
    volatile unsigned int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    atomic->var = val;
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    return atomic->var;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
    atomic_inc_32(&atomic->var);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    return atomic_inc_32_nv(&atomic->var);
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    atomic_add_32(&atomic->var, change);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    return atomic_add_32_nv(&atomic->var, change);
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    atomic_dec_32(&atomic->var);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    return atomic_dec_32_nv(&atomic->var);
}

static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    atomic_add_32(&atomic->var, 0 - change);
}

#elif defined(__GNUC__) && defined(HAVE_SYNC_FETCH_AND_ADD)

typedef struct {
   volatile int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    atomic->var = val;
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    return atomic->var;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
    (void)__sync_fetch_and_add(&atomic->var, 1);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    return __sync_add_and_fetch(&atomic->var, 1);
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    (void)__sync_fetch_and_add(&atomic->var, change);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    return __sync_fetch_and_add(&atomic->var, change);
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    (void)__sync_fetch_and_sub(&atomic->var, 1);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    return __sync_sub_and_fetch(&atomic->var, 1);
}

static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    (void)__sync_fetch_and_sub(&atomic->var, change);
}

#else

/* If we're on a platform where we have no idea how to do atomics,
 * then we fall back to using a single process wide mutex to protect
 * all atomic variables. This won't be the quickest thing ever.
 */

#ifdef RX_ENABLE_LOCKS
extern afs_kmutex_t rx_atomic_mutex;
#endif

typedef struct {
    int var;
} rx_atomic_t;

static_inline void
rx_atomic_set(rx_atomic_t *atomic, int val) {
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var = val;
    MUTEX_EXIT(&rx_atomic_mutex);
}

static_inline int
rx_atomic_read(rx_atomic_t *atomic) {
    int out;

    MUTEX_ENTER(&rx_atomic_mutex);
    out = atomic->var;
    MUTEX_EXIT(&rx_atomic_mutex);

    return out;
}

static_inline void
rx_atomic_inc(rx_atomic_t *atomic) {
   MUTEX_ENTER(&rx_atomic_mutex);
   atomic->var++;
   MUTEX_EXIT(&rx_atomic_mutex);
}

static_inline int
rx_atomic_inc_and_read(rx_atomic_t *atomic) {
    int retval;
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var++;
    retval = atomic->var;
    MUTEX_EXIT(&rx_atomic_mutex);
    return retval;
}

static_inline void
rx_atomic_add(rx_atomic_t *atomic, int change) {
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var += change;
    MUTEX_EXIT(&rx_atomic_mutex);
}

static_inline int
rx_atomic_add_and_read(rx_atomic_t *atomic, int change) {
    int retval;

    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var += change;
    retval = atomic->var;
    MUTEX_EXIT(&rx_atomic_mutex);

    return retval;
}

static_inline void
rx_atomic_dec(rx_atomic_t *atomic) {
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var--;
    MUTEX_EXIT(&rx_atomic_mutex);
}

static_inline int
rx_atomic_dec_and_read(rx_atomic_t *atomic) {
    int retval;
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var--;
    retval = atomic->var;
    MUTEX_EXIT(&rx_atomic_mutex);
    return retval;
}


static_inline void
rx_atomic_sub(rx_atomic_t *atomic, int change) {
    MUTEX_ENTER(&rx_atomic_mutex);
    atomic->var -= change;
    MUTEX_EXIT(&rx_atomic_mutex);
}

#endif

#endif
