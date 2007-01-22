/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_COUNTER_INLINE_H
#define _OSI_COMMON_COUNTER_INLINE_H 1

/*
 * implementation of reference counting
 * using atomic operations
 */

#if defined(OSI_IMPLEMENTS_COUNTER_ATOMIC)

osi_inline_define(
void
osi_counter_value(osi_counter_t * counter, osi_counter_val_t * val_out)
{
    *val_out = *counter;
}
)
osi_inline_prototype(
void
osi_counter_value(osi_counter_t * counter, osi_counter_val_t * val_out)
)

osi_inline_define(
void
osi_counter_inc_nv(osi_counter_t * counter, osi_counter_val_t * nv)
{
    *nv = _osi_counter_inc_nv(counter);
}
)
osi_inline_prototype(
void
osi_counter_inc_nv(osi_counter_t * counter, osi_counter_val_t * nv)
)

osi_inline_define(
void
osi_counter_add_nv(osi_counter_t * counter, 
                   osi_counter_delta_t inc,
                   osi_counter_val_t * nv)
{
    *nv = _osi_counter_add_nv(counter, inc);
}
)
osi_inline_prototype(
void
osi_counter_add_nv(osi_counter_t * counter, 
                   osi_counter_delta_t inc,
                   osi_counter_val_t * nv)
)

osi_inline_define(
int
osi_counter_inc_action(osi_counter_t * counter, 
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter_inc_nv(counter) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter_inc_action(osi_counter_t * counter, 
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
)

osi_inline_define(
int
osi_counter_add_action(osi_counter_t * counter, 
                       osi_counter_delta_t inc,
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter_add_nv(counter, inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter_add_action(osi_counter_t * counter, 
                       osi_counter_delta_t inc,
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_COUNTER_ATOMIC */


#if defined(OSI_IMPLEMENTS_COUNTER16_ATOMIC)

osi_inline_define(
void
osi_counter16_value(osi_counter16_t * counter, osi_counter16_val_t * val_out)
{
    *val_out = *counter;
}
)
osi_inline_prototype(
void
osi_counter16_value(osi_counter16_t * counter, osi_counter16_val_t * val_out)
)

osi_inline_define(
void
osi_counter16_inc_nv(osi_counter16_t * counter, osi_counter16_val_t * nv)
{
    *nv = _osi_counter16_inc_nv(counter);
}
)
osi_inline_prototype(
void
osi_counter16_inc_nv(osi_counter16_t * counter, osi_counter16_val_t * nv)
)

osi_inline_define(
void
osi_counter16_add_nv(osi_counter16_t * counter, 
                     osi_counter16_delta_t inc,
                     osi_counter16_val_t * nv)
{
    *nv = _osi_counter16_add_nv(counter, inc);
}
)
osi_inline_prototype(
void
osi_counter16_add_nv(osi_counter16_t * counter, 
                     osi_counter16_delta_t inc,
                     osi_counter16_val_t * nv)
)

osi_inline_define(
int
osi_counter16_inc_action(osi_counter16_t * counter, 
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter16_inc_nv(counter) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter16_inc_action(osi_counter16_t * counter, 
                         osi_counter16_val_t thresh,
	                 osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter16_add_action(osi_counter16_t * counter, 
                         osi_counter16_delta_t inc,
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter16_add_nv(counter, inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter16_add_action(osi_counter16_t * counter, 
                         osi_counter16_delta_t inc,
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_COUNTER16_ATOMIC */


#if defined(OSI_IMPLEMENTS_COUNTER32_ATOMIC)

osi_inline_define(
void
osi_counter32_value(osi_counter32_t * counter, osi_counter32_val_t * val_out)
{
    *val_out = *counter;
}
)
osi_inline_prototype(
void
osi_counter32_value(osi_counter32_t * counter, osi_counter32_val_t * val_out)
)

osi_inline_define(
void
osi_counter32_inc_nv(osi_counter32_t * counter, osi_counter32_val_t * nv)
{
    *nv = _osi_counter32_inc_nv(counter);
}
)
osi_inline_prototype(
void
osi_counter32_inc_nv(osi_counter32_t * counter, osi_counter32_val_t * nv)
)

osi_inline_define(
void
osi_counter32_add_nv(osi_counter32_t * counter, 
                     osi_counter32_delta_t inc,
                     osi_counter32_val_t * nv)
{
    *nv = _osi_counter32_add_nv(counter, inc);
}
)
osi_inline_prototype(
void
osi_counter32_add_nv(osi_counter32_t * counter, 
                     osi_counter32_delta_t inc,
                     osi_counter32_val_t * nv)
)

osi_inline_define(
int
osi_counter32_inc_action(osi_counter32_t * counter, 
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter32_inc_nv(counter) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter32_inc_action(osi_counter32_t * counter, 
                         osi_counter32_val_t thresh,
	                 osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter32_add_action(osi_counter32_t * counter, 
                         osi_counter32_delta_t inc,
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter32_add_nv(counter, inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter32_add_action(osi_counter32_t * counter, 
                         osi_counter32_delta_t inc,
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_COUNTER32_ATOMIC */


#if defined(OSI_IMPLEMENTS_COUNTER64_ATOMIC)

osi_inline_define(
void
osi_counter64_value(osi_counter64_t * counter, osi_counter64_val_t * val_out)
{
    *val_out = *counter;
}
)
osi_inline_prototype(
void
osi_counter64_value(osi_counter64_t * counter, osi_counter64_val_t * val_out)
)

osi_inline_define(
void
osi_counter64_inc_nv(osi_counter64_t * counter, osi_counter64_val_t * nv)
{
    *nv = _osi_counter64_inc_nv(counter);
}
)
osi_inline_prototype(
void
osi_counter64_inc_nv(osi_counter64_t * counter, osi_counter64_val_t * nv)
)

osi_inline_define(
void
osi_counter64_add_nv(osi_counter64_t * counter, 
                     osi_counter64_delta_t inc,
                     osi_counter64_val_t * nv)
{
    *nv = _osi_counter64_add_nv(counter, inc);
}
)
osi_inline_prototype(
void
osi_counter64_add_nv(osi_counter64_t * counter, 
                     osi_counter64_delta_t inc,
                     osi_counter64_val_t * nv)
)

osi_inline_define(
int
osi_counter64_inc_action(osi_counter64_t * counter, 
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter64_inc_nv(counter) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter64_inc_action(osi_counter64_t * counter, 
                         osi_counter64_val_t thresh,
	                 osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter64_add_action(osi_counter64_t * counter, 
                         osi_counter64_delta_t inc,
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    if (_osi_counter64_add_nv(counter, inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_counter64_add_action(osi_counter64_t * counter, 
                         osi_counter64_delta_t inc,
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_COUNTER64_ATOMIC */

#endif /* _OSI_COMMON_COUNTER_INLINE_H */
