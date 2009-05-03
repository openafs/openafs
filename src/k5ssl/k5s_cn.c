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

#include "k5s_config.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "k5ssl.h"
#include "k5s_im.h"

struct gic_work;
typedef int (*krb5_gic_get_as_key_fct)(krb5_context,
	krb5_principal, krb5_enctype, struct gic_work *);

void
krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *opt)
{
    memset(opt, 0, sizeof *opt);
}

void
krb5_get_init_creds_opt_set_forwardable(krb5_get_init_creds_opt *opt, int f)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
    opt->forwardable = f;
}

void
krb5_get_init_creds_opt_set_proxiable(krb5_get_init_creds_opt *opt, int f)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
    opt->proxiable = f;
}

void
krb5_get_init_creds_opt_set_address_list(krb5_get_init_creds_opt *opt,
    krb5_address **addrs)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
    opt->address_list = addrs;
}

void
krb5_get_init_creds_opt_set_tkt_life(krb5_get_init_creds_opt *opt,
    krb5_deltat lifetime)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->tkt_life = lifetime;
}

void
krb5_get_init_creds_opt_set_renew_life(krb5_get_init_creds_opt *opt,
    krb5_deltat lifetime)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->renew_life = lifetime;
}

void
krb5_get_init_creds_opt_set_etype_list(krb5_get_init_creds_opt *opt,
    krb5_enctype *list,
    int length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
    opt->etype_list = list;
    opt->etype_list_length = length;
}

void
krb5_free_salt_contents(krb5_context context, krb5_salt *salt)
{
    if (salt->s2kdata.data) {
	free(salt->s2kdata.data);
    }
    if (salt->s2kparams.data) {
	free(salt->s2kparams.data);
    }
    memset(salt, 0, sizeof *salt);
}

void
krb5_free_etype_info(krb5_context context, krb5_etype_info elist)
{
    int i;

    if (!elist) return;
    for (i = 0; elist[i]; ++i) {
	if (elist[i]->salt)
	    free(elist[i]->salt);
	if (elist[i]->s2kparams.data)
	    free(elist[i]->s2kparams.data);
	free(elist[i]);
    }
    free(elist);
}

int
krb5i_make_service(krb5_context context,
    char *service,
    krb5_principal client,
    krb5_principal *princp)
{
    int code;
    void *p;
    if (service) {
	if ((code = krb5_parse_name(context, service, princp)))
	    goto Done;
	if (krb5i_data_compare(&(*princp)->realm, &client->realm)) {
	    if ((*princp)->realm.length != client->realm.length) {
		p = malloc(client->realm.length);
		if (!p) {
		    errno = ENOMEM;
		    goto Done;
		}
		free((*princp)->realm.data);
		(*princp)->realm.length = client->realm.length;
		(*princp)->realm.data = p;
	    }
	    memcpy((*princp)->realm.data, client->realm.data, client->realm.length);
	}
    } else {
	if ((code = krb5_build_principal(context, princp,
		client->realm.length,
		client->realm.data,
		KRB5_TGS_NAME,
		client->realm.data,
		0)))
	    goto Done;
    }
Done:
    return code;
}

int
krb5i_send_as_request(krb5_context context,
    krb5_kdc_req *request,
    struct timeval *now,
    krb5_error **err_reply,
    krb5_kdc_rep **as_reply,
    int *flagsp)
{
    int code;
    krb5_kdc_rep *reply = 0;
    krb5_error *error = 0;
    krb5_data temp[1], *req = 0, rep[1];

    memset(rep, 0, sizeof *rep);
    temp->data = (void*) &request->nonce;
    temp->length = sizeof request->nonce;
    if ((code = krb5_c_random_make_octets(context, temp)))
	goto Done;
    if ((code = encode_krb5_as_req(request, &req)))
	goto Done;
    for (;;) {
	*err_reply = 0;
	code = krb5i_sendto_kdc(context, req, &request->server->realm,
	    rep, flagsp, now);
	if (code) goto Done;
	if (!krb5_is_krb_error(rep))
	    break;
	if ((code = decode_krb5_error(rep, &error)))
	    goto Done;
	if (error && error->error == KRB5KRB_ERR_RESPONSE_TOO_BIG && !(*flagsp & USE_TCP)) {
	    krb5_free_error(context, error);
	    continue;
	}
	if (error) {
	    if (err_reply) {
		*err_reply = error;
		error = 0;
	    }
	    goto Done;
	}
	break;
    }
    if (!krb5_is_as_rep(rep)) {
	if (rep->length >= 2 && 0[(unsigned char*)rep->data] == 4
		&& (~1&1[(unsigned char*)rep->data] == 10))
	    code = KRB5KRB_AP_ERR_V4_REPLY;
	else
	    code = KRB5KRB_AP_ERR_MSG_TYPE;
	goto Done;
    }
    if ((code = krb5i_decode_as_rep(rep, &reply)))
	goto Done;
    if (reply->msg_type != KRB5_AS_REP) {
	code = KRB5KRB_AP_ERR_MSG_TYPE;
	goto Done;
    }
    if (as_reply) {
	*as_reply = reply;
	reply = 0;
    }
    code = 0;
Done:
    if (req) {
	if (req->data) free(req->data);
	free(req);
    }
    if (rep->data) free(rep->data);
    krb5_free_error(context, error);
    krb5_free_kdc_rep(context, reply);
    return code;
}

struct gic_work {
    krb5_kdc_req *request;
    krb5_salt salt[1];
    krb5_enctype enctype;
    krb5_keyblock as_key[1];
    krb5_prompter_fct prompter;
    void *arg;
    krb5_gic_get_as_key_fct gak_fct;
    void *gak_arg;
    struct timeval now[1];
};

typedef int pa_function (krb5_context, struct gic_work *,
    krb5_pa_data *, krb5_pa_data **, krb5_pa_data **);

int
krb5i_pa_salt(krb5_context context,
    struct gic_work *work,
    krb5_pa_data *in_padata,
    krb5_pa_data **out_padata,
    krb5_pa_data **old_padata)
{
    unsigned char *afs = 0;

    if (work->salt->s2kdata.length != ~0) return 0;
    if (in_padata->pa_type == KRB5_PADATA_AFS3_SALT) {
	afs = malloc(1);
	if (!afs) return ENOMEM;
	*afs = 1;
    }
    work->salt->s2kdata.length = in_padata->length;
    work->salt->s2kdata.data = in_padata->contents;
    memset(in_padata, 0, sizeof *in_padata);
    work->salt->s2kparams.length = !!afs;
    work->salt->s2kparams.data = afs;
    return 0;
}

#ifdef KRB5_EXTENDED_SALTS
int
krb5i_pa_type_and_salt(krb5_context context,
    struct gic_work *work,
    krb5_pa_data *in_padata,
    krb5_pa_data **out_padata,
    krb5_pa_data **old_padata)
{
    int code;
    unsigned char *params = 0;
    krb5_data scratch[1];
    krb5_keyblock *sadt;

    if (work->salt->s2kdata.length != ~0) return 0;
    if (!in_padata->length) return 0;
    params = malloc(1);
    if (!params) {
	code = ENOMEM;
	goto Done;
    }
    scratch->length = in_padata->length;
    scratch->data = in_padata->contents;
    code = decode_krb5_encryption_key(scratch, &sadt);
    if (code) goto Done;
    work->salt->s2kdata.data = sadt->contents;
    work->salt->s2kdata.length = sadt->length;
    *params = sadt->enctype;
    work->salt->s2kparams.data = params;
    work->salt->s2kparams.length = 1;
    params = 0;
Done:
    if (params) free(params);
    if (sadt) free(sadt);
    return code;
}
#endif

int
krb5i_pa_enc_timestamp(krb5_context context,
    struct gic_work *work,
    krb5_pa_data *in_padata,
    krb5_pa_data **out_padata,
    krb5_pa_data **old_padata)
{
    int code;
    krb5_pa_enc_ts paplain[1];
    krb5_enc_data cipher[1];
    krb5_data *data = 0;
    krb5_pa_data *pa = 0;
    size_t enclen;

    memset(cipher, 0, sizeof *cipher);
    if (work->as_key->length)
	;
    else if ((code = (*work->gak_fct)(context,
	    work->request->client,
	    work->enctype, work)))
	goto Done;
    paplain->patimestamp = work->now->tv_sec;
    paplain->pausec = work->now->tv_usec;
    if ((code = encode_krb5_pa_enc_ts(paplain, &data)))
	goto Done;
    if ((code = krb5_c_encrypt_length(context, work->as_key->enctype,
	    data->length, &enclen)))
	goto Done;
    cipher->ciphertext.length = enclen;
    if (!(cipher->ciphertext.data = malloc(enclen))) {
	code = ENOMEM;
	goto Done;
    }
    if ((code = krb5_c_encrypt(context, work->as_key,
	    KRB5_KEYUSAGE_AS_REQ_PA_ENC_TS, 0,
	    data, cipher)))
	goto Done;
    free(data->data);
    free(data);
    data = 0;
    if ((code = encode_krb5_enc_data(cipher, &data))) {
	goto Done;
    }
    if (!(pa = malloc(sizeof *pa))) {
	code = ENOMEM;
	goto Done;
    }
    pa->pa_type = KRB5_PADATA_ENC_TIMESTAMP;
    pa->length = data->length;
    pa->contents = data->data;
    data->data = 0;
    *out_padata = pa;
Done:
    if (data) {
	if (data->data) free(data->data);
	free(data);
    }
    if (cipher->ciphertext.data) {
	free(cipher->ciphertext.data);
    }
    return code;
}

int
krb5i_pa_etype_info(krb5_context context,
    struct gic_work *work,
    krb5_pa_data *in_padata,
    krb5_pa_data **out_padata,
    krb5_pa_data **old_padata)
{
    int code;
    krb5_data scratch[1];
    krb5_etype_info elist = 0;
    int i, j;

    if (work->salt->s2kdata.length != ~0) return 0;
    scratch->length = in_padata->length;
    scratch->data = in_padata->contents;
    code = decode_krb5_etype_info(scratch, &elist);
    if (code) goto Done;
    if (!elist || !*elist) goto Done;
    for (i = 0; i < work->request->nktypes; ++i)
	for (j = 0; elist[j]; ++j)
	    if (work->request->ktype[i] == elist[j]->etype) {
		work->enctype = elist[j]->etype;
		work->salt->s2kdata.data = elist[j]->salt;
		work->salt->s2kdata.length = elist[j]->length;
		work->salt->s2kparams = elist[j]->s2kparams;
		memset(elist[j], 0, sizeof**elist);
		if (!work->salt->s2kdata.length
			&& !work->salt->s2kparams.length)
		    code = krb5_principal2salt(context, work->request->client,
			work->salt);
		goto Done;
	    }
    for (j = 0; elist[j]; ++j)
	if (krb5_c_valid_enctype(elist[j]->etype))
	    break;
    code = elist[j] ? KRB5_CONFIG_ETYPE_NOSUPP : KRB5_PROG_ETYPE_NOSUPP;
Done:
    krb5_free_etype_info(context, elist);
    return code;
}

struct pa_types {
    int type;
    pa_function *f;
    int flags;
};

#define PA_FIRST 4
#define PA_INFO 4
#define PA_INFO1 2
#define PA_REAL 1
const struct pa_types krb5i_pa_types[] = {
    { KRB5_PADATA_PW_SALT, krb5i_pa_salt, PA_INFO },
    { KRB5_PADATA_AFS3_SALT, krb5i_pa_salt, PA_INFO },
#ifdef KRB5_EXTENDED_SALTS
    { KRB5_PADATA_PW_TYPE_AND_SALT, krb5i_pa_type_and_salt, PA_INFO },
#endif
    { KRB5_PADATA_ENC_TIMESTAMP, krb5i_pa_enc_timestamp, PA_REAL },
    { KRB5_PADATA_ETYPE_INFO, krb5i_pa_etype_info, PA_INFO1 },
    { KRB5_PADATA_ETYPE_INFO2, krb5i_pa_etype_info, PA_INFO },
{ 0,0,0 } };

int
krb5i_do_preauth(krb5_context context,
    krb5_kdc_req *request,
    krb5_pa_data **in_padata,
    krb5_pa_data ***out_padata,
    struct gic_work *work,
    krb5_pa_data **old_padata)
{
    int code = 0;
    int h, i, j;
    krb5_pa_data *patemp, **pa_list;
    struct dp_work {
	krb5_pa_data *pa[2];
	struct dp_work *next;
    } *dplist = 0, *p, **pp = &dplist;

    if (out_padata) *out_padata = 0;
    if (!in_padata) return 0;
	/* move client selected padata first... */
    if (old_padata) {
	int k;
	j = 0;
	for (h = 0; old_padata[h]; ++h) {
	    for (i = 0; in_padata[i]; ++i) {
		if (old_padata[h]->pa_type != in_padata[i]->pa_type) continue;
		patemp = in_padata[i];
		for (k = i; k > j; ) {
		    --k;
		    in_padata[k+1] = in_padata[k];
		}
		in_padata[j++] = patemp;
		break;
	    }
	}
    }
    p = 0;
    for (h = PA_FIRST; h; h >>= 1) {
	for (i = 0; in_padata[i]; ++i)
	    for (j = 0; krb5i_pa_types[j].f; ++j) {
		if (in_padata[i]->pa_type != krb5i_pa_types[j].type) continue;
		if (!(krb5i_pa_types[j].flags & h)) break;
		if (out_padata) {
		    if (p)
			;
		    else if (!(p = malloc(sizeof *p))) {
			code = ENOMEM;
			goto Done;
		    }
		    else {
			memset(p, 0, sizeof *p);
			*pp = p;
			pp = &p->next;
		    }
		}
		code = (*krb5i_pa_types[j].f)(context,
		    work,
		    in_padata[i],
		    out_padata ? p->pa : 0,
		    old_padata);
		if (code || h == PA_REAL) goto Done;
		if (p && *p->pa)
		    p = 0;
	    }
    }
Done:
    if (!code && out_padata) {
	for (j = 1, p = dplist; p; p = p->next)
	    if (*p->pa)
		++j;
	if ((pa_list = malloc(sizeof *pa_list * j))) {
	    *out_padata = pa_list;
	    for (i = 0, p = dplist; p; p = p->next)
		if (*p->pa) {
		    pa_list[i++] = *p->pa;
		    *p->pa = 0;
		}
	    pa_list[i] = 0;
	} else code = ENOMEM;
    }
    while (p = dplist) {
	dplist = p->next;
	krb5_free_pa_data(context, p->pa);
    }
    return code;
}

int
krb5i_make_preauth_list(krb5_context context,
    krb5_preauthtype *ptypes,
    int nptypes,
    krb5_pa_data *** pa_datap)
{
    krb5_pa_data ** padata;
    int code;
    int i;

    code = ENOMEM;
    padata = (krb5_pa_data **)malloc(sizeof *padata * (nptypes + 1));
    if (!padata) goto Done;
    for (i = 0; i < nptypes; ++i) {
	if (!(padata[i] = malloc(sizeof **padata)))
	    goto Done;
	memset(padata[i], 0, sizeof **padata);
	padata[i]->pa_type = ptypes[i];
    }
    padata[i] = 0;
    *pa_datap = padata;
    padata = 0;
    code = 0;
Done:
    krb5_free_pa_data(context, padata);
    return code;
}

int
krb5i_decrypt_as_reply(krb5_context context,
    struct gic_work *work,
    krb5_kdc_rep *r)
{
    int code;
    static const krb5_keyusage usage[1] = {KRB5_KEYUSAGE_AS_REP_ENCPART};
    if (!work->as_key->length) {
printf ("no key?\n");
	return 666;
    }
    code = krb5_kdc_rep_decrypt_proc(context, work->as_key,
	usage, r);
Done:
    return code;
}

int
krb5i_verify_as_reply(krb5_context context,
    struct gic_work *work,
    krb5_kdc_req *q,
    krb5_kdc_rep *r)
{
    int code;
    int clockskew;

    (void) krb5_get_clockskew(context, &clockskew);
    if (!r || !r->enc_part2) {
if (!r) printf ("no r\n");
else if (!r->enc_part2) printf ("no enc_part2\n");
	return 666;
    }
    if (!r->enc_part2->times.starttime)
	r->enc_part2->times.starttime =
	    r->enc_part2->times.authtime;
    if (!krb5_principal_compare(context, q->client, r->client)
	    || !krb5_principal_compare(context, q->server, r->ticket->server)
	    || !krb5_principal_compare(context, q->server, r->enc_part2->server)
	    || q->nonce != r->enc_part2->nonce
	    || ((q->kdc_options & KDC_OPT_POSTDATED) &&
		    q->from && q->from != r->enc_part2->times.starttime)
	    || (q->till && q->till < r->enc_part2->times.endtime)
	    || ((q->kdc_options & KDC_OPT_RENEWABLE) &&
		    q->rtime && q->rtime !=
			    r->enc_part2->times.renew_till)
	    || ((q->kdc_options & (KDC_OPT_RENEWABLE_OK|KDC_OPT_RENEWABLE))
			!= KDC_OPT_RENEWABLE_OK
		    && (r->enc_part2->flags & KDC_OPT_RENEWABLE)
		    && q->till && q->till !=
			r->enc_part2->times.renew_till))
	return KRB5_KDCREP_MODIFIED;
    if (!q->from
	    && labs(r->enc_part2->times.starttime - work->now->tv_sec)
		> clockskew) {
	return KRB5_KDCREP_SKEW;
    }
    return 0;
}

int
krb5i_stash_as_reply(krb5_context context,
    krb5_kdc_rep *as_reply,
    krb5_creds *creds)
{
    int code;
    krb5_data *data = 0;

    memset(creds, 0, sizeof *creds);
    if ((code = krb5_copy_principal(context, as_reply->client, &creds->client)))
	goto Done;
    if ((code = krb5_copy_principal(context, as_reply->ticket->server,
	    &creds->server)))
	goto Done;
    if ((code = krb5_copy_keyblock_contents(context,
	    as_reply->enc_part2->session, &creds->keyblock)))
	goto Done;
    if ((code = krb5_copy_addresses(context, as_reply->enc_part2->caddrs,
	    &creds->addresses)))
	goto Done;
    if ((code = encode_krb5_ticket(as_reply->ticket, &data)))
	goto Done;
    creds->ticket = *data;
    free(data);
    data = 0;
    creds->times = as_reply->enc_part2->times;
    creds->ticket_flags = as_reply->enc_part2->flags;
Done:
    if (code) krb5_free_cred_contents(context, creds);
    if (data) {
	if (data->data) free(data->data);
	free(data);
    }
    return code;
}

int
krb5i_get_init_creds(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    krb5_prompter_fct prompter,
    void *arg,
    krb5_deltat stime,
    char *service,
    krb5_get_init_creds_opt *options,
    krb5_gic_get_as_key_fct gak_fct,
    void *gak_arg,
    int *flagsp,
    krb5_kdc_rep **as_reply_p)
{
    int i;
    int code;
    krb5_kdc_req request[1];
    krb5_pa_data **padata = 0, **old_padata = 0;
    krb5_kdc_rep *as_reply = 0;
    krb5_error *err_reply = 0;
    krb5_deltat lifetime, renew_life;
    int temp;
    krb5_get_init_creds_opt default_options[1];
    struct gic_work work[1];
    krb5_enctype *ktypes = 0;
    krb5_address **addrs = 0;
    krb5_preauthtype *ptypes = 0, *pt;
    int nptypes;

    memset(work, 0, sizeof *work);
    memset(request, 0, sizeof *request);
    work->salt->s2kdata.length = ~0;
    work->prompter = prompter;
    work->arg = arg;
    work->gak_fct = gak_fct;
    work->gak_arg = gak_arg;
    work->request = request;
    if (!options) {
	memset(default_options, 0, sizeof *default_options);
	options = default_options;
    }
    /* XXX request->msg_type = KRB5_AS_REQ; */
    code = krb5_get_kdc_default_options(context, &temp);
    if (code) goto Done;
    request->kdc_options = temp;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
	temp = options->forwardable;
    else if ((code = krb5_get_forwardable(context, &temp)))
	goto Done;
    if (temp)
	request->kdc_options |= KDC_OPT_FORWARDABLE;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
	temp = options->proxiable;
    else if ((code = krb5_get_proxiable(context, &temp)))
	goto Done;
    if (temp)
	request->kdc_options |= KDC_OPT_PROXIABLE;
    if (stime)
	request->kdc_options |= KDC_OPT_ALLOW_POSTDATE|KDC_OPT_POSTDATED;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
	lifetime = options->tkt_life;
    else if ((code = krb5_get_ticket_lifetime(context, &client->realm, &lifetime)))
	goto Done;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)
	renew_life = options->renew_life;
    else if ((code = krb5_get_renew_lifetime(context, &client->realm, &renew_life)))
	goto Done;
    gettimeofday(work->now, 0);
    request->from = work->now->tv_sec + stime;
    request->till = request->from + lifetime;
    if (renew_life)
	request->rtime = request->from + renew_life;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST) {
	request->ktype = options->etype_list;
	request->nktypes = options->etype_list_length;
    } else if ((code = krb5_get_tkt_ktypes(context, client, &ktypes)))
	goto Done;
    else {
	request->ktype = ktypes;
	while (request->ktype[request->nktypes])
	    ++request->nktypes;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST)
	request->addresses = options->address_list;
    else {
	if ((code = krb5_get_noaddresses(context, &temp)))
	    goto Done;
	if (temp) {
	    if ((code = krb5_os_localaddr(context, &addrs)))
		goto Done;
	    request->addresses = addrs;
	}
    }
    request->client = client;
    code = krb5i_make_service(context, service, client, &request->server);
    pt = 0;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST) {
	pt = options->preauth_list;
	nptypes = options->preauth_list_length;
    } else {
	if ((code = krb5_get_preauth_list(context, client, &ptypes, &nptypes)))
	    goto Done;
	pt = ptypes;
    }
    if (pt && (code = krb5i_make_preauth_list(context, pt, nptypes, &padata)))
	goto Done;
#define MAX_IN_TKT_LOOPS 16
    for (i = 0;; ++i) {
	if (i >= MAX_IN_TKT_LOOPS) {
	    code = KRBKRB5_GET_IN_TKT_LOOP;
	    goto Done;
	}
	krb5_free_pa_data(context, request->padata);
	request->padata = 0;
	code = krb5i_do_preauth(context, request,
	    padata, &request->padata, work, old_padata);
	if (code) goto Done;
	krb5_free_pa_data(context, old_padata);
	old_padata = padata;
	padata = 0;
	code = krb5i_send_as_request(context, request, work->now, &err_reply,
	    &as_reply, flagsp);
	if (code) goto Done;
	if (err_reply) {
	    if (err_reply->error == KRB5KDC_ERR_PREAUTH_REQUIRED
			- ERROR_TABLE_BASE_krb5
		    && err_reply->e_data.length > 0) {
		if ((code = decode_krb5_padata_sequence(&err_reply->e_data,
			&padata)))
		    goto Done;
		continue;
	    }
	    code = err_reply->error + ERROR_TABLE_BASE_krb5;
	    if (!code) code = ENXIO;
	    goto Done;
	} else break;
    }
    if (!as_reply) {
	code = KRB5KRB_AP_ERR_MSG_TYPE;
	goto Done;
    }
    work->enctype = as_reply->enc_part.enctype;
    code = krb5i_do_preauth(context, request,
	as_reply->padata, 0, work, old_padata);
    if (code) goto Done;
    code = (*gak_fct)(context, request->client, work->enctype,
	work);
    if (code) goto Done;
    code = krb5i_decrypt_as_reply(context, work, as_reply);
    if (code) goto Done;
    code = krb5i_verify_as_reply(context, work, request, as_reply);
    if (code) goto Done;
    code = krb5i_stash_as_reply(context, as_reply, creds);
    if (code) goto Done;
Done:
    krb5_free_principal(context, request->server);
    krb5_free_error(context, err_reply);
    krb5_free_pa_data(context, old_padata);
    krb5_free_pa_data(context, padata);
    krb5_free_pa_data(context, request->padata);
    if (as_reply_p)
	*as_reply_p = as_reply;
    else
	krb5_free_kdc_rep(context, as_reply);
    krb5_free_salt_contents(context, work->salt);
    krb5_free_keyblock_contents(context, work->as_key);
    if (ktypes) free(ktypes);
    krb5_free_addresses(context, addrs);
    if (ptypes) free(ptypes);
    return code;
}

int
krb5i_get_as_pw(krb5_context context,
    krb5_principal client,
    krb5_enctype etype,
    struct gic_work *work)
{
    krb5_data *passdata = (krb5_data *) work->gak_arg;
    int code;
    int l;
    char *user = 0;
    krb5_prompt prompt[1];
    krb5_salt tempsalt[1], *salt;

    memset(prompt, 0, sizeof *prompt);
    memset(tempsalt, 0, sizeof *tempsalt);
    if (work->as_key->length) return 0;
    if (!0[(unsigned char*) passdata->data]) {
	if (!work->prompter) {
	    code = EIO;
	    goto Done;
	}
	code = krb5_unparse_name(context, client, &user);
	if (code) goto Done;
	prompt->prompt = malloc((l = strlen(user) + 80));
	if (!prompt->prompt) {
	    code = ENOMEM;
	    goto Done;
	}
	snprintf(prompt->prompt, l, "Password for %s", user);
	prompt->hidden = 1;
	prompt->reply = passdata;
	prompt->type = KRB5_PROMPT_TYPE_PASSWORD;
	code = work->prompter(context, work->arg, NULL, NULL, 1, prompt);
	if (code) goto Done;
    }
    salt = work->salt;
    if (salt->s2kdata.length == ~0) {
	code = krb5_principal2salt(context, client, tempsalt);
	if (code) goto Done;
	salt = tempsalt;
    }
    code = krb5_c_string_to_key(context, etype, passdata,
	salt, work->as_key);
Done:
    if (user) free(user);
    if (prompt->prompt) free(prompt->prompt);
    krb5_free_salt_contents(context, tempsalt);
    return code;
}

krb5_error_code
krb5_get_init_creds_password(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    char *password,
    krb5_prompter_fct prompter,
    void *arg,
    krb5_deltat stime,
    char *service,
    krb5_get_init_creds_opt *options)
{
    int code;
    krb5_data pw0[1];
    char pw0data[1024];
    krb5_kdc_rep *as_reply = 0;
    int flags = 0;

    if (password && *password) {
	pw0->data = password;
	pw0->length = strlen(password);
    } else {
	memset(pw0data, 0, sizeof pw0data);
	pw0->data = pw0data;
	pw0->length = sizeof pw0data;
    }
    code = krb5i_get_init_creds(context, creds, client, prompter,
	arg, stime, service, options,
	krb5i_get_as_pw, (void *) pw0, &flags, &as_reply);
Done:
    krb5_free_kdc_rep(context, as_reply);
    if (!password || !*password)
	memset(pw0data, 0, sizeof pw0data);
    return code;
}

int
krb5i_get_as_kt(krb5_context context,
    krb5_principal client,
    krb5_enctype etype,
    struct gic_work *work)
{
    krb5_keytab keytab = (krb5_keytab) work->gak_arg;
    int code;
    krb5_keytab_entry ktentry[1];
    krb5_keyblock *key;

    if (!krb5_c_valid_enctype(etype))
	return KRB5_PROG_ETYPE_NOSUPP;
    if (!work->as_key->length)
	;
    else if (work->as_key->enctype == etype)
	return 0;
    else {
	krb5_free_keyblock_contents(context, work->as_key);
	memset(work->as_key, 0, sizeof *work->as_key);
    }
    code = krb5_kt_get_entry(context, keytab, client,
	0,	/* XXX no kvno? */
	etype,
	ktentry);
    if (code) return code;
    *work->as_key = ktentry->key;	/* steal key storage */
    ktentry->key.contents = 0;
    krb5_free_keytab_entry_contents(context, ktentry);
    return 0;
}

krb5_error_code
krb5_get_init_creds_keytab(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    krb5_keytab akeytab,
    krb5_deltat stime,
    char *service,
    krb5_get_init_creds_opt *options)
{
    int code, i;
    krb5_keytab keytab;
    int use_master;

    if (!(keytab = akeytab) && (code = krb5_kt_default(context, &keytab)))
	goto Done;
    for (use_master = 0; use_master <= USE_MASTER; use_master += USE_MASTER) {
	i = krb5i_get_init_creds(context, creds, client, NULL,
	    NULL, stime, service, options,
	    krb5i_get_as_kt, (void *) keytab, &use_master, NULL);
	switch(i) {
	default:
	    code = i;
	    continue;
	case KRB5_KDC_UNREACH:
	case KRB5_REALM_CANT_RESOLVE:
	    if (!use_master)
		code = i;
	    continue;
	case 0:
	    code = i;	/* fall through */
	    break;
	}
	break;
    }
Done:
    if (!akeytab)
	krb5_kt_close(context, keytab);
    return code;
}

int
krb5i_validate_or_renew_creds(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    krb5_ccache cache,
    char *service,
    int flag)
{
    int code;
    int i;
    krb5_creds mcreds[1];
    krb5_creds **tgts = 0, *out_cred = 0;

    memset(mcreds, 0, sizeof *mcreds);
    code = krb5i_make_service(context, service, client, &mcreds->server);
    if (code) goto Done;
    code = krb5_get_cred_from_kdc_opt(context, cache,
	mcreds, &out_cred, &tgts, flag);
    if (code) goto Done;
    if (out_cred) {
	*creds = *out_cred;
	free(out_cred);
	out_cred = 0;
    }
Done:
    if (tgts) {
	for (i = 0; tgts[i]; ++i)
	    krb5_free_creds(context, tgts[i]);
	free(tgts);
    }
    krb5_free_creds(context, out_cred);
    krb5_free_principal(context, mcreds->server);
    return code;
}

krb5_error_code
krb5_get_renewed_creds(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    krb5_ccache cache,
    char *service)
{
    return krb5i_validate_or_renew_creds(context, creds, client, cache,
	service, KDC_OPT_RENEW);
}

krb5_error_code
krb5_get_validated_creds(krb5_context context,
    krb5_creds *creds,
    krb5_principal client,
    krb5_ccache cache,
    char *service)
{
    return krb5i_validate_or_renew_creds(context, creds, client, cache,
	service, KDC_OPT_VALIDATE);
}
