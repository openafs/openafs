/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_USIGNAL_INLINE_H
#define _OSI_COMMON_USIGNAL_INLINE_H 1

/*
 * osi process userspace signal api
 * inlines
 */

osi_inline_define(
osi_signal_handler_t *
osi_signal_handler(osi_signal_t sig,
		   osi_signal_handler_t * fp)
{
    return (osi_signal_handler_t *) signal(sig, fp);
}
)

osi_inline_prototype(
osi_signal_handler_t *
osi_signal_handler(osi_signal_t sig,
		   osi_signal_handler_t * fp)
)

osi_inline_define(
osi_result
osi_signal_raise(osi_proc_p proc,
		 osi_signal_t sig)
{
    int code;

    code = kill(osi_proc_id(proc), sig);

    return (code == 0) ? OSI_OK : OSI_FAIL;
}
)

osi_inline_prototype(
osi_result
osi_signal_raise(osi_proc_p proc,
                 osi_signal_t sig)
)

#endif /* _OSI_COMMON_USIGNAL_INLINE_H */
