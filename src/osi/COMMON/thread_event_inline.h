/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_EVENT_INLINE_H
#define _OSI_COMMON_THREAD_EVENT_INLINE_H 1

/*
 * common thread event handler implementation
 * inlines
 */

#include <osi/osi_mem.h>


osi_inline_define(
osi_result
__osi_thread_run_args_set_fp(struct osi_thread_run_arg * arg,
			     osi_thread_func_t * fp)
{
    arg->fp = fp;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result __osi_thread_run_args_set_fp(struct osi_thread_run_arg *,
					osi_thread_func_t *)
)

osi_inline_define(
osi_result
__osi_thread_run_args_set_args(struct osi_thread_run_arg * arg,
			       void * argv,
			       size_t argv_len)
{
    arg->arg = argv;
    arg->arg_len = argv_len;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result __osi_thread_run_args_set_args(struct osi_thread_run_arg *,
                                          void *, size_t)
)

osi_inline_define(
osi_result
__osi_thread_run_args_set_opts(struct osi_thread_run_arg * arg,
                               osi_thread_options_t * opts)
{
    osi_thread_options_Copy(&arg->opt, opts);
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result __osi_thread_run_args_set_opts(struct osi_thread_run_arg *,
                                          osi_thread_options_t *)
)

osi_inline_define(
osi_thread_func_t *
__osi_thread_run_args_get_fp(struct osi_thread_run_arg * arg)
{
    return arg->fp;
}
)
osi_inline_prototype(
osi_thread_func_t * __osi_thread_run_args_get_fp(struct osi_thread_run_arg *)
)

osi_inline_define(
osi_thread_options_t *
__osi_thread_run_args_get_opts(struct osi_thread_run_arg * arg)
{
    return &arg->opt;
}
)
osi_inline_prototype(
osi_thread_options_t * __osi_thread_run_args_get_opts(struct osi_thread_run_arg *)
)

#endif /* _OSI_COMMON_THREAD_EVENT_INLINE_H */
