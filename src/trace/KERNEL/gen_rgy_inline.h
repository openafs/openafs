/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_GEN_RGY_INLINE_H
#define _OSI_TRACE_KERNEL_GEN_RGY_INLINE_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 * inline functions
 */

osi_inline_prototype(
void
osi_trace_gen_rgy_get(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_get_nl(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_put(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_put_nl(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_put_nz(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_put_nl_nz(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
void
osi_trace_gen_rgy_mailbox_put(osi_trace_mailbox_t * mbox)
)

/*
 * get a ref on a gen object
 *
 * [IN] gen  -- gen object pointer
 *
 */
osi_inline_define(
void
osi_trace_gen_rgy_get(osi_trace_generator_registration_t * gen)
{
    osi_mutex_Lock(&gen->lock);
    gen->refcnt++;
    osi_mutex_Unlock(&gen->lock);
}
)

/*
 * get a ref on a gen object
 *
 * [IN] gen  -- gen object pointer
 *
 * preconditions:
 *   gen->lock held
 */
osi_inline_define(
void
osi_trace_gen_rgy_get_nl(osi_trace_generator_registration_t * gen)
{
    gen->refcnt++;
}
)

/*
 * put back a ref on a gen object
 *
 * [IN] gen  -- gen object pointer
 *
 * see: "theory behind gen object deallocation" comment
 *      in src/trace/KERNEL/gen_rgy.c
 *
 * preconditions:
 *   ref on gen held
 *
 * postconditions:
 *   gen ref dropped
 *   gen registered on gc list if refcount reached zero
 */
osi_inline_define(
void
osi_trace_gen_rgy_put(osi_trace_generator_registration_t * gen)
{
    osi_mutex_Lock(&gen->lock);
    if (!(--gen->refcnt)) {
        /* __osi_trace_gen_rgy_free drops gen->lock internally */
	__osi_trace_gen_rgy_free(gen);
    } else {
        osi_mutex_Unlock(&gen->lock);
    }
}
)


/*
 * put back a ref on a gen object
 *
 * [IN] gen  -- gen object pointer
 *
 * see: "theory behind gen object deallocation" comment
 *      in src/trace/KERNEL/gen_rgy.c
 *
 * preconditions:
 *   ref on gen held
 *   gen->lock held
 *
 * postconditions:
 *   gen ref dropped
 *   gen->lock dropped
 *   gen registered on gc list if refcount reached zero
 */
osi_inline_define(
void
osi_trace_gen_rgy_put_nl(osi_trace_generator_registration_t * gen)
{
    if (!(--gen->refcnt)) {
        /* __osi_trace_gen_rgy_free drops gen->lock internally */
	__osi_trace_gen_rgy_free(gen);
    } else {
        osi_mutex_Unlock(&gen->lock);
    }
}
)


/*
 * put back a ref on a gen object, resulting count will be positive
 *
 * [IN] gen  -- gen object pointer
 *
 * see: "theory behind gen object deallocation" comment
 *      in src/trace/KERNEL/gen_rgy.c
 *
 * preconditions:
 *   multiple refs on gen held
 *
 * postconditions:
 *   gen ref dropped
 */
osi_inline_define(
void
osi_trace_gen_rgy_put_nz(osi_trace_generator_registration_t * gen)
{
    osi_mutex_Lock(&gen->lock);
    osi_AssertDebug(gen->refcnt > 1);
    gen->refcnt--;
    osi_mutex_Unlock(&gen->lock);
}
)


/*
 * put back a ref on a gen object, resulting count will be positive
 *
 * [IN] gen  -- gen object pointer
 *
 * see: "theory behind gen object deallocation" comment
 *      in src/trace/KERNEL/gen_rgy.c
 *
 * preconditions:
 *   multiple refs on gen held
 *   gen->lock held
 *
 * postconditions:
 *   gen ref dropped
 */
osi_inline_define(
void
osi_trace_gen_rgy_put_nl_nz(osi_trace_generator_registration_t * gen)
{
    osi_AssertDebug(gen->refcnt > 1);
    gen->refcnt--;
}
)


/*
 * put back a gen's mailbox
 *
 * [IN] mbox  -- mailbox object pointer
 *
 * returns:
 *   OSI_OK always
 */
osi_inline_define(
void
osi_trace_gen_rgy_mailbox_put(osi_trace_mailbox_t * mbox)
{
    osi_trace_gen_rgy_put(mbox->gen);
}
)


     /*
osi_inline_define(
osi_result
osi_trace_gen_id(osi_trace_gen_id_t * id)
{
    *id = 0;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result osi_trace_gen_id(osi_trace_gen_id_t * id)
)
     */

			  
#endif /* _OSI_TRACE_KERNEL_GEN_RGY_INLINE_H */
