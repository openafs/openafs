/*
 * Copyright 1985, 1986, 1987, 1988, 1989 by the Massachusetts Institute
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
 * The key schedule is passed as an arg, as well as the cleartext or
 * ciphertext.  The cleartext and ciphertext should be in host order.
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

#include "des.h"
#include "des_internal.h"
#include "des_prototypes.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/*
 * convert an arbitrary length string to a DES key
 */
void
des_string_to_key(char *str, des_cblock * key)
{
    char *in_str;
    unsigned temp, i, j;
    afs_int32 length;
    unsigned char *k_p;
    int forward;
    char *p_char;
    char k_char[64];
    des_key_schedule key_sked;

    in_str = str;
    forward = 1;
    p_char = k_char;
    length = strlen(str);

    /* init key array for bits */
    memset(k_char, 0, sizeof(k_char));

#ifdef DEBUG
    if (des_debug)
	fprintf(stdout,
		"\n\ninput str length = %d  string = %s\nstring = 0x ",
		length, str);
#endif

    /* get next 8 bytes, strip parity, xor */
    for (i = 1; i <= length; i++) {
	/* get next input key byte */
	temp = (unsigned int)*str++;
#ifdef DEBUG
	if (des_debug)
	    fprintf(stdout, "%02x ", temp & 0xff);
#endif
	/* loop through bits within byte, ignore parity */
	for (j = 0; j <= 6; j++) {
	    if (forward)
		*p_char++ ^= (int)temp & 01;
	    else
		*--p_char ^= (int)temp & 01;
	    temp = temp >> 1;
	} while (--j > 0);

	/* check and flip direction */
	if ((i % 8) == 0)
	    forward = !forward;
    }

    /* now stuff into the key des_cblock, and force odd parity */
    p_char = k_char;
    k_p = (unsigned char *)key;

    for (i = 0; i <= 7; i++) {
	temp = 0;
	for (j = 0; j <= 6; j++)
	    temp |= *p_char++ << (1 + j);
	*k_p++ = (unsigned char)temp;
    }

    /* fix key parity */
    des_fixup_key_parity(cblockptr_to_cblock(key));

    /* Now one-way encrypt it with the folded key */
    des_key_sched(cblockptr_to_cblock(key), key_sked);
    des_cbc_cksum(charptr_to_cblockptr(in_str), key, length, key_sked, key);
    /* erase key_sked */
    memset(key_sked, 0, sizeof(key_sked));

    /* now fix up key parity again */
    des_fixup_key_parity(cblockptr_to_cblock(key));

    if (des_debug)
	fprintf(stdout, "\nResulting string_to_key = 0x%x 0x%x\n",
		*((afs_uint32 *) key), *((afs_uint32 *) key + 1));
}
