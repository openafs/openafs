/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
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

/*!
 * @file rx_stats.c
 *
 * Code for handling statistics gathering within RX, and for mapping the
 * internal representation into an external one
 */
#include <afsconfig.h>
#include <afs/param.h>

#if !defined(KERNEL)
#include <roken.h>
#include <string.h>
#endif

#ifdef KERNEL
/* no kmutex, no atomic emulation...*/
#include "rx/rx_kcommon.h"
#else
#include "rx.h"
#endif
#include "rx_atomic.h"
#include "rx_stats.h"

/* Globals */

/*!
 * rx_stats_mutex protects the non-atomic members of the rx_stats structure
 */
#if defined(RX_ENABLE_LOCKS)
afs_kmutex_t rx_stats_mutex;
#endif

struct rx_statisticsAtomic rx_stats;

/*!
 * Return the internal statistics collected by rx
 *
 * @return
 * 	A statistics structure which must be freed using rx_FreeStatistics
 * @notes
 * 	Takes, and releases rx_stats_mutex
 */
struct rx_statistics *
rx_GetStatistics(void) {
    struct rx_statistics *stats = rxi_Alloc(sizeof(struct rx_statistics));
    MUTEX_ENTER(&rx_stats_mutex);
    memcpy(stats, &rx_stats, sizeof(struct rx_statistics));
    MUTEX_EXIT(&rx_stats_mutex);

    return stats;
}

/*!
 * Free a statistics block allocated by rx_GetStatistics
 *
 * @param stats
 * 	The statistics block to free
 */
void
rx_FreeStatistics(struct rx_statistics **stats) {
    if (*stats) {
	rxi_Free(*stats, sizeof(struct rx_statistics));
        *stats = NULL;
    }
}

/*!
 * Zero the internal statistics structure
 *
 * @private
 */

void
rxi_ResetStatistics(void) {
    memset(&rx_stats, 0, sizeof(struct rx_statisticsAtomic));
}
