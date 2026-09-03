/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include "rx.h"

#include "rx_atomic.h"
#include "rx_clock.h"
#include "rx_peer.h"

/*
 * rx_HostOf/rx_PortOf return the IPv4 view of a peer's address, for
 * compatibility with the many existing callers (including code emitted by
 * rxgen) that still deal in bare afs_uint32/u_short. A v6-only peer with no
 * IPv4-mapped address projects to host 0. rx_SockaddrOf() is the real,
 * family-agnostic accessor.
 */
afs_uint32 rx_HostOf(struct rx_peer *peer) {
    afs_uint32 ipv4 = 0;
    (void)rx_try_sockaddr_to_ipv4(&peer->saddr, &ipv4);
    return ipv4;
}

u_short rx_PortOf(struct rx_peer *peer) {
    return rx_get_sockaddr_port(&peer->saddr);
}

const struct rx_sockaddr *
rx_SockaddrOf(struct rx_peer *peer) {
    return &peer->saddr;
}
