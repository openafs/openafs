/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_REFCNT_INLINE_H
#define _OSI_COMMON_REFCNT_INLINE_H 1

/*
 * implementation of reference counting
 * using atomic operations
 */

#if defined(OSI_IMPLEMENTS_REFCNT_ATOMIC)

osi_inline_define(
int
osi_refcnt_inc_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
{
    int fired = 0;
    if (_osi_refcnt_inc_nv(refcnt) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_refcnt_inc_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata
		      osi_result * res_out)
)

osi_inline_define(
int
osi_refcnt_dec_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
{
    int fired = 0;
    if (_osi_refcnt_dec_nv(refcnt) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_refcnt_dec_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_REFCNT_ATOMIC */

#endif /* _OSI_COMMON_REFCNT_INLINE_H */
