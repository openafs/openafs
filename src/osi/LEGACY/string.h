/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_STRING_H
#define	_OSI_LEGACY_STRING_H

#if defined(OSI_IMPLEMENTS_LEGACY_STRING_NLEN)
osi_extern osi_size_t osi_string_nlen(const char * buf, osi_size_t buf_len);
#endif

#if defined(OSI_IMPLEMENTS_LEGACY_STRING_LCAT)
osi_extern osi_size_t osi_string_lcat(char * dst, const char * src, osi_size_t buf_len);
#endif

#if defined(OSI_IMPLEMENTS_LEGACY_STRING_LCPY)
osi_extern osi_size_t osi_string_lcpy(char * dst, const char * src, osi_size_t buf_len);
#endif

#endif /* _OSI_LEGACY_STRING_H */
