/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KERNEL_H
#define _OSI_SOLARIS_KERNEL_H


#include <sys/disp.h>
#include <sys/cpuvar.h>

#define osi_kernel_printf   printf
#define osi_kernel_uprintf  uprintf

#define OSI_IMPLEMENTS_KERNEL_PREEMPTION_CONTROL 1

#define osi_kernel_preemption_disable()   kpreempt_disable()
#define osi_kernel_preemption_enable()    kpreempt_enable()


#define OSI_KERNEL_OS_SPEC_UIO_COPY 1      /* solaris copyin/copyout don't return helpful error codes */

#define osi_kernel_copy_in(src,dst,len,code) \
    ((*(code)) = (copyin(osi_kernel_cast_user_addr(src),(dst),(len))) ? EFAULT : 0)
#define osi_kernel_copy_out(src,dst,len,code) \
    ((*(code)) = (copyout((src),osi_kernel_cast_user_addr(dst),(len))) ? EFAULT : 0)
#define osi_kernel_copy_in_string(src,dst,dst_len,str_len,code) \
    ((*(code)) = (copyinstr(osi_kernel_cast_user_addr(src),(dst),(dst_len),(str_len))) ? EFAULT : 0)
#define osi_kernel_copy_out_string(src,dst,dst_len,str_len,code) \
    ((*(code)) = (copyoutstr((src),osi_kernel_cast_user_addr(dst),(dst_len),(str_len))) ? EFAULT : 0)

#define osi_kernel_handle_copy_in(src,dst,len,code,jump_target) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(copyin(osi_kernel_cast_user_addr(src),(dst),(len)))) { \
            code = EFAULT; \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_out(src,dst,len,code,jump_target) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(copyout((src),osi_kernel_cast_user_addr(dst),(len)))) { \
            code = EFAULT; \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_in_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(copyinstr(osi_kernel_cast_user_addr(src),(dst), \
                                                (dst_len),&(str_len)))) { \
            code = EFAULT; \
            goto jump_target; \
        } \
        if (osi_compiler_expect_false((str_len) >= (dst_len))) { \
            code = EINVAL; \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_out_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(copyoutstr((src),osi_kernel_cast_user_addr(dst), \
                                                 (dst_len),&(str_len)))) { \
            code = EFAULT; \
            goto jump_target; \
        } \
        if (osi_compiler_expect_false((str_len) >= (dst_len))) { \
            code = EINVAL; \
            goto jump_target; \
        } \
    osi_Macro_End

#endif /* _OSI_SOLARIS_KERNEL_H */
