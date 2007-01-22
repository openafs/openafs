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
osi_result
osi_trace_gen_rgy_put(osi_trace_generator_registration_t * gen)
)
osi_inline_prototype(
osi_result
osi_trace_gen_rgy_mailbox_put(osi_trace_mailbox_t * mbox)
)

/*
 * get a ref on a gen object
 *
 * [IN] gen  -- gen object pointer
 *
 * returns:
 *   OSI_OK always
 */
osi_inline_define(
void
osi_trace_gen_rgy_get(osi_trace_generator_registration_t * gen)
{
    osi_refcnt_inc(&gen->refcnt);
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
 * returns:
 *   OSI_OK always
 */
osi_inline_define(
osi_result
osi_trace_gen_rgy_put(osi_trace_generator_registration_t * gen)
{
    osi_result res;

    do {
        res = OSI_OK;
        osi_refcnt_dec_action(&gen->refcnt,
			      0,
			      &__osi_trace_gen_rgy_free,
			      gen,
			      &res);
    } while (OSI_RESULT_FAIL_UNLIKELY(res));

    return res;
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
osi_result
osi_trace_gen_rgy_mailbox_put(osi_trace_mailbox_t * mbox)
{
    return osi_trace_gen_rgy_put(mbox->gen);
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
