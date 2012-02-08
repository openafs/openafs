/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * These routines perform encryption and decryption using the DES
 * private key algorithm, or else a subset of it -- fewer inner loops.
 * (AUTH_DES_ITER defaults to 16, may be less.)
 *
 * Under U.S. law, this software may not be exported outside the US
 * without license from the U.S. Commerce department.
 *
 * These routines form the library interface to the DES facilities.
 *
 * Originally written 8/85 by Steve Miller, MIT Project Athena.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include "mit-cpyright.h"

#ifndef KERNEL
#include <stdio.h>
#endif

#include "des.h"
#include "des_prototypes.h"



#define XPRT_CBC_ENCRYPT

/*
 * This routine performs DES cipher-block-chaining operation, either
 * encrypting from cleartext to ciphertext, if encrypt != 0 or
 * decrypting from ciphertext to cleartext, if encrypt == 0.
 *
 * The key schedule is passed as an arg, as well as the cleartext or
 * ciphertext.	The cleartext and ciphertext should be in host order.
 *
 * NOTE-- the output is ALWAYS an multiple of 8 bytes long.  If not
 * enough space was provided, your program will get trashed.
 *
 * For encryption, the cleartext string is null padded, at the end, to
 * an integral multiple of eight bytes.
 *
 * For decryption, the ciphertext will be used in integral multiples
 * of 8 bytes, but only the first "length" bytes returned into the
 * cleartext.
 */
/*
    des_cblock *in;		* >= length bytes of input text *
    des_cblock *out;		* >= length bytes of output text *
    afs_int32 length;	* in bytes *
    int encrypt;		* 0 ==> decrypt, else encrypt *
    des_key_schedule key;		* precomputed key schedule *
    des_cblock *iv;		* 8 bytes of ivec *
*/
afs_int32
des_cbc_encrypt(void * in, void * out, afs_int32 length,
		des_key_schedule key, des_cblock * iv, int encrypt)
{
    afs_uint32 *input = (afs_uint32 *) in;
    afs_uint32 *output = (afs_uint32 *) out;
    afs_uint32 *ivec = (afs_uint32 *) iv;

    afs_uint32 i, j;
    afs_uint32 t_input[2];
    afs_uint32 t_output[2];
    unsigned char *t_in_p = (unsigned char *)t_input;
    afs_uint32 xor_0, xor_1;

    if (encrypt) {
#ifdef MUSTALIGN
	if ((afs_int32) ivec & 3) {
	    memcpy((char *)&t_output[0], (char *)ivec++, sizeof(t_output[0]));
	    memcpy((char *)&t_output[1], (char *)ivec, sizeof(t_output[1]));
	} else
#endif
	{
	    t_output[0] = *ivec++;
	    t_output[1] = *ivec;
	}

	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
#ifdef MUSTALIGN
	    if ((afs_int32) input & 3) {
		memcpy((char *)&t_input[0], (char *)input++,
		       sizeof(t_input[0]));
		memcpy((char *)&t_input[1], (char *)input++,
		       sizeof(t_input[1]));
	    } else
#endif
	    {
		t_input[0] = *input++;
		t_input[1] = *input++;
	    }

	    /* zero pad */
	    if (length < 8)
		for (j = length; j <= 7; j++)
		    *(t_in_p + j) = 0;

#ifdef DEBUG
	    if (des_debug)
		des_debug_print("clear", length, t_input[0], t_input[1]);
#endif
	    /* do the xor for cbc into the temp */
	    t_input[0] ^= t_output[0];
	    t_input[1] ^= t_output[1];
	    /* encrypt */
	    (void)des_ecb_encrypt(t_input, t_output, key, encrypt);
	    /* copy temp output and save it for cbc */
#ifdef MUSTALIGN
	    if ((afs_int32) output & 3) {
		memcpy((char *)output++, (char *)&t_output[0],
		       sizeof(t_output[0]));
		memcpy((char *)output++, (char *)&t_output[1],
		       sizeof(t_output[1]));
	    } else
#endif
	    {
		*output++ = t_output[0];
		*output++ = t_output[1];
	    }

#ifdef DEBUG
	    if (des_debug) {
		des_debug_print("xor'ed", i, t_input[0], t_input[1]);
		des_debug_print("cipher", i, t_output[0], t_output[1]);
	    }
#endif
	}
	return 0;
    }

    else {
	/* decrypt */
#ifdef MUSTALIGN
	if ((afs_int32) ivec & 3) {
	    memcpy((char *)&xor_0, (char *)ivec++, sizeof(xor_0));
	    memcpy((char *)&xor_1, (char *)ivec, sizeof(xor_1));
	} else
#endif
	{
	    xor_0 = *ivec++;
	    xor_1 = *ivec;
	}

	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
#ifdef MUSTALIGN
	    if ((afs_int32) input & 3) {
		memcpy((char *)&t_input[0], (char *)input++,
		       sizeof(t_input[0]));
		memcpy((char *)&t_input[1], (char *)input++,
		       sizeof(t_input[0]));
	    } else
#endif
	    {
		t_input[0] = *input++;
		t_input[1] = *input++;
	    }

	    /* no padding for decrypt */
#ifdef DEBUG
	    if (des_debug)
		des_debug_print("cipher", i, t_input[0], t_input[1]);
#else
#ifdef lint
	    i = i;
#endif
#endif
	    /* encrypt */
	    (void)des_ecb_encrypt(t_input, t_output, key, encrypt);
#ifdef DEBUG
	    if (des_debug)
		des_debug_print("out pre xor", i, t_output[0], t_output[1]);
#endif
	    /* do the xor for cbc into the output */
	    t_output[0] ^= xor_0;
	    t_output[1] ^= xor_1;
	    /* copy temp output */
#ifdef MUSTALIGN
	    if ((afs_int32) output & 3) {
		memcpy((char *)output++, (char *)&t_output[0],
		       sizeof(t_output[0]));
		memcpy((char *)output++, (char *)&t_output[1],
		       sizeof(t_output[1]));
	    } else
#endif
	    {
		*output++ = t_output[0];
		*output++ = t_output[1];
	    }

	    /* save xor value for next round */
	    xor_0 = t_input[0];
	    xor_1 = t_input[1];
#ifdef DEBUG
	    if (des_debug)
		des_debug_print("clear", i, t_output[0], t_output[1]);
#endif
	}
	return 0;
    }
}
