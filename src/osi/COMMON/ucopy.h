/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_UCOPY_H
#define _OSI_COMMON_UCOPY_H 1

#include <osi/osi_string.h>
#include <osi/osi_mem.h>

/*
 * common copyin/copyout support
 */

#define osi_kernel_cast_user_addr(x) ((char *)(x))


#define osi_kernel_copy_in(src,dst,len,code) \
    (osi_mem_copy((dst),osi_kernel_cast_user_addr(src),(len)),*(code)=0)
#define osi_kernel_copy_out(src,dst,len,code) \
    (osi_mem_copy(osi_kernel_cast_user_addr(dst),(src),(len)),*(code)=0)

#define osi_kernel_copy_in_string(src,dst,dst_len,str_len,code) \
    ((*(str_len))=osi_string_lcpy((dst),osi_kernel_cast_user_addr(src),(dst_len)), \
     (*(code)) = (((*(str_len)) >= (dst_len)) ? EFAULT : 0))
#define osi_kernel_copy_out_string(src,dst,dst_len,str_len,code) \
    ((*(str_len))=osi_string_lcpy(osi_kernel_cast_user_addr(dst),(src),(dst_len)), \
     (*(code)) = (((*(str_len)) >= (dst_len)) ? EFAULT : 0))

/* uio copy handler macros */

#define osi_kernel_handle_copy_in(src,dst,len,code,jump_target) \
    (osi_mem_copy((dst),osi_kernel_cast_user_addr(src),(len)),(code)=0)
#define osi_kernel_handle_copy_out(src,dst,len,code,jump_target) \
    (osi_mem_copy(osi_kernel_cast_user_addr(dst),(src),(len)),(code)=0)

#define osi_kernel_handle_copy_in_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        str_len = osi_string_lcpy((dst),osi_kernel_cast_user_addr(src),(dst_len)); \
        if (osi_compiler_expect_false((str_len) >= (dst_len))) { \
            code = EINVAL; \
            goto jump_target; \
        } \
    osi_Macro_End
#define osi_kernel_handle_copy_out_string(src,dst,dst_len,str_len,code,jump_target) \
    osi_Macro_Begin \
        str_len = osi_string_lcpy(osi_kernel_cast_user_addr(dst),(src),(dst_len)); \
        if (osi_compiler_expect_false((str_len) >= (dst_len))) { \
            code = EINVAL; \
            goto jump_target; \
        } \
    osi_Macro_End

#endif /* _OSI_COMMON_UCOPY_H */
