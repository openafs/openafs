/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <netinet/in.h>

int
bin_dump(char *cp, int s)
{
    char *buffer;
    char c;
    int w;
    int i;
    long o;

    o = 0;
    buffer = cp;
    while (s > 0) {
	c = 16;
	if (c > s) c = s;
	printf ("%06lx:", o);
	w = 0;
	for (i = 0; i < c/2; ++i)
	    w += 5, printf (" %4x", htons(((unsigned short *)buffer)[i]));
	if (c & 1)
	    w += 3, printf (" %2x", (unsigned char)buffer[c-1]);
	while (w < 41)
	    ++w, putchar(' ');
	for (i = 0; i < c; ++i)
	    if (buffer[i] >= ' ' && buffer[i] < 0x7f)
/* isprint(buffer[i]) */
		putchar(buffer[i]);
	    else
		putchar('.');
	putchar('\n');
	o += c;
	buffer += c;
	s -= c;
    }
    printf ("%06lx:\n", o);
    return 1;
}
