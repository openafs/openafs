/*
 * Copyright (c) 2020 Sine Nomine Associates. All Rights Reserved.
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
 * Solaris kstat provider for OpenAFS.
 */

#include <afsconfig.h>
#include "afs/param.h"

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/kstat.h>

#include <rx/rx.h>

#include "afs/sysincludes.h"
#include "afs/afsincludes.h"
#include "afsint.h"
#include "vldbint.h"
#include "afs/lock.h"
#include "afs/afs_stats.h"
#include "afs/afs.h"
#include "afs/afs_prototypes.h"
#include "rx/rx_atomic.h"
#include "rx/rx_stats.h"

extern char *AFSVersion;

static kstat_t *afs_param_ksp;
static kstat_t *afs_cache_ksp;
static kstat_t *afs_rx_ksp;

static char cellname[AFSNAMEMAX];

/*
 * Cache manager initialization parameters.
 *
 *	nChunkFiles	The number of disk files to allocate to the cache.
 *	nStatCaches	The number of stat cache (vnode) entries to allocate.
 *	nDataCaches	Number of dcache entries to allocate.
 *	nVolumeCaches	Number of volume cache entries to allocate.
 *	chunkSize	The size of file chunks.
 *	cacheSize	The max number of 1 Kbyte blocks the cache may occupy.
 *	setTime		Indicates clock setting is enabled.
 *	memCache	Indicates memory-based cache is enabled.
 */
struct afs_param_kstat {
    kstat_named_t nChunkFiles;
    kstat_named_t nStatCaches;
    kstat_named_t nDataCaches;
    kstat_named_t nVolumeCaches;
    kstat_named_t chunkSize;
    kstat_named_t cacheSize;
    kstat_named_t setTime;
    kstat_named_t memCache;
};
static struct afs_param_kstat param_kstats = {
    .nChunkFiles = {"nChunkFiles", KSTAT_DATA_ULONG},
    .nStatCaches = {"nStatCaches", KSTAT_DATA_ULONG},
    .nDataCaches = {"nDataCaches", KSTAT_DATA_ULONG},
    .nVolumeCaches = {"nVolumeCaches", KSTAT_DATA_ULONG},
    .chunkSize = {"chunkSize", KSTAT_DATA_ULONG},
    .cacheSize = {"cacheSize", KSTAT_DATA_ULONG},
    .setTime = {"setTime", KSTAT_DATA_ULONG},
    .memCache = {"memCache", KSTAT_DATA_ULONG},
};

/*
 * Update the param kstat.
 *
 * All file chunks are the same size, so do not bother reporting the first
 * chunk size and other chunk size as two different numbers.
 */
static int
afs_param_ks_update(kstat_t * ksp, int rw)
{
    struct afs_param_kstat *k = ksp->ks_data;
    struct cm_initparams *s = &cm_initParams;

    if (rw == KSTAT_WRITE) {
	return EACCES;
    }

#define KSET(ks) do { k->ks.value.ul = s->cmi_ ## ks; } while (0)
#define KSET2(ks,src) do { k->ks.value.ul = s->cmi_ ## src; } while (0)
    KSET(nChunkFiles);
    KSET(nStatCaches);
    KSET(nDataCaches);
    KSET(nVolumeCaches);
    KSET2(chunkSize, firstChunkSize);  /* same as otherChunkSize */
    KSET(cacheSize);
    KSET(setTime);
    KSET(memCache);
#undef KSET
#undef KSET2
    return 0;
}

/*
 * Cache statistics.
 *
 *	dlocalAccesses		Number of data accesses to files within cell.
 *	vlocalAccesses		Number of stat accesses to files within cell.
 *	dremoteAccesses		Number of data accesses to files outside of cell.
 *	vremoteAccesses		Number of stat accesses to files outside of cell.
 *	cacheNumEntries		Number of cache entries (should match cacheFilesTotal).
 *	cacheBlocksTotal	Number of 1K blocks configured for cache.
 *	cacheBlocksInUse	Number of cache blocks actively in use.
 *	cacheMaxDirtyChunks	Max number of dirty cache chunks tolerated.
 *	cacheCurrDirtyChunks	Current number of dirty cache chunks.
 *	dcacheHits		Count of data files (daches) found in local cache.
 *	vcacheHits		Count of stat entries (vcaches) found in local cache.
 *	dcacheMisses		Count of data files NOT found in local cache.
 *	vcacheMisses		Count of of stat entries NOT found in local cache.
 *	cacheFlushes		Count of times files are flushed from cache.
 *	cacheFilesReused	Number of cache files reused.
 *	vcacheXAllocs		Count of additionally allocated vcaches.
 *	dcacheXAllocs		Count of additionally allocated dcaches.
 *	cacheFilesTotal		Number of cache files (chunks).
 *	cacheFilesFree		Number of cache files (chunks) available.
 *	cacheFilesInUse		Number of cache files (chunks) in use.
 *	cacheTooFull		Number of times the cache size or number of files exceeded.
 *	waitForDrain		Number of times waited for cache to drain.
 *	cellname		Local cellname.
 *	version			OpenAFS version of the cache manager.
 */
struct afs_cache_kstat {
    kstat_named_t dlocalAccesses;
    kstat_named_t vlocalAccesses;
    kstat_named_t dremoteAccesses;
    kstat_named_t vremoteAccesses;
    kstat_named_t cacheNumEntries;
    kstat_named_t cacheBlocksTotal;
    kstat_named_t cacheBlocksInUse;
    kstat_named_t cacheMaxDirtyChunks;
    kstat_named_t cacheCurrDirtyChunks;
    kstat_named_t dcacheHits;
    kstat_named_t vcacheHits;
    kstat_named_t dcacheMisses;
    kstat_named_t vcacheMisses;
    kstat_named_t cacheFlushes;
    kstat_named_t cacheFilesReused;
    kstat_named_t vcacheXAllocs;
    kstat_named_t dcacheXAllocs;
    kstat_named_t cacheFilesTotal;
    kstat_named_t cacheFilesFree;
    kstat_named_t cacheFilesInUse;
    kstat_named_t cacheTooFull;
    kstat_named_t waitForDrain;
    kstat_named_t cellname;
    kstat_named_t version;
};
static struct afs_cache_kstat cache_kstats = {
    .dlocalAccesses = {"dlocalAccesses", KSTAT_DATA_ULONG},
    .vlocalAccesses = {"vlocalAccesses", KSTAT_DATA_ULONG},
    .dremoteAccesses = {"dremoteAccesses", KSTAT_DATA_ULONG},
    .vremoteAccesses = {"vremoteAccesses", KSTAT_DATA_ULONG},
    .cacheNumEntries = {"cacheNumEntries", KSTAT_DATA_ULONG},
    .cacheBlocksTotal = {"cacheBlocksTotal", KSTAT_DATA_ULONG},
    .cacheBlocksInUse = {"cacheBlocksInUse", KSTAT_DATA_ULONG},
    .cacheMaxDirtyChunks = {"cacheMaxDirtyChunks", KSTAT_DATA_ULONG},
    .cacheCurrDirtyChunks = {"cacheCurrDirtyChunks", KSTAT_DATA_ULONG},
    .dcacheHits = {"dcacheHits", KSTAT_DATA_ULONG},
    .vcacheHits = {"vcacheHits", KSTAT_DATA_ULONG},
    .dcacheMisses = {"dcacheMisses", KSTAT_DATA_ULONG},
    .vcacheMisses = {"vcacheMisses", KSTAT_DATA_ULONG},
    .cacheFlushes = {"cacheFlushes", KSTAT_DATA_ULONG},
    .cacheFilesReused = {"cacheFilesReused", KSTAT_DATA_ULONG},
    .vcacheXAllocs = {"vcacheXAllocs", KSTAT_DATA_ULONG},
    .dcacheXAllocs = {"dcacheXAllocs", KSTAT_DATA_ULONG},
    .cacheFilesTotal = {"cacheFilesTotal", KSTAT_DATA_ULONG},
    .cacheFilesFree = {"cacheFilesFree", KSTAT_DATA_ULONG},
    .cacheFilesInUse = {"cacheFilesInUse", KSTAT_DATA_ULONG},
    .cacheTooFull = {"cacheTooFull", KSTAT_DATA_ULONG},
    .waitForDrain = {"waitForDrain", KSTAT_DATA_ULONG},
    .cellname = {"cellname", KSTAT_DATA_STRING},
    .version = {"version", KSTAT_DATA_STRING},
};

/*
 * Update the cache kstat.
 */
static int
afs_cache_ks_update(kstat_t * ksp, int rw)
{
    struct afs_cache_kstat *k = ksp->ks_data;
    struct afs_stats_CMPerf *s = &afs_stats_cmperf;

    if (rw == KSTAT_WRITE) {
	return EACCES;
    }


#define KSET(ks) do {k->ks.value.ul = s->ks;} while (0)
    KSET(dlocalAccesses);
    KSET(vlocalAccesses);
    KSET(dremoteAccesses);
    KSET(vremoteAccesses);
    KSET(cacheNumEntries);
    KSET(cacheBlocksTotal);
    KSET(cacheBlocksInUse);
    /* cacheBlocksOrig is a duplicate of cacheSize, so not repeated here. */
    KSET(cacheMaxDirtyChunks);
    KSET(cacheCurrDirtyChunks);
    KSET(dcacheHits);
    KSET(vcacheHits);
    KSET(dcacheMisses);
    KSET(vcacheMisses);
    KSET(cacheFlushes);
    KSET(cacheFilesReused);
    KSET(vcacheXAllocs);
    KSET(dcacheXAllocs);
#undef KSET

    /* The following stats not provided by xstat. */
    k->cacheFilesTotal.value.ul = afs_cacheFiles;
    k->cacheFilesFree.value.ul = afs_freeDCCount;
    k->cacheFilesInUse.value.ul = afs_cacheFiles - afs_freeDCCount;
    k->cacheTooFull.value.ul = afs_CacheTooFullCount;
    k->waitForDrain.value.ul = afs_WaitForCacheDrainCount;
    /* The following two values never change after OpenAFS init. */
    kstat_named_setstr(&k->cellname, cellname);
    kstat_named_setstr(&k->version, AFSVersion);

    return 0;
}

/*
 * Rx statistics.
 *
 *	packetRequests		Number of packet allocation requests.
 *
 *	receivePktAllocFailures	Packet allocation failure, receive.
 *	sendPktAllocFailures	Packet allocation failure, send.
 *	specialPktAllocFailures	Packet allocation failure, special.
 *
 *	socketGreedy		Whether SO_GREEDY succeeded.
 *	bogusPacketOnRead	Number of inappropriately short packets received.
 *	bogusHost		Host address from bogus packets.
 *	noPacketOnRead		Number of read packets attempted when there was
 *				 actually no packet to read off the wire.
 *	noPacketBuffersOnRead	Number of dropped data packets due to lack of
 *				 packet buffers.
 *	selects			Number of selects waiting for packet or timeout.
 *	sendSelects		Number of selects forced when sending packet.
 *
 *	packetsRead_data	Packets read, data type.
 *	packetsRead_ack		Packets read, ack type.
 *	packetsRead_busy	Packets read, busy type.
 *	packetsRead_abort	Packets read, abort type.
 *	packetsRead_ackall	Packets read, ackall type.
 *	packetsRead_challenge	Packets read, challenge type.
 *	packetsRead_response	Packets read, response type.
 *	packetsRead_debug	Packets read, debug type.
 *	packetsRead_params	Packets read, params type.
 *	packetsRead_unused0	Packets read, should be zero.
 *	packetsRead_unused1	Packets read, should be zero.
 *	packetsRead_unused2	Packets read, should be zero.
 *	packetsRead_version	Packets read, version type.
 *
 *	dataPacketsRead		Number of unique data packets read off the wire.
 *	ackPacketsRead		Number of ack packets read.
 *	dupPacketsRead		Number of duplicate data packets read.
 *	spuriousPacketsRead	Number of inappropriate data packets.
 *
 *	packetsSent_data	Packets sent, data type.
 *	packetsSent_ack		Packets sent, ack type.
 *	packetsSent_busy	Packets sent, busy type.
 *	packetsSent_abort	Packets sent, abort type.
 *	packetsSent_ackall	Packets sent, ackall type.
 *	packetsSent_challenge	Packets sent, challenge type.
 *	packetsSent_response	Packets sent, response type.
 *	packetsSent_debug	Packets sent, debug type.
 *	packetsSent_params	Packets sent, params type.
 *	packetsSent_unused0	Packets sent, should be zero.
 *	packetsSent_unused1	Packets sent, should be zero.
 *	packetsSent_unused2	Packets sent, should be zero.
 *	packetsSent_version	Packets sent, version type.
 *
 *	ackPacketsSent		Number of acks sent.
 *	pingPacketsSent		Total number of ping packets sent.
 *	abortPacketsSent	Total number of aborts sent.
 *	busyPacketsSent		Total number of busies sent.
 *	dataPacketsSent		Number of unique data packets sent.
 *	dataPacketsReSent	Number of retransmissions.
 *	dataPacketsPushed	Number of retransmissions pushed early by a NACK.
 *	ignoreAckedPacket	Number of packets with acked flag, on rxi_Start.
 *
 *	totalRtt_sec		Total round trip time measured, sec component.
 *	totalRtt_usec		Total rount trip time messured, usec component.
 *	minRtt_sec		Minimum round trip time measured, sec component
 *	minRtt_usec		Minimum rount trip time measured, usec component.
 *	maxRtt_sec		Maximum round trip time measured, sec component.
 *	maxRtt_usec		Maximum round trip time measured, usec component.
 *
 *	nRttSamples		Total number of round trip samples.
 *	nServerConns		Total number of server connections.
 *	nClientConns		Total number of client connections.
 *	nPeerStructs		Total number of peer structures.
 *	nCallStructs		Total number of call structures allocated.
 *	nFreeCallStructs	Total number of previously allocated free call
 *				 structures.
 *
 *	netSendFailures		Number of times osi_NetSend failed.
 *	fatalErrors		Number of connection errors.
 *	ignorePacketDally	Packets dropped because call is in dally state.
 *
 *	receiveCbufPktAllocFailures	Packet allocation failure, receive cbuf.
 *	sendCbufPktAllocFailures	Packet allocation failure, send cbuf.
 *
 *	nBusies			Number of times queued calls exceeds overload threshold.
 */
struct afs_rx_kstat {
    kstat_named_t packetRequests;
    kstat_named_t receivePktAllocFailures;
    kstat_named_t sendPktAllocFailures;
    kstat_named_t specialPktAllocFailures;
    kstat_named_t socketGreedy;
    kstat_named_t bogusPacketOnRead;
    kstat_named_t bogusHost;
    kstat_named_t noPacketOnRead;
    kstat_named_t noPacketBuffersOnRead;
    kstat_named_t selects;
    kstat_named_t sendSelects;
    kstat_named_t packetsRead[RX_N_PACKET_TYPES];
    kstat_named_t dataPacketsRead;
    kstat_named_t ackPacketsRead;
    kstat_named_t dupPacketsRead;
    kstat_named_t spuriousPacketsRead;
    kstat_named_t packetsSent[RX_N_PACKET_TYPES];
    kstat_named_t ackPacketsSent;
    kstat_named_t pingPacketsSent;
    kstat_named_t abortPacketsSent;
    kstat_named_t busyPacketsSent;
    kstat_named_t dataPacketsSent;
    kstat_named_t dataPacketsReSent;
    kstat_named_t dataPacketsPushed;
    kstat_named_t ignoreAckedPacket;
    kstat_named_t totalRtt_sec;
    kstat_named_t totalRtt_usec;
    kstat_named_t minRtt_sec;
    kstat_named_t minRtt_usec;
    kstat_named_t maxRtt_sec;
    kstat_named_t maxRtt_usec;
    kstat_named_t nRttSamples;
    kstat_named_t nServerConns;
    kstat_named_t nClientConns;
    kstat_named_t nPeerStructs;
    kstat_named_t nCallStructs;
    kstat_named_t nFreeCallStructs;
    kstat_named_t netSendFailures;
    kstat_named_t fatalErrors;
    kstat_named_t ignorePacketDally;
    kstat_named_t receiveCbufPktAllocFailures;
    kstat_named_t sendCbufPktAllocFailures;
    kstat_named_t nBusies;
};
static struct afs_rx_kstat rx_kstats = {
    .packetRequests = {"packetRequests", KSTAT_DATA_ULONG},
    .receivePktAllocFailures = {"receivePktAllocFailures", KSTAT_DATA_ULONG},
    .sendPktAllocFailures = {"sendPktAllocFailures", KSTAT_DATA_ULONG},
    .specialPktAllocFailures = {"specialPktAllocFailures", KSTAT_DATA_ULONG},
    .socketGreedy = {"socketGreedy", KSTAT_DATA_ULONG},
    .bogusPacketOnRead = {"bogusPacketOnRead", KSTAT_DATA_ULONG},
    .bogusHost = {"bogusHost", KSTAT_DATA_ULONG},
    .noPacketOnRead = {"noPacketOnRead", KSTAT_DATA_ULONG},
    .noPacketBuffersOnRead = {"noPacketBuffersOnRead", KSTAT_DATA_ULONG},
    .selects = {"selects", KSTAT_DATA_ULONG},
    .sendSelects = {"sendSelects", KSTAT_DATA_ULONG},
    .packetsRead = {
	{"packetsRead_data", KSTAT_DATA_ULONG},
	{"packetsRead_ack", KSTAT_DATA_ULONG},
	{"packetsRead_busy", KSTAT_DATA_ULONG},
	{"packetsRead_abort", KSTAT_DATA_ULONG},
	{"packetsRead_ackall", KSTAT_DATA_ULONG},
	{"packetsRead_challenge", KSTAT_DATA_ULONG},
	{"packetsRead_response", KSTAT_DATA_ULONG},
	{"packetsRead_debug", KSTAT_DATA_ULONG},
	{"packetsRead_params", KSTAT_DATA_ULONG},
	{"packetsRead_unused0", KSTAT_DATA_ULONG},
	{"packetsRead_unused1", KSTAT_DATA_ULONG},
	{"packetsRead_unused2", KSTAT_DATA_ULONG},
	{"packetsRead_version", KSTAT_DATA_ULONG},
    },
    .dataPacketsRead = {"dataPacketsRead", KSTAT_DATA_ULONG},
    .ackPacketsRead = {"ackPacketsRead", KSTAT_DATA_ULONG},
    .dupPacketsRead = {"dupPacketsRead", KSTAT_DATA_ULONG},
    .spuriousPacketsRead = {"spuriousPacketsRead", KSTAT_DATA_ULONG},
    .packetsSent = {
	{"packetsSent_data", KSTAT_DATA_ULONG},
	{"packetsSent_ack", KSTAT_DATA_ULONG},
	{"packetsSent_busy", KSTAT_DATA_ULONG},
	{"packetsSent_abort", KSTAT_DATA_ULONG},
	{"packetsSent_ackall", KSTAT_DATA_ULONG},
	{"packetsSent_challenge", KSTAT_DATA_ULONG},
	{"packetsSent_response", KSTAT_DATA_ULONG},
	{"packetsSent_debug", KSTAT_DATA_ULONG},
	{"packetsSent_params", KSTAT_DATA_ULONG},
	{"packetsSent_unused0", KSTAT_DATA_ULONG},
	{"packetsSent_unused1", KSTAT_DATA_ULONG},
	{"packetsSent_unused2", KSTAT_DATA_ULONG},
	{"packetsSent_version", KSTAT_DATA_ULONG},
    },
    .ackPacketsSent = {"ackPacketsSent", KSTAT_DATA_ULONG},
    .pingPacketsSent = {"pingPacketsSent", KSTAT_DATA_ULONG},
    .abortPacketsSent = {"abortPacketsSent", KSTAT_DATA_ULONG},
    .busyPacketsSent = {"busyPacketsSent", KSTAT_DATA_ULONG},
    .dataPacketsSent = {"dataPacketsSent", KSTAT_DATA_ULONG},
    .dataPacketsReSent = {"dataPacketsReSent", KSTAT_DATA_ULONG},
    .dataPacketsPushed = {"dataPacketsPushed", KSTAT_DATA_ULONG},

    .ignoreAckedPacket = {"ignoreAckedPacket", KSTAT_DATA_ULONG},
    .totalRtt_sec = {"totalRtt_sec", KSTAT_DATA_ULONG},
    .totalRtt_usec = {"totalRtt_usec", KSTAT_DATA_ULONG},
    .minRtt_sec = {"minRtt_sec", KSTAT_DATA_ULONG},
    .minRtt_usec = {"minRtt_usec", KSTAT_DATA_ULONG},
    .maxRtt_sec = {"maxRtt_sec", KSTAT_DATA_ULONG},
    .maxRtt_usec = {"maxRtt_usec", KSTAT_DATA_ULONG},
    .nRttSamples = {"nRttSamples", KSTAT_DATA_ULONG},
    .nServerConns = {"nServerConns", KSTAT_DATA_ULONG},
    .nClientConns = {"nClientConns", KSTAT_DATA_ULONG},
    .nPeerStructs = {"nPeerStructs", KSTAT_DATA_ULONG},
    .nCallStructs = {"nCallStructs", KSTAT_DATA_ULONG},
    .nFreeCallStructs = {"nFreeCallStructs", KSTAT_DATA_ULONG},
    .netSendFailures = {"netSendFailures", KSTAT_DATA_ULONG},
    .fatalErrors = {"fatalErrors", KSTAT_DATA_ULONG},
    .ignorePacketDally = {"ignorePacketDally", KSTAT_DATA_ULONG},
    .receiveCbufPktAllocFailures = {"receiveCbufPktAllocFailures", KSTAT_DATA_ULONG},
    .sendCbufPktAllocFailures = {"sendCbufPktAllocFailures", KSTAT_DATA_ULONG},
    .nBusies = {"nBusies", KSTAT_DATA_ULONG},
};

/*
 * Update the rx kstat.
 */
static int
afs_rx_ks_update(kstat_t * ksp, int rw)
{
    struct afs_rx_kstat *k = ksp->ks_data;  /* ks_data is void* */
    struct rx_statisticsAtomic *s = &rx_stats;
    int i;

    if (rw == KSTAT_WRITE) {
	return EACCES;
    }

#define KSET(ks) do {k->ks.value.ul = s->ks;} while (0)
#define KSET_ATOM(ks) do {k->ks.value.ul = rx_atomic_read(&(s->ks));} while (0)
    KSET_ATOM(packetRequests);
    KSET_ATOM(receivePktAllocFailures);
    KSET_ATOM(sendPktAllocFailures);
    KSET_ATOM(specialPktAllocFailures);
    KSET_ATOM(socketGreedy);
    KSET_ATOM(bogusPacketOnRead);
    KSET(bogusHost);
    KSET_ATOM(noPacketOnRead);
    KSET_ATOM(noPacketBuffersOnRead);
    KSET_ATOM(selects);
    KSET_ATOM(sendSelects);
    for (i = 0; i < RX_N_PACKET_TYPES; i++) {
	KSET_ATOM(packetsRead[i]);
    }
    KSET_ATOM(dataPacketsRead);
    KSET_ATOM(ackPacketsRead);
    KSET_ATOM(dupPacketsRead);
    KSET_ATOM(spuriousPacketsRead);
    for (i = 0; i < RX_N_PACKET_TYPES; i++) {
	KSET_ATOM(packetsSent[i]);
    }
    KSET_ATOM(ackPacketsSent);
    KSET_ATOM(pingPacketsSent);
    KSET_ATOM(abortPacketsSent);
    KSET_ATOM(busyPacketsSent);
    KSET_ATOM(dataPacketsSent);
    KSET_ATOM(dataPacketsReSent);
    KSET_ATOM(dataPacketsPushed);
    KSET_ATOM(ignoreAckedPacket);

    /* not rx_atomic */
    k->totalRtt_sec.value.ul = s->totalRtt.sec;
    k->totalRtt_usec.value.ul = s->totalRtt.usec;
    k->minRtt_sec.value.ul = s->minRtt.sec;
    k->minRtt_usec.value.ul = s->minRtt.usec;
    k->maxRtt_sec.value.ul = s->maxRtt.sec;
    k->maxRtt_usec.value.ul = s->maxRtt.usec;

    KSET_ATOM(nRttSamples);
    KSET_ATOM(nServerConns);
    KSET_ATOM(nClientConns);
    KSET_ATOM(nPeerStructs);
    KSET_ATOM(nCallStructs);
    KSET_ATOM(nFreeCallStructs);
    KSET_ATOM(netSendFailures);
    KSET_ATOM(fatalErrors);
    KSET_ATOM(ignorePacketDally);
    KSET_ATOM(receiveCbufPktAllocFailures);
    KSET_ATOM(sendCbufPktAllocFailures);
    KSET_ATOM(nBusies);
#undef KSET
#undef KSET_ATOM
    return 0;
}

/*
 * Create and install the kstats for this module.
 *
 * /pre AFS_GLOCK held
 */
void
afs_kstat_init(void)
{
    char *thiscell = NULL;
    struct cell *tcell;

    afs_param_ksp = kstat_create("openafs", 0, "param", "misc", KSTAT_TYPE_NAMED,
		       sizeof(param_kstats) / sizeof(kstat_named_t),
		       KSTAT_FLAG_VIRTUAL);

    if (afs_param_ksp == NULL) {
	afs_warn("afs_kstat_init: kstat_create() failed for 'param'.\n");
    } else {
	afs_param_ksp->ks_data = &param_kstats;
	afs_param_ksp->ks_update = afs_param_ks_update,
	kstat_install(afs_param_ksp);
    }

    tcell = afs_GetPrimaryCell(READ_LOCK);
    if (tcell)
	thiscell = tcell->cellName;
    if (thiscell == NULL || strlen(thiscell) == 0)
	thiscell = "-UNKNOWN-";
    strlcpy(cellname, thiscell, sizeof(cellname));
    if (tcell)
	afs_PutCell(tcell, READ_LOCK);

    afs_cache_ksp = kstat_create("openafs", 0, "cache", "misc", KSTAT_TYPE_NAMED,
		       sizeof(cache_kstats) / sizeof(kstat_named_t),
		       KSTAT_FLAG_VIRTUAL);
    if (afs_cache_ksp == NULL) {
	afs_warn("afs_kstat_init: kstat_create() failed for 'cache'.\n");
    } else {
	/* Calculate the size of the KSTAT_DATA_STRINGs. */
	afs_cache_ksp->ks_data_size += strlen(cellname) + 1;
	afs_cache_ksp->ks_data_size += strlen(AFSVersion) + 1;
	afs_cache_ksp->ks_data = &cache_kstats;
	afs_cache_ksp->ks_update = afs_cache_ks_update;
	kstat_install(afs_cache_ksp);
    }

    afs_rx_ksp = kstat_create("openafs", 0, "rx", "misc", KSTAT_TYPE_NAMED,
		       sizeof(rx_kstats) / sizeof(kstat_named_t),
		       KSTAT_FLAG_VIRTUAL);
    if (afs_rx_ksp == NULL) {
	afs_warn("afs_kstat_init: kstat_create() failed for 'rx'.\n");
    } else {
	afs_rx_ksp->ks_data = &rx_kstats;
	afs_rx_ksp->ks_update = afs_rx_ks_update;
	kstat_install(afs_rx_ksp);
    }

    return;
}

/*
 * Remove the kstats.
 */
void
afs_kstat_shutdown(void)
{
    if (afs_param_ksp != NULL) {
	kstat_delete(afs_param_ksp);
	afs_param_ksp = NULL;
    }
    if (afs_cache_ksp != NULL) {
	kstat_delete(afs_cache_ksp);
	afs_cache_ksp = NULL;
    }
    if (afs_rx_ksp != NULL) {
	kstat_delete(afs_rx_ksp);
	afs_rx_ksp = NULL;
    }
}
