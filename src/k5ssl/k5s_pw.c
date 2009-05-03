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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include "k5ssl.h"
#include "k5s_im.h"

extern struct krb5_enc_provider krb5i_des_cbc[];
extern struct krb5_enc_provider krb5i_des3_cbc[];
extern struct krb5_enc_provider krb5i_cast_cbc[];
extern struct krb5_enc_provider krb5i_rc6_cts[];
extern struct krb5_enc_provider krb5i_arcfour[];
extern struct krb5_enc_provider krb5i_aes128_cts[];
extern struct krb5_enc_provider krb5i_aes256_cts[];
extern struct krb5_hash_provider krb5i_keyhash_descbc[];
extern struct krb5_hash_provider krb5i_hash_hmac_sha1[];
extern struct krb5_hash_provider krb5i_hash_sha1[];
extern struct krb5_hash_provider krb5i_hash_md5[];
extern struct krb5_hash_provider krb5i_hash_md4[];
extern char * krb5i_des_unix_crypt(const char *, const char *, char *);

struct s2k_element;
typedef int string_to_key_fct (krb5_context,
    struct s2k_element *,
    const krb5_data *,
    const krb5_salt *,
    krb5_keyblock *);
struct s2k_element {
    krb5_enctype t;
    struct krb5_enc_provider *e;
    struct krb5_hash_provider *h;
    string_to_key_fct *f;
};
static char kerberos[] = "kerberos";

krb5_error_code
krb5_principal2salt (krb5_context context,
    krb5_const_principal princ, krb5_salt *salt)
{
    int i, l;
    int code;
    unsigned char *p;
    memset(salt, 0, sizeof *salt);
    l = 0;
    for (i = 0; i < princ->length; ++i) {
	l += princ->data[i].length;
    }
    l += princ->realm.length;
    code = ENOMEM;
    if (!(salt->s2kdata.data = malloc(l+1)))
	goto Done;
    p = salt->s2kdata.data;
    memcpy(p, princ->realm.data, princ->realm.length);
    p += princ->realm.length;
    for (i = 0; i < princ->length; ++i) {
	memcpy(p, princ->data[i].data, princ->data[i].length);
	p += princ->data[i].length;
    }
    salt->s2kdata.length = p - salt->s2kdata.data;
    code = 0;
Done:
    return code;
}

#if KRB5_DES_SUPPORT
/* note: krb5i_andrew_s2k algorithm limits keyspace to 2^48 */
int
krb5i_andrew_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    char passbuf[9], crypt_buf[8];
    int i, l;
    int code;
    krb5_data data[1];
    char iobuf[32];

    memset(passbuf, 0, sizeof passbuf);
    if (salt) {
	l = salt->s2kdata.length;
	if (l > 8) l = 8;
	memcpy(passbuf, salt->s2kdata.data, l);
	for (i = 0; i < l; ++i)
	    if (isupper(passbuf[i]))
		passbuf[i] ^= 32;
    }
    for (i = 0; i < string->length; ++i)
	passbuf[i] ^= i[(char *) string->data];
    for (i = 0; i < 8; ++i)
	if (!passbuf[i])
	    passbuf[i] = 'X';
    memcpy(crypt_buf, krb5i_des_unix_crypt(passbuf, "p1", iobuf)+2, 8);
    for (i = 0; i < 7; ++i) {
	crypt_buf[i] <<= 1;
	if (crypt_buf[7] & (1<<i))
	    crypt_buf[i] |= 1;
    }
    if (!(key->contents = malloc(8)))
	return ENOMEM;
    key->length = 8;
    key->enctype = et->t;
    data->data = crypt_buf;
    data->length = 7;
    key->length = 8;
    if ((code = et->e->make_key(data, key, 0))) goto Done;
Done:
    return code;
}

int
krb5i_afs3_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    krb5_data data[1];
    int i, l;
    int code;
    void *state;
    unsigned char k[8], passbuf[512], *p;

    memset(data, 0, sizeof *data);
    et->e->key_size(&data->length, &key->length);
    l = string->length;
    if (l > sizeof passbuf) l = sizeof passbuf;
    memcpy(passbuf,string->data,l);
    p = passbuf + l;
    if (salt) {
	l = sizeof passbuf-l;
	if (l > salt->s2kdata.length) l = salt->s2kdata.length;
	memcpy(p, salt->s2kdata.data, l);
	for (i = 0; i < l; ++i)
	    if (isupper(p[i]))
		p[i] ^= 0x20;
	p += l;
    }
    l = (passbuf - p) & 7;
    memset(p, 0, l);
    l += p - passbuf;
    memcpy(k, kerberos, 8);
    for (i = 0; i < 7; ++i) {
	k[i] &= ~1;
	if (k[7] & (2<<i))
	    k[i] |= 1;
    }
    data->data = k;
    key->contents = malloc(key->length);
    if (!key->contents) goto Done;
    if ((code = et->e->make_key(data, key, 0))) goto Done;
    if ((code = et->h->hash_init(&state))) goto Done;
    if ((code = et->h->hash_setivec(state, kerberos, 8))) goto Done;
    if ((code = et->h->hash_setkey(state, context, key, 0, NULL))) goto Done;
    if ((code = et->h->hash_update(state, passbuf, l))) goto Done;
    if ((code = et->h->hash_digest(state, k))) goto Done;

    if ((code = et->h->hash_setivec(state, k, 8))) goto Done;
    for (i = 0; i < 7; ++i) {
	k[i] &= ~1;
	if (k[7] & (2<<i))
	    k[i] |= 1;
    }
    if ((code = et->e->make_key(data, key, 0))) goto Done;
    if ((code = et->h->hash_setkey(state, context, key, 0, NULL))) goto Done;
    if ((code = et->h->hash_update(state, passbuf, l))) goto Done;
    if ((code = et->h->hash_digest(state, k))) goto Done;
    for (i = 0; i < 7; ++i) {
	k[i] &= ~1;
	if (k[7] & (2<<i))
	    k[i] |= 1;
    }
    if ((code = et->e->make_key(data, key, 0))) goto Done;
    key->enctype = et->t;
Done:
    if (state) et->h->free_state(state);
    return code;
}

int
krb5i_mit_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    krb5_data data[1];
    unsigned char *inp = 0;
    unsigned char *cp;
    int l, l2, i, c, f;
    unsigned char k[8], *p;
    void *state = 0;
    static unsigned char sw[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
    int code;

    memset(data, 0, sizeof *data);
    et->e->key_size(&data->length, &key->length);
    l = string->length;
    if (salt) l += salt->s2kdata.length;
    code = ENOMEM;
    l2 = (l+7)&~7;
    if (!(inp = malloc(l2))) goto Done;
    memcpy(inp, string->data, string->length);
    if (salt) memcpy(inp+string->length, salt->s2kdata.data, salt->s2kdata.length);
    if (l&7)
	memset(inp+l, 0, (-l)&7);
    cp = inp;
    f = 1;
    memset(k, 0, sizeof k);
    for (p = k, i = 0; i < l; ++i) {
	c = *cp++;
	if (f)
		*p++ ^= (c<<1);
	else
		*--p ^= (sw[(c>>4)] | (sw[15 & c]<<4));
	f ^= ((i&7) == 7);
    }
    for (i = 0; i < 7; ++i) {
	k[i] &= ~1;
	if (k[7] & (2<<i))
	    k[i] |= 1;
    }
    data->data = k;
    key->contents = malloc(key->length);
    if (!key->contents) goto Done;
    if ((code = et->e->make_key(data, key, 0))) goto Done;
    if ((code = et->h->hash_init(&state))) goto Done;
    if ((code = et->h->hash_setivec(state, key->contents,
	    key->length)))
	goto Done;
    if ((code = et->h->hash_setkey(state, context, key, 0, NULL))) goto Done;
    if ((code = et->h->hash_update(state, inp, l2))) goto Done;
    if ((code = et->h->hash_digest(state, k))) goto Done;
    for (i = 0; i < 7; ++i) {
	k[i] &= ~1;
	if (k[7] & (2<<i))
	    k[i] |= 1;
    }
    if ((code = et->e->make_key(data, key, 0))) goto Done;
    key->enctype = et->t;
Done:
    if (state) et->h->free_state(state);
    if (inp) free(inp);
    return code;
}

int
krb5i_des_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    if (salt && salt->s2kparams.length == 1
	    && 0[(unsigned char*)salt->s2kparams.data] == 1) {
	if (string->length <= 8)
	    return krb5i_andrew_s2k(context, et, string, salt, key);
	else
	    return krb5i_afs3_s2k(context, et, string, salt, key);
    }
    return krb5i_mit_s2k(context, et, string, salt, key);
}
#endif

#if KRB5_DES_SUPPORT || KRB5_DES3_SUPPORT
int
krb5i_dk_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    int l;
    int code;
    unsigned char *work = 0, *p;
    krb5_data data[1];
    krb5_keyblock temp_key[1];

    memset(key, 0, sizeof *key);
    memset(data, 0, sizeof *data);
    et->e->key_size(&data->length, &key->length);
    l = string->length + data->length + key->length;
    temp_key->length = key->length;
    if (salt)
	l += salt->s2kdata.length;
    code = ENOMEM;
    if (!(key->contents = malloc(key->length))) goto Done;
    if (!(work = malloc(l))) goto Done;
    temp_key->contents = work+l-key->length;
    p = work;
    memcpy(p, string->data, string->length);
    p += string->length;
    if (salt) {
	memcpy(work+string->length, salt->s2kdata.data, salt->s2kdata.length);
	p += salt->s2kdata.length;
    }
    krb5i_nfold(work, p-work, p, data->length);
    data->data = p;
    temp_key->enctype = key->enctype = et->t;
    if ((code = et->e->make_key(data, temp_key, 0))) goto Done;
    data->length = 8;
    data->data = kerberos;
    if ((code = krb5i_derive_key(et->e, temp_key, key, data))) goto Done;
Done:
    if (work) {
	memset(work, 0, l);
	free(work);
    }
    if (code) {
	krb5_free_keyblock_contents(context, key);
	memset(key, 0, sizeof *key);
    }
    return code;
}
#endif

#if KRB5_RC4_SUPPORT
int
krb5i_arcfour_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    struct krb5_hash_provider *hash = krb5i_hash_md4;
    unsigned char *work = 0, *p;
    int i, l;
    int code;
    unsigned int keybytes, keylength;
    void *state;

    if (salt && salt->s2kparams.length) return KRB5_ERR_BAD_S2K_PARAMS;
    key->enctype = et->t;
    et->e->key_size(&keybytes, &keylength);
    if (!(key->contents = malloc(keylength))) {
	return ENOMEM;
    }
    key->length = keylength;
    l = string->length << 1;
    if (!(work = malloc(l))) goto Done;
    p = work;
    for (i = 0; i < string->length; ++i) {
	*p++ = i[(unsigned char*)string->data];
	*p++ = 0;
    }
    if ((code = hash->hash_init(&state))) goto Done;
    if ((code = hash->hash_update(state, work, l))) goto Done;
    if ((code = hash->hash_digest(state, key->contents))) goto Done;
Done:
    if (state) hash->free_state(state);
    if (work) {
	memset(work, 0, l);
	free(work);
    }
    return code;
}
#endif

#ifdef KRB5_EXTENDED_SALTS
/*
 * Convert an "ascii 64" string (as returned by crypt)
 * into an 8 byte (64 bit) quantity.  (Uses same character
 * map as a64l(3)--[./0-9A-Za-z].)
 *
 * keylen=8 is 64 bits; other keylens also work.
 *
 * Bit/byte order is intended to be compatible with des/crypt
 * (basically, big-endian), md5crypt uses something different.
 */

static void
a64_to_bin(char *hw, unsigned char *obuf, int keylen)
{
    int c;
    int i, j;

    memset(obuf, 0, keylen);
    j = 0;
    for (i = 0; hw[i] && j < keylen; ++i) {
	c = (unsigned char) hw[i];
	if (c>'Z') c -= 6;
	if (c>'9') c -= 7;
	c -= '.';
	switch(i&3) {
	case 0: obuf[j] = c<<2; break;
	case 1: obuf[j++] |= (c>>4);
		if (j >= keylen) break;
		obuf[j] = (c<<4); break;
	case 2:	obuf[j++] |= (c>>2);
		if (j >= keylen) break;
		obuf[j] = (c<<6); break;
	case 3: obuf[j++] |= c;
	}
    }
}

int
krb5i_grex_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    unsigned char pad[128];
    struct krb5_hash_provider *hash = krb5i_hash_sha1;
    int i, l;
    int code;
    void *state;

    key->enctype = et->t;
    if (!(key->contents = malloc(20))) {
	return ENOMEM;
    }
    key->length = 20;
    memset(pad, 0x5c, 128);
    l = string->length + 1;
    if ((code = hash->hash_init(&state))) goto Done;
    if ((code = hash->hash_update(state, pad, 128-(l&63)))) goto Done;
    if ((code = hash->hash_update(state, string->data, string->length))) goto Done;
    if ((code = hash->hash_update(state, "", 1))) goto Done;
    if (salt) {
	if ((code = hash->hash_update(state, salt->s2kdata.data, salt->s2kdata.length))) goto Done;
	if ((code = hash->hash_update(state, "", 1))) goto Done;
    }
    if ((code = hash->hash_digest(state, key->contents))) goto Done;
    if (state) hash->free_state(state);
    state = 0;
    memset(pad, 0x36, 128);
    if ((code = hash->hash_init(&state))) goto Done;
    if ((code = hash->hash_update(state, string->data, string->length))) goto Done;
    if ((code = hash->hash_update(state, "", 1))) goto Done;
    if ((code = hash->hash_update(state, pad, 128-(l&63)))) goto Done;
    if ((code = hash->hash_update(state, key->contents, 20))) goto Done;
    if ((code = hash->hash_digest(state, key->contents))) goto Done;
Done:
    if (state) hash->free_state(state);
    return code;
}

/* should be same results (except for bit scrambling) as phk's md5crypt */
int
krb5i_crypt_md5(const char *pw,
    const krb5_data *salt,
    krb5_keyblock *key)
{
    const static char magic[] = "$1$";
    const unsigned char *sp, *ep;
    unsigned char k[16];
    int sl, pl, i, l;
    struct krb5_hash_provider *hash = krb5i_hash_md5;
    void *state1 = 0, *state2 = 0;
    int code;

    if ((code = hash->hash_init(&state1))) goto Done;
    if ((code = hash->hash_init(&state2))) goto Done;
    sp = (const unsigned char *)salt->data;
    ep = sp + salt->length;
    if (!strncmp((const char *)sp, magic, sizeof magic-1))
	sp += sizeof magic-1;
    sl = ep - sp;
    pl = strlen(pw);
    if ((code = hash->hash_update(state1, pw, pl))) goto Done;
    if ((code = hash->hash_update(state1, magic, sizeof magic-1))) goto Done;
    if ((code = hash->hash_update(state1, sp, sl))) goto Done;
    if ((code = hash->hash_update(state2, pw, pl))) goto Done;
    if ((code = hash->hash_update(state2, sp, sl))) goto Done;
    if ((code = hash->hash_update(state2, pw, pl))) goto Done;
    if ((code = hash->hash_digest(state2, k))) goto Done;
    for (i = pl; i > 0; i -= 16)
	if ((code = hash->hash_update(state1, k, i > 16 ? 16 : i)))
	    goto Done;
    for (i = pl; i > 0; i >>= 1)
	    if ((code = hash->hash_update(state1, i&1 ? magic+3 : pw, 1)))
		goto Done;
    if ((code = hash->hash_digest(state1, k))) goto Done;
    hash->free_state(state1);
    state1 = 0;
    for(i=0; i<1000; ++i) {
	if ((code = hash->hash_init(&state1))) goto Done;
	code = (i&1) ?
	    hash->hash_update(state1, pw, pl)
	:
	    hash->hash_update(state1, k, 16);
	if (code) goto Done;
	if ((i % 3) && ((code = hash->hash_update(state1, sp, sl))))
	    goto Done;
	if ((i % 7) && ((code = hash->hash_update(state1, pw, pl))))
	    goto Done;
	code = (i&1) ?
	    hash->hash_update(state1, k, 16)
	:
	    hash->hash_update(state1, pw, pl);
	if (code) goto Done;
	if ((code = hash->hash_digest(state1, k))) goto Done;
	hash->free_state(state1);
	state1 = 0;
    }
    l = key->length;
    if (l > 16) l = 16;
    memcpy(key->contents, k, l);
Done:
    memset(k, 0, 16);
    if (state1) hash->free_state(state1);
    if (state2) hash->free_state(state2);
    return code;
}

int
krb5i_crypt_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    char *pw, *cr, *st;
    int keylen = 16;
    unsigned char *keydata;
    int code;
    int (*f)();
    char iobuf[32];

    f = 0;
    st = (char*) salt->s2kdata.data;
    if (salt->s2kdata.length) switch(*st)
    {
    case '$':
	switch (st[1])
	{
	case '1':
		f = krb5i_crypt_md5;
		break;
#if 0
	case '2':
		keylen = 24;
		break;
#endif
	default:
#if 0
	fprintf (stderr,"krb5_crypt_string_to_key: bad salt type <%c>n", *st);
#endif
		return KRB5_ERR_BAD_S2K_PARAMS;	/* XXX */
	}
	if (f || keylen == 24) break;
	/* else fall through */
    default:
	if (salt->s2kdata.length == 3 && string->length > 8) {
	    keylen = 16;
	} else {
	    keylen = 8;
	}
	break;
    }

    memset(key, 0, sizeof *key);
    key->enctype = et->t;
    key->length = keylen;
    if (!(keydata = (unsigned char *)malloc(key->length))) {
	return ENOMEM;
    }
    key->contents = keydata;

    if (string->length && !(pw = (char*) string->data)[string->length-1])
	;
    else if (!(pw = malloc(string->length+1))) {
	return ENOMEM;
    } else {
	memcpy(pw, string->data, string->length);
	pw[string->length] = 0;
    }

    code = 0;
    if (f)	/* md5 */
	code = (*f)(pw, &salt->s2kdata, key);
    else if (*st == '_') {	/* 386bsd newsalt */
	cr = krb5i_des_unix_crypt(pw, salt->s2kdata.data, iobuf);
	a64_to_bin(cr+9, key->contents, 8);
    }
#if 0
    else if (key->length == 24) {	/* blowfish */
	cr = bcrypt(pw, salt->s2kdata.data);
	if (strlen(cr) == 60)
	    a64_to_bin(cr+29, key->contents, 24);
	else
	    code = EDOM;	/* XXX */
    }
#endif
    else {	/* old, (doublesize) */
	cr = krb5i_des_unix_crypt(pw, salt->s2kdata.data, iobuf);
	a64_to_bin(cr+2, key->contents, 8);
	if (key->length == 16) {	/* (doublesize) */
	    cr = krb5i_des_unix_crypt(pw+8, salt->s2kdata.data, iobuf);
	    a64_to_bin(cr+2, key->contents+8, 8);
	}
    }
    if (pw != (char*) string->data) {
	memset(pw, 0, string->length);
	free(pw);
    }
    return code;
}

int
krb5i_cast_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    if (!salt)
	;
    else if (salt->s2kparams.length != 1) {
	if (salt->s2kparams.length)
	    return KRB5_ERR_BAD_S2K_PARAMS;
    }
    else switch(0[(char*)salt->s2kparams.data]) {
    case 0:	/* "KRB5_KDB_SALTTYPE_NORMAL" */
    case -3:	/* "KRB5_KDB_SALTTYPE_GREX" */
	break;
    case -2:	/* "KRB5_KDB_SALTTYPE_CRYPT" */
	return krb5i_crypt_s2k(context, et, string, salt, key);
    default:
	return KRB5_ERR_BAD_S2K_PARAMS;
    }
    return krb5i_grex_s2k(context, et, string, salt, key);
}
#endif

#if KRB5_AES_SUPPORT
int
krb5i_aes_s2k (krb5_context context,
    struct s2k_element *et,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    int code;
    unsigned int keybytes, keylength;
    krb5_keyblock temp_key[1];
    krb5_data indata[1], rawkey[1];
    int temp;
    int iter = 4096;

    if (salt) switch(salt->s2kparams.length) {
    default:
	return KRB5_ERR_BAD_S2K_PARAMS;
    case 0: break;
    case 4:
	memcpy(&temp, salt->s2kparams.data, 4);
	iter = ntohl(temp);
    }
    memset(key, 0, sizeof *key);
    memset(rawkey, 0, sizeof *rawkey);
    memset(temp_key, 0, sizeof *temp_key);
    et->e->key_size(&keybytes, &keylength);
    rawkey->length = keybytes;
    key->length = temp_key->length = keylength;

    code = ENOMEM;
    if (!(temp_key->contents = malloc(temp_key->length))) goto Done;
    if (!(key->contents = malloc(key->length))) goto Done;
    if (!(rawkey->data = malloc(keybytes))) goto Done;

    code = krb5i_pkcs5_pbkdf2(string, &salt->s2kdata, iter, rawkey, et->h);
    if (code) goto Done;
    temp_key->enctype = key->enctype = et->t;
    et->e->make_key(rawkey, temp_key, 0);
    indata->length = 8;
    indata->data = kerberos;
    if ((code = krb5i_derive_key(et->e, temp_key, key, indata))) goto Done;
Done:
    krb5_free_keyblock_contents(context, temp_key);
    if (rawkey->data) {
	memset(rawkey->data, 0, keybytes);
	free(rawkey->data);
    }
    if (code) {
	krb5_free_keyblock_contents(context, key);
	memset(key, 0, sizeof *key);
    }
    return code;
}
#endif

struct s2k_element krb5i_s2k_types[] = {
#if KRB5_DES_SUPPORT
    {ENCTYPE_DES_CBC_CRC,krb5i_des_cbc,krb5i_keyhash_descbc,krb5i_des_s2k},
    {ENCTYPE_DES_CBC_MD4,krb5i_des_cbc,krb5i_keyhash_descbc,krb5i_des_s2k},
    {ENCTYPE_DES_CBC_MD5,krb5i_des_cbc,krb5i_keyhash_descbc,krb5i_des_s2k},
    {ENCTYPE_DES_HMAC_SHA1,krb5i_des_cbc,0,krb5i_dk_s2k},
#endif	/* KRB5_DES_SUPPORT */
#if KRB5_DES3_SUPPORT
    {ENCTYPE_DES3_CBC_SHA1,krb5i_des3_cbc,0,krb5i_dk_s2k},
#endif	/* KRB5_DES3_SUPPORT */
#if KRB5_RC4_SUPPORT
    {ENCTYPE_ARCFOUR_HMAC,krb5i_arcfour,0,krb5i_arcfour_s2k},
    {ENCTYPE_ARCFOUR_HMAC_EXP,krb5i_arcfour,0,krb5i_arcfour_s2k},
#endif	/* KRB5_RC4_SUPPORT */
#if KRB5_AES_SUPPORT
    {ENCTYPE_AES128_CTS_HMAC_SHA1_96,krb5i_aes128_cts,krb5i_hash_sha1,krb5i_aes_s2k},
    {ENCTYPE_AES256_CTS_HMAC_SHA1_96,krb5i_aes256_cts,krb5i_hash_sha1,krb5i_aes_s2k},
#endif	/* KRB5_AES_SUPPORT */

    /* old mdw experimental */
#if KRB5_CAST_SUPPORT
    {ENCTYPE_CAST_CBC_SHA,krb5i_cast_cbc,0,krb5i_cast_s2k},
#endif	/* KRB5_CAST_SUPPORT */
#if KRB5_RC6_SUPPORT
    {ENCTYPE_RC6_CBC_SHA,krb5i_rc6_cts,0,krb5i_cast_s2k},
#endif	/* KRB5_RC6_SUPPORT */
{0,0,0}};

krb5_error_code
krb5_c_string_to_key(krb5_context context,
    krb5_enctype enctype,
    const krb5_data *string,
    const krb5_salt *salt,
    krb5_keyblock *key)
{
    int i;
    for (i = 0; ; ++i) {
	if (!krb5i_s2k_types[i].f) return KRB5_BAD_ENCTYPE;
	if (krb5i_s2k_types[i].t == enctype) break;
    }
    if (salt->s2kdata.length == ~0) salt = 0;
    return (*krb5i_s2k_types[i].f)(context,
	krb5i_s2k_types+i, string, salt, key);
}
