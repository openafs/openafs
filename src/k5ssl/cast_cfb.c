/*
 * Copyright (c) 2006
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

#include "k5s_cast.h"

void
cast_cfb64_encrypt(const unsigned char *in, unsigned char *out,
    unsigned int length, const cast_key *key,
    unsigned char *ivec, int *num, const int enc)
{
    unsigned int n, c;

    n = *num;

    if (enc) {
	while(length--) {
	    if (!n) cast_encrypt(key, ivec, ivec);
	    ivec[n] = *(out++) = *(in++) ^ ivec[n];
	    if (++n >= CAST_BLOCK_SIZE) n = 0;
	}
    } else {
	while (length--) {
	    if (!n) cast_encrypt(key, ivec, ivec);
	    c = *in;
	    *out++ = *in++ ^ ivec[n];
	    ivec[n] = c;
	    if (++n >= CAST_BLOCK_SIZE) n = 0;
	}
    }
    *num = n;
}
