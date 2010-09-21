/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * These routines perform encryption and decryption using the DES
 * private key algorithm, or else a subset of it-- fewer inner loops.
 * (AUTH_DES_ITER defaults to 16, may be less.)
 *
 * Under U.S. law, this software may not be exported outside the US
 * without license from the U.S. Commerce department.
 *
 * These routines form the library interface to the DES facilities.
 *
 *	spm	8/85	MIT project athena
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "mit-cpyright.h"
#ifndef KERNEL
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "des.h"
#include "des_internal.h"
#include "des_prototypes.h"

#define XPRT_CKSUM

/*
 * This routine performs DES cipher-block-chaining checksum operation,
 * a.k.a.  Message Authentication Code.  It ALWAYS encrypts from input
 * to a single 64 bit output MAC checksum.
 *
 * The key schedule is passed as an arg, as well as the cleartext or
 * ciphertext. The cleartext and ciphertext should be in host order.
 *
 * NOTE-- the output is ALWAYS 8 bytes long.  If not enough space was
 * provided, your program will get trashed.
 *
 * The input is null padded, at the end (highest addr), to an integral
 * multiple of eight bytes.
 */
/*
    des_cblock *in;		* >= length bytes of inputtext *
    des_cblock *out;		* >= length bytes of outputtext *
    afs_int32 length;	* in bytes *
    des_key_schedule key;       * precomputed key schedule *
    des_cblock *iv;		* 8 bytes of ivec *
*/

afs_uint32
des_cbc_cksum(des_cblock * in, des_cblock * out, afs_int32 length,
	      des_key_schedule key, des_cblock * iv)
{
    afs_uint32 *input = (afs_uint32 *) in;
    afs_uint32 *output = (afs_uint32 *) out;
    afs_uint32 *ivec = (afs_uint32 *) iv;

    afs_uint32 i, j;
    afs_uint32 t_input[2];
    afs_uint32 t_output[8];
    unsigned char *t_in_p = (unsigned char *)t_input;

#ifdef MUSTALIGN
    if (afs_pointer_to_int(ivec) & 3) {
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
      if (afs_pointer_to_int(input) & 3) {
	    memcpy((char *)&t_input[0], (char *)input++, sizeof(t_input[0]));
	    memcpy((char *)&t_input[1], (char *)input++, sizeof(t_input[1]));
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
	(void)des_ecb_encrypt(t_input, t_output, key, 1);
#ifdef DEBUG
	if (des_debug) {
	    des_debug_print("xor'ed", i, t_input[0], t_input[1]);
	    des_debug_print("cipher", i, t_output[0], t_output[1]);
	}
#else
#ifdef lint
	i = i;
#endif
#endif
    }
    /* copy temp output and save it for checksum */
#ifdef MUSTALIGN
    if (afs_pointer_to_int(output) & 3) {
	memcpy((char *)output++, (char *)&t_output[0], sizeof(t_output[0]));
	memcpy((char *)output, (char *)&t_output[1], sizeof(t_output[1]));
    } else
#endif
    {
	*output++ = t_output[0];
	*output = t_output[1];
    }

    return (afs_uint32) t_output[1];
}
