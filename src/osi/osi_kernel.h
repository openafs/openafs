/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_KERNEL_H
#define _OSI_OSI_KERNEL_H 1


/*
 * platform-independent osi_kernel API
 *
 *
 * low-level user notification interfaces:
 *
 *  osi_kernel_printf(const char * msg, ...);
 *    -- low-level printf routine (try to direct at system console)
 *
 *  osi_kernel_uprintf(onst char * msg, ...);
 *    -- low-level printf routine (try to direct at user's session)
 *
 * macro interfaces:
 *
 *  osi_kernel_cast_user_addr(x)
 *    -- cast $x$ from user to kernel address type
 *
 * raw uio copy interfaces:
 *
 *  int osi_kernel_copy_in(const char * src, char * dst, size_t len, int * code);
 *    -- copy blob into kernel:
 *        from address $src$ in userspace context
 *        to address $dst$ in kernelspace context
 *        transfer $len$ contiguous bytes
 *        stores error code in $code$
 *        returns 0 on success; non-zero otherwise
 *
 *  int osi_kernel_copy_out(const char * src, char * dst, size_t len, int * code);
 *    -- copy blob into userspace context:
 *        to address $dst$ in userspace context
 *        from address $src$ in kernelspace context
 *        transfer $len$ contiguous bytes
 *        stores error code in $code$
 *        returns 0 on success; non-zero otherwise
 *
 *  int osi_kernel_copy_in_string(const char * src, char * dst, size_t dst_len,
 *                                 size_t * str_len, int * code);
 *    -- copy string into kernel:
 *        from address $src$ in userspace context
 *        to address $dst$ in kernelspace context
 *        transfer at most $dst_len$ contiguous bytes 
 *        store the string len into *$str_len$
 *        stores error code in $code$
 *        returns 0 on success; non-zero otherwise
 *
 *  int osi_kernel_copy_out_string(const char * src, char * dst, size_t dst_len,
 *                                 size_t * str_len, int * code);
 *    -- copy string into userspace context:
 *        from address $src$ in kernelspace context
 *        to address $dst$ in userspace context
 *        transfer at most $dst_len$ contiguous bytes 
 *        store the string len into *$str_len$
 *        stores error code in $code$
 *        returns 0 on success; non-zero otherwise
 *
 * specialized uio copy macro interfaces which automatically handle jumping to error handlers:
 *
 *  osi_kernel_handle_copy_in(const char * src, char * dst, size_t len, 
 *                            int code, jump_tag);
 *    -- copy blob into kernel:
 *        from address $src$ in userspace context
 *        to address $dst$ in kernelspace context
 *        transfer $len$ contiguous bytes
 *        stores error code in $code$
 *        performs goto $jump_tag$ on error
 *
 *  osi_kernel_handle_copy_out(const char * src, char * dst, size_t len, 
 *                             int code, jump_tag);
 *    -- copy blob into userspace context:
 *        to address $dst$ in userspace context
 *        from address $src$ in kernelspace context
 *        transfer $len$ contiguous bytes
 *        stores error code in $code$
 *        performs goto $jump_tag$ on error
 *
 *  osi_kernel_handle_copy_in_string(const char * src, char * dst, size_t dst_len,
 *                                   size_t str_len, int code, jump_tag);
 *    -- copy string into kernel:
 *        from address $src$ in userspace context
 *        to address $dst$ in kernelspace context
 *        transfer at most $dst_len$ contiguous bytes 
 *        store the string len into *$str_len$
 *        stores error code in $code$
 *        performs goto $jump_tag$ on error
 *
 *  osi_kernel_handle_copy_out_string(const char * src, char * dst, size_t dst_len,
 *                                    size_t str_len, int code, jump_tag);
 *    -- copy string into userspace context:
 *        from address $src$ in kernelspace context
 *        to address $dst$ in userspace context
 *        transfer at most $dst_len$ contiguous bytes 
 *        store the string len into *$str_len$
 *        stores error code in $code$
 *        performs goto $jump_tag$ on error
 *
 * the following two interfaces require
 * defined(OSI_IMPLEMENTS_KERNEL_PREEMPTION_CONTROL)
 *  void osi_kernel_preemption_disable();
 *    -- disable preemption on this cpu
 *
 *  void osi_kernel_preemption_enable();
 *    -- enable preemption on this cpu
 */

/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kernel.h>
#elif defined(OSI_LINUX20_ENV)
#include <osi/LINUX/kernel.h>
#else
#error "please port the osi kernel api to your platform"
#endif

#include <osi/COMMON/kcopy.h>

#else

#include <osi/COMMON/ucopy.h>

#endif


#endif /* _OSI_OSI_KERNEL_H */
