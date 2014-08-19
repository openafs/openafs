/*
 * Copyright (c) 2013 Chaskiel Grundman <cg2v@andrew.cmu.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>
#include <rx/rx.h>
#include "rxkad.h"

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>

#ifdef  RX_ENABLE_LOCKS
static afs_kmutex_t krb5_lock;
#endif

/* these globals are expected to be set only once, so locking is not needed */
static char *checkfile_path;
static char *keytab_name;
static int have_keytab_keys;

/*
 * krb5_lock must be held to use/set these globals, including any
 * krb5 api use with k5ctx
 */
static krb5_context k5ctx;
static int nkeys;
static krb5_keytab_entry *ktent;
static time_t last_reload;

#ifdef HAVE_KRB5_KEYBLOCK_ENCTYPE
# define kb_enctype(keyblock)     ((keyblock)->enctype)
#elif defined(HAVE_KRB5_KEYBLOCK_KEYTYPE)
# define kb_enctype(keyblock)     ((keyblock)->keytype)
#else
# error Cannot figure out how keyblocks work
#endif
#ifdef HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK
# define kte_keyblock(kte) (&(kte).keyblock)
#elif defined(HAVE_KRB5_KEYTAB_ENTRY_KEY)
# define kte_keyblock(kte) (&(kte).key)
#else
# error  Cannot figure out how keytab entries work
#endif
#if HAVE_DECL_KRB5_FREE_KEYTAB_ENTRY_CONTENTS
/* nothing */
#elif HAVE_DECL_KRB5_KT_FREE_ENTRY
# define krb5_free_keytab_entry_contents krb5_kt_free_entry
#else
static_inline int
krb5_free_keytab_entry_contents(krb5_context ctx, krb5_keytab_entry *ent)
{
    krb5_free_principal(ctx, ent->principal);
    krb5_free_keyblock_contents(ctx, kte_keyblock(ent));
    return 0;
}
#endif
#ifndef KRB5_KEYUSAGE_KDC_REP_TICKET
# ifdef HAVE_DECL_KRB5_KU_TICKET
#  define KRB5_KEYUSAGE_KDC_REP_TICKET KRB5_KU_TICKET
# else
#  define KRB5_KEYUSAGE_KDC_REP_TICKET 2
# endif
#endif

static krb5_error_code
reload_keys(void)
{
    krb5_error_code ret;
    krb5_keytab fkeytab = NULL;
    krb5_kt_cursor c;
    krb5_keytab_entry kte;
    int i, n_nkeys, o_nkeys;
    krb5_keytab_entry *n_ktent = NULL, *o_ktent;
    struct stat tstat;

    if (stat(checkfile_path, &tstat) == 0) {
	if (have_keytab_keys && tstat.st_mtime == last_reload) {
	    /* We haven't changed since the last time we loaded our keys, so
	     * there's nothing to do. */
	    ret = 0;
	    goto cleanup;
	}
	last_reload = tstat.st_mtime;
    } else if (have_keytab_keys) {
	/* stat() failed, but we already have keys, so don't do anything. */
	ret = 0;
	goto cleanup;
    }

    if (keytab_name != NULL)
	ret = krb5_kt_resolve(k5ctx, keytab_name, &fkeytab);
    else
	ret = krb5_kt_default(k5ctx, &fkeytab);
    if (ret != 0)
	goto cleanup;
    ret = krb5_kt_start_seq_get(k5ctx, fkeytab, &c);
    if (ret != 0)
	goto cleanup;
    n_nkeys = 0;
    while (krb5_kt_next_entry(k5ctx, fkeytab, &kte, &c) == 0) {
	krb5_free_keytab_entry_contents(k5ctx, &kte);
	n_nkeys++;
    }
    krb5_kt_end_seq_get(k5ctx, fkeytab, &c);
    if (n_nkeys == 0) {
	ret = KRB5_KT_NOTFOUND;
	goto cleanup;
    }
    n_ktent = calloc(n_nkeys, sizeof(krb5_keytab_entry));
    if (n_ktent == NULL) {
	ret = KRB5_KT_NOTFOUND;
	goto cleanup;
    }
    ret = krb5_kt_start_seq_get(k5ctx, fkeytab, &c);
    if (ret != 0)
	goto cleanup;
    for (i = 0; i < n_nkeys; i++)
	if (krb5_kt_next_entry(k5ctx, fkeytab, &n_ktent[i], &c) != 0)
	    break;
    krb5_kt_end_seq_get(k5ctx, fkeytab, &c);
    if (i < n_nkeys)
	goto cleanup;
    have_keytab_keys = 1;
    o_ktent = ktent;
    ktent = n_ktent;

    o_nkeys = nkeys;
    nkeys = n_nkeys;

    /* for cleanup */
    n_ktent = o_ktent;
    n_nkeys = o_nkeys;
cleanup:
    if (n_ktent != NULL) {
	for (i = 0; i < n_nkeys; i++)
	    krb5_free_keytab_entry_contents(k5ctx, &n_ktent[i]);
	free(n_ktent);
    }
    if (fkeytab != NULL) {
	krb5_kt_close(k5ctx, fkeytab);
    }
    return ret;
}

#if defined(HAVE_KRB5_DECRYPT_TKT_PART) && !defined(HAVE_KRB5_C_DECRYPT)
extern krb5_error_code
encode_krb5_enc_tkt_part(krb5_enc_tkt_part *encpart, krb5_data **a_out);
/*
 * AIX krb5 has krb5_decrypt_tkt_part, but no krb5_c_decrypt. So, implement our
 * own krb5_c_decrypt. Note that this krb5_c_decrypt is only suitable for
 * decrypting an encrypted krb5_enc_tkt_part. But since that's all we ever use
 * it for, that should be fine.
 */
static krb5_error_code
krb5_c_decrypt(krb5_context context, const krb5_keyblock *key,
               krb5_keyusage usage, const krb5_data *cipher_state,
               const krb5_enc_data *input, krb5_data *output)
{
    krb5_ticket tkt;
    krb5_error_code code;
    krb5_data *tout = NULL;

    /* We only handle a subset of possible arguments; if we somehow get passed
     * something else, bail out with EINVAL. */
    if (cipher_state != NULL)
	return EINVAL;
    if (usage != KRB5_KEYUSAGE_KDC_REP_TICKET)
	return EINVAL;

    memset(&tkt, 0, sizeof(tkt));

    tkt.enc_part = *input;

    code = krb5_decrypt_tkt_part(context, key, &tkt);
    if (code != 0)
	return code;

    code = encode_krb5_enc_tkt_part(tkt.enc_part2, &tout);
    if (code != 0)
	return code;

    if (tout->length > output->length) {
	/* This should never happen, but don't assert since we may be dealing
	 * with untrusted user data. */
	code = EINVAL;
	goto error;
    }

    memcpy(output->data, tout->data, tout->length);
    output->length = tout->length;

 error:
    if (tout)
	krb5_free_data(context, tout);
    return code;
}
#endif /* HAVE_KRB5_DECRYPT_TKT_PART && !HAVE_KRB5_C_DECRYPT */

static int
rxkad_keytab_decrypt(int kvno, int et, void *in, size_t inlen,
		     void *out, size_t *outlen)
{
    krb5_error_code code;
    /* use heimdal api if available, since heimdal's interface to
       krb5_c_decrypt is non-standard and annoying to use */
#ifdef HAVE_KRB5_CRYPTO_INIT
    krb5_crypto kcrypto;
#else
    krb5_enc_data ind;
#endif
    krb5_data outd;
    int i, foundkey;
    MUTEX_ENTER(&krb5_lock);
    reload_keys();
    if (have_keytab_keys == 0) {
	MUTEX_EXIT(&krb5_lock);
	return RXKADUNKNOWNKEY;
    }
    foundkey = 0;
    code = -1;
    for (i = 0; i < nkeys; i++) {
	/* foundkey determines what error code we return for failure */
	if (ktent[i].vno == kvno)
	    foundkey = 1;
	/* but check against all keys if the enctype matches, for robustness */
	if (kb_enctype(kte_keyblock(ktent[i])) == et) {
#ifdef HAVE_KRB5_CRYPTO_INIT
	    code = krb5_crypto_init(k5ctx, kte_keyblock(ktent[i]), et,
				    &kcrypto);
	    if (code == 0) {
		code = krb5_decrypt(k5ctx, kcrypto,
				    KRB5_KEYUSAGE_KDC_REP_TICKET, in, inlen,
				    &outd);
		krb5_crypto_destroy(k5ctx, kcrypto);
	    }
	    if (code == 0) {
		if (outd.length > *outlen) {
		    /* This should never happen, but don't assert since we may
		     * be dealing with untrusted user data. */
		    code = EINVAL;
		    krb5_data_free(&outd);
		    outd.data = NULL;
		}
	    }
	    if (code == 0) {
		/* heimdal allocates new memory for the decrypted data; put
		 * the data back into the requested 'out' buffer */
		*outlen = outd.length;
		memcpy(out, outd.data, outd.length);
		krb5_data_free(&outd);
		break;
	    }
#else
	    outd.length = *outlen;
	    outd.data = out;
	    ind.ciphertext.length = inlen;
	    ind.ciphertext.data = in;
	    ind.enctype = et;
	    ind.kvno = kvno;
	    code = krb5_c_decrypt(k5ctx, kte_keyblock(ktent[i]),
				  KRB5_KEYUSAGE_KDC_REP_TICKET, NULL, &ind,
				  &outd);
	    if (code == 0) {
		*outlen = outd.length;
		break;
	    }
#endif
	}
    }
    MUTEX_EXIT(&krb5_lock);
    if (code == 0)
	return 0;
    if (foundkey != 0)
	return RXKADBADTICKET;
    return RXKADUNKNOWNKEY;
}

#ifdef RX_ENABLE_LOCKS
static void
init_krb5_lock(void)
{
    MUTEX_INIT(&krb5_lock, "krb5 api", MUTEX_DEFAULT, 0);
}

static pthread_once_t rxkad_keytab_once_init = PTHREAD_ONCE_INIT;
#define INIT_PTHREAD_LOCKS osi_Assert(pthread_once(&rxkad_keytab_once_init, init_krb5_lock)==0)
#else
#define INIT_PTHREAD_LOCKS
#endif
int
rxkad_InitKeytabDecrypt(const char *csdb, const char *ktname)
{
    int code;
    static int keytab_init;
    INIT_PTHREAD_LOCKS;
    MUTEX_ENTER(&krb5_lock);
    if (keytab_init) {
	MUTEX_EXIT(&krb5_lock);
	return 0;
    }
    checkfile_path = strdup(csdb);
    if (checkfile_path == NULL) {
	code = ENOMEM;
	goto cleanup;
    }
    k5ctx = NULL;
    keytab_name = NULL;
    code = krb5_init_context(&k5ctx);
    if (code != 0)
	goto cleanup;
    if (ktname != NULL) {
	keytab_name = strdup(ktname);
	if (keytab_name == NULL) {
	    code = KRB5_KT_BADNAME;
	    goto cleanup;
	}
    }
    keytab_init=1;
    reload_keys();
    MUTEX_EXIT(&krb5_lock);
    return 0;
cleanup:
    if (checkfile_path != NULL) {
	free(checkfile_path);
    }
    if (keytab_name != NULL) {
	free(keytab_name);
    }
    if (k5ctx != NULL) {
	krb5_free_context(k5ctx);
    }
    MUTEX_EXIT(&krb5_lock);
    return code;
}

int
rxkad_BindKeytabDecrypt(struct rx_securityClass *aclass)
{
    return rxkad_SetAltDecryptProc(aclass, rxkad_keytab_decrypt);
}
