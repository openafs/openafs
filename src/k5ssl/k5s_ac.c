/*
 * Copyright (c) 2007
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

/*
 * interface glue to MacOSX ticket cache.  Use dlopen like
 * kth & commercial vendors.
 *
 * Note: while this is apparently the "official" way
 * for foreign kerberos implementations to access the
 * kerberos credentials cache, it would be better to use
 * the native Kerberos library straight.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "k5ssl.h"
#include "k5s_im.h"
#include <dlfcn.h>
#include "Kerberos/CredentialsCache2.h"

#ifdef __APPLE__
#define KRB5_CC_LIBNAME "/System/Library/Frameworks/Kerberos.framework/Kerberos"
#else
#define KRB5_CC_LIBNAME "/usr/lib/libkrb5_cc.so"
#endif

typedef (*cc_initialize_func)(cc_context_t*,cc_int32,cc_int32*,char const**);

static const char krb5i_acc_prefix[] = "API";

struct krb5_acc_data {
    void *dlhandle;
    cc_context_t context;
    cc_ccache_t ccache;
    int state;
    char name[sizeof krb5i_acc_prefix];
};

struct krb5_acc_cursor {
    cc_credentials_iterator_t iter;
};

krb5_error_code
krb5i_ccred_xlate_err(cc_int32 ccerr)
{
    switch(ccerr) {
    case ccNoError: return 0;
    default: return KRB5_FCC_INTERNAL;
    case ccErrBadName: return KRB5_CC_BADNAME;
    case ccErrCredentialsNotFound: return KRB5_CC_NOTFOUND;
    case ccErrCCacheNotFound: return KRB5_FCC_NOFILE;
    case ccErrContextNotFound: return KRB5_CC_NOTFOUND;
    case ccIteratorEnd: return KRB5_CC_END;
    case ccErrNoMem: return KRB5_CC_NOMEM;
    case ccErrServerUnavailable: return KRB5_CC_NOSUPP;
    }
}

void
krb5i_free_ccred_contents(cc_credentials_v5_t *c)
{
    int i;

    if (c->authdata) {
	for (i = 0; c->authdata[i]; ++i) {
	    if (c->authdata[i]->data) free(c->authdata[i]->data);
	    free(c->authdata[i]);
	}
	free(c->authdata);
    }
    if (c->addresses) {
	for (i = 0; c->addresses[i]; ++i) {
	    if (c->addresses[i]->data) free(c->addresses[i]->data);
	    free(c->addresses[i]);
	}
	free(c->addresses);
    }
    if (c->server) free(c->server);
    if (c->client) free(c->client);
}

int
krb5i_ccred_getdata(const cc_data *cd, krb5_data *data)
{
    data->length = cd->length;
    if (!(data->data = malloc(data->length))) return ENOMEM;
    memcpy(data->data, cd->data, data->length);
    return 0;
}

int
krb5i_ccred_getaddr(const cc_data *cd, krb5_address *ad)
{
    int code;
    krb5_data d[1];

    if ((code = krb5i_ccred_getdata(cd, d))) return code;
    ad->addrtype = cd->type;
    ad->contents = d->data;
    ad->length = d->length;
    return code;
}

int
krb5i_ccred_getauth(const cc_data *cd, krb5_authdata *ad)
{
    int code;
    krb5_data d[1];

    if ((code = krb5i_ccred_getdata(cd, d))) return code;
    ad->ad_type = cd->type;
    ad->contents = d->data;
    ad->length = d->length;
    return code;
}

int
krb5i_ccred_getarray(const void **in, int (*f)(), int z, void ***out)
{
    int code;
    int i, s;
    void *p;

    if (!in) {
	*out = 0;
	return 0;
    }
    for (s = 0; in[s]; ++s)
	;
    *out = malloc((s+1) * sizeof *out);
    if (!*out) goto Failed;
    for (i = 0; i < s; ++i) {
	if (!(i[*out] = p = malloc(z)))
	    goto Failed;
	memset(i[*out], 0, z);
    }
    i[*out] = 0;
    code = 0;
    for (i = 0; i < s; ++i) {
	if ((code = (*f)(i[in], i[*out])))
	    goto Failed;
    }
Failed:
    return code;
}

int
krb5i_ccred_2_cred(krb5_context context,
    const cc_credentials_v5_t *ccred,
    krb5_creds *creds)
{
    int code;
    krb5_creds c[1];

    memset(c, 0, sizeof *c);
    if ((code = krb5_parse_name(context, ccred->client, &c->client)))
	goto Done;
    if ((code = krb5_parse_name(context, ccred->server, &c->server)))
	goto Done;
    c->keyblock.enctype = ccred->keyblock.type;
    c->keyblock.length = ccred->keyblock.length;
    if (!(c->keyblock.contents = malloc(c->keyblock.length))) {
	code = ENOMEM;
	goto Done;
    }
    memcpy(c->keyblock.contents, ccred->keyblock.data, c->keyblock.length);
    c->times.authtime = ccred->authtime;
    c->times.starttime = ccred->starttime;
    c->times.endtime = ccred->endtime;
    c->times.renew_till = ccred->renew_till;
    c->is_skey = ccred->is_skey;;
    c->ticket_flags = ccred->ticket_flags;
    if ((code = krb5i_ccred_getarray((const void **) ccred->addresses, krb5i_ccred_getaddr, sizeof **c->addresses, (void ***) &c->addresses)))
	goto Done;
    if ((code = krb5i_ccred_getdata(&ccred->ticket, &c->ticket)))
	goto Done;
    if ((code = krb5i_ccred_getdata(&ccred->second_ticket, &c->second_ticket)))
	goto Done;
    if ((code = krb5i_ccred_getarray((const void **) ccred->authdata, krb5i_ccred_getauth, sizeof **c->authdata, (void ***) &c->authdata)))
	goto Done;
    *creds = *c;
    memset(c, 0, sizeof *c);
    code = 0;
Done:
    krb5_free_cred_contents(context, c);
    return code;
}

int
krb5i_ccred_putdata(cc_data *cd, const krb5_data *data)
{
    int code;
    cd->length = data->length;
    if (!(cd->data = malloc(cd->length))) {
	return ENOMEM;
    }
    memcpy(cd->data, data->data, cd->length);
    return 0;
}

int
krb5i_ccred_putaddr(cc_data *cd, const krb5_address *ad)
{
    krb5_data d[1];
    cd->type = ad->addrtype;
    d->data = ad->contents;
    d->length = ad->length;
    return krb5i_ccred_putdata(cd, d);
}

int
krb5i_ccred_putauth(cc_data *cd, const krb5_authdata *ad)
{
    krb5_data d[1];
    cd->type = ad->ad_type;
    d->data = ad->contents;
    d->length = ad->length;
    return krb5i_ccred_putdata(cd, d);
}

int
krb5i_ccred_putarray(cc_data ***outp, int (*f)(), const void **in)
{
    int i;
    int code;
    cc_data **out = 0;

    if (!in) {
	*outp = 0;
	return 0;
    }
    for (i = 0; in[i]; ++i)
	;
    code = ENOMEM;
    out = malloc(sizeof *out * (i+1));
    if (!out)
	goto Done;
    for (i = 0; in[i]; ++i) {
	if (!(out[i] = malloc(sizeof **out))) goto Done;
	memset(out[i], 0, sizeof **out);
    }
    out[i] = 0;
    for (i = 0; in[i]; ++i) {
	if ((code = (*f)(out[i], in[i])))
	    goto Done;
    }
    *outp = out;
    out = 0;
    code = 0;
Done:
    if (out) {
	for (i = 0; out[i]; ++i) {
	    if (out[i]->data) free(out[i]->data);
	    free(out[i]);
	}
	free(out);
    }
    return code;
}

int
krb5i_cred_2_ccred(krb5_context context,
    const krb5_creds *creds,
    cc_credentials_v5_t *ccred)
{
    int code;
    cc_credentials_v5_t c[1];

    memset(c, 0, sizeof *c);
    if ((code = krb5_unparse_name(context, creds->client, &c->client)))
	goto Done;
    if ((code = krb5_unparse_name(context, creds->server, &c->server)))
	goto Done;
    c->keyblock.type = creds->keyblock.enctype;
    c->keyblock.length = creds->keyblock.length;
    if (!(c->keyblock.data = malloc(c->keyblock.length))) {
	code = ENOMEM;
	goto Done;
    }
    memcpy(c->keyblock.data, creds->keyblock.contents, c->keyblock.length);
    c->authtime = creds->times.authtime;
    c->starttime = creds->times.starttime;
    c->endtime = creds->times.endtime;
    c->renew_till = creds->times.renew_till;
    c->is_skey = creds->is_skey;
    c->ticket_flags = creds->ticket_flags;
    if ((code = krb5i_ccred_putarray(&c->addresses, krb5i_ccred_putaddr,
	    (const void **) creds->addresses)))
	goto Done;
    if ((code = krb5i_ccred_putdata(&c->ticket, &creds->ticket)))
	goto Done;
    if ((code = krb5i_ccred_putdata(&c->second_ticket, &creds->second_ticket)))
	goto Done;
    if ((code = krb5i_ccred_putarray(&c->authdata, krb5i_ccred_putauth,
	    (const void **) creds->authdata)))
	goto Done;
    *ccred = *c;
    memset(c, 0, sizeof *c);
    code = 0;
Done:
    krb5i_free_ccred_contents(c);
    return code;
}

int
krb5i_acc_resolve(krb5_context context, krb5_ccache cache, const char *name)
{
    struct krb5_acc_data *ddata = 0;
    int code;
    cc_int32 x;
    unsigned int l;
    const char * names[3];
    char *lib, **strings = 0;
    cc_string_t n = 0;
    void *dlhandle = 0;
    cc_initialize_func cc_init;

    names[0] = "libdefaults";
    names[1] = "ccapi_library";
    names[2] = 0;
    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    if (strings && *strings)
	lib = *strings;
    else
	lib = KRB5_CC_LIBNAME;
    code = KRB5_CC_NOSUPP;
    dlhandle = dlopen(lib, 0);
    if (!dlhandle) goto Done;
    cc_init = dlsym(dlhandle, "cc_initialize");
    if (!cc_init) goto Done;

    if (name && *name)
	l = strlen(name);
    else l = 500;
    ddata = malloc(sizeof *ddata + l + 1);
    if (!ddata) {
	code = ENOMEM;
	goto Done;
    }
    memset(ddata, 0, sizeof *ddata);
    if ((x = (*cc_init)(&ddata->context,
	    ccapi_version_3, NULL, NULL))) {
	code = krb5i_ccred_xlate_err(x);
	goto Done;
    }
    if (name && *name) {
	memcpy(ddata->name + sizeof krb5i_acc_prefix, name, l);
	x = cc_context_open_ccache(ddata->context, name, &ddata->ccache);
	ddata->state = krb5i_ccred_xlate_err(x);
    } else {
	x = cc_context_open_default_ccache(ddata->context, &ddata->ccache);
	if (x) {
	    ddata->state = krb5i_ccred_xlate_err(x);
	    x = cc_context_get_default_ccache_name(ddata->context, &n);
	} else {
	    x = cc_ccache_get_name(ddata->ccache, &n);
	}
	if (x) {
	    code = krb5i_ccred_xlate_err(x);
	    goto Done;
	}
	snprintf(ddata->name + sizeof krb5i_acc_prefix,
	    l - sizeof krb5i_acc_prefix,
	    "%s", n->data);
    }
    memcpy(ddata->name, krb5i_acc_prefix, sizeof krb5i_acc_prefix - 1);
    ddata->name[sizeof krb5i_acc_prefix - 1] = ':';
    ddata->name[sizeof krb5i_acc_prefix + l] = 0;
    ddata->dlhandle = dlhandle;
    dlhandle = 0;
    cache->data = ddata;
    ddata = 0;
    code = 0;
Done:
    if (n) cc_string_release(n);
    if (ddata) {
	if (ddata->ccache)
	    cc_ccache_release(ddata->ccache);
	if (ddata->context)
	    cc_context_release(ddata->context);
	free(ddata);
    }
    if (strings) free(strings);
    if (dlhandle) dlclose(dlhandle);
    return code;
}

int
krb5i_acc_store_cred(krb5_context context, krb5_ccache cache,
    krb5_creds *creds)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;
    cc_credentials_union u[1];
    cc_credentials_v5_t v5[1];
    int code;
    cc_int32 x;

    if (ddata->state) return ddata->state;
    if (!ddata->ccache) return KRB5_FCC_INTERNAL;
    memset(u, 0, sizeof *u);
    memset(v5, 0, sizeof *v5);
    u->version = cc_credentials_v5;
    u->credentials.credentials_v5 = v5;
    if ((code = krb5i_cred_2_ccred(context, creds, v5)))
	goto Done;
    x = cc_ccache_store_credentials(ddata->ccache, u);
    if (x) {
	code = krb5i_ccred_xlate_err(x);
	goto Done;
    }
Done:
    krb5i_free_ccred_contents(v5);
    return code;
}

int
krb5i_acc_end_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_acc_cursor *dcursor = (struct krb5_acc_cursor *) *cursor;

    if (dcursor->iter) 
	cc_credentials_iterator_release(dcursor->iter);
    free(dcursor);
    return 0;
}

int
krb5i_acc_start_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;
    struct krb5_acc_cursor *dcursor;
    cc_int32 x;
    int code;

    *cursor = 0;
    if (ddata->state) return ddata->state;
    if (!ddata->ccache) return KRB5_FCC_INTERNAL;
    dcursor = (struct krb5_acc_cursor *)malloc(sizeof *dcursor);
    if (!dcursor)
	return ENOMEM;
    memset(dcursor, 0, sizeof *dcursor);
    x = cc_ccache_new_credentials_iterator(ddata->ccache, &dcursor->iter);
    if (x) {
	code = krb5i_ccred_xlate_err(x);
	goto Done;
    }
    *cursor = dcursor;
    dcursor = 0;
Done:
    if (dcursor) {
	(void) krb5i_acc_end_get(context, cache, (krb5_cc_cursor*)&dcursor);
    }
    return code;
}

int
krb5i_acc_get_next(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor, krb5_creds *creds)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;
    struct krb5_acc_cursor *dcursor = (struct krb5_acc_cursor *) *cursor;
    int code;
    cc_int32 x;
    krb5_creds c[1];
    cc_credentials_t a = 0;

    memset(c, 0, sizeof *c);
    for (;;) {
	x = cc_credentials_iterator_next(dcursor->iter, &a);
	if (x) {
	    code = krb5i_ccred_xlate_err(x);
	    goto Done;
	}
	if (a->data->version == cc_credentials_v5)
	    break;
	cc_credentials_release(a);
    }
    code = krb5i_ccred_2_cred(context,
	a->data->credentials.credentials_v5,
	c);
    if (code) goto Done;
    *creds = *c;
    memset(c, 0, sizeof *c);
Done:
    if (a) cc_credentials_release(a);
    krb5_free_cred_contents(context, c);
    return code;
}

int
krb5i_acc_get_principal(krb5_context context,
    krb5_ccache cache, krb5_principal *princ)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;
    int code;
    cc_int32 x;
    cc_string_t n = 0;

    if (ddata->state) return ddata->state;
    if (!ddata->ccache) return KRB5_FCC_INTERNAL;
    x = cc_ccache_get_principal(ddata->ccache, cc_credentials_v5, &n);
    if (x) {
	code = krb5i_ccred_xlate_err(x);
	goto Done;
    }
    if ((code = krb5_parse_name(context, n->data, princ)))
	goto Done;
Done:
    if (n) cc_string_release(n);
    return code;
}

int
krb5i_acc_close(krb5_context context, krb5_ccache cache)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;

    if (!ddata)
	return 0;
    if (ddata->ccache)
	cc_ccache_release(ddata->ccache);
    if (ddata->context)
	cc_context_release(ddata->context);
    if (ddata->dlhandle) dlclose(ddata->dlhandle);
    free(ddata);
    return 0;
}

const char *
krb5i_acc_get_name(krb5_context context, krb5_ccache cache)
{
    return ((struct krb5_acc_data *) cache->data)->name;
}

krb5_error_code
krb5i_acc_initialize(krb5_context context, krb5_ccache cache, krb5_principal princ)
{
    struct krb5_acc_data *ddata = (struct krb5_acc_data *) cache->data;
    cc_int32 x;
    int code;
    char *name = 0;
    cc_credentials_iterator_t iter = 0;
    cc_credentials_t c = 0;

    code = krb5_unparse_name(context, princ, &name);
    if (code) goto Done;
    if (ddata->ccache) {
	x = cc_ccache_new_credentials_iterator(ddata->ccache,
	    &iter);
	if (x) goto Failed;
	while (!(x = cc_ccache_iterator_next(iter, &c))) {
	    cc_ccache_remove_credentials(ddata->ccache, c);
	    cc_credentials_release(c);
	    c = 0;
	}
	cc_ccache_iterator_release(iter);
	iter = 0;
	x = cc_ccache_set_principal(ddata->ccache,
	    cc_credentials_v5, name);
	if (x) goto Failed;
    } else {
	x = cc_context_create_new_ccache(ddata->context,
		cc_credentials_v5,
		name,
		&ddata->ccache);
	if (x) goto Failed;
    }
    ddata->state = 0;
Failed:
    code = krb5i_ccred_xlate_err(x);
Done:
    if (c) cc_credentials_release(c);
    if (iter) cc_ccache_iterator_release(iter);
    if (name) free(name);
    return code;
}

const krb5_cc_ops krb5_acc_ops = {
    krb5i_acc_prefix,
    krb5i_acc_resolve,
    krb5i_acc_store_cred,
    krb5i_acc_start_get,
    krb5i_acc_get_next,
    krb5i_acc_end_get,
    krb5i_acc_get_principal,
    krb5i_acc_close,
    krb5i_acc_get_name,
    krb5i_acc_initialize,
};
