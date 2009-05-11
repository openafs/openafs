/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
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

#include <rx/rx.h>
#include "rxgk_proto.h"
#include "rxgk_proto.cs.h"

static int
decrypt_clientinfo(RXGK_Token *opaque, struct RXGK_ClientInfo *ticket,
		   gss_ctx_id_t ctx)
{
    size_t len;
    const char *ret;
    gss_buffer_desc encrypted_buffer;
    gss_buffer_desc buffer;
    OM_uint32 major_status, minor_status;

    encrypted_buffer.value = opaque->val;
    encrypted_buffer.length = opaque->len;

    major_status = gss_unwrap(&minor_status,
			       ctx,
			       &encrypted_buffer,
			       &buffer,
			       NULL,
			       NULL);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	return EINVAL;
    }

    len = buffer.length;
    ret = ydr_decode_RXGK_ClientInfo(ticket, buffer.value, &len);

    major_status = gss_release_buffer(&minor_status, &buffer);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	free(opaque->val);
	return EINVAL;
    }
    
    if (ret == NULL) {
        return EINVAL;
    }

    return 0;
}



int
rxgk_get_gss_cred(uint32_t addr,
		  short port,
		  gss_name_t client_name,
		  gss_name_t target_name,
		  time_t *expire,
		  rxgk_level *minlevel,
		  int32_t nametag,
		  RXGK_Ticket_Crypt *ticket,
		  struct rxgk_keyblock *key)
{
    RXGK_Token opaque_in, opaque_out, rxgk_cred;
    gss_buffer_desc input_token, output_token;
    struct rx_securityClass *secobj;
    struct rx_connection *conn;
    RXGK_client_start client_start;
    OM_uint32 minor_status, major_status;
    uint32_t status;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    int ret, server_done = 0, client_done = 0;
    int32_t enctypes[] = { RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96 };
    char clientnonce[RXGK_MAX_NONCE];

    memset(&client_start, 0, sizeof(client_start));
    memset(key, 0, sizeof(*key));
    memset(clientnonce, 0, sizeof(*clientnonce));

    client_start.sp_enctypes.val = enctypes;
    client_start.sp_enctypes.len = sizeof(enctypes)/sizeof(enctypes[0]);
    client_start.sp_nametag = nametag;
    client_start.sp_client_nonce.val = clientnonce;
    client_start.sp_client_nonce.len = sizeof(clientnonce);

    secobj = rxnull_NewClientSecurityObject();

    conn = rx_NewConnection(addr, port, RXGK_SERVICE_ID, secobj, 0);
    if (conn == NULL)
	return ENETDOWN;

    memset(&output_token, 0, sizeof(output_token));
    memset(&input_token, 0, sizeof(input_token));

    memset(&opaque_out, 0, sizeof(opaque_out));
    memset(&opaque_in, 0, sizeof(opaque_in));
    memset(&rxgk_cred, 0, sizeof(rxgk_cred));

    /*
     * XXX unbreak statemachine
     */

    while (1) {
	RXGK_Token input, output = { 0 , NULL };

	opaque_in = opaque_out;

	major_status = gss_init_sec_context(&minor_status,
					    GSS_C_NO_CREDENTIAL,
					    &ctx,
					    target_name,
					    GSS_C_NO_OID,
					    GSS_C_MUTUAL_FLAG|GSS_C_INTEG_FLAG,
					    GSS_C_INDEFINITE,
					    GSS_C_NO_CHANNEL_BINDINGS,
					    &input_token,
					    NULL,
					    &output_token,
					    NULL,
					    NULL);
	if (output.len) {
	    free(output.val);
	    output.len = 0;
	    output.val = NULL;
	}
	if (major_status == GSS_S_COMPLETE && output_token.length == 0) {
	    ret = 0;
	    break;
	} else if (major_status == GSS_S_COMPLETE) {
	    client_done = 1;
	} else if (major_status != GSS_S_COMPLETE && server_done) {
	    _rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	    ret = EINVAL;
	    break;
	} else if ((major_status & GSS_S_CONTINUE_NEEDED) == 0) {
	    _rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	    ret = EINVAL;
	    break;
	}

	input.val = output_token.value;
	input.len = output_token.length;

	ret = RXGK_GSSNegotiate(conn,
				&client_start,
				&input,
				&opaque_in,
				&output,
				&opaque_out,
				&status,
				&rxgk_cred);
	gss_release_buffer(&minor_status, &output_token);
	if (opaque_in.len != 0) {
	    free(opaque_in.val);
	    opaque_in.len = 0;
	}
	if (ret) {
	    break;
	}

	if (status == GSS_S_COMPLETE && output.len == 0) {
	    ret = 0;
	    break;
	} else if (client_done) {
	    ret = EINVAL;
	    break;
	} else if (status == GSS_S_COMPLETE) {
	    server_done = 1;
	} else if ((major_status & GSS_S_CONTINUE_NEEDED) == 0) {
	    ret = EINVAL;
	    break;
	} else if (rxgk_cred.len) {
	    ret = EINVAL;
	    break;
	}
	input_token.value = output.val;
	input_token.length = output.len;
    }

    rx_DestroyConnection(conn);

    if (ret == 0) {
	struct RXGK_ClientInfo clientinfo;

	decrypt_clientinfo(&rxgk_cred, &clientinfo, ctx);

	ticket->val = clientinfo.ci_ticket.val;
	ticket->len = clientinfo.ci_ticket.len;
	rxgk_cred.len = 0;
	rxgk_cred.val = NULL;

	ret = rxgk_derive_k0(ctx, 
			     clientinfo.ci_server_nonce.val, 
			     clientinfo.ci_server_nonce.len, 
			     client_start.sp_client_nonce.val,
			     client_start.sp_client_nonce.len,
			     RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96,
			     key);
    }

    gss_delete_sec_context(&minor_status, &ctx, GSS_C_NO_BUFFER);

    if (rxgk_cred.len)
	free(rxgk_cred.val);
    if (opaque_out.len != 0)
	free(opaque_out.val);

    return ret;
}
