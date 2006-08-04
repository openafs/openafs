/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* NOTE: fc_cbc_encrypt now modifies its 5th argument, to permit chaining over
 * scatter/gather vectors.
 */


#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#define DEBUG 0
#ifdef KERNEL
#ifndef UKERNEL
#include "afs/stds.h"
#if defined(AFS_AIX_ENV) || defined(AFS_AUX_ENV) || defined(AFS_SUN5_ENV) 
#include "h/systm.h"
#endif
#include "h/types.h"
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_OBSD_ENV)
#include "netinet/in.h"
#endif
#else /* UKERNEL */
#include "afs/sysincludes.h"
#include "afs/stds.h"
#endif /* UKERNEL */
#ifdef AFS_LINUX22_ENV
#include <asm/byteorder.h>
#endif

#else /* KERNEL */

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/rx.h>
#endif /* KERNEL */

#include "sboxes.h"
#include "fcrypt.h"
#include "rxkad.h"
#include <des/stats.h>

#ifdef TCRYPT
int ROUNDS = 16;
#else
#define ROUNDS 16
#endif

#define XPRT_FCRYPT

int
fc_keysched(struct ktc_encryptionKey *key, fc_KeySchedule schedule)
{
    unsigned char *keychar = (unsigned char *)key;
    afs_uint32 kword[2];

    unsigned int temp;
    int i;

    /* first, flush the losing key parity bits. */
    kword[0] = (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[1] = kword[0] >> 4;	/* get top 24 bits for hi word */
    kword[0] &= 0xf;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar) >> 1;

    schedule[0] = kword[0];
    for (i = 1; i < ROUNDS; i++) {
	/* rotate right 3 */
	temp = kword[0] & ((1 << 11) - 1);	/* get 11 lsb */
	kword[0] =
	    (kword[0] >> 11) | ((kword[1] & ((1 << 11) - 1)) << (32 - 11));
	kword[1] = (kword[1] >> 11) | (temp << (56 - 32 - 11));
	schedule[i] = kword[0];
    }
    INC_RXKAD_STATS(fc_key_scheds);
    return 0;
}

/* IN int encrypt; * 0 ==> decrypt, else encrypt */
afs_int32
fc_ecb_encrypt(void * clear, void * cipher,
	       fc_KeySchedule schedule, int encrypt)
{
    afs_uint32 L, R;
    volatile afs_uint32 S, P;
    volatile unsigned char *Pchar = (unsigned char *)&P;
    volatile unsigned char *Schar = (unsigned char *)&S;
    int i;

#ifndef WORDS_BIGENDIAN
#define Byte0 3
#define Byte1 2
#define Byte2 1
#define Byte3 0
#else
#define Byte0 0
#define Byte1 1
#define Byte2 2
#define Byte3 3
#endif

#if 0
    memcpy(&L, clear, sizeof(afs_int32));
    memcpy(&R, clear + 1, sizeof(afs_int32));
#else
    L = ntohl(*((afs_uint32 *)clear));
    R = ntohl(*((afs_uint32 *)clear + 1));
#endif

    if (encrypt) {
	INC_RXKAD_STATS(fc_encrypts[ENCRYPT]);
	for (i = 0; i < (ROUNDS / 2); i++) {
	    S = *schedule++ ^ R;	/* xor R with key bits from schedule */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];	/* do 8-bit S Box subst. */
	    Pchar[Byte3] = sbox1[Schar[Byte1]];	/* and permute the result */
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1 << 5) - 1)) << (32 - 5));	/* right rot 5 bits */
	    L ^= P;		/* we're done with L, so save there */
	    S = *schedule++ ^ L;	/* this time xor with L */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];
	    Pchar[Byte3] = sbox1[Schar[Byte1]];
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1 << 5) - 1)) << (32 - 5));	/* right rot 5 bits */
	    R ^= P;
	}
    } else {
	INC_RXKAD_STATS(fc_encrypts[DECRYPT]);
	schedule = &schedule[ROUNDS - 1];	/* start at end of key schedule */
	for (i = 0; i < (ROUNDS / 2); i++) {
	    S = *schedule-- ^ L;	/* xor R with key bits from schedule */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];	/* do 8-bit S Box subst. and */
	    Pchar[Byte3] = sbox1[Schar[Byte1]];	/* permute the result */
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1 << 5) - 1)) << (32 - 5));	/* right rot 5 bits */
	    R ^= P;		/* we're done with L, so save there */
	    S = *schedule-- ^ R;	/* this time xor with L */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];
	    Pchar[Byte3] = sbox1[Schar[Byte1]];
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1 << 5) - 1)) << (32 - 5));	/* right rot 5 bits */
	    L ^= P;
	}
    }
#if 0
    memcpy(cipher, &L, sizeof(afs_int32));
    memcpy(cipher + 1, &R, sizeof(afs_int32));
#else
    *((afs_int32 *)cipher) = htonl(L);
    *((afs_int32 *)cipher + 1) = htonl(R);
#endif
    return 0;
}

/* Crypting can be done in segments by recycling xor.  All but the final segment must
 * be multiples of 8 bytes.
 * NOTE: fc_cbc_encrypt now modifies its 5th argument, to permit chaining over
 * scatter/gather vectors.
 */
/*
  afs_int32 length; * in bytes *
  int encrypt; * 0 ==> decrypt, else encrypt *
  fc_KeySchedule key; * precomputed key schedule *
  afs_uint32 *xor; * 8 bytes of initialization vector *
*/
afs_int32
fc_cbc_encrypt(void *input, void *output, afs_int32 length,
	       fc_KeySchedule key, afs_uint32 * xor, int encrypt)
{
    afs_uint32 i, j;
    afs_uint32 t_input[2];
    afs_uint32 t_output[2];
    unsigned char *t_in_p = (unsigned char *)t_input;

    if (encrypt) {
	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
	    memcpy(t_input, input, sizeof(t_input));
	    input=((char *)input) + sizeof(t_input);

	    /* zero pad */
	    for (j = length; j <= 7; j++)
		*(t_in_p + j) = 0;

	    /* do the xor for cbc into the temp */
	    xor[0] ^= t_input[0];
	    xor[1] ^= t_input[1];
	    /* encrypt */
	    fc_ecb_encrypt(xor, t_output, key, encrypt);

	    /* copy temp output and save it for cbc */
	    memcpy(output, t_output, sizeof(t_output));
	    output=(char *)output + sizeof(t_output);

	    /* calculate xor value for next round from plain & cipher text */
	    xor[0] = t_input[0] ^ t_output[0];
	    xor[1] = t_input[1] ^ t_output[1];


	}
	t_output[0] = 0;
	t_output[1] = 0;
    } else {
	/* decrypt */
	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
	    memcpy(t_input, input, sizeof(t_input));
	    input=((char *)input) + sizeof(t_input);

	    /* no padding for decrypt */
	    fc_ecb_encrypt(t_input, t_output, key, encrypt);

	    /* do the xor for cbc into the output */
	    t_output[0] ^= xor[0];
	    t_output[1] ^= xor[1];

	    /* copy temp output */
	    memcpy(output, t_output, sizeof(t_output));
	    output=((char *)output) + sizeof(t_output);

	    /* calculate xor value for next round from plain & cipher text */
	    xor[0] = t_input[0] ^ t_output[0];
	    xor[1] = t_input[1] ^ t_output[1];
	}
    }
    return 0;
}
