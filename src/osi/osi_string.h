/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_STRING_H
#define _OSI_OSI_STRING_H 1


/*
 * platform-independent osi_string API
 *
 *  osi_size_t osi_string_len(const char * src);
 *    -- strlen() semantics
 *
 *  osi_size_t osi_string_nlen(const char * src, osi_size_t buf_len);
 *    -- strnlen() semantics
 *
 *  void osi_string_cpy(char * dst, const char * src);
 *    -- srcpy() semantics
 *
 *  void osi_string_ncpy(char * dst, const char * src, osi_size_t dst_len);
 *    -- strncpy() semantics
 *
 *  osi_size_t osi_string_lcpy(char * dst, const char * src, osi_size_t dst_len);
 *    -- strlcpy() semantics
 *
 *  void osi_string_cat(char * dst, const char * src)
 *    -- strcat() semantics
 *
 *  void osi_string_ncat(char * dst, const char * src, osi_size_t dst_len);
 *    -- strncat() semantics
 *
 *  osi_size_t osi_string_lcat(char * dst, const char * src, osi_size_t dst_len);
 *    -- strlcat() semantics
 *
 *  int osi_string_cmp(const char * s1, const char * s2);
 *    -- strcmp() semantics
 *
 *  int osi_string_ncmp(const char * s1, const char * s2, osi_size_t n);
 *    -- strncmp() semantics
 */

/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kstring.h>
#elif defined(OSI_LINUX_ENV)
#include <osi/LINUX/kstring.h>
#else
/* fall back and hope typical userspace api exists in this kernel... */
#include <osi/COMMON/string.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#include <osi/COMMON/string.h>

#endif /* !OSI_KERNELSPACE_ENV */

#include <osi/LEGACY/string.h>

#endif /* _OSI_OSI_STRING_H */
