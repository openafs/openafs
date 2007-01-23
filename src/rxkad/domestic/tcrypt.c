/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "fcrypt.h"


#ifdef TCRYPT
extern int ROUNDS;
#else
#define ROUNDS 16
#endif

struct ktc_encryptionKey {
    char data[8];
};

void
print_msg(text, msg, length)
     char *text;
     afs_int32 msg[];
     int length;
{
    int i;
    unsigned char *msgtext = (unsigned char *)msg;
    int cksum;

    if (length % 8 != 0) {
	printf("Length of message (%d) incorrect\n", length);
	return;
    }
    printf("%s\n", text);
    for (i = 0; i < length; i += 8) {
	unsigned char *m = msgtext + i;
	printf("%02x%02x%02x%02x %02x%02x%02x%02x", m[0], m[1], m[2], m[3],
	       m[4], m[5], m[6], m[7]);
	printf("  ");
    }
    cksum = 0;
    for (i = 0; i < length; i++)
	cksum += *(i + msgtext);
    printf(" (%4x)\n", cksum & 0xffff);
}

static int total_diff, minimum_diff, number_diff;

compare(orig, chgd)
     afs_uint32 orig[2];
     afs_uint32 chgd[2];
{
    afs_uint32 temp;
    int diff = 0;

    for (temp = orig[0] ^ chgd[0]; temp; temp >>= 1)
	diff += temp & 1;
    for (temp = orig[1] ^ chgd[1]; temp; temp >>= 1)
	diff += temp & 1;
    printf("%.8x %.8x  (%d)\n", chgd[0], chgd[1], diff);
    number_diff++;
    total_diff += diff;
    minimum_diff = (diff < minimum_diff) ? diff : minimum_diff;
}

#include "AFS_component_version_number.c"

int
main(argc, argv)
     int argc;
     char *argv[];
{
    struct ktc_encryptionKey key;
    fc_KeySchedule schedule[1];
    afs_int32 e[2];
    afs_int32 d[2];
    afs_int32 c[2];
    int i;
    int iterations;
    int for_time = (argc > 1) && (strcmp(argv[1], "time") == 0);
    int for_time_key = (argc > 1) && (strcmp(argv[1], "time_key") == 0);
    int for_cbc = (argc > 1) && (strcmp(argv[1], "cbc") == 0);

    if (for_time || for_time_key) {
	if (argc == 3)
	    iterations = atoi(argv[2]);
	else
	    iterations = 10000;
    }

    if (for_time)
	for (i = 0; i < iterations; i++)
	    fc_ecb_encrypt(e, d, schedule, 1);
    else if (for_time_key)
	for (i = 0; i < iterations; i++)
	    fc_keysched(&key, schedule);
    else if (for_cbc) {
	afs_int32 msg[10];
	afs_int32 out[10];
	afs_int32 dec[10];
	afs_uint32 xor[2];

	for (i = 0; i < 10; i++)
	    msg[i] = htonl(i);
	memcpy(&key, "abcdefgh", sizeof(struct ktc_encryptionKey));
	fc_keysched(&key, schedule);
	print_msg("Starting msg is:", msg, sizeof(msg));
	memcpy(xor, &key, 2 * sizeof(afs_int32));
	fc_cbc_encrypt(msg, out, sizeof(msg), schedule, &key, ENCRYPT);
	memcpy(xor, &key, 2 * sizeof(afs_int32));
	fc_cbc_encrypt(out, dec, sizeof(msg), schedule, &key, DECRYPT);
	if (memcmp(msg, dec, sizeof(msg)) != 0)
	    printf("Encryption FAILED!\n");
	print_msg("Encrypted is:", out, sizeof(out));
	print_msg("Decrypted is:", dec, sizeof(dec));
    } else {
	int rounds;
#ifndef TCRYPT
	printf
	    ("Number of rounds is fixed at %d; try recompiling w/ -DTCRYPT=1\n",
	     ROUNDS);
#endif
	e[0] = 0x11111111;
	e[1] = 0xaaaaaaaa;
	memcpy(&key, "abcdefgh", sizeof(struct ktc_encryptionKey));
	for (rounds = 2; rounds <= MAXROUNDS; rounds += 2) {
#ifdef TCRYPT
	    ROUNDS = rounds;
#endif
	    printf("\n  ROUNDS = %d\n", ROUNDS);
	    total_diff = 0;
	    minimum_diff = 64;
	    number_diff = 0;

	    fc_keysched(&key, schedule);
	    fc_ecb_encrypt(e, d, schedule, 1);

	    printf("Checking data bits\n");
	    for (i = 1; i; i <<= 1) {
		e[0] ^= i;
		fc_ecb_encrypt(e, c, schedule, 1);
		compare(d, c);
		e[0] ^= i;
		e[1] ^= i;
		fc_ecb_encrypt(e, c, schedule, 1);
		compare(d, c);
		e[1] ^= i;
	    }
	    printf("Checking key bits\n");
	    for (i = 0; i < 56; i++) {
		unsigned char *keyByte;
		keyByte = ((unsigned char *)(&key)) + i / 7;
		*keyByte ^= (2 << (i % 7));
		fc_keysched(&key, schedule);
		fc_ecb_encrypt(e, c, schedule, 1);
		compare(d, c);
		*keyByte ^= (2 << (i % 7));
	    }
	    print_msg("clear: ", e, sizeof(e));

	    print_msg("Encrypted: ", d, sizeof(d));

	    fc_keysched(&key, schedule);
	    fc_ecb_encrypt(d, e, schedule, 0);
	    print_msg("De-encrypted: ", e, sizeof(e));
	    printf("Rounds=%d, average diff = %d bits, minimum diff = %d\n",
		   ROUNDS, total_diff / number_diff, minimum_diff);
	}
    }
}
