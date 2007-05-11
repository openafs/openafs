/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_ATOMIC_H
#define _OSI_OSI_ATOMIC_H 1


/*
 * platform-independent osi_atomic API
 * atomic operations support
 *
 * NOTE: due to system implementation differences,
 *       please ONLY use atomic operations for
 *       variables which range between 0 and (2^N-1)-1
 *       (some platforms implement atomics as signed;
 *        others as unsigned)
 *
 * XXX the 32/64 dynamic size atomic types are not
 *     implemented on any platform yet!!! they are
 *     planned as a future convenience method...
 *
 * atomic types:
 *
 *   osi_atomic_t
 *     -- 32 or 64-bit size atomic integer
 *
 *   osi_atomic_val_t
 *     -- 32 or 64-bit size atomic value
 *
 *   osi_atomic_delta_t
 *     -- 32 or 64-bit size atomic delta (addend)
 *
 *   osi_atomic8_t
 *     -- 8-bit atomic integer
 *
 *   osi_atomic8_val_t
 *     -- 8-bit atomic value
 *
 *   osi_atomic8_delta_t
 *     -- 8-bit atomic delta (addend)
 *
 *   osi_atomic16_t;
 *     -- 16-bit atomic integer
 *
 *   osi_atomic16_val_t;
 *     -- 16-bit atomic value
 *
 *   osi_atomic16_delta_t
 *     -- 16-bit atomic delta (addend)
 *
 *   osi_atomic32_t;
 *     -- 32-bit atomic integer
 *
 *   osi_atomic32_val_t;
 *     -- 32-bit atomic value
 *
 *   osi_atomic32_delta_t
 *     -- 32-bit atomic delta (addend)
 *
 *   osi_atomic64_t;
 *     -- 64-bit atomic integer
 *
 *   osi_atomic64_val_t;
 *     -- 64-bit atomic value
 *
 *   osi_atomic64_delta_t
 *     -- 64-bit atomic delta (addend)
 *
 *   osi_atomic_fast_t;
 *     -- osi_fast_(u)int size atomic integer
 *
 *   osi_atomic_fast_val_t;
 *     -- osi_fast_(u)int size atomic value
 *
 *   osi_atomic_fast_delta_t
 *     -- osi_fast_(u)int size atomic delta (addend)
 *
 *   osi_atomic_register_t;
 *     -- osi_register_(u)int size atomic integer
 *
 *   osi_atomic_register_val_t;
 *     -- osi_register_(u)int size atomic value
 *
 *   osi_atomic_register_delta_t
 *     -- osi_register_(u)int size atomic delta (addend)
 *
 * atomic increment:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC)
 *
 *  void osi_atomic_inc(osi_atomic_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_8)
 *
 *  void osi_atomic_inc_8(osi_atomic8_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_16)
 *
 *  void osi_atomic_inc_16(osi_atomic16_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_32)
 *
 *  void osi_atomic_inc_32(osi_atomic32_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_64)
 *
 *  void osi_atomic_inc_64(osi_atomic64_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_FAST)
 *
 *  void osi_atomic_inc_fast(osi_atomic_fast_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_REGISTER)
 *
 *  void osi_atomic_inc_register(osi_atomic_register_t *);
 *
 *
 * atomic decrement:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC)
 *
 *  void osi_atomic_dec(osi_atomic_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_8)
 *
 *  void osi_atomic_dec_8(osi_atomic8_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_16)
 *
 *  void osi_atomic_dec_16(osi_atomic16_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_32)
 *
 *  void osi_atomic_dec_32(osi_atomic32_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_64)
 *
 *  void osi_atomic_dec_64(osi_atomic64_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_FAST)
 *
 *  void osi_atomic_dec_fast(osi_atomic_fast_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER)
 *
 *  void osi_atomic_dec_register(osi_atomic_register_t *);
 *
 *
 * atomic add:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD)
 *
 *  void osi_atomic_add(osi_atomic_t *, osi_atomic_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_8)
 *
 *  void osi_atomic_add_8(osi_atomic8_t *, osi_atomic8_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_16)
 *
 *  void osi_atomic_add_16(osi_atomic16_t *, osi_atomic16_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_32)
 *
 *  void osi_atomic_add_32(osi_atomic32_t *, osi_atomic32_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_64)
 *
 *  void osi_atomic_add_64(osi_atomic64_t *, osi_atomic64_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_FAST)
 *
 *  void osi_atomic_add_fast(osi_atomic_fast_t *, 
 *                           osi_atomic_fast_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER)
 *
 *  void osi_atomic_add_register(osi_atomic_register_t *, 
 *                               osi_atomic_register_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR)
 *
 *  void osi_atomic_add_ptr(volatile void *, osi_intptr_t);
 *
 *
 * atomic or:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR)
 *
 *  void osi_atomic_or(osi_atomic_t *, osi_atomic_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_8)
 *
 *  void osi_atomic_or_8(osi_atomic8_t *, osi_atomic8_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_16)
 *
 *  void osi_atomic_or_16(osi_atomic16_t *, osi_atomic16_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_32)
 *
 *  void osi_atomic_or_32(osi_atomic32_t *, osi_atomic32_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_64)
 *
 *  void osi_atomic_or_64(osi_atomic64_t *, osi_atomic64_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_FAST)
 *
 *  void osi_atomic_or_fast(osi_atomic_fast_t *, osi_atomic_fast_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_REGISTER)
 *
 *  void osi_atomic_or_register(osi_atomic_register_t *, osi_atomic_register_val_t);
 *
 *
 * atomic and:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND)
 *
 *  void osi_atomic_and(osi_atomic_t *, osi_atomic_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_8)
 *
 *  void osi_atomic_and_8(osi_atomic8_t *, osi_atomic8_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_16)
 *
 *  void osi_atomic_and_16(osi_atomic16_t *, osi_atomic16_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_32)
 *
 *  void osi_atomic_and_32(osi_atomic32_t *, osi_atomic32_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_64)
 *
 *  void osi_atomic_and_64(osi_atomic64_t *, osi_atomic64_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_FAST)
 *
 *  void osi_atomic_and_fast(osi_atomic_fast_t *, osi_atomic_fast_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_REGISTER)
 *
 *  void osi_atomic_and_register(osi_atomic_register_t *, osi_atomic_register_val_t);
 *
 *
 * atomic bitwise set:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET)
 *
 *  void osi_atomic_bit_set(osi_atomic_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_8)
 *
 *  void osi_atomic_bit_set_8(osi_atomic8_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_16)
 *
 *  void osi_atomic_bit_set_16(osi_atomic16_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_32)
 *
 *  void osi_atomic_bit_set_32(osi_atomic32_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_64)
 *
 *  void osi_atomic_bit_set_64(osi_atomic64_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_FAST)
 *
 *  void osi_atomic_bit_set_fast(osi_atomic_fast_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_REGISTER)
 *
 *  void osi_atomic_bit_set_register(osi_atomic_register_t *, unsigned int bitnum);
 *
 *
 * atomic bitwise clear:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR)
 *
 *  void osi_atomic_bit_clr(osi_atomic_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_8)
 *
 *  void osi_atomic_bit_clr_8(osi_atomic8_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_16)
 *
 *  void osi_atomic_bit_clr_16(osi_atomic16_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_32)
 *
 *  void osi_atomic_bit_clr_32(osi_atomic32_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_64)
 *
 *  void osi_atomic_bit_clr_64(osi_atomic64_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_FAST)
 *
 *  void osi_atomic_bit_clr_fast(osi_atomic_fast_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_REGISTER)
 *
 *  void osi_atomic_bit_clr_register(osi_atomic_register_t *, unsigned int bitnum);
 *
 *
 * please bear in mind that the interfaces below this line
 * are considerably slower on many architectures than the ones
 * above.  
 *
 *
 * atomic increment and return new value:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_NV)
 *
 *  osi_atomic_val_t osi_atomic_inc_nv(osi_atomic_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_8_NV)
 *
 *  osi_atomic8_val_t osi_atomic_inc_8_nv(osi_atomic8_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_16_NV)
 *
 *  osi_atomic16_val_t osi_atomic_inc_16_nv(osi_atomic16_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
 *
 *  osi_atomic32_val_t osi_atomic_inc_32_nv(osi_atomic32_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV)
 *
 *  osi_atomic64_val_t osi_atomic_inc_64_nv(osi_atomic64_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_FAST_NV)
 *
 *  osi_atomic_fast_val_t osi_atomic_inc_fast_nv(osi_atomic_fast_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_INC_REGISTER_NV)
 *
 *  osi_atomic_register_val_t osi_atomic_inc_register_nv(osi_atomic_register_t *);
 *
 *
 * atomic decrement and return new value:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_NV)
 *
 *  osi_atomic_val_t osi_atomic_dec_nv(osi_atomic_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_8_NV)
 *
 *  osi_atomic8_val_t osi_atomic_dec_8_nv(osi_atomic8_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_16_NV)
 *
 *  osi_atomic16_val_t osi_atomic_dec_16_nv(osi_atomic16_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
 *
 *  osi_atomic32_val_t osi_atomic_dec_32_nv(osi_atomic32_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_64_NV)
 *
 *  osi_atomic64_val_t osi_atomic_dec_64_nv(osi_atomic64_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_FAST_NV)
 *
 *  osi_atomic_fast_val_t osi_atomic_dec_fast_nv(osi_atomic_fast_t *);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_DEC_REGISTER_NV)
 *
 *  osi_atomic_register_val_t osi_atomic_dec_register_nv(osi_atomic_register_t *);
 *
 *
 * atomic add and return new value:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_NV)
 *
 *  osi_atomic_val_t osi_atomic_add_nv(osi_atomic_t *, osi_atomic_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_8_NV)
 *
 *  osi_atomic8_val_t osi_atomic_add_8_nv(osi_atomic8_t *, osi_atomic8_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV)
 *
 *  osi_atomic16_val_t osi_atomic_add_16_nv(osi_atomic16_t *, osi_atomic16_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
 *
 *  osi_atomic32_val_t osi_atomic_add_32_nv(osi_atomic32_t *, osi_atomic32_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
 *
 *  osi_atomic64_val_t osi_atomic_add_64_nv(osi_atomic64_t *, osi_atomic64_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_FAST_NV)
 *
 *  osi_atomic_fast_val_t osi_atomic_add_fast_nv(osi_atomic_fast_t *, 
 *                                               osi_atomic_fast_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_REGISTER_NV)
 *
 *  osi_atomic_register_val_t osi_atomic_add_register_nv(osi_atomic_register_t *, 
 *                                                       osi_atomic_register_delta_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_ADD_PTR_NV)
 *
 *  void * osi_atomic_add_ptr_nv(volatile void *, 
 *                               osi_intptr_t);
 *
 *
 * atomic or and return new value:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_NV)
 *
 *  osi_atomic_val_t osi_atomic_or_nv(osi_atomic_t *, 
 *                                    osi_atomic_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_8_NV)
 *
 *  osi_atomic8_val_t osi_atomic_or_8_nv(osi_atomic8_t *, 
 *                                       osi_atomic8_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_16_NV)
 *
 *  osi_atomic16_val_t osi_atomic_or_16_nv(osi_atomic16_t *, 
 *                                         osi_atomic16_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_32_NV)
 *
 *  osi_atomic32_val_t osi_atomic_or_32_nv(osi_atomic32_t *, 
 *                                         osi_atomic32_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_64_NV)
 *
 *  osi_atomic64_val_t osi_atomic_or_64_nv(osi_atomic64_t *, 
 *                                         osi_atomic64_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_FAST_NV)
 *
 *  osi_atomic_fast_val_t osi_atomic_or_fast_nv(osi_atomic_fast_t *, 
 *                                              osi_atomic_fast_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_OR_REGISTER_NV)
 *
 *  osi_atomic_register_val_t osi_atomic_or_register_nv(osi_atomic_register_t *, 
 *                                                      osi_atomic_register_val_t);
 *
 *
 * atomic and and return new value:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_NV)
 *
 *  osi_atomic_val_t osi_atomic_and_nv(osi_atomic_t *, 
 *                                     osi_atomic_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_8_NV)
 *
 *  osi_atomic8_val_t osi_atomic_and_8_nv(osi_atomic8_t *, 
 *                                        osi_atomic8_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_16_NV)
 *
 *  osi_atomic16_val_t osi_atomic_and_16_nv(osi_atomic16_t *, 
 *                                          osi_atomic16_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_32_NV)
 *
 *  osi_atomic32_val_t osi_atomic_and_32_nv(osi_atomic32_t *, 
 *                                          osi_atomic32_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_64_NV)
 *
 *  osi_atomic64_val_t osi_atomic_and_64_nv(osi_atomic64_t *, 
 *                                          osi_atomic64_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_FAST_NV)
 *
 *  osi_atomic_fast_val_t osi_atomic_and_fast_nv(osi_atomic_fast_t *, 
 *                                               osi_atomic_fast_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_AND_REGISTER_NV)
 *
 *  osi_atomic_register_val_t osi_atomic_and_register_nv(osi_atomic_register_t *, 
 *                                                       osi_atomic_register_val_t);
 *
 *
 * atomic bitwise set:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_NV)
 *
 *  int osi_atomic_bit_set_nv(osi_atomic_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_8_NV)
 *
 *  int osi_atomic_bit_set_8_nv(osi_atomic8_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_16_NV)
 *
 *  int osi_atomic_bit_set_16_nv(osi_atomic16_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_32_NV)
 *
 *  int osi_atomic_bit_set_32_nv(osi_atomic32_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_64_NV)
 *
 *  int osi_atomic_bit_set_64_nv(osi_atomic64_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_FAST_NV)
 *
 *  int osi_atomic_bit_set_fast_nv(osi_atomic_fast_t *, 
 *                                 unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_SET_REGISTER_NV)
 *
 *  int osi_atomic_bit_set_register_nv(osi_atomic_register_t *, 
 *                                     unsigned int bitnum);
 *
 *
 * atomic bitwise clear:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_NV)
 *
 *  int osi_atomic_bit_clr_nv(osi_atomic_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_8_NV)
 *
 *  int osi_atomic_bit_clr_8_nv(osi_atomic8_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_16_NV)
 *
 *  int osi_atomic_bit_clr_16_nv(osi_atomic16_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_32_NV)
 *
 *  int osi_atomic_bit_clr_32_nv(osi_atomic32_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_64_NV)
 *
 *  int osi_atomic_bit_clr_64_nv(osi_atomic64_t *, unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_FAST_NV)
 *
 *  int osi_atomic_bit_clr_fast_nv(osi_atomic_fast_t *, 
 *                                 unsigned int bitnum);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_BIT_CLR_REGISTER_NV)
 *
 *  int osi_atomic_bit_clr_register_nv(osi_atomic_register_t *, 
 *                                     unsigned int bitnum);
 *
 *
 * atomic compare and swap:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS)
 *
 *  osi_atomic_val_t osi_atomic_cas(osi_atomic_t * targ, 
 *                                  osi_atomic_val_t cmp, 
 *                                  osi_atomic_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_8)
 *
 *  osi_atomic8_val_t osi_atomic_cas_8(osi_atomic8_t * targ, 
 *                                     osi_atomic8_val_t cmp, 
 *                                     osi_atomic8_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_16)
 *
 *  osi_atomic16_val_t osi_atomic_cas_16(osi_atomic16_t * targ, 
 *                                       osi_atomic16_val_t cmp, 
 *                                       osi_atomic16_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_32)
 *
 *  osi_atomic32_val_t osi_atomic_cas_32(osi_atomic32_t * targ, 
 *                                       osi_atomic32_val_t cmp, 
 *                                       osi_atomic32_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_64)
 *
 *  osi_atomic64_val_t osi_atomic_cas_64(osi_atomic64_t * targ, 
 *                                       osi_atomic64_val_t cmp, 
 *                                       osi_atomic64_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_FAST)
 *
 *  osi_atomic_fast_val_t osi_atomic_cas_fast(osi_atomic_fast_t * targ, 
 *                                            osi_atomic_fast_val_t cmp, 
 *                                            osi_atomic_fast_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_REGISTER)
 *
 *  osi_atomic_register_val_t osi_atomic_cas_register(osi_atomic_register_t * targ, 
 *                                                    osi_atomic_register_val_t cmp, 
 *                                                    osi_atomic_register_val_t nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_CAS_PTR)
 *
 *  void * osi_atomic_cas_ptr(volatile void * targ, 
 *                            void * cmp, 
 *                            void * nv);
 *    -- compare value stored at $targ$ with $cmp$.  if they are equal, 
 *       store $nv$ into $targ$.  return old value from $targ$ regardless of 
 *       comparison outcome.
 *
 *
 * atomic swap:
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP)
 *
 *  osi_atomic_val_t osi_atomic_swap(osi_atomic_t *, osi_atomic_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_8)
 *
 *  osi_atomic8_val_t osi_atomic_swap_8(osi_atomic8_t *, osi_atomic8_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_16)
 *
 *  osi_atomic16_val_t osi_atomic_swap_16(osi_atomic16_t *, osi_atomic16_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_32)
 *
 *  osi_atomic32_val_t osi_atomic_swap_32(osi_atomic32_t *, osi_atomic32_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_64)
 *
 *  osi_atomic64_val_t osi_atomic_swap_64(osi_atomic64_t *, osi_atomic64_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_FAST)
 *
 *  osi_atomic_fast_val_t osi_atomic_swap_fast(osi_atomic_fast_t *, 
 *                                             osi_atomic_fast_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_REGISTER)
 *
 *  osi_atomic_register_val_t osi_atomic_swap_register(osi_atomic_register_t *, 
 *                                                     osi_atomic_register_val_t);
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_ATOMIC_SWAP_PTR)
 *
 *  void * osi_atomic_swap_ptr(volatile void *, void *);
 *
 *
 * explicit memory barrier operations:
 *
 * the following interface requires:
 * defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR)
 *
 *  void osi_atomic_membar(void);
 *    -- enforce complete ordering
 *
 * NOTE: the following two membar interfaces make it easy to develop
 *       your own locking primitives
 *
 * the following interface requires:
 * defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_ENTER)
 *
 *  void osi_atomic_membar_lock_enter(void);
 *    -- do not complete loads or stores beyond this point until we can guarantee that
 *       lock acquisition has reached global visibility
 *       (it protects against Store-Load and Store-Store data hazards)
 *
 * the following interface requires:
 * defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_LOCK_EXIT)
 *
 *  void osi_atomic_membar_lock_exit(void);
 *    -- make sure all previous loads and stores are resolved before we proceed with
 *       making lock release globally visible
 *       (it protects against Load-Store and Store-Store data hazards)
 *
 * NOTE: the following two membar interfaces make it easy to develop
 *       your own producer/consumer primitives
 *
 * the following interface requires:
 * defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS)
 *
 *  void osi_atomic_membar_order_loads(void);
 *    -- resolve all pending loads before subsequent load instructions
 *       (it protects against Load-Load data hazards)
 *
 * the following interface requires:
 * defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES)
 *
 *  void osi_atomic_membar_order_stores(void);
 *    -- resolve all pending stores before subsequent store instructions
 *       (it protects against Store-Store data hazards)
 */


#if defined(OSI_ENV_KERNELSPACE)

#ifdef OSI_SUN510_ENV
#include <osi/SOLARIS/atomic.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#ifdef OSI_SUN510_ENV
#include <osi/SOLARIS/atomic.h>
#endif

#endif /* !OSI_ENV_KERNELSPACE */


/* pull in atomic ops compat code */
#include <osi/COMMON/atomic.h>


#endif /* _OSI_OSI_ATOMIC_H */
