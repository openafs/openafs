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

#include "k5s_config.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "k5ssl.h"
#include "k5s_im.h"

/*
 * PKCS #5 / RFC 2898
 *	section 5.2, pages 9-11.
 *
 *	PBKDF2 - password based key derivation function #2
 *	pwdata	P	password, an octet string
 *	salt	S	salt, an octet string
 *	iter	c	iteration count, a positive integer
 *	outdata dkLen,DK	derived key output
 *	hash	PRF	underlying pseudo-random function (such as SHA-1)
 */

#ifndef VAR
#define FAR /**/
#endif

krb5_error_code
krb5i_pkcs5_pbkdf2(const krb5_data *pwdata,
    const krb5_data *salt,
    int iter,
    krb5_data *outdata,
    struct krb5_hash_provider *hash)
{
    krb5_error_code code;
    krb5_int32 itemp;
    krb5_data idata[2];
    unsigned int digest_size, block_size;
    krb5_data digest[1], ptemp[1];
    krb5_keyblock hashkey[1];
    int left, did;
    int i, j, k;
    char *p;
    void *state = 0;

    hash->hash_size(&digest_size);
    hash->block_size(&block_size);
    digest->length = digest_size;
    if (!(digest->data = malloc(digest->length)))
	return ENOMEM;

    memset(ptemp, 0, sizeof *ptemp);
    if (pwdata->length > block_size) {
	/* krb5i_hmac doesn't do this, and we may
	 *  as well do it once here anyways.
	 */
	ptemp->length = digest_size;
	if (!(ptemp->data = malloc(digest->length))) {
	    code = ENOMEM;
	    goto Done;
	}
	if ((code = hash->hash_init(&state))) goto Done;
	if ((code = hash->hash_update(state, pwdata->data, pwdata->length)))
	    goto Done;
	if ((code = hash->hash_digest(state, ptemp->data)))
	    goto Done;
	hashkey->contents = ptemp->data;
	hashkey->length = ptemp->length;
    } else {
	hashkey->contents = pwdata->data;
	hashkey->length = pwdata->length;
    }

    idata[0] = *salt;
    idata[1].data = (char*)&itemp;
    idata[1].length = sizeof itemp;	/* better be 4 !! */

    p = outdata->data;

    for (i = 1, left = outdata->length; left; left -= did, ++i) {
	did = (left > digest_size) ? digest_size : left;
	itemp = htonl(i);
	code = krb5i_hmac(hash, hashkey, 2, idata, digest);
	if (code) {
	    goto Done;
	}
	memcpy(p, digest->data, did);
	for (j = 1; j < iter; ++j) {
	    code = krb5i_hmac(hash, hashkey, 1, digest, digest);
	    if (code) {
		goto Done;
	    }
	    for (k = 0; k < did; ++k)
		p[k] ^= digest->data[k];
	}
	p += did;
    }

Done:
    if (state) hash->free_state(state);
    if (digest->data) {
	memset(digest->data, 0, digest->length);
	free(digest->data);
    }
    if (ptemp->data) {
	memset(ptemp->data, 0, digest->length);
	free(ptemp->data);
    }
    if (code)
	memset(outdata->data, 0, outdata->length);
    return code;
}
