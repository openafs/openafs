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

/*
 * k5s_rijndael.h
 */

struct rijndael_ks {
	unsigned char sched[14+1][8][4];
	int kl;
};
int rijndael_setkey_encrypt(struct rijndael_ks *, const unsigned char *, const int);
int rijndael_setkey_decrypt(struct rijndael_ks *, const unsigned char *, const int);
int rijndael_encrypt(const struct rijndael_ks *, const unsigned char[16], unsigned char[16]);
int rijndael_decrypt(const struct rijndael_ks *, const unsigned char[16], unsigned char[16]);

#define RIJNDAEL_BLOCK_SIZE 16
