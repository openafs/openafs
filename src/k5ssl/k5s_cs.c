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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include "k5ssl.h"
#include "k5s_im.h"

#define K5BUFSIZE 16384

struct send_receive_context;
typedef int sr_listen_function (krb5_context, struct send_receive_context *,
    void *, short, struct timeval *);
struct send_receive_context {
    struct timeout_request {
	struct timeout_request *next;
	int howlong;
	int (*f)(krb5_context, struct send_receive_context *, void *);
	void *arg;
    } *timeout_list;
    struct pollfd *fds;
    sr_listen_function **f;
    void **a;
    int nfds;
    int nspace;
    struct sr_cleanup {
	struct sr_cleanup *next;
	void (*f)(krb5_context, struct send_receive_context *, void *);
	void *arg;
    } *cleanups;
};

static void
free_fds(krb5_context context, struct send_receive_context *srq, void *a)
{
    if (srq->fds) free(srq->fds);
}

int
krb5i_sr_listen(struct send_receive_context *srq,
    sr_listen_function *f,
    void * a,
    int fd,
    short events)
{
    int x;

    for (x = 0; x < srq->nfds; ++x)
	if (srq->fds[x].fd == fd)
	    break;
    if (x < srq->nfds)
	;
    else {
	if (srq->nfds >= srq->nspace) {
	    struct pollfd *fds;
	    sr_listen_function **f;
	    void **a;
	    void *x;
	    int n,  nb;
	    n = (srq->nspace+5)*2;
	    nb = (int) (n +
		(void **)((n +
		    (int (**)())(n +
			((struct pollfd *)0)))));
	    x = malloc(nb);
	    if (!x) return ENOMEM;
	    fds = x; x = (fds+n);
	    f = x; x = (f+n);
	    a = x;
	    if (srq->nspace) {
		memcpy(fds, srq->fds, srq->nspace * sizeof *fds);
		memcpy(f, srq->f, srq->nspace * sizeof *fds);
		memcpy(a, srq->a, srq->nspace * sizeof *fds);
		free(srq->fds);
	    } else krb5i_sr_cleanup(srq, free_fds, 0);

	    srq->fds = fds;
	    srq->f = f;
	    srq->a = a;
	    srq->nspace = n;
	}
	x = srq->nfds++;
	memset(srq->fds+x, 0, sizeof *srq->fds);
    }
    srq->fds[x].fd = fd;
    srq->fds[x].events = events;
    srq->f[x] = f;
    srq->a[x] = a;
    return 0;
}

int
krb5i_sr_unlisten(struct send_receive_context *srq,
    int fd)
{
    int x;

    for (x = 0; x < srq->nfds; ++x)
	if (srq->fds[x].fd == fd)
	    break;
    if (x >= srq->nfds) {
	return ENOENT;
    }
    --srq->nfds;
    while (x < srq->nfds) {
	srq->fds[x] = srq->fds[x+1];
	++x;
    }
    return 0;
}

int
krb5i_sr_timeout(struct send_receive_context *srq,
    int (*f)(krb5_context, struct send_receive_context *, void *),
    void * a,
    int timeout)
{
    struct timeout_request **head, *rq, *p;

    head = &srq->timeout_list;
    rq = (struct timeout_request *) malloc(sizeof *rq);
    if (!rq) return ENOMEM;
    memset(rq, 0, sizeof *rq);
    rq->f = f;
    rq->arg = a;
    while ((p = *head) && p->howlong <= timeout) {
	timeout -= p->howlong;
	head = &p->next;
    }
    if (p) p->howlong -= timeout;
    rq->howlong = timeout;
    rq->next = p;
    *head = rq;
    return 0;
}

int
krb5i_sr_untimeout(struct send_receive_context *srq,
    int (*f)(krb5_context, struct send_receive_context *, void *),
    void * a)
{
    struct timeout_request **head, *p;
    int r;

    head = &srq->timeout_list;
    r = -1;
    while ((p = *head)) {
	if ((f && f != p->f) || (a && a != p->arg)) {
	    head = &p->next;
	    continue;
	}
	r = p->howlong;
	*head = p->next;
	free(p);
    }
    return r;
}

int
krb5i_sr_cleanup(struct send_receive_context *srq,
    void (*f)(krb5_context, struct send_receive_context *srq, void *),
    void * a)
{
    struct sr_cleanup *rq;

    rq = (struct sr_cleanup *) malloc(sizeof *rq);
    if (!rq) return ENOMEM;
    memset(rq, 0, sizeof *rq);
    rq->f = f;
    rq->arg = a;
    rq->next = srq->cleanups;
    srq->cleanups = rq;
    return 0;
}

static void
free_timeouts(krb5_context context, struct send_receive_context *srq, void *a)
{
    krb5i_sr_untimeout(srq, 0, 0);
}

krb5_error_code
krb5i_sendrecv(krb5_context context,
    int (*initf)(krb5_context, struct send_receive_context *, void *),
    void * a)
{
    int r, tmo, d, i;
    struct timeout_request *p;
    struct send_receive_context srq[1];
    struct sr_cleanup *sc;
    struct timeval tv[1];

    memset(srq, 0, sizeof *srq);
    krb5i_sr_cleanup(srq, free_timeouts, 0);
    r = initf(context, srq, a);
    if (r != -1) goto Done;
    r = d = 0;
    for (;;) {
	tmo = -1;
	for (;;) {
	    p = srq->timeout_list;
	    if (!p) break;
	    if (p->howlong > d) {
		p->howlong -= d;
		tmo = p->howlong;
		break;
	    }
	    d -= p->howlong;
	    srq->timeout_list = p->next;
	    r = p->f(context, srq, p->arg);
	    free(p);
	    if (r != -1) goto Done;
	}
	if (!srq->nfds && tmo == -1) {
	    r = KRB5_KDC_UNREACH;
	    goto Done;
	}
	gettimeofday(tv, 0);
	d = -(tv->tv_sec * 1000000 + tv->tv_usec);
	r = poll(srq->fds, srq->nfds, tmo);
	if (r < 0) {
	    r = errno;
	    goto Done;
	}
	gettimeofday(tv, 0);
	d += (tv->tv_sec * 1000000 + tv->tv_usec);
	d += 100; d /= 1000;	/* bias towards underestimating delay */
	if (d < 0 || d > 500+(tmo*2)) d = 0;
	r = -1;
	for (i = 0; i < srq->nfds; ++i) {
	    if (srq->fds[i].revents) {
		int x;
		x = srq->fds[i].fd;
		r = srq->f[i](context, srq, srq->a[i], srq->fds[i].revents, tv);
		if (r != -1) goto Done;
		i -= srq->fds[i].fd != x;
	    }
	}
    }
Done:
    while ((sc = srq->cleanups)) {
	srq->cleanups = sc->next;
	sc->f(context, srq, sc->arg);
	free(sc);
    }
    return r;
}

struct addrlist {
    struct addrinfo **addrs;
    int naddrs;
    int space;
};

void
krb5i_free_addrlist(struct addrlist *ap)
{
    int i;

    if (!ap->naddrs) return;
    for (i = 0; i < ap->naddrs; ++i)
	freeaddrinfo(ap->addrs[i]);
    free(ap->addrs);
    memset(ap, 0, sizeof *ap);
}

static int
lookup_add_host(struct addrlist *addrs,
    char *host,
    int hostlen,
    char **ports,
    int socktype)
{
    struct addrinfo hints[1], *res = 0, *next;
    int j;
    int code = 0;
    char *buf = 0;

    if (host[hostlen]) {
	buf = malloc(hostlen + 1);
	if (!buf) {
	    code = ENOMEM;
	    goto Done;
	}
	memcpy(buf, host, hostlen);
	buf[hostlen] = 0;
	host = buf;
    }
    for (j = 0; ports[j]; ++j) {
	memset(hints, 0, sizeof *hints);
#if 0
	hints->ai_family = AF_INET;
#endif
	hints->ai_socktype = socktype;
	code = getaddrinfo(host, ports[j], hints, &res);
	switch(code) {
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
	while (res) {
	    next = res->ai_next;
	    if (addrs->naddrs >= addrs->space) {
		int n = (addrs->space+5)*2;
		struct addrinfo **more;
		more = malloc(n * sizeof *more);
		if (!more) {
		    code = ENOMEM;
		    goto Done;
		}
		if (addrs->space)
		    memcpy(more, addrs->addrs, addrs->space * sizeof *more);
		free(addrs->addrs);
		addrs->addrs = more;
		addrs->space = n;
	    }
	    addrs->addrs[addrs->naddrs++] = res;
	    res->ai_next = 0;
	    res = next;
	}
    }
Done:
    if (buf) free(buf);
    if (res) freeaddrinfo(res);
    return code;
}

static int
find_colon_or_end(char *s)
{
    char *cp;
    int r;
    cp = strchr(s, ':');
    if (cp) r = cp - s; else r = strlen(s);
    return r;
}

krb5_error_code
krb5i_locate_by_conf(krb5_context context,
    const krb5_data *realm,
    struct addrlist *addrs,
    int flags)
{
    int code = 0;
    char *names[4];
    char **kdcs = 0, **masters = 0, **intersect = 0;
    char *ep;
    int i, j, k, a, l;
    int sl[3];
    char **pl;
    void *tofree = 0;
    static char *pl88_750[] = {"88","750",0};
    static char *pl88[] = {"88",0};
    static char *kdc_field[] = {"master_kdc", "kdc", 0};

    names[0] = "realms";
    names[1] = (char *) realm->data;	/* XXX */
    names[3] = 0;
    for (i=!(flags & USE_MASTER); kdc_field[i]; ++i) {
	names[2] = kdc_field[i];
	if (kdcs) free(kdcs);
	kdcs = 0;
	code = krb5i_config_get_strings(context, (const char *const*)names, &kdcs);
    Bad:
	switch (code) {
	default:
	    code = KRB5_REALM_UNKNOWN;
	    goto Done;
	case 0: ;
	}
	if (!(flags & USE_MASTER) || *kdcs) break;
    }
    if ((flags & USE_MASTER) && i) {
	names[2] = "admin_server";
	code = krb5i_config_get_strings(context,
		(const char *const*)names,
		&masters);
	if (code) goto Bad;
	for (i = 0; kdcs[i]; ++i)
	    ;
	++i;
	intersect = malloc(i * sizeof *intersect);
	if (!intersect) {
	    code = ENOMEM;
	    goto Done;
	}
	k = 0;
	for (i = 0; kdcs[i]; ++i) {
	    a = find_colon_or_end(kdcs[i]);
	    for (j = 0; masters[j]; ++j) {
		if (a != find_colon_or_end(masters[j])) continue;
		if (strncasecmp(kdcs[i], masters[j], a)) continue;
		intersect[k++] = kdcs[i];
		break;
	    }
	}
	intersect[k] = 0;
	free(kdcs);
	kdcs = intersect;
	intersect = 0;
    }
    k = 0;
    if (flags & TCP_ONLY) {
	sl[k++] = SOCK_STREAM;
    } else {
	sl[k] = (flags&TCP_FIRST) ? SOCK_STREAM : SOCK_DGRAM;
	sl[k+1] = sl[k] ^ (SOCK_DGRAM^SOCK_STREAM);
	k += 2;
    }
    sl[k] = 0;
    for (j = 0; sl[j]; ++j) for (i = 0; kdcs[i]; ++i) {
	a = find_colon_or_end(kdcs[i]);
	k = 0;
	if (kdcs[i][a]) {
	    ep = kdcs[i]+a+1;
	    l = strlen(ep)+1;
	    for (k = 0; (ep = strchr((ep), ',')); ++ep, ++k)
		;
	    tofree = malloc(l + (k+2)*sizeof*pl);
	    if (!tofree) {
		code = ENOMEM;
		goto Done;
	    }
	    pl = tofree;
	    ep = (char*)(pl + k + 2);
	    memcpy(ep, kdcs[i]+a+1, l);
	    for (k = 0;; ) {
		pl[k++] = ep;
		if (!(ep = strchr(ep, ',')))
		    break;
		*ep++ = 0;
	    }
	    pl[k] = 0;
	} else if (sl[j] == SOCK_DGRAM) {
	    pl = pl88_750;
	} else {
	    pl = pl88;
	}
	code = lookup_add_host(addrs, kdcs[i], a, pl, sl[j]);
	if (code) goto Done;
	if (tofree) free(tofree);
	tofree = 0;
    }
Done:
    if (tofree) free(tofree);
    if (kdcs) free(kdcs);
    if (masters) free(masters);
    if (intersect) free(intersect);
    return code;
}
struct rdata {
    struct rdata *next;
    int priority, weight, port, socktype;
    char name[1];
};

int
krb5i_randomize_srv(krb5_context context,
    int *sl,
    struct rdata **headp)
{
    unsigned r;
    struct rdata *this, *x, **rp, **thisp;
    int code, i, j, socktype;
    int prio, nextprio, sum;
    krb5_data rndstate[1];

    code = 0;
    rndstate->data = (unsigned char *) &r;
    rndstate->length = sizeof r;
    for (i = 0;i < 2; ++i) {
	socktype = sl[i];
	for (prio = 0; prio < 65536; prio = nextprio) {
	    sum = 0;
	    nextprio = 65536;
	    this = 0;
	    thisp = &this;
	    for (rp = headp; (x = *rp); ) {
		if (x->socktype != socktype || x->priority != prio) {
		    if (x->socktype == socktype && x->priority > prio && nextprio > x->priority)
			nextprio = x->priority;
		    rp = &x->next;
		    continue;
		}
		sum += (1+x->weight);
		*rp = x->next;
		x->next = 0;
		*thisp = x;
		thisp = &x->next;
	    }
	    code = 0;
	    while (this) {
		if (!sum) ++sum;
		code = krb5_c_random_make_octets(context, rndstate);
		j = r % sum;
		for (thisp = &this; (x = *thisp); thisp = &x->next) {
		    j -= (x->weight+1);
		    if (j < 0 || !x->next) break;
		}
		sum -= (x->weight+1);
		*thisp = x->next;
		x->next = *rp;
		*rp = x;
		rp = &x->next;
	    }
	    if (code) break;
	}
    }
    return code;
}

/*
rfc 4120:
.locate host of form:
service.protocol.realm
where service is: {_kerberos _kerberos-master _kerberos-adm _krb524 _kpasswd}
protocol is: _udp _tcp
force realm-is-fully-qualified (dns trailing . behavior.)
response is something like:
_kerberos._udp.ATHENA.MIT.EDU. 21570 IN SRV     0 0 88 KERBEROS-2.MIT.EDU.
rfc 2782:
_(service)._proto.Name TTL Class SRV prio weight port target
ttl=time to live(dns name lifetime)
class=in
select lowest priority, then randomly within priority selecting larger
weights more often.  Messy.
note that target="." means don't contact this target.
*/
krb5_error_code
krb5i_locate_by_dns(krb5_context context,
    const krb5_data *realm,
    struct addrlist *addrs,
    int flags)
{
    int code, out, i, j, socktype, l;
    int size, len;
    char buf[1024];
    char *pl[2];
    int sl[3];
    unsigned short ahdr[5+3];
    unsigned char *reply = 0, *p, *ep;
    int qdcount, ancount, nscount, arcount;
#ifdef HAVE_RES_NCLOSE
    struct __res_state nstate[1];
#endif
    struct rdata *head, *x, **rp;
    char *tcp_udp[] = {"udp", "tcp", 0};
    char *service[] = {"kerberos", "kerberos-master", 0};

    code = krb5_get_dns_fallback(context, "dns_lookup_kdc", &out);
    if (code) return code;
    if (!out) return -1;

    head = 0;
    rp = &head;
#ifdef HAVE_RES_NCLOSE
    memset(nstate, 0, sizeof *nstate);
    if (res_ninit(nstate)) {
	return EINVAL;
    }
#else
    if (res_init()) {
	return EINVAL;
    }
#endif
    size = 0;
    len = 4000;
    for (i = 0;i < 2; ++i) {
	if (i && (flags & TCP_ONLY)) break;
	j = i ^ !!(flags & USE_TCP);
	socktype = SOCK_DGRAM;
	if (j) socktype ^= (SOCK_DGRAM|SOCK_STREAM);
	sl[i] = socktype;
	snprintf(buf, sizeof buf, "_%s._%s.%.*s.",
	    service[!!(flags & USE_MASTER)],
	    tcp_udp[j],
	    realm->length, realm->data);
	for (;;) {
	    if (size < len) size = len;
	    if (reply) free(reply);
	    reply = malloc(size);
	    if (!reply) {
		code = ENOMEM;
		goto Done;
	    }
#ifdef HAVE_RES_NCLOSE
	    len = res_nsearch(nstate, buf, C_IN, T_SRV, reply, size);
#else
	    len = res_search(buf, C_IN, T_SRV, reply, size);
#endif
	    if (len < 0) {
#if 0
printf ("Can't resolve <%s>\n", buf);
#endif
		break;
	    }
	    if (size > len || len > 32768) break;
	}
	if (len < 0) continue;
	if (len > size) len = size;
	if (len < 12) {
	    code = KRB5_REALM_CANT_RESOLVE;
	    goto Done;
	}
	ep = reply + len;
	p = reply;
	qdcount = ntohs(2[(unsigned short *) reply]);
	ancount = ntohs(3[(unsigned short *) reply]);
	nscount = ntohs(4[(unsigned short *) reply]);
	arcount = ntohs(5[(unsigned short *) reply]);
	p += 12;
	if (qdcount != 1) {
		code = EINVAL;
		goto Done;
	}
	out = dn_expand(reply, ep, p, buf, sizeof buf);
	if (out < 0 || p + out + 4 > ep) {
	    code = EINVAL;
	    goto Done;
	}
	p += out;	/* [n]skip compressed domain */
	p += 4;		/* [2+2] skip type, class */
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
	    if (ntohs(ahdr[0]) == T_SRV) {
		dn_expand(reply, ep, p+6, buf, sizeof buf);
		memcpy(ahdr+5, p, 6);
#if 0
printf ("%d %d %d %s\n",
ntohs(ahdr[5]),
ntohs(ahdr[6]),
ntohs(ahdr[7]),
buf);
#endif
		x = malloc(sizeof *x + (l = strlen(buf)));
		if (!x) {
		    code = ENOMEM;
		    goto Done;
		}
		memset(x, 0, sizeof *x);
		x->priority = ntohs(ahdr[5]);
		x->weight = ntohs(ahdr[6]);
		x->port = ntohs(ahdr[7]);
		x->socktype = socktype;
		memcpy(x->name, buf, l+1);
		*rp = x;
		rp = &x->next;
	    }
	    p += out;
	}
    }
    sl[i] = 0;
    code = krb5i_randomize_srv(context, sl, &head);
    if (code) goto Done;
    pl[0] = buf; pl[1] = 0;
    for (x = head; x; x = x->next) {
	snprintf(buf, sizeof buf, "%d", x->port);
	code = lookup_add_host(addrs, x->name, strlen(x->name), pl, x->socktype);
	if (code) goto Done;
#if 0
printf ("%d %d %d %d %s\n", x->socktype, x->priority, x->weight, x->port, x->name);
#endif
    }
Done:
    while ((x = head)) {
	head = x->next;
	free(x);
    }
    if (reply) free(reply);
#ifdef HAVE_RES_NCLOSE
    res_nclose(nstate);
#endif
    return code;
}

krb5_error_code
krb5i_locate_kdc(krb5_context context,
    const krb5_data *realm,
    struct addrlist *addrs,
    int flags)
{
    int code, c;
    code = krb5i_locate_by_conf(context, realm, addrs, flags);
    if (!code && addrs->naddrs) goto Done;
    c = krb5i_locate_by_dns(context, realm, addrs, flags);
    if (c != -1) code = c;
Done:
    return code;
}

struct sendto_arg {
    const krb5_data *request;
    krb5_data *reply;
    struct addrlist addrs[1];
    const krb5_data *realm;
    int flags;
    unsigned char reqlenbuf[4];
    struct addrinfo *winner;
    struct timeval tv[1];
};

int
krb5i_in_addrlist(struct addrinfo *addr, struct addrlist *list)
{
    int i;

    if (addr && list)
	for (i = 0; i < list->naddrs; ++i) {
	    if (addr->ai_addrlen == list->addrs[i]->ai_addrlen
		    && !memcmp(addr->ai_addr, list->addrs[i]->ai_addr,
			addr->ai_addrlen))
		return 1;
	}
    return 0;
}

static void
st_cleanup(krb5_context context, struct send_receive_context *srq, void *a)
{
    struct sendto_arg *sa = a;
    if (sa->winner && !(sa->flags & USE_MASTER)) {
	struct addrlist addrs[1];
	int code;

	memset(addrs, 0, sizeof *addrs);
	code = krb5i_locate_kdc(context, sa->realm, sa->addrs,
	    sa->flags | USE_MASTER);
	if (!code && krb5i_in_addrlist(sa->winner, addrs)) {
	    sa->flags |= USE_MASTER;
	}
	krb5i_free_addrlist(addrs);
    }
    krb5i_free_addrlist(sa->addrs);
}

struct xmit_arg {
    struct sendto_arg *sa;
    struct addrinfo *addr;
    struct xmit_arg *next;
    int state;
    int code;
    int fd;
    struct iovec iovec[2];
    struct msghdr msghdr[1];
    unsigned char replenbuf[4];
    unsigned char *rbuf;
    unsigned received, expecting;
};

static void
st2_cleanup(krb5_context context, struct send_receive_context *srq, void *a)
{
    struct xmit_arg *ta = a;
    if (ta->fd >= 0) close(ta->fd);
    if (ta->rbuf) free(ta->rbuf);
    free(ta);
}

static int st2_timeout(krb5_context, struct send_receive_context *, void *);

static int
st2_giveup(krb5_context context,
    struct send_receive_context *srq,
    struct xmit_arg *ta, int code)
{
    ta->code = code;	/* XXX */
    krb5i_sr_untimeout(srq, st2_timeout, ta);
    krb5i_sr_unlisten(srq, ta->fd);
#if 0
    close(ta->fd);
    ta->fd = -1;
#endif
    if (code && ta->next) {
	code = krb5i_sr_timeout(srq, st2_timeout, ta->next, 0);
	if (code) return code;
	ta->next = 0;
    }
    return -1;
}

static int
st2_listen(krb5_context context,
    struct send_receive_context *srq,
    void *a,
    short e,
    struct timeval *tv)
{
    struct xmit_arg *ta = a;
    int r, j, temp, code;
    unsigned char buf[K5BUFSIZE], *bp;
    struct msghdr msghdr[1];
    struct iovec iovec[2];

    if (e & (POLLERR|POLLHUP|POLLNVAL)) {
	return st2_giveup(context, srq, ta, EINVAL);
    }
    switch(ta->state) {
    case 1:
	r = sendmsg(ta->fd, ta->msghdr, 0);
	if (r <= 0) {
	    return st2_giveup(context, srq, ta, errno);
	}
	while (r) {
	    if (!ta->msghdr->msg_iovlen) break;
	    if (r >= ta->msghdr->msg_iov->iov_len) {
		r -= ta->msghdr->msg_iov->iov_len;
		++ta->msghdr->msg_iov;
		--ta->msghdr->msg_iovlen;
		continue;
	    }
	    ta->msghdr->msg_iov->iov_len -= r;
	    ta->msghdr->msg_iov->iov_base += r;
	    r = 0;
	}
	if (ta->msghdr->msg_iovlen) {
	    break;
	}
	krb5i_sr_untimeout(srq, st2_timeout, ta);
	ta->state = 2;
	code = krb5i_sr_timeout(srq, st2_timeout, ta, 1000);
	if (code) return code;
	code = krb5i_sr_listen(srq, st2_listen, ta, ta->fd, POLLIN);
	if (!code) code = -1;
	return code;
    case 2:
	j = 0;
	memset(msghdr, 0, sizeof *msghdr);
	if (ta->addr->ai_socktype == SOCK_STREAM) {
	    if (ta->received < 4) {
		iovec[j].iov_base = ta->replenbuf + ta->received;
		iovec[j].iov_len = 4 - ta->received;
		++j;
	    }
	    if (ta->rbuf) {
		iovec[j].iov_base = ta->rbuf + (ta->received - 4);
		iovec[j].iov_len = ta->expecting - (ta->received - 4);
	    } else {
		iovec[j].iov_base = buf;
		iovec[j].iov_len = sizeof buf;
	    }
	    ++j;
	} else {
	    iovec[j].iov_base = buf;
	    iovec[j].iov_len = sizeof buf;
	    ++j;
	}
	msghdr->msg_iov = iovec;
	msghdr->msg_iovlen = j;
	r = recvmsg(ta->fd, msghdr, 0);
	if (r < 0) {
	    return st2_giveup(context, srq, ta, errno);
	}
	if (!r) {
	    return st2_giveup(context, srq, ta, EINVAL);
	}
	code = -1;
	if (ta->addr->ai_socktype == SOCK_STREAM) {
	    ta->received += r;
	    if (ta->received < 4) return -1;
	    if (!ta->rbuf) {
		memcpy(&temp, ta->replenbuf, 4);
		ta->expecting = ntohl(temp);
		if (ta->expecting > 16384) {
		    return EFBIG;	/* xXX */
		}
		if (!(ta->rbuf = malloc(ta->expecting)))
		    return ENOMEM;
		bp = ta->rbuf;
		if (r > ta->received - 4) r = ta->received - 4;
		if (r > ta->expecting) r = ta->expecting;
		memcpy(bp, buf, r);
	    }
	} else {
	    ta->expecting = r;
	    if (!(ta->rbuf = malloc(ta->expecting)))
		code = ENOMEM;
	    else
		memcpy(ta->rbuf, buf, r);
	}
	if (ta->addr->ai_socktype != SOCK_STREAM || ta->expecting == ta->received - 4) {
	    code = 0;
	    ta->sa->winner = ta->addr;
	    ta->sa->reply->data = ta->rbuf;
	    ta->sa->reply->length = ta->expecting;
	    ta->rbuf = 0;
	    *ta->sa->tv = *tv;
	}
	if (code != -1)
	    st2_giveup(context, srq, ta, code);
	return code;
    }
return -1;
}

static int
st2_timeout(krb5_context context, struct send_receive_context *srq, void *a)
{
    struct xmit_arg *ta = a;
    int code;
    code = -1;
    switch(ta->state) {
    case 0:
	ta->fd = socket(ta->addr->ai_family, ta->addr->ai_socktype, 0);
	if (ta->fd < 0) return errno;
	if (fcntl(ta->fd, F_SETFL, O_NONBLOCK) < 0) return errno;
	ta->state = 1;
	if (connect(ta->fd, ta->addr->ai_addr, ta->addr->ai_addrlen) >= 0)
	    return st2_listen(context, srq, ta, POLLOUT, 0);
	switch(errno) {
	case EINPROGRESS:
	    code = krb5i_sr_timeout(srq, st2_timeout, ta, 1000);
	    if (code) return code;
	    code = krb5i_sr_listen(srq, st2_listen, ta, ta->fd, POLLOUT);
	    if (!code) code = -1;
	    break;
	default:
	    ta->code = errno;
	    break;
	}
	break;
    case 1:
    default:
	st2_giveup(context, srq, ta, ETIMEDOUT);
	break;
    }
    return code;
}

static int
st_init(krb5_context context, struct send_receive_context *srq, void *a)
{
    struct sendto_arg *sa = a;
    struct xmit_arg *ta, *ta2;
    int code, i, j, temp;
    krb5i_sr_cleanup(srq, st_cleanup, sa);
    code = krb5i_locate_kdc(context, sa->realm, sa->addrs,
	sa->flags);
    if (code) goto Done;
    ta = 0;
    temp = htonl(sa->request->length);
    memcpy(sa->reqlenbuf, &temp, 4);
    for (i = sa->addrs->naddrs; --i >= 0; ) {
	ta2 = ta;
	ta = malloc(sizeof *ta);
	if (!ta) {
	    code = ENOMEM;
	    goto Done;
	}
	memset(ta, 0, sizeof *ta);
	ta->fd = -1;
	ta->code = -1;
	ta->sa = sa;
	code = krb5i_sr_cleanup(srq, st2_cleanup, ta);
	if (code) {
	    st2_cleanup(context, srq, ta);
	    goto Done;
	}
	ta->addr = sa->addrs->addrs[i];
	ta->next = ta2;
	j = 0;
	ta->msghdr->msg_iov = ta->iovec;
	if (ta->addr->ai_socktype == SOCK_STREAM) {
	    ta->iovec[j].iov_base = sa->reqlenbuf;
	    ta->iovec[j].iov_len = 4;
	    ++j;
	}
	ta->iovec[j].iov_base = sa->request->data;
	ta->iovec[j].iov_len = sa->request->length;
	++j;
	ta->msghdr->msg_iovlen = j;
    }
    if (ta)
	code = krb5i_sr_timeout(srq, st2_timeout, ta, 0);
    if (code) goto Done;
    code = -1;
Done:
    return code;
}

krb5_error_code
krb5i_sendto_kdc(krb5_context context,
    const krb5_data *request,
    const krb5_data *realm,
    krb5_data *reply,
    int *flags,
    struct timeval *tv)
{
    struct sendto_arg sa[1];
    int code;
    int len;

    memset(sa, 0, sizeof *sa);
    code = krb5_get_udp_preference_limit(context, &len);
    if (code) goto Done;
    sa->request = request;
    sa->reply = reply;
    sa->realm = realm;
    sa->flags = *flags;
    if (request->length >= len)
	sa->flags |= TCP_FIRST;
    code = krb5i_sendrecv(context, st_init, sa);
    if (code) goto Done;
Done:
    if (tv) *tv = *sa->tv;
    *flags = sa->flags;
    return code;
}
