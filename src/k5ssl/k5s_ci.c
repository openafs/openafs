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

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "k5ssl.h"

#define DEFAULT_CLOCKSKEW	300
#define DEFAULT_UDP_PREF_LIMIT	1465
#define DEFAULT_CHECKSUM_TYPE	CKSUMTYPE_RSA_MD5
#define DEFAULT_DNS_FALLBACK	1

typedef int string_to_x_f(char *, void *);

krb5_error_code
krb5i_words_to_ints(char *types,
    string_to_x_f *f,
    void **rp,
    int sizer,
    int *countp)
{
    char *work;
    char *cp, *wp;
    int c, i;
    int count;
    char *result;

    work = malloc(strlen(types)+1);
    if (!work) {
	return ENOMEM;
    }
    cp = types;
    wp = work;
    count = 0;
    for (;;) {
	while (isspace(*cp)) ++cp;
	if (*cp == ',') ++cp;
	while (isspace(*cp)) ++cp;
	if (!*cp) break;
	while ((c = *cp) && c != ',' && !isspace(c))
	    ++cp, *wp++ = c;
	*wp++ = 0;
	++count;
    }
    result = malloc(sizer * (count+!countp));
    if (!result) {
	free(work);
	return ENOMEM;
    }
    wp = work;
    i = 0;
    for (c = 0; c < count; ++c) {
	if (!(*f)(wp, result+i*sizer)) ++i;
	while (*wp++) ;
    }
    if (countp)
	*countp = i;
    else
	memset(result + i*sizer, 0, sizer);
    if (rp)
	*rp = result;
    else free(result);
    free(work);
    return 0;
}

struct gtk_arg {
    int count;
    struct gtk_item {
	krb5_enctype e;
	int s;
    } *result;
};

int
krb5i_gtk_helper(void *a, krb5_enctype enctype,
	char *const *names,
	void (*block_size)(unsigned int *, unsigned int *),
	void (*key_size)(unsigned int *, unsigned int *))
{
    struct gtk_arg *ga = a;
    unsigned int mbs, bs, kb, kl;
    int s;
    if (ga->result) {
	block_size(&mbs, &bs);
	key_size(&kb, &kl);
	ga->result[ga->count].e = enctype;
	s = kb * mbs + enctype;
	switch(enctype) {
	case ENCTYPE_DES3_CBC_SHA1:
	    s = (s * 2)/3;
	    break;
	case ENCTYPE_ARCFOUR_HMAC:
	    s *= 6;
	    break;
	}
	ga->result[ga->count].s = s;
    }
    ++ga->count;
    return -1;
}

int
krb5i_gtk_cmp(const void *a, const void *b)
{
    const struct gtk_item *ap = a, *bp = b;
    return bp->s - ap->s;
}

krb5_error_code
krb5_get_x_ktypes(krb5_context context,
    krb5_const_principal princ, krb5_enctype ** rp, const char* name)
{
    char *names[3];
    int c, i;
    char *enctypes;
    const char **strings = 0;

    names[0] = "libdefaults";
    names[1] = name;
    names[2] = 0;
    c = krb5i_config_get_strings(context, names, &strings);
    if (c) return c;
    enctypes = *strings;
    free(strings);
    if (!enctypes) {
	struct gtk_arg ga[1];
	memset(ga, 0, sizeof *ga);
	c = krb5i_iterate_enctypes(krb5i_gtk_helper, ga);
	if (c && c != -1) return c;
	ga->result = malloc(ga->count * sizeof *ga->result);
	if (!ga->result) return ENOMEM;
	ga->count = 0;
	c = krb5i_iterate_enctypes(krb5i_gtk_helper, ga);
	if (c && c != -1) {
	    free(ga->result);
	    return c;
	}
	/* XXX ga->results should be sorted in some sensible order??? */
	qsort(ga->result, ga->count, sizeof *ga->result, krb5i_gtk_cmp);
	*rp = malloc((ga->count+1) * sizeof **rp);
	if (!*rp)
	    c = ENOMEM;
	else {
	    c = 0;
	    for (i = 0; i < ga->count; ++i) {
		(*rp)[i] = ga->result[i].e;
	    }
	    (*rp)[i] = 0;
	}
	free(ga->result);
	return c;
    }
    return krb5i_words_to_ints(enctypes,
	(string_to_x_f *) krb5_string_to_enctype,
	(void *) rp,
	sizeof **rp, 0);
}

krb5_error_code
krb5_get_tgs_ktypes(krb5_context context,
    krb5_const_principal princ, krb5_enctype ** rp)
{
    return krb5_get_x_ktypes(context, princ, rp, "default_tgs_enctypes");
}

krb5_error_code
krb5_get_tkt_ktypes(krb5_context context,
    krb5_const_principal princ, krb5_enctype ** rp)
{
    return krb5_get_x_ktypes(context, princ, rp, "default_tkt_enctypes");
}

const char *
krb5_cc_default_name(krb5_context context)
{
    char *cp = 0;
    /* XXX should also handle profile_get_string ???
     */
    if (!krb5i_issuid())
	cp = getenv("KRB5CCNAME");
    else
	cp = "";
    return cp;
}

krb5_error_code
krb5_cc_default(krb5_context context, krb5_ccache *cache)
{
    krb5_error_code code;
    const char *s;

    s = krb5_cc_default_name(context);
    code = krb5_cc_resolve(context, s, cache);
    return code;
}

krb5_error_code
krb5i_get_integer(krb5_context context, const char * const *names,
    int def, int *out)
{
    char **strings = 0;
    int code;
    char *ep;

    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    *out = def; if (!*strings) goto Done;
    errno = 0;
    *out = strtol(*strings, &ep, 0);
    if (ep == *strings || *ep || errno == ERANGE)
	code = PROF_BAD_INTEGER;
Done:
    if (strings) free(strings);
    return code;
}

krb5_error_code
krb5_get_clockskew(krb5_context context, int *out)
{
    static const char * const names[] = {"libdefaults", "clockskew", 0};
    return krb5i_get_integer(context, names, DEFAULT_CLOCKSKEW, out);
}

krb5_error_code
krb5_get_udp_preference_limit(krb5_context context, int *out)
{
    static const char * const names[] = {"libdefaults", "udp_preference_limit", 0};
    return krb5i_get_integer(context, names, DEFAULT_UDP_PREF_LIMIT, out);
}

krb5_error_code
krb5_get_kdc_default_options(krb5_context context, int *out)
{
    static const char * const names[] = {"libdefaults", "kdc_default_options", 0};
    return krb5i_get_integer(context, names, KDC_OPT_RENEWABLE_OK, out);
}

krb5_error_code
krb5i_get_boolean(krb5_context context, const char * const *names,
    int def, int *out)
{
    char **strings = 0;
    int code;
    char *ep;
    int n, i;
    static const char * const yes[] = {
	"y", "yes", "true", "t", "on",
    0};

    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    *out = def;
    if (!*strings) goto Done;
    n = !!strtol(*strings, &ep, 0);
    if (ep == *strings || *ep)
	for (n = i = 0; yes[i]; ++i)
	    if (strcasecmp(yes[i], *strings)) {
		n = 1;
		break;
	    }
    *out = n;
Done:
    if (strings) free(strings);
    return code;
}

krb5_error_code
krb5_get_dns_fallback(krb5_context context, char *which, int *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = which;
    names[2] = 0;
    code = krb5i_get_boolean(context, (const char *const*) names,
	-1, out);
    if (!code && *out == -1) {
	names[1] = "dns_fallback";
	code = krb5i_get_boolean(context, (const char *const*) names,
	    DEFAULT_DNS_FALLBACK, out);
    }
    return code;
}

krb5_error_code
krb5_get_forwardable(krb5_context context, int *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = "forwardable";
    names[2] = 0;
    code = krb5i_get_boolean(context, (const char *const *)names,
	0, out);
    return code;
}

krb5_error_code
krb5_get_proxiable(krb5_context context, int *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = "proxiable";
    names[2] = 0;
    code = krb5i_get_boolean(context, (const char *const *)names,
	0, out);
    return code;
}

krb5_error_code
krb5_get_noaddresses(krb5_context context, int *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = "noaddresses";
    names[2] = 0;
    code = krb5i_get_boolean(context, (const char *const *)names,
	0, out);
    return code;
}

/* #d#h#m#s / [-]h:m:s */
krb5_error_code
krb5_string_to_deltat(char *cp, krb5_deltat *out)
{
    char *ep;
    int n;
    int m;
    krb5_deltat r;

    *out = 0;
    m = 60*60;
    while (*cp) {
	switch(*cp++) {
	case 's':
	    break;
	case 'm':
	    n *= 60; break;
	case 'h':
	    n *= (60*60); break;
	case 'd':
	    n *= (24*60*60); break;
	case ':':
	    n *= m; m /= 60; break;
	default:
	    n = strtol(--cp, &ep, 10);
	    if (cp == ep)
		return KRB5_DELTAT_BADFORMAT;
	    cp = ep;
	    continue;
	}
	r += n;
	n = 0;
    }
    if (n) r += n;
    *out = r;
    return 0;
}

krb5_error_code
krb5i_get_deltat(krb5_context context, const char *const *names, int def, krb5_deltat *out)
{
    char **strings = 0;
    int code;
    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    if (!*strings) {
	*out = def;
	goto Done;
    }
    code = krb5_string_to_deltat(*strings, out);
Done:
    if (strings) free(strings);
    return code;
}

krb5_error_code
krb5_get_ticket_lifetime(krb5_context context, char *realm, krb5_deltat *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = "ticket_lifetime";
    names[2] = 0;
    code = krb5i_get_deltat(context, (const char *const *)names, 24*60*60, out);
    return code;
}

krb5_error_code
krb5_get_renew_lifetime(krb5_context context, char *realm, krb5_deltat *out)
{
    char *names[3];
    int code;

    names[0] = "libdefaults";
    names[1] = "renew_lifetime";
    names[2] = 0;
    code = krb5i_get_deltat(context, (const char *const *)names, 0, out);
    return code;
}

krb5_error_code
krb5i_get_checksum_type(krb5_context context, const char * const *names,
    int def, krb5_cksumtype *out)
{
    char **strings = 0;
    int code, i;
    char *ep;

    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    if (!*strings) {
	*out = def;
	goto Done;
    }
    for (i = 0; strings[i]; ++i) {
	if (!krb5_string_to_cksumtype(strings[i], out)) goto Done;
    }
    errno = 0;
    *out = strtol(*strings, &ep, 0);
    if (ep == *strings || *ep || errno == ERANGE)
	code = PROF_BAD_INTEGER;
Done:
    if (strings) free(strings);
    return code;
}

krb5_error_code
krb5_get_kdc_req_checksum_type(krb5_context context, krb5_cksumtype *out)
{
    static const char * const names[] =
	{"libdefaults", "kdc_req_checksum_type", 0};
    return krb5i_get_checksum_type(context, names, DEFAULT_CHECKSUM_TYPE, out);
}

krb5_error_code
krb5_get_ap_req_checksum_type(krb5_context context, krb5_cksumtype *out)
{
    static const char * const names[] =
	{"libdefaults", "ap_req_checksum_type", 0};
    return krb5i_get_checksum_type(context, names, DEFAULT_CHECKSUM_TYPE, out);
}

struct preauth_type {
    int type;
    char *what;
};

static const struct preauth_type preauth_types[] = {
	{ KRB5_PADATA_NONE, "none" },
	{ KRB5_PADATA_ENC_TIMESTAMP, "enc-timestamp" },
{ 0, 0 } } ;

krb5_error_code
krb5_string_to_preauthtype(char *cp, krb5_preauthtype *out)
{
    int i;
    for (i = 0; preauth_types[i].what; ++i)
	if (!strcasecmp(cp, preauth_types[i].what)) {
	    *out = preauth_types[i].type;
	    return 0;
	}
    return EINVAL;
}

krb5_error_code
krb5_get_preauth_list(krb5_context context,
    krb5_const_principal princ, krb5_preauthtype ** rp, int *np)
{
    char *names[3];
    int code;
    char *preauthtypes;
    char **strings = 0;

    names[0] = "libdefaults";
    names[1] = "default_preauthtypes";
    names[2] = 0;
    code = krb5i_config_get_strings(context, names, &strings);
    if (code) return code;
    preauthtypes = *strings;
    free(strings);
    if (!preauthtypes) {
	*rp = 0;
	return 0;
    }
    return krb5i_words_to_ints(preauthtypes,
	(string_to_x_f *) krb5_string_to_preauthtype,
	(void *) rp,
	sizeof **rp, np);
}
