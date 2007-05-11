/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_MEM_LOCAL_INLINE_H
#define _OSI_COMMON_MEM_LOCAL_INLINE_H 1



/*
 * context-local getters and setters
 */


#if defined(OSI_ENV_KERNELSPACE) && !defined(OSI_MEM_LOCAL_PREEMPT_INTERNAL)

osi_inline_define(
void *
osi_mem_local_get(osi_mem_local_key_t key)
{
    osi_kernel_preemption_disable();
    return osi_mem_local_ctx_get()->keys[key];
}
)

osi_inline_prototype(
void *
osi_mem_local_get(osi_mem_local_key_t key)
)

#endif /* OSI_ENV_KERNELSPACE && !OSI_MEM_LOCAL_PREEMPT_INTERNAL */


#endif /* _OSI_COMMON_MEM_LOCAL_INLINE_H */
