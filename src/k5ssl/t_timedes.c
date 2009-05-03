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
#include <stdlib.h>
#include <unistd.h>

#include "cputime.h"
#ifdef USING_MIT
#include <kerberosIV/des.h>
typedef struct _my_des_key {
    des_key_schedule s;
} my_des_key;
#define des_setkey(ks,k,l)  des_key_sched(k,ks->s)
#define des_encrypt(ks,p,c) des_ecb_encrypt((des_cblock *)p,(des_cblock *)c,ks->s,1)
#define des_decrypt(ks,c,p) des_ecb_encrypt((des_cblock *)c,(des_cblock *)p,ks->s,0)
#define DES_BLOCK_SIZE sizeof(des_cblock)
#else
#if USING_HCRYPTO
#include <hcrypto/des.h>
typedef DES_key_schedule my_des_key;
#define des_setkey(ks,k,l)  DES_set_key((DES_cblock *)k,ks)
#define des_encrypt(ks, p, c)	DES_ecb_encrypt((DES_cblock *)p, (DES_cblock *)c, ks, 1)
#define des_decrypt(ks, c, p)	DES_ecb_encrypt((DES_cblock *)c, (DES_cblock *)p, ks, 0)
#define DES_BLOCK_SIZE sizeof(DES_cblock)
#else
#ifdef USING_FAKESSL
#include "des.h"
typedef des_key my_des_key;
#else
#include "k5ssl.h"
typedef struct _my_des_key {
    des_key_schedule s;
} my_des_key;
#define des_setkey(ks,k,l)  des_key_sched((des_cblock *)k,ks->s)
#ifdef des_encrypt
#undef des_encrypt
#undef des_decrypt
#endif
#define des_encrypt(ks,p,c) des_ecb_encrypt((des_cblock *)p,(des_cblock *)c,ks->s,1)
#define des_decrypt(ks,c,p) des_ecb_encrypt((des_cblock *)c,(des_cblock *)p,ks->s,0)
#define DES_BLOCK_SIZE sizeof(des_cblock)
#endif
#endif
#endif

/* char **cipher_name(); */

struct test_struct
{
    int keylen;
    unsigned char key[8];
    int datalen;
    unsigned char pt[8];
    unsigned char ct[8];
} tests[] =
{
    { 8,   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    8,	{0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00},
	{0x8f,0xc3,0xa5,0x36, 0x56,0xb1,0xf7,0x78},
    },
};

void
time_encrypt(struct test_struct *p)
{
    unsigned char ct[16];
    my_des_key ks[1];
    CPU_TIME mstart, mend;
    long count;

    des_setkey(ks, p->key, p->keylen);

    set_alarm();
    mark_time(&mstart);
    for (count=0; !went_off; ++count) {
	des_encrypt(ks, p->pt, ct);
    }
    mark_time(&mend);
    alarm(0);
    printf (" %16s %s\n", "e,blocks", sfclock(&mstart, &mend, count, "us"));
    printf (" %16s %s\n", "e,bytes", sfclock(&mstart, &mend, count*16, "us"));
}

void
time_decrypt(struct test_struct *p)
{
    unsigned char pt[16];
    my_des_key ks[1];
    CPU_TIME mstart, mend;
    long count;

    des_setkey(ks, p->key, p->keylen);

    set_alarm();
    mark_time(&mstart);
    for (count=0; !went_off; ++count) {
	des_decrypt(ks, p->ct, pt);
    }
    mark_time(&mend);
    /*
    */
    alarm(0);
    printf (" %16s %s\n", "d,blocks", sfclock(&mstart, &mend, count, "us"));
    printf (" %16s %s\n", "d,bytes", sfclock(&mstart, &mend, count*16, "us"));
}

void
time_sched(struct test_struct *p)
{
    my_des_key ks[1];
    CPU_TIME mstart, mend;
    long count;

    set_alarm();
    mark_time(&mstart);
    for (count=0; !went_off; ++count) {
	des_setkey(ks, p->key, p->keylen);
    }
    mark_time(&mend);
    /*
    */
    alarm(0);
    printf (" %16s %s (count=%ld)\n", "setkey", sfclock(&mstart, &mend, count, "us"), count);
}

int
main(int argc,char **argv)
{
    struct test_struct *p;
    printf ("des\n");
    /* printf ("%s\n", *cipher_name()); */
    for (p = tests; p < tests+1; ++p) {
    printf (" %d byte key\n", p->keylen);
	time_encrypt(p);
	time_decrypt(p);
	time_sched(p);
    }
    exit(0);
}
