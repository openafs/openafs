/*
 *	nfold.c
 *
 * Copyright (c) 2003,2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>

/* the n-fold algorithm is described in RFC 3961 */

static int
compute_lcm(int a, int b)
{
    int p = a*b;

    while (b) {
	int c = b;
	b = a % b;
	a = c;
    }
    return p/a;
}

char *
rxk5i_nfold(char *in, int inlen, char *out, int outlen)
{
	int lcm;
	int i;
	int w;
	int wi, sc;
	int accum;
	int msbit;
	int inbits;
	unsigned char *op;

	memset(out, 0, outlen);
	lcm = compute_lcm(inlen, outlen);
/* printf ("in=%d out=%d lcm=%d\n", inlen, outlen, lcm); */
	accum = 0;
	inbits = inlen<<3;
	op = (unsigned char*) (out + outlen);
	for (i = lcm; --i >= 0; ) {
		if (op == (unsigned char *) out)
			op = (unsigned char*) (out + outlen);
		--op;
		msbit = (
			 ((inbits)-1)
			+(((inbits)+13)*(i/inlen))
			 +((inlen-(i%inlen))<<3)
			 )%(inbits);
		wi = (((inlen)-((msbit>>3))) % inlen);
		sc = 1+(msbit&7);

		w = (unsigned char) in[wi];
		--wi; if (wi < 0) wi = inlen-1;
		w += in[wi]<<8;
		accum += (unsigned char) (w>>sc);
		accum += *op;
		*op = accum;
		accum >>= 8;
	}
	if (accum) {
		for (i = outlen; --i >= 0; ) {
			accum += (unsigned char) (out[i]);
			out[i] = accum;
			accum >>= 8;
			if (!accum) break;
		}
	}
	return out;
}
