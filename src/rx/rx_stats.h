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

/* rx_stats.h
 *
 * These are internal structures used by the rx statistics code. Nothing
 * in this file should be visible outside the RX module.
 */

/* We memcpy between rx_statisticsAtomic and rx_statistics, so the two
 * structures must have the same elements, and sizeof(rx_atomic_t) must
 * equal sizeof(int)
 */

struct rx_statisticsAtomic {    /* Atomic version of rx_statistics */
    rx_atomic_t packetRequests;
    rx_atomic_t receivePktAllocFailures;
    rx_atomic_t sendPktAllocFailures;
    rx_atomic_t specialPktAllocFailures;
    rx_atomic_t socketGreedy;
    rx_atomic_t bogusPacketOnRead;
    int bogusHost;
    rx_atomic_t noPacketOnRead;
    rx_atomic_t noPacketBuffersOnRead;
    rx_atomic_t selects;
    rx_atomic_t sendSelects;
    rx_atomic_t packetsRead[RX_N_PACKET_TYPES];
    rx_atomic_t dataPacketsRead;
    rx_atomic_t ackPacketsRead;
    rx_atomic_t dupPacketsRead;
    rx_atomic_t spuriousPacketsRead;
    rx_atomic_t packetsSent[RX_N_PACKET_TYPES];
    rx_atomic_t ackPacketsSent;
    rx_atomic_t pingPacketsSent;
    rx_atomic_t abortPacketsSent;
    rx_atomic_t busyPacketsSent;
    rx_atomic_t dataPacketsSent;
    rx_atomic_t dataPacketsReSent;
    rx_atomic_t dataPacketsPushed;
    rx_atomic_t ignoreAckedPacket;
    struct clock totalRtt;
    struct clock minRtt;
    struct clock maxRtt;
    rx_atomic_t nRttSamples;
    rx_atomic_t nServerConns;
    rx_atomic_t nClientConns;
    rx_atomic_t nPeerStructs;
    rx_atomic_t nCallStructs;
    rx_atomic_t nFreeCallStructs;
    rx_atomic_t netSendFailures;
    rx_atomic_t fatalErrors;
    rx_atomic_t ignorePacketDally;
    rx_atomic_t receiveCbufPktAllocFailures;
    rx_atomic_t sendCbufPktAllocFailures;
    rx_atomic_t nBusies;
    rx_atomic_t spares[4];
};

#if defined(RX_ENABLE_LOCKS)
extern afs_kmutex_t rx_stats_mutex;
#endif

extern struct rx_statisticsAtomic rx_stats;

extern void rxi_ResetStatistics(void);
