/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "afsutil.h"

static char *c_xlate = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


/* int_to_base32
 * Create a base 32 string representation of a number.
 * The supplied string 's' must be at least 8 bytes long.
 * Use the b32_string_t tyepdef.
 */
char *
int_to_base32(b32_string_t s, int a)
{
    int i, j;
    unsigned int n;

    i = 0;
    if (a == 0)
	s[i++] = c_xlate[0];
    else {
	j = 25;
	n = a & 0xc0000000;
	if (n) {
	    n >>= 30;
	    s[i++] = c_xlate[n];
	    a &= ~0xc0000000;
	} else {
	    for (; j >= 0; j -= 5) {
		n = a & (0x1f << j);
		if (n)
		    break;	/* found highest bits set. */
	    }
	    s[i++] = c_xlate[n >> j];
	    a &= ~(0x1f << j);
	    j -= 5;
	}
	/* more to do. */
	for (; j >= 0; j -= 5) {
	    n = a & (0x1f << j);
	    s[i++] = c_xlate[n >> j];
	    a &= ~(0x1f << j);
	}
    }
    s[i] = '\0';
    return s;
}

int
base32_to_int(char *s)
{
    int n = 0;
    int result = 0;

    for (; *s; s++) {
	if (*s <= '9') {
	    n = (int)(*s - '0');
	} else {
	    n = 10 + (int)(*s - 'A');
	}
	result = (result << 5) + n;
    }
    return result;
}
