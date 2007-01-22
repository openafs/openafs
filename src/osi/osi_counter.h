/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_COUNTER_H
#define _OSI_OSI_COUNTER_H 1


/*
 * platform-independent osi_counter API
 * high-performance counter interface
 * 
 * NOTE: 64-bit counters may not exist on all platforms.
 *  please check for defined(OSI_IMPLEMENTS_COUNTER64)
 *
 * types:
 *
 *  osi_counter_action_t
 *    -- action function type
 *
 *  osi_counter_val_t
 *    -- counter value type
 *
 *  osi_counter_delta_t
 *    -- counter signed delta type
 *
 *  osi_counter_t
 *    -- counter type
 *
 *  osi_counter16_val_t
 *    -- 16-bit counter value type
 *
 *  osi_counter16_delta_t
 *    -- 16-bit counter signed delta type
 *
 *  osi_counter16_t
 *    -- 16-bit counter type
 *
 *  osi_counter32_val_t
 *    -- 32-bit counter value type
 *
 *  osi_counter32_delta_t
 *    -- 32-bit counter signed delta type
 *
 *  osi_counter32_t
 *    -- 32-bit counter type
 *
 *  osi_counter64_val_t
 *    -- 64-bit counter value type
 *
 *  osi_counter64_delta_t
 *    -- 64-bit counter signed delta type
 *
 *  osi_counter64_t
 *    -- 64-bit counter type
 *
 * interface:
 *
 *  osi_counter_init(osi_counter_t *, osi_counter_val_t init_val);
 *    -- initialize a counter object
 *
 *  osi_counter16_init(osi_counter16_t *, osi_counter16_val_t init_val);
 *    -- initialize a counter object
 *
 *  osi_counter32_init(osi_counter32_t *, osi_counter32_val_t init_val);
 *    -- initialize a counter object
 *
 *  osi_counter64_init(osi_counter64_t *, osi_counter64_val_t init_val);
 *    -- initialize a counter object
 *
 *  osi_counter_destroy(osi_counter_t *);
 *    -- destroy a counter object
 *
 *  osi_counter16_destroy(osi_counter16_t *);
 *    -- destroy a counter object
 *
 *  osi_counter32_destroy(osi_counter32_t *);
 *    -- destroy a counter object
 *
 *  osi_counter64_destroy(osi_counter64_t *);
 *    -- destroy a counter object
 *
 *  osi_counter_inc(osi_counter_t *);
 *    -- increment counter
 *
 *  osi_counter16_inc(osi_counter16_t *);
 *    -- increment counter
 *
 *  osi_counter32_inc(osi_counter32_t *);
 *    -- increment counter
 *
 *  osi_counter64_inc(osi_counter64_t *);
 *    -- increment counter
 *
 *  osi_counter_add(osi_counter_t *, osi_counter_delta_t inc);
 *    -- add $inc$ to counter
 *
 *  osi_counter16_add(osi_counter16_t *, osi_counter16_delta_t inc);
 *    -- add $inc$ to counter
 *
 *  osi_counter32_add(osi_counter32_t *, osi_counter32_delta_t inc);
 *    -- add $inc$ to counter
 *
 *  osi_counter64_add(osi_counter64_t *, osi_counter64_delta_t inc);
 *    -- add $inc$ to counter
 *
 * NOTICE: the following methods provide read access to a counter.
 *         please understand that these methods only provide an
 *         _approximate_ value on some platforms due to optimizing
 *         for performance rather than coherency
 *
 *  osi_counter_value(osi_counter_t *, osi_counter_val_t * val);
 *    -- store the "current" value of the counter in $val$
 *
 *  osi_counter16_value(osi_counter16_t *, osi_counter16_val_t * val);
 *    -- store the "current" value of the counter in $val$
 *
 *  osi_counter32_value(osi_counter32_t *, osi_counter32_val_t * val);
 *    -- store the "current" value of the counter in $val$
 *
 *  osi_counter64_value(osi_counter64_t *, osi_counter64_val_t * val);
 *    -- store the "current" value of the counter in $val$
 *
 * WARNING: the following interface must be used very carefully!
 *          it allows you to reset the value of a reference count
 *          in a non-atomic manner.  it is mostly useful in object
 *          caching scenarios when you are recycling a freed object
 *
 *  osi_counter_reset(osi_counter_t *, osi_counter_val_t new_val);
 *    -- reset the value of a previously initialized counter
 *
 *  osi_counter16_reset(osi_counter16_t *, osi_counter16_val_t new_val);
 *    -- reset the value of a previously initialized counter
 *
 *  osi_counter32_reset(osi_counter32_t *, osi_counter32_val_t new_val);
 *    -- reset the value of a previously initialized counter
 *
 *  osi_counter64_reset(osi_counter64_t *, osi_counter64_val_t new_val);
 *    -- reset the value of a previously initialized counter
 *
 * NOTE: The following operations use slower atomic operations than the
 *       interfaces defined above, and thus should be used only when
 *       absolutely necessary.
 * 
 *  osi_counter_inc_nv(osi_counter_t *, osi_counter_val_t * nv);
 *    -- increment counter and return new value in nv
 *
 *  osi_counter16_inc_nv(osi_counter16_t *, osi_counter_val_t * nv);
 *    -- increment counter and return new value in nv
 *
 *  osi_counter32_inc_nv(osi_counter32_t *, osi_counter_val_t * nv);
 *    -- increment counter and return new value in nv
 *
 *  osi_counter64_inc_nv(osi_counter64_t *, osi_counter_val_t * nv);
 *    -- increment counter and return new value in nv
 *
 *  osi_counter_add_nv(osi_counter_t *, osi_counter_delta_t inc, osi_counter_val_t * nv);
 *    -- add $inc$ to counter and return new value in nv
 *
 *  osi_counter16_add_nv(osi_counter16_t *, osi_counter16_delta_t inc, osi_counter16_val_t * nv);
 *    -- add $inc$ to counter and return new value in nv
 *
 *  osi_counter32_add_nv(osi_counter32_t *, osi_counter32_delta_t inc, osi_counter32_val_t * nv);
 *    -- add $inc$ to counter and return new value in nv
 *
 *  osi_counter64_add_nv(osi_counter64_t *, osi_counter64_delta_t inc, osi_counter64_val_t * nv);
 *    -- add $inc$ to counter and return new value in nv
 *
 *  int osi_counter_inc_action(osi_counter_t * counter, 
 *                             osi_counter_val_t thresh, 
 *                             osi_counter_action_t * action, 
 *                             void * sdata,
 *                             osi_result * res_out);
 *    -- increment $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter16_inc_action(osi_counter16_t * counter, 
 *                               osi_counter16_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- increment $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter32_inc_action(osi_counter32_t * counter, 
 *                               osi_counter32_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- increment $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter64_inc_action(osi_counter64_t * counter, 
 *                               osi_counter64_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- increment $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter_add_action(osi_counter_t * counter, 
 *                             osi_counter_delta_t inc,
 *                             osi_counter_val_t thresh, 
 *                             osi_counter_action_t * action, 
 *                             void * sdata,
 *                             osi_result * res_out);
 *    -- add $inc$ to $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter_add16_action(osi_counter16_t * counter, 
 *                               osi_counter16_delta_t inc,
 *                               osi_counter16_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- add $inc$ to $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter_add32_action(osi_counter32_t * counter, 
 *                               osi_counter32_delta_t inc,
 *                               osi_counter32_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- add $inc$ to $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 *  int osi_counter_add64_action(osi_counter64_t * counter, 
 *                               osi_counter64_delta_t inc,
 *                               osi_counter64_val_t thresh, 
 *                               osi_counter_action_t * action, 
 *                               void * sdata,
 *                               osi_result * res_out);
 *    -- add $inc$ to $counter$, if $counter$ is now equal to $thresh$ then
 *       call $action$, passing in $sdata$ as an argument,
 *       setting $res_out$ to the return code from $action$.
 *       return 1 if fired, 0 otherwise
 *
 */

typedef osi_result osi_counter_action_t(void * sdata);

#include <osi/COMMON/counter.h>
#include <osi/LEGACY/counter.h>


#endif /* _OSI_OSI_COUNTER_H */
