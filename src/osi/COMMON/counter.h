/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_COUNTER_H
#define _OSI_COMMON_COUNTER_H 1

/*
 * implementation of a counter
 * using atomic operations
 */

#include <osi/osi_atomic.h>

/* osi_counter_t atomic support */
#if defined(OSI_ENV_NATIVE_INT64_TYPE) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
#define OSI_IMPLEMENTS_COUNTER 1
#define OSI_IMPLEMENTS_COUNTER_ATOMIC 1
#define OSI_COUNTER_BITS 64
#define _osi_counter_inc osi_atomic_inc_64
#define _osi_counter_inc_nv osi_atomic_inc_64_nv
#define _osi_counter_add osi_atomic_add_64
#define _osi_counter_add_nv osi_atomic_add_64_nv
typedef osi_uint64 osi_volatile osi_counter_t;
typedef osi_uint64 osi_counter_val_t;
typedef osi_int64 osi_counter_delta_t;
#elif defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
#define OSI_IMPLEMENTS_COUNTER 1
#define OSI_IMPLEMENTS_COUNTER_ATOMIC 1
#define OSI_COUNTER_BITS 32
#define _osi_counter_inc osi_atomic_inc_32
#define _osi_counter_inc_nv osi_atomic_inc_32_nv
#define _osi_counter_add osi_atomic_add_32
#define _osi_counter_add_nv osi_atomic_add_32_nv
typedef osi_uint32 osi_volatile osi_counter_t;
typedef osi_uint32 osi_counter_val_t;
typedef osi_int32 osi_counter_delta_t;
#endif

/* osi_counter16_t support */
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV)
#define OSI_IMPLEMENTS_COUNTER16 1
#define OSI_IMPLEMENTS_COUNTER16_ATOMIC 1
#define _osi_counter16_inc osi_atomic_inc_16
#define _osi_counter16_inc_nv osi_atomic_inc_16_nv
#define _osi_counter16_add osi_atomic_add_16
#define _osi_counter16_add_nv osi_atomic_add_16_nv
typedef osi_uint16 osi_volatile osi_counter16_t;
typedef osi_uint16 osi_counter16_val_t;
typedef osi_int16 osi_counter16_delta_t;
#endif

/* osi_counter32_t support */
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
#define OSI_IMPLEMENTS_COUNTER32 1
#define OSI_IMPLEMENTS_COUNTER32_ATOMIC 1
#define _osi_counter32_inc osi_atomic_inc_32
#define _osi_counter32_inc_nv osi_atomic_inc_32_nv
#define _osi_counter32_add osi_atomic_add_32
#define _osi_counter32_add_nv osi_atomic_add_32_nv
typedef osi_uint32 osi_volatile osi_counter32_t;
typedef osi_uint32 osi_counter32_val_t;
typedef osi_int32 osi_counter32_delta_t;
#endif

/* osi_counter64_t support */
#if defined(OSI_ENV_NATIVE_INT64_TYPE) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
#define OSI_IMPLEMENTS_COUNTER64 1
#define OSI_IMPLEMENTS_COUNTER64_ATOMIC 1
#define _osi_counter64_inc osi_atomic_inc_64
#define _osi_counter64_inc_nv osi_atomic_inc_64_nv
#define _osi_counter64_add osi_atomic_add_64
#define _osi_counter64_add_nv osi_atomic_add_64_nv
typedef osi_uint64 osi_volatile osi_counter64_t;
typedef osi_uint64 osi_counter64_val_t;
typedef osi_int64 osi_counter64_delta_t;
#endif /* OSI_ENV_NATIVE_INT64_TYPE && OSI_IMPLEMENTS_ATOMIC_ADD_64_NV */


#if defined(OSI_IMPLEMENTS_COUNTER_ATOMIC)
#define osi_counter_inc(counter) \
    _osi_counter_inc(counter)
#define osi_counter_add(counter, inc) \
    _osi_counter_add(counter, inc)

osi_extern void osi_counter_init(osi_counter_t *, osi_counter_val_t);
osi_extern void osi_counter_reset(osi_counter_t *, osi_counter_val_t);
osi_extern void osi_counter_destroy(osi_counter_t *);
#endif /* OSI_IMPLEMENTS_COUNTER_ATOMIC */

#if defined(OSI_IMPLEMENTS_COUNTER16_ATOMIC)
#define osi_counter16_inc(counter) \
    _osi_counter16_inc(counter)
#define osi_counter16_add(counter, inc) \
    _osi_counter16_add(counter, inc)

osi_extern void osi_counter16_init(osi_counter16_t *, osi_counter16_val_t);
osi_extern void osi_counter16_reset(osi_counter16_t *, osi_counter16_val_t);
osi_extern void osi_counter16_destroy(osi_counter16_t *);
#endif /* OSI_IMPLEMENTS_COUNTER16_ATOMIC */

#if defined(OSI_IMPLEMENTS_COUNTER32_ATOMIC)
#define osi_counter32_inc(counter) \
    _osi_counter32_inc(counter)
#define osi_counter32_add(counter, inc) \
    _osi_counter32_add(counter, inc)

osi_extern void osi_counter32_init(osi_counter32_t *, osi_counter32_val_t);
osi_extern void osi_counter32_reset(osi_counter32_t *, osi_counter32_val_t);
osi_extern void osi_counter32_destroy(osi_counter32_t *);
#endif /* OSI_IMPLEMENTS_COUNTER32_ATOMIC */

#if defined(OSI_IMPLEMENTS_COUNTER64_ATOMIC)
#define osi_counter64_inc(counter) \
    _osi_counter64_inc(counter)
#define osi_counter64_add(counter, inc) \
    _osi_counter64_add(counter, inc)

osi_extern void osi_counter64_init(osi_counter64_t *, osi_counter64_val_t);
osi_extern void osi_counter64_reset(osi_counter64_t *, osi_counter64_val_t);
osi_extern void osi_counter64_destroy(osi_counter64_t *);
#endif /* OSI_IMPLEMENTS_COUNTER64_ATOMIC */


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/counter_inline.h>
#endif

#endif /* _OSI_COMMON_COUNTER_H */
