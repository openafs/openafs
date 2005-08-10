/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/util/flipbase64.c,v 1.10.2.1 2005/05/08 06:10:26 shadow Exp $");


#if defined(AFS_NAMEI_ENV)
#include <sys/types.h>
#include "afsutil.h"

/* This version of base64 gets it right and starts converting from the low
 * bits to the high bits. 
 */
/* This table needs to be in lexical order to efficiently map back from
 * characters to the numerical value.
 *
 * In c_reverse, we use 99 to represent an illegal value, rather than -1
 * which would assume "char" is signed.
 */
#ifdef AFS_DARWIN_ENV
static char c_xlate[80] =
        "!\"#$%&()*+,-0123456789:;<=>?@[]^_`abcdefghijklmnopqrstuvwxyz{|}~";
static char c_reverse[] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99,  0,  1,  2,  3,  4,  5, 99,  6,  7,  8,  9, 10, 11, 99, 99,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 29, 99, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};
#else /* AFS_DARWIN_ENV */
static char c_xlate[80] =
    "+=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char c_reverse[] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,  0, 99, 99, 99, 99,
     2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 99, 99, 99,  1, 99, 99,
    99, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 99, 99, 99, 99, 99,
    99, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};
#endif /* AFS_DARWIN_ENV */

/* int_to_base64
 * Create a base 64 string representation of a number.
 * The supplied string 's' must be at least 12 bytes long.
 * lb64_string in stds.h provides a typedef to get the length.
 */
#ifdef AFS_64BIT_ENV
char *
int64_to_flipbase64(lb64_string_t s, afs_uint64 a)
#else
char *
int64_to_flipbase64(lb64_string_t s, u_int64_t a)
#endif
{
    int i;
#ifdef AFS_64BIT_ENV
    afs_uint64 n;
#else
    u_int64_t n;
#endif

    i = 0;
    if (a == 0)
	s[i++] = c_xlate[0];
    else {
	for (n = a & 0x3f; a; n = ((a >>= 6) & 0x3f)) {
	    s[i++] = c_xlate[n];
	}
    }
    s[i] = '\0';
    return s;
}


#ifdef AFS_64BIT_ENV
afs_int64
flipbase64_to_int64(char *s)
#else
int64_t
flipbase64_to_int64(char *s)
#endif
{
#ifdef AFS_64BIT_ENV
    afs_int64 n = 0;
    afs_int64 result = 0;
#else
    int64_t n = 0;
    int64_t result = 0;
#endif
    int shift;

    for (shift = 0; *s; s++) {
        n = c_reverse[*(unsigned char *)s];
        if (n >= 64)    /* should never happen */
            continue;
        n <<= shift;
        result |= n ;
        shift += 6;
    }
    return result;
}


#endif /* AFS_NAMEI_ENV */
