/*
 * shs.h--NIST Secure Hash Standard as implemented in
 *        _Applied Cryptography_ by Bruce Schneier
 *
 * Copyright (c) 2003
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

typedef unsigned char    SHS_BYTE;
#if 1
typedef unsigned int    SHS_LONG;
#else
typedef unsigned long    SHS_LONG;
#endif

/* The SHS block size and message digest sizes in bytes */
#define SHS_BLOCKSIZE 64
#define SHS_DIGESTSIZE 20

typedef struct
{
    SHS_LONG digest[5];		/* message digest */
    SHS_LONG countLo, countHi;	/* 64-bit bit count */
    SHS_LONG data[16];		/* SHS data buffer */
} SHS_INFO;

#ifdef HAVE_PROTOTYPES
#ifndef PROTO
#define PROTO(args) args
#endif
#endif
#ifndef PROTO
#define PROTO(args) ()
#endif

void shsInit PROTO ((SHS_INFO *shsInfo));
void shsTransform PROTO ((SHS_INFO *shsInfo));
void shsUpdate PROTO ((SHS_INFO *shsInfo, SHS_BYTE *buffer, int count));
void shsFinal PROTO ((SHS_INFO *shsInfo));
#define shsDigest(si)	((SHS_BYTE*)((si)->data))
