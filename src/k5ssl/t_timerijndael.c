/*
 * Copyright (c) 2005
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
 * rc4 timing
 */

#include <stdio.h>

#include "cputime.h"
#if USING_HCRYPTO
#include <hcrypto/aes.h>
typedef struct _my_aes_key {
	AES_KEY enckey[1];
	AES_KEY deckey[1];
} my_aes_key;
#define rijndael_setkey(ks,k,l) (AES_set_encrypt_key(k,l<<3,ks->enckey),AES_set_decrypt_key(k,l<<3,ks->deckey))
#define rijndael_encrypt(ks, p, c)	AES_encrypt(p, c, ks->enckey)
#define rijndael_decrypt(ks, c, p)	AES_decrypt(c, p, ks->deckey)
#else
#include "k5s_rijndael.h"
typedef struct rijndael_ks my_aes_key;
#endif

/* char **cipher_name(); */

struct test_struct
{
	int keylen;
	unsigned char key[32];
	int datalen;
	unsigned char pt[16];
	unsigned char ct[16];
} tests[] =
{
	{ 16,   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	16,	{0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00},
		{0x8f,0xc3,0xa5,0x36, 0x56,0xb1,0xf7,0x78,
		 0xc1,0x29,0xdf,0x4e, 0x98,0x48,0xa4,0x1e},
	},
	{ 24,   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	16,	{0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00},
		{0x8f,0xc3,0xa5,0x36, 0x56,0xb1,0xf7,0x78,
		 0xc1,0x29,0xdf,0x4e, 0x98,0x48,0xa4,0x1e},
	},
	{ 32,   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	16,	{0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00},
		{0x8f,0xc3,0xa5,0x36, 0x56,0xb1,0xf7,0x78,
		 0xc1,0x29,0xdf,0x4e, 0x98,0x48,0xa4,0x1e},
	},
};

int
main()
{
	struct test_struct *p;
	printf ("rijndael\n");
	/* printf ("%s\n", *cipher_name()); */
	for (p = tests; p < tests+3; ++p)
	{
	printf (" %d byte key\n", p->keylen);
		time_encrypt(p);
		time_decrypt(p);
		time_sched(p);
	}
}

time_encrypt(p)
	struct test_struct *p;
{
	unsigned char ct[16], pt[16];
	my_aes_key ks[1];
	CPU_TIME mstart, mend;
	long count;

	rijndael_setkey(ks, p->key, p->keylen);

	set_alarm();
	mark_time(&mstart);
	for (count=0; !went_off; ++count)
	{
		rijndael_encrypt(ks, p->pt, ct);
	}
	mark_time(&mend);
	alarm(0);
	printf (" %16s %s\n", "e,blocks", sfclock(&mstart, &mend, count, "us"));
	printf (" %16s %s\n", "e,bytes", sfclock(&mstart, &mend, count*16, "us"));
}

time_decrypt(p)
	struct test_struct *p;
{
	unsigned char ct[16], pt[16];
	my_aes_key ks[1];
	CPU_TIME mstart, mend;
	long count;

	rijndael_setkey(ks, p->key, p->keylen);

	set_alarm();
	mark_time(&mstart);
	for (count=0; !went_off; ++count)
	{
		rijndael_decrypt(ks, p->ct, pt);
	}
	mark_time(&mend);
	/*
	*/
	alarm(0);
	printf (" %16s %s\n", "d,blocks", sfclock(&mstart, &mend, count, "us"));
	printf (" %16s %s\n", "d,bytes", sfclock(&mstart, &mend, count*16, "us"));
}

time_sched(p)
	struct test_struct *p;
{
	unsigned char ct[16], pt[16];
	my_aes_key ks[1];
	CPU_TIME mstart, mend;
	long count;

	set_alarm();
	mark_time(&mstart);
	for (count=0; !went_off; ++count)
	{
		rijndael_setkey(ks, p->key, p->keylen);
	}
	mark_time(&mend);
	/*
	*/
	alarm(0);
	printf (" %16s %s (count=%d)\n", "setkey", sfclock(&mstart, &mend, count, "us"), count);
}
