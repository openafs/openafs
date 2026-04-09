/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* statistics-gathering package */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs_stats.h"

struct afs_CMStats afs_cmstats;
struct afs_stats_CMPerf afs_stats_cmperf;
struct afs_stats_CMFullPerf afs_stats_cmfullperf;
afs_int32 afs_stats_XferSumBytes[AFS_STATS_NUM_FS_XFER_OPS];

#if defined(AFS_DARWIN_ENV)
/*
 * On DARWIN, 'struct vnode' is opaque, so we cannot get the size of it
 * directly. But the size of 'struct vnode' is not hidden; the definition of
 * 'struct vnode' is available in vnode_internal.h, which is publically
 * available (just not shipped in the MacOS SDKs).
 *
 * Also, vnodes on macOS are allocated via "zone"s, and information about zones
 * can be obtained by running the "zprint" command on macOS. Over time, this
 * value can change, but the current value was last checked by running "zprint"
 * on macOS 15:
 *
 * $ zprint vnodes
 *                             elem         cur         max        cur         max         cur  alloc  alloc
 * zone name                   size        size        size      #elts       #elts       inuse   size  count
 * -------------------------------------------------------------------------------------------------------------
 * vnodes                       264      xxxxxK      xxxxxK     xxxxxx      xxxxxx      xxxxxx     xK     xx
 */
# define AFS_SIZEOF_VNODE 264
#else
# define AFS_SIZEOF_VNODE (sizeof(struct vnode))
#endif /* AFS_DARWIN_ENV */

/*
 * afs_InitStats
 *
 * Description:
 *	Initialize all of the CM statistics structures.
 *
 * Parameters:
 *	None.
 *
 * Environment:
 *	This routine should only be called once, at initialization time.
 */
void
afs_InitStats(void)
{
    struct afs_stats_opTimingData *opTimeP;	/*Ptr to curr timing struct */
    struct afs_stats_xferData *xferP;	/*Ptr to curr xfer struct */
    int currIdx;		/*Current index */

    /*
     * First step is to zero everything out.
     */
    memset((&afs_cmstats), 0, sizeof(struct afs_CMStats));
    memset((&afs_stats_cmperf), 0, sizeof(struct afs_stats_CMPerf));
    memset((&afs_stats_cmfullperf), 0,
	   sizeof(struct afs_stats_CMFullPerf));

    /*
     * Some fields really should be non-zero at the start, so set 'em up.
     */
    afs_stats_cmperf.srvNumBuckets = NSERVERS;

    opTimeP = &(afs_stats_cmfullperf.rpc.fsRPCTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS;
	 currIdx++, opTimeP++)
	opTimeP->minTime.tv_sec = 999999;

    opTimeP = &(afs_stats_cmfullperf.rpc.cmRPCTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS;
	 currIdx++, opTimeP++)
	opTimeP->minTime.tv_sec = 999999;

    xferP = &(afs_stats_cmfullperf.rpc.fsXferTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++, xferP++) {
	xferP->minTime.tv_sec = 999999;
	xferP->minBytes = 999999999;
    }

    afs_stats_cmperf.sizeof_struct_vcache = sizeof(struct vcache);
    afs_stats_cmperf.sizeof_struct_vnode = AFS_SIZEOF_VNODE;

    afs_stats_cmperf.stat_entry_size = afs_stats_cmperf.sizeof_struct_vcache;
#if !defined(AFS_VCACHE_EMBEDDED_VNODE)
    afs_stats_cmperf.stat_entry_size += afs_stats_cmperf.sizeof_struct_vnode;
#endif
}
