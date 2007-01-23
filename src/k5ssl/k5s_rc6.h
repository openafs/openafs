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

#define RC6_INT32	unsigned int
typedef struct rc6_ks_struct {
	RC6_INT32 S[44];
} RC6_KEY;
#define RC6_BLOCK_SIZE	16
#define RC6_MIN_KEY_LENGTH 1
#define RC6_MAX_KEY_LENGTH 32

int RC6_set_key(RC6_KEY *ks, unsigned b, const unsigned char *K);
void RC6_encrypt(const RC6_KEY *ks,
	const unsigned char *pt, unsigned char *ct);
void RC6_decrypt(const RC6_KEY *ks,
	const unsigned char *ct, unsigned char *pt);

void RC6_ecb_encrypt(const unsigned char *in, unsigned char *out,
	const RC6_KEY *key, int enc);
void RC6_cbc_encrypt(const unsigned char *in, unsigned char *out,
	unsigned int length, const RC6_KEY *key,
	unsigned char *ivec, const int enc);
void RC6_cfb128_encrypt(const unsigned char *in, unsigned char *out,
	unsigned int length, const RC6_KEY *key,
	unsigned char *ivec, int *num, const int enc);
void RC6_ofb128_encrypt(const unsigned char *in, unsigned char *out,
	unsigned int length, const RC6_KEY *key,
	unsigned char *ivec, int *num);
void RC6_ctr128_encrypt(const unsigned char *in, unsigned char *out,
	unsigned int length, const RC6_KEY *key,
	unsigned char ivec[RC6_BLOCK_SIZE],
	unsigned char ecount_buf[RC6_BLOCK_SIZE],
	unsigned int *num);
