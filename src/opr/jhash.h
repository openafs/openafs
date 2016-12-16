/*
 * Copyright (c) 2006 Bob Jenkins
 * Copyright (c) 2011 Your File System Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This is an OPR version of Bob Jenkins' hash routines. His original copyright
 * declaration reads:
 *
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 *
 * You can use this free for any purpose.  It's in the public domain.
 * It has no warranty.
 */

#ifndef OPENAFS_OPR_JHASH_H
#define OPENAFS_OPR_JHASH_H 1

#define opr_jhash_size(n) ((afs_uint32)1<<(n))
#define opr_jhash_mask(n) (opr_jhash_size(n)-1)
#define opr_jhash_rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define opr_jhash_mix(a,b,c) \
{ \
    a -= c;  a ^= opr_jhash_rot(c, 4);  c += b; \
    b -= a;  b ^= opr_jhash_rot(a, 6);  a += c; \
    c -= b;  c ^= opr_jhash_rot(b, 8);  b += a; \
    a -= c;  a ^= opr_jhash_rot(c,16);  c += b; \
    b -= a;  b ^= opr_jhash_rot(a,19);  a += c; \
    c -= b;  c ^= opr_jhash_rot(b, 4);  b += a; \
}

#define opr_jhash_final(a,b,c) \
{ \
    c ^= b; c -= opr_jhash_rot(b,14); \
    a ^= c; a -= opr_jhash_rot(c,11); \
    b ^= a; b -= opr_jhash_rot(a,25); \
    c ^= b; c -= opr_jhash_rot(b,16); \
    a ^= c; a -= opr_jhash_rot(c,4);  \
    b ^= a; b -= opr_jhash_rot(a,14); \
    c ^= b; c -= opr_jhash_rot(b,24); \
}

static_inline afs_uint32
opr_jhash(const afs_uint32 *k, size_t length, afs_uint32 initval)
{
    afs_uint32 a,b,c;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((afs_uint32)length)<<2) + initval;

    while (length > 3) {
	a += k[0];
	b += k[1];
	c += k[2];
	opr_jhash_mix(a, b, c);
	length -= 3;
	k += 3;
    }

    /* All the case statements fall through */
    switch(length) {
      case 3 : c+=k[2];
      case 2 : b+=k[1];
      case 1 : a+=k[0];
	opr_jhash_final(a, b, c);
      case 0:     /* case 0: nothing left to add */
	break;
    }

    return c;
}

/* Simplified version of the above function to hash just one int */

static_inline afs_uint32
opr_jhash_int(afs_uint32 a, afs_uint32 initval) {
   afs_uint32 b, c;

   a += 0xdeadbeef + 4 + initval;
   b = c = 0xdeadbeef + 4 + initval;
   opr_jhash_final(a, b, c);

   return c;
}

/* and one to do two ints */

static_inline afs_uint32
opr_jhash_int2(afs_uint32 a, afs_uint32 b, afs_uint32 initval)
{
   afs_uint32 c;

   a += 0xdeadbeef + 8 + initval;
   b += 0xdeadbeef + 8 + initval;
   c =  0xdeadbeef + 8 + initval;
   opr_jhash_final(a, b, c);

   return c;
}

static_inline afs_uint32
opr_jhash_opaque(const void *val, size_t length, afs_uint32 initval)
{
    const unsigned char *str = (const unsigned char *) val;
    afs_uint32 a,b,c;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + ((afs_uint32)length) + initval;

    while (length > 12) {
	a += (afs_uint32) str[3]<<24 |
	    (afs_uint32) str[2]<<16 |
	    (afs_uint32) str[1]<<8 |
	    (afs_uint32) str[0];
	b += (afs_uint32) str[7]<<24 |
	    (afs_uint32) str[6]<<16 |
	    (afs_uint32) str[5]<<8 |
	    (afs_uint32) str[4];
	c += (afs_uint32) str[11]<<24 |
	    (afs_uint32) str[10]<<16 |
	    (afs_uint32) str[9]<<8 |
	    (afs_uint32) str[8];
	opr_jhash_mix(a, b, c);
	length -= 12;
	str += 12;
    }

    /* All the case statements fall through */
    switch(length) {
      case 12 : c += (afs_uint32) str[11]<<24;
      case 11 : c += (afs_uint32) str[10]<<16;
      case 10 : c += (afs_uint32) str[9]<<8;
      case 9  : c += (afs_uint32) str[8];
      case 8  : b += (afs_uint32) str[7]<<24;
      case 7  : b += (afs_uint32) str[6]<<16;
      case 6  : b += (afs_uint32) str[5]<<8;
      case 5  : b += (afs_uint32) str[4];
      case 4  : a += (afs_uint32) str[3]<<24;
      case 3  : a += (afs_uint32) str[2]<<16;
      case 2  : a += (afs_uint32) str[1]<<8;
      case 1  : a += (afs_uint32) str[0];
	opr_jhash_final(a, b, c);
      case 0:     /* case 0: nothing left to add */
	break;
    }

    return c;
}

#endif
