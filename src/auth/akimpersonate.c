/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */
/*
 * Copyright (C) 2013 by Alexander Chernyakhovsky and the
 * Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>

#include <string.h>
#include <sys/stat.h>

#include "akimpersonate.h"
#include "akimpersonate_v5gen.h"

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

#if USING_HEIMDAL
#define deref_keyblock_enctype(kb)	((kb)->keytype)
#define deref_entry_keyblock(entry)	((entry)->keyblock)
#define deref_session_key(creds)	((creds)->session)
#define deref_enc_tkt_addrs(tkt)	((tkt)->caddr)
#define deref_enc_data(enc)		((enc)->cipher.data)
#else
#define deref_keyblock_enctype(kb)	((kb)->enctype)
#define deref_entry_keyblock(entry)	((entry)->key)
#define deref_session_key(creds)	((creds)->keyblock)
#define deref_enc_tkt_addrs(tkt)	((tkt)->caddrs)
#define deref_enc_data(enc)		((enc)->ciphertext.data)
#endif
#if HAVE_DECL_KRB5_FREE_KEYTAB_ENTRY_CONTENTS
/* nothing */
#elif HAVE_DECL_KRB5_KT_FREE_ENTRY
#define krb5_free_keytab_entry_contents krb5_kt_free_entry
#else
static inline int
krb5_free_keytab_entry_contents(krb5_context ctx, krb5_keytab_entry * ent)
{
    krb5_free_principal(ctx, ent->principal);
    krb5_free_keyblock_contents(ctx, kte_keyblock(ent));
    return 0;
}
#endif

#define deref_entry_enctype(entry)			\
    deref_keyblock_enctype(&deref_entry_keyblock(entry))

#ifdef USING_MIT
# if !defined(HAVE_ENCODE_KRB5_TICKET)
/*
 * Solaris doesn't have encode_krb5_ticket and encode_krb5_enc_tkt_part, so we
 * need to implement our own. The akv5gen_* functions below are implemented
 * using v5gen code; so, they need to have no krb5 structures in their
 * arguments, since using system krb5 headers at the same time as v5gen
 * headers is problematic. That's why the ticket contents are exploded.
 */
static krb5_error_code
encode_krb5_ticket(krb5_ticket *rep, krb5_data **a_out)
{
    krb5_error_code code = 0;
    int i;
    char **names = NULL;
    krb5_data *out = NULL;
    size_t out_len = 0;
    char *out_data = NULL;

    *a_out = NULL;

    out = calloc(1, sizeof(*out));
    if (!out) {
	code = ENOMEM;
	goto cleanup;
    }

    names = calloc(rep->server->length, sizeof(names[0]));
    if (names == NULL) {
	code = ENOMEM;
	goto cleanup;
    }

    for (i = 0; i < rep->server->length; i++) {
	names[i] = rep->server->data[i].data;
    }

    code = akv5gen_encode_krb5_ticket(rep->enc_part.kvno,
                                      rep->server->realm.data,
                                      rep->server->type,
                                      rep->server->length,
                                      names,
                                      rep->enc_part.enctype,
                                      rep->enc_part.ciphertext.length,
                                      rep->enc_part.ciphertext.data,
                                      &out_len,
                                      &out_data);
    if (code != 0) {
	goto cleanup;
    }

    out->length = out_len;
    out->data = out_data;
    *a_out = out;
    out = NULL;

 cleanup:
    free(names);
    free(out);
    return code;
}
# else
extern krb5_error_code encode_krb5_ticket(krb5_ticket *rep,
					  krb5_data **a_out);
# endif /* !HAVE_ENCODE_KRB5_TICKET */

# if !defined(HAVE_ENCODE_KRB5_ENC_TKT_PART)
static krb5_error_code
encode_krb5_enc_tkt_part(krb5_enc_tkt_part *encpart, krb5_data **a_out)
{
    krb5_error_code code = 0;
    int i;
    char **names = NULL;
    krb5_data *out = NULL;
    size_t out_len = 0;
    char *out_data = NULL;

    *a_out = NULL;

    out = calloc(1, sizeof(*out));
    if (out == NULL) {
	code = ENOMEM;
	goto cleanup;
    }

    names = calloc(encpart->client->length, sizeof(names[0]));
    if (names == NULL) {
	code = ENOMEM;
	goto cleanup;
    }

    for (i = 0; i < encpart->client->length; i++)
	names[i] = encpart->client->data[i].data;

    if (encpart->flags != TKT_FLG_INITIAL) {
	/* We assume the ticket has the flag _INITIAL set, and only that flag.
	 * passing each individual flag to akv5gen would be really ugly, and
	 * should be unnecessary. */
	goto invalid;
    }
    if (encpart->caddrs != NULL && encpart->caddrs[0] != NULL)
	goto invalid;
    if (encpart->authorization_data && encpart->authorization_data[0])
	goto invalid;

    code = akv5gen_encode_krb5_enc_tkt_part(encpart->session->enctype,
                                            encpart->session->length,
                                            encpart->session->contents,
                                            encpart->client->realm.data,
                                            encpart->client->type,
                                            encpart->client->length,
                                            names,
                                            encpart->transited.tr_type,
                                            encpart->transited.tr_contents.length,
                                            encpart->transited.tr_contents.data,
                                            encpart->times.authtime,
                                            encpart->times.starttime,
                                            encpart->times.endtime,
                                            encpart->times.renew_till,
                                            &out_len,
                                            &out_data);
    if (code != 0)
	goto cleanup;

    out->length = out_len;
    out->data = out_data;
    *a_out = out;
    out = NULL;

 cleanup:
    free(names);
    free(out);
    return code;

 invalid:
    /* We don't handle all possible ticket options, features, etc. If we are
     * given a ticket we can't handle, bail out with EINVAL. */
    code = EINVAL;
    goto cleanup;
}
# endif /* !HAVE_ENCODE_KRB5_ENC_TKT_PART */

# if !defined(HAVE_KRB5_ENCRYPT_TKT_PART)
krb5_error_code
krb5_encrypt_tkt_part(krb5_context context,
		      const krb5_keyblock *key,
		      krb5_ticket *ticket)
{
    krb5_data *data = 0;
    int code;
    size_t enclen;

    if ((code = encode_krb5_enc_tkt_part(ticket->enc_part2, &data)))
	goto Done;
    if ((code = krb5_c_encrypt_length(context, key->enctype,
				      data->length, &enclen)))
	goto Done;
    ticket->enc_part.ciphertext.length = enclen;
    if (!(ticket->enc_part.ciphertext.data = malloc(enclen))) {
	code = ENOMEM;
	goto Done;
    }
    if ((code = krb5_c_encrypt(context, key, KRB5_KEYUSAGE_KDC_REP_TICKET,
			       0, data, &ticket->enc_part))) {
	free(ticket->enc_part.ciphertext.data);
	ticket->enc_part.ciphertext.data = 0;
    }
Done:
    if (data) {
	if (data->data)
	    free(data->data);
	free(data);
    }
    return code;
}
# else
extern krb5_error_code krb5_encrypt_tkt_part(krb5_context context,
					     const krb5_keyblock *key,
					     krb5_ticket *ticket);
# endif /* HAVE_KRB5_ENCRYPT_TKT_PART */
#endif /* USING_MIT */

static const int any_enctype[2] = {0, 0};
static const krb5_data empty_string;

/*
 * Routines to allocate/free the extra storage involved in a ticket structure.
 * When changing one, ensure that the other is changed to reflect the
 * allocation contract.
 */
#if USING_HEIMDAL
static int
alloc_ticket(Ticket **out)
{
    *out = calloc(1, sizeof(Ticket));
    if (*out == NULL)
	return ENOMEM;

    (*out)->enc_part.kvno = malloc(sizeof(*(*out)->enc_part.kvno));
    if ((*out)->enc_part.kvno == NULL)
	return ENOMEM;

    return 0;
}
#else
static int
alloc_ticket(krb5_ticket **out)
{
    *out = calloc(1, sizeof(krb5_ticket));
    if (*out == NULL)
	return ENOMEM;

    return 0;
}
#endif

static void
free_ticket(void *in)
{
#if USING_HEIMDAL
    Ticket *ticket_reply;
#else
    krb5_ticket *ticket_reply;
#endif

    /* requisite aliasing for MIT/Heimdal support. */
    ticket_reply = in;
    if (ticket_reply == NULL)
	return;

#if USING_HEIMDAL
    if (ticket_reply->enc_part.kvno != NULL)
	free(ticket_reply->enc_part.kvno);
#else
    /* No allocations needed for MIT's krb5_ticket structure. */
#endif
    free(ticket_reply);
}

/*
 * Routines to allocate/free the extra storage involved in an encrypted
 * ticket part structure.
 * When changing one, ensure that the other is changed to reflect the
 * allocation contract.
 */
#if USING_HEIMDAL
static int
alloc_enc_tkt_part(EncTicketPart **out)
{
    *out = calloc(1, sizeof(EncTicketPart));
    if (*out == NULL)
	return ENOMEM;

    (*out)->starttime = malloc(sizeof(*(*out)->starttime));
    if ((*out)->starttime == NULL)
	return ENOMEM;
    return 0;
}
#else
static int
alloc_enc_tkt_part(krb5_enc_tkt_part **out)
{
    *out = calloc(1, sizeof(krb5_enc_tkt_part));
    if (*out == NULL)
	return ENOMEM;

    return 0;
}
#endif

static void
free_enc_tkt_part(void *in)
{
#if USING_HEIMDAL
    EncTicketPart *enc_tkt_reply;
#else
    krb5_enc_tkt_part *enc_tkt_reply;
#endif

    /* Aliasing for MIT/Heimdal support. */
    enc_tkt_reply = in;
    if (enc_tkt_reply == NULL)
	return;

#if USING_HEIMDAL
    if (enc_tkt_reply->starttime != NULL)
	free(enc_tkt_reply->starttime);
#else
    /* No allocations needed for MIT's krb5_enc_tkt_part structure. */
#endif
    free(enc_tkt_reply);
}

/*
 * Given a keytab, extract the principal name of the (first) entry with
 * the highest kvno in the keytab.  This provides compatibility with the
 * rxkad KeyFile behavior of always using the highest kvno entry when
 * printing tickets.  We could return the kvno as well, but krb5_kt_get_entry
 * can find the highest kvno on its own.
 *
 * Returns 0 on success, krb5 errors on failure.
 */
static int
pick_principal(krb5_context context, krb5_keytab kt,
	       krb5_principal *service_principal)
{
    krb5_error_code code;
    krb5_kvno vno = 0;
    krb5_kt_cursor c;
    krb5_keytab_entry n_entry;

    /* Nothing to do */
    if (*service_principal != NULL)
	return 0;

    memset(&n_entry, 0, sizeof(n_entry));

    code = krb5_kt_start_seq_get(context, kt, &c);
    if (code != 0)
	goto cleanup;
    while (code == 0 && krb5_kt_next_entry(context, kt, &n_entry, &c) == 0) {
	if (n_entry.vno > vno) {
	    vno = n_entry.vno;
	    (void)krb5_free_principal(context, *service_principal);
	    code = krb5_copy_principal(context, n_entry.principal,
				       service_principal);
	}
	(void)krb5_free_keytab_entry_contents(context, &n_entry);
    }
    if (code != 0) {
	(void)krb5_kt_end_seq_get(context, kt, &c);
	goto cleanup;
    }
    code = krb5_kt_end_seq_get(context, kt, &c);

cleanup:
    return code;
}

/*
 * Given a keytab and a list of allowed enctypes, and optionally a known
 * service principal, choose an appropriate enctype, and choose a
 * service principal if one was not given.  Return the keytab entry
 * corresponding to this service principal and enctype.
 *
 * The list of allowed enctypes must be zero-terminated.
 */
static int
pick_enctype_and_principal(krb5_context context, krb5_keytab kt,
			   const int *allowed_enctypes, krb5_enctype *enctype,
			   krb5_principal *service_principal,
			   krb5_keytab_entry *entry)
{
    krb5_error_code code;
    int i;

    if (*service_principal == NULL) {
	code = pick_principal(context, kt, service_principal);
	if (code != 0) {
	    goto cleanup;
	}
    }

    /* We always have a service_principal, now. */
    i = 0;
    do {
	*enctype = allowed_enctypes[i];
	code = krb5_kt_get_entry(context, kt, *service_principal, 0 /* any */,
				 *enctype, entry);
	if (code == 0) {
	    if (*enctype == 0)
		*enctype = deref_entry_enctype(entry);
	    break;
	}
	++i;
    } while(allowed_enctypes[i] != 0);
    if (code != 0)
	goto cleanup;

cleanup:
    return code;
}

/*
 * Populate the encrypted part of the ticket.
 */
static void
populate_enc_tkt(krb5_keyblock *session_key, krb5_principal client_principal,
		 time_t starttime, time_t endtime, void *out)
{
#if USING_HEIMDAL
    EncTicketPart *enc_tkt_reply;
#else
    krb5_enc_tkt_part *enc_tkt_reply;
#endif

    /* Alias through void* since Heimdal and MIT's types differ. */
    enc_tkt_reply = out;

#if USING_HEIMDAL
    enc_tkt_reply->flags.initial = 1;
    enc_tkt_reply->transited.tr_type = DOMAIN_X500_COMPRESS;
    enc_tkt_reply->cname = client_principal->name;
    enc_tkt_reply->crealm = client_principal->realm;
    enc_tkt_reply->key = *session_key;
    enc_tkt_reply->transited.contents = empty_string;
    enc_tkt_reply->authtime = starttime;
    *enc_tkt_reply->starttime = starttime;
    enc_tkt_reply->endtime = endtime;
#else
    enc_tkt_reply->magic = KV5M_ENC_TKT_PART;
    enc_tkt_reply->flags |= TKT_FLG_INITIAL;
    enc_tkt_reply->transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
    enc_tkt_reply->session = session_key;
    enc_tkt_reply->client = client_principal;
    enc_tkt_reply->transited.tr_contents = empty_string;
    enc_tkt_reply->times.authtime = starttime;
    enc_tkt_reply->times.starttime = starttime; /* krb524init needs this */
    enc_tkt_reply->times.endtime = endtime;
#endif  /* USING_HEIMDAL */
}

/*
 * Encrypt the provided enc_tkt_part structure with the key from the keytab
 * entry entry, and place the resulting blob in the ticket_reply structure.
 */
static int
encrypt_enc_tkt(krb5_context context, krb5_principal service_principal,
		krb5_keytab_entry *entry, void *tr_out, void *er_in)
{
    krb5_error_code code;
#if USING_HEIMDAL
    Ticket *ticket_reply;
    EncTicketPart *enc_tkt_reply;
    krb5_crypto crypto = 0;
    unsigned char *buf = 0;
    size_t buf_size, buf_len;
#else
    krb5_ticket *ticket_reply;
    krb5_enc_tkt_part *enc_tkt_reply;
#endif

    /* Requisite aliasing for Heimdal/MIT support. */
    ticket_reply = tr_out;
    enc_tkt_reply = er_in;

#if USING_HEIMDAL
    ticket_reply->sname = service_principal->name;
    ticket_reply->realm = service_principal->realm;

    ASN1_MALLOC_ENCODE(EncTicketPart, buf, buf_size, enc_tkt_reply,
		       &buf_len, code);
    if (code != 0)
	goto cleanup;

    if (buf_len != buf_size)
	goto cleanup;
    code = krb5_crypto_init(context,
			    &deref_entry_keyblock(entry),
			    deref_entry_enctype(entry),
			    &crypto);
    if (code != 0)
	goto cleanup;
    code = krb5_encrypt_EncryptedData(context, crypto, KRB5_KU_TICKET, buf,
				      buf_len, entry->vno,
				      &(ticket_reply->enc_part));
    if (code != 0)
	goto cleanup;
    ticket_reply->enc_part.etype = deref_entry_enctype(entry);
    *ticket_reply->enc_part.kvno = entry->vno;
    ticket_reply->tkt_vno = 5;
#else
    ticket_reply->server = service_principal;
    ticket_reply->enc_part2 = enc_tkt_reply;
    code = krb5_encrypt_tkt_part(context, &deref_entry_keyblock(entry),
				 ticket_reply);
    if (code != 0)
        goto cleanup;
    ticket_reply->enc_part.kvno = entry->vno;
#endif

cleanup:
#if USING_HEIMDAL
    if (buf != NULL)
	free(buf);
    if (crypto != NULL)
	krb5_crypto_destroy(context, crypto);
#endif
    return code;
}

/*
 * Populate the credentials structure corresponding to the ticket we are
 * printing.
 */
static int
populate_creds(krb5_context context, krb5_principal service_principal,
	       krb5_principal client_principal, krb5_keyblock *session_key,
	       void *tr_in, void *er_in, krb5_creds *creds)
{
    krb5_error_code code;
#if USING_HEIMDAL
    Ticket *ticket_reply;
    EncTicketPart *enc_tkt_reply;
    size_t dummy;
#else
    krb5_ticket *ticket_reply;
    krb5_enc_tkt_part *enc_tkt_reply;
    krb5_data *temp = NULL;
#endif

    /* Requisite aliasing for Heimdal/MIT support. */
    ticket_reply = tr_in;
    enc_tkt_reply = er_in;

    code = krb5_copy_principal(context, service_principal, &creds->server);
    if (code != 0)
        goto cleanup;
    code = krb5_copy_principal(context, client_principal, &creds->client);
    if (code != 0)
        goto cleanup;
    code = krb5_copy_keyblock_contents(context, session_key,
				       &deref_session_key(creds));
    if (code != 0)
        goto cleanup;

#if USING_HEIMDAL
    creds->times.authtime = enc_tkt_reply->authtime;
    creds->times.starttime = *(enc_tkt_reply->starttime);
    creds->times.endtime = enc_tkt_reply->endtime;
    creds->times.renew_till = 0; /* *(enc_tkt_reply->renew_till) */
    creds->flags.b = enc_tkt_reply->flags;
#else
    creds->times = enc_tkt_reply->times;
    creds->ticket_flags = enc_tkt_reply->flags;
#endif

#if USING_HEIMDAL
    ASN1_MALLOC_ENCODE(Ticket, creds->ticket.data, creds->ticket.length,
		       ticket_reply, &dummy, code);
    if (code != 0 || dummy != creds->ticket.length)
	goto cleanup;
#else
    code = encode_krb5_ticket(ticket_reply, &temp);
    if (code != 0)
	goto cleanup;
    creds->ticket = *temp;
#endif

cleanup:
#if USING_HEIMDAL
    /* nothing */
#else
    free(temp);
#endif
    return code;
}

/*
 * Print a krb5 ticket in our service key, for the supplied client principal.
 * The path to a keytab is mandatory, but the service principal may be
 * guessed from the keytab contents if desired.  The keytab entry must be
 * one of the allowed_enctypes (a zero-terminated list) if a non-NULL
 * parameter is passed.
 */
krb5_error_code
get_credv5_akimpersonate(krb5_context context, char* keytab,
			 krb5_principal service_principal,
			 krb5_principal client_principal, time_t starttime,
			 time_t endtime, const int *allowed_enctypes,
			 krb5_creds** out_creds /* out */ )
{
    char *tmpkt = NULL;
    struct stat tstat;
    char *ktname = NULL;
    krb5_error_code code;
    krb5_keytab kt = 0;
    krb5_keytab_entry entry[1];
    krb5_creds *creds = 0;
    krb5_enctype enctype;
    krb5_keyblock session_key[1];
#if USING_HEIMDAL
    Ticket *ticket_reply;
    EncTicketPart *enc_tkt_reply;
#else
    krb5_ticket *ticket_reply;
    krb5_enc_tkt_part *enc_tkt_reply;
#endif
    *out_creds = NULL;
    enctype = 0; /* AKIMPERSONATE_IGNORE_ENCTYPE */
    memset(entry, 0, sizeof *entry);
    memset(session_key, 0, sizeof *session_key);
    ticket_reply = NULL;
    enc_tkt_reply = NULL;

    creds = calloc(1, sizeof(*creds));
    if (creds == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    code = alloc_ticket(&ticket_reply);
    if (code != 0)
	goto cleanup;
    code = alloc_enc_tkt_part(&enc_tkt_reply);
    if (code != 0)
	goto cleanup;
    /* Empty list of allowed etypes must fail.  Do it here to avoid issues. */
    if (allowed_enctypes != NULL && *allowed_enctypes == 0) {
	code = KRB5_BAD_ENCTYPE;
	goto cleanup;
    }
    if (allowed_enctypes == NULL)
        allowed_enctypes = any_enctype;

    if (keytab != NULL) {
	tmpkt = strdup(keytab);
	if (!tmpkt)
	    code = ENOMEM;
    } else {
	tmpkt = malloc(256);
	if (!tmpkt)
	    code = ENOMEM;
	else
	    code = krb5_kt_default_name(context, tmpkt, 256);
    }
    if (code)
	goto cleanup;

    if (strncmp(tmpkt, "WRFILE:", 7) == 0)
	ktname = &(tmpkt[7]);
    else if (strncmp(tmpkt, "FILE:", 5) == 0)
	ktname = &(tmpkt[5]);

    if (ktname && (stat(ktname, &tstat) != 0)) {
	code = KRB5_KT_NOTFOUND;
	goto cleanup;
    }

    code = krb5_kt_resolve(context, tmpkt, &kt);
    if (code != 0)
        goto cleanup;

    code = pick_enctype_and_principal(context, kt, allowed_enctypes,
				      &enctype, &service_principal, entry);
    if (code != 0)
	goto cleanup;

    /* Conjure up a random session key */
    deref_keyblock_enctype(session_key) = enctype;
#if USING_HEIMDAL
    code = krb5_generate_random_keyblock(context, enctype, session_key);
#else
    code = krb5_c_make_random_key(context, enctype, session_key);
#endif
    if (code != 0)
        goto cleanup;

    populate_enc_tkt(session_key, client_principal, starttime, endtime,
		     enc_tkt_reply);

    code = encrypt_enc_tkt(context, service_principal, entry, ticket_reply,
			   enc_tkt_reply);
    if (code != 0)
	goto cleanup;

    code = populate_creds(context, service_principal, client_principal,
			  session_key, ticket_reply, enc_tkt_reply, creds);
    if (code != 0)
	goto cleanup;

    /* return creds */
    *out_creds = creds;
    creds = NULL;
cleanup:
    if (tmpkt)
	free(tmpkt);
    if (deref_enc_data(&ticket_reply->enc_part) != NULL)
        free(deref_enc_data(&ticket_reply->enc_part));
    krb5_free_keytab_entry_contents(context, entry);
    if (client_principal != NULL)
        krb5_free_principal(context, client_principal);
    if (service_principal != NULL)
        krb5_free_principal(context, service_principal);
    if (kt != NULL)
        krb5_kt_close(context, kt);
    if (creds != NULL)
	krb5_free_creds(context, creds);
    krb5_free_keyblock_contents(context, session_key);
    free_ticket(ticket_reply);
    free_enc_tkt_part(enc_tkt_reply);
    return code;
}
