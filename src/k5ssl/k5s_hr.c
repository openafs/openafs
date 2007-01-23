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

#include "afsconfig.h"
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

krb5_error_code
krb5_free_host_realm(krb5_context context, char * const * list)
{
    char **xp;
    int i;

    if ((xp = (char **) list)) {
	for (i = 0; xp[i]; ++i)
	    free(xp[i]);
	free(xp);
    }
    return 0;
}

/*
 * lookup TXT records for _kerberos.(hostname)
 * if found: return realms in record.
 * MIT only processes the first TXT record found.
 * heimdal processes all.
 */

krb5_error_code
krb5i_dns_find_realm(krb5_context context, const char *ahost, char ***listp)
{
    int code = -1, size, len, out, i, j;
    unsigned char *reply = 0, *p, *ep;
    int qdcount, ancount, nscount, arcount;
    unsigned short ahdr[5+3];
    struct hrdata {
	struct hrdata *next;
	char name[1];
    } *head, *x, **rp;
    int xcount;
    char **realms = 0;
    char buf[1024];
#ifdef HAVE_RES_NCLOSE
    struct __res_state nstate[1];

    memset(nstate, 0, sizeof *nstate);
    if (res_ninit(nstate)) {
	return EINVAL;
    }
#else
    if (res_init()) {
	return EINVAL;
    }
#endif
    snprintf (buf, sizeof buf, "_kerberos.%s", ahost);
    *listp = 0;
    head = 0;
    rp = &head;
    xcount = 1;
    size = 0;
    len = 4000;
    for (;;) {
	if (size < len) size = len;
	if (reply) free(reply);
	reply = malloc(size);
	if (!reply) {
	    code = ENOMEM;
	    goto Done;
	}
#ifdef HAVE_RES_NCLOSE
	len = res_nsearch(nstate, buf, C_IN, T_TXT, reply, size);
#else
	len = res_search(buf, C_IN, T_TXT, reply, size);
#endif
	if (len < 0) {
	    break;
	}
	if (size > len || len > 32768) break;
    }
    if (len > size) len = size;
    if (len < 12) goto Done;
    ep = reply + len;
    p = reply;
    qdcount= ntohs(2[(unsigned short *) reply]);
    ancount = ntohs(3[(unsigned short *) reply]);
    nscount = ntohs(4[(unsigned short *) reply]);
    arcount = ntohs(5[(unsigned short *) reply]);
    if (qdcount != 1) {
	code = EINVAL;
	goto Done;
    }
    p += 12;
    out = dn_expand(reply, ep, p, buf, sizeof buf);
    if (out < 0 || p + out + 4 > ep) {
	code = EINVAL;
	goto Done;
    }
    p += out;	/* [n]skip compressed domain */
    p += 4;	/* [2+2] skip type, class */
    for (j = ancount; j-- > 0; ) {
	out = dn_expand(reply, ep, p, buf, sizeof buf);
	if (out < 0 || p + out + 4 > ep) {
	    code = EINVAL;
	    goto Done;
	}
	p += out;
	if (ep-p < 10) {
	    code = EINVAL;
	    goto Done;
	}
	memcpy(ahdr, p, 10);
	p += 10;	/* [2+6+2] ntype, ttl+class, rdlen */
	out = ntohs(ahdr[4]);
	if (out < 0 || p + out + 4 > ep) {
	    code = EINVAL;
	    goto Done;
	}
	if (ntohs(ahdr[0]) == T_TXT) {
	    i = *p;
	    if (i+p+1 > ep) {
		code = EINVAL;
		goto Done;
	    }
	    if (p[i] == '.') --i;
	    x = malloc(sizeof *x + i);
	    if (!x) {
		code = ENOMEM;
		goto Done;
	    }
	    memset(x, 0, sizeof *x);
	    memcpy(x->name, p+1, i);
	    x->name[i] = 0;
	    *rp = x;
	    rp = &x->next;
	    ++xcount;
	}
	p += out;
    }
    if (!head) goto Done;
    realms = malloc(xcount * sizeof *realms);
    if (!realms) goto Done;
    i = 0;
    for (x = head; x; x = x->next) {
	if (!(realms[i++] = strdup(x->name))) {
	    code = ENOMEM;
	    goto Done;
	}
    }
    realms[i] = 0;
    *listp = realms;
    realms = 0;
    code = 0;
Done:
    if (realms) {
	for (i = 0; realms[i]; ++i) {
	    free(realms[i]);
	}
	free(realms);
    }
    while ((x = head)) {
	head = x->next;
	free(x);
    }
    if (reply) free(reply);
#ifdef HAVE_RES_NCLOSE
    res_nclose(nstate);
#endif
    if (code == EINVAL) code = -1;	/* permit the search to go on... */
    return code;
}

krb5_error_code
krb5_get_host_realm(krb5_context context, const char *ahost, char ***listp)
{
    struct addrinfo hints[1], *res = 0;
    char myname[256], *tofree = 0, *first, **realms, *host, *cp;
    char *names[3];
    int code;
    int out;
    int i;

    *listp = 0;
    if (!ahost) {
	if (gethostname(myname, sizeof myname) < 0) return errno;
	ahost = myname;
    }
    memset(hints, 0, sizeof *hints);
#if 0
    hints->ai_family = AF_INET;
#endif
    hints->ai_socktype = 0;
    if (ahost == myname)
	hints->ai_flags = AI_CANONNAME;
    else
	hints->ai_flags = AI_NUMERICHOST;
    code = getaddrinfo(ahost, NULL, hints, &res);
    if (ahost != myname) {
	if (!code) {
	    code = KRB5_ERR_NUMERIC_REALM;
	    goto Done;
	}
    }
    else switch(code) {
    default: code = EINVAL; goto Done;
    case EAI_SYSTEM: code = errno; goto Done;
    case EAI_MEMORY: code = ENOMEM; goto Done;
    case EAI_AGAIN: code = EAGAIN; goto Done;
#ifdef EAI_ADDRFAMILY
    case EAI_ADDRFAMILY:
#endif
#if EAI_NODATA != EAI_NONAME
    case EAI_NODATA:
#endif
    case EAI_NONAME:
	code = 0; break;
    case 0: break;
    }
    if (ahost == myname)
	ahost = res->ai_canonname;
    tofree = host = strdup(ahost);
    if (!host) {
	code = ENOMEM;
	goto Done;
    }
    for (cp = host; *cp; ++cp)
	if (isupper(*cp))
	    *cp = tolower(*cp);
    if (cp > host && *--cp == '.')
	*cp = 0;
    names[2] = 0;
    *names = "domain_realm";
    for (first = 0, cp = host; cp; ) {
	names[1] = cp;
	code = krb5i_config_get_strings(context, (const char *const*)names, &realms);
	if (code) goto Done;
	if (*realms) {
	    for (i = 0; realms[i]; ++i)
		if (!(realms[i] = strdup(realms[i]))) {
		    code = ENOMEM;
		    goto Done;
		}
	    break;
	}
	free(realms);
	realms = 0;
	if (*cp == '.') {
	    ++cp;
	    if (!first) first = cp;
	} else {
	    cp = strchr(cp, '.');
	}
    }
    if (!realms) {
	code = krb5_get_dns_fallback(context, "dns_lookup_realm", &out);
	if (code) goto Done;
	if (out) {
	    for (cp = host; *cp; ++cp) {
		code = krb5i_dns_find_realm(context, cp, &realms);
		if (!code) break;
		if (code != -1) goto Done;
		if (!(cp = strchr(cp, '.'))) break;
	    }
	}
    }
    if (!realms) {
	if (first) {
	    code = ENOMEM;
	    realms = malloc(2 * sizeof *realms);
	    if (!realms) goto Done;
	    realms[0] = strdup(first);
	    if (!realms[0]) goto Done;
	    realms[1] = 0;
	    for (cp = realms[0]; *cp; ++cp)
		if (islower(*cp))
		    *cp = toupper(*cp);
	} else {
	    code = ENOMEM;
	    realms = malloc(2 * sizeof *realms);
	    if (!realms) goto Done;
	    memset(realms, 0, 2 * sizeof *realms);
	    code = krb5_get_default_realm(context, realms);
	    if (code) goto Done;
	}
    }
    *listp = realms;
    realms = 0;
    code = 0;
Done:
    if (res) freeaddrinfo(res);
    if (tofree) free(tofree);
    if (realms) {
	krb5_free_host_realm(context, realms);
    }
    return code;
}
