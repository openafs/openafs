/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* strnlen.c - fixed length string length */

#include <osi/osi_impl.h>
#include <osi/osi_string.h>

RCSID
    ("$Header$");

#if defined(OSI_IMPLEMENTS_LEGACY_STRING_NLEN)

#include <sys/types.h>
#include <stdarg.h>
#include <ctype.h>


osi_size_t
osi_string_nlen(const char * buf, osi_size_t len)
{
    osi_size_t i;

    for (i = 0; i < len; i++) {
	if (buf[i] == '\0')
	    break;
    }

    return i;
}
#endif /* OSI_IMPLEMENTS_LEGACY_STRING_NLEN */
