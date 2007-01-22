/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_ATOMIC_H
#define _OSI_COMMON_ATOMIC_H 1

/*
 * osi atomic operations
 * common compatibility layer code
 */

/* 
 * compat logic
 * when add_N doesn't exist, but add_N_nv does:
 * 
 * implement add_N using add_N_nv
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_8_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_8)
#define OSI_IMPLEMENTS_ATOMIC_ADD_8 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_ADD_8 1
#define osi_atomic_add_8(var, val) ((void)osi_atomic_add_8_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_16)
#define OSI_IMPLEMENTS_ATOMIC_ADD_16 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_ADD_16 1
#define osi_atomic_add_16(var, val) ((void)osi_atomic_add_16_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_32)
#define OSI_IMPLEMENTS_ATOMIC_ADD_32 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_ADD_32 1
#define osi_atomic_add_32(var, val) ((void)osi_atomic_add_32_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_64)
#define OSI_IMPLEMENTS_ATOMIC_ADD_64 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_ADD_64 1
#define osi_atomic_add_64(var, val) ((void)osi_atomic_add_64_nv(var, val))
#endif

/* 
 * compat logic
 * when and_N doesn't exist, but and_N_nv does:
 * 
 * implement and_N using and_N_nv
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_AND_8_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_AND_8)
#define OSI_IMPLEMENTS_ATOMIC_AND_8 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_AND_8 1
#define osi_atomic_and_8(var, val) ((void)osi_atomic_and_8_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_16_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_AND_16)
#define OSI_IMPLEMENTS_ATOMIC_AND_16 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_AND_16 1
#define osi_atomic_and_16(var, val) ((void)osi_atomic_and_16_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_AND_32)
#define OSI_IMPLEMENTS_ATOMIC_AND_32 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_AND_32 1
#define osi_atomic_and_32(var, val) ((void)osi_atomic_and_32_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_AND_64)
#define OSI_IMPLEMENTS_ATOMIC_AND_64 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_AND_64 1
#define osi_atomic_and_64(var, val) ((void)osi_atomic_and_64_nv(var, val))
#endif


/* 
 * compat logic
 * when or_N doesn't exist, but or_N_nv does:
 * 
 * implement or_N using or_N_nv
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_OR_8_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_OR_8)
#define OSI_IMPLEMENTS_ATOMIC_OR_8 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_OR_8 1
#define osi_atomic_or_8(var, val) ((void)osi_atomic_or_8_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_16_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_OR_16)
#define OSI_IMPLEMENTS_ATOMIC_OR_16 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_OR_16 1
#define osi_atomic_or_16(var, val) ((void)osi_atomic_or_16_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_OR_32)
#define OSI_IMPLEMENTS_ATOMIC_OR_32 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_OR_32 1
#define osi_atomic_or_32(var, val) ((void)osi_atomic_or_32_nv(var, val))
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_OR_64)
#define OSI_IMPLEMENTS_ATOMIC_OR_64 1
#define OSI_IMPLEMENTS_SLOW_ATOMIC_OR_64 1
#define osi_atomic_or_64(var, val) ((void)osi_atomic_or_64_nv(var, val))
#endif


/* 
 * compat logic
 * when inc_N doesn't exist, but add_N does:
 *
 * implement inc_N using add_N
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_8) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_8)
#define OSI_IMPLEMENTS_ATOMIC_INC_8 1
#define osi_atomic_inc_8(var) osi_atomic_add_8(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_16)
#define OSI_IMPLEMENTS_ATOMIC_INC_16 1
#define osi_atomic_inc_16(var) osi_atomic_add_16(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_32)
#define OSI_IMPLEMENTS_ATOMIC_INC_32 1
#define osi_atomic_inc_32(var) osi_atomic_add_32(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_64)
#define OSI_IMPLEMENTS_ATOMIC_INC_64 1
#define osi_atomic_inc_64(var) osi_atomic_add_64(var, 1)
#endif


/* 
 * compat logic
 * when dec_N doesn't exist, but add_N does:
 *
 * implement dec_N using add_N
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_8) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_8)
#define OSI_IMPLEMENTS_ATOMIC_DEC_8 1
#define osi_atomic_dec_8(var) osi_atomic_add_8(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_16)
#define OSI_IMPLEMENTS_ATOMIC_DEC_16 1
#define osi_atomic_dec_16(var) osi_atomic_add_16(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_32)
#define OSI_IMPLEMENTS_ATOMIC_DEC_32 1
#define osi_atomic_dec_32(var) osi_atomic_add_32(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_64)
#define OSI_IMPLEMENTS_ATOMIC_DEC_64 1
#define osi_atomic_dec_64(var) osi_atomic_add_64(var, -1)
#endif


/* 
 * compat logic
 * when inc_N_nv doesn't exist, but add_N_nv does:
 *
 * implement inc_N_nv using add_N_nv
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_8_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_8_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_8_NV 1
#define osi_atomic_inc_8_nv(var) osi_atomic_add_8_nv(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_16_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_16_NV 1
#define osi_atomic_inc_16_nv(var) osi_atomic_add_16_nv(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_32_NV 1
#define osi_atomic_inc_32_nv(var) osi_atomic_add_32_nv(var, 1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_64_NV 1
#define osi_atomic_inc_64_nv(var) osi_atomic_add_64_nv(var, 1)
#endif


/* 
 * compat logic
 * when dec_N_nv doesn't exist, but add_N_nv does:
 *
 * implement dec_N_nv using add_N_nv
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_8_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_8_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_8_NV 1
#define osi_atomic_dec_8_nv(var) osi_atomic_add_8_nv(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_16_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_16_NV 1
#define osi_atomic_dec_16_nv(var) osi_atomic_add_16_nv(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_32_NV 1
#define osi_atomic_dec_32_nv(var) osi_atomic_add_32_nv(var, -1)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_DEC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_64_NV 1
#define osi_atomic_dec_64_nv(var) osi_atomic_add_64_nv(var, -1)
#endif


/* 
 * compat logic
 * when membar exists, but special membars don't
 *
 * back with slower full membar
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR) && !defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_ENTER)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_ENTER 1
#define osi_atomic_membar_lock_enter() osi_atomic_membar()
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR) && !defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_EXIT)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_EXIT 1
#define osi_atomic_membar_lock_exit() osi_atomic_membar()
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR) && !defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS 1
#define osi_atomic_membar_order_loads() osi_atomic_membar()
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR) && !defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES)
#define OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES 1
#define osi_atomic_membar_order_stores() osi_atomic_membar()
#endif


/* 
 * special type atomic ops
 * atomic ops for pointers
 */

#if (osi_datamodel_pointer() == 32)

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR 1
#define osi_atomic_add_ptr(x, y) osi_atomic_add_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV 1
#define osi_atomic_add_ptr_nv(x, y) osi_atomic_add_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_32) && !defined(OSI_IMPLEMENTS_ATOMIC_CAS_PTR)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR 1
#define osi_atomic_cas_ptr(x, y, z) osi_atomic_cas_32(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_32) && !defined(OSI_IMPLEMENTS_ATOMIC_SWAP_PTR)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_PTR 1
#define osi_atomic_swap_ptr(x, y) osi_atomic_swap_32(x, y)
#endif

#elif (osi_datamodel_pointer() == 64)

#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR 1
#define osi_atomic_add_ptr(x, y) osi_atomic_add_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV) && !defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV 1
#define osi_atomic_add_ptr_nv(x, y) osi_atomic_add_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_64) && !defined(OSI_IMPLEMENTS_ATOMIC_CAS_PTR)
#define OSI_IMPLEMENTS_ATOMIC_ADD_PTR 1
#define osi_atomic_cas_ptr(x, y, z) osi_atomic_cas_64(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_64) && !defined(OSI_IMPLEMENTS_ATOMIC_SWAP_PTR)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_PTR 1
#define osi_atomic_swap_ptr(x, y) osi_atomic_swap_64(x, y)
#endif

#endif /* osi_datamodel_pointer() */


/* 
 * special type atomic ops
 * atomic ops for osi_fast_uint type
 */

#if (OSI_TYPE_FAST_BITS == 32)

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32)
#define OSI_IMPLEMENTS_ATOMIC_INC_FAST		1
#define osi_atomic_inc_fast(x) osi_atomic_inc_32(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_32)
#define OSI_IMPLEMENTS_ATOMIC_DEC_FAST		1
#define osi_atomic_dec_fast(x) osi_atomic_dec_32(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32)
#define OSI_IMPLEMENTS_ATOMIC_ADD_FAST		1
#define osi_atomic_add_fast(x, y) osi_atomic_add_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_32)
#define OSI_IMPLEMENTS_ATOMIC_OR_FAST		1
#define osi_atomic_or_fast(x, y) osi_atomic_or_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_32)
#define OSI_IMPLEMENTS_ATOMIC_AND_FAST		1
#define osi_atomic_and_fast(x, y) osi_atomic_and_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_FAST_NV		1
#define osi_atomic_inc_fast_nv(x) osi_atomic_inc_32_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_FAST_NV		1
#define osi_atomic_dec_fast_nv(x) osi_atomic_dec_32_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_FAST_NV		1
#define osi_atomic_add_fast_nv(x, y) osi_atomic_add_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_OR_FAST_NV		1
#define osi_atomic_or_fast_nv(x, y) osi_atomic_or_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_AND_FAST_NV		1
#define osi_atomic_and_fast_nv(x, y) osi_atomic_and_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_32)
#define OSI_IMPLEMENTS_ATOMIC_CAS_FAST		1
#define osi_atomic_cas_fast(x, y, z) osi_atomic_cas_32(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_32)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_FAST		1
#define osi_atomic_swap_fast(x, y) osi_atomic_swap_32(x, y)
#endif

#elif (OSI_TYPE_FAST_BITS == 64)

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64)
#define OSI_IMPLEMENTS_ATOMIC_INC_FAST		1
#define osi_atomic_inc_fast(x) osi_atomic_inc_64(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_64)
#define OSI_IMPLEMENTS_ATOMIC_DEC_FAST		1
#define osi_atomic_dec_fast(x) osi_atomic_dec_64(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64)
#define OSI_IMPLEMENTS_ATOMIC_ADD_FAST		1
#define osi_atomic_add_fast(x, y) osi_atomic_add_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_64)
#define OSI_IMPLEMENTS_ATOMIC_OR_FAST		1
#define osi_atomic_or_fast(x, y) osi_atomic_or_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_64)
#define OSI_IMPLEMENTS_ATOMIC_AND_FAST		1
#define osi_atomic_and_fast(x, y) osi_atomic_and_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_FAST_NV		1
#define osi_atomic_inc_fast_nv(x) osi_atomic_inc_64_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_FAST_NV		1
#define osi_atomic_dec_fast_nv(x) osi_atomic_dec_64_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_FAST_NV		1
#define osi_atomic_add_fast_nv(x, y) osi_atomic_add_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_OR_FAST_NV		1
#define osi_atomic_or_fast_nv(x, y) osi_atomic_or_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_AND_FAST_NV		1
#define osi_atomic_and_fast_nv(x, y) osi_atomic_and_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_64)
#define OSI_IMPLEMENTS_ATOMIC_CAS_FAST		1
#define osi_atomic_cas_fast(x, y, z) osi_atomic_cas_64(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_64)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_FAST		1
#define osi_atomic_swap_fast(x, y) osi_atomic_swap_64(x, y)
#endif

#endif /* OSI_TYPE_FAST_BITS */


/* 
 * special type atomic ops
 * atomic ops for osi_register_uint type
 */

#if (OSI_TYPE_REGISTER_BITS == 32)

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32)
#define OSI_IMPLEMENTS_ATOMIC_INC_REGISTER		1
#define osi_atomic_inc_register(x) osi_atomic_inc_32(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_32)
#define OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER		1
#define osi_atomic_dec_register(x) osi_atomic_dec_32(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32)
#define OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER		1
#define osi_atomic_add_register(x, y) osi_atomic_add_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_32)
#define OSI_IMPLEMENTS_ATOMIC_OR_REGISTER		1
#define osi_atomic_or_register(x, y) osi_atomic_or_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_32)
#define OSI_IMPLEMENTS_ATOMIC_AND_REGISTER		1
#define osi_atomic_and_register(x, y) osi_atomic_and_32(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_REGISTER_NV		1
#define osi_atomic_inc_register_nv(x) osi_atomic_inc_32_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER_NV		1
#define osi_atomic_dec_register_nv(x) osi_atomic_dec_32_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER_NV		1
#define osi_atomic_add_register_nv(x, y) osi_atomic_add_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_OR_REGISTER_NV		1
#define osi_atomic_or_register_nv(x, y) osi_atomic_or_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_32_NV)
#define OSI_IMPLEMENTS_ATOMIC_AND_REGISTER_NV		1
#define osi_atomic_and_register_nv(x, y) osi_atomic_and_32_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_32)
#define OSI_IMPLEMENTS_ATOMIC_CAS_REGISTER		1
#define osi_atomic_cas_register(x, y, z) osi_atomic_cas_32(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_32)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_REGISTER		1
#define osi_atomic_swap_register(x, y) osi_atomic_swap_32(x, y)
#endif

#elif (OSI_TYPE_REGISTER_BITS == 64)

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64)
#define OSI_IMPLEMENTS_ATOMIC_INC_REGISTER		1
#define osi_atomic_inc_register(x) osi_atomic_inc_64(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_64)
#define OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER		1
#define osi_atomic_dec_register(x) osi_atomic_dec_64(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64)
#define OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER		1
#define osi_atomic_add_register(x, y) osi_atomic_add_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_64)
#define OSI_IMPLEMENTS_ATOMIC_OR_REGISTER		1
#define osi_atomic_or_register(x, y) osi_atomic_or_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_64)
#define OSI_IMPLEMENTS_ATOMIC_AND_REGISTER		1
#define osi_atomic_and_register(x, y) osi_atomic_and_64(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_INC_REGISTER_NV		1
#define osi_atomic_inc_register_nv(x) osi_atomic_inc_64_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER_NV		1
#define osi_atomic_dec_register_nv(x) osi_atomic_dec_64_nv(x)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER_NV		1
#define osi_atomic_add_register_nv(x, y) osi_atomic_add_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_OR_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_OR_REGISTER_NV		1
#define osi_atomic_or_register_nv(x, y) osi_atomic_or_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_AND_64_NV)
#define OSI_IMPLEMENTS_ATOMIC_AND_REGISTER_NV		1
#define osi_atomic_and_register_nv(x, y) osi_atomic_and_64_nv(x, y)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_64)
#define OSI_IMPLEMENTS_ATOMIC_CAS_REGISTER		1
#define osi_atomic_cas_register(x, y, z) osi_atomic_cas_64(x, y, z)
#endif
#if defined(OSI_IMPLEMENTS_ATOMIC_SWAP_64)
#define OSI_IMPLEMENTS_ATOMIC_SWAP_REGISTER		1
#define osi_atomic_swap_register(x, y) osi_atomic_swap_64(x, y)
#endif

#endif /* OSI_TYPE_REGISTER_BITS */


#endif /* _OSI_COMMON_ATOMIC_H */
