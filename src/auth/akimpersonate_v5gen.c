/*
 * Copyright (c) 2013 Sine Nomine Associates
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

/* are we using MIT krb5, and are we missing the functions encode_krb5_ticket
 * and encode_krb5_enc_tkt_part? */
#if defined(HAVE_KRB5_CREDS_KEYBLOCK) && !defined(HAVE_KRB5_CREDS_SESSION) \
    && !defined(HAVE_ENCODE_KRB5_TICKET) && !defined(HAVE_ENCODE_KRB5_ENC_TKT_PART)

# include <afs/stds.h>

# include <sys/types.h>
# include <time.h>
# include <errno.h>
# include <netinet/in.h>
# include <string.h>
# include <rx/xdr.h>
# include <rx/rx.h>
# include <des.h>
# include <des_prototypes.h>

# include "lifetimes.h"
# include "rxkad.h"

# include "v5gen-rewrite.h"
# include "v5gen.h"
# include "der.h"

# include "akimpersonate_v5gen.h"

int
akv5gen_encode_krb5_ticket(int kvno,
                           char *realm,
                           int name_type,
                           int name_len,
                           char **name_parts,
                           int enctype,
			   size_t cipher_len,
			   char *cipher_data,
			   size_t *a_out_len,
			   char **a_out_data)
{
    Ticket v5gen_tkt;
    int code = 0;
    size_t dummy;
    char *outdata = NULL;
    size_t outlen = 0;

    memset(&v5gen_tkt, 0, sizeof(v5gen_tkt));

    v5gen_tkt.tkt_vno = 5;
    v5gen_tkt.realm = realm;

    v5gen_tkt.sname.name_type = name_type;
    v5gen_tkt.sname.name_string.len = name_len;
    v5gen_tkt.sname.name_string.val = name_parts;

    v5gen_tkt.enc_part.etype = enctype;
    v5gen_tkt.enc_part.kvno = &kvno;
    v5gen_tkt.enc_part.cipher.length = cipher_len;
    v5gen_tkt.enc_part.cipher.data = cipher_data;

    ASN1_MALLOC_ENCODE(Ticket, outdata, outlen,
                       &v5gen_tkt, &dummy, code);
    if (code == 0 && dummy != outlen)
	code = EINVAL;
    if (code)
	goto cleanup;

    *a_out_len = outlen;
    *a_out_data = outdata;
    outdata = NULL;

 cleanup:
    free(outdata);
    return code;
}

int
akv5gen_encode_krb5_enc_tkt_part(int enctype,
                                 size_t key_len,
                                 unsigned char *key_data,
                                 char *realm,
                                 int name_type,
                                 int name_len,
                                 char **name_parts,
                                 int transited_type,
                                 int transited_len,
                                 char *transited_data,
                                 time_t authtime,
                                 time_t starttime,
                                 time_t endtime,
                                 time_t renew_till,
			         size_t *a_out_len,
			         char **a_out_data)
{
    EncTicketPart v5gen_enc;
    size_t dummy;
    int code = 0;
    char *outdata = NULL;
    size_t outlen = 0;

    memset(&v5gen_enc, 0, sizeof(v5gen_enc));

    /* assume the only flag that should be set is _INITIAL */
    v5gen_enc.flags.initial = 1;

    v5gen_enc.key.keytype = enctype;
    v5gen_enc.key.keyvalue.length = key_len;
    v5gen_enc.key.keyvalue.data = key_data;

    v5gen_enc.crealm = realm;

    v5gen_enc.cname.name_type = name_type;
    v5gen_enc.cname.name_string.len = name_len;
    v5gen_enc.cname.name_string.val = name_parts;

    v5gen_enc.transited.tr_type = transited_type;
    v5gen_enc.transited.contents.length = transited_len;
    v5gen_enc.transited.contents.data = transited_data;

    v5gen_enc.authtime = authtime;
    v5gen_enc.starttime = &starttime;
    v5gen_enc.endtime = endtime;
    v5gen_enc.renew_till = &renew_till;

    /* assume we have no addresses */
    v5gen_enc.caddr = NULL;

    /* assume we have no authz data */
    v5gen_enc.authorization_data = NULL;

    ASN1_MALLOC_ENCODE(EncTicketPart, outdata, outlen,
                       &v5gen_enc, &dummy, code);
    if (code == 0 && dummy != outlen)
	code = EINVAL;
    if (code)
	goto cleanup;

    *a_out_len = outlen;
    *a_out_data = outdata;
    outdata = NULL;

 cleanup:
    free(outdata);
    return code;
}

#endif
