/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This code borrows from nsafs.c - slightly modified - names,etc. */

#include <afsconfig.h>
#include <afs/param.h>


#include "apache_afs_cache.h"

/*
 * Value used to initialize SHA checksums on username/password pairs
 */
afs_uint32 weblog_login_pad[SHA_HASH_INTS] = {
    0x0D544971, 0x2281AC5B, 0x58B51218, 0x4085E08D, 0xB68C484B
};

/*
 * Cache of AFS logins
 */
struct {
    struct weblog_login *head;
    struct weblog_login *tail;
} weblog_login_cache[WEBLOG_LOGIN_HASH_SIZE];

/* shchecksum.c copied here */

/*
 * This module implements the Secure Hash Algorithm (SHA) as specified in
 * the Secure Hash Standard (SHS, FIPS PUB 180.1).
 */

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
/*    assert((bufferLen >= 0) && (bufferLen < SHA_CHUNK_BYTES)); */
    if ((bufferLen < 0) || (bufferLen > SHA_CHUNK_BYTES)) {
	fprintf(stderr, "apache_afs_cache: ASSERTION FAILED...exiting\n");
	exit(-1);
    }

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


/*
 * Hash function for the AFS login cache
 */
int
weblog_login_hash(char *name, char *cell)
{
    char *p;
    afs_uint32 val;
    for (val = *name, p = name; *p != '\0'; p++) {
	val = (val << 2) ^ val ^ (afs_uint32) (*p);
    }
    for (p = cell; *p != '\0'; p++) {
	val = (val << 2) ^ val ^ (afs_uint32) (*p);
    }
    return val & (WEBLOG_LOGIN_HASH_SIZE - 1);
}

/*
 * Compute a SHA checksum on the username, cellname, and password
 */
void
weblog_login_checksum(char *user, char *cell, char *passwd, char *cksum)
{
    int passwdLen;
    int userLen;
    int cellLen;
    char *shaBuffer;
    shaState state;

    /*
     * Compute SHA(username,SHA(password,pad))
     */
    passwdLen = strlen(passwd);
    userLen = strlen(user);
    cellLen = strlen(cell);
    shaBuffer =
	(char *)malloc(MAX(userLen + cellLen, passwdLen) + SHA_HASH_BYTES);
    strcpy(shaBuffer, passwd);
    memcpy((void *)(shaBuffer + passwdLen), (void *)(&weblog_login_pad[0]),
	   SHA_HASH_BYTES);
    sha_clear(&state);
    sha_hash(&state, shaBuffer, passwdLen + SHA_HASH_BYTES);
    memcpy(shaBuffer, user, userLen);
    memcpy(shaBuffer + userLen, cell, cellLen);
    sha_bytes(&state, shaBuffer + userLen + cellLen);
    sha_clear(&state);
    sha_hash(&state, shaBuffer, userLen + cellLen + SHA_HASH_BYTES);
    sha_bytes(&state, &cksum[0]);
    memset(shaBuffer, 0, MAX(userLen + cellLen, passwdLen) + SHA_HASH_BYTES);
    free(shaBuffer);
}

/*
 * Look up a login ID in the cache. If an entry name is found for the
 * given username, and the SHA checksums match, then
 * set the token parameter and return 1, otherwise return 0.
 */
int
weblog_login_lookup(char *user, char *cell, char *cksum, char *token)
{
    int index;
    long curTime;
    struct weblog_login *loginP, *tmpP, loginTmp;

    /*
     * Search the hash chain for a matching entry, free
     * expired entries as we search
     */
    index = weblog_login_hash(user, cell);
    curTime = time(NULL);

    loginP = weblog_login_cache[index].head;
    while (loginP != NULL) {
	if (loginP->expiration < curTime) {
	    tmpP = loginP;
	    loginP = tmpP->next;

	    DLL_DELETE(tmpP, weblog_login_cache[index].head,
		       weblog_login_cache[index].tail, next, prev);
	    free(tmpP);
	    continue;
	}
	if (strcmp(loginP->username, user) == 0
	    && strcmp(loginP->cellname, cell) == 0
	    && memcmp((void *)&loginP->cksum[0], (void *)cksum,
		      SHA_HASH_BYTES) == 0) {

	    memcpy((void *)token, (void *)&loginP->token[0], MAXBUFF);
	    return loginP->tokenLen;
	}
	loginP = loginP->next;
    }
    return 0;
}

/*
 * Insert a login token into the cache. If the user already has an entry,
 * then overwrite the old entry.
 */
int
weblog_login_store(char *user, char *cell, char *cksum, char *token,
		   int tokenLen, afs_uint32 expiration)
{
    int index;
    long curTime;
    struct weblog_login *loginP, *tmpP, loginTmp;

    int parseToken(char *tokenBuf);

    /*
     * Search the hash chain for a matching entry, free
     * expired entries as we search
     */
    index = weblog_login_hash(user, cell);
    curTime = time(NULL);
    loginP = weblog_login_cache[index].head;

    while (loginP != NULL) {
	if (strcmp(loginP->username, user) == 0
	    && strcmp(loginP->cellname, cell) == 0) {
	    break;
	}
	if (loginP->expiration < curTime) {
	    tmpP = loginP;
	    loginP = tmpP->next;

	    DLL_DELETE(tmpP, weblog_login_cache[index].head,
		       weblog_login_cache[index].tail, next, prev);
	    free(tmpP);
	    continue;
	}
	loginP = loginP->next;
    }
    if (loginP == NULL) {
	loginP = (struct weblog_login *)malloc(sizeof(struct weblog_login));
	strcpy(&loginP->username[0], user);
	strcpy(&loginP->cellname[0], cell);
    } else {
	DLL_DELETE(loginP, weblog_login_cache[index].head,
		   weblog_login_cache[index].tail, next, prev);
    }

    memcpy((void *)&loginP->cksum[0], (void *)cksum, SHA_HASH_BYTES);
    loginP->expiration = expiration;
    loginP->tokenLen = getTokenLen(token);
    memcpy((void *)&loginP->token[0], (void *)token, MAXBUFF);

    DLL_INSERT_TAIL(loginP, weblog_login_cache[index].head,
		    weblog_login_cache[index].tail, next, prev);
    return 0;
}

token_cache_init()
{
    int i;
    for (i = 0; i < WEBLOG_LOGIN_HASH_SIZE; i++) {
	DLL_INIT_LIST(weblog_login_cache[i].head, weblog_login_cache[i].tail);
    }
}

int
getTokenLen(char *buf)
{
    afs_int32 len = 0;
    afs_int32 rc = 0;
    char cellName[WEBLOG_CELLNAME_MAX];
    char *tp;
    int n = sizeof(afs_int32);
    struct ClearToken {
	afs_int32 AuthHandle;
	char HandShakeKey[8];
	afs_int32 ViceId;
	afs_int32 BeginTimestamp;
	afs_int32 EndTimestamp;
    } token;
    tp = buf;
    memcpy(&len, tp, sizeof(afs_int32));	/* get size of secret token */
    rc = (len + sizeof(afs_int32));
    tp += (sizeof(afs_int32) + len);	/* skip secret token and its length */
    memcpy(&len, tp, sizeof(afs_int32));	/* get size of clear token */
    if (len != sizeof(struct ClearToken)) {
#ifdef DEBUG
	fprintf(stderr,
		"apache_afs_cache.c:getExpiration:"
		"something's wrong with the length of ClearToken:%d\n", len);
#endif
	return -1;
    }
    rc += (sizeof(afs_int32) + len);	/* length of clear token + length itself */
    tp += (sizeof(afs_int32) + len);	/* skip clear token  and its length */
    rc += sizeof(afs_int32);	/* length of primary flag */
    tp += sizeof(afs_int32);	/* skip over primary flag */
    strcpy(cellName, tp);
    if (cellName != NULL)
	rc += strlen(cellName);
    return rc;
}

long
getExpiration(char *buf)
{
    afs_int32 len = 0;
    char *tp;
    int n = sizeof(afs_int32);
    struct ClearToken {
	afs_int32 AuthHandle;
	char HandShakeKey[8];
	afs_int32 ViceId;
	afs_int32 BeginTimestamp;
	afs_int32 EndTimestamp;
    } token;

    tp = buf;
    memcpy(&len, tp, sizeof(afs_int32));	/* get size of secret token */
    tp += (sizeof(afs_int32) + len);	/* skip secret token and its length */
    memcpy(&len, tp, sizeof(afs_int32));	/* get size of clear token */
    if (len != sizeof(struct ClearToken)) {
#ifdef DEBUG
	fprintf(stderr,
		"apache_afs_cache.c:getExpiration:"
		"something's wrong with the length of ClearToken:%d\n", len);
#endif
	return -1;
    }

    tp += sizeof(afs_int32);	/* skip length of clear token */
    memcpy(&token, tp, sizeof(struct ClearToken));	/* copy the token */
    return token.EndTimestamp;
}
