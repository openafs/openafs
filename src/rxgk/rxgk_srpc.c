/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
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
#include "rxgk_proto.ss.h"

static int
encrypt_clientinfo(struct RXGK_ClientInfo *clientinfo, RXGK_Token *opaque,
		   gss_ctx_id_t ctx)
{
    char *ret;
    size_t len;
    gss_buffer_desc buffer;
    gss_buffer_desc encrypted_buffer;
    OM_uint32 major_status, minor_status;

    len = RXGK_CLIENTINFO_MAX_SIZE;
    buffer.value = malloc(len);

    ret = ydr_encode_RXGK_ClientInfo(clientinfo, buffer.value, &len);
    if (ret == NULL) {
	return errno;
    }

    buffer.length = RXGK_CLIENTINFO_MAX_SIZE - len;

    major_status = gss_wrap(&minor_status,
			    ctx,
			    1,
			    GSS_C_QOP_DEFAULT,
			    &buffer,
			    NULL,
			    &encrypted_buffer);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	free(buffer.value);
	return EINVAL;
    }

    free(buffer.value);
    opaque->val = malloc(encrypted_buffer.length);
    opaque->len = encrypted_buffer.length;
    memcpy(opaque->val, encrypted_buffer.value, encrypted_buffer.length);
    
    major_status = gss_release_buffer(&minor_status, &encrypted_buffer);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	free(opaque->val);
	return EINVAL;
    }

    return 0;

}

static int
choose_enctype(const RXGK_Enctypes *client_enctypes,
	       const RXGK_Enctypes *server_enctypes,
	       int32_t *chosen_enctype) {

    if (client_enctypes->len == 0) {
	return EINVAL;
    }

    /* XXX actually do matching */
    *chosen_enctype = RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96;
    return 0;
}

int
SRXGK_GSSNegotiate(struct rx_call *call,
		   const struct RXGK_client_start *client_start,
		   const RXGK_Token *input_token_buffer,
		   const RXGK_Token *opaque_in,
		   RXGK_Token *output_token_buffer,
		   RXGK_Token *opaque_out,
		   uint32_t *gss_status,
		   RXGK_Token *rxgk_info)
{
    gss_buffer_desc input_token, output_token;
    OM_uint32 major_status, minor_status;
    gss_ctx_id_t ctx;
    struct rxgk_server_params *params;
    char servernonce[RXGK_MAX_NONCE];
    int retval;

    output_token_buffer->val = NULL;
    output_token_buffer->len = 0;

    opaque_out->val = NULL;
    opaque_out->len = 0;
    
    rxgk_info->val = NULL;
    rxgk_info->len = 0;

    /* the GSS-API context is stored on the server rock */
    ctx = rx_getConnRock(call->conn);
    params = (struct rxgk_server_params *) rx_getServiceRock(call->conn->service);

    input_token.value = input_token_buffer->val;
    input_token.length = input_token_buffer->len;

    major_status = gss_accept_sec_context(&minor_status,
					  &ctx,
					  GSS_C_NO_CREDENTIAL,
					  &input_token,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  NULL,
					  NULL,
					  &output_token,
					  NULL,
					  NULL,
					  NULL);
    *gss_status = major_status;
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	gss_delete_sec_context(&minor_status, &ctx, NULL);
	goto out;
    }

    /* copy out the buffer */
    output_token_buffer->val = malloc(output_token.length);
    if (output_token_buffer->val == NULL) {
	gss_release_buffer(&minor_status, &output_token);
	goto out;
    }
    output_token_buffer->len = output_token.length;
    memcpy(output_token_buffer->val,
	   output_token.value, output_token.length);
    gss_release_buffer(&minor_status, &output_token);

    if (major_status != GSS_S_COMPLETE) {
	retval = 0;
	goto out;
    }

    RXGK_Ticket_Crypt ticket;
    struct RXGK_ClientInfo clientinfo;
    RXGK_Token encrypted_info;

    int32_t chosen_enctype;
    
    retval = choose_enctype(&client_start->sp_enctypes,
			    &params->enctypes, &chosen_enctype);
    if (retval) {
	gss_delete_sec_context(&minor_status, &ctx, NULL);
	goto out;
    }
    
    memset(&clientinfo, 0, sizeof(clientinfo));
    
    clientinfo.ci_server_nonce.val = servernonce;
    clientinfo.ci_server_nonce.len = sizeof(servernonce);

    retval = rxgk_make_ticket(params, ctx, 
			      clientinfo.ci_server_nonce.val, 
			      clientinfo.ci_server_nonce.len,
			      client_start->sp_client_nonce.val,
			      client_start->sp_client_nonce.len,
			      &ticket, chosen_enctype);
    if (retval) {
	gss_delete_sec_context(&minor_status, &ctx, NULL);
	goto out;
    }
    
    clientinfo.ci_ticket = ticket;

    retval = encrypt_clientinfo(&clientinfo, &encrypted_info, ctx);
    if (retval) {
	free(ticket.val);
	gss_delete_sec_context(&minor_status, &ctx, NULL);
	goto out;
    }
    
    *rxgk_info = encrypted_info;
    
    gss_delete_sec_context(&minor_status, &ctx, NULL);

out:
    rx_setConnRock(call->conn, ctx);

    return retval;
}
