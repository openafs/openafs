/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_REFCNT_H
#define _OSI_OSI_REFCNT_H 1


/*
 * platform-independent osi_refcnt API
 * high-performance object refcounting interface
 * 
 * types:
 *
 *  osi_refcnt_val_t
 *    -- refcount value type
 *
 *  osi_refcnt_t
 *    -- refcounting type
 *
 *  osi_refcnt_action_t
 *    -- action function type
 *
 * interface:
 *
 *  osi_refcnt_init(osi_refcnt_t *, osi_refcnt_val_t init_val);
 *    -- initialize a refcount object
 *
 *  osi_refcnt_destroy(osi_refcnt_t *);
 *    -- destroy a refcount object
 *
 *  osi_refcnt_inc(osi_refcnt_t *);
 *    -- increment refcount
 *
 *  osi_refcnt_dec(osi_refcnt_t *);
 *    -- decrement refcount
 *
 * WARNING: the following interface must be used very carefully!
 *          it allows you to reset the value of a reference count
 *          in a non-atomic manner.  it is mostly useful in object
 *          caching scenarios when you are recycling a freed object
 *
 *  osi_refcnt_reset(osi_refcnt_t *, osi_refcnt_val_t new_val);
 *    -- reset the value of a previously initialized refcount
 *
 * NOTE: The following operations use slower atomic operations than the
 *       interfaces defined above, and thus should be used only when
 *       absolutely necessary.
 * 
 *  int osi_refcnt_inc_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh, 
 *                            osi_refcnt_action_t * action, void *sdata,
 *                            osi_result * res_out);
 *    -- increment $refcnt$, if $refcnt$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_refcnt_dec_action(osi_refcnt_t * refcnt, osi_refcnt_val_t thresh,
 *                            osi_refcnt_action_t * action, void *sdata,
 *                            osi_result * res_out);
 *    -- decrement $refcnt$, if $refcnt$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 */

typedef osi_result osi_refcnt_action_t(void * sdata);

#include <osi/COMMON/refcnt.h>

#if !defined(OSI_IMPLEMENTS_REFCNT_ATOMIC)
#include <osi/LEGACY/refcnt.h>
#endif


#endif /* _OSI_OSI_REFCNT_H */
