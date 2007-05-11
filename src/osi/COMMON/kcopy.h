/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_KCOPY_H
#define _OSI_COMMON_KCOPY_H 1


/*
 * common copyin/copyout support
 */


#if defined(OSI_ENV_KERNELSPACE)


#if defined(CAST_USER_ADDR_T)
#define osi_kernel_cast_user_addr(x) (CAST_USER_ADDR_T(x))
#else
#define osi_kernel_cast_user_addr(x) ((char *)(x))
#endif

#if !defined(OSI_KERNEL_OS_SPEC_UIO_COPY)
#define osi_kernel_copy_in(src,dst,len,code) \
    ((*(code))=copyin(osi_kernel_cast_user_addr(src),(dst),(len)))
#define osi_kernel_copy_out(src,dst,len,code) \
    ((*(code))=copyout((src),osi_kernel_cast_user_addr(dst),(len)))
#define osi_kernel_copy_in_string(src,dst,dst_len,str_len,code) \
    ((*(code))=copyinstr(osi_kernel_cast_user_addr(src),(dst),(dst_len),(str_len)))
#define osi_kernel_copy_out_string(src,dst,dst_len,str_len,code) \
    ((*(code))=copyoutstr((src),osi_kernel_cast_user_addr(dst),(dst_len),(str_len)))
#define osi_kernel_handle_copy_in(src,dst,len,code,jump_target) \
    osi_Macro_Begin \
        code = copyin(osi_kernel_cast_user_addr(src),(dst),(len)); \
        if (osi_compiler_expect_false(code != 0)) { \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_out(src,dst,len,code,jump_target) \
    osi_Macro_Begin \
        code = copyout((src),osi_kernel_cast_user_addr(dst),(len)); \
        if (osi_compiler_expect_false(code != 0)) { \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_in_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        code = copyinstr(osi_kernel_cast_user_addr(src),(dst),(dst_len),&(str_len)); \
        if (osi_compiler_expect_false((code != 0) || ((str_len) >= (dst_len)))) { \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_out_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        code = copyoutstr((src),osi_kernel_cast_user_addr(dst),(dst_len),&(str_len)); \
        if (osi_compiler_expect_false((code != 0) || ((str_len) >= (dst_len)))) { \
            goto jump_target; \
        } \
    osi_Macro_End
#endif /* !OSI_KERNEL_OS_SPEC_UIO_COPY */

#endif /* OSI_ENV_KERNELSPACE */


#endif /* _OSI_COMMON_KCOPY_H */
