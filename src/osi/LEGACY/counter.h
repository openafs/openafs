/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_COUNTER_H
#define	_OSI_LEGACY_COUNTER_H

/*
 * implementation of a counter
 * using spinlocks
 */

#include <osi/osi_spinlock.h>

#if !defined(OSI_IMPLEMENTS_COUNTER_ATOMIC)

#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define OSI_IMPLEMENTS_COUNTER 1
#define OSI_IMPLEMENTS_LEGACY_COUNTER 1
#define OSI_COUNTER_BITS 64
typedef osi_uint64 osi_counter_val_t;
typedef osi_int64 osi_counter_delta_t;
typedef struct {
    osi_counter_val_t osi_volatile counter;
    osi_spinlock_t lock;
} osi_counter_t;
#else /* !OSI_LP64_ENV */
#define OSI_IMPLEMENTS_COUNTER 1
#define OSI_IMPLEMENTS_LEGACY_COUNTER 1
#define OSI_COUNTER_BITS 32
typedef osi_uint32 osi_counter_val_t;
typedef osi_int32 osi_counter_delta_t;
typedef struct {
    osi_counter_val_t osi_volatile counter;
    osi_spinlock_t lock;
} osi_counter_t;
#endif /* !OSI_LP64_ENV */

osi_extern void osi_counter_init(osi_counter_t *, osi_counter_val_t);
osi_extern void osi_counter_reset(osi_counter_t *, osi_counter_val_t);
osi_extern void osi_counter_destroy(osi_counter_t *);

#endif /* !OSI_IMPLEMENTS_COUNTER_ATOMIC */


#if !defined(OSI_IMPLEMENTS_COUNTER16_ATOMIC)
#define OSI_IMPLEMENTS_COUNTER16 1
#define OSI_IMPLEMENTS_LEGACY_COUNTER16 1
typedef osi_uint16 osi_counter16_val_t;
typedef osi_int16 osi_counter16_delta_t;
typedef struct {
    osi_counter16_val_t osi_volatile counter;
    osi_spinlock_t lock;
} osi_counter16_t;

osi_extern void osi_counter16_init(osi_counter16_t *, osi_counter16_val_t);
osi_extern void osi_counter16_reset(osi_counter16_t *, osi_counter16_val_t);
osi_extern void osi_counter16_destroy(osi_counter16_t *);
#endif /* !OSI_IMPLEMENTS_COUNTER16_ATOMIC */


#if !defined(OSI_IMPLEMENTS_COUNTER32_ATOMIC)
#define OSI_IMPLEMENTS_COUNTER32 1
#define OSI_IMPLEMENTS_LEGACY_COUNTER32 1
typedef osi_uint32 osi_counter32_val_t;
typedef osi_int32 osi_counter32_delta_t;
typedef struct {
    osi_counter32_val_t osi_volatile counter;
    osi_spinlock_t lock;
} osi_counter32_t;

osi_extern void osi_counter32_init(osi_counter32_t *, osi_counter32_val_t);
osi_extern void osi_counter32_reset(osi_counter32_t *, osi_counter32_val_t);
osi_extern void osi_counter32_destroy(osi_counter32_t *);
#endif /* !OSI_IMPLEMENTS_COUNTER32_ATOMIC */


#if defined(OSI_ENV_NATIVE_INT64_TYPE) && !defined(OSI_IMPLEMENTS_COUNTER64_ATOMIC)
#define OSI_IMPLEMENTS_COUNTER64 1
#define OSI_IMPLEMENTS_LEGACY_COUNTER64 1
typedef osi_uint64 osi_counter64_val_t;
typedef osi_int64 osi_counter64_delta_t;
typedef struct {
    osi_counter64_val_t osi_volatile counter;
    osi_spinlock_t lock;
} osi_counter64_t;

osi_extern void osi_counter64_init(osi_counter64_t *, osi_counter64_val_t);
osi_extern void osi_counter64_reset(osi_counter64_t *, osi_counter64_val_t);
osi_extern void osi_counter64_destroy(osi_counter64_t *);
#endif /* !OSI_IMPLEMENTS_COUNTER64_ATOMIC */


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LEGACY/counter_inline.h>
#endif

#endif /* _OSI_LEGACY_COUNTER_H */
