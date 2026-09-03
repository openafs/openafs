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

/*
 * rx_addr.h - a family-agnostic address abstraction for Rx.
 *
 * struct rx_sockaddr wraps an IPv4 or (when HAVE_IPV6 is defined) IPv6
 * socket address in a single type, along with the Rx service id and socket
 * type that travel alongside an address everywhere Rx uses one. It is
 * modelled on Linux's struct sockaddr_rxrpc (include/uapi/linux/rxrpc.h),
 * which independently converged on the same shape.
 *
 * struct rx_address is the narrower "just an address, no port" type used
 * where Rx needs to compare or store addresses without a port attached.
 */

#ifndef OPENAFS_RX_ADDR_H
#define OPENAFS_RX_ADDR_H

/*
 * struct sockaddr_in is already visible here via rx.h's own preamble
 * ("netinet/in.h"/<netinet/in.h>). Pull in struct sockaddr_in6/in6_addr the
 * same way rx.h pulls in the IPv4 equivalents: quoted (and thus subject to
 * the platform's kernel-build header aliasing, e.g. the "netinet" -> Linux
 * kernel "linux" directory symlink) under KERNEL, angle-bracket otherwise.
 */
#ifdef HAVE_IPV6
# ifdef KERNEL
#  include "netinet/in6.h"
# else
#  include <netinet/in.h>
# endif
#endif

typedef afs_uint16 rx_service_t;

/*
 * Socket address for rx.
 */
struct rx_sockaddr {
    rx_service_t service;	/**< rx service id, 0 if not applicable */
    int socktype;		/**< socket type (e.g. SOCK_DGRAM) */
    socklen_t addrlen;		/**< length in bytes of the active union member */
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
	struct sockaddr_storage ss;	/* for alignment/sizing only */
#endif
    } addr;
};
#define rxsa_family       addr.sa.sa_family
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
# define rxsa_in_len      addr.sin.sin_len
#endif
#define rxsa_in_family    addr.sin.sin_family
#define rxsa_in_addr      addr.sin.sin_addr
#define rxsa_s_addr       addr.sin.sin_addr.s_addr
#define rxsa_in_port      addr.sin.sin_port
#ifdef HAVE_IPV6
# ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
#  define rxsa_in6_len    addr.sin6.sin6_len
# endif
# define rxsa_in6_family  addr.sin6.sin6_family
# define rxsa_in6_addr    addr.sin6.sin6_addr
# define rxsa_s6_addr     addr.sin6.sin6_addr.s6_addr
# define rxsa_in6_port    addr.sin6.sin6_port
#endif

/**
 * Generic address for rx (no port).
 */
struct rx_address {
    int addrtype;		/**< AF_INET, or AF_INET6 when HAVE_IPV6 */
    union {
	struct in_addr in;	/**< in network byte order */
#ifdef HAVE_IPV6
	struct in6_addr in6;	/**< in network byte order */
#endif
    } addr;
};
#define rxa_in_addr   addr.in
#define rxa_s_addr    addr.in.s_addr
#ifdef HAVE_IPV6
# define rxa_in6_addr addr.in6
#endif

/*
 * rx_debugAddr - the wire form of an address used by the RX_DEBUGI_GETCONN6/
 * RX_DEBUGI_GETALLCONN6/RX_DEBUGI_GETPEER6 rxdebug replies (RX_DEBUGI_VERSION
 * 'T' and later). A plain sa_family_t is not portable on the wire - Linux and
 * Darwin/BSD disagree on the numeric value of AF_INET6 - so family is one of
 * the RX_DEBUG_AF_* constants below, not the local AF_INET6. port and addr
 * are in network byte order; addr holds all 16 bytes for an IPv6 address, or
 * an IPv4 address in the first 4 bytes (the rest zero).
 */
struct rx_debugAddr {
    afs_uint16 family;
    afs_uint16 port;
    unsigned char addr[16];
};
#define RX_DEBUG_AF_INET  2	/* matches AF_INET on every real platform */
#define RX_DEBUG_AF_INET6 10	/* wire-protocol constant; not the local AF_INET6 */

void rx_sockaddr_to_debugAddr(const struct rx_sockaddr *sa,
			      struct rx_debugAddr *da);
int rx_debugAddr_to_sockaddr(const struct rx_debugAddr *da,
			     struct rx_sockaddr *sa);

/* Masks for rx_compare_sockaddr() */
#define RXA_ADDR      0x01
#define RXA_PORT      0x02
#define RXA_SERVICE   0x04
#define RXA_SOCKTYPE  0x08
#define RXA_AP        (RXA_ADDR | RXA_PORT)
#define RXA_ALL       (RXA_ADDR | RXA_PORT | RXA_SERVICE | RXA_SOCKTYPE)

/* Text buffer sized for "[" + a full IPv6 address + "]:" + a port. */
typedef struct rx_inet_fmtbuf {
    char buf[3 + 8 * 4 + 7 + 6];	/* "[" + 8*"xxxx" + 7*":" + "]:" + port */
} rx_inet_fmtbuf_t;

static_inline rx_service_t
rx_get_sockaddr_service(const struct rx_sockaddr *sa)
{
    return sa->service;
}

static_inline void
rx_set_sockaddr_service(struct rx_sockaddr *sa, rx_service_t service)
{
    sa->service = service;
}

static_inline afs_uint16
rx_get_sockaddr_port(const struct rx_sockaddr *sa)
{
    if (sa->rxsa_family == AF_INET) {
	return sa->rxsa_in_port;
    }
#ifdef HAVE_IPV6
    if (sa->rxsa_family == AF_INET6) {
	return sa->rxsa_in6_port;
    }
#endif
    return 0;
}

static_inline afs_uint16
rx_set_sockaddr_port(struct rx_sockaddr *sa, afs_uint16 port)
{
    if (sa->rxsa_family == AF_INET) {
	return (sa->rxsa_in_port = port);
    }
#ifdef HAVE_IPV6
    if (sa->rxsa_family == AF_INET6) {
	return (sa->rxsa_in6_port = port);
    }
#endif
    return port;
}

/* rx_sockaddr */
#ifndef KERNEL
# include <netdb.h>
int rx_addrinfo_to_sockaddr(const struct addrinfo *ai, rx_service_t service,
			    struct rx_sockaddr *sa);
char *rx_print_sockaddr(const struct rx_sockaddr *a, char *dst, size_t size);
char *rx_sockaddr2str(const struct rx_sockaddr *a, rx_inet_fmtbuf_t *buf);
#endif
afs_uint32 rx_hash_sockaddr(const struct rx_sockaddr *a, afs_uint32 hashsize);
int rx_compare_sockaddr(const struct rx_sockaddr *a,
			const struct rx_sockaddr *b, int mask);
int rx_order_sockaddr(const struct rx_sockaddr *a,
		      const struct rx_sockaddr *b);
int rx_is_loopback_sockaddr(const struct rx_sockaddr *a);
int rx_copy_sockaddr(const struct rx_sockaddr *src, struct rx_sockaddr *dst);

/* For compatibility with IPv4-only interfaces. */
void rx_ipv4_to_sockaddr(afs_uint32 ipv4, afs_uint16 port,
			 rx_service_t service, struct rx_sockaddr *sa);
int rx_try_sockaddr_to_ipv4(const struct rx_sockaddr *a, afs_uint32 *ipv4);

/* rx_address */
#ifndef KERNEL
char *rx_print_address(const struct rx_address *a, char *dst, size_t size);
char *rx_inet42str(afs_uint32 addr, rx_inet_fmtbuf_t *buf);
#endif
int rx_compare_address(const struct rx_address *a, const struct rx_address *b);
int rx_is_loopback_address(const struct rx_address *a);
int rx_copy_address(const struct rx_address *src, struct rx_address *dst);
int rx_address_to_sockaddr(const struct rx_address *a, afs_uint16 port,
			   rx_service_t service, struct rx_sockaddr *sa);
int rx_sockaddr_to_address(const struct rx_sockaddr *sa, struct rx_address *a);

/* For compatibility with IPv4-only on-the-wire fields. */
int rx_ipv4_to_address(afs_uint32 ipv4, struct rx_address *a);
int rx_try_address_to_ipv4(const struct rx_address *a, afs_uint32 *ipv4);

#endif /* OPENAFS_RX_ADDR_H */
