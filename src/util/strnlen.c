/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* strnlen.c - fixed length string length */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stdarg.h>
#include <ctype.h>


size_t
afs_strnlen(char * buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
	if (buf[i] == '\0')
	    break;
    }

    return i;
}

