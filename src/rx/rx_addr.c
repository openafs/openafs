/*
 * Copyright (c) 2026, Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifndef KERNEL
# include <roken.h>
#else
# include "afs/sysincludes.h"
# include "afsincludes.h"
#endif
#include <afs/opr.h>
#include <opr/jhash.h>

#include "rx.h"
#include "rx_addr.h"

/*
 * -------------------------------------------------------------------------
 * Small predicates, kept static so the family-dispatch logic below reads
 * top to bottom instead of jumping into a dozen tiny helper files.
 * -------------------------------------------------------------------------
 */

static int
is_loopback_v4(afs_uint32 addr_hbo)
{
    return (addr_hbo & 0xff000000) == 0x7f000000;	/* 127.0.0.0/8 */
}

#ifdef HAVE_IPV6
static int
is_loopback_v6(const struct in6_addr *a)
{
#ifdef IN6_IS_ADDR_LOOPBACK
    return IN6_IS_ADDR_LOOPBACK(a);
#else
    static const unsigned char loop[16] =
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    return memcmp(a->s6_addr, loop, 16) == 0;
#endif
}

/*
 * A link-local address (fe80::/10) is only ever dialable qualified with
 * the specific interface it was observed on (a zone/scope id neither the
 * VLDB's endpoint list nor struct rx_sockaddr carries anywhere in this
 * tree) - unlike a loopback or a real global/ULA address, it is never a
 * meaningful candidate to just connect() to standalone. A multi-homed
 * server's self-registered endpoint list can absolutely contain one
 * (any host with IPv6 enabled always has at least one, on every
 * interface, entirely automatically) alongside its real, useful
 * addresses - confirmed live in this project: afs-fs6's own endpoint
 * list mixed real usable addresses with link-local ones from the same
 * registration.
 */
static int
is_linklocal_v6(const struct in6_addr *a)
{
#ifdef IN6_IS_ADDR_LINKLOCAL
    return IN6_IS_ADDR_LINKLOCAL(a);
#else
    return (a->s6_addr[0] == 0xfe) && ((a->s6_addr[1] & 0xc0) == 0x80);
#endif
}

static int
is_v4_mapped(const struct in6_addr *a)
{
#ifdef IN6_IS_ADDR_V4MAPPED
    return IN6_IS_ADDR_V4MAPPED(a);
#else
    static const unsigned char prefix[12] =
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    return memcmp(a->s6_addr, prefix, 12) == 0;
#endif
}
#endif /* HAVE_IPV6 */

/*
 * -------------------------------------------------------------------------
 * rx_sockaddr
 * -------------------------------------------------------------------------
 */

afs_uint32
rx_hash_sockaddr(const struct rx_sockaddr *a, afs_uint32 hashsize)
{
    afs_uint32 hash;

    if (a->rxsa_family == AF_INET) {
	hash = opr_jhash_int(a->rxsa_s_addr, a->rxsa_in_port);
#ifdef HAVE_IPV6
    } else if (a->rxsa_family == AF_INET6) {
	hash = opr_jhash_opaque(a->rxsa_s6_addr, sizeof(a->rxsa_s6_addr),
				a->rxsa_in6_port);
#endif
    } else {
	hash = 0;
    }

    if (hashsize == 0) {
	return hash;
    }
    return hash % hashsize;
}

int
rx_compare_sockaddr(const struct rx_sockaddr *a, const struct rx_sockaddr *b,
		    int mask)
{
    if ((mask & RXA_SERVICE) && a->service != b->service) {
	return 0;
    }
    if ((mask & RXA_SOCKTYPE) && a->socktype != b->socktype) {
	return 0;
    }
    if (mask & RXA_AP) {
	if (a->rxsa_family != b->rxsa_family) {
	    return 0;
	}
	if (a->rxsa_family == AF_INET) {
	    if ((mask & RXA_ADDR) && a->rxsa_s_addr != b->rxsa_s_addr) {
		return 0;
	    }
	    if ((mask & RXA_PORT) && a->rxsa_in_port != b->rxsa_in_port) {
		return 0;
	    }
#ifdef HAVE_IPV6
	} else if (a->rxsa_family == AF_INET6) {
	    if ((mask & RXA_ADDR)
		&& memcmp(a->rxsa_s6_addr, b->rxsa_s6_addr,
			 sizeof(a->rxsa_s6_addr)) != 0) {
		return 0;
	    }
	    if ((mask & RXA_PORT) && a->rxsa_in6_port != b->rxsa_in6_port) {
		return 0;
	    }
#endif
	} else {
	    /* Unknown family: only equal to an identical unknown family. */
	    return a->rxsa_family == b->rxsa_family;
	}
    }
    return 1;
}

/*
 * A total order over addresses: family first (so a v4 address always sorts
 * before a v6 one, arbitrarily but consistently), then the raw address bytes
 * in network order, then the port. This is *not* meant to correspond to any
 * numeric interpretation of the address - it only needs to be the same
 * order on every host, which ubik's election (src/ubik/vote.c, beacon.c)
 * relies on.
 *
 * Returns <0, 0, >0 the way memcmp/strcmp do.
 */
int
rx_order_sockaddr(const struct rx_sockaddr *a, const struct rx_sockaddr *b)
{
    int cmp;

    if (a->rxsa_family != b->rxsa_family) {
	return (a->rxsa_family < b->rxsa_family) ? -1 : 1;
    }

    if (a->rxsa_family == AF_INET) {
	afs_uint32 ha = ntohl(a->rxsa_s_addr);
	afs_uint32 hb = ntohl(b->rxsa_s_addr);
	if (ha != hb) {
	    return (ha < hb) ? -1 : 1;
	}
	cmp = 0;
#ifdef HAVE_IPV6
    } else if (a->rxsa_family == AF_INET6) {
	cmp = memcmp(a->rxsa_s6_addr, b->rxsa_s6_addr,
		    sizeof(a->rxsa_s6_addr));
	if (cmp != 0) {
	    return cmp;
	}
#endif
    } else {
	return 0;
    }

    {
	afs_uint16 pa = ntohs(rx_get_sockaddr_port(a));
	afs_uint16 pb = ntohs(rx_get_sockaddr_port(b));
	if (pa != pb) {
	    return (pa < pb) ? -1 : 1;
	}
    }
    return 0;
}

int
rx_is_loopback_sockaddr(const struct rx_sockaddr *a)
{
    if (a->rxsa_family == AF_INET) {
	return is_loopback_v4(ntohl(a->rxsa_s_addr));
    }
#ifdef HAVE_IPV6
    if (a->rxsa_family == AF_INET6) {
	if (is_loopback_v6(&a->rxsa_in6_addr)) {
	    return 1;
	}
	if (is_v4_mapped(&a->rxsa_in6_addr)) {
	    afs_uint32 v4;
	    memcpy(&v4, &a->rxsa_s6_addr[12], sizeof(v4));
	    return is_loopback_v4(ntohl(v4));
	}
	return 0;
    }
#endif
    return 0;
}

/*
 * IPv4 has no real equivalent worth checking here (169.254.0.0/16 exists,
 * but nothing in this tree currently registers/dials one) - only IPv6
 * link-local addresses are what this predicate actually needs to catch,
 * so AF_INET always returns false rather than growing a parallel check
 * nothing exercises.
 */
int
rx_is_linklocal_sockaddr(const struct rx_sockaddr *a)
{
#ifdef HAVE_IPV6
    if (a->rxsa_family == AF_INET6) {
	return is_linklocal_v6(&a->rxsa_in6_addr);
    }
#endif
    return 0;
}

/*
 * Genuine bit-level prefix comparison of two address byte strings, each
 * totalbits long, matching the leading prefixlen bits. Not just a
 * byte-level check - a caller-supplied prefixlen that isn't a multiple of
 * 8 (e.g. /64 is fine, but /21 or /127 must work too) still has to compare
 * only the bits that are actually part of the prefix, in the partial byte
 * straddling the boundary. Clamped so an out-of-range prefixlen (<0 or
 * >totalbits) is treated as 0 or totalbits respectively, rather than
 * reading past either buffer.
 */
static int
bytes_prefix_match(const unsigned char *a, const unsigned char *b,
		    int prefixlen, int totalbits)
{
    int fullbytes, remainder_bits;

    if (prefixlen < 0)
	prefixlen = 0;
    if (prefixlen > totalbits)
	prefixlen = totalbits;
    if (prefixlen == 0)
	return 1;

    fullbytes = prefixlen / 8;
    if (fullbytes && memcmp(a, b, fullbytes) != 0)
	return 0;

    remainder_bits = prefixlen % 8;
    if (remainder_bits) {
	unsigned char mask = (unsigned char)(0xff << (8 - remainder_bits));
	if ((a[fullbytes] & mask) != (b[fullbytes] & mask))
	    return 0;
    }
    return 1;
}

/*
 * Is address 'a' within the prefixlen-bit prefix of 'net'? A byte-for-byte
 * generalization of rx_compare_sockaddr(a, net, RXA_ADDR): prefixlen==32
 * (v4) or prefixlen==128 (v6) is defined to compare every address bit, so
 * it behaves identically to an exact rx_compare_sockaddr() match - this is
 * what lets a caller (netrestrict.c's exact-match NetRestrict entries,
 * an implicit /128) switch to this function without changing behavior for
 * any address that has no explicit /N in the config file.
 *
 * Different families never match, same as rx_compare_sockaddr(). v4's
 * rxsa_s_addr and v6's rxsa_s6_addr are both already stored in network
 * byte order (the whole point of struct in_addr/in6_addr), so comparing
 * them as raw byte strings, most-significant byte (and, within the final
 * partial byte, most-significant bit) first, is exactly the bit order a
 * prefix length is conventionally written in.
 */
int
rx_prefix_match_sockaddr(const struct rx_sockaddr *a,
			 const struct rx_sockaddr *net, int prefixlen)
{
    if (a->rxsa_family != net->rxsa_family) {
	return 0;
    }
    if (a->rxsa_family == AF_INET) {
	return bytes_prefix_match((const unsigned char *)&a->rxsa_s_addr,
				  (const unsigned char *)&net->rxsa_s_addr,
				  prefixlen, 32);
#ifdef HAVE_IPV6
    } else if (a->rxsa_family == AF_INET6) {
	return bytes_prefix_match(a->rxsa_s6_addr, net->rxsa_s6_addr,
				  prefixlen, 128);
#endif
    }
    return 0;
}

int
rx_copy_sockaddr(const struct rx_sockaddr *src, struct rx_sockaddr *dst)
{
    memcpy(dst, src, sizeof(*dst));
    return 0;
}

#if !defined(KERNEL) || defined(UKERNEL)
int
rx_addrinfo_to_sockaddr(const struct addrinfo *ai, rx_service_t service,
			struct rx_sockaddr *sa)
{
    if (ai == NULL || ai->ai_addr == NULL) {
	return EINVAL;
    }
    if (ai->ai_family != AF_INET
#ifdef HAVE_IPV6
	&& ai->ai_family != AF_INET6
#endif
	) {
	return EAFNOSUPPORT;
    }
    if ((size_t)ai->ai_addrlen > sizeof(sa->addr)) {
	return EINVAL;
    }

    memset(sa, 0, sizeof(*sa));
    memcpy(&sa->addr, ai->ai_addr, ai->ai_addrlen);
    sa->addrlen = ai->ai_addrlen;
    sa->socktype = ai->ai_socktype ? ai->ai_socktype : SOCK_DGRAM;
    sa->service = service;
    return 0;
}

char *
rx_print_sockaddr(const struct rx_sockaddr *a, char *dst, size_t size)
{
    char abuf[64];
    const void *src;

    if (a->rxsa_family == AF_INET) {
	src = &a->rxsa_in_addr;
#ifdef HAVE_IPV6
    } else if (a->rxsa_family == AF_INET6) {
	src = &a->rxsa_in6_addr;
#endif
    } else {
	snprintf(dst, size, "<unknown address family %d>", a->rxsa_family);
	return dst;
    }

    if (inet_ntop(a->rxsa_family, src, abuf, sizeof(abuf)) == NULL) {
	snprintf(dst, size, "<unprintable address>");
	return dst;
    }

#ifdef HAVE_IPV6
    if (a->rxsa_family == AF_INET6) {
	snprintf(dst, size, "[%s]:%u", abuf, ntohs(a->rxsa_in6_port));
	return dst;
    }
#endif
    snprintf(dst, size, "%s:%u", abuf, ntohs(a->rxsa_in_port));
    return dst;
}

char *
rx_sockaddr2str(const struct rx_sockaddr *a, rx_inet_fmtbuf_t *buf)
{
    return rx_print_sockaddr(a, buf->buf, sizeof(buf->buf));
}
#endif /* !KERNEL */

void
rx_ipv4_to_sockaddr(afs_uint32 ipv4, afs_uint16 port, rx_service_t service,
		    struct rx_sockaddr *sa)
{
    memset(sa, 0, sizeof(*sa));
    sa->rxsa_in_family = AF_INET;
    sa->rxsa_s_addr = ipv4;
    sa->rxsa_in_port = port;
    sa->addrlen = sizeof(struct sockaddr_in);
    sa->socktype = SOCK_DGRAM;
    sa->service = service;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
}

#ifdef HAVE_IPV6
/* For compatibility with callers that decode a raw 16-byte IPv6 address
 * (network byte order) from some other wire format - e.g. the VLDB
 * GetEndpoints RPC's vlendpoint.value[4] - rather than obtaining a
 * struct rx_sockaddr from the OS or from Rx itself. */
void
rx_ipv6_to_sockaddr(const unsigned char addr[16], afs_uint16 port,
		    rx_service_t service, struct rx_sockaddr *sa)
{
    memset(sa, 0, sizeof(*sa));
    sa->rxsa_in6_family = AF_INET6;
    memcpy(sa->rxsa_s6_addr, addr, 16);
    sa->rxsa_in6_port = port;
    sa->addrlen = sizeof(struct sockaddr_in6);
    sa->socktype = SOCK_DGRAM;
    sa->service = service;
# ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
    sa->rxsa_in6_len = sizeof(struct sockaddr_in6);
# endif
}
#endif /* HAVE_IPV6 */

void
rx_sockaddr_to_debugAddr(const struct rx_sockaddr *sa, struct rx_debugAddr *da)
{
    memset(da, 0, sizeof(*da));
    if (sa->rxsa_family == AF_INET) {
	da->family = htons(RX_DEBUG_AF_INET);
	da->port = sa->rxsa_in_port;
	memcpy(da->addr, &sa->rxsa_s_addr, 4);
    }
#ifdef HAVE_IPV6
    else if (sa->rxsa_family == AF_INET6) {
	da->family = htons(RX_DEBUG_AF_INET6);
	da->port = sa->rxsa_in6_port;
	memcpy(da->addr, sa->rxsa_s6_addr, 16);
    }
#endif
}

int
rx_debugAddr_to_sockaddr(const struct rx_debugAddr *da, struct rx_sockaddr *sa)
{
    afs_uint16 family = ntohs(da->family);

    memset(sa, 0, sizeof(*sa));
    if (family == RX_DEBUG_AF_INET) {
	afs_uint32 addr;
	memcpy(&addr, da->addr, 4);
	rx_ipv4_to_sockaddr(addr, da->port, 0, sa);
	return 0;
    }
#ifdef HAVE_IPV6
    if (family == RX_DEBUG_AF_INET6) {
	sa->rxsa_in6_family = AF_INET6;
	memcpy(&sa->rxsa_s6_addr, da->addr, 16);
	sa->rxsa_in6_port = da->port;
	sa->addrlen = sizeof(struct sockaddr_in6);
	sa->socktype = SOCK_DGRAM;
# ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
	sa->rxsa_in6_len = sizeof(struct sockaddr_in6);
# endif
	return 0;
    }
#endif
    return -1;
}

int
rx_try_sockaddr_to_ipv4(const struct rx_sockaddr *a, afs_uint32 *ipv4)
{
    if (a->rxsa_family == AF_INET) {
	*ipv4 = a->rxsa_s_addr;
	return 1;
    }
#ifdef HAVE_IPV6
    if (a->rxsa_family == AF_INET6 && is_v4_mapped(&a->rxsa_in6_addr)) {
	memcpy(ipv4, &a->rxsa_s6_addr[12], sizeof(*ipv4));
	return 1;
    }
#endif
    return 0;
}

/*
 * -------------------------------------------------------------------------
 * rx_address (no port)
 * -------------------------------------------------------------------------
 */

int
rx_compare_address(const struct rx_address *a, const struct rx_address *b)
{
    if (a->addrtype != b->addrtype) {
	return 0;
    }
    if (a->addrtype == AF_INET) {
	return a->rxa_s_addr == b->rxa_s_addr;
    }
#ifdef HAVE_IPV6
    if (a->addrtype == AF_INET6) {
	return memcmp(&a->rxa_in6_addr, &b->rxa_in6_addr,
		     sizeof(a->rxa_in6_addr)) == 0;
    }
#endif
    return 0;
}

int
rx_is_loopback_address(const struct rx_address *a)
{
    if (a->addrtype == AF_INET) {
	return is_loopback_v4(ntohl(a->rxa_s_addr));
    }
#ifdef HAVE_IPV6
    if (a->addrtype == AF_INET6) {
	if (is_loopback_v6(&a->rxa_in6_addr)) {
	    return 1;
	}
	if (is_v4_mapped(&a->rxa_in6_addr)) {
	    afs_uint32 v4;
	    memcpy(&v4, &a->rxa_in6_addr.s6_addr[12], sizeof(v4));
	    return is_loopback_v4(ntohl(v4));
	}
	return 0;
    }
#endif
    return 0;
}

int
rx_copy_address(const struct rx_address *src, struct rx_address *dst)
{
    memcpy(dst, src, sizeof(*dst));
    return 0;
}

int
rx_address_to_sockaddr(const struct rx_address *a, afs_uint16 port,
		       rx_service_t service, struct rx_sockaddr *sa)
{
    memset(sa, 0, sizeof(*sa));
    if (a->addrtype == AF_INET) {
	sa->rxsa_in_family = AF_INET;
	sa->rxsa_in_addr = a->rxa_in_addr;
	sa->rxsa_in_port = port;
	sa->addrlen = sizeof(struct sockaddr_in);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
#ifdef HAVE_IPV6
    } else if (a->addrtype == AF_INET6) {
	sa->rxsa_in6_family = AF_INET6;
	sa->rxsa_in6_addr = a->rxa_in6_addr;
	sa->rxsa_in6_port = port;
	sa->addrlen = sizeof(struct sockaddr_in6);
# ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
	sa->rxsa_in6_len = sizeof(struct sockaddr_in6);
# endif
#endif
    } else {
	return EAFNOSUPPORT;
    }
    sa->socktype = SOCK_DGRAM;
    sa->service = service;
    return 0;
}

int
rx_sockaddr_to_address(const struct rx_sockaddr *sa, struct rx_address *a)
{
    memset(a, 0, sizeof(*a));
    if (sa->rxsa_family == AF_INET) {
	a->addrtype = AF_INET;
	a->rxa_in_addr = sa->rxsa_in_addr;
	return 0;
    }
#ifdef HAVE_IPV6
    if (sa->rxsa_family == AF_INET6) {
	a->addrtype = AF_INET6;
	a->rxa_in6_addr = sa->rxsa_in6_addr;
	return 0;
    }
#endif
    return EAFNOSUPPORT;
}

int
rx_ipv4_to_address(afs_uint32 ipv4, struct rx_address *a)
{
    memset(a, 0, sizeof(*a));
    a->addrtype = AF_INET;
    a->rxa_s_addr = ipv4;
    return 0;
}

int
rx_try_address_to_ipv4(const struct rx_address *a, afs_uint32 *ipv4)
{
    if (a->addrtype == AF_INET) {
	*ipv4 = a->rxa_s_addr;
	return 1;
    }
#ifdef HAVE_IPV6
    if (a->addrtype == AF_INET6 && is_v4_mapped(&a->rxa_in6_addr)) {
	memcpy(ipv4, &a->rxa_in6_addr.s6_addr[12], sizeof(*ipv4));
	return 1;
    }
#endif
    return 0;
}

#if !defined(KERNEL) || defined(UKERNEL)
char *
rx_print_address(const struct rx_address *a, char *dst, size_t size)
{
    const void *src;

    if (a->addrtype == AF_INET) {
	src = &a->rxa_in_addr;
#ifdef HAVE_IPV6
    } else if (a->addrtype == AF_INET6) {
	src = &a->rxa_in6_addr;
#endif
    } else {
	snprintf(dst, size, "<unknown address family %d>", a->addrtype);
	return dst;
    }
    if (inet_ntop(a->addrtype, src, dst, size) == NULL) {
	snprintf(dst, size, "<unprintable address>");
    }
    return dst;
}

char *
rx_inet42str(afs_uint32 addr, rx_inet_fmtbuf_t *buf)
{
    struct in_addr in;
    in.s_addr = addr;
    if (inet_ntop(AF_INET, &in, buf->buf, sizeof(buf->buf)) == NULL) {
	snprintf(buf->buf, sizeof(buf->buf), "<unprintable address>");
    }
    return buf->buf;
}
#endif /* !KERNEL */
