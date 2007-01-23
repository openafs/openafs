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
#include <netinet/in.h>
#include "k5ssl.h"
#include "k5s_im.h"

krb5_error_code
krb5_copy_authdata(krb5_context context,
    krb5_authdata *const *in,
    krb5_authdata ***out)
{
    krb5_authdata **x;
    int code;
    int i;

    if (!in) {
	*out = 0;
	return 0;
    }
    for (i = 0; in[i++]; )
	;
    code = ENOMEM;
    if (!(x = (krb5_authdata **)malloc(i * sizeof *x)))
	goto Done;
    x[--i] = 0;
    for (i = 0; in[i]; ++i) {
	if (!(x[i] = (krb5_authdata *) malloc(sizeof **x)))
	    goto Done;
	if (!(x[i]->contents = malloc(x[i]->length = in[i]->length))) {
	    x[i+1] = 0;
	    goto Done;
	}
	memcpy(x[i]->contents, in[i]->contents, x[i]->length);
	x[i]->ad_type = in[i]->ad_type;
    }
    *out = x;
    x = 0;
    code = 0;
Done:
    krb5_free_authdata(context, x);
    return code;
}

krb5_error_code
krb5_copy_creds(krb5_context context, const krb5_creds *in, krb5_creds **out)
{
    krb5_creds *t;
    int code;
    code = ENOMEM;
    t = malloc(sizeof *t);
    if (!t) goto Done;
    memset(t, 0, sizeof *t);
    if (in->ticket.length) {
	if (!(t->ticket.data = malloc(in->ticket.length)))
	    goto Done;
	memcpy(t->ticket.data,
	    in->ticket.data,
	    t->ticket.length = in->ticket.length);
    }
    if (in->second_ticket.length) {
	if (!(t->second_ticket.data = malloc(in->second_ticket.length)))
	    goto Done;
	memcpy(t->second_ticket.data,
	    in->second_ticket.data,
	    t->second_ticket.length = in->second_ticket.length);
    }
    t->times = in->times;
    t->is_skey = in->is_skey;
    t->ticket_flags = in->ticket_flags;
    code = krb5_copy_principal(context, in->client, &t->client);
    if (code) goto Done;
    code = krb5_copy_principal(context, in->server, &t->server);
    if (code) goto Done;
    code = krb5_copy_keyblock_contents(context, &in->keyblock, &t->keyblock);
    if (code) goto Done;
    code = krb5_copy_addresses(context, in->addresses, &t->addresses);
    if (code) goto Done;
    code = krb5_copy_authdata(context, in->authdata, &t->authdata);
    if (code) goto Done;
    *out = t;
    t = 0;
    code = 0;
Done:
    krb5_free_creds(context, t);
    return code;
}

const char krb5i_mcc_prefix[] = "MEMORY";

struct krb5_mcc_item {
    struct krb5_mcc_item *next;
    krb5_creds *creds;
};

struct krb5_mcc_data {
    krb5_principal princ;
    struct krb5_mcc_item *first;
    char name[sizeof krb5i_mcc_prefix];
};

struct krb5_mcc_cursor {
    struct krb5_mcc_item **pos;
};

void
krb5_mcc_free(krb5_context context, krb5_ccache cache)
{
    struct krb5_mcc_data *mdata = (struct krb5_mcc_data *) cache->data;
    struct krb5_mcc_item *d;

    while (d = mdata->first) {
	mdata->first = d->next;
	krb5_free_creds(context, d->creds);
	free(d);
    }
    krb5_free_principal(context, mdata->princ);
    mdata->princ = 0;
}

int
krb5i_mcc_resolve(krb5_context context, krb5_ccache cache, const char *name)
{
    struct krb5_mcc_data *mdata;
    int l;

    mdata = malloc(sizeof *mdata + (l = strlen(name)));
    if (!mdata) return ENOMEM;
    memset(mdata, 0, sizeof *mdata);
    memcpy(mdata->name, krb5i_mcc_prefix, sizeof krb5i_mcc_prefix - 1);
    mdata->name[sizeof krb5i_mcc_prefix - 1] = ':';
    memcpy(mdata->name + sizeof krb5i_mcc_prefix, name, l+1);
    cache->data = mdata;
    return 0;
}

int
krb5i_mcc_store_cred(krb5_context context, krb5_ccache cache,
    krb5_creds *creds)
{
    struct krb5_mcc_item **p, *d;
    struct krb5_mcc_data *mdata = (struct krb5_mcc_data *) cache->data;
    int code;

    for (p = &mdata->first; d = *p; p = &d->next)
	;
    code = ENOMEM;
    d = malloc(sizeof *d);
    if (!d) goto Done;
    memset(d, 0, sizeof *d);
    code = krb5_copy_creds(context, creds, &d->creds);
    if (code) goto Done;
    *p = d;
    d = 0;
    code = 0;
Done:
    if (d) {
	krb5_free_creds(context, d->creds);
	free(d);
    }
    return code;
}

int
krb5i_mcc_start_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_mcc_data *mdata = (struct krb5_mcc_data *) cache->data;
    struct krb5_mcc_cursor *mcursor;
    int code;

    *cursor = 0;
    mcursor = (struct krb5_mcc_cursor *)malloc(sizeof *mcursor);
    if (!mcursor)
	return ENOMEM;
    mcursor->pos = &mdata->first;
    *cursor = (krb5_cc_cursor) mcursor;
    return code;
}

int
krb5i_mcc_get_next(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor, krb5_creds *creds)
{
    struct krb5_mcc_cursor *mcursor = (struct krb5_mcc_cursor *) *cursor;
    struct krb5_mcc_item *d = *mcursor->pos;
    int code;
    krb5_creds *t = 0;

    memset(creds, 0, sizeof *creds);
    d = *mcursor->pos;
    if (!d) return KRB5_CC_END;
    code = krb5_copy_creds(context, d->creds, &t);
    mcursor->pos = &d->next;
    *creds = *t;
    free(t);
    return code;
}

int
krb5i_mcc_end_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_mcc_cursor *mcursor = (struct krb5_mcc_cursor *) *cursor;

    free(mcursor);
    return 0;
}

int
krb5i_mcc_get_principal(krb5_context context,
    krb5_ccache cache, krb5_principal *princ)
{
    struct krb5_mcc_data *mdata = (struct krb5_mcc_data *) cache->data;
    int code;

    *princ = 0;
    code = krb5_copy_principal(context, mdata->princ, princ);
    return code;
}

int
krb5i_mcc_close(krb5_context context, krb5_ccache cache)
{
    krb5_mcc_free(context, cache);
    free((struct krb5_mcc_data *) cache->data);
    return 0;
}

const char *
krb5i_mcc_get_name(krb5_context context, krb5_ccache cache)
{
    return ((struct krb5_mcc_data *) cache->data)->name;
}

krb5_error_code
krb5i_mcc_initialize(krb5_context context, krb5_ccache cache, krb5_principal princ)
{
    krb5_mcc_free(context, cache);
    return krb5_copy_principal(context,
	princ,
	& ((struct krb5_mcc_data *) cache->data)->princ);
}

const krb5_cc_ops krb5_mcc_ops = {
    krb5i_mcc_prefix,
    krb5i_mcc_resolve,
    krb5i_mcc_store_cred,
    krb5i_mcc_start_get,
    krb5i_mcc_get_next,
    krb5i_mcc_end_get,
    krb5i_mcc_get_principal,
    krb5i_mcc_close,
    krb5i_mcc_get_name,
    krb5i_mcc_initialize,
};
