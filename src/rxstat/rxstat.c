/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef UKERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif
#include <afsconfig.h>

RCSID
    ("$Header$");

#ifdef UKERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include <rx/rxstat.h>
#else /* UKERNEL */
#include <afs/stds.h>
#include <rx/rx.h>
#include <rx/rxstat.h>
#ifdef KERNEL
#include "sys/errno.h"
#else /* KERNEL */
#include <errno.h>
#endif /* KERNEL */
#endif /* UKERNEL */

/*
 * This file creates a centralized mechanism for implementing the rpc
 * stat code - which is generic across all servers.
 */

afs_int32
MRXSTATS_RetrieveProcessRPCStats(struct rx_call *call,
				 IN afs_uint32 clientVersion,
				 OUT afs_uint32 * serverVersion,
				 OUT afs_uint32 * clock_sec,
				 OUT afs_uint32 * clock_usec,
				 OUT afs_uint32 * stat_count,
				 OUT rpcStats * stats)
{
    afs_int32 rc;
    size_t allocSize;

    rc = rx_RetrieveProcessRPCStats(clientVersion, serverVersion, clock_sec,
				    clock_usec, &allocSize, stat_count,
				    &stats->rpcStats_val);
    stats->rpcStats_len = (u_int)(allocSize / sizeof(afs_uint32));
    return rc;
}


afs_int32
MRXSTATS_RetrievePeerRPCStats(struct rx_call * call,
			      IN afs_uint32 clientVersion,
			      OUT afs_uint32 * serverVersion,
			      OUT afs_uint32 * clock_sec,
			      OUT afs_uint32 * clock_usec,
			      OUT afs_uint32 * stat_count,
			      OUT rpcStats * stats)
{
    afs_int32 rc;
    size_t allocSize;

    rc = rx_RetrievePeerRPCStats(clientVersion, serverVersion, clock_sec,
				 clock_usec, &allocSize, stat_count,
				 &stats->rpcStats_val);
    stats->rpcStats_len = (u_int)(allocSize / sizeof(afs_uint32));
    return rc;
}


afs_int32
MRXSTATS_QueryProcessRPCStats(struct rx_call * call, OUT afs_int32 * on)
{
    afs_int32 rc = 0;
    *on = rx_queryProcessRPCStats();
    return rc;
}


afs_int32
MRXSTATS_QueryPeerRPCStats(struct rx_call * call, OUT afs_int32 * on)
{
    afs_int32 rc = 0;
    *on = rx_queryPeerRPCStats();
    return rc;
}


afs_int32
MRXSTATS_EnableProcessRPCStats(struct rx_call * call)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_enableProcessRPCStats();
    }
    return rc;
}

afs_int32
MRXSTATS_EnablePeerRPCStats(struct rx_call * call)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_enablePeerRPCStats();
    }
    return rc;
}


afs_int32
MRXSTATS_DisableProcessRPCStats(struct rx_call * call)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_disableProcessRPCStats();
    }
    return rc;
}

afs_int32
MRXSTATS_DisablePeerRPCStats(struct rx_call * call)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_disablePeerRPCStats();
    }
    return rc;
}

afs_int32
MRXSTATS_QueryRPCStatsVersion(struct rx_call * call, OUT afs_uint32 * ver)
{
    afs_int32 rc = 0;
    *ver = RX_STATS_RETRIEVAL_VERSION;
    return rc;
}

afs_int32
MRXSTATS_ClearProcessRPCStats(struct rx_call * call, IN afs_uint32 clearFlag)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_clearProcessRPCStats(clearFlag);
    }
    return rc;
}

afs_int32
MRXSTATS_ClearPeerRPCStats(struct rx_call * call, IN afs_uint32 clearFlag)
{
    afs_int32 rc = 0;
    if (!rx_RxStatUserOk(call)) {
	rc = EPERM;
    } else {
	rx_clearPeerRPCStats(clearFlag);
    }
    return rc;
}
