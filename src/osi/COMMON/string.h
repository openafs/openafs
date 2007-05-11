/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_STRING_H
#define _OSI_COMMON_STRING_H 1

#include <util/afsutil.h>

#if defined(OSI_ENV_USERSPACE)
#include <string.h>
#endif

#define osi_string_len(str) strlen(str)
#define osi_string_cpy(dst,src) ((void)strcpy(dst,src))
#define osi_string_ncpy(dst,src,dst_len) ((void)strncpy(dst,src,dst_len))
#define osi_string_cat(dst,src) ((void)strcat(dst,src))
#define osi_string_ncat(dst,src,dst_len) ((void)strncat(dst,src,dst_len))
#define osi_string_cmp(a,b) strcmp(a,b)
#define osi_string_ncmp(a,b,len) strncmp(a,b,len)

#if defined(OSI_ENV_KERNELSPACE) || defined(HAVE_STRNLEN)
#define osi_string_nlen(str,buf_len) strnlen(str,buf_len)
#else
#define OSI_IMPLEMENTS_LEGACY_STRING_NLEN 1
#endif

#if defined(OSI_ENV_KERNELSPACE) || defined(HAVE_STRLCPY)
#define osi_string_lcpy(dst,src,dst_len) strlcpy(dst,src,dst_len)
#else
#define OSI_IMPLEMENTS_LEGACY_STRING_LCPY 1
#endif

#if defined(OSI_ENV_KERNELSPACE) || defined(HAVE_STRLCAT)
#define osi_string_lcat(dst,src,dst_len) strlcat(dst,src,dst_len)
#else
#define OSI_IMPLEMENTS_LEGACY_STRING_LCAT 1
#endif

#endif /* _OSI_COMMON_STRING_H */
