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

#include <sys/types.h>
#include <netinet/in.h>
#if USE_FAKESSL
#include "k5s_evp.h"
#else
#include <openssl/evp.h>
#endif

#ifdef KERNEL
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#endif

#define BACKWARDS 1

#ifdef BACKWARDS
/* Archive::Zip::computeCRC32 */
#define CRC32_POLY 0xedb88320
#else
/* util-linux-2.11z/partx/crc32.c */
#define CRC32_POLY 0x04c11db7
#endif
static unsigned int crc32_table[256];
static void init_crc32(void);
unsigned int crc32_1(unsigned char const *buf, int len, unsigned int state);
static void init_crc32(void)
{
	int i, j;
	unsigned int c;
	for (i = 0; i < 256; ++i)
	{
#ifdef BACKWARDS
		for (c = i, j = 8; j; --j)
			c = (c & 1)
				? (c >> 1) ^ CRC32_POLY
				: (c >> 1);
#else
		for (c = i<<24, j = 8; j; --j)
			c = c & 0x80000000
				? (c << 1) ^ CRC32_POLY
				: (c << 1);
#endif
		crc32_table[i] = c;
	}
}

unsigned int crc32_1(unsigned char const *buf, int len, unsigned int state)
{
	unsigned char const *p;

	if (!crc32_table[1]) init_crc32();
	for (p = buf; len; ++p, --len)
#ifdef BACKWARDS
		state = (state>>8) ^ crc32_table[(unsigned char)(state ^ *p)];
#else
		state = (state<<8) ^ crc32_table[(state>>24) ^ *p];
#endif
	return ~state;
}

#define NID_crc32 NID_undef

#define CRC32_DIGEST_LENGTH 4
#define CRC32_BLOCK 1

const EVP_MD *EVP_crc32(void);
static int init(EVP_MD_CTX *ctx)
{
	*(unsigned int *) ctx->md_data = 0;
	return 1;
}
static int update(EVP_MD_CTX *ctx,const void *data,size_t count)
{
	*(unsigned int *) ctx->md_data = crc32_1(data,
		count,
		~*(unsigned int*) ctx->md_data);
	return 1;
}
static int final(EVP_MD_CTX *ctx,unsigned char *md)
{
	int x[1];
	*x = ntohl(*(unsigned int *)ctx->md_data);
	memcpy(md, x, 4);
	return 1;
}

static const EVP_MD crc32_md[1] = {{
	NID_crc32,
	NID_crc32,
	CRC32_DIGEST_LENGTH,
	0,
	init,
	update,
	final,
	(int) /* !! */ NULL,
	(int) /* !! */ NULL,
	EVP_PKEY_NULL_method,	/* ??? */
	CRC32_BLOCK,
	sizeof(EVP_MD*)+sizeof(unsigned),
}};

const EVP_MD *EVP_crc32(void)
{
	return crc32_md;
}
