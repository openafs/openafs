/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *
 * COMPONENT_NAME: nsafs
 *
 */

/*
 * This module implements the Secure Hash Algorithm (SHA) as specified in
 * the Secure Hash Standard (SHS, FIPS PUB 180.1).
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include <net/if.h>
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "afs/auth.h"
#include "afs/cellconfig.h"
#include "afs/vice.h"
#include "afs/nsafs.h"

static int big_endian;

static const sha_int hashinit[] = {
    0x67452301, 0xEFCDAB89, 0x98BADCFE,
    0x10325476, 0xC3D2E1F0
};

#define ROTL(n, x) (((x) << (n)) | ((x) >> (SHA_BITS_PER_INT - (n))))

#ifdef DISABLED_CODE_HERE
static sha_int
f(int t, sha_int x, sha_int y, sha_int z)
{
    if (t < 0 || t >= SHA_ROUNDS)
	return 0;
    if (t < 20)
	return (z ^ (x & (y ^ z)));
    if (t < 40)
	return (x ^ y ^ z);
    if (t < 60)
	return ((x & y) | (z & (x | y)));	/* saves 1 boolean op */
    return (x ^ y ^ z);		/* 60-79 same as 40-59 */
}
#endif

/* This is the "magic" function used for each round.         */
/* Were this a C function, the interface would be:           */
/* static sha_int f(int t, sha_int x, sha_int y, sha_int z) */
/* The function call version preserved above until stable    */

#define f_a(x, y, z) (z ^ (x & (y ^ z)))
#define f_b(x, y, z) (x ^ y ^ z)
#define f_c(x, y, z) (( (x & y) | (z & (x | y))))

#define f(t, x, y, z)                     \
      ( (t < 0 || t >= SHA_ROUNDS) ? 0 :  \
          ( (t < 20) ? f_a(x, y, z) :     \
	      ( (t < 40) ? f_b(x, y, z) : \
	          ( (t < 60) ? f_c(x, y, z) : f_b(x, y, z)))))

/*
 *static sha_int K(int t)
 *{
 *    if (t < 0 || t >= SHA_ROUNDS) return 0;
 *   if (t < 20)
 *	return 0x5A827999;
 *   if (t < 40)
 *	return 0x6ED9EBA1;
 *   if (t < 60)
 *	return 0x8F1BBCDC;
 *   return 0xCA62C1D6;
 * }
 */

/* This macro/function supplies the "magic" constant for each round. */
/* The function call version preserved above until stable            */

static const sha_int k_vals[] =
    { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };

#define K(t) ( (t < 0 || t >= SHA_ROUNDS) ? 0 : k_vals[ t/20 ] )

/*
 * Update the internal state based on the given chunk.
 */
static void
transform(shaState * shaStateP, sha_int * chunk)
{
    sha_int A = shaStateP->digest[0];
    sha_int B = shaStateP->digest[1];
    sha_int C = shaStateP->digest[2];
    sha_int D = shaStateP->digest[3];
    sha_int E = shaStateP->digest[4];
    sha_int TEMP = 0;

    int t;
    sha_int W[SHA_ROUNDS];

    for (t = 0; t < SHA_CHUNK_INTS; t++)
	W[t] = chunk[t];
    for (; t < SHA_ROUNDS; t++) {
	TEMP = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
	W[t] = ROTL(1, TEMP);
    }

    for (t = 0; t < SHA_ROUNDS; t++) {
	TEMP = ROTL(5, A) + f(t, B, C, D) + E + W[t] + K(t);
	E = D;
	D = C;
	C = ROTL(30, B);
	B = A;
	A = TEMP;
    }

    shaStateP->digest[0] += A;
    shaStateP->digest[1] += B;
    shaStateP->digest[2] += C;
    shaStateP->digest[3] += D;
    shaStateP->digest[4] += E;
}


/*
 * This function takes an array of SHA_CHUNK_BYTES bytes
 * as input and produces an output array of ints that is
 * SHA_CHUNK_INTS long.
 */
static void
buildInts(const char *data, sha_int * chunk)
{
    /*
     * Need to copy the data because we can't be certain that
     * the input buffer will be aligned correctly.
     */
    memcpy((void *)chunk, (void *)data, SHA_CHUNK_BYTES);

    if (!big_endian) {
	/* This loop does nothing but waste time on a big endian machine. */
	int i;

	for (i = 0; i < SHA_CHUNK_INTS; i++)
	    chunk[i] = ntohl(chunk[i]);
    }
}

/*
 * This function updates the internal state of the hash by using
 * buildInts to break the input up into chunks and repeatedly passing
 * these chunks to transform().
 */
void
sha_update(shaState * shaStateP, const char *buffer, int bufferLen)
{
    int i;
    sha_int chunk[SHA_CHUNK_INTS];
    sha_int newLo;

    if (buffer == NULL || bufferLen == 0)
	return;

    newLo = shaStateP->bitcountLo + (bufferLen << 3);
    if (newLo < shaStateP->bitcountLo)
	shaStateP->bitcountHi++;
    shaStateP->bitcountLo = newLo;
    shaStateP->bitcountHi += ((bufferLen >> (SHA_BITS_PER_INT - 3)) & 0x07);

    /*
     * If we won't have enough for a full chunk, just tack this
     * buffer onto the leftover piece and return.
     */
    if (shaStateP->leftoverLen + bufferLen < SHA_CHUNK_BYTES) {
	memcpy((void *)&(shaStateP->leftover[shaStateP->leftoverLen]),
	       (void *)buffer, bufferLen);
	shaStateP->leftoverLen += bufferLen;
	return;
    }

    /* If we have a leftover chunk, process it first. */
    if (shaStateP->leftoverLen > 0) {
	i = (SHA_CHUNK_BYTES - shaStateP->leftoverLen);
	memcpy((void *)&(shaStateP->leftover[shaStateP->leftoverLen]),
	       (void *)buffer, i);
	buffer += i;
	bufferLen -= i;
	buildInts(shaStateP->leftover, chunk);
	shaStateP->leftoverLen = 0;
	transform(shaStateP, chunk);
    }

    while (bufferLen >= SHA_CHUNK_BYTES) {
	buildInts(buffer, chunk);
	transform(shaStateP, chunk);
	buffer += SHA_CHUNK_BYTES;
	bufferLen -= SHA_CHUNK_BYTES;
    }
    assert((bufferLen >= 0) && (bufferLen < SHA_CHUNK_BYTES));

    if (bufferLen > 0) {
	memcpy((void *)&shaStateP->leftover[0], (void *)buffer, bufferLen);
	shaStateP->leftoverLen = bufferLen;
    }
}


/*
 * This method updates the internal state of the hash using
 * any leftover data plus appropriate padding and incorporation
 * of the hash bitcount to finish the hash.  The hash value
 * is not valid until finish() has been called.
 */
void
sha_finish(shaState * shaStateP)
{
    sha_int chunk[SHA_CHUNK_INTS];
    int i;

    if (shaStateP->leftoverLen > (SHA_CHUNK_BYTES - 9)) {
	shaStateP->leftover[shaStateP->leftoverLen++] = 0x80;
	memset(&(shaStateP->leftover[shaStateP->leftoverLen]), 0,
	       (SHA_CHUNK_BYTES - shaStateP->leftoverLen));
	buildInts(shaStateP->leftover, chunk);
	transform(shaStateP, chunk);
	memset(chunk, 0, SHA_CHUNK_BYTES);
    } else {
	shaStateP->leftover[shaStateP->leftoverLen++] = 0x80;
	memset(&(shaStateP->leftover[shaStateP->leftoverLen]), 0,
	       (SHA_CHUNK_BYTES - shaStateP->leftoverLen));
	buildInts(shaStateP->leftover, chunk);
    }
    shaStateP->leftoverLen = 0;

    chunk[SHA_CHUNK_INTS - 2] = shaStateP->bitcountHi;
    chunk[SHA_CHUNK_INTS - 1] = shaStateP->bitcountLo;
    transform(shaStateP, chunk);
}


/*
 * Initialize the hash to its "magic" initial value specified by the
 * SHS standard, and clear out the bitcount and leftover vars.
 * This should be used to initialize an shaState.
 */
void
sha_clear(shaState * shaStateP)
{
    big_endian = (0x01020304 == htonl(0x01020304));

    memcpy((void *)&shaStateP->digest[0], (void *)&hashinit[0],
	   SHA_HASH_BYTES);
    shaStateP->bitcountLo = shaStateP->bitcountHi = 0;
    shaStateP->leftoverLen = 0;
}


/*
 * Hash the buffer and place the result in *shaStateP.
 */
void
sha_hash(shaState * shaStateP, const char *buffer, int bufferLen)
{
    sha_clear(shaStateP);
    sha_update(shaStateP, buffer, bufferLen);
    sha_finish(shaStateP);
}


/*
 * Returns the current state of the hash as an array of 20 bytes.
 * This will be an interim result if finish() has not yet been called.
 */
void
sha_bytes(const shaState * shaStateP, char *bytes)
{
    sha_int temp[SHA_HASH_INTS];
    int i;

    for (i = 0; i < SHA_HASH_INTS; i++)
	temp[i] = htonl(shaStateP->digest[i]);
    memcpy(bytes, (void *)&temp[0], SHA_HASH_BYTES);
}
