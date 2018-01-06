/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

const char hckernel_strcasecmp_placeholder[] =
		"This is not an empty compilation unit.";

#ifndef afs_strcasecmp
int
afs_strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
	char c1, c2;

	c1 = *s1++;
	c2 = *s2++;
	if (c1 >= 'A' && c1 <= 'Z')
	    c1 += 0x20;
	if (c2 >= 'A' && c2 <= 'Z')
	    c2 += 0x20;
	if (c1 != c2)
	    return c1 - c2;
    }

    return *s1 - *s2;
}
#endif
