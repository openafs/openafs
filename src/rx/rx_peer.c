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

afs_uint32 rx_HostOf(struct rx_peer *peer) {
    return peer->host;
}

u_short rx_PortOf(struct rx_peer *peer) {
    return peer->port;
}
