/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LEGACY_REFCNT_INLINE_H
#define	_OSI_OSI_LEGACY_REFCNT_INLINE_H

/*
 * implementation of reference counting
 * using spinlocks
 * inline functions
 */

#if defined(OSI_IMPLEMENTS_LEGACY_REFCNT)

osi_inline_define(
void
osi_refcnt_reset(osi_refcnt_t * obj, osi_refcnt_val_t val)
{
    obj->refcnt = val;
}
)
osi_inline_prototype(
void
osi_refcnt_reset(osi_refcnt_t * obj, osi_refcnt_val_t val)
)

osi_inline_define(
void
osi_refcnt_inc(osi_refcnt_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->refcnt++;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_refcnt_inc(osi_refcnt_t * obj)
)

osi_inline_define(
void
osi_refcnt_dec(osi_refcnt_t * obj)
{
    osi_spinlock_Lock(&obj->lock);
    obj->refcnt--;
    osi_spinlock_Unlock(&obj->lock);
}
)
osi_inline_prototype(
void
osi_refcnt_dec(osi_refcnt_t * obj)
)

osi_inline_define(
int
osi_refcnt_inc_action(osi_refcnt_t * obj, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
{
    int fired = 0;
    osi_refcnt_val_t nv;

    osi_spinlock_Lock(&obj->lock);
    nv = ++obj->refcnt;
    osi_spinlock_Unlock(&obj->lock);

    if (nv == thresh) {
	*res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_refcnt_inc_action(osi_refcnt_t * obj, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
)

osi_inline_define(
int
osi_refcnt_dec_action(osi_refcnt_t * obj, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
{
    int fired = 0;
    osi_refcnt_val_t nv;

    osi_spinlock_Lock(&obj->lock);
    nv = --obj->refcnt;
    osi_spinlock_Unlock(&obj->lock);

    if (nv == thresh) {
        *res_out = (*action)(sdata);
	fired = 1;
    }
    return fired;
}
)
osi_inline_prototype(
int
osi_refcnt_dec_action(osi_refcnt_t * obj, osi_refcnt_val_t thresh,
		      osi_refcnt_action_t * action, void * sdata,
		      osi_result * res_out)
)

#endif /* OSI_IMPLEMENTS_LEGACY_REFCNT */

#endif /* _OSI_OSI_LEGACY_REFCNT_INLINE_H */
