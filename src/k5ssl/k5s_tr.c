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

#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "k5ssl.h"

krb5_error_code
krb5_tgtname(krb5_context context,
    const krb5_data *server,
    const krb5_data *client,
    krb5_principal *tgt)
{
    int code;
    krb5_principal p;

    *tgt = 0;
    code = ENOMEM;
    p = malloc(sizeof *p);
    if (!p) goto Done;
    memset(p, 0, sizeof *p);

    p->realm.length = client->length;
    if (!(p->realm.data = malloc(p->realm.length+1)))
	goto Done;
    memcpy(p->realm.data, client->data, p->realm.length);
    p->realm.data[p->realm.length] = 0;

    p->length = 2;
    if (!(p->data = malloc(sizeof *p->data * 2)))
	goto Done;

    p->data[0].length = KRB5_TGS_NAME_SIZE;
    if (!(p->data[0].data = malloc(p->data[0].length+1)))
	goto Done;
    memcpy(p->data[0].data, KRB5_TGS_NAME, KRB5_TGS_NAME_SIZE);
    p->data[0].data[KRB5_TGS_NAME_SIZE] = 0;

    p->data[1].length = server->length;
    if (!(p->data[1].data = malloc(p->data[1].length+1)))
	goto Done;
    memcpy(p->data[1].data, server->data, p->data[1].length);
    p->data[1].data[p->data[1].length] = 0;

    *tgt = p;
    p = 0;
    code = 0;
Done:
    if (p) krb5_free_principal(context, p);
    return code;
}

void
krb5_free_realm_tree(krb5_context context, krb5_principal *t)
{
    int i;

    if (t) {
	for (i = 0; t[i]; ++i)
	    krb5_free_principal(context, t[i]);
	free(t);
    }
}

krb5_error_code
krb5i_dns_hier(krb5_context context,
    const krb5_data *client, const krb5_data *server,
    int rbc,
    char ***strings)
{
    krb5_data c[1], s[1];
    unsigned char *cp;
    char **r = 0;
    int n, i, j;
    int com;
    int code;

    *strings = 0;
    *c = *client; *s = *server;
    n = 1;
    for (;;) {
	if (c->length == s->length && !memcmp(s->data, c->data,
		c->length)) {
	    break;
	} else if (c->length > s->length) {
	    cp = memchr(c->data, rbc, c->length);
	    if (cp)
	    {
		++cp;
		c->length -= (cp - c->data);
		c->data = cp;
	    } else c->length = 0;
	} else {
	    cp = memchr(s->data, rbc, s->length);
	    if (cp) {
		++cp;
		s->length -= (cp - s->data);
		s->data = cp;
	    } else s->length = 0;
	}
	++n;
    }
    com = s->length;
    n += !!com;
    *c = *client; *s = *server;
    code = ENOMEM;
    if (!(r = malloc(n * sizeof *r)))
	goto Done;
    r[n-1] = 0;
    i = 0;
    while (c->length > com) {
	if (!(r[i] = malloc(c->length+1)))
	    goto Done;
	memcpy(r[i], c->data, c->length);
	r[i][c->length] = 0;
	++i;
	cp = memchr(c->data, rbc, c->length - com);
	if (!cp) break;
	++cp;
	c->length -= (cp - c->data);
	c->data = cp;
    }
    if (com) {
	if (!(r[i] = malloc(com+1)))
		goto Done;
	memcpy(r[i], c->data, com);
	r[i][com] = 0;
	++i;
    }
    r[i] = 0;
    while (s->length > com) {
	if (!(cp = malloc(s->length+1)))
	    goto Done;
	for (j = n-1; j > i; --j)
		r[j] = r[j-1];
	r[i] = (char *) cp;
	memcpy(r[i], s->data, s->length);
	r[i][s->length] = 0;
	cp = memchr(s->data, rbc, s->length - com);
	if (!cp) break;
	++cp;
	s->length -= (cp - s->data);
	s->data = cp;
    }
    code = 0;
Done:
    if (!code) {
	*strings = r;
	r = 0;
    }
    if (r) {
	for (i = 0; r[i]; ++i)
	    free(r[i]);
	free(r);
	r = 0;
    }
    return code;
}

krb5_error_code
krb5i_x500_hier(krb5_context context,
    const krb5_data *client, const krb5_data *server,
    int rbc,
    char ***strings)
{
    krb5_data c[1], s[1];
    unsigned char *cp, *ep;
    char **r = 0;
    int n, i, j;
    int com;
    int code;

    *c = *client; *s = *server;
    n = 1;
    j = c->length; if (j > s->length) j = s->length;
    for (i = 0; i < j; ++i)
	if (c->data[i] != s->data[i])
	    break;
    if (!(i >= c->length || c->data[i] == rbc)
	    || !(i >= s->length || s->data[i] == rbc)) {
	while (i && c->data[--i] != rbc)
	    ;
    }
    com = i;
    n += !!com;
    ep = c->data + c->length;
    for (cp = c->data + com;
	    (cp = memchr(cp, rbc, ep-cp));
	    ++cp) {
	++n;
    }
    ep = s->data + s->length;
    for (cp = s->data + com;
	    (cp = memchr(cp, rbc, ep-cp));
	    ++cp) {
	++n;
    }

    code = ENOMEM;
    if (!(r = malloc(n * sizeof *r)))
	goto Done;
    r[n-1] = 0;
    i = 0;
    ep = c->data + c->length;
    cp = c->data + com;
    while (cp < ep) {
	for (j = i; j > 0; --j)
	    r[j] = r[j-1];
	if (cp < ep && *cp == rbc)
	    ++cp;
	cp = memchr(cp, rbc, ep - cp);
	if (!cp) cp = ep;
	if (!(r[0] = malloc(1 + cp - c->data)))
	    goto Done;
	memcpy(r[0], c->data, cp - c->data);
	r[0][cp - c->data] = 0;
	++i;
    }
    if (com) {
	if (!(r[i] = malloc(com+1)))
		goto Done;
	memcpy(r[i], c->data, com);
	r[i][com] = 0;
	++i;
    }
    r[i] = 0;
    ep = s->data + s->length;
    cp = s->data + com;
    while (cp < ep) {
	if (cp < ep && *cp == rbc)
	    ++cp;
	cp = memchr(cp, rbc, ep - cp);
	if (!cp) cp = ep;
	if (!(r[i] = malloc(1 + cp - s->data))) break;
	memcpy(r[i], s->data, cp - s->data);
	r[i][cp - s->data] = 0;
	++i;
    }
    code = 0;
Done:
    if (!code) {
	*strings = r;
	r = 0;
    }
    if (r) {
	for (i = 0; r[i]; ++i)
	    free(r[i]);
	free(r);
	r = 0;
    }
    return code;
}

krb5_error_code
krb5i_hier_list(krb5_context context,
    const krb5_data *client, const krb5_data *server,
    int rbc,
    char ***strings)
{
    int i, j;

    *strings = 0;
    if (!rbc) {
	j = i = 0;
	if (client->length && *client->data == '/') i = '/';
	if (server->length && *server->data == '/') j = '/';
	if (i != j) return KRB5KRB_AP_ERR_ILL_CR_TKT;
	rbc = i ? '/' : '.';
    }
    switch(rbc) {
    case '/':
	return krb5i_x500_hier(context, client, server, rbc, strings);
    default:
	return krb5i_dns_hier(context, client, server, rbc, strings);
    }
}

krb5_error_code
krb5_walk_realm_tree(krb5_context context,
    const krb5_data *client,
    const krb5_data *server,
    krb5_principal **rt,
    int rbc)
{
    int code;
    char *names[4];
    char *t = 0;
    char **strings = 0;
    int i, free_hier;
    krb5_data data1[1], data2[1], *cr, *sr;
    krb5_principal *r = 0;

    *rt = 0;
    code = ENOMEM;
    t = malloc(client->length + server->length + 2);
    if (!t) goto Done;
    names[0] = "capaths";
    names[1] = t;
    names[2] = t+client->length+1;
    memcpy(names[1], client->data, client->length);
    names[1][client->length] = 0;
    memcpy(names[2], server->data, server->length);
    names[2][server->length] = 0;
    names[3] = 0;
    free_hier = 0;
    code = krb5i_config_get_strings(context, (const char*const*)names, &strings);
    if (!code && *strings)
    {
	if (!strcmp(*strings, ".")) {
	    *strings = 0;
	}
    } else {
	free_hier = 1;
	if (strings) free(strings);
	code = krb5i_hier_list(context, client, server, rbc, &strings);
    }
    if (code) goto Done;
    for (i = 0; strings[i]; ++i)
	;
    code = ENOMEM;
    if (!(r = malloc(sizeof *r * (i+2))))
    if (!r) goto Done;
    memset(r, 0, sizeof *r * (i+2));
    cr = (krb5_data *) client;
    sr = data1;
    i = 0;
    while (strings[i]) {
	sr->data = (unsigned char *) strings[i];
	sr->length = strlen(strings[i]);
	if ((code = krb5_tgtname(context, sr, cr, r+i)))
		goto Done;
	cr = sr;
	sr = (cr == data1) ? data2 : data1;
	++i;
    }
    *rt = r;
    r = 0;
Done:
    krb5_free_realm_tree(context, r);
    if (strings) {
	if (free_hier)
	    for (i = 0; strings[i]; ++i) free(strings[i]);
	free(strings);
    }
    if (t) free(t);
    return code;
}

static void
_free_data_list(krb5_data *r, int n)
{
    int i;

    if (!r) return;
    for (i = 0; i < n; ++i)
	if (r[i].data)
	    free(r[i].data);
    free(r);
}

krb5_error_code
krb5i_domain_x500_decode(krb5_context context,
    const krb5_data *tr,
    const krb5_data *client_realm,
    const krb5_data *server_realm,
    krb5_data **out,
    int *nr)
{
    int l, tl;
    int i, j, n, c, lastc, esc;
    int code;
    char *cp;
    char *tp, *temp = 0;
    krb5_data *r = 0, *newr = 0;
    krb5_data last[1];
    char **strings = 0;

    *out = 0;
    code = KRB5KRB_AP_ERR_ILL_CR_TKT;
    if (!(cp = memchr(tr->data, 0, tl = tr->length)))
	;
    else if (tr->data + tr->length - (unsigned char*) cp != 1) {
	return code;
    } else {
	--tl;
    }
    memset(last, 0, sizeof *last);
    if (!tl) return 0;
    n = 1;
    esc = 0;
    for (cp = (char *) tr->data, l = tl; l > 0; ++cp, --l) {
	if (esc) esc = 0;
	else switch(*cp) {
	case '\\':
	    esc = 1;
	    break;
	case ',':
	    ++n;
	}
    }
    if (!(r = malloc(sizeof *r * n))) {
	code = ENOMEM;
	goto Done;
    }
    l = tl + 1 + client_realm->length + server_realm->length;
    if (!(temp = malloc(l))) {
	code = ENOMEM;
	goto Done;
    }
    memset(r, 0, sizeof *r * n);
    i = 0;
    for (cp = (char *) tr->data, l = tl; l > 0; ) {
	esc = 0;
	lastc = 0;
	tp = temp;
	for ( ; l > 0; ++cp, --l) {
	    if (esc) {
		lastc = 0;
		esc = 0;
		*tp++ = *cp;
		continue;
	    }
	    switch(c = *cp) {
	    default:
		break;
	    case '\\':
		esc = 1;
		continue;
	    case ',':
		++cp; --l;
		goto Sep;
	    case ' ':
		if (tp == temp) {
		    last->length = 0;
		    continue;
		}
		break;
	    case '/':
		if (tp == temp) {
		    memcpy(tp, last->data, last->length);
		    tp += last->length;
		}
		break;
	    }
	    lastc = c;
	    *tp++ = c;
	}
	if (esc) goto Done;
Sep:
	switch (lastc) {
	case '.':
	    memcpy(tp, last->data, last->length);
	    tp += last->length;
	}
	r[i].length = tp-temp;
	if (!(r[i].data = malloc(r[i].length + 1))) {
	    code = ENOMEM;
	    goto Done;
	}
	memcpy(r[i].data, temp, r[i].length);
	r[i].data[r[i].length] = 0;
	*last = r[i];
	++i;
    }
    for (i = 0; i < n; i += c) {
	c = 1;
	if (r[i].length) continue;
	for (j = i+1; j < n; ++j)
	    if (r[j].length) break;
	code = krb5i_hier_list(context,
	    !i ? client_realm : r+i-1,
	    j==n ? server_realm : r+j,
	    0, &strings);
	if (code) goto Done;
	for (c = 0; strings[c]; ++c)
	    ;
	c -= 2;
	if (c != j - i) {
	    code = ENOMEM;
	    if (c < 0) goto Done;
	    newr = malloc(sizeof *r * (n + c + i - j));
	    if (!newr) goto Done;
	    memcpy(newr, r, sizeof *r * i);
	    memcpy(newr+i+c, r+j, sizeof *r * (n-j));
	    free(r);
	    r = newr;
	    n += (c + i - j);
	}
	for (j = 0; j < c; ++j) {
	    r[i+j].length = strlen(strings[j+1]);
	    r[i+j].data = (unsigned char *) strings[j+1];
	}
	if (strings[0]) free(strings[0]);
	if (strings[c+1]) free(strings[c+1]);
	free(strings);
	strings = 0;
    }
    code = 0;
    *out = r;
    *nr = n;
    r = 0;
Done:
    if (temp) free(temp);
    _free_data_list(r, n);
    if (strings) {
	for (i = 0; strings[i]; ++i) free(strings[i]);
	free(strings);
    }
    return code;
}

krb5_error_code
krb5i_check_transited_list(krb5_context context,
    const krb5_transited *tr,
    const krb5_data *client_realm,
    const krb5_data *server_realm)
{
    int code;
    krb5_principal *tgs = 0;
    krb5_data *realms = 0;
    int nr = 0;
    int i, j;
    static const char * const names[] = {"libdefaults",
	"transited_realms_reject", 0};
    char **strings = 0;

    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;

    switch(tr->tr_type) {
    case KRB5_DOMAIN_X500_COMPRESS:
	code = krb5i_domain_x500_decode(context, &tr->tr_contents,
	    client_realm, server_realm, &realms, &nr);
	break;
    default:
	code = KRB5KDC_ERR_TRTYPE_NOSUPP;
    }
    if (code) goto Done;

    code = krb5_walk_realm_tree(context, client_realm, server_realm,
	&tgs, 0);
    if (code) goto Done;

    code = KRB5KRB_AP_ERR_ILL_CR_TKT;
    for (i = 0; i < nr; ++i) {
	for (j = 0; strings[j]; ++j)
		if (strlen(strings[j]) == realms[i].length
			&& !strcmp(strings[j], (char *) realms[i].data))
		    goto Done;
	for (j = 0; tgs[j]; ++j)
	    if (krb5i_data_compare(realms+i, &tgs[i]->realm))
		break;
	if (!tgs[j]) goto Done;
    }
    code = 0;

Done:
    if (strings) {
	for (i = 0; strings[i]; ++i) free(strings[i]);
	free(strings);
    }
    krb5_free_realm_tree(context, tgs);
    _free_data_list(realms, nr);
    return code;
}

/*

special characters in transited encoding
,
\
trailing .
leading spaces

\ quotes next character
. ending means prepend previous path element
/ means prefix previous path element
<space>/ means this field should NOT be prefixed with the previous one

end points are not included.  Only intermediate points
in the middle.

for prefix & append, "previous" field is empty.

null field means "all domains inbetween" have been traversed.
for this, previous & next fields are the end points.

EDU,MIT.,ATHENA.,WASHINGTON.EDU,CS.
	->
    [1] EDU
    [2] MIT.EDU
    [3] ATHENA.MIT.EDU
    [4] WASHINGTON.EDU
    [5] CS.WASHINGTON.EDU 

/COM,/HP,/APOLLO, /COM/DEC
	->
[1] /COM/HP/APOLLO
[2] /COM/HP
[3] /COM
[4] /COM/DEC

,EDU, /COM
	->
[-1] client (ATHENA.MIT.EDU)
[0] MIT.EDU
[1] /COM
[endpoint] /COM/HP

*/
