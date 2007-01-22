/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KSTRING_H
#define _OSI_SOLARIS_KSTRING_H 1

/* strnlen() is not implemented in solaris kernels */
#define OSI_IMPLEMENTS_LEGACY_STRING_NLEN 1

#define osi_string_len(str) strlen(str)
#define osi_string_cpy(dst,src) ((void)strcpy(dst,src))
#define osi_string_ncpy(dst,src,dst_len) ((void)strncpy(dst,src,dst_len))
#define osi_string_lcpy(dst,src,dst_len) strlcpy(dst,src,dst_len)
#define osi_string_cat(dst,src) ((void)strcat(dst,src))
#define osi_string_ncat(dst,src,dst_len) ((void)strncat(dst,src,dst_len))
#define osi_string_lcat(dst,src,dst_len) strlcat(dst,src,dst_len)
#define osi_string_cmp(a,b) strcmp(a,b)
#define osi_string_ncmp(a,b,len) strncmp(a,b,len)

#endif /* _OSI_SOLARIS_KSTRING_H */
