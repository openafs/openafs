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
#define XSTATS_END_TIME

#else /* AFS_NOSTATS */

#define AFS_STATS(arg) arg
#ifndef KERNEL
/* NOTE: Ensure this is the same size in user and kernel mode. */
typedef struct timeval osi_timeval_t;
#endif /* !KERNEL */

#define XSTATS_DECLS struct afs_stats_opTimingData *opP = NULL; \
    osi_timeval_t opStartTime, opStopTime, elapsedTime

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
 */
struct afs_CMCallStats {
    afs_int32 C_afs_init;	/* afs_aix_subr.c */
    afs_int32 C_gop_rdwr;	/* afs_aix_subr.c */
    afs_int32 C_aix_gnode_rele;	/* afs_aix_subr.c */
    afs_int32 C_gettimeofday;	/* afs_aix_subr.c */
    afs_int32 C_m_cpytoc;	/* afs_aix_subr.c */
    afs_int32 C_aix_vattr_null;	/* afs_aix_subr.c */
    afs_int32 C_afs_gn_ftrunc;	/* afs_aixops.c */
    afs_int32 C_afs_gn_rdwr;	/* afs_aixops.c */
    afs_int32 C_afs_gn_ioctl;	/* afs_aixops.c */
    afs_int32 C_afs_gn_lockctl;	/* afs_aixops.c */
    afs_int32 C_afs_gn_readlink;	/* afs_aixops.c */
    afs_int32 C_afs_gn_readdir;	/* afs_aixops.c */
    afs_int32 C_afs_gn_select;	/* afs_aixops.c */
    afs_int32 C_afs_gn_strategy;	/* afs_aixops.c */
    afs_int32 C_afs_gn_symlink;	/* afs_aixops.c */
    afs_int32 C_afs_gn_revoke;	/* afs_aixops.c */
    afs_int32 C_afs_gn_link;	/* afs_aixops.c */
    afs_int32 C_afs_gn_mkdir;	/* afs_aixops.c */
    afs_int32 C_afs_gn_mknod;	/* afs_aixops.c */
    afs_int32 C_afs_gn_remove;	/* afs_aixops.c */
    afs_int32 C_afs_gn_rename;	/* afs_aixops.c */
    afs_int32 C_afs_gn_rmdir;	/* afs_aixops.c */
    afs_int32 C_afs_gn_fid;	/* afs_aixops.c */
    afs_int32 C_afs_gn_lookup;	/* afs_aixops.c */
    afs_int32 C_afs_gn_open;	/* afs_aixops.c */
    afs_int32 C_afs_gn_create;	/* afs_aixops.c */
    afs_int32 C_afs_gn_hold;	/* afs_aixops.c */
    afs_int32 C_afs_gn_close;	/* afs_aixops.c */
    afs_int32 C_afs_gn_map;	/* afs_aixops.c */
    afs_int32 C_afs_gn_rele;	/* afs_aixops.c */
    afs_int32 C_afs_gn_unmap;	/* afs_aixops.c */
    afs_int32 C_afs_gn_access;	/* afs_aixops.c */
    afs_int32 C_afs_gn_getattr;	/* afs_aixops.c */
    afs_int32 C_afs_gn_setattr;	/* afs_aixops.c */
    afs_int32 C_afs_gn_fclear;	/* afs_aixops.c */
    afs_int32 C_afs_gn_fsync;	/* afs_aixops.c */
    afs_int32 C_pHash;		/* afs_buffer.c */
    afs_int32 C_DInit;		/* afs_buffer.c */
    afs_int32 C_DRead;		/* afs_buffer.c */
    afs_int32 C_FixupBucket;	/* afs_buffer.c */
    afs_int32 C_afs_newslot;	/* afs_buffer.c */
    afs_int32 C_DRelease;	/* afs_buffer.c */
    afs_int32 C_DFlush;		/* afs_buffer.c */
    afs_int32 C_DFlushEntry;	/* afs_buffer.c */
    afs_int32 C_DVOffset;	/* afs_buffer.c */
    afs_int32 C_DZap;		/* afs_buffer.c */
    afs_int32 C_DNew;		/* afs_buffer.c */
    afs_int32 C_shutdown_bufferpackage;	/* afs_buffer.c */
    afs_int32 C_afs_CheckKnownBad;	/* afs_cache.c */
    afs_int32 C_afs_RemoveVCB;	/* afs_cache.c */
    afs_int32 C_afs_NewVCache;	/* afs_cache.c */
    afs_int32 C_afs_FlushActiveVcaches;	/* afs_cache.c */
    afs_int32 C_afs_VerifyVCache;	/* afs_cache.c */
    afs_int32 C_afs_WriteVCache;	/* afs_cache.c */
    afs_int32 C_afs_GetVCache;	/* afs_cache.c */
    afs_int32 C_afs_StuffVcache;	/* afs_cache.c */
    afs_int32 C_afs_FindVCache;	/* afs_cache.c */
    afs_int32 C_afs_PutDCache;	/* afs_cache.c */
    afs_int32 C_afs_PutVCache;	/* afs_cache.c */
    afs_int32 C_CacheStoreProc;	/* afs_cache.c */
    afs_int32 C_afs_FindDCache;	/* afs_cache.c */
    afs_int32 C_afs_TryToSmush;	/* afs_cache.c */
    afs_int32 C_afs_AdjustSize;	/* afs_cache.c */
    afs_int32 C_afs_CheckSize;	/* afs_cache.c */
    afs_int32 C_afs_StoreWarn;	/* afs_cache.c */
    afs_int32 C_CacheFetchProc;	/* afs_cache.c */
    afs_int32 C_UFS_CacheStoreProc;	/* afs_cache.c */
    afs_int32 C_UFS_CacheFetchProc;	/* afs_cache.c */
    afs_int32 C_afs_GetDCache;	/* afs_cache.c */
    afs_int32 C_afs_SimpleVStat;	/* afs_cache.c */
    afs_int32 C_afs_ProcessFS;	/* afs_cache.c */
    afs_int32 C_afs_InitCacheInfo;	/* afs_cache.c */
    afs_int32 C_afs_InitVolumeInfo;	/* afs_cache.c */
    afs_int32 C_afs_InitCacheFile;	/* afs_cache.c */
    afs_int32 C_afs_CacheInit;	/* afs_cache.c */
    afs_int32 C_afs_GetDSlot;	/* afs_cache.c */
    afs_int32 C_afs_WriteThroughDSlots;	/* afs_cache.c */
    afs_int32 C_afs_MemGetDSlot;	/* afs_cache.c */
    afs_int32 C_afs_UFSGetDSlot;	/* afs_cache.c */
    afs_int32 C_afs_StoreDCache;	/* afs_cache.c */
    afs_int32 C_afs_StoreMini;	/* afs_cache.c */
    afs_int32 C_shutdown_cache;	/* afs_cache.c */
    afs_int32 C_afs_StoreAllSegments;	/* afs_cache.c */
    afs_int32 C_afs_InvalidateAllSegments;	/* afs_cache.c */
    afs_int32 C_afs_TruncateAllSegments;	/* afs_cache.c */
    afs_int32 C_afs_CheckVolSync;	/* afs_cache.c */
    afs_int32 C_afs_wakeup;	/* afs_cache.c */
    afs_int32 C_afs_CFileOpen;	/* afs_cache.c */
    afs_int32 C_afs_CFileTruncate;	/* afs_cache.c */
    afs_int32 C_afs_GetDownD;	/* afs_cache.c */
    afs_int32 C_afs_WriteDCache;	/* afs_cache.c */
    afs_int32 C_afs_FlushDCache;	/* afs_cache.c */
    afs_int32 C_afs_GetDownDSlot;	/* afs_cache.c */
    afs_int32 C_afs_FlushVCache;	/* afs_cache.c */
    afs_int32 C_afs_GetDownV;	/* afs_cache.c */
    afs_int32 C_afs_QueueVCB;	/* afs_cache.c */
    afs_int32 C_afs_call;	/* afs_call.c */
    afs_int32 C_afs_syscall_call;	/* afs_call.c */
    afs_int32 C_syscall;	/* afs_call.c */
    afs_int32 C_lpioctl;	/* afs_call.c */
    afs_int32 C_lsetpag;	/* afs_call.c */
    afs_int32 C_afs_syscall;	/* afs_call.c */
    afs_int32 C_afs_CheckInit;	/* afs_call.c */
    afs_int32 C_afs_shutdown;	/* afs_call.c */
    afs_int32 C_shutdown_BKG;	/* afs_call.c */
    afs_int32 C_shutdown_afstest;	/* afs_call.c */
    afs_int32 C_SRXAFSCB_GetCE;	/* afs_callback.c */
    afs_int32 C_ClearCallBack;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetLock;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_CallBack;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_InitCallBackState;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_Probe;	/* afs_callback.c */
    afs_int32 C_afs_RXCallBackServer;	/* afs_callback.c */
    afs_int32 C_shutdown_CB;	/* afs_callback.c */
    afs_int32 C_afs_Chunk;	/* afs_chunk.c */
    afs_int32 C_afs_ChunkBase;	/* afs_chunk.c */
    afs_int32 C_afs_ChunkOffset;	/* afs_chunk.c */
    afs_int32 C_afs_ChunkSize;	/* afs_chunk.c */
    afs_int32 C_afs_ChunkToBase;	/* afs_chunk.c */
    afs_int32 C_afs_ChunkToSize;	/* afs_chunk.c */
    afs_int32 C_afs_SetChunkSize;	/* afs_chunk.c */

    afs_int32 C_afs_config;	/* afs_config.c */
    afs_int32 C_mem_freebytes;	/* afs_config.c */
    afs_int32 C_mem_getbytes;	/* afs_config.c */
    afs_int32 C_fpalloc;	/* afs_config.c */
    afs_int32 C_kluge_init;	/* afs_config.c */
    afs_int32 C_ufdalloc;	/* afs_config.c */
    afs_int32 C_ufdfree;	/* afs_config.c */
    afs_int32 C_commit;		/* afs_config.c */
    afs_int32 C_dev_ialloc;	/* afs_config.c */
    afs_int32 C_ffree;		/* afs_config.c */
    afs_int32 C_iget;		/* afs_config.c */
    afs_int32 C_iptovp;		/* afs_config.c */
    afs_int32 C_ilock;		/* afs_config.c */
    afs_int32 C_irele;		/* afs_config.c */
    afs_int32 C_iput;		/* afs_config.c */

    afs_int32 C_afs_Daemon;	/* afs_daemons.c */
    afs_int32 C_afs_CheckRootVolume;	/* afs_daemons.c */
    afs_int32 C_BPath;		/* afs_daemons.c */
    afs_int32 C_BPrefetch;	/* afs_daemons.c */
    afs_int32 C_BStore;		/* afs_daemons.c */
    afs_int32 C_afs_BBusy;	/* afs_daemons.c */
    afs_int32 C_afs_BQueue;	/* afs_daemons.c */
    afs_int32 C_afs_BRelease;	/* afs_daemons.c */
    afs_int32 C_afs_BackgroundDaemon;	/* afs_daemons.c */
    afs_int32 C_shutdown_daemons;	/* afs_daemons.c */
    afs_int32 C_exporter_add;	/* afs_exporter.c */
    afs_int32 C_exporter_find;	/* afs_exporter.c */
    afs_int32 C_afs_gfs_kalloc;	/* afs_gfs_subr.c */
    afs_int32 C_IsAfsVnode;	/* afs_gfs_subr.c */
    afs_int32 C_SetAfsVnode;	/* afs_gfs_subr.c */
    afs_int32 C_afs_gfs_kfree;	/* afs_gfs_subr.c */
    afs_int32 C_gop_lookupname;	/* afs_gfs_subr.c */
    afs_int32 C_gfsvop_getattr;	/* afs_gfs_subr.c */
    afs_int32 C_gfsvop_rdwr;	/* afs_gfs_subr.c */
    afs_int32 C_afs_uniqtime;	/* afs_gfs_subr.c */
    afs_int32 C_gfs_vattr_null;	/* afs_gfs_subr.c */
    afs_int32 C_afs_lock;	/* afs_gfsops.c */
    afs_int32 C_afs_unlock;	/* afs_gfsops.c */
    afs_int32 C_afs_update;	/* afs_gfsops.c */
    afs_int32 C_afs_gclose;	/* afs_gfsops.c */
    afs_int32 C_afs_gopen;	/* afs_gfsops.c */
    afs_int32 C_afs_greadlink;	/* afs_gfsops.c */
    afs_int32 C_afs_select;	/* afs_gfsops.c */
    afs_int32 C_afs_gbmap;	/* afs_gfsops.c */
    afs_int32 C_afs_getfsdata;	/* afs_gfsops.c */
    afs_int32 C_afs_gsymlink;	/* afs_gfsops.c */
    afs_int32 C_afs_namei;	/* afs_gfsops.c */
    afs_int32 C_printgnode;	/* afs_gfsops.c */
    afs_int32 C_HaveGFSLock;	/* afs_gfsops.c */
    afs_int32 C_afs_gmount;	/* afs_gfsops.c */
    afs_int32 C_AddGFSLock;	/* afs_gfsops.c */
    afs_int32 C_RemoveGFSLock;	/* afs_gfsops.c */
    afs_int32 C_afs_grlock;	/* afs_gfsops.c */
    afs_int32 C_afs_gumount;	/* afs_gfsops.c */
    afs_int32 C_afs_gget;	/* afs_gfsops.c */
    afs_int32 C_afs_glink;	/* afs_gfsops.c */
    afs_int32 C_afs_gmkdir;	/* afs_gfsops.c */
    afs_int32 C_afs_sbupdate;	/* afs_gfsops.c */
    afs_int32 C_afs_unlink;	/* afs_gfsops.c */
    afs_int32 C_afs_grmdir;	/* afs_gfsops.c */
    afs_int32 C_afs_makenode;	/* afs_gfsops.c */
    afs_int32 C_afs_grename;	/* afs_gfsops.c */
    afs_int32 C_afs_rele;	/* afs_gfsops.c */
    afs_int32 C_afs_syncgp;	/* afs_gfsops.c */
    afs_int32 C_afs_getval;	/* afs_gfsops.c */
    afs_int32 C_afs_gfshack;	/* afs_gfsops.c */
    afs_int32 C_afs_trunc;	/* afs_gfsops.c */
    afs_int32 C_afs_rwgp;	/* afs_gfsops.c */
    afs_int32 C_afs_stat;	/* afs_gfsops.c */
    afs_int32 C_afsc_link;	/* afs_hp_subr.c */
    afs_int32 C_hpsobind;	/* afs_hp_subr.c */
    afs_int32 C_hpsoclose;	/* afs_hp_subr.c */
    afs_int32 C_hpsocreate;	/* afs_hp_subr.c */
    afs_int32 C_hpsoreserve;	/* afs_hp_subr.c */
    afs_int32 C_afs_vfs_mount;	/* afs_hp_subr.c */
    afs_int32 C_devtovfs;	/* afs_istuff.c */
    afs_int32 C_igetinode;	/* afs_istuff.c */
    afs_int32 C_afs_syscall_iopen;	/* afs_istuff.c */
    afs_int32 C_iopen;		/* afs_istuff.c */
    afs_int32 C_afs_syscall_iincdec;	/* afs_istuff.c */
    afs_int32 C_afs_syscall_ireadwrite;	/* afs_istuff.c */
    afs_int32 C_iincdec;	/* afs_istuff.c */
    afs_int32 C_ireadwrite;	/* afs_istuff.c */
    afs_int32 C_oiread;		/* afs_istuff.c */
    afs_int32 C_AHash;		/* afs_istuff.c */
    afs_int32 C_QTOA;		/* afs_istuff.c */
    afs_int32 C_afs_FindPartByDev;	/* afs_istuff.c */
    afs_int32 C_aux_init;	/* afs_istuff.c */
    afs_int32 C_afs_GetNewPart;	/* afs_istuff.c */
    afs_int32 C_afs_InitAuxVolFile;	/* afs_istuff.c */
    afs_int32 C_afs_CreateAuxEntry;	/* afs_istuff.c */
    afs_int32 C_afs_GetAuxSlot;	/* afs_istuff.c */
    afs_int32 C_afs_GetDownAux;	/* afs_istuff.c */
    afs_int32 C_afs_FlushAuxCache;	/* afs_istuff.c */
    afs_int32 C_afs_GetAuxInode;	/* afs_istuff.c */
    afs_int32 C_afs_PutAuxInode;	/* afs_istuff.c */
    afs_int32 C_afs_ReadAuxInode;	/* afs_istuff.c */
    afs_int32 C_afs_WriteAuxInode;	/* afs_istuff.c */
    afs_int32 C_afs_auxcall;	/* afs_istuff.c */
    afs_int32 C_tmpdbg_auxtbl;	/* afs_istuff.c */
    afs_int32 C_tmpdbg_parttbl;	/* afs_istuff.c */
    afs_int32 C_idec;		/* afs_istuff.c */
    afs_int32 C_iinc;		/* afs_istuff.c */
    afs_int32 C_iread;		/* afs_istuff.c */
    afs_int32 C_iwrite;		/* afs_istuff.c */
    afs_int32 C_getinode;	/* afs_istuff.c */
    afs_int32 C_trygetfs;	/* afs_istuff.c */
    afs_int32 C_iforget;	/* afs_istuff.c */
    afs_int32 C_afs_syscall_icreate;	/* afs_istuff.c */
    afs_int32 C_icreate;	/* afs_istuff.c */
    afs_int32 C_Lock_Init;	/* afs_lock.c */
    afs_int32 C_Lock_Obtain;	/* afs_lock.c */
    afs_int32 C_Lock_ReleaseR;	/* afs_lock.c */
    afs_int32 C_Lock_ReleaseW;	/* afs_lock.c */
    afs_int32 C_afs_BozonLock;	/* afs_lock.c */
    afs_int32 C_afs_BozonUnlock;	/* afs_lock.c */
    afs_int32 C_osi_SleepR;	/* afs_lock.c */
    afs_int32 C_osi_SleepS;	/* afs_lock.c */
    afs_int32 C_osi_SleepW;	/* afs_lock.c */
    afs_int32 C_osi_Sleep;	/* afs_lock */
    afs_int32 C_afs_BozonInit;	/* afs_lock.c */
    afs_int32 C_afs_CheckBozonLock;	/* afs_lock.c */
    afs_int32 C_afs_CheckBozonLockBlocking;	/* afs_lock.c */
    afs_int32 C_xxxinit;	/* afs_main.c */
    afs_int32 C_KernelEntry;	/* afs_main.c */
    afs_int32 C_afs_InitMemCache;	/* afs_memcache.c */
    afs_int32 C_afs_LookupMCE;	/* afs_memcache.c */
    afs_int32 C_afs_MemReadBlk;	/* afs_memcache.c */
    afs_int32 C_afs_MemReadUIO;	/* afs_memcache.c */
    afs_int32 C_afs_MemWriteBlk;	/* afs_memcache.c */
    afs_int32 C_afs_MemCacheStoreProc;	/* afs_memcache.c */
    afs_int32 C_afs_MemCacheTruncate;	/* afs_memcache.c */
    afs_int32 C_afs_MemWriteUIO;	/* afs_memcache.c */
    afs_int32 C_afs_MemCacheFetchProc;	/* afs_memcache.c */
    afs_int32 C_afs_vnode_pager_create;	/* afs_next_aux.c */
    afs_int32 C_next_KernelEntry;	/* afs_next_subr.c */
    afs_int32 C_afs_GetNfsClientPag;	/* afs_nfsclnt.c */
    afs_int32 C_afs_FindNfsClientPag;	/* afs_nfsclnt.c */
    afs_int32 C_afs_PutNfsClientPag;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_reqhandler;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_GC;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_hold;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_stats;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_sysname;	/* afs_nfsclnt.c */
    afs_int32 C_afs_nfsclient_shutdown;	/* afs_nfsclnt.c */
    afs_int32 C_afs_rfs_readdir_fixup;	/* afs_nfssrv.c */
    afs_int32 C_afs_rfs_dispatch;	/* afs_nfssrv.c */
    afs_int32 C_afs_xnfs_svc;	/* afs_nfssrv.c */
    afs_int32 C_afs_xdr_putrddirres;	/* afs_nfssrv.c */
    afs_int32 C_afs_rfs_readdir;	/* afs_nfssrv.c */
    afs_int32 C_afs_rfs_rddirfree;	/* afs_nfssrv.c */
    afs_int32 C_rfs_dupcreate;	/* afs_nfssrv.c */
    afs_int32 C_rfs_dupsetattr;	/* afs_nfssrv.c */
    afs_int32 C_Nfs2AfsCall;	/* afs_nfssrv.c */
    afs_int32 C_afs_sun_xuntext;	/* afs_osi.c */
    afs_int32 C_osi_Active;	/* afs_osi.c */
    afs_int32 C_osi_FlushPages;	/* afs_osi.c */
    afs_int32 C_osi_FlushText;	/* afs_osi.c */
    afs_int32 C_osi_CallProc;	/* afs_osi.c */
    afs_int32 C_osi_CancelProc;	/* afs_osi.c */
    afs_int32 C_osi_Invisible;	/* afs_osi.c */
    afs_int32 C_osi_Time;	/* afs_osi.c */
    afs_int32 C_osi_Alloc;	/* afs_osi.c */
    afs_int32 C_osi_SetTime;	/* afs_osi.c */
    afs_int32 C_osi_Dump;	/* afs_osi.c */
    afs_int32 C_osi_Free;	/* afs_osi.c */
    afs_int32 C_shutdown_osi;	/* afs_osi.c */
    afs_int32 C_osi_UFSOpen;	/* afs_osifile.c */
    afs_int32 C_osi_Close;	/* afs_osifile.c */
    afs_int32 C_osi_Stat;	/* afs_osifile.c */
    afs_int32 C_osi_Truncate;	/* afs_osifile.c */
    afs_int32 C_osi_Read;	/* afs_osifile.c */
    afs_int32 C_osi_Write;	/* afs_osifile.c */
    afs_int32 C_osi_MapStrategy;	/* afs_osifile.c */
    afs_int32 C_shutdown_osifile;	/* afs_osifile.c */
    afs_int32 C_osi_FreeLargeSpace;	/* afs_osinet.c */
    afs_int32 C_osi_FreeSmallSpace;	/* afs_osinet.c */
    afs_int32 C_pkt_iodone;	/* afs_osinet.c */
    afs_int32 C_shutdown_osinet;	/* afs_osinet.c */
    afs_int32 C_afs_cs;		/* afs_osinet.c */
    afs_int32 C_osi_AllocLargeSpace;	/* afs_osinet.c */
    afs_int32 C_osi_AllocSmallSpace;	/* afs_osinet.c */
    afs_int32 C_osi_CloseToTheEdge;	/* afs_osinet.c */
    afs_int32 C_osi_xgreedy;	/* afs_osinet.c */
    afs_int32 C_osi_FreeSocket;	/* afs_osinet.c */
    afs_int32 C_osi_NewSocket;	/* afs_osinet.c */
    afs_int32 C_trysblock;	/* afs_osinet.c */
    afs_int32 C_osi_NetSend;	/* afs_osinet.c */
    afs_int32 C_WaitHack;	/* afs_osinet.c */
    afs_int32 C_osi_CancelWait;	/* afs_osinet.c */
    afs_int32 C_osi_InitWaitHandle;	/* afs_osinet.c */
    afs_int32 C_osi_Wakeup;	/* afs_osinet.c */
    afs_int32 C_osi_Wait;	/* afs_osinet.c */
    afs_int32 C_dirp_Read;	/* afs_physio.c */
    afs_int32 C_dirp_SetCacheDev;	/* afs_physio.c */
    afs_int32 C_Die;		/* afs_physio.c */
    afs_int32 C_dirp_Cpy;	/* afs_physio.c */
    afs_int32 C_dirp_Eq;	/* afs_physio.c */
    afs_int32 C_dirp_Write;	/* afs_physio.c */
    afs_int32 C_dirp_Zap;	/* afs_physio.c */
    afs_int32 C_PSetVolumeStatus;	/* afs_pioctl.c */
    afs_int32 C_PFlush;		/* afs_pioctl.c */
    afs_int32 C_PNewStatMount;	/* afs_pioctl.c */
    afs_int32 C_PGetTokens;	/* afs_pioctl.c */
    afs_int32 C_PUnlog;		/* afs_pioctl.c */
    afs_int32 C_PCheckServers;	/* afs_pioctl.c */
    afs_int32 C_PMariner;	/* afs_pioctl.c */
    afs_int32 C_PCheckAuth;	/* afs_pioctl.c */
    afs_int32 C_PCheckVolNames;	/* afs_pioctl.c */
    afs_int32 C_PFindVolume;	/* afs_pioctl.c */
    afs_int32 C_Prefetch;	/* afs_pioctl.c */
    afs_int32 C_PGetCacheSize;	/* afs_pioctl.c */
    afs_int32 C_PRemoveCallBack;	/* afs_pioctl.c */
    afs_int32 C_PSetCacheSize;	/* afs_pioctl.c */
    afs_int32 C_PViceAccess;	/* afs_pioctl.c */
    afs_int32 C_PListCells;	/* afs_pioctl.c */
    afs_int32 C_PNewCell;	/* afs_pioctl.c */
    afs_int32 C_PRemoveMount;	/* afs_pioctl.c */
    afs_int32 C_HandleIoctl;	/* afs_pioctl.c */
    afs_int32 C__AFSIOCTL;	/* afs_pioctl.c */
    afs_int32 C__VALIDAFSIOCTL;	/* afs_pioctl.c */
    afs_int32 C_PGetCellStatus;	/* afs_pioctl.c */
    afs_int32 C_PSetCellStatus;	/* afs_pioctl.c */
    afs_int32 C_PVenusLogging;	/* afs_pioctl.c */
    afs_int32 C_PFlushVolumeData;	/* afs_pioctl.c */
    afs_int32 C_PSetSysName;	/* afs_pioctl.c */
    afs_int32 C_PExportAfs;	/* afs_pioctl.c */
    afs_int32 C_HandleClientContext;	/* afs_pioctl.c */
    afs_int32 C_afs_ioctl;	/* afs_pioctl.c */
    afs_int32 C_afs_xioctl;	/* afs_pioctl.c */
    afs_int32 C_afs_pioctl;	/* afs_pioctl.c */
    afs_int32 C_afs_syscall_pioctl;	/* afs_pioctl.c */
    afs_int32 C_HandlePioctl;	/* afs_pioctl.c */
    afs_int32 C_PGetAcl;	/* afs_pioctl.c */
    afs_int32 C_PGetFID;	/* afs_pioctl.c */
    afs_int32 C_PSetAcl;	/* afs_pioctl.c */
    afs_int32 C_PBogus;		/* afs_pioctl.c */
    afs_int32 C_PGetFileCell;	/* afs_pioctl.c */
    afs_int32 C_PGetWSCell;	/* afs_pioctl.c */
    afs_int32 C_PNoop;		/* afs_pioctl.c */
    afs_int32 C_PGetUserCell;	/* afs_pioctl.c */
    afs_int32 C_PSetTokens;	/* afs_pioctl.c */
    afs_int32 C_PGetVolumeStatus;	/* afs_pioctl.c */
    afs_int32 C_afs_ResetAccessCache;	/* afs_resource.c */
    afs_int32 C_afs_FindUser;	/* afs_resource.c */
    afs_int32 C_afs_ResetUserConns;	/* afs_resource.c */
    afs_int32 C_afs_ResourceInit;	/* afs_resource.c */
    afs_int32 C_afs_GetCell;	/* afs_resource.c */
    afs_int32 C_afs_GetCellByIndex;	/* afs_resource.c */
    afs_int32 C_afs_GetCellByName;	/* afs_resource.c */
    afs_int32 C_afs_GetRealCellByIndex;	/* afs_resource.c */
    afs_int32 C_afs_NewCell;	/* afs_resource.c */
    afs_int32 C_afs_GetUser;	/* afs_resource.c */
    afs_int32 C_afs_PutUser;	/* afs_resource.c */
    afs_int32 C_afs_SetPrimary;	/* afs_resource.c */
    afs_int32 C_CheckVLDB;	/* afs_resource.c */
    afs_int32 C_afs_GetVolume;	/* afs_resource.c */
    afs_int32 C_afs_GetVolumeByName;	/* afs_resource.c */
    afs_int32 C_InstallVolumeEntry;	/* afs_resource.c */
    afs_int32 C_InstallVolumeInfo;	/* afs_resource.c */
    afs_int32 C_afs_FindServer;	/* afs_resource.c */
    afs_int32 C_afs_PutVolume;	/* afs_resource.c */
    afs_int32 C_afs_random;	/* afs_resource.c */
    afs_int32 C_ranstage;	/* afs_resource.c */
    afs_int32 C_RemoveUserConns;	/* afs_resource.c */
    afs_int32 C_afs_MarinerLog;	/* afs_resource.c */
    afs_int32 C_afs_vtoi;	/* afs_resource.c */
    afs_int32 C_afs_GetServer;	/* afs_resource.c */
    afs_int32 C_afs_SortServers;	/* afs_resource.c */
    afs_int32 C_afs_Conn;	/* afs_resource.c */
    afs_int32 C_afs_ConnByHost;	/* afs_resource.c */
    afs_int32 C_afs_ConnByMHosts;	/* afs_resource.c */
    afs_int32 C_afs_Analyze;	/* afs_resource.c */
    afs_int32 C_afs_PutConn;	/* afs_resource.c */
    afs_int32 C_afs_ResetVolumeInfo;	/* afs_resource.c */
    afs_int32 C_StartLogFile;	/* afs_resource.c */
    afs_int32 C_afs_SetLogFile;	/* afs_resource.c */
    afs_int32 C_EndLogFile;	/* afs_resource.c */
    afs_int32 C_afs_dp;		/* afs_resource.c */
    afs_int32 C_fprf;		/* afs_resource.c */
    afs_int32 C_fprint;		/* afs_resource.c */
    afs_int32 C_fprintn;	/* afs_resource.c */
    afs_int32 C_afs_CheckLocks;	/* afs_resource.c */
    afs_int32 C_puttofile;	/* afs_resource.c */
    afs_int32 C_shutdown_AFS;	/* afs_resource.c */
    afs_int32 C_afs_CheckCacheResets;	/* afs_resource.c */
    afs_int32 C_afs_GCUserData;	/* afs_resource.c */
    afs_int32 C_VSleep;		/* afs_resource.c */
    afs_int32 C_afs_CheckCode;	/* afs_resource.c */
    afs_int32 C_afs_CopyError;	/* afs_resource.c */
    afs_int32 C_afs_FinalizeReq;	/* afs_resource.c */
    afs_int32 C_afs_cv2string;	/* afs_resource.c */
    afs_int32 C_afs_FindVolCache;	/* afs_resource.c */
    afs_int32 C_afs_GetVolCache;	/* afs_resource.c */
    afs_int32 C_afs_GetVolSlot;	/* afs_resource.c */
    afs_int32 C_afs_WriteVolCache;	/* afs_resource.c */
    afs_int32 C_afs_UFSGetVolSlot;	/* afs_resource.c */
    afs_int32 C_afs_CheckVolumeNames;	/* afs_resource.c */
    afs_int32 C_afs_MemGetVolSlot;	/* afs_resource.c */
    afs_int32 C_print_internet_address;	/* afs_resource.c */
    afs_int32 C_CheckVLServer;	/* afs_resource.c */
    afs_int32 C_HaveCallBacksFrom;	/* afs_resource.c */
    afs_int32 C_ServerDown;	/* afs_resource.c */
    afs_int32 C_afs_CheckServers;	/* afs_resource.c */
    afs_int32 C_afs_AddToMean;	/* afs_stat.c */
    afs_int32 C_afs_GetCMStat;	/* afs_stat.c */
    afs_int32 C_afs_getpage;	/* afs_sun_subr.c */
    afs_int32 C_afs_putpage;	/* afs_sun_subr.c */
    afs_int32 C_afs_nfsrdwr;	/* afs_sun_subr.c */
    afs_int32 C_afs_map;	/* afs_sun_subr.c */
    afs_int32 C_afs_cmp;	/* afs_sun_subr.c */
    afs_int32 C_afs_cntl;	/* afs_sun_subr.c */
    afs_int32 C_afs_dump;	/* afs_sun_subr.c */
    afs_int32 C_afs_realvp;	/* afs_sun_subr.c */
    afs_int32 C_afs_PageLeft;	/* afs_sun_subr.c */
    afs_int32 C_afsinit;	/* afs_vfsops.c */
    afs_int32 C_afs_mount;	/* afs_vfsops.c */
    afs_int32 C_afs_unmount;	/* afs_vfsops.c */
    afs_int32 C_afs_root;	/* afs_vfsops.c */
    afs_int32 C_afs_statfs;	/* afs_vfsops.c */
    afs_int32 C_afs_sync;	/* afs_vfsops.c */
    afs_int32 C_afs_vget;	/* afs_vfsops.c */
    afs_int32 C_afs_mountroot;	/* afs_vfsops.c */
    afs_int32 C_afs_swapvp;	/* afs_vfsops.c */
    afs_int32 C_afs_AddMarinerName;	/* afs_vnodeops.c */
    afs_int32 C_afs_setpag;	/* afs_vnodeops.c */
    afs_int32 C_genpag;		/* afs_vnodeops.c */
    afs_int32 C_getpag;		/* afs_vnodeops.c */
    afs_int32 C_afs_GetMariner;	/* afs_vnodeops.c */
    afs_int32 C_afs_badop;	/* afs_vnodeops.c */
    afs_int32 C_afs_index;	/* afs_vnodeops.c */
    afs_int32 C_afs_noop;	/* afs_vnodeops.c */
    afs_int32 C_afs_open;	/* afs_vnodeops.c */
    afs_int32 C_afs_closex;	/* afs_vnodeops.c */
    afs_int32 C_afs_close;	/* afs_vnodeops.c */
    afs_int32 C_afs_MemWrite;	/* afs_vnodeops.c */
    afs_int32 C_afs_write;	/* afs_vnodeops.c */
    afs_int32 C_afs_UFSWrite;	/* afs_vnodeops.c */
    afs_int32 C_afs_rdwr;	/* afs_vnodeops.c */
    afs_int32 C_afs_MemRead;	/* afs_vnodeops.c */
    afs_int32 C_afs_read;	/* afs_vnodeops.c */
    afs_int32 C_FIXUPSTUPIDINODE;	/* afs_vnodeops.c */
    afs_int32 C_afs_UFSRead;	/* afs_vnodeops.c */
    afs_int32 C_afs_CopyOutAttrs;	/* afs_vnodeops.c */
    afs_int32 C_afs_getattr;	/* afs_vnodeops.c */
    afs_int32 C_afs_VAttrToAS;	/* afs_vnodeops.c */
    afs_int32 C_afs_setattr;	/* afs_vnodeops.c */
    afs_int32 C_EvalMountPoint;	/* afs_vnodeops.c */
    afs_int32 C_afs_access;	/* afs_vnodeops.c */
    afs_int32 C_ENameOK;	/* afs_vnodeops.c */
    afs_int32 C_HandleAtName;	/* afs_vnodeops.c */
    afs_int32 C_getsysname;	/* afs_vnodeops.c */
    afs_int32 C_strcat;		/* afs_vnodeops.c */
    afs_int32 C_afs_lookup;	/* afs_vnodeops.c */
    afs_int32 C_afs_create;	/* afs_vnodeops.c */
    afs_int32 C_afs_LocalHero;	/* afs_vnodeops.c */
    afs_int32 C_FetchWholeEnchilada;	/* afs_vnodeops.c */
    afs_int32 C_afs_remove;	/* afs_vnodeops.c */
    afs_int32 C_afs_link;	/* afs_vnodeops.c */
    afs_int32 C_afs_rename;	/* afs_vnodeops.c */
    afs_int32 C_afs_InitReq;	/* afs_vnodeops.c */
    afs_int32 C_afs_mkdir;	/* afs_vnodeops.c */
    afs_int32 C_BlobScan;	/* afs_vnodeops.c */
    afs_int32 C_afs_rmdir;	/* afs_vnodeops.c */
    afs_int32 C_RecLen;		/* afs_vnodeops.c */
    afs_int32 C_RoundToInt;	/* afs_vnodeops.c */
    afs_int32 C_afs_readdir_with_offlist;	/* afs_vnodeops.c */
    afs_int32 C_DIRSIZ_LEN;	/* afs_vnodeops.c */
    afs_int32 C_afs_readdir_move;	/* afs_vnodeops.c */
    afs_int32 C_afs_readdir_iter;	/* afs_vnodeops.c */
    afs_int32 C_HandleFlock;	/* afs_vnodeops.c */
    afs_int32 C_afs_readdir;	/* afs_vnodeops.c */
    afs_int32 C_afs_symlink;	/* afs_vnodeops.c */
    afs_int32 C_afs_HandleLink;	/* afs_vnodeops.c */
    afs_int32 C_afs_MemHandleLink;	/* afs_vnodeops.c */
    afs_int32 C_afs_UFSHandleLink;	/* afs_vnodeops.c */
    afs_int32 C_afs_readlink;	/* afs_vnodeops.c */
    afs_int32 C_afs_fsync;	/* afs_vnodeops.c */
    afs_int32 C_afs_inactive;	/* afs_vnodeops.c */
    afs_int32 C_afs_ustrategy;	/* afs_vnodeops.c */
    afs_int32 C_afs_bread;	/* afs_vnodeops.c */
    afs_int32 C_afs_brelse;	/* afs_vnodeops.c */
    afs_int32 C_afs_bmap;	/* afs_vnodeops.c */
    afs_int32 C_afs_fid;	/* afs_vnodeops.c */
    afs_int32 C_afs_strategy;	/* afs_vnodeops.c */
    afs_int32 C_afs_FakeClose;	/* afs_vnodeops.c */
    afs_int32 C_afs_FakeOpen;	/* afs_vnodeops.c */
    afs_int32 C_afs_StoreOnLastReference;	/* afs_vnodeops.c */
    afs_int32 C_afs_GetAccessBits;	/* afs_vnodeops.c */
    afs_int32 C_afs_AccessOK;	/* afs_vnodeops.c */
    afs_int32 C_shutdown_vnodeops;	/* afs_vnodeops.c */
    afs_int32 C_afsio_copy;	/* afs_vnodeops.c */
    afs_int32 C_afsio_trim;	/* afs_vnodeops.c */
    afs_int32 C_afs_page_read;	/* afs_vnodeops.c */
    afs_int32 C_afs_page_write;	/* afs_vnodeops.c */
    afs_int32 C_afsio_skip;	/* afs_vnodeops.c */
    afs_int32 C_afs_read1dir;	/* afs_vnodeops.c */
    afs_int32 C_afs_get_groups_from_pag;	/* afs_vnodeops.c */
    afs_int32 C_afs_get_pag_from_groups;	/* afs_vnodeops.c */
    afs_int32 C_PagInCred;	/* afs_vnodeops.c */
    afs_int32 C_afs_getgroups;	/* afs_vnodeops.c */
    afs_int32 C_setpag;		/* afs_vnodeops.c */
    afs_int32 C_afs_setgroups;	/* afs_vnodeops.c */
    afs_int32 C_afs_page_in;	/* afs_vnodeops.c */
    afs_int32 C_afs_page_out;	/* afs_vnodeops.c */
    afs_int32 C_AddPag;		/* afs_vnodeops.c */
    afs_int32 C_afs_AdvanceFD;	/* afs_vnodeops.c */
    afs_int32 C_afs_lockf;	/* afs_vnodeops.c */
    afs_int32 C_afs_xsetgroups;	/* afs_vnodeops.c */
    afs_int32 C_afs_nlinks;	/* afs_vnodeops.c */
    afs_int32 C_DoLockWarning;	/* afs_vnodeops.c */
    afs_int32 C_afs_lockctl;	/* afs_vnodeops.c */
    afs_int32 C_afs_xflock;	/* afs_vnodeops.c */
    afs_int32 C_PSetSPrefs;	/* afs_pioctl.c */
    afs_int32 C_PGetSPrefs;	/* afs_pioctl.c */
    afs_int32 C_afs_warn;	/* afs_resource.c */
    afs_int32 C_afs_warnuser;	/* afs_resource.c */
    afs_int32 C_afs_pagein;	/* afs_hp_subr.c */
    afs_int32 C_afs_pageout;	/* afs_hp_subr.c */
    afs_int32 C_afs_hp_strategy;	/* afs_hp_subr.c */
    afs_int32 C_PGetCPrefs;	/* afs_pioctl.c */
    afs_int32 C_PSetCPrefs;	/* afs_pioctl.c */
    afs_int32 C_SRXAFSCB_WhoAreYou;	/* afs_callback.c */
    afs_int32 C_afs_DiscardDCache;	/* afs_dcache.c */
    afs_int32 C_afs_FreeDiscardedDCache;	/* afs_dcache.c */
    afs_int32 C_afs_MaybeFreeDiscardedDCache;	/* afs_dcache.c */
    afs_int32 C_PFlushMount;	/* afs_pioctl.c */
    afs_int32 C_SRXAFSCB_GetServerPrefs;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetCellServDB;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetLocalCell;	/* afs_callback.c */
    afs_int32 C_afs_MarshallCacheConfig;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetCacheConfig;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetCE64;	/* afs_callback.c */
    afs_int32 C_SRXAFSCB_GetCellByNum;	/* afs_callback.c */
    afs_int32 C_PGetCapabilities;	/* afs_pioctl.c */
    afs_int32 C_PGetTokensNew;	/* afs_pioctl.c */
    afs_int32 C_PSetTokensNew;	/* afs_pioctl.c */    
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

#define AFS_STATS_NUM_FS_RPC_OPS		29

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
    osi_timeval_t sumTime;	/*Sum of sample timings */
    osi_timeval_t sqrTime;	/*Sum of squares of sample timings */
    osi_timeval_t minTime;	/*Minimum timing value observed */
    osi_timeval_t maxTime;	/*Minimum timing value observed */
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
    osi_timeval_t sumTime;	/*Sum of timing values */
    osi_timeval_t sqrTime;	/*Sum of squares of timing values */
    osi_timeval_t minTime;	/*Minimum xfer time recorded */
    osi_timeval_t maxTime;	/*Maximum xfer time recorded */
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
