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
#include <roken.h>

#include <tests/tap/basic.h>

#include <rx/rx.h>
#include <rx/rx_addr.h>

/*
 * The test count depends on whether this build has IPv6 support, so use
 * plan_lazy() rather than trying to keep a literal count in sync with two
 * different configure outcomes.
 */

static struct rx_sockaddr
mkv4(afs_uint32 host_hbo, afs_uint16 port_hbo, rx_service_t service)
{
    struct rx_sockaddr sa;
    rx_ipv4_to_sockaddr(htonl(host_hbo), htons(port_hbo), service, &sa);
    return sa;
}

static void
test_ipv4_roundtrip(void)
{
    struct rx_sockaddr sa;
    afs_uint32 back;

    sa = mkv4(0xc0a80001, 7000, 1);	/* 192.168.0.1:7000 */

    is_int(AF_INET, sa.rxsa_family, "v4 sockaddr has AF_INET family");
    is_int(sizeof(struct sockaddr_in), sa.addrlen,
	  "v4 sockaddr has correct addrlen");
    is_int(1, sa.service, "v4 sockaddr keeps the service id");
    is_int(htons(7000), rx_get_sockaddr_port(&sa),
	  "rx_get_sockaddr_port reads back the v4 port");

    ok(rx_try_sockaddr_to_ipv4(&sa, &back), "v4 sockaddr converts back to an ipv4");
    is_int(htonl(0xc0a80001), back, "round-tripped ipv4 address matches");
}

static void
test_ipv4_compare_and_hash(void)
{
    struct rx_sockaddr a, b, c;
    afs_uint32 h1, h2;

    a = mkv4(0x0a000001, 7000, 1);
    b = mkv4(0x0a000001, 7000, 1);
    c = mkv4(0x0a000002, 7000, 1);

    ok(rx_compare_sockaddr(&a, &b, RXA_ALL), "identical v4 addrs compare equal (RXA_ALL)");
    ok(!rx_compare_sockaddr(&a, &c, RXA_ADDR), "different v4 addrs compare unequal (RXA_ADDR)");
    ok(rx_compare_sockaddr(&a, &c, RXA_PORT | RXA_SERVICE),
      "different v4 addrs still equal on port+service alone");

    is_int(0, rx_order_sockaddr(&a, &b), "identical v4 addrs order equal");
    ok(rx_order_sockaddr(&a, &c) < 0, "10.0.0.1 sorts before 10.0.0.2");
    ok(rx_order_sockaddr(&c, &a) > 0, "10.0.0.2 sorts after 10.0.0.1 (antisymmetric)");

    h1 = rx_hash_sockaddr(&a, 257);
    h2 = rx_hash_sockaddr(&b, 257);
    is_int(h1, h2, "hash of identical v4 addrs is identical");
    ok(h1 < 257, "hash respects the requested hashsize");
}

static void
test_ipv4_loopback(void)
{
    struct rx_sockaddr loop, notloop, edge;

    loop = mkv4(0x7f000001, 7000, 0);		/* 127.0.0.1 */
    edge = mkv4(0x7fffffff, 7000, 0);		/* 127.255.255.255 - still 127/8 */
    notloop = mkv4(0x08080808, 7000, 0);	/* 8.8.8.8 */

    ok(rx_is_loopback_sockaddr(&loop), "127.0.0.1 is loopback");
    ok(rx_is_loopback_sockaddr(&edge), "127.255.255.255 is loopback (127.0.0.0/8)");
    ok(!rx_is_loopback_sockaddr(&notloop), "8.8.8.8 is not loopback");
}

static void
test_ipv4_address_type(void)
{
    struct rx_address a, b;
    struct rx_sockaddr sa;
    afs_uint32 back;

    rx_ipv4_to_address(htonl(0x0a010203), &a);
    rx_ipv4_to_address(htonl(0x0a010203), &b);
    ok(rx_compare_address(&a, &b), "identical rx_address values compare equal");

    ok(rx_try_address_to_ipv4(&a, &back), "rx_address converts back to ipv4");
    is_int(htonl(0x0a010203), back, "round-tripped rx_address matches");

    ok(rx_address_to_sockaddr(&a, htons(7000), 5, &sa) == 0,
      "rx_address_to_sockaddr succeeds for an ipv4 address");
    is_int(AF_INET, sa.rxsa_family, "resulting sockaddr has AF_INET family");

    ok(rx_sockaddr_to_address(&sa, &b) == 0,
      "rx_sockaddr_to_address succeeds for a v4 sockaddr");
    ok(rx_compare_address(&a, &b), "sockaddr->address->sockaddr preserves the address");
}

static void
test_malformed(void)
{
    struct rx_sockaddr sa;
    struct rx_address addr;

    memset(&addr, 0, sizeof(addr));
    addr.addrtype = AF_UNIX;	/* not a family rx_addr understands */
    ok(rx_address_to_sockaddr(&addr, 0, 0, &sa) != 0,
      "rx_address_to_sockaddr rejects an unsupported family");

#ifndef KERNEL
    ok(rx_addrinfo_to_sockaddr(NULL, 0, &sa) != 0,
      "rx_addrinfo_to_sockaddr rejects a NULL addrinfo");
#endif
}

#ifndef KERNEL
static void
test_print(void)
{
    struct rx_sockaddr sa;
    rx_inet_fmtbuf_t buf;
    char *s;

    sa = mkv4(0xc0a80101, 80, 0);	/* 192.168.1.1:80 */
    s = rx_sockaddr2str(&sa, &buf);
    is_string("192.168.1.1:80", s, "rx_sockaddr2str formats a v4 address as addr:port");
}
#endif

#ifdef HAVE_IPV6

static struct rx_sockaddr
mkv6(const char *addrtext, afs_uint16 port_hbo, rx_service_t service)
{
    struct rx_address a;
    struct rx_sockaddr sa;

    memset(&a, 0, sizeof(a));
    a.addrtype = AF_INET6;
    ok(inet_pton(AF_INET6, addrtext, &a.rxa_in6_addr) == 1,
      "inet_pton parses test address %s", addrtext);
    ok(rx_address_to_sockaddr(&a, htons(port_hbo), service, &sa) == 0,
      "rx_address_to_sockaddr builds a v6 sockaddr for %s", addrtext);
    return sa;
}

static void
test_ipv6_roundtrip(void)
{
    struct rx_sockaddr sa = mkv6("2001:db8::1", 7000, 1);
    struct rx_address a, b;

    is_int(AF_INET6, sa.rxsa_family, "v6 sockaddr has AF_INET6 family");
    is_int(sizeof(struct sockaddr_in6), sa.addrlen,
	  "v6 sockaddr has correct addrlen");
    is_int(htons(7000), rx_get_sockaddr_port(&sa),
	  "rx_get_sockaddr_port reads back the v6 port");

    ok(rx_sockaddr_to_address(&sa, &a) == 0,
      "rx_sockaddr_to_address succeeds for a v6 sockaddr");
    is_int(AF_INET6, a.addrtype, "resulting rx_address has AF_INET6 type");

    ok(rx_address_to_sockaddr(&a, htons(7000), 1, &sa) == 0,
      "rx_address_to_sockaddr rebuilds the v6 sockaddr");
    ok(rx_sockaddr_to_address(&sa, &b) == 0, "round trip completes");
    ok(rx_compare_address(&a, &b), "v6 address survives sockaddr round trip");
}

static void
test_ipv6_compare_order_hash(void)
{
    struct rx_sockaddr a = mkv6("2001:db8::1", 7000, 1);
    struct rx_sockaddr b = mkv6("2001:db8::1", 7000, 1);
    struct rx_sockaddr c = mkv6("2001:db8::2", 7000, 1);
    struct rx_sockaddr v4 = mkv4(0x0a000001, 7000, 1);
    afs_uint32 h1, h2;

    ok(rx_compare_sockaddr(&a, &b, RXA_ALL), "identical v6 addrs compare equal");
    ok(!rx_compare_sockaddr(&a, &c, RXA_ADDR), "different v6 addrs compare unequal");
    ok(!rx_compare_sockaddr(&a, &v4, RXA_ADDR),
      "a v6 addr never compares equal to a v4 addr");

    is_int(0, rx_order_sockaddr(&a, &b), "identical v6 addrs order equal");
    ok(rx_order_sockaddr(&a, &c) < 0, "2001:db8::1 sorts before 2001:db8::2");
    ok(rx_order_sockaddr(&c, &a) > 0, "...and the reverse comparison is antisymmetric");
    ok(rx_order_sockaddr(&v4, &a) != 0,
      "ordering is consistent (non-zero) between different families");

    h1 = rx_hash_sockaddr(&a, 257);
    h2 = rx_hash_sockaddr(&b, 257);
    is_int(h1, h2, "hash of identical v6 addrs is identical");
    ok(h1 < 257, "v6 hash respects the requested hashsize");
}

static void
test_ipv6_loopback(void)
{
    struct rx_sockaddr loop = mkv6("::1", 7000, 0);
    struct rx_sockaddr notloop = mkv6("2001:db8::1", 7000, 0);
    struct rx_sockaddr mapped = mkv6("::ffff:127.0.0.1", 7000, 0);

    ok(rx_is_loopback_sockaddr(&loop), "::1 is loopback");
    ok(!rx_is_loopback_sockaddr(&notloop), "2001:db8::1 is not loopback");
    ok(rx_is_loopback_sockaddr(&mapped),
      "a v4-mapped ::ffff:127.0.0.1 is recognised as loopback");
}

static void
test_ipv6_no_v4_projection(void)
{
    struct rx_sockaddr sa = mkv6("2001:db8::1", 7000, 0);
    afs_uint32 ipv4;

    ok(!rx_try_sockaddr_to_ipv4(&sa, &ipv4),
      "a non-mapped v6 address has no ipv4 projection");
}

# ifndef KERNEL
static void
test_ipv6_print(void)
{
    struct rx_sockaddr sa = mkv6("2001:db8::1", 443, 0);
    rx_inet_fmtbuf_t buf;
    char *s;

    s = rx_sockaddr2str(&sa, &buf);
    is_string("[2001:db8::1]:443", s,
	     "rx_sockaddr2str brackets a v6 address with its port");
}
# endif

#endif /* HAVE_IPV6 */

int
main(int argc, char **argv)
{
    plan_lazy();

    test_ipv4_roundtrip();
    test_ipv4_compare_and_hash();
    test_ipv4_loopback();
    test_ipv4_address_type();
    test_malformed();
#ifndef KERNEL
    test_print();
#endif

#ifdef HAVE_IPV6
    test_ipv6_roundtrip();
    test_ipv6_compare_order_hash();
    test_ipv6_loopback();
    test_ipv6_no_v4_projection();
# ifndef KERNEL
    test_ipv6_print();
# endif
#else
    diag("HAVE_IPV6 not defined; skipping IPv6-specific checks");
#endif

    return 0;
}
