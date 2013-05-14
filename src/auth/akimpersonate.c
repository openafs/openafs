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

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <krb5.h>

#include "akimpersonate.h"

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

#if USING_HEIMDAL
#define deref_keyblock_enctype(kb)		\
    ((kb)->keytype)

#define deref_entry_keyblock(entry)		\
    entry->keyblock

#define deref_session_key(creds)		\
    creds->session

#if !defined(HAVE_KRB5_ENCRYPT_TKT_PART) && defined(HAVE_ENCODE_KRB5_ENC_TKT_PART) && defined(HAVE_KRB5_C_ENCRYPT)
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
#endif

static const int any_enctype[2] = {0, 0};
static const krb5_data empty_string;

#define deref_enc_tkt_addrs(tkt)		\
    tkt->caddr

#define deref_enc_length(enc)			\
    ((enc)->cipher.length)

#define deref_enc_data(enc)			\
    ((enc)->cipher.data)

#define krb5_free_keytab_entry_contents krb5_kt_free_entry

#else
#define deref_keyblock_enctype(kb)		\
    ((kb)->enctype)

#define deref_entry_keyblock(entry)		\
    entry->key

#define deref_session_key(creds)		\
    creds->keyblock

#define deref_enc_tkt_addrs(tkt)		\
    tkt->caddrs

#define deref_enc_length(enc)			\
    ((enc)->ciphertext.length)

#define deref_enc_data(enc)			\
    ((enc)->ciphertext.data)

#endif

#define deref_entry_enctype(entry)			\
    deref_keyblock_enctype(&deref_entry_keyblock(entry))

krb5_error_code get_credv5_akimpersonate(krb5_context context,
					 char* keytab,
					 krb5_principal service_principal,
					 krb5_principal client_principal,
					 time_t starttime,
					 time_t endtime,
					 int *allowed_enctypes,
					 int *paddress,
					 krb5_creds** out_creds /* out */ )
{
#if defined(USING_HEIMDAL) || (defined(HAVE_ENCODE_KRB5_ENC_TKT) && defined(HAVE_ENCODE_KRB5_TICKET) && defined(HAVE_KRB5_C_ENCRYPT))
    krb5_error_code code;
    krb5_keytab kt = 0;
    krb5_kt_cursor cursor[1];
    krb5_keytab_entry entry[1];
    krb5_ccache cc = 0;
    krb5_creds *creds = 0;
    krb5_enctype enctype;
    krb5_kvno kvno;
    krb5_keyblock session_key[1];
#if USING_HEIMDAL
    Ticket ticket_reply[1];
    EncTicketPart enc_tkt_reply[1];
    krb5_address address[30];
    krb5_addresses faddr[1];
    int temp_vno[1];
    time_t temp_time[2];
#else
    krb5_ticket ticket_reply[1];
    krb5_enc_tkt_part enc_tkt_reply[1];
    krb5_address address[30], *faddr[30];
#endif
    krb5_data * temp;
    int i;
    static int any_enctype[] = {0};
    *out_creds = 0;
    if (!(creds = malloc(sizeof *creds))) {
        code = ENOMEM;
        goto cleanup;
    }
    if (!allowed_enctypes)
        allowed_enctypes = any_enctype;

    cc = 0;
    enctype = 0; /* AKIMPERSONATE_IGNORE_ENCTYPE */
    kvno = 0; /* AKIMPERSONATE_IGNORE_VNO */
    memset((char*)creds, 0, sizeof *creds);
    memset((char*)entry, 0, sizeof *entry);
    memset((char*)session_key, 0, sizeof *session_key);
    memset((char*)ticket_reply, 0, sizeof *ticket_reply);
    memset((char*)enc_tkt_reply, 0, sizeof *enc_tkt_reply);
    code = krb5_kt_resolve(context, keytab, &kt);
    if (code) {
        goto cleanup;
    }

    if (service_principal) {
        for (i = 0; (enctype = allowed_enctypes[i]) || !i; ++i) {
	    code = krb5_kt_get_entry(context,
				     kt,
				     service_principal,
				     kvno,
				     enctype,
				     entry);
	    if (!code) {
		if (allowed_enctypes[i])
		    deref_keyblock_enctype(session_key) = allowed_enctypes[i];
		break;
	    }
        }
        if (code) {
	    goto cleanup;
        }
    } else {
        krb5_keytab_entry new[1];
        int best = -1;
        memset(new, 0, sizeof *new);
        if ((code == krb5_kt_start_seq_get(context, kt, cursor))) {
            goto cleanup;
        }
        while (!(code = krb5_kt_next_entry(context, kt, new, cursor))) {
            for (i = 0;
                    allowed_enctypes[i] && allowed_enctypes[i]
		     != deref_entry_enctype(new); ++i)
                ;
            if ((!i || allowed_enctypes[i]) &&
		(best < 0 || best > i)) {
                krb5_free_keytab_entry_contents(context, entry);
                *entry = *new;
                memset(new, 0, sizeof *new);
            } else krb5_free_keytab_entry_contents(context, new);
        }
        if ((i = krb5_kt_end_seq_get(context, kt, cursor))) {
            code = i;
            goto cleanup;
        }
        if (best < 0) {
            goto cleanup;
        }
        deref_keyblock_enctype(session_key) = deref_entry_enctype(entry);
    }

    /* Make Ticket */

#if USING_HEIMDAL
    if ((code = krb5_generate_random_keyblock(context,
					      deref_keyblock_enctype(session_key), session_key))) {
        goto cleanup;
    }
    enc_tkt_reply->flags.initial = 1;
    enc_tkt_reply->transited.tr_type = DOMAIN_X500_COMPRESS;
    enc_tkt_reply->cname = client_principal->name;
    enc_tkt_reply->crealm = client_principal->realm;
    enc_tkt_reply->key = *session_key;
    {
        static krb5_data empty_string;
        enc_tkt_reply->transited.contents = empty_string;
    }
    enc_tkt_reply->authtime = starttime;
    enc_tkt_reply->starttime = temp_time;
    *enc_tkt_reply->starttime = starttime;
#if 0
    enc_tkt_reply->renew_till = temp_time + 1;
    *enc_tkt_reply->renew_till = endtime;
#endif
    enc_tkt_reply->endtime = endtime;
#else
    if ((code = krb5_c_make_random_key(context,
				       deref_keyblock_enctype(session_key), session_key))) {
        goto cleanup;
    }
    enc_tkt_reply->magic = KV5M_ENC_TKT_PART;
#define DATACAST        (unsigned char *)
    enc_tkt_reply->flags |= TKT_FLG_INITIAL;
    enc_tkt_reply->transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
    enc_tkt_reply->session = session_key;
    enc_tkt_reply->client = client_principal;
    {
        static krb5_data empty_string;
        enc_tkt_reply->transited.tr_contents = empty_string;
    }
    enc_tkt_reply->times.authtime = starttime;
    enc_tkt_reply->times.starttime = starttime; /* krb524init needs this */
    enc_tkt_reply->times.endtime = endtime;
#endif  /* USING_HEIMDAL */
    /* NB:  We will discard address for now--ignoring caddr field
       in any case.  MIT branch does what it always did. */

    if (paddress && *paddress) {
        deref_enc_tkt_addrs(enc_tkt_reply) = faddr;
#if USING_HEIMDAL
        faddr->len = 0;
        faddr->val = address;
#endif
        for (i = 0; paddress[i]; ++i) {
#if USING_HEIMDAL
            address[i].addr_type = KRB5_ADDRESS_INET;
            address[i].address.data = (void*)(paddress+i);
            address[i].address.length = sizeof(paddress[i]);
#else
#if !USING_SSL
            address[i].magic = KV5M_ADDRESS;
            address[i].addrtype = ADDRTYPE_INET;
#else
            address[i].addrtype = AF_INET;
#endif
            address[i].contents = (void*)(paddress+i);
            address[i].length = sizeof(int);
            faddr[i] = address+i;
#endif
        }
#if USING_HEIMDAL
        faddr->len = i;
#else
        faddr[i] = 0;
#endif
    }

#if USING_HEIMDAL
    ticket_reply->sname = service_principal->name;
    ticket_reply->realm = service_principal->realm;

    { /* crypto block */
        krb5_crypto crypto = 0;
        unsigned char *buf = 0;
        size_t buf_size, buf_len;
        char *what;

        ASN1_MALLOC_ENCODE(EncTicketPart, buf, buf_size,
			   enc_tkt_reply, &buf_len, code);
        if(code) {
            goto cleanup;
        }

        if(buf_len != buf_size) {
            goto cleanup;
        }
        what = "krb5_crypto_init";
        code = krb5_crypto_init(context,
				&deref_entry_keyblock(entry),
				deref_entry_enctype(entry),
				&crypto);
        if(!code) {
            what = "krb5_encrypt";
            code = krb5_encrypt_EncryptedData(context, crypto, KRB5_KU_TICKET,
					      buf, buf_len, entry->vno, &(ticket_reply->enc_part));
        }
        if (buf) free(buf);
        if (crypto) krb5_crypto_destroy(context, crypto);
        if(code) {
            goto cleanup;
        }
    } /* crypto block */
    ticket_reply->enc_part.etype = deref_entry_enctype(entry);
    ticket_reply->enc_part.kvno = temp_vno;
    *ticket_reply->enc_part.kvno = entry->vno;
    ticket_reply->tkt_vno = 5;
#else
    ticket_reply->server = service_principal;
    ticket_reply->enc_part2 = enc_tkt_reply;
    if ((code = krb5_encrypt_tkt_part(context, &deref_entry_keyblock(entry), ticket_reply))) {
        goto cleanup;
    }
    ticket_reply->enc_part.kvno = entry->vno;
#endif

    /* Construct Creds */

    if ((code = krb5_copy_principal(context, service_principal,
				    &creds->server))) {
        goto cleanup;
    }
    if ((code = krb5_copy_principal(context, client_principal,
				    &creds->client))) {
        goto cleanup;
    }
    if ((code = krb5_copy_keyblock_contents(context, session_key,
					    &deref_session_key(creds)))) {
        goto cleanup;
    }

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
    if (!deref_enc_tkt_addrs(enc_tkt_reply))
        ;
    else if ((code = krb5_copy_addresses(context,
					 deref_enc_tkt_addrs(enc_tkt_reply), &creds->addresses))) {
        goto cleanup;
    }

#if USING_HEIMDAL
    {
	size_t creds_tkt_len;
	ASN1_MALLOC_ENCODE(Ticket, creds->ticket.data, creds->ticket.length,
			   ticket_reply, &creds_tkt_len, code);
	if(code) {
	    goto cleanup;
	}
    }
#else
    if ((code = encode_krb5_ticket(ticket_reply, &temp))) {
	goto cleanup;
    }
    creds->ticket = *temp;
    free(temp);
#endif
    /* return creds */
    *out_creds = creds;
    creds = 0;
cleanup:
    if (deref_enc_data(&ticket_reply->enc_part))
        free(deref_enc_data(&ticket_reply->enc_part));
    krb5_free_keytab_entry_contents(context, entry);
    if (client_principal)
        krb5_free_principal(context, client_principal);
    if (service_principal)
        krb5_free_principal(context, service_principal);
    if (cc)
        krb5_cc_close(context, cc);
    if (kt)
        krb5_kt_close(context, kt);
    if (creds) krb5_free_creds(context, creds);
    krb5_free_keyblock_contents(context, session_key);
out:
    return code;
#else
    return -1;
#endif
}
