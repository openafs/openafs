/*
 * Copyright (c) 2002 - 2004, 2007, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 * 
 * Redistribution is not permitted
 */

#include "rxgk_locl.h"

RCSID("$Id$");

#include <errno.h>

#include <rx/rx.h>
#include "rxgk_proto.h"

/*
 *
 */

void
rxgk_getheader(struct rx_packet *pkt, struct rxgk_header_data *h)
{
  uint32_t t;

  /* Collect selected packet fields */
  h->call_number = htonl(pkt->header.callNumber);
  t = ((pkt->header.cid & RX_CHANNELMASK) << (32 - RX_CIDSHIFT))
    | ((pkt->header.seq & 0x3fffffff));
  h->channel_and_seq = htonl(t);
}


static rxgk_logf logfunc = NULL;
static void *logctx = NULL;

void
rxgk_set_log(rxgk_logf func, void *ctx)
{
    logfunc = func;
    logctx = ctx;
}

void
rxgk_log_stdio(void *ctx, const char *fmt, ...)
{
    FILE *out = ctx;
    va_list va;

    if (out == NULL)
	out = stderr;

    va_start(va, fmt);
    vfprintf(out, fmt, va);
    va_end(va);
}

void
_rxgk_gssapi_err(OM_uint32 maj_stat, OM_uint32 min_stat, gss_OID mech)
{
    OM_uint32 junk;
    gss_buffer_desc maj_error_message;
    gss_buffer_desc min_error_message;
    OM_uint32 msg_ctx = 0;

    if (logfunc == NULL)
	return;

    maj_error_message.value = NULL;
    min_error_message.value = NULL;
	
    gss_display_status(&junk, maj_stat, GSS_C_GSS_CODE,
		       mech, &msg_ctx, &maj_error_message);
    gss_display_status(&junk, min_stat, GSS_C_MECH_CODE,
		       mech, &msg_ctx, &min_error_message);
    (*logfunc)(logctx,
	       "gss-code: %.*s\nmech-code: %.*s\n", 
	       (int)maj_error_message.length, 
	       (char *)maj_error_message.value, 
	       (int)min_error_message.length, 
	       (char *)min_error_message.value);
    
    gss_release_buffer(&junk, &maj_error_message);
    gss_release_buffer(&junk, &min_error_message);
}

int
rxgk_derive_k0(gss_ctx_id_t ctx,
	       const void *snonce, size_t slength,
	       const void *cnonce, size_t clength,
	       int32_t enctype, struct rxgk_keyblock *key)
{
    gss_buffer_desc prf_in, prf_out;
    OM_uint32 major_status, minor_status;
    unsigned char *nonces;

    if (enctype != RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96)
	return EINVAL;

    key->length = 32;
    key->enctype = enctype;
    
    key->data = malloc(key->length);
    if (key->data == NULL)
	return ENOMEM;

    nonces = malloc(clength + slength);
    memcpy(nonces, snonce, slength);
    memcpy(&nonces[slength], cnonce, clength);

    prf_in.value = nonces;
    prf_in.length = clength + slength;

    prf_out.value = NULL;
    prf_out.length = 0;

    major_status = gss_pseudo_random(&minor_status, ctx, GSS_C_PRF_KEY_FULL,
				     &prf_in, key->length, &prf_out);
    free(nonces);
    if (major_status != GSS_S_COMPLETE || prf_out.length != key->length) {
	gss_release_buffer(&minor_status, &prf_out);
	free(key->data);
	memset(key, 0, sizeof(*key));
	return EINVAL;
    }

    memcpy(key->data, prf_out.value, key->length);

    gss_release_buffer(&minor_status, &prf_out);

    return 0;
}

void
print_chararray(char *val, unsigned len)
{
    int i;

    for (i = 0; i < len; i++) {
	fprintf(stderr, " %02x", (unsigned char)val[i]);
    }
    fprintf(stderr, "\n");    
}

int
rxgk_encrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage)
{
    int i;
    int x;

    out->len = in->len + 4;
    out->val = malloc(out->len);

    out->val[0] = 1;
    out->val[1] = 2;
    out->val[2] = 3;
    out->val[3] = 4;

    /* XXX add real crypto */
    x = keyusage;
    for (i = 0; i < in->len; i++) {
	x += i * 3 + ((unsigned char *)key->data)[i%key->length];
	out->val[i+4] = in->val[i] ^ x;
    }
    
    return 0;
}

int
rxgk_decrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage)
{
    int i;
    int x;

    if (in->len < 4) {
	return EINVAL;
    }

    if (in->val[0] != 1 ||
	in->val[0] != 1 ||
	in->val[0] != 1 ||
	in->val[0] != 1) {
	return EINVAL;
    }

    out->len = in->len - 4;
    out->val = malloc(out->len);

    /* XXX add real crypto */
    x = keyusage;
    for (i = 0; i < out->len; i++) {
	x += i * 3 + ((unsigned char *)key->data)[i%key->length];
	out->val[i] = in->val[i+4] ^ x;
    }

    return 0;
}

int
rxgk_get_server_ticket_key(struct rxgk_keyblock *key)
{
    int i;
    /* XXX get real key */

    key->length = 32;
    key->enctype = RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96;
    
    key->data = malloc(key->length);
    if (key->data == NULL)
	return ENOMEM;

    for (i = 0; i < key->length; i++) {
	((unsigned char *)key->data)[i] = 0x23 + i * 47;
    }

    return 0;
}
