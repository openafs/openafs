/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_REFCNT_H
#define _OSI_COMMON_REFCNT_H 1

/*
 * implementation of reference counting
 * using atomic operations
 */

#include <osi/osi_atomic.h>


#if defined(OSI_IMPLEMENTS_ATOMIC_INC_16_NV) && defined(OSI_IMPLEMENTS_ATOMIC_DEC_16_NV)
#define OSI_IMPLEMENTS_REFCNT 1
#define OSI_IMPLEMENTS_REFCNT_ATOMIC 1
#define _osi_refcnt_inc osi_atomic_inc_16
#define _osi_refcnt_dec osi_atomic_dec_16
#define _osi_refcnt_inc_nv osi_atomic_inc_16_nv
#define _osi_refcnt_dec_nv osi_atomic_dec_16_nv
typedef osi_uint16 osi_volatile osi_refcnt_t;
typedef osi_uint16 osi_refcnt_val_t;
#elif defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV) && defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
#define OSI_IMPLEMENTS_REFCNT 1
#define OSI_IMPLEMENTS_REFCNT_ATOMIC 1
#define _osi_refcnt_inc osi_atomic_inc_32
#define _osi_refcnt_dec osi_atomic_dec_32
#define _osi_refcnt_inc_nv osi_atomic_inc_32_nv
#define _osi_refcnt_dec_nv osi_atomic_inc_32_nv
typedef osi_uint32 osi_volatile osi_refcnt_t;
typedef osi_uint32 osi_refcnt_val_t;
#endif


#if defined(OSI_IMPLEMENTS_REFCNT_ATOMIC)

#define osi_refcnt_init(refcnt, val)  (*(refcnt) = (val))
#define osi_refcnt_reset(refcnt, val)  (*(refcnt) = (val))
#define osi_refcnt_destroy(refcnt)
#define osi_refcnt_inc(refcnt) \
    _osi_refcnt_inc(refcnt)
#define osi_refcnt_dec(refcnt) \
    _osi_refcnt_dec(refcnt)

#endif /* OSI_IMPLEMENTS_REFCNT_ATOMIC */

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/refcnt_inline.h>
#endif

#endif /* _OSI_COMMON_REFCNT_H */
