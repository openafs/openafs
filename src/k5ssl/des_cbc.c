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

#include "k5s_des.h"
#include "k5s_desint.h"

#ifdef KERNEL
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#endif

void des_cbc_encrypt(const unsigned char *in, unsigned char *out,
    unsigned int length, const des_key schedule[32],
    unsigned char *ivec, const int enc)
{
    unsigned char buf[DES_BLOCK_SIZE];
    const unsigned char *ip;
    unsigned char *op;
    int i;

    if (enc) {
	memcpy(buf, ivec, 8);
	ip = (const unsigned char*)in;
	op = (unsigned char *)out;
	while (length) {
	    if (length >= 8) {
		buf[0] ^= *ip++;
		buf[1] ^= *ip++;
		buf[2] ^= *ip++;
		buf[3] ^= *ip++;
		buf[4] ^= *ip++;
		buf[5] ^= *ip++;
		buf[6] ^= *ip++;
		buf[7] ^= *ip++;
		length -= 8;
	    } else {
		ip += length;
		switch(length) {
		case 7: buf[6] ^= *--ip;
		case 6: buf[5] ^= *--ip;
		case 5: buf[4] ^= *--ip;
		case 4: buf[3] ^= *--ip;
		case 3: buf[2] ^= *--ip;
		case 2: buf[1] ^= *--ip;
		case 1: buf[0] ^= *--ip;
		}
		length = 0;
	    }
	    des_encrypt(schedule, buf, buf);
	    memcpy(op, buf, 8);
	    op += 8;
	}
	memcpy(ivec, buf, 8);
    } else {
	unsigned char ocipher[DES_BLOCK_SIZE], cipher[DES_BLOCK_SIZE];
    
	if (!length) return;
	memcpy(ocipher, ivec, 8);
	ip = (const unsigned char*) in;
	op = (unsigned char *) out;
	for (;;) {
	    memcpy(buf, ip, 8);
	    ip += 8;
	    memcpy(cipher, buf, 8);
	    des_decrypt((const des_key*) schedule, buf, buf);
	    for (i = 0; i < 8; ++i)
		buf[i] ^= ocipher[i];
	    if (length > 8) {
		length -= 8;
		memcpy(op, buf, 8);
		op += 8;
		memcpy(ocipher, cipher, 8);
	    } else {
		op += (int) length;
		switch(length) {
		case 8: *--op = buf[7];
		case 7: *--op = buf[6];
		case 6: *--op = buf[5];
		case 5: *--op = buf[4];
		case 4: *--op = buf[3];
		case 3: *--op = buf[2];
		case 2: *--op = buf[1];
		case 1: *--op = buf[0];
		}
		break;
	    }
	}
	memcpy(ivec, cipher, 8);
    }
}
