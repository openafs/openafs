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

#include "k5s_rc4.h"

void RC4_set_key_ex(RC4_KEY *ks, int len, const unsigned char *key, int rekey)
{
    unsigned char t;
    unsigned y;
    unsigned x;
    int z;

    if (!rekey) {
	for (x = 0; x <= 255; ++x)
	    ks->state[x] = x;
	ks->x = 0;     
	ks->y = 0;     
    }
    z=y=0;     

    x = 0;
    do {
	t=ks->state[x];
	y = (unsigned char)(key[z] + t + y);
	if (++z == len) z=0;
	ks->state[x]=ks->state[y];
	ks->state[y]=t;
    } while (++x < 256);
}
    
void
RC4(RC4_KEY *ks, unsigned int len, const unsigned char *ip, unsigned char *op)
{
    unsigned char x,y,t,u;
    
    x=ks->x;     
    y=ks->y;     

    while (len--) {
	++x;
	t=ks->state[x];
	y += t;
	u = ks->state[y];
	ks->state[x] = u;
	ks->state[y] = t;
	t += u;
	(*op++) = (*ip++) ^ ks->state[t];
    }               
    ks->x=x;     
    ks->y=y;
}
