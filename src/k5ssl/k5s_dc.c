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
 * interface glue to darwin cache IPC.
 *
 * TEMP CODE
 * ONLY EXPERIMENTAL USE
 * if you don't know why this is a bad idea; don't use it.
 *
 * the darwin ccache IPC is an internal (unstable) interface,
 * it is probably subject to change without notice.
 * Interface might be equivalent to Kerberos-65.10;
 * tested with Mac OS X Server ProductVersion: 10.4.8 8L127
 *
 * The right answer is to link with the native Kerberos library
 * and not this code.  Unfortunately the native library doesn't
 * currently expose the functionality required by rxk5.
 * A safer answer is to use file based credentials caches.
 */


#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <servers/bootstrap_defs.h>
#include "CCacheIPC.h"
#include "k5ssl.h"
#include "k5s_im.h"
#include "cci_er.h"
#define cc_credentials_v5 2

static const char krb5i_dcc_prefix[] = "API";

struct krb5_dcc_data {
    mach_port_t cc_port;
    ContextID x;
    CCacheID cc;
    int state;
    char name[sizeof krb5i_dcc_prefix];
};
#define STATE_OK 0
#define STATE_WO CCERCCacheNotFound
#define STATE_V4 KRB5_CC_FORMAT

struct krb5_dcc_cursor {
    CredentialsIDArray a;
    unsigned int as, l;
    int pos;
};

struct mem_buf {
    unsigned char *buf;
    unsigned length, pos;
};

int
krb5i_cci_getint32(struct mem_buf *mb, int *out)
{
    if (sizeof *out > mb->length - mb->pos) return KRB5_CC_FORMAT;
    memcpy(out, mb->buf + mb->pos, sizeof *out);
    mb->pos += sizeof *out;
    return 0;
}

int
krb5i_cci_getdata(struct mem_buf *mb, krb5_data *data)
{
    int code;
    int s;
    if ((code = krb5i_cci_getint32(mb, &s))) return code;
    if (s > mb->length - mb->pos) return KRB5_CC_FORMAT;
    if (!(data->data = malloc(s))) return ENOMEM;
    data->length = s;
    memcpy(data->data, mb->buf + mb->pos, s);
    mb->pos += s;
    return 0;
}

int
krb5i_cci_getstring(struct mem_buf *mb, krb5_data *data)
{
    int code;
    krb5_data d[1];

    memset(d, 0, sizeof *d);
    if ((code = krb5i_cci_getdata(mb, d)))
	goto Failed;
    if (d->length < 1 || (d->length-1)[(char *)d->data]) {
	code = KRB5_CC_FORMAT;
	goto Failed;
    }
    *data = *d;
    d->data = 0;
Failed:
    if (d->data) free(d->data);
    return code;
}

int
krb5i_cci_getkey(struct mem_buf *mb, krb5_keyblock *kb)
{
    int code;
    krb5_data d[1];
    int s;

    if ((code = krb5i_cci_getint32(mb, &s))) return code;
    memset(d, 0, sizeof *d);
    if ((code = krb5i_cci_getdata(mb, d)))
	goto Failed;
    kb->enctype = s;
    kb->length = d->length;
    kb->contents = (void *) d->data;
    d->data = 0;
Failed:
    if (d->data) free(d->data);
    return code;
}

int
krb5i_cci_getaddr(struct mem_buf *mb, krb5_address *ad)
{
    int code;
    krb5_data d[1];
    int s;

    if ((code = krb5i_cci_getint32(mb, &s))) return code;
    memset(d, 0, sizeof *d);
    if ((code = krb5i_cci_getdata(mb, d)))
	goto Failed;
    ad->addrtype = s;
    ad->length = d->length;
    ad->contents = (void *) d->data;
    d->data = 0;
Failed:
    if (d->data) free(d->data);
    return code;
}

int
krb5i_cci_getauth(struct mem_buf *mb, krb5_authdata *ad)
{
    int code;
    krb5_data d[1];
    int s;

    if ((code = krb5i_cci_getint32(mb, &s))) return code;
    memset(d, 0, sizeof *d);
    if ((code = krb5i_cci_getdata(mb, d)))
	goto Failed;
    ad->ad_type = s;
    ad->length = d->length;
    ad->contents = (void *) d->data;
    d->data = 0;
Failed:
    if (d->data) free(d->data);
    return code;
}

int
krb5i_cci_getarray(struct mem_buf *mb, int (*f)(), int z, void ***out)
{
    int code;
    int i, s;
    void *p;

    if ((code = krb5i_cci_getint32(mb, &s))) return code;
    code = ENOMEM;
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
	if ((code = (*f)(mb, i[*out])))
	    goto Failed;
    }
Failed:
    return code;
}

int
krb5i_cci_2_cred(krb5_context context,
    krb5_data *data,
    krb5_creds *creds)
{
    int code;
    struct mem_buf mb[1];
    int temp;
    krb5_data d[1];
    krb5_creds c[1];

    memset(mb, 0, sizeof *mb);
    memset(d, 0, sizeof *d);
    memset(c, 0, sizeof *c);
    mb->buf = (void *) data->data;
    mb->length = data->length;
    if ((code = krb5i_cci_getint32(mb, &temp)))
	goto Done;
    switch(temp) {
    default:
	code = KRB5_CC_FORMAT;
	goto Done;
    case cc_credentials_v5:
	if ((code = krb5i_cci_getstring(mb, d)))
	    goto Done;
	if ((code = krb5_parse_name(context, (void*) d->data, &c->client)))
	    goto Done;
	free(d->data); d->data = 0;
	if ((code = krb5i_cci_getstring(mb, d)))
	    goto Done;
	if ((code = krb5_parse_name(context, (void*) d->data, &c->server)))
	    goto Done;
	free(d->data); d->data = 0;
	if ((code = krb5i_cci_getkey(mb, &c->keyblock)))
	    goto Done;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->times.authtime = temp;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->times.starttime = temp;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->times.endtime = temp;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->times.renew_till = temp;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->is_skey = temp;
	if ((code = krb5i_cci_getint32(mb, &temp)))
	    goto Done;
	c->ticket_flags = temp;
	if ((code = krb5i_cci_getarray(mb, krb5i_cci_getaddr, sizeof **c->addresses, (void ***) &c->addresses)))
	    goto Done;
	if ((code = krb5i_cci_getint32(mb, &temp))) return code;
	memset(d, 0, sizeof *d);
	if ((code = krb5i_cci_getdata(mb, &c->ticket)))
	    goto Done;
	if ((code = krb5i_cci_getint32(mb, &temp))) return code;
	if ((code = krb5i_cci_getdata(mb, &c->second_ticket)))
	    goto Done;
	if ((code = krb5i_cci_getarray(mb, krb5i_cci_getauth, sizeof **c->authdata, (void ***) &c->authdata)))
	    goto Done;
	if (mb->pos != mb->length)	/* left-over stuff? */
	    code = KRB5_CC_FORMAT;
    }
    *creds = *c;
    memset(c, 0, sizeof *c);
Done:
    krb5_free_cred_contents(context, c);
    if (d->data) free(d->data);
    return code;
}

int
krb5i_cci_putopaque(struct mem_buf *mb, void *s, int l)
{
    if (mb->pos + l > mb->length) return KRB5_CC_FORMAT;
    memcpy(mb->buf + mb->pos, s, l);
    mb->pos += l;
    return 0;
}

int
krb5i_cci_putint32(struct mem_buf *mb, int i)
{
    return krb5i_cci_putopaque(mb, (void *) &i, sizeof i);
}

int
krb5i_cci_putstring(struct mem_buf *mb, char *s)
{
    int code;
    int l = strlen(s)+1;

    if ((code = krb5i_cci_putint32(mb, l)))
	goto Done;
    if ((code = krb5i_cci_putopaque(mb, s, l)))
	goto Done;
Done:
    return code;
}

int
krb5i_cci_putdata(struct mem_buf *mb, krb5_data *data)
{
    int code;

    if ((code = krb5i_cci_putint32(mb, data->length)))
	goto Done;
    if ((code = krb5i_cci_putopaque(mb, data->data, data->length)))
	goto Done;
Done:
    return code;
}

int
krb5i_cci_putkey(struct mem_buf *mb, krb5_keyblock *kb)
{
    int code;

    if ((code = krb5i_cci_putint32(mb, kb->enctype)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, kb->length)))
	goto Done;
    if ((code = krb5i_cci_putopaque(mb, kb->contents, kb->length)))
	goto Done;
Done:
    return code;
}

int
krb5i_cci_putaddr(struct mem_buf *mb, krb5_address *ad)
{
    int code;

    if ((code = krb5i_cci_putint32(mb, ad->addrtype)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, ad->length)))
	goto Done;
    if ((code = krb5i_cci_putopaque(mb, ad->contents, ad->length)))
	goto Done;
Done:
    return code;
}

int
krb5i_cci_putauth(struct mem_buf *mb, krb5_authdata *ad)
{
    int code;

    if ((code = krb5i_cci_putint32(mb, ad->ad_type)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, ad->length)))
	goto Done;
    if ((code = krb5i_cci_putopaque(mb, ad->contents, ad->length)))
	goto Done;
Done:
    return code;
}

int
krb5i_cci_putarray(struct mem_buf *mb, int (*f)(), void **in)
{
    int i;
    int code;

    if (!in) {
	return krb5i_cci_putint32(mb, 0);
    }
    for (i = 0; in[i]; ++i)
	;
    if ((code = krb5i_cci_putint32(mb, i)))
	goto Done;
    for (i = 0; in[i]; ++i) {
	if ((code = (*f)(mb, in[i])))
	    goto Done;
    }
Done:
    return code;
}

int
krb5i_cred_2_cci(krb5_context context, krb5_creds *creds, krb5_data *data)
{
    int code;
    char *s = 0;
    struct mem_buf mb[1];

    memset(mb, 0, sizeof *mb);
    if (!(mb->buf = malloc(mb->length = 16384))) {
	code = ENOMEM;
	goto Done;
    }
    if ((code = krb5i_cci_putint32(mb, cc_credentials_v5)))
	goto Done;
    if ((code = krb5_unparse_name(context, creds->client, &s)))
	goto Done;
    if ((code = krb5i_cci_putstring(mb, s)))
	goto Done;
    free(s); s = 0;
    if ((code = krb5_unparse_name(context, creds->server, &s)))
	goto Done;
    if ((code = krb5i_cci_putstring(mb, s)))
	goto Done;
    free(s); s = 0;
    if ((code = krb5i_cci_putkey(mb, &creds->keyblock)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->times.authtime)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->times.starttime)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->times.endtime)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->times.renew_till)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->is_skey)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, creds->ticket_flags)))
	goto Done;
    if ((code = krb5i_cci_putarray(mb, krb5i_cci_putaddr,
	    (void **) creds->addresses)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, 0))) return code;
    if ((code = krb5i_cci_putdata(mb, &creds->ticket)))
	goto Done;
    if ((code = krb5i_cci_putint32(mb, 0))) return code;
    if ((code = krb5i_cci_putdata(mb, &creds->second_ticket)))
	goto Done;
    if ((code = krb5i_cci_putarray(mb, krb5i_cci_putauth,
	    (void **) creds->authdata)))
	goto Done;
    code = ENOMEM;
    if (!(data->data = malloc(mb->pos)))
	goto Done;
    memcpy(data->data, mb->buf, mb->pos);
    data->length = mb->pos;
    code = 0;
Done:
    if (s) free(s);
    if (mb->buf) free(mb->buf);
    return code;
}

int
krb5i_dcc_resolve(krb5_context context, krb5_ccache cache, const char *name)
{
    struct krb5_dcc_data *ddata = 0;
    int code;
    unsigned int l;
    int i;
    CCIResult r = 0;
    CCIUInt32 v = 0;
    ContextID x;
    CCacheID cc;
    kern_return_t ke;
    mach_port_t boot_port = 0, priv_port = 0, cc_port = 0;
    char *buf = 0;
    mach_port_t me = mach_task_self();
    boolean_t active;
    int state = 0;

    initialize_ccER_error_table();
    if ((ke = task_get_bootstrap_port(me, &boot_port))) {
	code = code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    for (i = 0; i < 1; ++i) {
	code = 0;
	if ((ke = bootstrap_status(boot_port, kCCacheServerBundleID, &active))) {
	    code = ke + ERROR_TABLE_BASE_mach;
	}
	if (code == MACH_BOOTSTRAP_UNKNOWN_SERVICE)
	    ;
	else if (code)
	    goto Done;
	else if (active != BOOTSTRAP_STATUS_INACTIVE)
	    break;
	if ((ke = bootstrap_create_server(boot_port, kCCacheServerPath, getuid(), TRUE, &priv_port))) {
	    code = ke + ERROR_TABLE_BASE_mach;
	    goto Done;
	}
	if ((ke = bootstrap_create_service(priv_port, kCCacheServerBundleID, &cc_port))) {
	    code = ke + ERROR_TABLE_BASE_mach;
	    goto Done;
	}
	if (priv_port) mach_port_deallocate(me, priv_port);
	if (cc_port) mach_port_deallocate(me, cc_port);
	cc_port = priv_port = 0;
    }
    if ((ke = bootstrap_look_up(boot_port, kCCacheServerBundleID, &cc_port))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if ((ke = ContextIPC_GetGlobalContextID(cc_port, &x, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
    if (!name || !*name) {
	/* XXX cheezy -- how do I know buf is 0 terminated? */
	if ((ke = ContextIPC_GetDefaultCCacheName(cc_port, x, &buf, &l, &r))) {
	    code = ke + ERROR_TABLE_BASE_mach;
	    goto Done;
	}
	name = buf;
    } else {
	l = strlen(name);
    }
    if ((ke = ContextIPC_OpenCCache(cc_port, x, name, l, &cc, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	if (code != CCERCCacheNotFound)
	    goto Done;
	state = STATE_WO;
	goto Ready;
    }
    if ((ke = CCacheIPC_GetCredentialsVersion(cc_port, cc, &v, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
    if (!(v & cc_credentials_v5)) {
	state = STATE_V4;
	goto Done;
    }
Ready:
    ddata = malloc(sizeof *ddata + l + 1);
    if (!ddata) {
	code = ENOMEM;
	goto Done;
    }
    memset(ddata, 0, sizeof *ddata);
    memcpy(ddata->name, krb5i_dcc_prefix, sizeof krb5i_dcc_prefix - 1);
    ddata->name[sizeof krb5i_dcc_prefix - 1] = ':';
    memcpy(ddata->name + sizeof krb5i_dcc_prefix, name, l);
    ddata->name[sizeof krb5i_dcc_prefix + l] = 0;
    ddata->cc = cc;
    ddata->cc_port = cc_port;
    ddata->x = x;
    ddata->state = state;
    cc_port = 0;
    cache->data = ddata;
    ddata = 0;
    code = 0;
Done:
    if (priv_port) mach_port_deallocate(me, priv_port);
    if (boot_port) mach_port_deallocate(me, boot_port);
    if (cc_port) mach_port_deallocate(me, cc_port);
    if (ddata) free(ddata);
    if (buf) mig_deallocate((vm_address_t) buf, l);
    return code;
}

int
krb5i_dcc_store_cred(krb5_context context, krb5_ccache cache,
    krb5_creds *creds)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    kern_return_t ke;
    CCIResult r = 0;
    krb5_data data[1];
    int code;

    if (ddata->state != STATE_OK) return ddata->state;
    memset(data, 0, sizeof *data);
    if ((code = krb5i_cred_2_cci(context, creds, data)))
	goto Done;
    if ((ke = CCacheIPC_StoreCredentials(ddata->cc_port, ddata->cc,
	    (void*)data->data, data->length, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
Done:
    if (data->data) free(data->data);
    return code;
}

int
krb5i_dcc_end_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_dcc_cursor *dcursor = (struct krb5_dcc_cursor *) *cursor;

    if (dcursor->a) mig_deallocate((vm_address_t) dcursor->a, dcursor->as);
    free(dcursor);
    return 0;
}

int
krb5i_dcc_start_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    struct krb5_dcc_cursor *dcursor;
    int code;
    CCIResult r = 0;
    kern_return_t ke;

    *cursor = 0;
    if (ddata->state != STATE_OK) return ddata->state;
    dcursor = (struct krb5_dcc_cursor *)malloc(sizeof *dcursor);
    if (!dcursor)
	return ENOMEM;
    memset(dcursor, 0, sizeof *dcursor);
    if ((ke = CCacheIPC_GetCredentialsIDs(ddata->cc_port, ddata->cc,
	    &dcursor->a, &dcursor->as, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
    dcursor->l = dcursor->as >> 2;
    *cursor = dcursor;
    dcursor = 0;
Done:
    if (dcursor) {
	(void) krb5i_dcc_end_get(context, cache, (krb5_cc_cursor*)&dcursor);
    }
    return code;
}

int
krb5i_dcc_get_next(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor, krb5_creds *creds)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    struct krb5_dcc_cursor *dcursor = (struct krb5_dcc_cursor *) *cursor;
    CCIResult r = 0;
    kern_return_t ke;
    int code;
    krb5_creds c[1];
    char *flat = 0;
    unsigned int l;
    int i;
    krb5_data data[1];
    CCIUInt32 cv;

    memset(c, 0, sizeof *c);
    for (i = dcursor->pos; i < dcursor->l; ++i) {
	if ((ke = CredentialsIPC_GetVersion(ddata->cc_port, dcursor->a[i], &cv, &r))) {
	    code = ke + ERROR_TABLE_BASE_mach;
	    goto Done;
	}
	if (r) {
	    code = r + ERROR_TABLE_BASE_ccER;
	    goto Done;
	}
	if (cv != cc_credentials_v5) continue;
	dcursor->pos = i+1;
	if ((ke = CredentialsIPC_FlattenCredentials(ddata->cc_port,
		dcursor->a[i], &flat, &l, &r)) || r) {
	    code = ke + ERROR_TABLE_BASE_mach;
	    goto Done;
	}
	if (r) {
	    code = r + ERROR_TABLE_BASE_ccER;
	    goto Done;
	}
	data->data = (void*) flat;
	data->length = l;
	if ((code = krb5i_cci_2_cred(context, data, c)))
	    goto Done;
	*creds = *c;
	memset(c, 0, sizeof *c);
	code = 0;
	goto Done;
    }
    dcursor->pos = i;
    code = KRB5_CC_END;
Done:
    if (flat) mig_deallocate((vm_address_t) flat, l);
    krb5_free_cred_contents(context, c);
    return code;
}

int
krb5i_dcc_get_principal(krb5_context context,
    krb5_ccache cache, krb5_principal *princ)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    int code;
    CCIResult r = 0;
    kern_return_t ke;
    char *prin = 0;
    unsigned int l;

    if (ddata->state != STATE_OK) return ddata->state;
    if ((ke = CCacheIPC_GetPrincipal(ddata->cc_port, ddata->cc,
	    cc_credentials_v5, &prin, &l, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
    if ((code = krb5_parse_name(context, prin, princ)))
	goto Done;
Done:
    if (prin) mig_deallocate((vm_address_t) prin, l);
    return code;
}

int
krb5i_dcc_close(krb5_context context, krb5_ccache cache)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    mach_port_t me = mach_task_self();

    if (!ddata)
	return 0;
    else if (ddata->cc_port)
	mach_port_deallocate(me, ddata->cc_port);
    free(ddata);
    return 0;
}

const char *
krb5i_dcc_get_name(krb5_context context, krb5_ccache cache)
{
    return ((struct krb5_dcc_data *) cache->data)->name;
}

krb5_error_code
krb5i_dcc_initialize(krb5_context context, krb5_ccache cache, krb5_principal princ)
{
    struct krb5_dcc_data *ddata = (struct krb5_dcc_data *) cache->data;
    int code;
    CCIResult r = 0;
    kern_return_t ke;
    CCacheID cc;
    char *name = 0;

    code = krb5_unparse_name(context, princ, &name);
    if ((ke = ContextIPC_CreateCCache(ddata->cc_port, ddata->x,
	    ddata->name + sizeof krb5i_dcc_prefix,
	    strlen(ddata->name) - sizeof krb5i_dcc_prefix,
	    cc_credentials_v5, name, strlen(name), &cc, &r))) {
	code = ke + ERROR_TABLE_BASE_mach;
	goto Done;
    }
    if (r) {
	code = r + ERROR_TABLE_BASE_ccER;
	goto Done;
    }
    ddata->cc = cc;
    ddata->state = 0;
Done:
    if (name) free(name);
    return code;
}

const krb5_cc_ops krb5_dcc_ops = {
    krb5i_dcc_prefix,
    krb5i_dcc_resolve,
    krb5i_dcc_store_cred,
    krb5i_dcc_start_get,
    krb5i_dcc_get_next,
    krb5i_dcc_end_get,
    krb5i_dcc_get_principal,
    krb5i_dcc_close,
    krb5i_dcc_get_name,
    krb5i_dcc_initialize,
};
