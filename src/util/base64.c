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



#ifdef AFS_SGI_XFS_IOPS_ENV

static char c_xlate[80] =
    "+,0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/* int_to_base64
 * Create a base 64 string representation of a number.
 * The supplied string 's' must be at least 8 bytes long.
 * b64_string in stds.h provides a tyepdef to get the length.
 */
char *
int_to_base64(b64_string_t s, int a)
{
    int i, j;
    unsigned int n;

    i = 0;
    if (a == 0)
	s[i++] = c_xlate[0];
    else {
	j = 24;
	n = a & 0xc0000000;
	if (n) {
	    n >>= 30;
	    s[i++] = c_xlate[n];
	    a &= ~0xc0000000;
	} else {
	    for (; j >= 0; j -= 6) {
		n = a & (0x3f << j);
		if (n)
		    break;	/* found highest bits set. */
	    }
	    s[i++] = c_xlate[n >> j];
	    a &= ~(0x3f << j);
	    j -= 6;
	}
	/* more to do. */
	for (; j >= 0; j -= 6) {
	    n = a & (0x3f << j);
	    s[i++] = c_xlate[n >> j];
	    a &= ~(0x3f << j);
	}
    }
    s[i] = '\0';
    return s;
}

/* Mapping: +=0, ,=1, 0-9 = 2-11, A-Z = 12-37, a-z = 38-63 */
int
base64_to_int(char *s)
{
    int n = 0;
    int result = 0;
    unsigned char *p;

    for (; *s; s++) {
	if (*s < '0') {
	    n = *s - '+';
	} else if (*s <= '9') {
	    n = 2 + (int)(*s - '0');
	} else if (*s <= 'Z') {
	    n = 12 + (int)(*s - 'A');
	} else {
	    n = 38 + (int)(*s - 'a');
	}
	result = (result << 6) + n;
    }
    return result;
}


#endif /* AFS_SGI_XFS_IOPS_ENV */
