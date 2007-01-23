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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include "k5ssl.h"
#include "k5s_im.h"

krb5_error_code
krb5_send_tgs(krb5_context context,
    const krb5_flags options,
    const krb5_ticket_times *times,
    const krb5_enctype *enctypes,
    krb5_const_principal server,
    krb5_address * const *addresses,
    krb5_authdata ** authdata,
    krb5_pa_data * const * padata,
    const krb5_data *second_ticket,
    krb5_creds *tgt,
    krb5_response *response)
{
    struct timeval now[1];
    krb5_kdc_req request[1];
    krb5_data *data = 0;
    krb5_ticket *tickets[2];
    krb5_cksumtype sumtype;
    krb5_checksum sumout[1];
    krb5_authenticator auth[1];
    krb5_ap_req apreq[1];
    krb5_pa_data **combined, pareq[1];
    int code;
    int flags = 0;
    krb5_error *error = 0;
    int i;
    size_t enclen;

    combined = 0;
    memset(request, 0, sizeof *request);
    memset(tickets, 0, sizeof tickets);
    memset(sumout, 0, sizeof sumout);
    memset(apreq, 0, sizeof apreq);
    request->kdc_options = options;
    request->server = (krb5_principal) server;
    request->from = times->starttime ? times->starttime : tgt->times.starttime;
    request->till = times->endtime ? times->endtime : tgt->times.endtime;
    request->rtime = times->renew_till;
    request->addresses = (krb5_address **) addresses;
    if (gettimeofday(now, 0) < 0)
	return errno;
    response->expected_nonce = request->nonce = (int) now->tv_sec;
    response->request_time = (int) now->tv_sec;
    if (authdata) {
	if ((code = encode_krb5_authdata(authdata, &data)))
	    goto Done;
	if ((code = krb5_c_encrypt_length(context, tgt->keyblock.enctype,
		data->length, &enclen)))
	    goto Done;
	request->authorization_data.ciphertext.length = enclen;
	if (!(request->authorization_data.ciphertext.data = malloc(enclen))) {
	    code = ENOMEM;
	    goto Done;
	}
	if ((code = krb5_c_encrypt(context, &tgt->keyblock,
		KRB5_KEYUSAGE_TGS_REQ_AD_SESSKEY, 0,
		data, &request->authorization_data)))
	    goto Done;
	if (data->data) free(data->data);
	free(data);
	data = 0;
    }
    if (!(request->ktype = (krb5_enctype *) enctypes)) {
	if ((code = krb5_get_tgs_ktypes(context, server, &request->ktype)))
	    goto Done;
    }
    code = KRB5_PROG_ETYPE_NOSUPP;
    i = 0;
    if (request->ktype)
	for (; request->ktype[i]; ++i) {
	    if (!krb5_c_valid_enctype(request->ktype[i]))
		goto Done;
	}
    request->nktypes = i;
    if (second_ticket) {
	if ((code = krb5_decode_ticket(second_ticket, tickets)))
	    goto Done;
	request->second_ticket = tickets;
    }
    if ((code = encode_krb5_kdc_req_body(request, &data)))
	goto Done;
    if ((code = krb5_get_kdc_req_checksum_type(context, &sumtype)))
	goto Done;
    if ((code = krb5_c_make_checksum(context, sumtype,
	    &tgt->keyblock, KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM, data, sumout)))
	goto Done;
    if (data->data) free(data->data);
    free(data);
    data = 0;
    memset(auth, 0, sizeof *auth);
    auth->subkey = 0;
    auth->seq_number = 0;
    auth->checksum = sumout;
    auth->client = tgt->client;
    auth->authorization_data = tgt->authdata;
    auth->ctime = now->tv_sec;
    auth->cusec = now->tv_usec;
    code = encode_krb5_authenticator(auth, &data);
    if (code) goto Done;
    free(sumout->contents);
    sumout->contents = 0;
    if ((code = krb5_decode_ticket(&tgt->ticket, &apreq->ticket)))
	goto Done;
    if ((code = krb5_c_encrypt_length(context, tgt->keyblock.enctype,
	    data->length, &enclen)))
	goto Done;
    apreq->authenticator.ciphertext.length = enclen;
    if (!(apreq->authenticator.ciphertext.data = malloc(enclen))) {
	code = ENOMEM;
	goto Done;
    }
    if ((code = krb5_c_encrypt(context, &tgt->keyblock,
	    KRB5_KEYUSAGE_TGS_REQ_AUTH, 0,
	    data, &apreq->authenticator)))
	goto Done;
    if (data->data) free(data->data);
    free(data);
    data = 0;
    code = encode_krb5_ap_req(apreq, &data);
    memset(apreq->authenticator.ciphertext.data, 0,
	apreq->authenticator.ciphertext.length);
    free(apreq->authenticator.ciphertext.data);
    apreq->authenticator.ciphertext.data = 0;
    if (code) goto Done;
    pareq->pa_type = KRB5_PADATA_AP_REQ;
    pareq->length = data->length;
    pareq->contents = data->data;
    free(data);
    data = 0;
    i = 0;
    if (padata) {
	for (; padata[i]; ++i)
	    ;
    }
    if (!(combined = (krb5_pa_data **) malloc((i+2) * sizeof *combined))) {
	code = ENOMEM;
	goto Done;
    }
    *combined = pareq;
    if (i) memcpy(combined+1, padata, i * sizeof *combined);
    combined[i+1] = 0;
    request->padata = combined;
    if ((code = encode_krb5_tgs_req(request, &data)))
	goto Done;
    for (;;) {
	code = krb5i_sendto_kdc(context, data, &server->realm,
		&response->response, &flags, 0);
	if (code) break;
	if (krb5_is_tgs_rep(&response->response)) {
	    response->message_type = KRB5_TGS_REP;
	    break;
	}
	response->message_type = KRB5_ERROR;
	if (!krb5_is_krb_error(&response->response))
	    break;
	else if (!(flags & USE_TCP))
	    break;
	code = decode_krb5_error(&response->response, &error);
	if (code || error->error != KRB5KRB_ERR_RESPONSE_TOO_BIG)
	    break;
	flags |= USE_TCP;
	krb5_free_error(context, error);
	error = 0;
	free(response->response.data);
	response->response.data = 0;
    }
Done:
    krb5_free_error(context, error);
    if (combined) free(combined);
    if (pareq->contents) free(pareq->contents);
    krb5_free_ticket(context, apreq->ticket);
    if (sumout->contents) {
	free(sumout->contents);
    }
    krb5_free_ticket(context, *tickets);
    if (apreq->authenticator.ciphertext.data)
	free(apreq->authenticator.ciphertext.data);
    if (request->authorization_data.ciphertext.data)
	free(request->authorization_data.ciphertext.data);
    if (data) {
	if (data->data) free(data->data);
	free(data);
    }
    if (!enctypes && request->ktype) {
	free(request->ktype);
    }
    return code;
}

krb5_error_code
krb5_copy_principal(krb5_context context,
    krb5_const_principal from,
    krb5_principal *to)
{
    krb5_principal x;
    int code;
    int i;

    code = ENOMEM;
    x = (krb5_principal) malloc(sizeof *x);
    if (!x) goto Done;
    memset(x, 0, sizeof *x);
    x->length = from->length;
    x->type = from->type;
    if (!(x->realm.data = malloc(x->realm.length = from->realm.length)))
	goto Done;
    memcpy(x->realm.data, from->realm.data, x->realm.length);
    x->data = (krb5_data *) malloc(x->length * sizeof *x->data);
    if (!x->data) goto Done;
    for (i = 0; i < x->length; ++i) {
	if (!(x->data[i].data = malloc(x->data[i].length = from->data[i].length)))
	    goto Done;
	memcpy(x->data[i].data, from->data[i].data, x->data[i].length);
    }
    *to = x;
    x = 0;
    code = 0;
Done:
    krb5_free_principal(context, x);
    return code;
}

krb5_error_code
krb5_copy_addresses(krb5_context context,
    krb5_address *const *in,
    krb5_address ***out)
{
    krb5_address **x;
    int code;
    int i;

    if (!in) {
	*out = 0;
	return 0;
    }
    for (i = 0; in[i++]; )
	;
    code = ENOMEM;
    if (!(x = (krb5_address **)malloc(i * sizeof *x)))
	goto Done;
    x[--i] = 0;
    for (i = 0; in[i]; ++i) {
	if (!(x[i] = (krb5_address *) malloc(sizeof **x)))
	    goto Done;
	if (!(x[i]->contents = malloc(x[i]->length = in[i]->length))) {
	    x[i+1] = 0;
	    goto Done;
	}
	memcpy(x[i]->contents, in[i]->contents, x[i]->length);
	x[i]->addrtype = in[i]->addrtype;
    }
    *out = x;
    x = 0;
    code = 0;
Done:
    krb5_free_addresses(context, x);
    return code;
}

krb5_error_code
krb5_get_cred_via_tkt(krb5_context context,
    krb5_creds *tgt,
    krb5_flags options,
    krb5_address *const *addresses,
    krb5_creds *match,
    krb5_creds **out_cred)
{
    int code;
    krb5_enctype *enctypes = 0;
    krb5_error *error = 0;
    krb5_response answer[1];
    krb5_kdc_rep *response = 0;
    krb5_creds *out = 0;
    krb5_data *data = 0;
    int clockskew;

    (void) krb5_get_clockskew(context, &clockskew);
    memset(answer, 0, sizeof *answer);
    if (match->keyblock.enctype) {
	enctypes = (krb5_enctype *) malloc(2*sizeof *enctypes);
	if (!enctypes) return ENOMEM;
	enctypes[0] = match->keyblock.enctype;
	enctypes[1] = 0;
    }
    code = krb5_send_tgs(context, options, &match->times, enctypes,
	match->server, addresses, match->authdata, 0,
	(options & KDC_OPT_ENC_TKT_IN_SKEY) ? &match->second_ticket : 0,
	tgt, answer);
    if (code) goto Done;

    switch(answer->message_type) {
    default:
	code = KRB5KRB_AP_ERR_MSG_TYPE;
	goto Done;
    case KRB5_ERROR:
	code = decode_krb5_error(&answer->response, &error);
	if (code) goto Done;
#if 0
printf ("error: ctime=%d.%06d stime=%d.%06d code=%d",
error->ctime, error->cusec,
error->stime, error->susec,
error->error);
{
char *p = 0;
if (error->client && (!krb5_unparse_name(context, error->client, &p))) {
printf (" client <%s>", p);
free(p);
}
if (error->server && (!krb5_unparse_name(context, error->server, &p))) {
printf (" server <%s>", p);
free(p);
}
}
#if 0
printf ("\ntext is:\n");
bin_dump(error->text.data, error->text.length, 0);
printf ("e_data is %#x(%d):\n", (int) error->e_data.data, error->e_data.length);
bin_dump(error->e_data.data, error->e_data.length, 0);
#endif
#endif
	code = error->error + ERROR_TABLE_BASE_krb5;
	goto Done;
    case KRB5_TGS_REP:
	break;
    }
    code = krb5_decode_kdc_rep(context, &answer->response, &tgt->keyblock, &response);
    if (code) goto Done;
    if (response->msg_type != KRB5_TGS_REP) {
	code = KRB5KRB_AP_ERR_MSG_TYPE;
	goto Done;
    }
    code = KRB5_KDCREP_MODIFIED;
    if (!krb5_principal_compare(context,
	    response->client, tgt->client))
	goto Done;
    if (!krb5_principal_compare(context,
	    response->enc_part2->server, match->server))
	goto Done;
    if (!krb5_principal_compare(context,
	    response->ticket->server, match->server))
	goto Done;
    if (response->enc_part2->nonce != answer->expected_nonce)
	goto Done;
    if ((options & KDC_OPT_POSTDATED) && match->times.starttime &&
	    match->times.starttime != response->enc_part2->times.starttime)
	goto Done;
    if (match->times.endtime &&
	    match->times.endtime < response->enc_part2->times.endtime)
	goto Done;
    if ((options & KDC_OPT_RENEWABLE) && match->times.renew_till &&
	    match->times.renew_till < response->enc_part2->times.renew_till)
	goto Done;
    if ((options & KDC_OPT_RENEWABLE_OK) &&
	    (response->enc_part2->flags & KDC_OPT_RENEWABLE)
	    && match->times.endtime &&
	    match->times.endtime < response->enc_part2->times.renew_till)
	goto Done;
    code = KRB5_KDCREP_MODIFIED;
    if (!match->times.starttime &&
	    labs(response->enc_part2->times.starttime -
		answer->request_time) >= clockskew)
	goto Done;
    code = ENOMEM;
    if (!(out = (krb5_creds *)malloc(sizeof *out))) goto Done;
    memset(out, 0, sizeof *out);
    if ((!options & KDC_OPT_ENC_TKT_IN_SKEY))
	;
    else if (!(out->second_ticket.data = malloc(out->second_ticket.length
		= match->second_ticket.length)))
	goto Done;
    else
	memcpy(out->second_ticket.data, match->second_ticket.data,
	    out->second_ticket.length);
    if ((code = krb5_copy_principal(context, response->client,
	    &out->client)))
	goto Done;
    if ((code = krb5_copy_principal(context, response->enc_part2->server,
	    &out->server)))
	goto Done;
    if ((code = krb5_copy_keyblock_contents(context,
	    response->enc_part2->session, &out->keyblock)))
	goto Done;
    out->ticket_flags = response->enc_part2->flags;
    out->times = response->enc_part2->times;
    out->is_skey = !!out->second_ticket.length;
    if (response->enc_part2->caddrs) addresses = response->enc_part2->caddrs;
    if ((code = krb5_copy_addresses(context, addresses, &out->addresses)))
	goto Done;
    if ((code = encode_krb5_ticket(response->ticket, &data)))
	goto Done;
    out->ticket = *data;
    free(data);
    data = 0;
    *out_cred = out;
    out = 0;
Done:
    krb5_free_kdc_rep(context, response);
    krb5_free_error(context, error);
    if (answer->response.data) free(answer->response.data);
    if (data) {
	if (data->data) free(data->data);
	free(data);
    }
    if (enctypes) free(enctypes);
    krb5_free_creds(context, out);
    return code;
}

#define SVSK	(KRB5_TC_MATCH_SRV_NAMEONLY|KRB5_TC_SUPPORTED_KTYPES)
#define FLAGS2OPTS(f)	((f)&KDC_TKT_COMMON_MASK)
#define TGTNAME(p,f,t) p->realm=f->realm,p->data=t->data,p->length=t->length

krb5_error_code krb5_get_cred_from_kdc_opt(krb5_context context,
    krb5_ccache cache,
    krb5_creds *in_cred, krb5_creds **out_creds, krb5_creds ***tgts,
    krb5_flags options)
{
    krb5_creds match[1], tgtc[1], *tgt, **out_tgts = 0;
    int code, i, j, k, n;
    krb5_principal *tgs_list;
    krb5_principal_data tgtprinc[1];

    *out_creds = 0;
    *tgts = 0;
    memset(tgtc, 0, sizeof *tgtc);
    k = 0;

    memset(match, 0, sizeof match);
    code = krb5_walk_realm_tree(context, &in_cred->client->realm,
	&in_cred->server->realm, &tgs_list, KRB5_REALM_BRANCH_CHAR);
    if (code) goto Done;
    for (i = 0; tgs_list[i]; ++i)
	;
    n = i-1;
    memset(tgtprinc, 0, sizeof *tgtprinc);
    TGTNAME(tgtprinc, tgs_list[0], tgs_list[n]);
    match->server = tgtprinc;
    match->client = in_cred->client;
    code = krb5_cc_retrieve_cred(context, cache, SVSK, match, tgtc);
    switch(code) {
    default: goto Done;
    case 0: tgt = tgtc; goto Fetch;
    case KRB5_CC_NOTFOUND: case KRB5_CC_NOT_KTYPE: break;
    }
    match->server = *tgs_list;
    code = krb5_cc_retrieve_cred(context, cache, SVSK, match, tgtc);
    if (code) goto Done;
    tgt = tgtc;
    code = ENOMEM;
    out_tgts = (krb5_creds **) malloc(i * sizeof sizeof *out_tgts);
    if (!out_tgts) goto Done;
    memset(out_tgts, 0, i * sizeof sizeof *out_tgts);
    for (i = 1, j = n; tgs_list[i] && j >= i; ) {
	TGTNAME(tgtprinc, tgs_list[i], tgs_list[j]);
	match->server = tgtprinc;
	code = krb5_cc_retrieve_cred(context, cache, SVSK, match, tgtc);
	switch(code) {
	default: goto Done;
	case 0: tgt = tgtc; goto Next;
	case KRB5_CC_NOTFOUND: case KRB5_CC_NOT_KTYPE: break;
	}
	if (!krb5_c_valid_enctype(tgt->keyblock.enctype)) {
	    code = KRB5_PROG_ETYPE_NOSUPP;
	    goto Done;
	}
	code = krb5_get_cred_via_tkt(context, tgt, FLAGS2OPTS(tgt->ticket_flags),
	    tgt->addresses, match, out_tgts+k);
	if (code) {
	    --j;
	    continue;
	}
	tgt = out_tgts[k];
	++k;
Next:
	i = j + 1; j = n;
	continue;
    }
    if (tgs_list[i]) goto Done;
Fetch:
    if (!krb5_c_valid_enctype(tgt->keyblock.enctype)) {
	code = KRB5_PROG_ETYPE_NOSUPP;
	goto Done;
    }
    options |= FLAGS2OPTS(tgt->ticket_flags);
    if (in_cred->second_ticket.length)
	options |= KDC_OPT_ENC_TKT_IN_SKEY;
    code = krb5_get_cred_via_tkt(context, tgt, options, tgt->addresses,
	in_cred, out_creds);
Done:
    if (out_tgts && k) {
	out_tgts[k] = 0;
	*tgts = out_tgts;
	out_tgts = 0;
    }
    if (out_tgts) {
#if 0	/* can't happen - above if will catch this */
	for (i = 0; out_tgts[i]; ++i)
	    krb5_free_creds(context, out_tgts[i]);
#endif
	free(out_tgts);
    }
    krb5_free_realm_tree(context, tgs_list);
    krb5_free_cred_contents(context, tgtc);
    return code;
}

krb5_error_code krb5_get_cred_from_kdc(krb5_context context,
    krb5_ccache cache,
    krb5_creds *in_creds, krb5_creds **out_creds, krb5_creds ***tgts)
{
    return krb5_get_cred_from_kdc_opt(context, cache, in_creds, out_creds, tgts, 0);
}

krb5_error_code
krb5_get_credentials(krb5_context context, krb5_flags options,
    krb5_ccache cache, krb5_creds *in_creds, krb5_creds **out_creds)
{
    krb5_creds match[1], *output, **tgts;
    krb5_flags fields;
    int code, r, i;

    *out_creds = 0;
    tgts = 0;
    memset(match, 0, sizeof *match);
    if (!in_creds || !in_creds->server || !in_creds->client)
	return EINVAL;
    match->times.endtime = in_creds->times.endtime
	? in_creds->times.endtime
	: time(0);
    match->keyblock = in_creds->keyblock;
    match->server = in_creds->server;
    match->client = in_creds->client;
    match->authdata = in_creds->authdata;
    if (!(output = malloc(sizeof *output)))
	return ENOMEM;
    memset(output, 0, sizeof *output);
    fields = 0;
    code = krb5_cc_retrieve_cred(context, cache, fields, match, output);
    switch(code) {
    case KRB5_CC_NOTFOUND:
    case KRB5_CC_NOT_KTYPE:
	if (!(options & KRB5_GC_CACHED)) break;
	/* fall through */
    default: goto Done;
    }
    krb5_free_creds(context, output);
    output = 0;
    r = krb5_get_cred_from_kdc(context, cache, in_creds, &output, &tgts);
    switch(r) {
    default:
	code = r;
	/* fall through */
    case KRB5_CC_NOTFOUND:
	break;
    }
    if (tgts) for (i = 0; tgts[i]; ++i) {
	r = krb5_cc_store_cred(context, cache, tgts[i]);
	if (r) code = r;
	krb5_free_creds(context, tgts[i]);
    }
    if (!code)
	(void) krb5_cc_store_cred(context, cache, output);
Done:
    if (code) {
	krb5_free_creds(context, output);
    } else *out_creds = output;
    return code;
}
