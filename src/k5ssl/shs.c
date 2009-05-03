/*
 * shs.c--NIST Secure Hash Standard as implemented in
 *        _Applied Cryptography_ by Bruce Schneier
 *
 * originally written 2 September 1992, Peter C. Gutmann.
 *  His version placed in the public domain.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <sys/types.h>

#include "k5s_shs.h"

#ifdef KERNEL
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#endif

#define bzero(a,c)	memset(a,0,c)
#define bcopy(a,b,c)	memcpy(b,a,c)

/* SHS f()-functions */
#if 0
#define f1(x,y,z)	((x & y) | (~x & z)) /* rounds 0-19 */
#define f2(x,y,z)	(x ^ y ^ z) /* rounds 20-39 */
#define f3(x,y,z)	((x & y) | (x & z) | (y & z)) /* rounds 40-59 */
#define f4(x,y,z)	(x ^ y ^ z) /* rounds 60-79 */
#else
/* < Wei Dai <weidai@eskimo.com, < Peter Gutmann, < Rich Schroeppel */
#define f1(x,y,z)	(((y ^ z) & x) ^ z)
#define f2(x,y,z)	(x ^ y ^ z) /* rounds 20-39 */
#define f3(x,y,z)	((x & y) | (x & z) | (y & z)) /* rounds 40-59 */
#define f4(x,y,z)	f2(x,y,z)
#endif

/* SHS mysterious contants */
#define K1 0x5a827999L		/* rounds 0-19 */
#define K2 0x6ed9eba1L		/* rounds 20-39 */
#define K3 0x8f1bbcdcL		/* rounds 40-59 */
#define K4 0xca62c1d6L		/* rounds 60-79 */

/* SHS initial values */
#define h0init 0x67452301L
#define h1init 0xefcdab89L
#define h2init 0x98BADCFEL
#define h3init 0x10325476L
#define h4init 0xc3d2e1f0L


/* 32-bit rotate--kludged with shifts */
#define S(n,X) ((X << n) | (X >> (32 - n)))

/* initial expanding function */
#ifdef SHA_0
/* old, bad */
#define expand(c) W[c] = W[c-3] ^ W[c-8] ^ W[c-14] ^ W[c-16]
#else
#define expand(c) temp = W[c-3] ^ W[c-8] ^ W[c-14] ^ W[c-16], W[c] = S(1,temp)
#endif

#define FF(A,B,C,D,E,c) E += S(5,A) + f1(B,C,D) + W[c] + K1; B=S(30,B); QQ(c);
#define GG(A,B,C,D,E,c) E += S(5,A) + f2(B,C,D) + W[c] + K2; B=S(30,B); QQ(c);
#define HH(A,B,C,D,E,c) E += S(5,A) + f3(B,C,D) + W[c] + K3; B=S(30,B); QQ(c);
#define II(A,B,C,D,E,c) E += S(5,A) + f4(B,C,D) + W[c] + K4; B=S(30,B); QQ(c);
#ifdef SHS_DEBUG
#define QQ(c) if (shs_print_flag & 2) printf ("%d: A=%u B=%u C=%u D=%u E=%u W=%u\n", c,A,B,C,D,E, W[c])
#else
#define QQ(c) /**/
#endif

#ifdef SHS_DEBUG
int shs_print_flag;
#endif

#ifndef WORDS_BIGENDIAN
static void
longReverse(SHS_LONG *buffer, int byteCount)
{
    SHS_LONG value;
    int count;

    byteCount /= sizeof(SHS_LONG);
    for (count = 0; count < byteCount; ++count)
    {
#define SWAP1(l) ((l << 16) | ( l >> 16))
#define SWAP2(l) (((l & 0xFF00FF00L) >> 8) | ((l & 0x00FF00FFL) << 8))
	value = SWAP1(buffer[count]);
	buffer[count] = SWAP2(value);
    }
}
#endif

/* initialize the SHS values */
void
shsInit(SHS_INFO *shsInfo)
{
    /* zero the data */
    bzero(shsInfo, sizeof(*shsInfo));

    /* initalize the h-vars */
    shsInfo->digest[0] = h0init;
    shsInfo->digest[1] = h1init;
    shsInfo->digest[2] = h2init;
    shsInfo->digest[3] = h3init;
    shsInfo->digest[4] = h4init;

    /* initialize bit count */
    shsInfo->countLo = shsInfo->countHi = 0L;
#ifdef SHS_DEBUG
{char *cp; shs_print_flag = (cp = getenv("SHS_PRINT_FLAG"))
? atoi(cp) : 0; }
#endif

    return;
}


/* perform the SHS transformations. This code, like MD5 seems to break
   some optimizing compilers--it may be necessary to split it into
   sections, e.g., based on the four rounds. */
void
shsTransform(SHS_INFO *shsInfo, char *data)
{
    SHS_LONG W[80];
{
    register SHS_LONG temp;

#if 0
printf ("%#lx: shsTransform\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n%#lx: %08x%08x\n"
, shsInfo
, shsInfo
,0[(long*)data]
,1[(long*)data]
, shsInfo
,2[(long*)data]
,3[(long*)data]
, shsInfo
,4[(long*)data]
,5[(long*)data]
, shsInfo
,6[(long*)data]
,7[(long*)data]
, shsInfo
,8[(long*)data]
,9[(long*)data]
, shsInfo
,10[(long*)data]
,11[(long*)data]
, shsInfo
,12[(long*)data]
,13[(long*)data]
, shsInfo
,14[(long*)data]
,15[(long*)data]
);
#endif

    /* A: copy the data buffer into the local work buffer */
    bcopy(data, W, 64);
#ifdef SHS_DEBUG
if (shs_print_flag) {printf("SHS%d: %#lx, %#lx, %#lx, %#lx, %#lx\n\
      %#lx, %#lx, %#lx, %#lx, %#lx\n      %#lx, %#lx, %#lx, %#lx, %#lx\n",
(shsInfo->countLo>>(3+6)),
W[0], W[1], W[2], W[3],
W[4], W[5], W[6], W[7],
W[8], W[9], W[10], W[11],
W[12], W[13], W[14], W[15]); }
#endif
#ifndef WORDS_BIGENDIAN
#if 0
    longReverse(W, 64);
#else
	{
		int i;
		for (i = 0; i < 16; ++i)
		{
			temp=SWAP1(W[i]);
			W[i] = SWAP2(temp);
		}
	}
#endif
#endif
expand(16); expand(17); expand(18); expand(19);
expand(20); expand(21); expand(22); expand(23);
expand(24); expand(25); expand(26); expand(27);
expand(28); expand(29); expand(30); expand(31);
expand(32); expand(33); expand(34); expand(35);
expand(36); expand(37); expand(38); expand(39);
expand(40); expand(41); expand(42); expand(43);
expand(44); expand(45); expand(46); expand(47);
expand(48); expand(49); expand(50); expand(51);
expand(52); expand(53); expand(54); expand(55);
expand(56); expand(57); expand(58); expand(59);
expand(60); expand(61); expand(62); expand(63);
expand(64); expand(65); expand(66); expand(67);
expand(68); expand(69); expand(70); expand(71);
expand(72); expand(73); expand(74); expand(75);
expand(76); expand(77); expand(78); expand(79);
}

{
    register SHS_LONG A, B, C, D, E;

    /* C: set up first buffer */
    A = shsInfo->digest[0];
    B = shsInfo->digest[1];
    C = shsInfo->digest[2];
    D = shsInfo->digest[3];
    E = shsInfo->digest[4];

    /* D: serious mangling, divided into four sub-rounds */
FF(A,B,C,D,E,0); FF(E,A,B,C,D,1); FF(D,E,A,B,C,2); FF(C,D,E,A,B,3);
FF(B,C,D,E,A,4); FF(A,B,C,D,E,5); FF(E,A,B,C,D,6); FF(D,E,A,B,C,7);
FF(C,D,E,A,B,8); FF(B,C,D,E,A,9); FF(A,B,C,D,E,10); FF(E,A,B,C,D,11);
FF(D,E,A,B,C,12); FF(C,D,E,A,B,13); FF(B,C,D,E,A,14); FF(A,B,C,D,E,15);
FF(E,A,B,C,D,16); FF(D,E,A,B,C,17); FF(C,D,E,A,B,18); FF(B,C,D,E,A,19);
GG(A,B,C,D,E,20); GG(E,A,B,C,D,21); GG(D,E,A,B,C,22); GG(C,D,E,A,B,23);
GG(B,C,D,E,A,24); GG(A,B,C,D,E,25); GG(E,A,B,C,D,26); GG(D,E,A,B,C,27);
GG(C,D,E,A,B,28); GG(B,C,D,E,A,29); GG(A,B,C,D,E,30); GG(E,A,B,C,D,31);
GG(D,E,A,B,C,32); GG(C,D,E,A,B,33); GG(B,C,D,E,A,34); GG(A,B,C,D,E,35);
GG(E,A,B,C,D,36); GG(D,E,A,B,C,37); GG(C,D,E,A,B,38); GG(B,C,D,E,A,39);
HH(A,B,C,D,E,40); HH(E,A,B,C,D,41); HH(D,E,A,B,C,42); HH(C,D,E,A,B,43);
HH(B,C,D,E,A,44); HH(A,B,C,D,E,45); HH(E,A,B,C,D,46); HH(D,E,A,B,C,47);
HH(C,D,E,A,B,48); HH(B,C,D,E,A,49); HH(A,B,C,D,E,50); HH(E,A,B,C,D,51);
HH(D,E,A,B,C,52); HH(C,D,E,A,B,53); HH(B,C,D,E,A,54); HH(A,B,C,D,E,55);
HH(E,A,B,C,D,56); HH(D,E,A,B,C,57); HH(C,D,E,A,B,58); HH(B,C,D,E,A,59);
II(A,B,C,D,E,60); II(E,A,B,C,D,61); II(D,E,A,B,C,62); II(C,D,E,A,B,63);
II(B,C,D,E,A,64); II(A,B,C,D,E,65); II(E,A,B,C,D,66); II(D,E,A,B,C,67);
II(C,D,E,A,B,68); II(B,C,D,E,A,69); II(A,B,C,D,E,70); II(E,A,B,C,D,71);
II(D,E,A,B,C,72); II(C,D,E,A,B,73); II(B,C,D,E,A,74); II(A,B,C,D,E,75);
II(E,A,B,C,D,76); II(D,E,A,B,C,77); II(C,D,E,A,B,78); II(B,C,D,E,A,79);
#ifdef SHS_DEBUG
if (shs_print_flag) printf (" -> %#lx, %#lx, %#lx, %#lx, %#lx\n", A,B,C,D,E);
#endif

    /* E: build message digest */
    shsInfo->digest[0] += A;
    shsInfo->digest[1] += B;
    shsInfo->digest[2] += C;
    shsInfo->digest[3] += D;
    shsInfo->digest[4] += E;
}
}


/* update SHS for a block of data */
void
shsUpdate(SHS_INFO *shsInfo, SHS_BYTE *buffer, int count)
{
    int	offset, need;

    /* determine if there are left over blocks in the
       shs data. they are specially handled below */
    offset = (int) ((shsInfo->countLo >> 3) & 0x3f);
    need = SHS_BLOCKSIZE - offset;
#if 0
printf ("%#lx: Retained %d bytes, need %d more bytes first pass\n", shsInfo, offset, need);
#endif

    /* update bitcount */
    if ((shsInfo->countLo + ((SHS_LONG) count << 3)) < shsInfo->countLo)
        shsInfo->countHi++;	/* carry from low to high bitCount */

    shsInfo->countLo += ((SHS_LONG) count << 3);
    shsInfo->countHi += ((SHS_LONG) count >> 29);

    /* if there were indeed left over data blocks,
       see if the incoming data is sufficient to
       fill to SHS_BLOCKSIZE. if not, copy the
       incoming data and return; otherwise fill
       the block, perform a transformation, and
       continue as usual */
    if (offset && count >= need)
    {
#if 0
printf ("%#lx: Fill with %d bytes\n", shsInfo, need);
#endif
        bcopy(buffer, (SHS_BYTE *) shsInfo->data + offset, need);
	buffer += need;	/* NEW!!! */
	shsTransform(shsInfo, (SHS_BYTE*) shsInfo->data);
	count -= need;
	if (!count)
            return;
	offset = 0;
    }

    /* process data in SHS_BLOCKSIZE chunks */
    while (count >= SHS_BLOCKSIZE)
    {
        shsTransform(shsInfo, buffer);
        buffer += SHS_BLOCKSIZE;
        count -= SHS_BLOCKSIZE;
    }

    /* store the left over data */
    if (count)
#if 0
printf ("%#lx: Saved %d bytes\n", shsInfo, count),
#endif
	bcopy(buffer, (SHS_BYTE *) shsInfo->data + offset, count);
}


/* handle any remaining bytes of data. this should only happen once on
   the final lot of data. */
void
shsFinal(SHS_INFO *shsInfo)
{
    int count;
    SHS_LONG lowBitCount;
    SHS_LONG highBitCount;

    lowBitCount = shsInfo->countLo;
    highBitCount = shsInfo->countHi;

    /* compute number of bytes mod 64 */
    count = (int) ((shsInfo->countLo >> 3) & 0x3f);

    /* set the first char of padding to 0x80. This is safe since there
       is always at least one byte free. */
    ((SHS_BYTE *) shsInfo->data)[count++] = 0x80;

    /* pad out to 56 mod 64 */
    if (count > 56)
    {
	/* two lots of padding: pad the first block to 64 bytes */
	bzero((SHS_BYTE *) (&shsInfo->data) + count, 64 - count);
	shsTransform(shsInfo, (SHS_BYTE*) shsInfo->data);

	/* fill out the next block with 56 bytes */
	bzero(&shsInfo->data, 56);
    }
    else
    {
	/* pad block to 56 bytes */
	bzero((SHS_BYTE *) (&shsInfo->data) + count, 56 - count);
    }

    /* append length in bits and transform */
    shsInfo->data[14] = highBitCount;
    shsInfo->data[15] = lowBitCount;
#ifndef WORDS_BIGENDIAN
    longReverse((SHS_LONG *) (shsInfo->data+14), 8);
#endif


    shsTransform(shsInfo, (SHS_BYTE*) shsInfo->data);
    shsInfo->data[0] = shsInfo->digest[0];
    shsInfo->data[1] = shsInfo->digest[1];
    shsInfo->data[2] = shsInfo->digest[2];
    shsInfo->data[3] = shsInfo->digest[3];
    shsInfo->data[4] = shsInfo->digest[4];
#ifndef WORDS_BIGENDIAN
    longReverse(shsInfo->data, 20);
#endif
}

