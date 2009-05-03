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

/*
 * test name lookup
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifdef USING_K5SSL
#include "k5ssl.h"
#include "k5s_im.h"
#else
#include <krb5.h>
#define krb5i_timegm	timegm
#endif
struct addrlist {	/* XXX */
    struct addrinfo **addrs;
    int naddrs;
    int space;
};

extern int bin_dump();

int exitrc;
int errors;
int howlook;
int mflag;
int tflag;
int vflag;
int Dflag;
krb5_context k5context;

char *addrinfo_to_name(struct addrinfo *ap, char *buf, int len)
{
    char *cp;
    int i;
    switch(ap->ai_family) {
    case AF_INET:
	inet_ntop(ap->ai_family,
	    &((struct sockaddr_in *)ap->ai_addr)->sin_addr, buf, len);
	cp = buf + strlen(buf);
	if (cp - buf < len-1)
	    snprintf(cp, buf + len - cp,
		" port %d", ntohs(((struct sockaddr_in *)ap->ai_addr)->sin_port));
	break;
    case AF_INET6:
	inet_ntop(ap->ai_family,
	    &((struct sockaddr_in6 *)ap->ai_addr)->sin6_addr, buf, len);
	cp = buf + strlen(buf);
	if (cp - buf < len-1)
	    snprintf(cp, buf + len - cp,
		" port %d", ntohs(((struct sockaddr_in6 *)ap->ai_addr)->sin6_port));
	break;
    default:
	cp = buf;
	for (i = 0; i < ap->ai_addrlen; ++i) {
	    if (cp - buf >= len-1) break;
	    snprintf (cp, buf + len - cp, ".%02x"+!i, i[(unsigned char *)ap->ai_addr]);
	    cp += strlen(cp);
	}
    }
    return buf;
}

int
stripnl(char *line)
{
    char *cp;
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    if ((cp = strchr(line, '\r')))
	*cp = 0;
    return 0;
}

void
process(char *realm)
{
    krb5_data data[1];
#ifndef USING_HEIMDAL
    struct addrlist addrs[1];
    char *what;
#endif
    int code;
    int i;
    char buf[320];
#ifndef USING_MIT
    int flags;
#endif
#ifdef USING_HEIMDAL
    int type;
    krb5_krbhst_handle handle;
    krb5_krbhst_info *hi;
    struct addrinfo *ai, *a;
#endif

printf ("Looking up <%s>%s%s\n", realm, mflag ? " master" : "",
(tflag&1) ? " tcp-only" : (tflag&2) ? " tcp-first" : "");
    memset(data, 0, sizeof *data);
    data->data = (unsigned char *) realm;
    data->length = strlen(realm);
#ifdef USING_HEIMDAL
    flags = 0;
    code = -1;
    type = /* mflag ? KRB5_KRBHST_ADMIN : */ KRB5_KRBHST_KDC;
    if (mflag) flags |= KRB5_KRBHST_FLAGS_MASTER;
    if (tflag&2) flags |= KRB5_KRBHST_FLAGS_LARGE_MSG;
    code = krb5_krbhst_init_flags(k5context, realm, type, flags, &handle);
    if (code) {
	fprintf(stderr,"krb5_krbhst_init_flags <%s> failed - %d (%s)\n",
	    realm, code, afs_error_message(code));
	exitrc = 1;
    }
    if (!code) {
	i = 0;
	while (krb5_krbhst_next(k5context, handle, &hi) == 0) {
	    code = krb5_krbhst_get_addrinfo(k5context, hi, &ai);
	    if (code) break;
	    for (a = ai; a; a = a->ai_next) {
		++i;
		strcpy(buf, "?");
		addrinfo_to_name(a, buf, sizeof buf);
		printf ("%d: %saf=%d type=%d <%s>\n",
		    i,
		    hi->proto == KRB5_KRBHST_HTTP ?
			"http" : "",
		    a->ai_family,
		    a->ai_socktype,
		    buf);
	    }
	}		
	printf ("%d total\n", i);
    }
#else
    memset(addrs, 0, sizeof *addrs);
#ifdef USING_MIT
    switch(howlook) {
    default:
    case 0:
	what = "krb5i_locate_kdc";
	code = krb5_locate_kdc(k5context, data, addrs, mflag, 0, 0);
	break;
#if 0
    case 'c':
	what = "krb5i_locate_by_conf";
	code = krb5_locate_srv_conf(k5context, data, addrs, mflag, 0, 0);
	break;
    case 'd':
	what = "krb5i_locate_by_dns";
	code = krb5_locate_srv_dns_1(data, "_kerberos", "_udp", addrs, 0);
	break;
#endif
    }
#else
    flags = 0;
    if (mflag) flags |= USE_MASTER;
    if (tflag) flags |= TCP_FIRST;
    switch(howlook) {
    case 0:
	what = "krb5i_locate_kdc";
	code = krb5i_locate_kdc(k5context, data, addrs, flags);
	break;
    case 'c':
	what = "krb5i_locate_by_conf";
	code = krb5i_locate_by_conf(k5context, data, addrs, flags);
	break;
    case 'd':
	what = "krb5i_locate_by_dns";
	code = krb5i_locate_by_dns(k5context, data, addrs, flags);
	break;
    default:
	code = howlook;
	what = "bad howlook";
	break;
    }
#endif
    if (code) {
	fprintf (stderr, "%s <%s> failed - %d (%s)\n", what, realm,
	    code, afs_error_message(code));
	exitrc = 1;
    } else {
	for (i = 0; i < addrs->naddrs; ++i) {
	    strcpy(buf, "?");
	    addrinfo_to_name(addrs->addrs[i], buf, sizeof buf);
	    printf ("%d: af=%d type=%d <%s>\n",
		i,
		addrs->addrs[i]->ai_family,
		addrs->addrs[i]->ai_socktype,
		buf);
	}
	printf ("%d total\n", i);
    }

#ifdef USING_MIT
    krb5int_free_addrlist(addrs);
#else
    krb5i_free_addrlist(addrs);
#endif
#endif
}

int
main(int argc, char **argv)
{
    char *argp;
    char *progname = argv[0];
    int f;
    int code;

    f = 0;
    while (--argc > 0) if (*(argp = *++argv) == '-')
    while (*++argp) switch(*argp) {
    case 'm':
	++mflag;
	break;
    case 'c':
    case 'd':
#if defined(USING_HEIMDAL) || defined(USING_MIT)
	fprintf (stderr,"-%c ignored here\n", *argp);
#endif
	howlook = *argp;
	break;
    case 't':
	++tflag;
	break;
    case 'D':
	++Dflag;
	break;
    case 'v':
	++vflag;
	break;
    case '-':
	break;
    default:
    Usage:
	fprintf(stderr,"Usage: %s [-cdmtvD] realm ...\n", progname);
	exit(1);
    } else {
	if (!k5context && (code = krb5_init_context(&k5context))) {
	    fprintf(stderr,"krb5_init_context failed - %d (%s)\n",
		code, afs_error_message(code));
	    exit(2);
	}
	++f;
	process(argp);
    }

    if (!f) goto Usage;

    if (k5context) krb5_free_context(k5context);

    exit(exitrc);
}
