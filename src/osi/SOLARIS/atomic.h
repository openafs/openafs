/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_ATOMIC_H
#define	_OSI_SOLARIS_ATOMIC_H


#if defined(OSI_ENV_KERNELSPACE)
#include <sys/atomic.h>
#else
#include <atomic.h>
#endif

/*
 * XXX for now assume complete parity between userspace and
 * kernelspace atomic ops implementations
 *
 * for old solaris express releases we know this is not true.
 * however, the way we test for things, is conservative --
 * solaris always seems to support atomic ops first in the kernel
 * and then port them to userspace when time permits.  we test
 * the userspace case, so we may just miss ops that are actually there
 */

typedef osi_uint8 osi_atomic8_t;
typedef osi_uint8 osi_atomic8_val_t;
typedef osi_int8  osi_atomic8_delta_t;

typedef osi_uint16 osi_atomic16_t;
typedef osi_uint16 osi_atomic16_val_t;
typedef osi_int16  osi_atomic16_delta_t;

typedef osi_uint32 osi_atomic32_t;
typedef osi_uint32 osi_atomic32_val_t;
typedef osi_int32  osi_atomic32_delta_t;

typedef osi_uint64 osi_atomic64_t;
typedef osi_uint64 osi_atomic64_val_t;
typedef osi_int64  osi_atomic64_delta_t;


#if (HAVE_ATOMIC_INC_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_8			1
#define osi_atomic_inc_8(x) atomic_inc_8(x)
#endif
#if (HAVE_ATOMIC_DEC_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_8			1
#define osi_atomic_dec_8(x) atomic_dec_8(x)
#endif
#if (HAVE_ATOMIC_ADD_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_8			1
#define osi_atomic_add_8(x, y) atomic_add_8(x, y)
#endif
#if (HAVE_ATOMIC_OR_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_8			1
#define osi_atomic_or_8(x, y) atomic_or_8(x, y)
#endif
#if (HAVE_ATOMIC_AND_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_8			1
#define osi_atomic_and_8(x, y) atomic_and_8(x, y)
#endif
#if (HAVE_ATOMIC_INC_8_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_8_NV			1
#define osi_atomic_inc_8_nv(x) atomic_inc_8_nv(x)
#endif
#if (HAVE_ATOMIC_DEC_8_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_8_NV			1
#define osi_atomic_dec_8_nv(x) atomic_dec_8_nv(x)
#endif
#if (HAVE_ATOMIC_ADD_8_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_8_NV			1
#define osi_atomic_add_8_nv(x, y) atomic_add_8_nv(x, y)
#endif
#if (HAVE_ATOMIC_OR_8_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_8_NV			1
#define osi_atomic_or_8_nv(x, y) atomic_or_8_nv(x, y)
#endif
#if (HAVE_ATOMIC_AND_8_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_8_NV			1
#define osi_atomic_and_8_nv(x, y) atomic_and_8_nv(x, y)
#endif
#if (HAVE_ATOMIC_CAS_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_CAS_8			1
#define osi_atomic_cas_8(x, y, z) atomic_cas_8(x, y, z)
#endif
#if (HAVE_ATOMIC_SWAP_8 == 1)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_8			1
#define osi_atomic_swap_8(x, y) atomic_swap_8(x, y)
#endif

#if (HAVE_ATOMIC_INC_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_16			1
#define osi_atomic_inc_16(x) atomic_inc_16(x)
#endif
#if (HAVE_ATOMIC_DEC_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_16			1
#define osi_atomic_dec_16(x) atomic_dec_16(x)
#endif
#if (HAVE_ATOMIC_ADD_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_16			1
#define osi_atomic_add_16(x, y) atomic_add_16(x, y)
#endif
#if (HAVE_ATOMIC_OR_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_16			1
#define osi_atomic_or_16(x, y) atomic_or_16(x, y)
#endif
#if (HAVE_ATOMIC_AND_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_16			1
#define osi_atomic_and_16(x, y) atomic_and_16(x, y)
#endif
#if (HAVE_ATOMIC_INC_16_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_16_NV			1
#define osi_atomic_inc_16_nv(x) atomic_inc_16_nv(x)
#endif
#if (HAVE_ATOMIC_DEC_16_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_16_NV			1
#define osi_atomic_dec_16_nv(x) atomic_dec_16_nv(x)
#endif
#if (HAVE_ATOMIC_ADD_16_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_16_NV			1
#define osi_atomic_add_16_nv(x, y) atomic_add_16_nv(x, y)
#endif
#if (HAVE_ATOMIC_OR_16_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_16_NV			1
#define osi_atomic_or_16_nv(x, y) atomic_or_16_nv(x, y)
#endif
#if (HAVE_ATOMIC_AND_16_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_16_NV			1
#define osi_atomic_and_16_nv(x, y) atomic_and_16_nv(x, y)
#endif
#if (HAVE_ATOMIC_CAS_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_CAS_16			1
#define osi_atomic_cas_16(x, y, z) atomic_cas_16(x, y, z)
#endif
#if (HAVE_ATOMIC_SWAP_16 == 1)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_16			1
#define osi_atomic_swap_16(x, y) atomic_swap_16(x, y)
#endif

#if (HAVE_ATOMIC_INC_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_32			1
#define osi_atomic_inc_32(x) atomic_inc_32(x)
#endif
#if (HAVE_ATOMIC_DEC_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_32			1
#define osi_atomic_dec_32(x) atomic_dec_32(x)
#endif
#if (HAVE_ATOMIC_ADD_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_32			1
#define osi_atomic_add_32(x, y) atomic_add_32(x, y)
#endif
#if (HAVE_ATOMIC_OR_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_32			1
#define osi_atomic_or_32(x, y) atomic_or_32(x, y)
#endif
#if (HAVE_ATOMIC_AND_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_32			1
#define osi_atomic_and_32(x, y) atomic_and_32(x, y)
#endif
#if (HAVE_ATOMIC_INC_32_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_32_NV			1
#define osi_atomic_inc_32_nv(x) atomic_inc_32_nv(x)
#endif
#if (HAVE_ATOMIC_DEC_32_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_32_NV			1
#define osi_atomic_dec_32_nv(x) atomic_dec_32_nv(x)
#endif
#if (HAVE_ATOMIC_ADD_32_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_32_NV			1
#define osi_atomic_add_32_nv(x, y) atomic_add_32_nv(x, y)
#endif
#if (HAVE_ATOMIC_OR_32_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_32_NV			1
#define osi_atomic_or_32_nv(x, y) atomic_or_32_nv(x, y)
#endif
#if (HAVE_ATOMIC_AND_32_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_32_NV			1
#define osi_atomic_and_32_nv(x, y) atomic_and_32_nv(x, y)
#endif
#if (HAVE_ATOMIC_CAS_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_CAS_32			1
#define osi_atomic_cas_32(x, y, z) atomic_cas_32(x, y, z)
#endif
#if (HAVE_ATOMIC_SWAP_32 == 1)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_32			1
#define osi_atomic_swap_32(x, y) atomic_swap_32(x, y)
#endif

#if (HAVE_ATOMIC_INC_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_64			1
#define osi_atomic_inc_64(x) atomic_inc_64(x)
#endif
#if (HAVE_ATOMIC_DEC_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_64			1
#define osi_atomic_dec_64(x) atomic_dec_64(x)
#endif
#if (HAVE_ATOMIC_ADD_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_64			1
#define osi_atomic_add_64(x, y) atomic_add_64(x, y)
#endif
#if (HAVE_ATOMIC_OR_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_64			1
#define osi_atomic_or_64(x, y) atomic_or_64(x, y)
#endif
#if (HAVE_ATOMIC_AND_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_64			1
#define osi_atomic_and_64(x, y) atomic_and_64(x, y)
#endif
#if (HAVE_ATOMIC_INC_64_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_INC_64_NV			1
#define osi_atomic_inc_64_nv(x) atomic_inc_64_nv(x)
#endif
#if (HAVE_ATOMIC_DEC_64_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_DEC_64_NV			1
#define osi_atomic_dec_64_nv(x) atomic_dec_64_nv(x)
#endif
#if (HAVE_ATOMIC_ADD_64_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_64_NV			1
#define osi_atomic_add_64_nv(x, y) atomic_add_64_nv(x, y)
#endif
#if (HAVE_ATOMIC_OR_64_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_OR_64_NV			1
#define osi_atomic_or_64_nv(x, y) atomic_or_64_nv(x, y)
#endif
#if (HAVE_ATOMIC_AND_64_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_AND_64_NV			1
#define osi_atomic_and_64_nv(x, y) atomic_and_64_nv(x, y)
#endif
#if (HAVE_ATOMIC_CAS_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_CAS_64			1
#define osi_atomic_cas_64(x, y, z) atomic_cas_64(x, y, z)
#endif
#if (HAVE_ATOMIC_SWAP_64 == 1)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_64			1
#define osi_atomic_swap_64(x, y) atomic_swap_64(x, y)
#endif

#if (HAVE_ATOMIC_ADD_PTR == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR			1
#define osi_atomic_add_ptr(x, y) atomic_add_ptr(x, y)
#endif
#if (HAVE_ATOMIC_ADD_PTR_NV == 1)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV		1
#define osi_atomic_add_ptr_nv(x, y) atomic_add_ptr_nv(x, y)
#endif
#if (HAVE_ATOMIC_CAS_PTR == 1)
#define OSI_IMPLEMENTS_ATOMIC_CAS_PTR			1
#define osi_atomic_cas_ptr(x, y, z) atomic_cas_ptr(x, y, z)
#endif
#if (HAVE_ATOMIC_SWAP_PTR == 1)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_PTR			1
#define osi_atomic_swap_ptr(x, y) atomic_swap_ptr(x, y)
#endif

#if (HAVE_MEMBAR_ENTER == 1)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_ENTER		1
#define osi_atomic_membar_lock_enter() membar_enter()
#endif
#if (HAVE_MEMBAR_EXIT == 1)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_EXIT		1
#define osi_atomic_membar_lock_exit() membar_exit()
#endif
#if (HAVE_MEMBAR_CONSUMER == 1)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS	1
#define osi_atomic_membar_order_loads() membar_consumer()
#endif
#if (HAVE_MEMBAR_PRODUCER == 1)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES	1
#define osi_atomic_membar_order_stores() membar_producer()
#endif


#endif /* _OSI_SOLARIS_ATOMIC_H */
