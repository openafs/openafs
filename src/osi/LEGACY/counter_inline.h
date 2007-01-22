/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LEGACY_COUNTER_INLINE_H
#define	_OSI_OSI_LEGACY_COUNTER_INLINE_H

/*
 * implementation of reference counting
 * using spinlocks
 * inline functions
 */

#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER)
osi_inline_define(
void
osi_counter_value(osi_counter_t * obj, osi_counter_val_t * val_out)
{
    *val_out = obj->counter;
}
)
osi_inline_prototype(
void
osi_counter_value(osi_counter_t * obj, osi_counter_val_t * val_out)
)

osi_inline_define(
void
osi_counter_inc(osi_counter_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter++;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter_inc(osi_counter_t * obj)
)

osi_inline_define(
void
osi_counter_inc_nv(osi_counter_t * obj, osi_counter_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = ++obj->counter;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter_inc_nv(osi_counter_t * obj, osi_counter_val_t * nv)
)

osi_inline_define(
void
osi_counter_add(osi_counter_t * obj, osi_counter_delta_t inc)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter += inc;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter_add(osi_counter_t * obj, osi_counter_delta_t inc)
)

osi_inline_define(
void
osi_counter_add_nv(osi_counter_t * obj, 
                   osi_counter_delta_t inc,
                   osi_counter_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = (obj->counter += inc);
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter_add_nv(osi_counter_t * obj, 
                   osi_counter_delta_t inc,
                   osi_counter_val_t * nv)
)

osi_inline_define(
int
osi_counter_inc_action(osi_counter_t * obj, 
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((++(obj->counter)) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter_inc_action(osi_counter_t * obj, 
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
)

osi_inline_define(
int
osi_counter_add_action(osi_counter_t * obj, 
                       osi_counter_delta_t inc,
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((obj->counter += inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter_add_action(osi_counter_t * obj, 
                       osi_counter_delta_t inc,
                       osi_counter_val_t thresh,
		       osi_counter_action_t * action, 
                       void * sdata,
		       osi_result * res_out)
)
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER16)
osi_inline_define(
void
osi_counter16_value(osi_counter16_t * obj, osi_counter16_val_t * val_out)
{
    *val_out = obj->counter;
}
)
osi_inline_prototype(
void
osi_counter16_value(osi_counter16_t * obj, osi_counter16_val_t * val_out)
)

osi_inline_define(
void
osi_counter16_inc(osi_counter16_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter++;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter16_inc(osi_counter16_t * obj)
)

osi_inline_define(
void
osi_counter16_inc_nv(osi_counter16_t * obj, osi_counter16_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = ++obj->counter;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter16_inc_nv(osi_counter16_t * obj, osi_counter16_val_t * nv)
)

osi_inline_define(
void
osi_counter16_add(osi_counter16_t * obj, osi_counter16_delta_t inc)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter += inc;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter16_add(osi_counter16_t * obj, osi_counter16_delta_t inc)
)

osi_inline_define(
void
osi_counter16_add_nv(osi_counter16_t * obj, 
                     osi_counter16_delta_t inc,
                     osi_counter16_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = (obj->counter += inc);
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter16_add_nv(osi_counter16_t * obj, 
                     osi_counter16_delta_t inc,
                     osi_counter16_val_t * nv)
)

osi_inline_define(
int
osi_counter16_inc_action(osi_counter16_t * obj, 
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((++(obj->counter)) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter16_inc_action(osi_counter16_t * obj, 
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter16_add_action(osi_counter16_t * obj, 
                         osi_counter16_delta_t inc,
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((obj->counter += inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter16_add_action(osi_counter16_t * obj, 
                         osi_counter16_delta_t inc,
                         osi_counter16_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)
#endif /* !OSI_IMPLEMENTS_LEGACY_COUNTER16 */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER32)
osi_inline_define(
void
osi_counter32_value(osi_counter32_t * obj, osi_counter32_val_t * val_out)
{
    *val_out = obj->counter;
}
)
osi_inline_prototype(
void
osi_counter32_value(osi_counter32_t * obj, osi_counter32_val_t * val_out)
)

osi_inline_define(
void
osi_counter32_inc(osi_counter32_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter++;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter32_inc(osi_counter32_t * obj)
)

osi_inline_define(
void
osi_counter32_inc_nv(osi_counter32_t * obj, osi_counter32_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = ++obj->counter;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter32_inc_nv(osi_counter32_t * obj, osi_counter32_val_t * nv)
)

osi_inline_define(
void
osi_counter32_add(osi_counter32_t * obj, osi_counter32_delta_t inc)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter += inc;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter32_add(osi_counter32_t * obj, osi_counter32_delta_t inc)
)

osi_inline_define(
void
osi_counter32_add_nv(osi_counter32_t * obj, 
                     osi_counter32_delta_t inc,
                     osi_counter32_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = (obj->counter += inc);
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter32_add_nv(osi_counter32_t * obj, 
                     osi_counter32_delta_t inc,
                     osi_counter32_val_t * nv)
)

osi_inline_define(
int
osi_counter32_inc_action(osi_counter32_t * obj, 
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((++(obj->counter)) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter32_inc_action(osi_counter32_t * obj, 
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter32_add_action(osi_counter32_t * obj, 
                         osi_counter32_delta_t inc,
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((obj->counter += inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter32_add_action(osi_counter32_t * obj, 
                         osi_counter32_delta_t inc,
                         osi_counter32_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER32 */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER64)
osi_inline_define(
void
osi_counter64_value(osi_counter64_t * obj, osi_counter64_val_t * val_out)
{
    *val_out = obj->counter;
}
)
osi_inline_prototype(
void
osi_counter64_value(osi_counter64_t * obj, osi_counter64_val_t * val_out)
)

osi_inline_define(
void
osi_counter64_inc(osi_counter64_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter++;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter64_inc(osi_counter64_t * obj)
)

osi_inline_define(
void
osi_counter64_inc_nv(osi_counter64_t * obj, osi_counter64_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = ++obj->counter;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter64_inc_nv(osi_counter64_t * obj, osi_counter64_val_t * nv)
)

osi_inline_define(
void
osi_counter64_add(osi_counter64_t * obj, osi_counter64_delta_t inc)
{
    osi_spinlock_Lock(&obj->lock);
    obj->counter += inc;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter64_add(osi_counter64_t * obj, osi_counter64_delta_t inc)
)

osi_inline_define(
void
osi_counter64_add_nv(osi_counter64_t * obj, 
                     osi_counter64_delta_t inc,
                     osi_counter64_val_t * nv)
{
    osi_spinlock_Lock(&obj->lock);
    *nv = (obj->counter += inc);
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_counter64_add_nv(osi_counter64_t * obj, 
                     osi_counter64_delta_t inc,
                     osi_counter64_val_t * nv)
)

osi_inline_define(
int
osi_counter64_inc_action(osi_counter64_t * obj, 
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((++(obj->counter)) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter64_inc_action(osi_counter64_t * obj, 
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)

osi_inline_define(
int
osi_counter64_add_action(osi_counter64_t * obj, 
                         osi_counter64_delta_t inc,
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
{
    int fired = 0;
    osi_spinlock_Lock(&obj->lock);
    if ((obj->counter += inc) == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    osi_spinlock_Unlock(&obj->lock);
    return fired;
}
)
osi_inline_prototype(
int
osi_counter64_add_action(osi_counter64_t * obj, 
                         osi_counter64_delta_t inc,
                         osi_counter64_val_t thresh,
		         osi_counter_action_t * action, 
                         void * sdata,
		         osi_result * res_out)
)
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER64 */

#endif /* _OSI_OSI_LEGACY_COUNTER_INLINE_H */
