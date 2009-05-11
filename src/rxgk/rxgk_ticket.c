/*
 * Copyright (c) 1995 - 1998, 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rxgk_locl.h"

RCSID("$Id$");

#include <errno.h>

#include "rxgk_proto.h"

int
rxgk_make_ticket(struct rxgk_server_params *params, 
		 gss_ctx_id_t ctx, 
		 const void *snonce, size_t slength,
		 const void *cnonce, size_t clength,
		 RXGK_Ticket_Crypt *token, 
		 int32_t enctype)
{
    struct rxgk_ticket ticket;
    gss_name_t src_name;
    OM_uint32 lifetime;
    OM_uint32 major_status, minor_status;
    gss_buffer_desc exported_name;
    struct rxgk_keyblock key;
    int ret;

    memset(&ticket, 0, sizeof(ticket));

    ticket.ticketversion = 0;
    ticket.enctype = enctype;

    ret = rxgk_derive_k0(ctx, snonce, slength, cnonce, clength, enctype, &key);
    if (ret) {
	return ret;
    }
    ticket.key.len = key.length;
    ticket.key.val = key.data;
    ticket.level = RXGK_WIRE_ENCRYPT;
    ticket.starttime = time(NULL);
    major_status = gss_inquire_context(&minor_status,
				       ctx,
				       &src_name,
				       NULL,
				       &lifetime,
				       NULL, NULL, NULL, NULL);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	free(ticket.key.val);
	return EINVAL;
    }

    ticket.expirationtime = time(NULL) + lifetime;

    ticket.lifetime = params->connection_lifetime;
    ticket.bytelife = params->bytelife;

    major_status = gss_export_name(&minor_status,
				   src_name,
				   &exported_name);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	major_status = gss_release_name(&minor_status,
					&src_name);
	if (GSS_ERROR(major_status)) {
	    _rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	}
	free(ticket.key.val);
	return EINVAL;
    }

    major_status = gss_release_name(&minor_status,
				    &src_name);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	major_status = gss_release_buffer(&minor_status, &exported_name);
	if (GSS_ERROR(major_status)) {
            _rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
        }
	free(ticket.key.val);
	return EINVAL;
    }

    ticket.ticketprincipal.len = exported_name.length;
    ticket.ticketprincipal.val = malloc(exported_name.length);
    if (ticket.ticketprincipal.val == NULL) {
	major_status = gss_release_buffer(&minor_status, &exported_name);
	if (GSS_ERROR(major_status)) {
            _rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
        }
	free(ticket.key.val);
        return EINVAL;
    }
    memcpy(ticket.ticketprincipal.val,
	   exported_name.value,
	   exported_name.length);
    
    major_status = gss_release_buffer(&minor_status, &exported_name);
    if (GSS_ERROR(major_status)) {
	_rxgk_gssapi_err(major_status, minor_status, GSS_C_NO_OID);
	free(ticket.key.val);
	return EINVAL;
    }

    ticket.ext.len = 0;
    ticket.ext.val = NULL;

    rxgk_encrypt_ticket(&ticket, token);

    return 0;
}

int
rxgk_encrypt_ticket(struct rxgk_ticket *ticket, RXGK_Ticket_Crypt *opaque)
{
    int ret;
    char *buf;
    size_t len;
    RXGK_Token clear, crypt;
    struct rxgk_keyblock key;

    len = RXGK_TICKET_MAX_SIZE;
    buf = malloc(RXGK_TICKET_MAX_SIZE);

    if (ydr_encode_rxgk_ticket(ticket, buf, &len) == NULL) {
	return errno;
    }

    clear.val = buf;
    clear.len = RXGK_TICKET_MAX_SIZE - len;

    fprintf(stderr, "before encrypt:");
    print_chararray(clear.val, clear.len);

    ret = rxgk_get_server_ticket_key(&key);
    if (ret) {
	free(buf);
	return EINVAL;
    }

    ret = rxgk_encrypt_buffer(&clear, &crypt, &key, RXGK_SERVER_ENC_TICKET);
    if (ret) {
	free(key.data);
	free(buf);
	return EINVAL;
    }

    opaque->val = crypt.val;
    opaque->len = crypt.len;
    
    fprintf(stderr, "after encrypt:");
    print_chararray(crypt.val, crypt.len);

    return 0;
}

int
rxgk_decrypt_ticket(RXGK_Ticket_Crypt *opaque, struct rxgk_ticket *ticket)
{
    size_t len;
    int ret;
    RXGK_Token clear, crypt;
    struct rxgk_keyblock key;

    ret = rxgk_get_server_ticket_key(&key);
    if (ret) {
	return EINVAL;
    }

    crypt.val = opaque->val;
    crypt.len = opaque->len;

    fprintf(stderr, "before decrypt:");
    print_chararray(crypt.val, crypt.len);

    ret = rxgk_decrypt_buffer(&crypt, &clear, &key, RXGK_SERVER_ENC_TICKET);
    if (ret) {
	free(key.data);
	return EINVAL;
    }

    fprintf(stderr, "after decrypt:");
    print_chararray(clear.val, clear.len);

    len = clear.len;
    if (ydr_decode_rxgk_ticket(ticket, clear.val, &len) == NULL) {
        return errno;
    }

    return 0;
}

