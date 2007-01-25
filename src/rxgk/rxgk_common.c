/*
 * Copyright (c) 2002 - 2004, 2007, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

