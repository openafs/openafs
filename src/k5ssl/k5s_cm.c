/*
 * Copyright (c) 2005
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

#if defined(KERNEL)
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afs/afsincludes.h"        /*AFS-based standard headers */
#include "afs_stats.h"
#else
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#include <errno.h>
#endif
#include <sys/types.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#else
#include "k5s_evp.h"
#endif
#include "k5ssl.h"

void
krb5_free_checksum_contents(krb5_context context, krb5_checksum *sum)
{
    if (sum->contents) {
	memset(sum->contents, 0, sum->length);
	afs_osi_Free(sum->contents, sum->length);
	sum->contents = 0;
    }
}

void
krb5_free_keyblock_contents(krb5_context context, krb5_keyblock *key)
{
    if (key->contents) {
	memset(key->contents, 0, key->length);
	afs_osi_Free(key->contents, key->length);
	key->contents = 0;
    }
}

void
krb5_free_keyblock(krb5_context context, krb5_keyblock *key)
{
    if (!key) return;
    krb5_free_keyblock_contents(context, key);
    afs_osi_Free(key, sizeof *key);
}

void
krb5_free_principal(krb5_context context, krb5_principal princ)
{
    int i;
    if (princ) {
	if (princ->realm.data)
	    afs_osi_Free(princ->realm.data, 1 + princ->realm.length);
	if (princ->data) {
	    for (i = 0; i < princ->length; ++i)
		if (princ->data[i].data)
		    afs_osi_Free(princ->data[i].data, 1 + princ->data[i].length);
		else break;
	    afs_osi_Free(princ->data, (princ->length * sizeof(krb5_data)));
	}
	afs_osi_Free(princ, sizeof(*princ));
    }
    return;
}

void
krb5_free_addresses(krb5_context context, krb5_address **ap)
{
    int i;

    if (!ap) return;
    for (i = 0; ap[i]; ++i) {
	if (!ap[i]) break;
	if (ap[i]->contents)
	    afs_osi_Free(ap[i]->contents, ap[i]->length);
	afs_osi_Free(ap[i], sizeof(ap[i]));
    }
    afs_osi_Free(ap, sizeof(*ap));
}

void
krb5_free_authdata(krb5_context context, krb5_authdata **ap)
{
    int i;

    if (!ap) return;
    for (i = 0; ap[i]; ++i) {
	if (!ap[i]) break;
	if (ap[i]->contents)
	    afs_osi_Free(ap[i]->contents, ap[i]->length);
	afs_osi_Free(ap[i], sizeof(ap[i]));
    }
    afs_osi_Free(ap, sizeof(*ap));
}

void
krb5_free_cred_contents(krb5_context context, krb5_creds *creds)
{
    krb5_free_principal(context, creds->client);
    krb5_free_principal(context, creds->server);
    krb5_free_keyblock_contents(context, &creds->keyblock);
    if (creds->ticket.data)
	afs_osi_Free(creds->ticket.data, creds->ticket.length);
    if (creds->second_ticket.data)
	afs_osi_Free(creds->second_ticket.data, creds->second_ticket.length);
    krb5_free_addresses(context, creds->addresses);
    krb5_free_authdata(context, creds->authdata);
    memset(creds, 0, sizeof *creds);
}

int
krb5i_data_compare(const krb5_data *d1, const krb5_data *d2)
{
    if (d1 == d2) return 1;
    if (d1->length != d2->length)
	return 0;
    if (d1->data == d2->data)
	return 1;
    if (!d1->data || !d2->data) return 0;
    return !memcmp(d1->data, d2->data, d1->length);
}

int
krb5_principal_compare(krb5_context context,
    krb5_const_principal p1, krb5_const_principal p2)
{
    int i;
    if (p1 == p2) return 1;
    if (p1->length != p2->length)
	return 0;

    if (!krb5i_data_compare(&p1->realm, &p2->realm))
	return 0;
    if (p1->data == p2->data)
	return 1;
    for (i = 0; i < p1->length; ++i)
	if (!krb5i_data_compare(p1->data+i, p2->data+i))
	    return 0;
    return 1;
}

void
krb5_free_creds(krb5_context context, krb5_creds *creds)
{
    if (creds) {
	krb5_free_cred_contents(context, creds);
	afs_osi_Free(creds, sizeof(*creds));
    }
}

krb5_error_code
krb5_copy_keyblock_contents(krb5_context context,
    const krb5_keyblock *from, 
    krb5_keyblock *to)
{
    to->contents = afs_osi_Alloc(from->length);
    memcpy(to->contents, from->contents, from->length);
    to->length = from->length;
    to->enctype = from->enctype;
    return 0;
}

int
krb5_parse_name(krb5_context context, const char *name, krb5_principal *princ)
{
/* XXX local realm? */
    krb5_principal p;
    const char *cp;
    int n, c;
    int esc, cont;
    unsigned char *temp, *tp;
#if !defined(KERNEL)
    char *lrealm;
#endif
    int code;
    int temp_sz;
    krb5_data *data;

    code = ENOMEM;
    p = (krb5_principal) afs_osi_Alloc(sizeof *p);
    if (!p) return code;
    memset(p, 0, sizeof *p);
    p->type = KRB5_NT_PRINCIPAL;
    temp_sz = strlen(name) + 1;
    temp = afs_osi_Alloc(temp_sz);
    if (!temp) goto Done;
    esc = 0;
    for (n = 1, cp = name;*cp; ++cp)
	if (esc) esc = 0;
	else if (*cp == '\\') esc = 1;
	else if (*cp == '@') break;
	else n += (*cp == '/');
    p->length = n;
    p->data = (krb5_data *) afs_osi_Alloc((n * sizeof(*p->data)));
    memset(p->data, 0, n);
    if (!p->data) goto Done;
    cont = esc = n = 0;
    code = KRB5_PARSE_MALFORMED;
    while (*name || cont) {
	cont = 0;
	tp = temp;
	for (cp = name; (c = *cp); ++cp) {
	    if (esc) switch(esc = 0, c) {
	    case '0': c = 0; break;
	    case 'n': c = '\n'; break;
	    case 't': c = '\t'; break;
	    case 'b': c = '\b'; break;
	    }
	    else if (*cp == '\\') {
		esc = 1;
		continue;
	    } else if (*cp == '@' || *cp == '/') {
		cont = 1;
		++cp;
		break;
	    }
	    *tp++ = c;
	}
	if (esc) goto Done;
	c = tp - temp;
	if (n > p->length) goto Done;
	data = n == p->length ? &p->realm : p->data+n;
	data->data = afs_osi_Alloc((data->length = c) + 1);
	if (!data->data) {
	    code = ENOMEM; goto Done;
	}
	memcpy(data->data, temp, c);
	data->data[c] = 0;
	++n;
	name = cp;
    }
#if !defined(KERNEL)
    if (!p->realm.data && context) {
	code = krb5_get_default_realm(context, &lrealm);
	if (code) goto Done;
	p->realm.data = (unsigned char *) lrealm;
	p->realm.length = strlen(lrealm);
    }
#endif
    code = 0;
Done:
    if (temp) afs_osi_Free(temp, temp_sz);
    if (code)
	krb5_free_principal(context, p);
    else
	*princ = p;
    return code;
}

static int
count_quotable(const krb5_data *data)
{
    unsigned char *cp = data->data;
    int l, r;
    r = l = data->length;
    while (l) switch(--l, *cp++) {
    case 0:
    case '\n': case '\t': case '\b':
    case '\\': case '/': case '@':
	++r;
    }
    return r;
}

static char *
copy_quotable(char *out, const krb5_data *data)
{
    unsigned char *cp = data->data;
    int l = data->length, c;

    while (l) {
	switch(--l, c = *cp++) {
	default:
	    *out++ = c;
	    continue;
	case 0: c = '0'; break;
	case '\n': c = 'n'; break;
	case '\t': c = 't'; break;
	case '\b': c = 'b'; break;
	case ' ': case '\\': case '/': case '@': break;
	}
	*out++ = '\\';
	*out++ = c;
    }
    return out;
}

int 
krb5_unparse_name(krb5_context context, krb5_const_principal princ, char **pn)
{
    int i, l;
    char *cp;

    l = 0;
    *pn = 0;
    if (princ->realm.data)
	l += count_quotable(&princ->realm);
    for (i = 0; i < princ->length; ++i)
	l += count_quotable(princ->data+i);
    l += princ->length;
    cp = afs_osi_Alloc(l+1);
    if (!cp) return ENOMEM;
    *pn = cp;
    for (i = 0; i < princ->length; ++i) {
	if (i) *cp++ = '/';
	cp = copy_quotable(cp, princ->data+i);
    }
    *cp++ = '@';
    if (princ->realm.data)
	cp = copy_quotable(cp, &princ->realm);
    *cp = 0;
/* assert(cp == *pn + l); */
    return 0;
}
