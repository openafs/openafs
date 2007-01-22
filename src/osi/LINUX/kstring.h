/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KSTRING_H
#define	_OSI_LINUX_KSTRING_H

#define osi_string_len(str) strlen(str)
#define osi_string_nlen(str,buf_len) strnlen(str,buf_len)
#define osi_string_cpy(dst,src) ((void)strcpy(dst,src))
#define osi_string_ncpy(dst,src,dst_len) ((void)strncpy(dst,src,dst_len))
#define osi_string_cat(dst,src) ((void)strcat(dst,src))
#define osi_string_ncat(dst,src,dst_len) ((void)strncat(dst,src,dst_len))
#define osi_string_cmp(a,b) strcmp(a,b)
#define osi_string_ncmp(a,b,len) strncmp(a,b,len)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define osi_string_lcpy(dst,src,dst_len) strlcpy(dst,src,dst_len)
#define osi_string_lcat(dst,src,dst_len) strlcat(dst,src,dst_len)
#else
#define OSI_IMPLEMENTS_LEGACY_STRING_LCPY 1
#define OSI_IMPLEMENTS_LEGACY_STRING_LCAT 1
#endif

#endif /* _OSI_LINUX_KSTRING_H */
