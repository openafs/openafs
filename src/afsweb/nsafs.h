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
 * COMPONENT_NAME: Delite Gateway
 *
 */

#ifndef _NSAFS_H_
#define _NSAFS_H_

/*
 * SHA is defined to work on unsigned 32 bit integers.
 * These values should therefore not change.
 */
typedef afs_uint32 sha_int;

#define SHA_BITS_PER_INT	32
#define SHA_CHUNK_BITS		512
#define SHA_CHUNK_BYTES		64
#define SHA_CHUNK_INTS		16
#define SHA_HASH_BITS		160
#define SHA_HASH_BYTES		20
#define SHA_HASH_INTS		5
#define SHA_ROUNDS		80


typedef struct shaState {
    sha_int digest[SHA_HASH_INTS];
    sha_int bitcountLo;
    sha_int bitcountHi;
    sha_int leftoverLen;
    char leftover[SHA_CHUNK_BYTES];
} shaState;


void sha_clear(shaState * shaStateP);
void sha_update(shaState * shaStateP, const char *buffer, int bufferLen);
void sha_finish(shaState * shaStateP);
void sha_hash(shaState * shaStateP, const char *buffer, int bufferLen);
void sha_bytes(const shaState * shaStateP, char *bytes);

extern afs_int32 nsafs_SetToken(struct ktc_principal *aserver,
				struct ktc_token *atoken,
				struct ktc_principal *aclient,
				afs_int32 flags);

#endif /* _NSAFS_H_ */
