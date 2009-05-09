/*
 * Copyright (c) 1995 - 2000, 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <afs/param.h>
#include "rxkad.h"
#include <rx/rx.h>
#include "private_data.h"

#define ROUNDS 16
#define ENCRYPT 1
#define DECRYPT 0

typedef afs_int32 int32;
typedef afs_uint32 u_int32;

#include <stdio.h>
#include <string.h>

#include <time.h>

const char the_quick[] = "The quick brown fox jumps over the lazy dogs.\0\0";

const struct ktc_encryptionKey key1[1] =
    { 0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87 };
const unsigned char ciph1[] = {
    0x00, 0xf0, 0xe, 0x11, 0x75, 0xe6, 0x23, 0x82, 0xee, 0xac, 0x98, 0x62,
    0x44, 0x51, 0xe4, 0x84, 0xc3, 0x59, 0xd8, 0xaa, 0x64, 0x60, 0xae, 0xf7,
    0xd2, 0xd9, 0x13, 0x79, 0x72, 0xa3, 0x45, 0x03, 0x23, 0xb5, 0x62, 0xd7,
    0xc, 0xf5, 0x27, 0xd1, 0xf8, 0x91, 0x3c, 0xac, 0x44, 0x22, 0x92, 0xef
};

const struct ktc_encryptionKey key2[1] =
    { 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 };
const unsigned char ciph2[] = {
    0xca, 0x90, 0xf5, 0x9d, 0xcb, 0xd4, 0xd2, 0x3c, 0x01, 0x88, 0x7f, 0x3e,
    0x31, 0x6e, 0x62, 0x9d, 0xd8, 0xe0, 0x57, 0xa3, 0x06, 0x3a, 0x42, 0x58,
    0x2a, 0x28, 0xfe, 0x72, 0x52, 0x2f, 0xdd, 0xe0, 0x19, 0x89, 0x09, 0x1c,
    0x2a, 0x8e, 0x8c, 0x94, 0xfc, 0xc7, 0x68, 0xe4, 0x88, 0xaa, 0xde, 0x0f
};

#ifdef TEST_KERNEL
#define fc_keysched    _afs_QTKrFdpoFL
#define fc_ecb_encrypt _afs_sDLThwNLok
#define fc_cbc_encrypt _afs_fkyCWTvfRS
#define rxkad_DecryptPacket _afs_SRWEeqTXrS
#define rxkad_EncryptPacket _afs_bpwQbdoghO
#endif

int
main(void)
{
    fc_KeySchedule sched[1];
    char ciph[100], clear[100];
    u_int32 data[2];
    fc_InitializationVector iv[1];
    struct rx_connection conn;
    struct rx_securityClass obj;
    struct rxkad_cprivate cpriv;
    struct rx_packet packet;
    int fail = 0;

    conn.securityObject = &obj;
    obj.privateData = (void *)&cpriv;
    cpriv.type = 0;

    if (sizeof(int32) != 4) {
	fprintf(stderr, "error: sizeof(int32) != 4\n");
	fail++;
    }
    if (sizeof(u_int32) != 4) {
	fprintf(stderr, "error: sizeof(u_int32) != 4\n");
	fail++;
    }

    /*
     * Use key1 and key2 as iv */
    fc_keysched(key1, sched);
    memcpy(iv, key2, sizeof(iv));
    fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
    if (memcmp(ciph1, ciph, sizeof(ciph1)) != 0) {
	fprintf(stderr, "encrypt FAILED\n");
	fail++;
    }
    memcpy(iv, key2, sizeof(iv));
    fc_cbc_encrypt(ciph, clear, sizeof(the_quick), sched, iv, DECRYPT);
    if (strcmp(the_quick, clear) != 0) {
	fprintf(stderr, "crypt decrypt FAILED\n");
	fail++;
    }

    /*
     * Use key2 and key1 as iv
     */
    fc_keysched(key2, sched);
    memcpy(iv, key1, sizeof(iv));
    fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
    if (memcmp(ciph2, ciph, sizeof(ciph2)) != 0) {
	fprintf(stderr, "encrypt FAILED\n");
	fail++;
    }
    memcpy(iv, key1, sizeof(iv));
    fc_cbc_encrypt(ciph, clear, sizeof(the_quick), sched, iv, DECRYPT);
    if (strcmp(the_quick, clear) != 0) {
	fprintf(stderr, "crypt decrypt FAILED\n");
	fail++;
    }

    /*
     * Test Encrypt- and Decrypt-Packet, use key1 and key2 as iv
     */
    fc_keysched(key1, sched);
    memcpy(iv, key2, sizeof(iv));
    strcpy(clear, the_quick);
    packet.wirevec[1].iov_base = clear;
    packet.wirevec[1].iov_len = sizeof(the_quick);
    packet.wirevec[2].iov_len = 0;

    /* For unknown reasons bytes 4-7 are zeroed in rxkad_EncryptPacket */
    rxkad_EncryptPacket(&conn, sched, iv, sizeof(the_quick), &packet);
    rxkad_DecryptPacket(&conn, sched, iv, sizeof(the_quick), &packet);
    clear[4] ^= 'q';
    clear[5] ^= 'u';
    clear[6] ^= 'i';
    clear[7] ^= 'c';
    if (strcmp(the_quick, clear) != 0)
	fprintf(stderr, "rxkad_EncryptPacket/rxkad_DecryptPacket FAILED\n");

    {
	struct timeval start, stop;
	int i;

#define INT_MILLION 1000000
#define FLOAT_MILLION 1e6
#define COUNT 1
	fc_keysched(key1, sched);
	gettimeofday(&start, 0);
	for (i = 0; i < INT_MILLION*COUNT; i++)
	    fc_keysched(key1, sched);
	gettimeofday(&stop, 0);
	printf("fc_keysched    = %2.2f us\n",
	       (stop.tv_sec - start.tv_sec +
		(stop.tv_usec - start.tv_usec) / FLOAT_MILLION) / COUNT);

	fc_ecb_encrypt(data, data, sched, ENCRYPT);
	gettimeofday(&start, 0);
	for (i = 0; i < INT_MILLION*COUNT; i++)
	    fc_ecb_encrypt(data, data, sched, ENCRYPT);
	gettimeofday(&stop, 0);
	printf("fc_ecb_encrypt = %2.2f us\n",
	       (stop.tv_sec - start.tv_sec +
		(stop.tv_usec - start.tv_usec) / FLOAT_MILLION) / COUNT);

	fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv,
		       ENCRYPT);
	gettimeofday(&start, 0);
	for (i = 0; i < (INT_MILLION*COUNT)/10; i++)
	    fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv,
			   ENCRYPT);
	gettimeofday(&stop, 0);
	printf("fc_cbc_encrypt = %2.2f us\n",
	       ((stop.tv_sec - start.tv_sec +
		(stop.tv_usec - start.tv_usec) / FLOAT_MILLION) * 10)/COUNT);

    }

    exit(fail);
}
