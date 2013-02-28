/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Statistics gathering stuff for the AFS cache manager.
 */
/*
 *  The remainder of this file contains the statistics gathering stuff.
 */

#ifndef __OPENAFS_AFS_STATS_H__
#define __OPENAFS_AFS_STATS_H__

#include "afs/param.h"

/* the following is to work around a VAX compiler limitation */
#if defined(vax)
#undef AFS_NOSTATS
#define AFS_NOSTATS
#endif /* VAX environment */

#ifdef AFS_NOSTATS

/*
 * The data collection routines are simply no-ops
 */
#define AFS_STATCNT(arg)
#define AFS_MEANCNT(arg, value)
#define AFS_STATS(arg)
#define XSTATS_DECLS
#define XSTATS_START_TIME(arg)
#define XSTATS_START_CMTIME(arg)
#define XSTATS_END_TIME

#else /* AFS_NOSTATS */

#define AFS_STATS(arg) arg
#ifndef KERNEL
/* NOTE: Ensure this is the same size in user and kernel mode. */
typedef struct {
    afs_int32 tv_sec;
    afs_int32 tv_usec;
} osi_timeval32_t;
#endif /* !KERNEL */

#define XSTATS_DECLS struct afs_stats_opTimingData *opP = NULL; \
    osi_timeval_t opStartTime = { 0, 0}, opStopTime, elapsedTime

#define XSTATS_START_TIME(arg) \
  opP = &(afs_stats_cmfullperf.rpc.fsRPCTimes[arg]); \
  osi_GetuTime(&opStartTime);

#define XSTATS_START_CMTIME(arg) \
  opP = &(afs_stats_cmfullperf.rpc.cmRPCTimes[arg]); \
  osi_GetuTime(&opStartTime);

#define XSTATS_END_TIME osi_GetuTime(&opStopTime); \
  (opP->numOps)++; \
  if (!code) { (opP->numSuccesses)++; \
     afs_stats_GetDiff(elapsedTime, opStartTime, opStopTime); \
     afs_stats_AddTo((opP->sumTime), elapsedTime); \
     afs_stats_SquareAddTo((opP->sqrTime), elapsedTime); \
     if (afs_stats_TimeLessThan(elapsedTime, (opP->minTime))) { \
        afs_stats_TimeAssign((opP->minTime), elapsedTime); \
     } if (afs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) { \
          afs_stats_TimeAssign((opP->maxTime), elapsedTime); } }

#endif /* AFS_NOSTATS */



struct afs_MeanStats {
    afs_int32 average;
    afs_int32 elements;
};

/*
 * struct afs_CMCallStats
 *	This is the place where we keep records on each and every
 *	function call.
 *
 * This structure is encoded as a binary blob and thrown at the client
 * for use by the xstat_cm interface. As the datastructure is unversioned,
 * some rules apply;
 *    *) Never add elements to the middle of the list. Everything new
 *       must go at the end
 *    *) Never remove elements from the list. If a function dies, don't
 *       remove it's entry here (by all means, flag it as dead, though)
 *    *) Never make elements conditional on preprocessor symbols. Doing
 *       this would mean that the client has to be built with exactly
 *       the same options as you are. Which isn't a great idea.
 *
 */
#define AFS_CM_CALL_STATS \
    AFS_CS(afs_init)		/* afs_aix_subr.c */ \
    AFS_CS(gop_rdwr)		/* afs_aix_subr.c */ \
    AFS_CS(aix_gnode_rele)	/* afs_aix_subr.c */ \
    AFS_CS(gettimeofday)	/* afs_aix_subr.c */ \
    AFS_CS(m_cpytoc)		/* afs_aix_subr.c */ \
    AFS_CS(aix_vattr_null)	/* afs_aix_subr.c */ \
    AFS_CS(afs_gn_ftrunc)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_rdwr)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_ioctl)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_lockctl)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_readlink)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_readdir)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_select)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_strategy)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_symlink)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_revoke)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_link)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_mkdir)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_mknod)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_remove)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_rename)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_rmdir)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_fid)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_lookup)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_open)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_create)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_hold)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_close)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_map)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_rele)		/* afs_aixops.c */ \
    AFS_CS(afs_gn_unmap)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_access)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_getattr)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_setattr)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_fclear)	/* afs_aixops.c */ \
    AFS_CS(afs_gn_fsync)	/* afs_aixops.c */ \
    AFS_CS(pHash)		/* afs_buffer.c */ \
    AFS_CS(DInit)		/* afs_buffer.c */ \
    AFS_CS(DRead)		/* afs_buffer.c */ \
    AFS_CS(FixupBucket)		/* afs_buffer.c */ \
    AFS_CS(afs_newslot)		/* afs_buffer.c */ \
    AFS_CS(DRelease)		/* afs_buffer.c */ \
    AFS_CS(DFlush)		/* afs_buffer.c */ \
    AFS_CS(DFlushEntry)		/* afs_buffer.c */ \
    AFS_CS(DVOffset)		/* afs_buffer.c */ \
    AFS_CS(DZap)		/* afs_buffer.c */ \
    AFS_CS(DNew)		/* afs_buffer.c */ \
    AFS_CS(shutdown_bufferpackage)	/* afs_buffer.c */ \
    AFS_CS(afs_CheckKnownBad)	/* afs_cache.c */ \
    AFS_CS(afs_RemoveVCB)	/* afs_cache.c */ \
    AFS_CS(afs_NewVCache)	/* afs_cache.c */ \
    AFS_CS(afs_FlushActiveVcaches)	/* afs_cache.c */ \
    AFS_CS(afs_VerifyVCache)	/* afs_cache.c */ \
    AFS_CS(afs_WriteVCache)	/* afs_cache.c */ \
    AFS_CS(afs_GetVCache)	/* afs_cache.c */ \
    AFS_CS(afs_StuffVcache)	/* afs_cache.c */ \
    AFS_CS(afs_FindVCache)	/* afs_cache.c */ \
    AFS_CS(afs_PutDCache)	/* afs_cache.c */ \
    AFS_CS(afs_PutVCache)	/* afs_cache.c */ \
    AFS_CS(CacheStoreProc)	/* afs_cache.c */ \
    AFS_CS(afs_FindDCache)	/* afs_cache.c */ \
    AFS_CS(afs_TryToSmush)	/* afs_cache.c */ \
    AFS_CS(afs_AdjustSize)	/* afs_cache.c */ \
    AFS_CS(afs_CheckSize)	/* afs_cache.c */ \
    AFS_CS(afs_StoreWarn)	/* afs_cache.c */ \
    AFS_CS(CacheFetchProc)	/* afs_cache.c */ \
    AFS_CS(UFS_CacheStoreProc)	/* afs_cache.c */ \
    AFS_CS(UFS_CacheFetchProc)	/* afs_cache.c */ \
    AFS_CS(afs_GetDCache)	/* afs_cache.c */ \
    AFS_CS(afs_SimpleVStat)	/* afs_cache.c */ \
    AFS_CS(afs_ProcessFS)	/* afs_cache.c */ \
    AFS_CS(afs_InitCacheInfo)	/* afs_cache.c */ \
    AFS_CS(afs_InitVolumeInfo)	/* afs_cache.c */ \
    AFS_CS(afs_InitCacheFile)	/* afs_cache.c */ \
    AFS_CS(afs_CacheInit)	/* afs_cache.c */ \
    AFS_CS(afs_GetDSlot)	/* afs_cache.c */ \
    AFS_CS(afs_WriteThroughDSlots)	/* afs_cache.c */ \
    AFS_CS(afs_MemGetDSlot)	/* afs_cache.c */ \
    AFS_CS(afs_UFSGetDSlot)	/* afs_cache.c */ \
    AFS_CS(afs_StoreDCache)	/* afs_cache.c */ \
    AFS_CS(afs_StoreMini)	/* afs_cache.c */ \
    AFS_CS(shutdown_cache)	/* afs_cache.c */ \
    AFS_CS(afs_StoreAllSegments)	/* afs_cache.c */ \
    AFS_CS(afs_InvalidateAllSegments)	/* afs_cache.c */ \
    AFS_CS(afs_TruncateAllSegments)	/* afs_cache.c */ \
    AFS_CS(afs_CheckVolSync)	/* afs_cache.c */ \
    AFS_CS(afs_wakeup)		/* afs_cache.c */ \
    AFS_CS(afs_CFileOpen)	/* afs_cache.c */ \
    AFS_CS(afs_CFileTruncate)	/* afs_cache.c */ \
    AFS_CS(afs_GetDownD)	/* afs_cache.c */ \
    AFS_CS(afs_WriteDCache)	/* afs_cache.c */ \
    AFS_CS(afs_FlushDCache)	/* afs_cache.c */ \
    AFS_CS(afs_GetDownDSlot)	/* afs_cache.c */ \
    AFS_CS(afs_FlushVCache)	/* afs_cache.c */ \
    AFS_CS(afs_GetDownV)	/* afs_cache.c */ \
    AFS_CS(afs_QueueVCB)	/* afs_cache.c */ \
    AFS_CS(afs_call)		/* afs_call.c */ \
    AFS_CS(afs_syscall_call)	/* afs_call.c */ \
    AFS_CS(syscall)		/* afs_call.c */ \
    AFS_CS(lpioctl)		/* afs_call.c */ \
    AFS_CS(lsetpag)		/* afs_call.c */ \
    AFS_CS(afs_syscall)		/* afs_call.c */ \
    AFS_CS(afs_CheckInit)	/* afs_call.c */ \
    AFS_CS(afs_shutdown)	/* afs_call.c */ \
    AFS_CS(shutdown_BKG)	/* afs_call.c */ \
    AFS_CS(shutdown_afstest)	/* afs_call.c */ \
    AFS_CS(SRXAFSCB_GetCE)	/* afs_callback.c */ \
    AFS_CS(ClearCallBack)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetLock)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_CallBack)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_InitCallBackState)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_Probe)	/* afs_callback.c */ \
    AFS_CS(afs_RXCallBackServer)	/* afs_callback.c */ \
    AFS_CS(shutdown_CB)		/* afs_callback.c */ \
    AFS_CS(afs_Chunk)		/* afs_chunk.c */ \
    AFS_CS(afs_ChunkBase)	/* afs_chunk.c */ \
    AFS_CS(afs_ChunkOffset)	/* afs_chunk.c */ \
    AFS_CS(afs_ChunkSize)	/* afs_chunk.c */ \
    AFS_CS(afs_ChunkToBase)	/* afs_chunk.c */ \
    AFS_CS(afs_ChunkToSize)	/* afs_chunk.c */ \
    AFS_CS(afs_SetChunkSize)	/* afs_chunk.c */ \
    AFS_CS(afs_config)		/* afs_config.c */ \
    AFS_CS(mem_freebytes)	/* afs_config.c */ \
    AFS_CS(mem_getbytes)	/* afs_config.c */ \
    AFS_CS(fpalloc)		/* afs_config.c */ \
    AFS_CS(kluge_init)		/* afs_config.c */ \
    AFS_CS(ufdalloc)		/* afs_config.c */ \
    AFS_CS(ufdfree)		/* afs_config.c */ \
    AFS_CS(commit)		/* afs_config.c */ \
    AFS_CS(dev_ialloc)		/* afs_config.c */ \
    AFS_CS(ffree)		/* afs_config.c */ \
    AFS_CS(iget)		/* afs_config.c */ \
    AFS_CS(iptovp)		/* afs_config.c */ \
    AFS_CS(ilock)		/* afs_config.c */ \
    AFS_CS(irele)		/* afs_config.c */ \
    AFS_CS(iput)		/* afs_config.c */ \
    AFS_CS(afs_Daemon)		/* afs_daemons.c */ \
    AFS_CS(afs_CheckRootVolume)	/* afs_daemons.c */ \
    AFS_CS(BPath)		/* afs_daemons.c */ \
    AFS_CS(BPrefetch)		/* afs_daemons.c */ \
    AFS_CS(BStore)		/* afs_daemons.c */ \
    AFS_CS(afs_BBusy)		/* afs_daemons.c */ \
    AFS_CS(afs_BQueue)		/* afs_daemons.c */ \
    AFS_CS(afs_BRelease)	/* afs_daemons.c */ \
    AFS_CS(afs_BackgroundDaemon)	/* afs_daemons.c */ \
    AFS_CS(shutdown_daemons)	/* afs_daemons.c */ \
    AFS_CS(exporter_add)	/* afs_exporter.c */ \
    AFS_CS(exporter_find)	/* afs_exporter.c */ \
    AFS_CS(afs_gfs_kalloc)	/* afs_gfs_subr.c */ \
    AFS_CS(IsAfsVnode)		/* afs_gfs_subr.c */ \
    AFS_CS(SetAfsVnode)		/* afs_gfs_subr.c */ \
    AFS_CS(afs_gfs_kfree)	/* afs_gfs_subr.c */ \
    AFS_CS(gop_lookupname)	/* afs_gfs_subr.c */ \
    AFS_CS(gfsvop_getattr)	/* afs_gfs_subr.c */ \
    AFS_CS(gfsvop_rdwr)		/* afs_gfs_subr.c */ \
    AFS_CS(afs_uniqtime)	/* afs_gfs_subr.c */ \
    AFS_CS(gfs_vattr_null)	/* afs_gfs_subr.c */ \
    AFS_CS(afs_lock)		/* afs_gfsops.c */ \
    AFS_CS(afs_unlock)		/* afs_gfsops.c */ \
    AFS_CS(afs_update)		/* afs_gfsops.c */ \
    AFS_CS(afs_gclose)		/* afs_gfsops.c */ \
    AFS_CS(afs_gopen)		/* afs_gfsops.c */ \
    AFS_CS(afs_greadlink)	/* afs_gfsops.c */ \
    AFS_CS(afs_select)		/* afs_gfsops.c */ \
    AFS_CS(afs_gbmap)		/* afs_gfsops.c */ \
    AFS_CS(afs_getfsdata)	/* afs_gfsops.c */ \
    AFS_CS(afs_gsymlink)	/* afs_gfsops.c */ \
    AFS_CS(afs_namei)		/* afs_gfsops.c */ \
    AFS_CS(printgnode)		/* afs_gfsops.c */ \
    AFS_CS(HaveGFSLock)		/* afs_gfsops.c */ \
    AFS_CS(afs_gmount)		/* afs_gfsops.c */ \
    AFS_CS(AddGFSLock)		/* afs_gfsops.c */ \
    AFS_CS(RemoveGFSLock)	/* afs_gfsops.c */ \
    AFS_CS(afs_grlock)		/* afs_gfsops.c */ \
    AFS_CS(afs_gumount)		/* afs_gfsops.c */ \
    AFS_CS(afs_gget)		/* afs_gfsops.c */ \
    AFS_CS(afs_glink)		/* afs_gfsops.c */ \
    AFS_CS(afs_gmkdir)		/* afs_gfsops.c */ \
    AFS_CS(afs_sbupdate)	/* afs_gfsops.c */ \
    AFS_CS(afs_unlink)		/* afs_gfsops.c */ \
    AFS_CS(afs_grmdir)		/* afs_gfsops.c */ \
    AFS_CS(afs_makenode)	/* afs_gfsops.c */ \
    AFS_CS(afs_grename)		/* afs_gfsops.c */ \
    AFS_CS(afs_rele)		/* afs_gfsops.c */ \
    AFS_CS(afs_syncgp)		/* afs_gfsops.c */ \
    AFS_CS(afs_getval)		/* afs_gfsops.c */ \
    AFS_CS(afs_gfshack)		/* afs_gfsops.c */ \
    AFS_CS(afs_trunc)		/* afs_gfsops.c */ \
    AFS_CS(afs_rwgp)		/* afs_gfsops.c */ \
    AFS_CS(afs_stat)		/* afs_gfsops.c */ \
    AFS_CS(afsc_link)		/* afs_hp_subr.c */ \
    AFS_CS(hpsobind)		/* afs_hp_subr.c */ \
    AFS_CS(hpsoclose)		/* afs_hp_subr.c */ \
    AFS_CS(hpsocreate)		/* afs_hp_subr.c */ \
    AFS_CS(hpsoreserve)		/* afs_hp_subr.c */ \
    AFS_CS(afs_vfs_mount)	/* afs_hp_subr.c */ \
    AFS_CS(devtovfs)		/* afs_istuff.c */ \
    AFS_CS(igetinode)		/* afs_istuff.c */ \
    AFS_CS(afs_syscall_iopen)	/* afs_istuff.c */ \
    AFS_CS(iopen)		/* afs_istuff.c */ \
    AFS_CS(afs_syscall_iincdec)	/* afs_istuff.c */ \
    AFS_CS(afs_syscall_ireadwrite)	/* afs_istuff.c */ \
    AFS_CS(iincdec)		/* afs_istuff.c */ \
    AFS_CS(ireadwrite)		/* afs_istuff.c */ \
    AFS_CS(oiread)		/* afs_istuff.c */ \
    AFS_CS(AHash)		/* afs_istuff.c */ \
    AFS_CS(QTOA)		/* afs_istuff.c */ \
    AFS_CS(afs_FindPartByDev)	/* afs_istuff.c */ \
    AFS_CS(aux_init)		/* afs_istuff.c */ \
    AFS_CS(afs_GetNewPart)	/* afs_istuff.c */ \
    AFS_CS(afs_InitAuxVolFile)	/* afs_istuff.c */ \
    AFS_CS(afs_CreateAuxEntry)	/* afs_istuff.c */ \
    AFS_CS(afs_GetAuxSlot)	/* afs_istuff.c */ \
    AFS_CS(afs_GetDownAux)	/* afs_istuff.c */ \
    AFS_CS(afs_FlushAuxCache)	/* afs_istuff.c */ \
    AFS_CS(afs_GetAuxInode)	/* afs_istuff.c */ \
    AFS_CS(afs_PutAuxInode)	/* afs_istuff.c */ \
    AFS_CS(afs_ReadAuxInode)	/* afs_istuff.c */ \
    AFS_CS(afs_WriteAuxInode)	/* afs_istuff.c */ \
    AFS_CS(afs_auxcall)		/* afs_istuff.c */ \
    AFS_CS(tmpdbg_auxtbl)	/* afs_istuff.c */ \
    AFS_CS(tmpdbg_parttbl)	/* afs_istuff.c */ \
    AFS_CS(idec)		/* afs_istuff.c */ \
    AFS_CS(iinc)		/* afs_istuff.c */ \
    AFS_CS(iread)		/* afs_istuff.c */ \
    AFS_CS(iwrite)		/* afs_istuff.c */ \
    AFS_CS(getinode)		/* afs_istuff.c */ \
    AFS_CS(trygetfs)		/* afs_istuff.c */ \
    AFS_CS(iforget)		/* afs_istuff.c */ \
    AFS_CS(afs_syscall_icreate)	/* afs_istuff.c */ \
    AFS_CS(icreate)		/* afs_istuff.c */ \
    AFS_CS(Lock_Init)		/* afs_lock.c */ \
    AFS_CS(Lock_Obtain)		/* afs_lock.c */ \
    AFS_CS(Lock_ReleaseR)	/* afs_lock.c */ \
    AFS_CS(Lock_ReleaseW)	/* afs_lock.c */ \
    AFS_CS(afs_BozonLock)	/* afs_lock.c */ \
    AFS_CS(afs_BozonUnlock)	/* afs_lock.c */ \
    AFS_CS(osi_SleepR)		/* afs_lock.c */ \
    AFS_CS(osi_SleepS)		/* afs_lock.c */ \
    AFS_CS(osi_SleepW)		/* afs_lock.c */ \
    AFS_CS(osi_Sleep)		/* afs_lock */ \
    AFS_CS(afs_BozonInit)	/* afs_lock.c */ \
    AFS_CS(afs_CheckBozonLock)	/* afs_lock.c */ \
    AFS_CS(afs_CheckBozonLockBlocking)	/* afs_lock.c */ \
    AFS_CS(xxxinit)		/* afs_main.c */ \
    AFS_CS(KernelEntry)		/* afs_main.c */ \
    AFS_CS(afs_InitMemCache)	/* afs_memcache.c */ \
    AFS_CS(afs_LookupMCE)	/* afs_memcache.c */ \
    AFS_CS(afs_MemReadBlk)	/* afs_memcache.c */ \
    AFS_CS(afs_MemReadUIO)	/* afs_memcache.c */ \
    AFS_CS(afs_MemWriteBlk)	/* afs_memcache.c */ \
    AFS_CS(afs_MemCacheStoreProc)	/* afs_memcache.c */ \
    AFS_CS(afs_MemCacheTruncate)	/* afs_memcache.c */ \
    AFS_CS(afs_MemWriteUIO)	/* afs_memcache.c */ \
    AFS_CS(afs_MemCacheFetchProc)	/* afs_memcache.c */ \
    AFS_CS(afs_vnode_pager_create)	/* afs_next_aux.c */ \
    AFS_CS(next_KernelEntry)	/* afs_next_subr.c */ \
    AFS_CS(afs_GetNfsClientPag)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_FindNfsClientPag)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_PutNfsClientPag)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_reqhandler)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_GC)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_hold)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_stats)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_sysname)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_nfsclient_shutdown)	/* afs_nfsclnt.c */ \
    AFS_CS(afs_rfs_readdir_fixup)	/* afs_nfssrv.c */ \
    AFS_CS(afs_rfs_dispatch)	/* afs_nfssrv.c */ \
    AFS_CS(afs_xnfs_svc)	/* afs_nfssrv.c */ \
    AFS_CS(afs_xdr_putrddirres)	/* afs_nfssrv.c */ \
    AFS_CS(afs_rfs_readdir)	/* afs_nfssrv.c */ \
    AFS_CS(afs_rfs_rddirfree)	/* afs_nfssrv.c */ \
    AFS_CS(rfs_dupcreate)	/* afs_nfssrv.c */ \
    AFS_CS(rfs_dupsetattr)	/* afs_nfssrv.c */ \
    AFS_CS(Nfs2AfsCall)		/* afs_nfssrv.c */ \
    AFS_CS(afs_sun_xuntext)	/* afs_osi.c */ \
    AFS_CS(osi_Active)		/* afs_osi.c */ \
    AFS_CS(osi_FlushPages)	/* afs_osi.c */ \
    AFS_CS(osi_FlushText)	/* afs_osi.c */ \
    AFS_CS(osi_CallProc)	/* afs_osi.c */ \
    AFS_CS(osi_CancelProc)	/* afs_osi.c */ \
    AFS_CS(osi_Invisible)	/* afs_osi.c */ \
    AFS_CS(osi_Time)		/* afs_osi.c */ \
    AFS_CS(osi_Alloc)		/* afs_osi.c */ \
    AFS_CS(osi_SetTime)		/* afs_osi.c */ \
    AFS_CS(osi_Dump)		/* afs_osi.c */ \
    AFS_CS(osi_Free)		/* afs_osi.c */ \
    AFS_CS(shutdown_osi)	/* afs_osi.c */ \
    AFS_CS(osi_UFSOpen)		/* afs_osifile.c */ \
    AFS_CS(osi_Close)		/* afs_osifile.c */ \
    AFS_CS(osi_Stat)		/* afs_osifile.c */ \
    AFS_CS(osi_Truncate)	/* afs_osifile.c */ \
    AFS_CS(osi_Read)		/* afs_osifile.c */ \
    AFS_CS(osi_Write)		/* afs_osifile.c */ \
    AFS_CS(osi_MapStrategy)	/* afs_osifile.c */ \
    AFS_CS(shutdown_osifile)	/* afs_osifile.c */ \
    AFS_CS(osi_FreeLargeSpace)	/* afs_osinet.c */ \
    AFS_CS(osi_FreeSmallSpace)	/* afs_osinet.c */ \
    AFS_CS(pkt_iodone)		/* afs_osinet.c */ \
    AFS_CS(shutdown_osinet)	/* afs_osinet.c */ \
    AFS_CS(afs_cs)		/* afs_osinet.c */ \
    AFS_CS(osi_AllocLargeSpace)	/* afs_osinet.c */ \
    AFS_CS(osi_AllocSmallSpace)	/* afs_osinet.c */ \
    AFS_CS(osi_CloseToTheEdge)	/* afs_osinet.c */ \
    AFS_CS(osi_xgreedy)		/* afs_osinet.c */ \
    AFS_CS(osi_FreeSocket)	/* afs_osinet.c */ \
    AFS_CS(osi_NewSocket)	/* afs_osinet.c */ \
    AFS_CS(trysblock)		/* afs_osinet.c */ \
    AFS_CS(osi_NetSend)		/* afs_osinet.c */ \
    AFS_CS(WaitHack)		/* afs_osinet.c */ \
    AFS_CS(osi_CancelWait)	/* afs_osinet.c */ \
    AFS_CS(osi_InitWaitHandle)	/* afs_osinet.c */ \
    AFS_CS(osi_Wakeup)		/* afs_osinet.c */ \
    AFS_CS(osi_Wait)		/* afs_osinet.c */ \
    AFS_CS(dirp_Read)		/* afs_physio.c */ \
    AFS_CS(dirp_SetCacheDev)	/* afs_physio.c */ \
    AFS_CS(Die)			/* afs_physio.c */ \
    AFS_CS(dirp_Cpy)		/* afs_physio.c */ \
    AFS_CS(dirp_Eq)		/* afs_physio.c */ \
    AFS_CS(dirp_Write)		/* afs_physio.c */ \
    AFS_CS(dirp_Zap)		/* afs_physio.c */ \
    AFS_CS(PSetVolumeStatus)	/* afs_pioctl.c */ \
    AFS_CS(PFlush)		/* afs_pioctl.c */ \
    AFS_CS(PNewStatMount)	/* afs_pioctl.c */ \
    AFS_CS(PGetTokens)		/* afs_pioctl.c */ \
    AFS_CS(PUnlog)		/* afs_pioctl.c */ \
    AFS_CS(PCheckServers)	/* afs_pioctl.c */ \
    AFS_CS(PMariner)		/* afs_pioctl.c */ \
    AFS_CS(PCheckAuth)		/* afs_pioctl.c */ \
    AFS_CS(PCheckVolNames)	/* afs_pioctl.c */ \
    AFS_CS(PFindVolume)		/* afs_pioctl.c */ \
    AFS_CS(Prefetch)		/* afs_pioctl.c */ \
    AFS_CS(PGetCacheSize)	/* afs_pioctl.c */ \
    AFS_CS(PRemoveCallBack)	/* afs_pioctl.c */ \
    AFS_CS(PSetCacheSize)	/* afs_pioctl.c */ \
    AFS_CS(PViceAccess)		/* afs_pioctl.c */ \
    AFS_CS(PListCells)		/* afs_pioctl.c */ \
    AFS_CS(PNewCell)		/* afs_pioctl.c */ \
    AFS_CS(PRemoveMount)	/* afs_pioctl.c */ \
    AFS_CS(HandleIoctl)		/* afs_pioctl.c */ \
    AFS_CS(_AFSIOCTL)		/* afs_pioctl.c */ \
    AFS_CS(_VALIDAFSIOCTL)	/* afs_pioctl.c */ \
    AFS_CS(PGetCellStatus)	/* afs_pioctl.c */ \
    AFS_CS(PSetCellStatus)	/* afs_pioctl.c */ \
    AFS_CS(PVenusLogging)	/* afs_pioctl.c */ \
    AFS_CS(PFlushVolumeData)	/* afs_pioctl.c */ \
    AFS_CS(PSetSysName)		/* afs_pioctl.c */ \
    AFS_CS(PExportAfs)		/* afs_pioctl.c */ \
    AFS_CS(HandleClientContext)	/* afs_pioctl.c */ \
    AFS_CS(afs_ioctl)		/* afs_pioctl.c */ \
    AFS_CS(afs_xioctl)		/* afs_pioctl.c */ \
    AFS_CS(afs_pioctl)		/* afs_pioctl.c */ \
    AFS_CS(afs_syscall_pioctl)	/* afs_pioctl.c */ \
    AFS_CS(HandlePioctl)	/* afs_pioctl.c */ \
    AFS_CS(PGetAcl)		/* afs_pioctl.c */ \
    AFS_CS(PGetFID)		/* afs_pioctl.c */ \
    AFS_CS(PSetAcl)		/* afs_pioctl.c */ \
    AFS_CS(PBogus)		/* afs_pioctl.c */ \
    AFS_CS(PGetFileCell)	/* afs_pioctl.c */ \
    AFS_CS(PGetWSCell)		/* afs_pioctl.c */ \
    AFS_CS(PNoop)		/* afs_pioctl.c */ \
    AFS_CS(PGetUserCell)	/* afs_pioctl.c */ \
    AFS_CS(PSetTokens)		/* afs_pioctl.c */ \
    AFS_CS(PGetVolumeStatus)	/* afs_pioctl.c */ \
    AFS_CS(afs_ResetAccessCache)	/* afs_resource.c */ \
    AFS_CS(afs_FindUser)	/* afs_resource.c */ \
    AFS_CS(afs_ResetUserConns)	/* afs_resource.c */ \
    AFS_CS(afs_ResourceInit)	/* afs_resource.c */ \
    AFS_CS(afs_GetCell)		/* afs_resource.c */ \
    AFS_CS(afs_GetCellByIndex)	/* afs_resource.c */ \
    AFS_CS(afs_GetCellByName)	/* afs_resource.c */ \
    AFS_CS(afs_GetRealCellByIndex)	/* afs_resource.c */ \
    AFS_CS(afs_NewCell)		/* afs_resource.c */ \
    AFS_CS(afs_GetUser)		/* afs_resource.c */ \
    AFS_CS(afs_PutUser)		/* afs_resource.c */ \
    AFS_CS(afs_SetPrimary)	/* afs_resource.c */ \
    AFS_CS(CheckVLDB)		/* afs_resource.c */ \
    AFS_CS(afs_GetVolume)	/* afs_resource.c */ \
    AFS_CS(afs_GetVolumeByName)	/* afs_resource.c */ \
    AFS_CS(InstallVolumeEntry)	/* afs_resource.c */ \
    AFS_CS(InstallVolumeInfo)	/* afs_resource.c */ \
    AFS_CS(afs_FindServer)	/* afs_resource.c */ \
    AFS_CS(afs_PutVolume)	/* afs_resource.c */ \
    AFS_CS(afs_random)		/* afs_resource.c */ \
    AFS_CS(ranstage)		/* afs_resource.c */ \
    AFS_CS(RemoveUserConns)	/* afs_resource.c */ \
    AFS_CS(afs_MarinerLog)	/* afs_resource.c */ \
    AFS_CS(afs_vtoi)		/* afs_resource.c */ \
    AFS_CS(afs_GetServer)	/* afs_resource.c */ \
    AFS_CS(afs_SortServers)	/* afs_resource.c */ \
    AFS_CS(afs_Conn)		/* afs_resource.c */ \
    AFS_CS(afs_ConnByHost)	/* afs_resource.c */ \
    AFS_CS(afs_ConnByMHosts)	/* afs_resource.c */ \
    AFS_CS(afs_Analyze)		/* afs_resource.c */ \
    AFS_CS(afs_PutConn)		/* afs_resource.c */ \
    AFS_CS(afs_ResetVolumeInfo)	/* afs_resource.c */ \
    AFS_CS(StartLogFile)	/* afs_resource.c */ \
    AFS_CS(afs_SetLogFile)	/* afs_resource.c */ \
    AFS_CS(EndLogFile)		/* afs_resource.c */ \
    AFS_CS(afs_dp)		/* afs_resource.c */ \
    AFS_CS(fprf)		/* afs_resource.c */ \
    AFS_CS(fprint)		/* afs_resource.c */ \
    AFS_CS(fprintn)		/* afs_resource.c */ \
    AFS_CS(afs_CheckLocks)	/* afs_resource.c */ \
    AFS_CS(puttofile)		/* afs_resource.c */ \
    AFS_CS(shutdown_AFS)	/* afs_resource.c */ \
    AFS_CS(afs_CheckCacheResets)	/* afs_resource.c */ \
    AFS_CS(afs_GCUserData)	/* afs_resource.c */ \
    AFS_CS(VSleep)		/* afs_resource.c */ \
    AFS_CS(afs_CheckCode)	/* afs_resource.c */ \
    AFS_CS(afs_CopyError)	/* afs_resource.c */ \
    AFS_CS(afs_FinalizeReq)	/* afs_resource.c */ \
    AFS_CS(afs_cv2string)	/* afs_resource.c */ \
    AFS_CS(afs_FindVolCache)	/* afs_resource.c */ \
    AFS_CS(afs_GetVolCache)	/* afs_resource.c */ \
    AFS_CS(afs_GetVolSlot)	/* afs_resource.c */ \
    AFS_CS(afs_WriteVolCache)	/* afs_resource.c */ \
    AFS_CS(afs_UFSGetVolSlot)	/* afs_resource.c */ \
    AFS_CS(afs_CheckVolumeNames)	/* afs_resource.c */ \
    AFS_CS(afs_MemGetVolSlot)	/* afs_resource.c */ \
    AFS_CS(print_internet_address)	/* afs_resource.c */ \
    AFS_CS(CheckVLServer)	/* afs_resource.c */ \
    AFS_CS(HaveCallBacksFrom)	/* afs_resource.c */ \
    AFS_CS(ServerDown)		/* afs_resource.c */ \
    AFS_CS(afs_CheckServers)	/* afs_resource.c */ \
    AFS_CS(afs_AddToMean)	/* afs_stat.c */ \
    AFS_CS(afs_GetCMStat)	/* afs_stat.c */ \
    AFS_CS(afs_getpage)		/* afs_sun_subr.c */ \
    AFS_CS(afs_putpage)		/* afs_sun_subr.c */ \
    AFS_CS(afs_nfsrdwr)		/* afs_sun_subr.c */ \
    AFS_CS(afs_map)		/* afs_sun_subr.c */ \
    AFS_CS(afs_cmp)		/* afs_sun_subr.c */ \
    AFS_CS(afs_cntl)		/* afs_sun_subr.c */ \
    AFS_CS(afs_dump)		/* afs_sun_subr.c */ \
    AFS_CS(afs_realvp)		/* afs_sun_subr.c */ \
    AFS_CS(afs_PageLeft)	/* afs_sun_subr.c */ \
    AFS_CS(afsinit)		/* afs_vfsops.c */ \
    AFS_CS(afs_mount)		/* afs_vfsops.c */ \
    AFS_CS(afs_unmount)		/* afs_vfsops.c */ \
    AFS_CS(afs_root)		/* afs_vfsops.c */ \
    AFS_CS(afs_statfs)		/* afs_vfsops.c */ \
    AFS_CS(afs_sync)		/* afs_vfsops.c */ \
    AFS_CS(afs_vget)		/* afs_vfsops.c */ \
    AFS_CS(afs_mountroot)	/* afs_vfsops.c */ \
    AFS_CS(afs_swapvp)		/* afs_vfsops.c */ \
    AFS_CS(afs_AddMarinerName)	/* afs_vnodeops.c */ \
    AFS_CS(afs_setpag)		/* afs_vnodeops.c */ \
    AFS_CS(genpag)		/* afs_vnodeops.c */ \
    AFS_CS(getpag)		/* afs_vnodeops.c */ \
    AFS_CS(afs_GetMariner)	/* afs_vnodeops.c */ \
    AFS_CS(afs_badop)		/* afs_vnodeops.c */ \
    AFS_CS(afs_index)		/* afs_vnodeops.c */ \
    AFS_CS(afs_noop)		/* afs_vnodeops.c */ \
    AFS_CS(afs_open)		/* afs_vnodeops.c */ \
    AFS_CS(afs_closex)		/* afs_vnodeops.c */ \
    AFS_CS(afs_close)		/* afs_vnodeops.c */ \
    AFS_CS(afs_MemWrite)	/* afs_vnodeops.c */ \
    AFS_CS(afs_write)		/* afs_vnodeops.c */ \
    AFS_CS(afs_UFSWrite)	/* afs_vnodeops.c */ \
    AFS_CS(afs_rdwr)		/* afs_vnodeops.c */ \
    AFS_CS(afs_MemRead)		/* afs_vnodeops.c */ \
    AFS_CS(afs_read)		/* afs_vnodeops.c */ \
    AFS_CS(FIXUPSTUPIDINODE)	/* afs_vnodeops.c */ \
    AFS_CS(afs_UFSRead)		/* afs_vnodeops.c */ \
    AFS_CS(afs_CopyOutAttrs)	/* afs_vnodeops.c */ \
    AFS_CS(afs_getattr)		/* afs_vnodeops.c */ \
    AFS_CS(afs_VAttrToAS)	/* afs_vnodeops.c */ \
    AFS_CS(afs_setattr)		/* afs_vnodeops.c */ \
    AFS_CS(EvalMountPoint)	/* afs_vnodeops.c */ \
    AFS_CS(afs_access)		/* afs_vnodeops.c */ \
    AFS_CS(ENameOK)		/* afs_vnodeops.c */ \
    AFS_CS(HandleAtName)	/* afs_vnodeops.c */ \
    AFS_CS(getsysname)		/* afs_vnodeops.c */ \
    AFS_CS(strcat)		/* afs_vnodeops.c */ \
    AFS_CS(afs_lookup)		/* afs_vnodeops.c */ \
    AFS_CS(afs_create)		/* afs_vnodeops.c */ \
    AFS_CS(afs_LocalHero)	/* afs_vnodeops.c */ \
    AFS_CS(FetchWholeEnchilada)	/* afs_vnodeops.c */ \
    AFS_CS(afs_remove)		/* afs_vnodeops.c */ \
    AFS_CS(afs_link)		/* afs_vnodeops.c */ \
    AFS_CS(afs_rename)		/* afs_vnodeops.c */ \
    AFS_CS(afs_InitReq)		/* afs_vnodeops.c */ \
    AFS_CS(afs_mkdir)		/* afs_vnodeops.c */ \
    AFS_CS(BlobScan)		/* afs_vnodeops.c */ \
    AFS_CS(afs_rmdir)		/* afs_vnodeops.c */ \
    AFS_CS(RecLen)		/* afs_vnodeops.c */ \
    AFS_CS(RoundToInt)		/* afs_vnodeops.c */ \
    AFS_CS(afs_readdir_with_offlist)	/* afs_vnodeops.c */ \
    AFS_CS(DIRSIZ_LEN)		/* afs_vnodeops.c */ \
    AFS_CS(afs_readdir_move)	/* afs_vnodeops.c */ \
    AFS_CS(afs_readdir_iter)	/* afs_vnodeops.c */ \
    AFS_CS(HandleFlock)		/* afs_vnodeops.c */ \
    AFS_CS(afs_readdir)		/* afs_vnodeops.c */ \
    AFS_CS(afs_symlink)		/* afs_vnodeops.c */ \
    AFS_CS(afs_HandleLink)	/* afs_vnodeops.c */ \
    AFS_CS(afs_MemHandleLink)	/* afs_vnodeops.c */ \
    AFS_CS(afs_UFSHandleLink)	/* afs_vnodeops.c */ \
    AFS_CS(afs_readlink)	/* afs_vnodeops.c */ \
    AFS_CS(afs_fsync)		/* afs_vnodeops.c */ \
    AFS_CS(afs_inactive)	/* afs_vnodeops.c */ \
    AFS_CS(afs_ustrategy)	/* afs_vnodeops.c */ \
    AFS_CS(afs_bread)		/* afs_vnodeops.c */ \
    AFS_CS(afs_brelse)		/* afs_vnodeops.c */ \
    AFS_CS(afs_bmap)		/* afs_vnodeops.c */ \
    AFS_CS(afs_fid)		/* afs_vnodeops.c */ \
    AFS_CS(afs_strategy)	/* afs_vnodeops.c */ \
    AFS_CS(afs_FakeClose)	/* afs_vnodeops.c */ \
    AFS_CS(afs_FakeOpen)	/* afs_vnodeops.c */ \
    AFS_CS(afs_StoreOnLastReference)	/* afs_vnodeops.c */ \
    AFS_CS(afs_GetAccessBits)	/* afs_vnodeops.c */ \
    AFS_CS(afs_AccessOK)	/* afs_vnodeops.c */ \
    AFS_CS(shutdown_vnodeops)	/* afs_vnodeops.c */ \
    AFS_CS(afsio_copy)		/* afs_vnodeops.c */ \
    AFS_CS(afsio_trim)		/* afs_vnodeops.c */ \
    AFS_CS(afs_page_read)	/* afs_vnodeops.c */ \
    AFS_CS(afs_page_write)	/* afs_vnodeops.c */ \
    AFS_CS(afsio_skip)		/* afs_vnodeops.c */ \
    AFS_CS(afs_read1dir)	/* afs_vnodeops.c */ \
    AFS_CS(afs_get_groups_from_pag)	/* afs_vnodeops.c */ \
    AFS_CS(afs_get_pag_from_groups)	/* afs_vnodeops.c */ \
    AFS_CS(PagInCred)		/* afs_vnodeops.c */ \
    AFS_CS(afs_getgroups)	/* afs_vnodeops.c */ \
    AFS_CS(setpag)		/* afs_vnodeops.c */ \
    AFS_CS(afs_setgroups)	/* afs_vnodeops.c */ \
    AFS_CS(afs_page_in)		/* afs_vnodeops.c */ \
    AFS_CS(afs_page_out)	/* afs_vnodeops.c */ \
    AFS_CS(AddPag)		/* afs_vnodeops.c */ \
    AFS_CS(afs_AdvanceFD)	/* afs_vnodeops.c */ \
    AFS_CS(afs_lockf)		/* afs_vnodeops.c */ \
    AFS_CS(afs_xsetgroups)	/* afs_vnodeops.c */ \
    AFS_CS(afs_nlinks)		/* afs_vnodeops.c */ \
    AFS_CS(DoLockWarning)	/* afs_vnodeops.c */ \
    AFS_CS(afs_lockctl)		/* afs_vnodeops.c */ \
    AFS_CS(afs_xflock)		/* afs_vnodeops.c */ \
    AFS_CS(PSetSPrefs)		/* afs_pioctl.c */ \
    AFS_CS(PGetSPrefs)		/* afs_pioctl.c */ \
    AFS_CS(afs_warn)		/* afs_resource.c */ \
    AFS_CS(afs_warnuser)	/* afs_resource.c */ \
    AFS_CS(afs_pagein)		/* afs_hp_subr.c */ \
    AFS_CS(afs_pageout)		/* afs_hp_subr.c */ \
    AFS_CS(afs_hp_strategy)	/* afs_hp_subr.c */ \
    AFS_CS(PGetCPrefs)		/* afs_pioctl.c */ \
    AFS_CS(PSetCPrefs)		/* afs_pioctl.c */ \
    AFS_CS(SRXAFSCB_WhoAreYou)	/* afs_callback.c */ \
    AFS_CS(afs_DiscardDCache)	/* afs_dcache.c */ \
    AFS_CS(afs_FreeDiscardedDCache)	/* afs_dcache.c */ \
    AFS_CS(afs_MaybeFreeDiscardedDCache)	/* afs_dcache.c */ \
    AFS_CS(PFlushMount)		/* afs_pioctl.c */ \
    AFS_CS(SRXAFSCB_GetServerPrefs)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetCellServDB)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetLocalCell)	/* afs_callback.c */ \
    AFS_CS(afs_MarshallCacheConfig)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetCacheConfig)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetCE64)	/* afs_callback.c */ \
    AFS_CS(SRXAFSCB_GetCellByNum)	/* afs_callback.c */ \
    AFS_CS(BPrefetchNoCache)	/* afs_daemons.c */ \
    AFS_CS(afs_ReadNoCache)	/* osi_vnodeops.c */ \
    AFS_CS(PSetTokens2)		/* afs_pioctl.c */ \
    AFS_CS(PPrefetchFromTape)   /* afs_pioctl.c */ \
    AFS_CS(PFlushAllVolumeData)	/* afs_pioctl.c */

struct afs_CMCallStats {
#define AFS_CS(call) afs_int32 C_ ## call;
    AFS_CM_CALL_STATS
#undef AFS_CS
};

struct afs_CMMeanStats {
    struct afs_MeanStats something;	/* fill this in */
};

struct afs_CMStats {
    struct afs_CMCallStats callInfo;
    struct afs_CMMeanStats meanInfo;
};

/*
 * This is the structure accessible by specifying the
 * AFSCB_XSTATSCOLL_CALL_INFO collection to the xstat package.
 */
extern struct afs_CMStats afs_cmstats;

/*
 * Constants to track downtime durations:
 *	Bucket 0:           dur <= 10 min
 *	Bucket 1: 10 min  < dur <= 30 min
 *	Bucket 2: 30 min  < dur <= 1 hour
 *	Bucket 3: 1 hour  < dur <= 2 hours
 *	Bucket 4: 2 hours < dur <= 4 hours
 *	Bucket 5: 4 hours < dur <= 8 hours
 *	Bucket 6:           dur >= 8 hours
 */
#define AFS_STATS_NUM_DOWNTIME_DURATION_BUCKETS       7

#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET0     600	/*10 minutes */
#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET1    1800	/*30 minutes */
#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET2    3600	/*60 minutes */
#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET3    7200	/*2 hours */
#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET4   14400	/*4 hours */
#define AFS_STATS_MAX_DOWNTIME_DURATION_BUCKET5   28800	/*8 hours */

/*
 * Constants to track downtime incidents:
 *	Bucket 0:            down  =  0 times
 *	Bucket 1:            down  =  1 time
 *	Bucket 2:  1 time  < down <=  5 times
 *	Bucket 3:  5 times < down <= 10 times
 *	Bucket 4: 10 times < down <= 50 times
 *	Bucket 5:            down >  50 times
 */
#define AFS_STATS_NUM_DOWNTIME_INCIDENTS_BUCKETS 6

#define AFS_STATS_MAX_DOWNTIME_INCIDENTS_BUCKET0   0
#define AFS_STATS_MAX_DOWNTIME_INCIDENTS_BUCKET1   1
#define AFS_STATS_MAX_DOWNTIME_INCIDENTS_BUCKET2   5
#define AFS_STATS_MAX_DOWNTIME_INCIDENTS_BUCKET3   10
#define AFS_STATS_MAX_DOWNTIME_INCIDENTS_BUCKET4   50

/*
 * Numbers used to track aggregate up/downtime stats for servers.  We'll
 * keep these numbers separately for FS and VL server records, and then
 * again separately for servers in the same cell as this client machine
 * and those outside the client's cell.
 */
struct afs_stats_SrvUpDownInfo {
    afs_int32 numTtlRecords;	/*# records, active or inactive */
    afs_int32 numUpRecords;	/*# (active) records currently marked up */
    afs_int32 numDownRecords;	/*# (active) records currently marked down */
    afs_int32 sumOfRecordAges;	/*Sum of server record lifetimes */
    afs_int32 ageOfYoungestRecord;	/*Age of youngest server record */
    afs_int32 ageOfOldestRecord;	/*Age of oldest server record */
    afs_int32 numDowntimeIncidents;	/*Number of (completed) downtime incidents */
    afs_int32 numRecordsNeverDown;	/*Number of server records never marked down */
    afs_int32 maxDowntimesInARecord;	/*Max downtimes seen by any record */
    afs_int32 sumOfDowntimes;	/*Sum of all (completed) downtimes, in seconds */
    afs_int32 shortestDowntime;	/*Shortest downtime, in seconds */
    afs_int32 longestDowntime;	/*Longest downtime, in seconds */
    /*
     * Arrays keeping distributions on downtime durations and number of
     * downtime incidents.
     */
    afs_int32 downDurations[AFS_STATS_NUM_DOWNTIME_DURATION_BUCKETS];
    afs_int32 downIncidents[AFS_STATS_NUM_DOWNTIME_INCIDENTS_BUCKETS];
};

/*
 * Define indices for the server up/downtime arrays below.
 */
#define AFS_STATS_UPDOWN_IDX_SAME_CELL 0
#define AFS_STATS_UPDOWN_IDX_DIFF_CELL 1

/*
 * Performance numbers for the Cache Manager.
 */
struct afs_stats_CMPerf {
    afs_int32 numPerfCalls;	/*# of performance calls rcvd */

    afs_int32 epoch;		/*Cache Manager epoch time */
    afs_int32 numCellsVisible;	/*# cells we know about */
    afs_int32 numCellsContacted;	/*# cells corresponded with */
    afs_int32 dlocalAccesses;	/*# data accesses to files within cell */
    afs_int32 vlocalAccesses;	/*# stat accesses to files within cell */
    afs_int32 dremoteAccesses;	/*# data accesses to files outside of cell */
    afs_int32 vremoteAccesses;	/*# stat accesses to files outside of cell */
    afs_int32 cacheNumEntries;	/*# cache entries */
    afs_int32 cacheBlocksTotal;	/*# (1K) blocks configured for cache */
    afs_int32 cacheBlocksInUse;	/*# cache blocks actively in use */
    afs_int32 cacheBlocksOrig;	/*# cache blocks at bootup */
    afs_int32 cacheMaxDirtyChunks;	/*Max # dirty cache chunks tolerated */
    afs_int32 cacheCurrDirtyChunks;	/*Current # dirty cache chunks */
    afs_int32 dcacheHits;	/*# data files found in local cache */
    afs_int32 vcacheHits;	/*# stat entries found in local cache */
    afs_int32 dcacheMisses;	/*# data files NOT found in local cache */
    afs_int32 vcacheMisses;	/*# stat entries NOT found in local cache */
    afs_int32 cacheFlushes;	/*# files flushed from cache */
    afs_int32 cacheFilesReused;	/*# cache files reused */
    afs_int32 ProtServerAddr;	/*Addr of Protection Server used */
    afs_int32 vcacheXAllocs;	/* Additionally allocated vcaches */
    afs_int32 dcacheXAllocs;	/* Additionally allocated dcaches */

    /*
     * Some stats related to our buffer package
     */
    afs_int32 bufAlloced;	/* # of buffers allocated by afs */
    afs_int32 bufHits;		/* # of pages found on buffer cache */
    afs_int32 bufMisses;	/* # of pages NOT found on buffer cache */
    afs_int32 bufFlushDirty;	/* # of cached dirty bufs flushed because all busy */

    /*
     * Stats that keep track of all allocated/used objects in CM
     */
    afs_int32 LargeBlocksActive;	/* # of currently used large free pool entries */
    afs_int32 LargeBlocksAlloced;	/* # of allocated large free pool entries */
    afs_int32 SmallBlocksActive;	/* # of currently used small free pool entries */
    afs_int32 SmallBlocksAlloced;	/* # of allocated used small free pool entries */
    afs_int32 MediumBlocksActive;	/* # of currently used medium free pool entries */
    afs_int32 MediumBlocksAlloced;	/* # of allocated used medium free pool entries */
    afs_int32 OutStandingMemUsage;	/* # of alloced memory */
    afs_int32 OutStandingAllocs;	/* Outstanding osi_allocs (no osi_frees yet) */
    afs_int32 CallBackAlloced;	/* # callback structures allocated */
    afs_int32 CallBackFlushes;	/* # callback flush operations performed */

    /*
     * Accounting stats having to do with the server table & records.
     */
    afs_int32 srvRecords;	/*# of servers currently on record */
    afs_int32 srvRecordsHWM;	/* Server record high water mark */
    afs_int32 srvNumBuckets;	/* Num server hash chain buckets */
    afs_int32 srvMaxChainLength;	/* Max server hash chain length */
    afs_int32 srvMaxChainLengthHWM;	/* Server hash chain high water mark */

    /*
     * Stats having to do with the systype upon which the Cache Manager
     * is running.
     */
    afs_int32 sysName_ID;	/*Sysname ID for host hardware */

    /*
     * Stats recording downtime characteristics for each File Server and Volume
     * Location Server we've dealt with, both within the same cell and in
     * other cells.
     */
    struct afs_stats_SrvUpDownInfo fs_UpDown[2];
    struct afs_stats_SrvUpDownInfo vl_UpDown[2];

    afs_uint32 cbloops;
    afs_uint32 osiread_efaults;
    afs_int32 cacheBlocksDiscarded;	/*# cache blocks free but not truncated */
    afs_int32 cacheBucket0_Discarded;
    afs_int32 cacheBucket1_Discarded;
    afs_int32 cacheBucket2_Discarded;

    /*
     * Spares for future expansion.
     */
    afs_int32 spare[10];	/*Spares */
};


/*
 * Values denoting the File Server and Cache Manager opcodes.
 */
#define AFS_STATS_FS_RPCIDX_FETCHDATA		 0
#define AFS_STATS_FS_RPCIDX_FETCHACL		 1
#define AFS_STATS_FS_RPCIDX_FETCHSTATUS		 2
#define AFS_STATS_FS_RPCIDX_STOREDATA		 3
#define AFS_STATS_FS_RPCIDX_STOREACL		 4
#define AFS_STATS_FS_RPCIDX_STORESTATUS		 5
#define AFS_STATS_FS_RPCIDX_REMOVEFILE		 6
#define AFS_STATS_FS_RPCIDX_CREATEFILE		 7
#define AFS_STATS_FS_RPCIDX_RENAME		 8
#define AFS_STATS_FS_RPCIDX_SYMLINK		 9
#define AFS_STATS_FS_RPCIDX_LINK		10
#define AFS_STATS_FS_RPCIDX_MAKEDIR		11
#define AFS_STATS_FS_RPCIDX_REMOVEDIR		12
#define AFS_STATS_FS_RPCIDX_SETLOCK		13
#define AFS_STATS_FS_RPCIDX_EXTENDLOCK		14
#define AFS_STATS_FS_RPCIDX_RELEASELOCK		15
#define AFS_STATS_FS_RPCIDX_GETSTATISTICS	16
#define AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS	17
#define AFS_STATS_FS_RPCIDX_GETVOLUMEINFO	18
#define AFS_STATS_FS_RPCIDX_GETVOLUMESTATUS	19
#define AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS	20
#define AFS_STATS_FS_RPCIDX_GETROOTVOLUME	21
#define AFS_STATS_FS_RPCIDX_CHECKTOKEN		22
#define AFS_STATS_FS_RPCIDX_GETTIME		23
#define AFS_STATS_FS_RPCIDX_NGETVOLUMEINFO	24
#define AFS_STATS_FS_RPCIDX_BULKSTATUS		25
#define AFS_STATS_FS_RPCIDX_XSTATSVERSION	26
#define AFS_STATS_FS_RPCIDX_GETXSTATS		27
#define AFS_STATS_FS_RPCIDX_XLOOKUP             28
#define AFS_STATS_FS_RPCIDX_RESIDENCYRPCS       29

#define AFS_STATS_NUM_FS_RPC_OPS		30

#define AFS_STATS_FS_RPCIDXES_ISWRITE(X)        (((X > AFS_STATS_FS_RPCIDX_FETCHSTATUS) && (X < AFS_STATS_FS_RPCIDX_GETSTATISTICS)) || (X == AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS))
#define AFS_STATS_FS_RPCIDXES_WRITE_RETRIABLE(X) ((X >= AFS_STATS_FS_RPCIDX_STOREDATA) && (X <= AFS_STATS_FS_RPCIDX_STORESTATUS))

#define AFS_STATS_FS_XFERIDX_FETCHDATA		 0
#define AFS_STATS_FS_XFERIDX_STOREDATA		 1

#define AFS_STATS_NUM_FS_XFER_OPS		 2

#define AFS_STATS_CM_RPCIDX_CALLBACK		 0
#define AFS_STATS_CM_RPCIDX_INITCALLBACKSTATE	 1
#define AFS_STATS_CM_RPCIDX_PROBE		 2
#define AFS_STATS_CM_RPCIDX_GETLOCK		 3
#define AFS_STATS_CM_RPCIDX_GETCE		 4
#define AFS_STATS_CM_RPCIDX_XSTATSVERSION	 5
#define AFS_STATS_CM_RPCIDX_GETXSTATS		 6

#define AFS_STATS_NUM_CM_RPC_OPS		 7


/*
 * Record to track timing numbers for each Cache Manager RPC operation.
 */
struct afs_stats_opTimingData {
    afs_int32 numOps;		/*Number of operations executed */
    afs_int32 numSuccesses;	/*Number of successful ops */
    osi_timeval32_t sumTime;	/*Sum of sample timings */
    osi_timeval32_t sqrTime;	/*Sum of squares of sample timings */
    osi_timeval32_t minTime;	/*Minimum timing value observed */
    osi_timeval32_t maxTime;	/*Minimum timing value observed */
};

/*
 * We discriminate byte size transfers into this many buckets.
 */
#define AFS_STATS_NUM_XFER_BUCKETS       9

#define AFS_STATS_MAXBYTES_BUCKET0     128
#define AFS_STATS_MAXBYTES_BUCKET1    1024
#define AFS_STATS_MAXBYTES_BUCKET2    8192
#define AFS_STATS_MAXBYTES_BUCKET3   16384
#define AFS_STATS_MAXBYTES_BUCKET4   32768
#define AFS_STATS_MAXBYTES_BUCKET5  131072
#define AFS_STATS_MAXBYTES_BUCKET6  524288
#define AFS_STATS_MAXBYTES_BUCKET7 1048576

/*
 * Record to track timings and byte sizes for data transfers.
 */
struct afs_stats_xferData {
    afs_int32 numXfers;		/*Number of successful xfers */
    afs_int32 numSuccesses;	/*Number of successful xfers */
    osi_timeval32_t sumTime;	/*Sum of timing values */
    osi_timeval32_t sqrTime;	/*Sum of squares of timing values */
    osi_timeval32_t minTime;	/*Minimum xfer time recorded */
    osi_timeval32_t maxTime;	/*Maximum xfer time recorded */
    afs_int32 sumBytes;		/*Sum of KBytes transferred */
    afs_int32 minBytes;		/*Minimum value observed */
    afs_int32 maxBytes;		/*Maximum value observed */
    afs_int32 count[AFS_STATS_NUM_XFER_BUCKETS];	/*Tally for each range of bytes */
};

/*
 * Macros to operate on time values.
 *
 * afs_stats_TimeLessThan(t1, t2)     Non-zero if t1 is less than t2
 * afs_stats_TimeGreaterThan(t1, t2)  Non-zero if t1 is greater than t2
 * afs_stats_GetDiff(t3, t1, t2)      Set t3 to the difference between
 *					t1 and t2 (t1 is less than or
 *					equal to t2).
 * afs_stats_AddTo(t1, t2)            Add t2 to t1
 * afs_stats_TimeAssign(t1, t2)	     Assign time t2 to t1
 * afs_stats_SquareAddTo(t1,t2)      Add square of t2 to t1
 */
#define afs_stats_TimeLessThan(t1, t2)        \
            ((t1.tv_sec  < t2.tv_sec)  ? 1 : \
	     (t1.tv_sec  > t2.tv_sec)  ? 0 : \
	     (t1.tv_usec < t2.tv_usec) ? 1 : \
	     0)

#define afs_stats_TimeGreaterThan(t1, t2)     \
            ((t1.tv_sec  > t2.tv_sec)  ? 1 : \
	     (t1.tv_sec  < t2.tv_sec)  ? 0 : \
	     (t1.tv_usec > t2.tv_usec) ? 1 : \
	     0)

#define afs_stats_GetDiff(t3, t1, t2)				\
{								\
    /*								\
     * If the microseconds of the later time are smaller than	\
     * the earlier time, set up for proper subtraction (doing	\
     * the carry).						\
     */								\
    if (t2.tv_usec < t1.tv_usec) {				\
	t2.tv_usec += 1000000;					\
	t2.tv_sec -= 1;						\
    }								\
    t3.tv_sec  = t2.tv_sec  - t1.tv_sec;			\
    t3.tv_usec = t2.tv_usec - t1.tv_usec;			\
}

#define afs_stats_AddTo(t1, t2)    \
{                                 \
    t1.tv_sec  += t2.tv_sec;      \
    t1.tv_usec += t2.tv_usec;     \
    if (t1.tv_usec > 1000000) {   \
	t1.tv_usec -= 1000000;    \
	t1.tv_sec++;              \
    }                             \
}

#define afs_stats_TimeAssign(t1, t2)	\
{					\
    t1.tv_sec = t2.tv_sec;		\
    t1.tv_usec = t2.tv_usec;		\
}
/*
 * We calculate the square of a timeval as follows:
 *
 * The timeval struct contains two ints - the number of seconds and the
 * number of microseconds.  These two numbers together gives the correct
 * amount of time => t = t.tv_sec + (t.tv_usec / 1000000);
 *
 * if x = t.tv_sec and y = (t.tv_usec / 1000000) then the square is simply:
 *
 * x^2 + 2xy + y^2
 *
 * Since we are trying to avoid floating point math, we use the following
 * observations to simplify the above equation:
 *
 * The resulting t.tv_sec (x') only depends upon the x^2 + 2xy portion
 * of the equation.  This is easy to see if you think about y^2 in
 * decimal notation.  y^2 is always < 0 since y < 0.  Therefore in calculating
 * x', we can ignore y^2 (we do need to take care of rounding which is
 * done below).
 *
 * Similarly, in calculating t.tv_usec (y') we can ignore x^2 and concentrate
 * on 2xy + y^2.
 *
 * You'll notice that both x' and y' depend upon 2xy.  We can further
 * simplify things by realizing that x' depends on upon the integer
 * portion of the 2xy term.  We can get part of this integer by
 * multiplying 2 * x * t.tv_usec and then truncating the result by
 * / 1000000.  Similarly, we can get the decimal portion of this term
 * by performing the same multiplication and then % 1000000.  It is
 * possible that the decimal portion will in fact contain some of the
 * integer portion (this will be taken care of when we ensure that y'
 * is less than 1000000).
 *
 * The only other non-obvious calculation involves y^2.  The key to
 * understanding this part of the calculation is to expand y again
 * in a nonobvious manner.  We do this via the following expansion:
 *
 * y = t.tv_usec / 1000000;
 * let abcdef represent the six digits of t.tv_usec then we have:
 * t.tv_usec / 1000000 = abc/1000 + def/1000000;
 *
 * squaring yields:
 *
 * y^2 = (abc/1000)^2 + 2 * (abc/1000) * (def/1000000) + (def/1000000)^2
 *
 * Examining this equation yields the following observations:
 *
 * The second term can be calculated by multiplying abc and def then
 * shifting the decimal correctly.
 *
 * (def/1000000)^2 contributes only to rounding and we only round up
 * if def > 707.
 *
 * These two observations are the basis for the somewhat cryptic
 * calculation of usec^2 (i.e. they are the "tricks").
 */

#define afs_stats_SquareAddTo(t1, t2)                     \
{                                                         \
    /*                                                    \
     *  We use some tricks here to avoid floating point arithmetic  \
     */                                                             \
   if(t2.tv_sec > 0 )                                               \
     {                                                                        \
       t1.tv_sec += t2.tv_sec * t2.tv_sec                                     \
                    +  2 * t2.tv_sec * t2.tv_usec /1000000;                   \
       t1.tv_usec += (2 * t2.tv_sec * t2.tv_usec) % 1000000                   \
                     + (t2.tv_usec / 1000)*(t2.tv_usec / 1000)                \
                     + 2 * (t2.tv_usec / 1000) * (t2.tv_usec % 1000) / 1000   \
                     + (((t2.tv_usec % 1000) > 707) ? 1 : 0);                 \
     }                                                                        \
   else                                                                       \
     {                                                                        \
       t1.tv_usec += (t2.tv_usec / 1000)*(t2.tv_usec / 1000)                  \
                     + 2 * (t2.tv_usec / 1000) * (t2.tv_usec % 1000) / 1000   \
                     + (((t2.tv_usec % 1000) > 707) ? 1 : 0);                 \
     }                                                                        \
   if (t1.tv_usec > 1000000) {                                                \
        t1.tv_usec -= 1000000;                                                \
        t1.tv_sec++;                                                          \
   }                                                                          \
}




/*
 * Structure recording RPC outcomes.
 */
struct afs_stats_RPCErrors {
    afs_int32 err_Server;	/*Server down error */
    afs_int32 err_Network;	/*Network error */
    afs_int32 err_Protection;	/*Protection violation */
    afs_int32 err_Volume;	/*Volume-related error */
    afs_int32 err_VolumeBusies;	/*"Volume busy conditions encountered */
    afs_int32 err_Other;	/*Misc other errors */
};


/*
 * Structure holding RPC interface opcode measurements for the Cache Manager.
 */
struct afs_stats_RPCOpInfo {
    struct afs_stats_opTimingData
      fsRPCTimes[AFS_STATS_NUM_FS_RPC_OPS];	/*Individual FS RPC op timings */
    struct afs_stats_RPCErrors
      fsRPCErrors[AFS_STATS_NUM_FS_RPC_OPS];	/*Individual FS RPC op errors */
    struct afs_stats_xferData
      fsXferTimes[AFS_STATS_NUM_FS_XFER_OPS];	/*Individual FS RPC xfer timings */
    struct afs_stats_opTimingData
      cmRPCTimes[AFS_STATS_NUM_CM_RPC_OPS];	/*Individual CM RPC op timings */
};

/*
 * Structure holding authentication info for the CM.
 */
struct afs_stats_AuthentInfo {
    /*
     * This first set of fields don't have any history - they are simply
     * snapshots of the system at the time of the probe.
     */
    afs_int32 curr_PAGs;	/*Current number of PAGs */
    afs_int32 curr_Records;	/*Current # of records in table */
    afs_int32 curr_AuthRecords;	/*Current # of authenticated
				 * records (w/valid ticket) */
    afs_int32 curr_UnauthRecords;	/*Current # of unauthenticated
					 * records (w/o any ticket at all) */
    afs_int32 curr_MaxRecordsInPAG;	/*Max records for a single PAG */
    afs_int32 curr_LongestChain;	/*Length of longest current hash chain */

    /*
     * This second set of fields are values accumulated over the lifetme
     * of the current CM incarnation.
     */
    afs_int32 PAGCreations;	/*# PAG creations */
    afs_int32 TicketUpdates;	/*# ticket additions/refreshes */
    afs_int32 HWM_PAGs;		/*High water mark - # PAGs */
    afs_int32 HWM_Records;	/* " - # records */
    afs_int32 HWM_MaxRecordsInPAG;	/* " - max records for a single PAG */
    afs_int32 HWM_LongestChain;	/* " - longest hash chain */
};

/*
 * [Un]replicated file access.  These count the number of RXAFS_FetchData
 * calls get accomplished, and their need to call upon other replicas in
 * case of failure.
 */
struct afs_stats_AccessInfo {
    afs_int32 unreplicatedRefs;	/*# references to unreplicated data */
    afs_int32 replicatedRefs;	/*# references to replicated data */
    afs_int32 numReplicasAccessed;	/*# replicas accessed */
    afs_int32 maxReplicasPerRef;	/*Max # replicas accessed per ref */
    afs_int32 refFirstReplicaOK;	/*# references satisfied by 1st replica */
};

/*
 * Structure holding authoring info for the CM.  We keep track of
 * the results of writes on files and directories independently.
 * Results cover all objects in the cache uniformly.
 */
struct afs_stats_AuthorInfo {
    afs_int32 fileSameAuthor;	/*File write by same author */
    afs_int32 fileDiffAuthor;	/*File write by diff author */
    afs_int32 dirSameAuthor;	/*Directory write by same author */
    afs_int32 dirDiffAuthor;	/*Directory write by diff author */
};

/*
 * Structure holding ``full'' CM peformance measurements.
 */
struct afs_stats_CMFullPerf {
    afs_int32 numFullPerfCalls;	/*Number of accesses */
    struct afs_stats_CMPerf perf;	/*General performance stats */
    struct afs_stats_RPCOpInfo rpc;	/*RPC op stats */
    struct afs_stats_AuthentInfo authent;	/*Authentication stats */
    struct afs_stats_AccessInfo accessinf;	/*Access stats */
    struct afs_stats_AuthorInfo author;	/*Authorship stats */
};

/*
 * These are the storage declarations for the structures accessible
 * via the xstat package.
 */
/* extern struct afs_stats_CMPerf afs_stats_cmperf; */
/* extern struct afs_stats_CMFullPerf afs_stats_cmfullperf; */
/* extern afs_int32 afs_stats_XferSumBytes[]; */

#ifndef AFS_NOSTATS
/*
 * We define routines to keep running counts and means.  For the
 * running count, we have to concatenate the ``C_'' prefix on to
 * the routine name passed in as an argument to get the right
 * field name.
 */
#define AFS_STATCNT(arg)  ((afs_cmstats.callInfo.C_ ## arg)++)

#define AFS_MEANCNT(arg, value) \
    (afs_AddToMean(((afs_cmstats.meanInfo).(arg)),value))

#endif /* AFS_NOSTATS */


#endif /* __OPENAFS_AFS_STATS_H__ */
