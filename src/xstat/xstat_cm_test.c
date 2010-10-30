/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Description:
 *	Test of the xstat_cm module.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include "xstat_cm.h"		/*Interface for xstat_cm module */
#include <afs/cmd.h>		/*Command line interpreter */
#include <time.h>
#include <string.h>
#include <afs/afsutil.h>

/*
 * Command line parameter indices.
 *	P_CM_NAMES : List of CacheManager names.
 *	P_COLL_IDS : List of collection IDs to pick up.
 *	P_ONESHOT  : Are we gathering exactly one round of data?
 *	P_DEBUG    : Enable debugging output?
 */
#define P_CM_NAMES 	0
#define P_COLL_IDS 	1
#define P_ONESHOT  	2
#define P_FREQUENCY 	3
#define P_PERIOD 	4
#define P_DEBUG    	5

/*
 * Private globals.
 */
static int debugging_on = 0;	/*Are we debugging? */
static int one_shot = 0;	/*Single round of data collection? */

static char *fsOpNames[] = {
    "FetchData",
    "FetchACL",
    "FetchStatus",
    "StoreData",
    "StoreACL",
    "StoreStatus",
    "RemoveFile",
    "CreateFile",
    "Rename",
    "Symlink",
    "Link",
    "MakeDir",
    "RemoveDir",
    "SetLock",
    "ExtendLock",
    "ReleaseLock",
    "GetStatistics",
    "GiveUpCallbacks",
    "GetVolumeInfo",
    "GetVolumeStatus",
    "SetVolumeStatus",
    "GetRootVolume",
    "CheckToken",
    "GetTime",
    "NGetVolumeInfo",
    "BulkStatus",
    "XStatsVersion",
    "GetXStats",
    "XLookup"
};

static char *cmOpNames[] = {
    "CallBack",
    "InitCallBackState",
    "Probe",
    "GetLock",
    "GetCE",
    "XStatsVersion",
    "GetXStats"
};

static char *xferOpNames[] = {
    "FetchData",
    "StoreData"
};


/*------------------------------------------------------------------------
 * PrintCallInfo
 *
 * Description:
 *	Print out the AFSCB_XSTATSCOLL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintCallInfo(void)
{				/*PrintCallInfo */

    int i;		/*Loop variable */
    int numInt32s;		/*# int32words returned */
    afs_int32 *currInt32;	/*Ptr to current afs_int32 value */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;
    /*
     * Just print out the results of the particular probe.
     */
    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    currInt32 = (afs_int32 *) (xstat_cm_Results.data.AFSCB_CollData_val);
    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Poll %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    if (debugging_on)
	printf("\n[%u entries returned at %" AFS_PTR_FMT "]\n\n", numInt32s, currInt32);

    for (i = 0; i < numInt32s; i++)
	printf("%u ", *currInt32++);
    printf("\n");


}				/*PrintCallInfo */

/* Print detailed functional call statistics */

void
print_cmCallStats(void)
{
    char *printableTime;	/*Ptr to printable time string */
    struct afs_CMStats *cmp;
    time_t probeTime = xstat_cm_Results.probeTime;

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    cmp = (struct afs_CMStats *)(xstat_cm_Results.data.AFSCB_CollData_val);

    if (xstat_cm_Results.data.AFSCB_CollData_len != sizeof(struct afs_CMStats))
	printf("Data sets differ in size. Is cache manager a different version?");

    printf("\t%10u afs_init\n", cmp->callInfo.C_afs_init);
    printf("\t%10u gop_rdwr\n", cmp->callInfo.C_gop_rdwr);
    printf("\t%10u aix_gnode_rele\n", cmp->callInfo.C_aix_gnode_rele);
    printf("\t%10u gettimeofday\n", cmp->callInfo.C_gettimeofday);
    printf("\t%10u m_cpytoc\n", cmp->callInfo.C_m_cpytoc);
    printf("\t%10u aix_vattr_null\n", cmp->callInfo.C_aix_vattr_null);
    printf("\t%10u afs_gn_ftrunc\n", cmp->callInfo.C_afs_gn_ftrunc);
    printf("\t%10u afs_gn_rdwr\n", cmp->callInfo.C_afs_gn_rdwr);
    printf("\t%10u afs_gn_ioctl\n", cmp->callInfo.C_afs_gn_ioctl);
    printf("\t%10u afs_gn_locktl\n", cmp->callInfo.C_afs_gn_lockctl);
    printf("\t%10u afs_gn_readlink\n", cmp->callInfo.C_afs_gn_readlink);
    printf("\t%10u afs_gn_readdir\n", cmp->callInfo.C_afs_gn_readdir);
    printf("\t%10u afs_gn_select\n", cmp->callInfo.C_afs_gn_select);
    printf("\t%10u afs_gn_strategy\n", cmp->callInfo.C_afs_gn_strategy);
    printf("\t%10u afs_gn_symlink\n", cmp->callInfo.C_afs_gn_symlink);
    printf("\t%10u afs_gn_revoke\n", cmp->callInfo.C_afs_gn_revoke);
    printf("\t%10u afs_gn_link\n", cmp->callInfo.C_afs_gn_link);
    printf("\t%10u afs_gn_mkdir\n", cmp->callInfo.C_afs_gn_mkdir);
    printf("\t%10u afs_gn_mknod\n", cmp->callInfo.C_afs_gn_mknod);
    printf("\t%10u afs_gn_remove\n", cmp->callInfo.C_afs_gn_remove);
    printf("\t%10u afs_gn_rename\n", cmp->callInfo.C_afs_gn_rename);
    printf("\t%10u afs_gn_rmdir\n", cmp->callInfo.C_afs_gn_rmdir);
    printf("\t%10u afs_gn_fid\n", cmp->callInfo.C_afs_gn_fid);
    printf("\t%10u afs_gn_lookup\n", cmp->callInfo.C_afs_gn_lookup);
    printf("\t%10u afs_gn_open\n", cmp->callInfo.C_afs_gn_open);
    printf("\t%10u afs_gn_create\n", cmp->callInfo.C_afs_gn_create);
    printf("\t%10u afs_gn_hold\n", cmp->callInfo.C_afs_gn_hold);
    printf("\t%10u afs_gn_rele\n", cmp->callInfo.C_afs_gn_rele);
    printf("\t%10u afs_gn_unmap\n", cmp->callInfo.C_afs_gn_unmap);
    printf("\t%10u afs_gn_access\n", cmp->callInfo.C_afs_gn_access);
    printf("\t%10u afs_gn_getattr\n", cmp->callInfo.C_afs_gn_getattr);
    printf("\t%10u afs_gn_setattr\n", cmp->callInfo.C_afs_gn_setattr);
    printf("\t%10u afs_gn_fclear\n", cmp->callInfo.C_afs_gn_fclear);
    printf("\t%10u afs_gn_fsync\n", cmp->callInfo.C_afs_gn_fsync);
    printf("\t%10u phash\n", cmp->callInfo.C_pHash);
    printf("\t%10u DInit\n", cmp->callInfo.C_DInit);
    printf("\t%10u DRead\n", cmp->callInfo.C_DRead);
    printf("\t%10u FixupBucket\n", cmp->callInfo.C_FixupBucket);
    printf("\t%10u afs_newslot\n", cmp->callInfo.C_afs_newslot);
    printf("\t%10u DRelease\n", cmp->callInfo.C_DRelease);
    printf("\t%10u DFlush\n", cmp->callInfo.C_DFlush);
    printf("\t%10u DFlushEntry\n", cmp->callInfo.C_DFlushEntry);
    printf("\t%10u DVOffset\n", cmp->callInfo.C_DVOffset);
    printf("\t%10u DZap\n", cmp->callInfo.C_DZap);
    printf("\t%10u DNew\n", cmp->callInfo.C_DNew);
    printf("\t%10u afs_RemoveVCB\n", cmp->callInfo.C_afs_RemoveVCB);
    printf("\t%10u afs_NewVCache\n", cmp->callInfo.C_afs_NewVCache);
    printf("\t%10u afs_FlushActiveVcaches\n",
	   cmp->callInfo.C_afs_FlushActiveVcaches);
    printf("\t%10u afs_VerifyVCache\n", cmp->callInfo.C_afs_VerifyVCache);
    printf("\t%10u afs_WriteVCache\n", cmp->callInfo.C_afs_WriteVCache);
    printf("\t%10u afs_GetVCache\n", cmp->callInfo.C_afs_GetVCache);
    printf("\t%10u afs_StuffVcache\n", cmp->callInfo.C_afs_StuffVcache);
    printf("\t%10u afs_FindVCache\n", cmp->callInfo.C_afs_FindVCache);
    printf("\t%10u afs_PutDCache\n", cmp->callInfo.C_afs_PutDCache);
    printf("\t%10u afs_PutVCache\n", cmp->callInfo.C_afs_PutVCache);
    printf("\t%10u CacheStoreProc\n", cmp->callInfo.C_CacheStoreProc);
    printf("\t%10u afs_FindDcache\n", cmp->callInfo.C_afs_FindDCache);
    printf("\t%10u afs_TryToSmush\n", cmp->callInfo.C_afs_TryToSmush);
    printf("\t%10u afs_AdjustSize\n", cmp->callInfo.C_afs_AdjustSize);
    printf("\t%10u afs_CheckSize\n", cmp->callInfo.C_afs_CheckSize);
    printf("\t%10u afs_StoreWarn\n", cmp->callInfo.C_afs_StoreWarn);
    printf("\t%10u CacheFetchProc\n", cmp->callInfo.C_CacheFetchProc);
    printf("\t%10u UFS_CacheStoreProc\n", cmp->callInfo.C_UFS_CacheStoreProc);
    printf("\t%10u UFS_CacheFetchProc\n", cmp->callInfo.C_UFS_CacheFetchProc);
    printf("\t%10u afs_GetDCache\n", cmp->callInfo.C_afs_GetDCache);
    printf("\t%10u afs_SimpleVStat\n", cmp->callInfo.C_afs_SimpleVStat);
    printf("\t%10u afs_ProcessFS\n", cmp->callInfo.C_afs_ProcessFS);
    printf("\t%10u afs_InitCacheInfo\n", cmp->callInfo.C_afs_InitCacheInfo);
    printf("\t%10u afs_InitVolumeInfo\n", cmp->callInfo.C_afs_InitVolumeInfo);
    printf("\t%10u afs_InitCacheFile\n", cmp->callInfo.C_afs_InitCacheFile);
    printf("\t%10u afs_CacheInit\n", cmp->callInfo.C_afs_CacheInit);
    printf("\t%10u afs_GetDSlot\n", cmp->callInfo.C_afs_GetDSlot);
    printf("\t%10u afs_WriteThroughDSlots\n",
	   cmp->callInfo.C_afs_WriteThroughDSlots);
    printf("\t%10u afs_MemGetDSlot\n", cmp->callInfo.C_afs_MemGetDSlot);
    printf("\t%10u afs_UFSGetDSlot\n", cmp->callInfo.C_afs_UFSGetDSlot);
    printf("\t%10u afs_StoreDCache\n", cmp->callInfo.C_afs_StoreDCache);
    printf("\t%10u afs_StoreMini\n", cmp->callInfo.C_afs_StoreMini);
    printf("\t%10u afs_StoreAllSegments\n",
	   cmp->callInfo.C_afs_StoreAllSegments);
    printf("\t%10u afs_InvalidateAllSegments\n",
	   cmp->callInfo.C_afs_InvalidateAllSegments);
    printf("\t%10u afs_TruncateAllSegments\n",
	   cmp->callInfo.C_afs_TruncateAllSegments);
    printf("\t%10u afs_CheckVolSync\n", cmp->callInfo.C_afs_CheckVolSync);
    printf("\t%10u afs_wakeup\n", cmp->callInfo.C_afs_wakeup);
    printf("\t%10u afs_CFileOpen\n", cmp->callInfo.C_afs_CFileOpen);
    printf("\t%10u afs_CFileTruncate\n", cmp->callInfo.C_afs_CFileTruncate);
    printf("\t%10u afs_GetDownD\n", cmp->callInfo.C_afs_GetDownD);
    printf("\t%10u afs_WriteDCache\n", cmp->callInfo.C_afs_WriteDCache);
    printf("\t%10u afs_FlushDCache\n", cmp->callInfo.C_afs_FlushDCache);
    printf("\t%10u afs_GetDownDSlot\n", cmp->callInfo.C_afs_GetDownDSlot);
    printf("\t%10u afs_FlushVCache\n", cmp->callInfo.C_afs_FlushVCache);
    printf("\t%10u afs_GetDownV\n", cmp->callInfo.C_afs_GetDownV);
    printf("\t%10u afs_QueueVCB\n", cmp->callInfo.C_afs_QueueVCB);
    printf("\t%10u afs_call\n", cmp->callInfo.C_afs_call);
    printf("\t%10u afs_syscall_call\n", cmp->callInfo.C_afs_syscall_call);
    printf("\t%10u afs_syscall_icreate\n",
	   cmp->callInfo.C_afs_syscall_icreate);
    printf("\t%10u afs_syscall_iopen\n", cmp->callInfo.C_afs_syscall_iopen);
    printf("\t%10u afs_syscall_iincdec\n",
	   cmp->callInfo.C_afs_syscall_iincdec);
    printf("\t%10u afs_syscall_ireadwrite\n",
	   cmp->callInfo.C_afs_syscall_ireadwrite);
    printf("\t%10u afs_syscall\n", cmp->callInfo.C_afs_syscall);
    printf("\t%10u lpioctl\n", cmp->callInfo.C_lpioctl);
    printf("\t%10u lsetpag\n", cmp->callInfo.C_lsetpag);
    printf("\t%10u afs_CheckInit\n", cmp->callInfo.C_afs_CheckInit);
    printf("\t%10u ClearCallback\n", cmp->callInfo.C_ClearCallBack);
    printf("\t%10u SRXAFSCB_GetCE\n", cmp->callInfo.C_SRXAFSCB_GetCE);
    printf("\t%10u SRXAFSCB_GetLock\n", cmp->callInfo.C_SRXAFSCB_GetLock);
    printf("\t%10u SRXAFSCB_CallBack\n", cmp->callInfo.C_SRXAFSCB_CallBack);
    printf("\t%10u SRXAFSCB_InitCallBackState\n",
	   cmp->callInfo.C_SRXAFSCB_InitCallBackState);
    printf("\t%10u SRXAFSCB_Probe\n", cmp->callInfo.C_SRXAFSCB_Probe);
    printf("\t%10u afs_Chunk\n", cmp->callInfo.C_afs_Chunk);
    printf("\t%10u afs_ChunkBase\n", cmp->callInfo.C_afs_ChunkBase);
    printf("\t%10u afs_ChunkOffset\n", cmp->callInfo.C_afs_ChunkOffset);
    printf("\t%10u afs_ChunkSize\n", cmp->callInfo.C_afs_ChunkSize);
    printf("\t%10u afs_ChunkToBase\n", cmp->callInfo.C_afs_ChunkToBase);
    printf("\t%10u afs_ChunkToSize\n", cmp->callInfo.C_afs_ChunkToSize);
    printf("\t%10u afs_SetChunkSize\n", cmp->callInfo.C_afs_SetChunkSize);
    printf("\t%10u afs_config\n", cmp->callInfo.C_afs_config);
    printf("\t%10u mem_freebytes\n", cmp->callInfo.C_mem_freebytes);
    printf("\t%10u mem_getbytes\n", cmp->callInfo.C_mem_getbytes);
    printf("\t%10u afs_Daemon\n", cmp->callInfo.C_afs_Daemon);
    printf("\t%10u afs_CheckRootVolume\n",
	   cmp->callInfo.C_afs_CheckRootVolume);
    printf("\t%10u BPath\n", cmp->callInfo.C_BPath);
    printf("\t%10u BPrefetch\n", cmp->callInfo.C_BPrefetch);
    printf("\t%10u BStore\n", cmp->callInfo.C_BStore);
    printf("\t%10u afs_BBusy\n", cmp->callInfo.C_afs_BBusy);
    printf("\t%10u afs_BQueue\n", cmp->callInfo.C_afs_BQueue);
    printf("\t%10u afs_BRelease\n", cmp->callInfo.C_afs_BRelease);
    printf("\t%10u afs_BackgroundDaemon\n",
	   cmp->callInfo.C_afs_BackgroundDaemon);
    printf("\t%10u exporter_add\n", cmp->callInfo.C_exporter_add);
    printf("\t%10u exporter_find\n", cmp->callInfo.C_exporter_find);
    printf("\t%10u afs_gfs_kalloc\n", cmp->callInfo.C_afs_gfs_kalloc);
    printf("\t%10u afs_gfs_kfree\n", cmp->callInfo.C_afs_gfs_kfree);
    printf("\t%10u gop_lookupname\n", cmp->callInfo.C_gop_lookupname);
    printf("\t%10u afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10u gfs_vattr_null\n", cmp->callInfo.C_gfs_vattr_null);
    printf("\t%10u afs_lock\n", cmp->callInfo.C_afs_lock);
    printf("\t%10u afs_unlock\n", cmp->callInfo.C_afs_unlock);
    printf("\t%10u afs_update\n", cmp->callInfo.C_afs_update);
    printf("\t%10u afs_gclose\n", cmp->callInfo.C_afs_gclose);
    printf("\t%10u afs_gopen\n", cmp->callInfo.C_afs_gopen);
    printf("\t%10u afs_greadlink\n", cmp->callInfo.C_afs_greadlink);
    printf("\t%10u afs_select\n", cmp->callInfo.C_afs_select);
    printf("\t%10u afs_gbmap\n", cmp->callInfo.C_afs_gbmap);
    printf("\t%10u afs_getfsdata\n", cmp->callInfo.C_afs_getfsdata);
    printf("\t%10u afs_gsymlink\n", cmp->callInfo.C_afs_gsymlink);
    printf("\t%10u afs_namei\n", cmp->callInfo.C_afs_namei);
    printf("\t%10u afs_gmount\n", cmp->callInfo.C_afs_gmount);
    printf("\t%10u afs_gget\n", cmp->callInfo.C_afs_gget);
    printf("\t%10u afs_glink\n", cmp->callInfo.C_afs_glink);
    printf("\t%10u afs_gmkdir\n", cmp->callInfo.C_afs_gmkdir);
    printf("\t%10u afs_unlink\n", cmp->callInfo.C_afs_unlink);
    printf("\t%10u afs_grmdir\n", cmp->callInfo.C_afs_grmdir);
    printf("\t%10u afs_makenode\n", cmp->callInfo.C_afs_makenode);
    printf("\t%10u afs_grename\n", cmp->callInfo.C_afs_grename);
    printf("\t%10u afs_rele\n", cmp->callInfo.C_afs_rele);
    printf("\t%10u afs_syncgp\n", cmp->callInfo.C_afs_syncgp);
    printf("\t%10u afs_getval\n", cmp->callInfo.C_afs_getval);
    printf("\t%10u afs_trunc\n", cmp->callInfo.C_afs_trunc);
    printf("\t%10u afs_rwgp\n", cmp->callInfo.C_afs_rwgp);
    printf("\t%10u afs_stat\n", cmp->callInfo.C_afs_stat);
    printf("\t%10u afsc_link\n", cmp->callInfo.C_afsc_link);
    printf("\t%10u afs_vfs_mount\n", cmp->callInfo.C_afs_vfs_mount);
    printf("\t%10u afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10u iopen\n", cmp->callInfo.C_iopen);
    printf("\t%10u idec\n", cmp->callInfo.C_idec);
    printf("\t%10u iinc\n", cmp->callInfo.C_iinc);
    printf("\t%10u ireadwrite\n", cmp->callInfo.C_ireadwrite);
    printf("\t%10u iread\n", cmp->callInfo.C_iread);
    printf("\t%10u iwrite\n", cmp->callInfo.C_iwrite);
    printf("\t%10u iforget\n", cmp->callInfo.C_iforget);
    printf("\t%10u icreate\n", cmp->callInfo.C_icreate);
    printf("\t%10u igetinode\n", cmp->callInfo.C_igetinode);
    printf("\t%10u osi_SleepR\n", cmp->callInfo.C_osi_SleepR);
    printf("\t%10u osi_SleepS\n", cmp->callInfo.C_osi_SleepS);
    printf("\t%10u osi_SleepW\n", cmp->callInfo.C_osi_SleepW);
    printf("\t%10u osi_Sleep\n", cmp->callInfo.C_osi_Sleep);
    printf("\t%10u afs_LookupMCE\n", cmp->callInfo.C_afs_LookupMCE);
    printf("\t%10u afs_MemReadBlk\n", cmp->callInfo.C_afs_MemReadBlk);
    printf("\t%10u afs_MemReadUIO\n", cmp->callInfo.C_afs_MemReadUIO);
    printf("\t%10u afs_MemWriteBlk\n", cmp->callInfo.C_afs_MemWriteBlk);
    printf("\t%10u afs_MemWriteUIO\n", cmp->callInfo.C_afs_MemWriteUIO);
    printf("\t%10u afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10u afs_MemCacheFetchProc\n",
	   cmp->callInfo.C_afs_MemCacheFetchProc);
    printf("\t%10u afs_MemCacheTruncate\n",
	   cmp->callInfo.C_afs_MemCacheTruncate);
    printf("\t%10u afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10u afs_GetNfsClientPag\n",
	   cmp->callInfo.C_afs_GetNfsClientPag);
    printf("\t%10u afs_FindNfsClientPag\n",
	   cmp->callInfo.C_afs_FindNfsClientPag);
    printf("\t%10u afs_PutNfsClientPag\n",
	   cmp->callInfo.C_afs_PutNfsClientPag);
    printf("\t%10u afs_nfsclient_reqhandler\n",
	   cmp->callInfo.C_afs_nfsclient_reqhandler);
    printf("\t%10u afs_nfsclient_GC\n", cmp->callInfo.C_afs_nfsclient_GC);
    printf("\t%10u afs_nfsclient_hold\n", cmp->callInfo.C_afs_nfsclient_hold);
    printf("\t%10u afs_nfsclient_stats\n",
	   cmp->callInfo.C_afs_nfsclient_stats);
    printf("\t%10u afs_nfsclient_sysname\n",
	   cmp->callInfo.C_afs_nfsclient_sysname);
    printf("\t%10u afs_rfs_dispatch\n", cmp->callInfo.C_afs_rfs_dispatch);
    printf("\t%10u afs_nfs2afscall\n", cmp->callInfo.C_Nfs2AfsCall);
    printf("\t%10u afs_sun_xuntext\n", cmp->callInfo.C_afs_sun_xuntext);
    printf("\t%10u osi_Active\n", cmp->callInfo.C_osi_Active);
    printf("\t%10u osi_FlushPages\n", cmp->callInfo.C_osi_FlushPages);
    printf("\t%10u osi_FlushText\n", cmp->callInfo.C_osi_FlushText);
    printf("\t%10u osi_CallProc\n", cmp->callInfo.C_osi_CallProc);
    printf("\t%10u osi_CancelProc\n", cmp->callInfo.C_osi_CancelProc);
    printf("\t%10u osi_Invisible\n", cmp->callInfo.C_osi_Invisible);
    printf("\t%10u osi_Time\n", cmp->callInfo.C_osi_Time);
    printf("\t%10u osi_Alloc\n", cmp->callInfo.C_osi_Alloc);
    printf("\t%10u osi_SetTime\n", cmp->callInfo.C_osi_SetTime);
    printf("\t%10u osi_Dump\n", cmp->callInfo.C_osi_Dump);
    printf("\t%10u osi_Free\n", cmp->callInfo.C_osi_Free);
    printf("\t%10u osi_UFSOpen\n", cmp->callInfo.C_osi_UFSOpen);
    printf("\t%10u osi_Close\n", cmp->callInfo.C_osi_Close);
    printf("\t%10u osi_Stat\n", cmp->callInfo.C_osi_Stat);
    printf("\t%10u osi_Truncate\n", cmp->callInfo.C_osi_Truncate);
    printf("\t%10u osi_Read\n", cmp->callInfo.C_osi_Read);
    printf("\t%10u osi_Write\n", cmp->callInfo.C_osi_Write);
    printf("\t%10u osi_MapStrategy\n", cmp->callInfo.C_osi_MapStrategy);
    printf("\t%10u osi_AllocLargeSpace\n",
	   cmp->callInfo.C_osi_AllocLargeSpace);
    printf("\t%10u osi_FreeLargeSpace\n", cmp->callInfo.C_osi_FreeLargeSpace);
    printf("\t%10u osi_AllocSmallSpace\n",
	   cmp->callInfo.C_osi_AllocSmallSpace);
    printf("\t%10u osi_FreeSmallSpace\n", cmp->callInfo.C_osi_FreeSmallSpace);
    printf("\t%10u osi_CloseToTheEdge\n", cmp->callInfo.C_osi_CloseToTheEdge);
    printf("\t%10u osi_xgreedy\n", cmp->callInfo.C_osi_xgreedy);
    printf("\t%10u osi_FreeSocket\n", cmp->callInfo.C_osi_FreeSocket);
    printf("\t%10u osi_NewSocket\n", cmp->callInfo.C_osi_NewSocket);
    printf("\t%10u osi_NetSend\n", cmp->callInfo.C_osi_NetSend);
    printf("\t%10u WaitHack\n", cmp->callInfo.C_WaitHack);
    printf("\t%10u osi_CancelWait\n", cmp->callInfo.C_osi_CancelWait);
    printf("\t%10u osi_Wakeup\n", cmp->callInfo.C_osi_Wakeup);
    printf("\t%10u osi_Wait\n", cmp->callInfo.C_osi_Wait);
    printf("\t%10u dirp_Read\n", cmp->callInfo.C_dirp_Read);
    printf("\t%10u dirp_Cpy\n", cmp->callInfo.C_dirp_Cpy);
    printf("\t%10u dirp_Eq\n", cmp->callInfo.C_dirp_Eq);
    printf("\t%10u dirp_Write\n", cmp->callInfo.C_dirp_Write);
    printf("\t%10u dirp_Zap\n", cmp->callInfo.C_dirp_Zap);
    printf("\t%10u afs_ioctl\n", cmp->callInfo.C_afs_ioctl);
    printf("\t%10u handleIoctl\n", cmp->callInfo.C_HandleIoctl);
    printf("\t%10u afs_xioctl\n", cmp->callInfo.C_afs_xioctl);
    printf("\t%10u afs_pioctl\n", cmp->callInfo.C_afs_pioctl);
    printf("\t%10u HandlePioctl\n", cmp->callInfo.C_HandlePioctl);
    printf("\t%10u PGetVolumeStatus\n", cmp->callInfo.C_PGetVolumeStatus);
    printf("\t%10u PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10u PFlush\n", cmp->callInfo.C_PFlush);
    printf("\t%10u PFlushVolumeData\n", cmp->callInfo.C_PFlushVolumeData);
    printf("\t%10u PNewStatMount\n", cmp->callInfo.C_PNewStatMount);
    printf("\t%10u PGetTokens\n", cmp->callInfo.C_PGetTokens);
    printf("\t%10u PSetTokens\n", cmp->callInfo.C_PSetTokens);
    printf("\t%10u PUnlog\n", cmp->callInfo.C_PUnlog);
    printf("\t%10u PCheckServers\n", cmp->callInfo.C_PCheckServers);
    printf("\t%10u PCheckAuth\n", cmp->callInfo.C_PCheckAuth);
    printf("\t%10u PCheckVolNames\n", cmp->callInfo.C_PCheckVolNames);
    printf("\t%10u PFindVolume\n", cmp->callInfo.C_PFindVolume);
    printf("\t%10u Prefetch\n", cmp->callInfo.C_Prefetch);
    printf("\t%10u PGetCacheSize\n", cmp->callInfo.C_PGetCacheSize);
    printf("\t%10u PSetCacheSize\n", cmp->callInfo.C_PSetCacheSize);
    printf("\t%10u PSetSysName\n", cmp->callInfo.C_PSetSysName);
    printf("\t%10u PExportAfs\n", cmp->callInfo.C_PExportAfs);
    printf("\t%10u HandleClientContext\n",
	   cmp->callInfo.C_HandleClientContext);
    printf("\t%10u PViceAccess\n", cmp->callInfo.C_PViceAccess);
    printf("\t%10u PRemoveCallBack\n", cmp->callInfo.C_PRemoveCallBack);
    printf("\t%10u PRemoveMount\n", cmp->callInfo.C_PRemoveMount);
    printf("\t%10u PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10u PListCells\n", cmp->callInfo.C_PListCells);
    printf("\t%10u PNewCell\n", cmp->callInfo.C_PNewCell);
    printf("\t%10u PGetUserCell\n", cmp->callInfo.C_PGetUserCell);
    printf("\t%10u PGetCellStatus\n", cmp->callInfo.C_PGetCellStatus);
    printf("\t%10u PSetCellStatus\n", cmp->callInfo.C_PSetCellStatus);
    printf("\t%10u PVenusLogging\n", cmp->callInfo.C_PVenusLogging);
    printf("\t%10u PGetAcl\n", cmp->callInfo.C_PGetAcl);
    printf("\t%10u PGetFID\n", cmp->callInfo.C_PGetFID);
    printf("\t%10u PSetAcl\n", cmp->callInfo.C_PSetAcl);
    printf("\t%10u PGetFileCell\n", cmp->callInfo.C_PGetFileCell);
    printf("\t%10u PGetWSCell\n", cmp->callInfo.C_PGetWSCell);
    printf("\t%10u PGetSPrefs\n", cmp->callInfo.C_PGetSPrefs);
    printf("\t%10u PSetSPrefs\n", cmp->callInfo.C_PSetSPrefs);
    printf("\t%10u afs_ResetAccessCache\n",
	   cmp->callInfo.C_afs_ResetAccessCache);
    printf("\t%10u afs_FindUser\n", cmp->callInfo.C_afs_FindUser);
    printf("\t%10u afs_GetUser\n", cmp->callInfo.C_afs_GetUser);
    printf("\t%10u afs_GCUserData\n", cmp->callInfo.C_afs_GCUserData);
    printf("\t%10u afs_PutUser\n", cmp->callInfo.C_afs_PutUser);
    printf("\t%10u afs_SetPrimary\n", cmp->callInfo.C_afs_SetPrimary);
    printf("\t%10u afs_ResetUserConns\n", cmp->callInfo.C_afs_ResetUserConns);
    printf("\t%10u afs_RemoveUserConns\n", cmp->callInfo.C_RemoveUserConns);
    printf("\t%10u afs_ResourceInit\n", cmp->callInfo.C_afs_ResourceInit);
    printf("\t%10u afs_GetCell\n", cmp->callInfo.C_afs_GetCell);
    printf("\t%10u afs_GetCellByIndex\n", cmp->callInfo.C_afs_GetCellByIndex);
    printf("\t%10u afs_GetCellByName\n", cmp->callInfo.C_afs_GetCellByName);
    printf("\t%10u afs_GetRealCellByIndex\n",
	   cmp->callInfo.C_afs_GetRealCellByIndex);
    printf("\t%10u afs_NewCell\n", cmp->callInfo.C_afs_NewCell);
    printf("\t%10u CheckVLDB\n", cmp->callInfo.C_CheckVLDB);
    printf("\t%10u afs_GetVolume\n", cmp->callInfo.C_afs_GetVolume);
    printf("\t%10u afs_PutVolume\n", cmp->callInfo.C_afs_PutVolume);
    printf("\t%10u afs_GetVolumeByName\n",
	   cmp->callInfo.C_afs_GetVolumeByName);
    printf("\t%10u afs_random\n", cmp->callInfo.C_afs_random);
    printf("\t%10u InstallVolumeEntry\n", cmp->callInfo.C_InstallVolumeEntry);
    printf("\t%10u InstallVolumeInfo\n", cmp->callInfo.C_InstallVolumeInfo);
    printf("\t%10u afs_ResetVolumeInfo\n",
	   cmp->callInfo.C_afs_ResetVolumeInfo);
    printf("\t%10u afs_FindServer\n", cmp->callInfo.C_afs_FindServer);
    printf("\t%10u afs_GetServer\n", cmp->callInfo.C_afs_GetServer);
    printf("\t%10u afs_SortServers\n", cmp->callInfo.C_afs_SortServers);
    printf("\t%10u afs_CheckServers\n", cmp->callInfo.C_afs_CheckServers);
    printf("\t%10u ServerDown\n", cmp->callInfo.C_ServerDown);
    printf("\t%10u afs_Conn\n", cmp->callInfo.C_afs_Conn);
    printf("\t%10u afs_PutConn\n", cmp->callInfo.C_afs_PutConn);
    printf("\t%10u afs_ConnByHost\n", cmp->callInfo.C_afs_ConnByHost);
    printf("\t%10u afs_ConnByMHosts\n", cmp->callInfo.C_afs_ConnByMHosts);
    printf("\t%10u afs_Analyze\n", cmp->callInfo.C_afs_Analyze);
    printf("\t%10u afs_CheckLocks\n", cmp->callInfo.C_afs_CheckLocks);
    printf("\t%10u CheckVLServer\n", cmp->callInfo.C_CheckVLServer);
    printf("\t%10u afs_CheckCacheResets\n",
	   cmp->callInfo.C_afs_CheckCacheResets);
    printf("\t%10u afs_CheckVolumeNames\n",
	   cmp->callInfo.C_afs_CheckVolumeNames);
    printf("\t%10u afs_CheckCode\n", cmp->callInfo.C_afs_CheckCode);
    printf("\t%10u afs_CopyError\n", cmp->callInfo.C_afs_CopyError);
    printf("\t%10u afs_FinalizeReq\n", cmp->callInfo.C_afs_FinalizeReq);
    printf("\t%10u afs_GetVolCache\n", cmp->callInfo.C_afs_GetVolCache);
    printf("\t%10u afs_GetVolSlot\n", cmp->callInfo.C_afs_GetVolSlot);
    printf("\t%10u afs_UFSGetVolSlot\n", cmp->callInfo.C_afs_UFSGetVolSlot);
    printf("\t%10u afs_MemGetVolSlot\n", cmp->callInfo.C_afs_MemGetVolSlot);
    printf("\t%10u afs_WriteVolCache\n", cmp->callInfo.C_afs_WriteVolCache);
    printf("\t%10u haveCallbacksfrom\n", cmp->callInfo.C_HaveCallBacksFrom);
    printf("\t%10u afs_getpage\n", cmp->callInfo.C_afs_getpage);
    printf("\t%10u afs_putpage\n", cmp->callInfo.C_afs_putpage);
    printf("\t%10u afs_nfsrdwr\n", cmp->callInfo.C_afs_nfsrdwr);
    printf("\t%10u afs_map\n", cmp->callInfo.C_afs_map);
    printf("\t%10u afs_cmp\n", cmp->callInfo.C_afs_cmp);
    printf("\t%10u afs_PageLeft\n", cmp->callInfo.C_afs_PageLeft);
    printf("\t%10u afs_mount\n", cmp->callInfo.C_afs_mount);
    printf("\t%10u afs_unmount\n", cmp->callInfo.C_afs_unmount);
    printf("\t%10u afs_root\n", cmp->callInfo.C_afs_root);
    printf("\t%10u afs_statfs\n", cmp->callInfo.C_afs_statfs);
    printf("\t%10u afs_sync\n", cmp->callInfo.C_afs_sync);
    printf("\t%10u afs_vget\n", cmp->callInfo.C_afs_vget);
    printf("\t%10u afs_index\n", cmp->callInfo.C_afs_index);
    printf("\t%10u afs_setpag\n", cmp->callInfo.C_afs_setpag);
    printf("\t%10u genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10u getpag\n", cmp->callInfo.C_getpag);
    printf("\t%10u genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10u afs_GetMariner\n", cmp->callInfo.C_afs_GetMariner);
    printf("\t%10u afs_AddMarinerName\n", cmp->callInfo.C_afs_AddMarinerName);
    printf("\t%10u afs_open\n", cmp->callInfo.C_afs_open);
    printf("\t%10u afs_close\n", cmp->callInfo.C_afs_close);
    printf("\t%10u afs_closex\n", cmp->callInfo.C_afs_closex);
    printf("\t%10u afs_write\n", cmp->callInfo.C_afs_write);
    printf("\t%10u afs_UFSwrite\n", cmp->callInfo.C_afs_UFSWrite);
    printf("\t%10u afs_Memwrite\n", cmp->callInfo.C_afs_MemWrite);
    printf("\t%10u afs_rdwr\n", cmp->callInfo.C_afs_rdwr);
    printf("\t%10u afs_read\n", cmp->callInfo.C_afs_read);
    printf("\t%10u afs_UFSread\n", cmp->callInfo.C_afs_UFSRead);
    printf("\t%10u afs_Memread\n", cmp->callInfo.C_afs_MemRead);
    printf("\t%10u afs_CopyOutAttrs\n", cmp->callInfo.C_afs_CopyOutAttrs);
    printf("\t%10u afs_access\n", cmp->callInfo.C_afs_access);
    printf("\t%10u afs_getattr\n", cmp->callInfo.C_afs_getattr);
    printf("\t%10u afs_setattr\n", cmp->callInfo.C_afs_setattr);
    printf("\t%10u afs_VAttrToAS\n", cmp->callInfo.C_afs_VAttrToAS);
    printf("\t%10u EvalMountPoint\n", cmp->callInfo.C_EvalMountPoint);
    printf("\t%10u afs_lookup\n", cmp->callInfo.C_afs_lookup);
    printf("\t%10u afs_create\n", cmp->callInfo.C_afs_create);
    printf("\t%10u afs_LocalHero\n", cmp->callInfo.C_afs_LocalHero);
    printf("\t%10u afs_remove\n", cmp->callInfo.C_afs_remove);
    printf("\t%10u afs_link\n", cmp->callInfo.C_afs_link);
    printf("\t%10u afs_rename\n", cmp->callInfo.C_afs_rename);
    printf("\t%10u afs_InitReq\n", cmp->callInfo.C_afs_InitReq);
    printf("\t%10u afs_mkdir\n", cmp->callInfo.C_afs_mkdir);
    printf("\t%10u afs_rmdir\n", cmp->callInfo.C_afs_rmdir);
    printf("\t%10u afs_readdir\n", cmp->callInfo.C_afs_readdir);
    printf("\t%10u afs_read1dir\n", cmp->callInfo.C_afs_read1dir);
    printf("\t%10u afs_readdir_move\n", cmp->callInfo.C_afs_readdir_move);
    printf("\t%10u afs_readdir_iter\n", cmp->callInfo.C_afs_readdir_iter);
    printf("\t%10u afs_symlink\n", cmp->callInfo.C_afs_symlink);
    printf("\t%10u afs_HandleLink\n", cmp->callInfo.C_afs_HandleLink);
    printf("\t%10u afs_MemHandleLink\n", cmp->callInfo.C_afs_MemHandleLink);
    printf("\t%10u afs_UFSHandleLink\n", cmp->callInfo.C_afs_UFSHandleLink);
    printf("\t%10u HandleFlock\n", cmp->callInfo.C_HandleFlock);
    printf("\t%10u afs_readlink\n", cmp->callInfo.C_afs_readlink);
    printf("\t%10u afs_fsync\n", cmp->callInfo.C_afs_fsync);
    printf("\t%10u afs_inactive\n", cmp->callInfo.C_afs_inactive);
    printf("\t%10u afs_ustrategy\n", cmp->callInfo.C_afs_ustrategy);
    printf("\t%10u afs_strategy\n", cmp->callInfo.C_afs_strategy);
    printf("\t%10u afs_bread\n", cmp->callInfo.C_afs_bread);
    printf("\t%10u afs_brelse\n", cmp->callInfo.C_afs_brelse);
    printf("\t%10u afs_bmap\n", cmp->callInfo.C_afs_bmap);
    printf("\t%10u afs_fid\n", cmp->callInfo.C_afs_fid);
    printf("\t%10u afs_FakeOpen\n", cmp->callInfo.C_afs_FakeOpen);
    printf("\t%10u afs_FakeClose\n", cmp->callInfo.C_afs_FakeClose);
    printf("\t%10u afs_StoreOnLastReference\n",
	   cmp->callInfo.C_afs_StoreOnLastReference);
    printf("\t%10u afs_AccessOK\n", cmp->callInfo.C_afs_AccessOK);
    printf("\t%10u afs_GetAccessBits\n", cmp->callInfo.C_afs_GetAccessBits);
    printf("\t%10u afsio_copy\n", cmp->callInfo.C_afsio_copy);
    printf("\t%10u afsio_trim\n", cmp->callInfo.C_afsio_trim);
    printf("\t%10u afsio_skip\n", cmp->callInfo.C_afsio_skip);
    printf("\t%10u afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10u afs_page_write\n", cmp->callInfo.C_afs_page_write);
    printf("\t%10u afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10u afs_get_groups_from_pag\n",
	   cmp->callInfo.C_afs_get_groups_from_pag);
    printf("\t%10u afs_get_pag_from_groups\n",
	   cmp->callInfo.C_afs_get_pag_from_groups);
    printf("\t%10u AddPag\n", cmp->callInfo.C_AddPag);
    printf("\t%10u PagInCred\n", cmp->callInfo.C_PagInCred);
    printf("\t%10u afs_getgroups\n", cmp->callInfo.C_afs_getgroups);
    printf("\t%10u afs_page_in\n", cmp->callInfo.C_afs_page_in);
    printf("\t%10u afs_page_out\n", cmp->callInfo.C_afs_page_out);
    printf("\t%10u afs_AdvanceFD\n", cmp->callInfo.C_afs_AdvanceFD);
    printf("\t%10u afs_lockf\n", cmp->callInfo.C_afs_lockf);
    printf("\t%10u afs_xsetgroups\n", cmp->callInfo.C_afs_xsetgroups);
    printf("\t%10u afs_nlinks\n", cmp->callInfo.C_afs_nlinks);
    printf("\t%10u afs_lockctl\n", cmp->callInfo.C_afs_lockctl);
    printf("\t%10u afs_xflock\n", cmp->callInfo.C_afs_xflock);
    printf("\t%10u PGetCPrefs\n", cmp->callInfo.C_PGetCPrefs);
    printf("\t%10u PSetCPrefs\n", cmp->callInfo.C_PSetCPrefs);
#ifdef	AFS_HPUX_ENV
    printf("\t%10u afs_pagein\n", cmp->callInfo.C_afs_pagein);
    printf("\t%10u afs_pageout\n", cmp->callInfo.C_afs_pageout);
    printf("\t%10u afs_hp_strategy\n", cmp->callInfo.C_afs_hp_strategy);
#endif
    printf("\t%10u PFlushMount\n", cmp->callInfo.C_PFlushMount);
    printf("\t%10u SRXAFSCB_GetServerPrefs\n",
	   cmp->callInfo.C_SRXAFSCB_GetServerPrefs);
    printf("\t%10u SRXAFSCB_GetCellServDB\n",
	   cmp->callInfo.C_SRXAFSCB_GetCellServDB);
    printf("\t%10u SRXAFSCB_GetLocalCell\n",
           cmp->callInfo.C_SRXAFSCB_GetLocalCell);
    printf("\t%10u afs_MarshallCacheConfig\n",
	   cmp->callInfo.C_afs_MarshallCacheConfig);
    printf("\t%10u SRXAFSCB_GetCacheConfig\n",
	   cmp->callInfo.C_SRXAFSCB_GetCacheConfig);
    printf("\t%10u SRXAFSCB_GetCE64\n", cmp->callInfo.C_SRXAFSCB_GetCE64);
    printf("\t%10u SRXAFSCB_GetCellByNum\n",
	   cmp->callInfo.C_SRXAFSCB_GetCellByNum);
    printf("\t%10u BPrefetchNoCache\n", cmp->callInfo.C_BPrefetchNoCache);
    printf("\t%10u afs_ReadNoCache\n", cmp->callInfo.C_afs_ReadNoCache);
}


/*------------------------------------------------------------------------
 * PrintUpDownStats
 *
 * Description:
 *	Print the up/downtime stats for the given class of server records
 *	provided.
 *
 * Arguments:
 *	a_upDownP : Ptr to the server up/down info.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintUpDownStats(struct afs_stats_SrvUpDownInfo *a_upDownP)
{				/*PrintUpDownStats */

    /*
     * First, print the simple values.
     */
    printf("\t\t%10u numTtlRecords\n", a_upDownP->numTtlRecords);
    printf("\t\t%10u numUpRecords\n", a_upDownP->numUpRecords);
    printf("\t\t%10u numDownRecords\n", a_upDownP->numDownRecords);
    printf("\t\t%10u sumOfRecordAges\n", a_upDownP->sumOfRecordAges);
    printf("\t\t%10u ageOfYoungestRecord\n", a_upDownP->ageOfYoungestRecord);
    printf("\t\t%10u ageOfOldestRecord\n", a_upDownP->ageOfOldestRecord);
    printf("\t\t%10u numDowntimeIncidents\n",
	   a_upDownP->numDowntimeIncidents);
    printf("\t\t%10u numRecordsNeverDown\n", a_upDownP->numRecordsNeverDown);
    printf("\t\t%10u maxDowntimesInARecord\n",
	   a_upDownP->maxDowntimesInARecord);
    printf("\t\t%10u sumOfDowntimes\n", a_upDownP->sumOfDowntimes);
    printf("\t\t%10u shortestDowntime\n", a_upDownP->shortestDowntime);
    printf("\t\t%10u longestDowntime\n", a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    printf("\t\tDowntime duration distribution:\n");
    printf("\t\t\t%8u: 0 min .. 10 min\n", a_upDownP->downDurations[0]);
    printf("\t\t\t%8u: 10 min .. 30 min\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8u: 30 min .. 1 hr\n", a_upDownP->downDurations[2]);
    printf("\t\t\t%8u: 1 hr .. 2 hr\n", a_upDownP->downDurations[3]);
    printf("\t\t\t%8u: 2 hr .. 4 hr\n", a_upDownP->downDurations[4]);
    printf("\t\t\t%8u: 4 hr .. 8 hr\n", a_upDownP->downDurations[5]);
    printf("\t\t\t%8u: > 8 hr\n", a_upDownP->downDurations[6]);

    printf("\t\tDowntime incident distribution:\n");
    printf("\t\t\t%8u: 0 times\n", a_upDownP->downIncidents[0]);
    printf("\t\t\t%8u: 1 time\n", a_upDownP->downIncidents[1]);
    printf("\t\t\t%8u: 2 .. 5 times\n", a_upDownP->downIncidents[2]);
    printf("\t\t\t%8u: 6 .. 10 times\n", a_upDownP->downIncidents[3]);
    printf("\t\t\t%8u: 10 .. 50 times\n", a_upDownP->downIncidents[4]);
    printf("\t\t\t%8u: > 50 times\n", a_upDownP->downIncidents[5]);

}				/*PrintUpDownStats */


/*------------------------------------------------------------------------
 * PrintOverallPerfInfo
 *
 * Description:
 *	Print out overall performance numbers.
 *
 * Arguments:
 *	a_ovP : Ptr to the overall performance numbers.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintOverallPerfInfo(struct afs_stats_CMPerf *a_ovP)
{				/*PrintOverallPerfInfo */

    printf("\t%10u numPerfCalls\n", a_ovP->numPerfCalls);

    printf("\t%10u epoch\n", a_ovP->epoch);
    printf("\t%10u numCellsVisible\n", a_ovP->numCellsVisible);
    printf("\t%10u numCellsContacted\n", a_ovP->numCellsContacted);
    printf("\t%10u dlocalAccesses\n", a_ovP->dlocalAccesses);
    printf("\t%10u vlocalAccesses\n", a_ovP->vlocalAccesses);
    printf("\t%10u dremoteAccesses\n", a_ovP->dremoteAccesses);
    printf("\t%10u vremoteAccesses\n", a_ovP->vremoteAccesses);
    printf("\t%10u cacheNumEntries\n", a_ovP->cacheNumEntries);
    printf("\t%10u cacheBlocksTotal\n", a_ovP->cacheBlocksTotal);
    printf("\t%10u cacheBlocksInUse\n", a_ovP->cacheBlocksInUse);
    printf("\t%10u cacheBlocksOrig\n", a_ovP->cacheBlocksOrig);
    printf("\t%10u cacheMaxDirtyChunks\n", a_ovP->cacheMaxDirtyChunks);
    printf("\t%10u cacheCurrDirtyChunks\n", a_ovP->cacheCurrDirtyChunks);
    printf("\t%10u dcacheHits\n", a_ovP->dcacheHits);
    printf("\t%10u vcacheHits\n", a_ovP->vcacheHits);
    printf("\t%10u dcacheMisses\n", a_ovP->dcacheMisses);
    printf("\t%10u vcacheMisses\n", a_ovP->vcacheMisses);
    printf("\t%10u cacheFilesReused\n", a_ovP->cacheFilesReused);
    printf("\t%10u vcacheXAllocs\n", a_ovP->vcacheXAllocs);
    printf("\t%10u dcacheXAllocs\n", a_ovP->dcacheXAllocs);

    printf("\t%10u bufAlloced\n", a_ovP->bufAlloced);
    printf("\t%10u bufHits\n", a_ovP->bufHits);
    printf("\t%10u bufMisses\n", a_ovP->bufMisses);
    printf("\t%10u bufFlushDirty\n", a_ovP->bufFlushDirty);

    printf("\t%10u LargeBlocksActive\n", a_ovP->LargeBlocksActive);
    printf("\t%10u LargeBlocksAlloced\n", a_ovP->LargeBlocksAlloced);
    printf("\t%10u SmallBlocksActive\n", a_ovP->SmallBlocksActive);
    printf("\t%10u SmallBlocksAlloced\n", a_ovP->SmallBlocksAlloced);
    printf("\t%10u OutStandingMemUsage\n", a_ovP->OutStandingMemUsage);
    printf("\t%10u OutStandingAllocs\n", a_ovP->OutStandingAllocs);
    printf("\t%10u CallBackAlloced\n", a_ovP->CallBackAlloced);
    printf("\t%10u CallBackFlushes\n", a_ovP->CallBackFlushes);
    printf("\t%10u CallBackLoops\n", a_ovP->cbloops);

    printf("\t%10u srvRecords\n", a_ovP->srvRecords);
    printf("\t%10u srvNumBuckets\n", a_ovP->srvNumBuckets);
    printf("\t%10u srvMaxChainLength\n", a_ovP->srvMaxChainLength);
    printf("\t%10u srvMaxChainLengthHWM\n", a_ovP->srvMaxChainLengthHWM);
    printf("\t%10u srvRecordsHWM\n", a_ovP->srvRecordsHWM);

    printf("\t%10u cacheBucket0_Discarded\n",  a_ovP->cacheBucket0_Discarded);
    printf("\t%10u cacheBucket1_Discarded\n",  a_ovP->cacheBucket1_Discarded);
    printf("\t%10u cacheBucket2_Discarded\n",  a_ovP->cacheBucket2_Discarded);

    printf("\t%10u sysName_ID\n", a_ovP->sysName_ID);

    printf("\tFile Server up/downtimes, same cell:\n");
    PrintUpDownStats(&(a_ovP->fs_UpDown[0]));

    printf("\tFile Server up/downtimes, diff cell:\n");
    PrintUpDownStats(&(a_ovP->fs_UpDown[1]));

    printf("\tVL Server up/downtimes, same cell:\n");
    PrintUpDownStats(&(a_ovP->vl_UpDown[0]));

    printf("\tVL Server up/downtimes, diff cell:\n");
    PrintUpDownStats(&(a_ovP->vl_UpDown[1]));

}				/*PrintOverallPerfInfo */


/*------------------------------------------------------------------------
 * PrintPerfInfo
 *
 * Description:
 *	Print out the AFSCB_XSTATSCOLL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintPerfInfo(void)
{				/*PrintPerfInfo */

    static afs_int32 perfInt32s = (sizeof(struct afs_stats_CMPerf) >> 2);	/*Correct # int32s to rcv */
    afs_int32 numInt32s;	/*# int32words received */
    struct afs_stats_CMPerf *perfP;	/*Ptr to performance stats */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != perfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %u, got %u\n", perfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    perfP = (struct afs_stats_CMPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_PERF_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    PrintOverallPerfInfo(perfP);

}				/*PrintPerfInfo */


/*------------------------------------------------------------------------
 * PrintOpTiming
 *
 * Description:
 *	Print out the contents of an FS RPC op timing structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing number on.
 *	a_opNames : Ptr to table of operaton names.
 *	a_opTimeP : Ptr to the op timing structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintOpTiming(int a_opIdx, char *a_opNames[],
	      struct afs_stats_opTimingData *a_opTimeP)
{				/*PrintOpTiming */

    printf
	("%15s: %u ops (%u OK); sum=%lu.%06lu, sqr=%lu.%06lu, min=%lu.%06lu, max=%lu.%06lu\n",
	 a_opNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	 (long)a_opTimeP->sumTime.tv_sec, (long)a_opTimeP->sumTime.tv_usec,
	 (long)a_opTimeP->sqrTime.tv_sec, (long)a_opTimeP->sqrTime.tv_usec,
	 (long)a_opTimeP->minTime.tv_sec, (long)a_opTimeP->minTime.tv_usec,
	 (long)a_opTimeP->maxTime.tv_sec, (long)a_opTimeP->maxTime.tv_usec);

}				/*PrintOpTiming */


/*------------------------------------------------------------------------
 * PrintXferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
 *	a_opNames : Ptr to table of operation names.
 *	a_xferP : Ptr to the data transfer structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintXferTiming(int a_opIdx, char *a_opNames[],
		struct afs_stats_xferData *a_xferP)
{				/*PrintXferTiming */

    printf
	("%s: %u xfers (%u OK), time sum=%lu.%06lu, sqr=%lu.%06lu, min=%lu.%06lu, max=%lu.%06lu\n",
	 a_opNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	 (long)a_xferP->sumTime.tv_sec, (long)a_xferP->sumTime.tv_usec,
	 (long)a_xferP->sqrTime.tv_sec, (long)a_xferP->sqrTime.tv_usec,
	 (long)a_xferP->minTime.tv_sec, (long)a_xferP->minTime.tv_usec,
	 (long)a_xferP->maxTime.tv_sec, (long)a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%u, min=%u, max=%u]\n", a_xferP->sumBytes,
	   a_xferP->minBytes, a_xferP->maxBytes);
    printf
	("\t[buckets: 0: %u, 1: %u, 2: %u, 3: %u, 4: %u, 5: %u, 6: %u, 7: %u, 8: %u]\n",
	 a_xferP->count[0], a_xferP->count[1], a_xferP->count[2],
	 a_xferP->count[3], a_xferP->count[4], a_xferP->count[5],
	 a_xferP->count[6], a_xferP->count[7], a_xferP->count[8]);


}				/*PrintXferTiming */


/*------------------------------------------------------------------------
 * PrintErrInfo
 *
 * Description:
 *	Print out the contents of an FS RPC error info structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing.
 *	a_opNames : Ptr to table of operation names.
 *	a_opErrP  : Ptr to the op timing structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintErrInfo(int a_opIdx, char *a_opNames[],
	     struct afs_stats_RPCErrors *a_opErrP)
{				/*PrintErrInfo */

    printf
	("%15s: %u server, %u network, %u prot, %u vol, %u busies, %u other\n",
	 a_opNames[a_opIdx], a_opErrP->err_Server, a_opErrP->err_Network,
	 a_opErrP->err_Protection, a_opErrP->err_Volume,
	 a_opErrP->err_VolumeBusies, a_opErrP->err_Other);

}				/*PrintErrInfo */


/*------------------------------------------------------------------------
 * PrintRPCPerfInfo
 *
 * Description:
 *	Print out a set of RPC performance numbers.
 *
 * Arguments:
 *	a_rpcP : Ptr to RPC perf numbers to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintRPCPerfInfo(struct afs_stats_RPCOpInfo *a_rpcP)
{				/*PrintRPCPerfInfo */

    int currIdx;		/*Loop variable */

    /*
     * Print the contents of each of the opcode-related arrays.
     */
    printf("FS Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	PrintOpTiming(currIdx, fsOpNames, &(a_rpcP->fsRPCTimes[currIdx]));

    printf("\nError Info:\n-----------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	PrintErrInfo(currIdx, fsOpNames, &(a_rpcP->fsRPCErrors[currIdx]));

    printf("\nTransfer timings:\n-----------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++)
	PrintXferTiming(currIdx, xferOpNames,
			&(a_rpcP->fsXferTimes[currIdx]));

    printf("\nCM Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS; currIdx++)
	PrintOpTiming(currIdx, cmOpNames, &(a_rpcP->cmRPCTimes[currIdx]));

}				/*PrintRPCPerfInfo */


/*------------------------------------------------------------------------
 * PrintFullPerfInfo
 *
 * Description:
 *	Print out a set of full performance numbers.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintFullPerfInfo(void)
{				/*PrintFullPerfInfo */

    struct afs_stats_AuthentInfo *authentP;	/*Ptr to authentication stats */
    struct afs_stats_AccessInfo *accessinfP;	/*Ptr to access stats */
    static afs_int32 fullPerfInt32s = (sizeof(struct afs_stats_CMFullPerf) >> 2);	/*Correct #int32s */
    afs_int32 numInt32s;	/*# int32s actually received */
    struct afs_stats_CMFullPerf *fullP;	/*Ptr to full perf info */

    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != fullPerfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %u, got %u\n", fullPerfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    fullP = (struct afs_stats_CMFullPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_FULL_PERF_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    /*
     * Print the overall numbers first, followed by all of the RPC numbers,
     * then each of the other groupings.
     */
    printf("Overall Performance Info:\n-------------------------\n");
    PrintOverallPerfInfo(&(fullP->perf));
    printf("\n");
    PrintRPCPerfInfo(&(fullP->rpc));

    authentP = &(fullP->authent);
    printf("\nAuthentication info:\n--------------------\n");
    printf
	("\t%u PAGS, %u records (%u auth, %u unauth), %u max in PAG, chain max: %u\n",
	 authentP->curr_PAGs, authentP->curr_Records,
	 authentP->curr_AuthRecords, authentP->curr_UnauthRecords,
	 authentP->curr_MaxRecordsInPAG, authentP->curr_LongestChain);
    printf("\t%u PAG creations, %u tkt updates\n", authentP->PAGCreations,
	   authentP->TicketUpdates);
    printf("\t[HWMs: %u PAGS, %u records, %u max in PAG, chain max: %u]\n",
	   authentP->HWM_PAGs, authentP->HWM_Records,
	   authentP->HWM_MaxRecordsInPAG, authentP->HWM_LongestChain);

    accessinfP = &(fullP->accessinf);
    printf("\n[Un]replicated accesses:\n------------------------\n");
    printf
	("\t%u unrep, %u rep, %u reps accessed, %u max reps/ref, %u first OK\n\n",
	 accessinfP->unreplicatedRefs, accessinfP->replicatedRefs,
	 accessinfP->numReplicasAccessed, accessinfP->maxReplicasPerRef,
	 accessinfP->refFirstReplicaOK);

    /* There really isn't any authorship info
     * authorP = &(fullP->author); */

}				/*PrintFullPerfInfo */


/*------------------------------------------------------------------------
 * CM_Handler
 *
 * Description:
 *	Handler routine passed to the xstat_cm module.  This handler is
 *	called immediately after a poll of one of the Cache Managers has
 *	taken place.  All it needs to know is exported by the xstat_cm
 *	module, namely the data structure where the probe results are
 *	stored.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 on success,
 *	-1 otherwise.
 *
 * Environment:
 *	See above.  All we do now is print out what we got.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
CM_Handler(void)
{				/*CM_Handler */

    static char rn[] = "CM_Handler";	/*Routine name */

    printf("\n-----------------------------------------------------------\n");

    /*
     * If the probe failed, there isn't much we can do except gripe.
     */
    if (xstat_cm_Results.probeOK) {
	printf("%s: Probe %u, collection %d to CM on '%s' failed, code=%d\n",
	       rn, xstat_cm_Results.probeNum,
	       xstat_cm_Results.collectionNumber,
	       xstat_cm_Results.connP->hostName, xstat_cm_Results.probeOK);
	return (0);
    }

    switch (xstat_cm_Results.collectionNumber) {
    case AFSCB_XSTATSCOLL_CALL_INFO:
	/* Why was this commented out in 3.3 ? */
	/* PrintCallInfo();  */
	print_cmCallStats();
	break;

    case AFSCB_XSTATSCOLL_PERF_INFO:
	/* we will do nothing here */
	/* PrintPerfInfo(); */
	break;

    case AFSCB_XSTATSCOLL_FULL_PERF_INFO:
	PrintFullPerfInfo();
	break;

    default:
	printf("** Unknown collection: %d\n",
	       xstat_cm_Results.collectionNumber);
    }

    /*
     * Return the happy news.
     */
    return (0);

}				/*CM_Handler */


/*------------------------------------------------------------------------
 * CountListItems
 *
 * Description:
 *	Given a pointer to the list of Cache Managers we'll be polling
 *	(or, in fact, any list at all), compute the length of the list.
 *
 * Arguments:
 *	struct cmd_item *a_firstItem : Ptr to first item in list.
 *
 * Returns:
 *	Length of the above list.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
CountListItems(struct cmd_item *a_firstItem)
{				/*CountListItems */

    int list_len;		/*List length */
    struct cmd_item *curr_item;	/*Ptr to current item */

    list_len = 0;
    curr_item = a_firstItem;

    /*
     * Count 'em up.
     */
    while (curr_item) {
	list_len++;
	curr_item = curr_item->next;
    }

    /*
     * Return our tally.
     */
    return (list_len);

}				/*CountListItems */


/*------------------------------------------------------------------------
 * RunTheTest
 *
 * Description:
 *	Routine called by the command line interpreter to execute the
 *	meat of the program.  We count the number of Cache Managers
 *	to watch, allocate enough space to remember all the connection
 *	info for them, then go for it.
 *
 *
 * Arguments:
 *	a_s : Ptr to the command line syntax descriptor.
 *
 * Returns:
 *	0, but may exit the whole program on an error!
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
RunTheTest(struct cmd_syndesc *a_s, void *arock)
{				/*RunTheTest */

    static char rn[] = "RunTheTest";	/*Routine name */
    int code;			/*Return code */
    int numCMs;			/*# Cache Managers to monitor */
    int numCollIDs;		/*# collections to fetch */
    int currCM;			/*Loop index */
    int currCollIDIdx;		/*Index of current collection ID */
    afs_int32 *collIDP;		/*Ptr to array of collection IDs */
    afs_int32 *currCollIDP;	/*Ptr to current collection ID */
    struct cmd_item *curr_item;	/*Current CM cmd line record */
    struct sockaddr_in *CMSktArray;	/*Cache Manager socket array */
    struct hostent *he;		/*Host entry */
    struct timeval tv;		/*Time structure */
    int sleep_secs;		/*Number of seconds to sleep */
    int initFlags;		/*Flags passed to the init fcn */
    int waitCode;		/*Result of LWP_WaitProcess() */
    int freq;			/*Frequency of polls */
    int period;			/*Time in minutes of data collection */

    /*
     * Are we doing one-shot measurements?
     */
    if (a_s->parms[P_ONESHOT].items != 0)
	one_shot = 1;

    /*
     * Are we doing debugging output?
     */
    if (a_s->parms[P_DEBUG].items != 0)
	debugging_on = 1;

    /*
     * Pull out the number of Cache Managers to watch and the number of
     * collections to get.
     */
    numCMs = CountListItems(a_s->parms[P_CM_NAMES].items);
    numCollIDs = CountListItems(a_s->parms[P_COLL_IDS].items);

    /* Get the polling frequency */
    if (a_s->parms[P_FREQUENCY].items != 0)
	freq = atoi(a_s->parms[P_FREQUENCY].items->data);
    else
	freq = 30;		/* default to 30 seconds */

    /* Get the time duration to run the tests */
    if (a_s->parms[P_PERIOD].items != 0)
	period = atoi(a_s->parms[P_PERIOD].items->data);
    else
	period = 10;		/* default to 10 minutes */

    /*
     * Allocate the socket array.
     */
    if (debugging_on)
	printf("%s: Allocating socket array for %d Cache Manager(s)\n", rn,
	       numCMs);
    CMSktArray = (struct sockaddr_in *)
	malloc(numCMs * sizeof(struct sockaddr_in));
    if (CMSktArray == (struct sockaddr_in *)0) {
	printf("%s: Can't allocate socket array for %d Cache Managers\n", rn,
	       numCMs);
	exit(1);
    }

    /*
     * Fill in the socket array for each of the Cache Managers listed.
     */
    curr_item = a_s->parms[P_CM_NAMES].items;
    for (currCM = 0; currCM < numCMs; currCM++) {
	CMSktArray[currCM].sin_family = AF_INET;
	CMSktArray[currCM].sin_port = htons(7001);	/* Cache Manager port */
	he = hostutil_GetHostByName(curr_item->data);
	if (he == NULL) {
	    fprintf(stderr, "[%s] Can't get host info for '%s'\n", rn,
		    curr_item->data);
	    exit(-1);
	}
	memcpy(&(CMSktArray[currCM].sin_addr.s_addr), he->h_addr, 4);

	/*
	 * Move to the next CM name.
	 */
	curr_item = curr_item->next;

    }				/*Get socket info for each Cache Manager */

    /*
     * Create and fill up the array of desired collection IDs.
     */
    if (debugging_on)
	printf("Allocating %d long(s) for coll ID\n", numCollIDs);
    collIDP = (afs_int32 *) (malloc(numCollIDs * sizeof(afs_int32)));
    currCollIDP = collIDP;
    curr_item = a_s->parms[P_COLL_IDS].items;
    for (currCollIDIdx = 0; currCollIDIdx < numCollIDs; currCollIDIdx++) {
	*currCollIDP = (afs_int32) (atoi(curr_item->data));
	if (debugging_on)
	    printf("CollID at index %d is %d\n", currCollIDIdx, *currCollIDP);
	curr_item = curr_item->next;
	currCollIDP++;
    };

    /*
     * Crank up the Cache Manager prober, then sit back and have fun.
     */
    printf("\nStarting up the xstat_cm service, ");
    initFlags = 0;
    if (debugging_on) {
	initFlags |= XSTAT_CM_INITFLAG_DEBUGGING;
	printf("debugging enabled, ");
    } else
	printf("no debugging, ");
    if (one_shot) {
	initFlags |= XSTAT_CM_INITFLAG_ONE_SHOT;
	printf("one-shot operation\n");
    } else
	printf("continuous operation\n");

    code = xstat_cm_Init(numCMs,	/*Num CMs */
			 CMSktArray,	/*File Server socket array */
			 freq,	/*Probe every 30 seconds */
			 CM_Handler,	/*Handler routine */
			 initFlags,	/*Initialization flags */
			 numCollIDs,	/*Number of collection IDs */
			 collIDP);	/*Ptr to collection ID array */
    if (code) {
	fprintf(stderr, "[%s] Error returned by xstat_cm_Init: %d\n", rn,
		code);
	xstat_cm_Cleanup(1);	/*Get rid of malloc'ed structures */
	exit(-1);
    }

    if (one_shot) {
	/*
	 * One-shot operation; just wait for the collection to be done.
	 */
	if (debugging_on)
	    printf("[%s] Calling LWP_WaitProcess() on event %" AFS_PTR_FMT
		   "\n", rn, &terminationEvent);
	waitCode = LWP_WaitProcess(&terminationEvent);
	if (debugging_on)
	    printf("[%s] Returned from LWP_WaitProcess()\n", rn);
	if (waitCode) {
	    if (debugging_on)
		fprintf(stderr,
			"[%s] Error %d encountered by LWP_WaitProcess()\n",
			rn, waitCode);
	}
    } else {
	/*
	 * Continuous operation.
	 */
	sleep_secs = 60 * period;	/*length of data collection */
	printf
	    ("xstat_cm service started, main thread sleeping for %d secs.\n",
	     sleep_secs);

	/*
	 * Let's just fall asleep for a while, then we'll clean up.
	 */
	tv.tv_sec = sleep_secs;
	tv.tv_usec = 0;
	code = IOMGR_Select(0,	/*Num fds */
			    0,	/*Descriptors ready for reading */
			    0,	/*Descriptors ready for writing */
			    0,	/*Descriptors with exceptional conditions */
			    &tv);	/*Timeout structure */
	if (code) {
	    fprintf(stderr,
		    "[%s] IOMGR_Select() returned non-zero value: %d\n", rn,
		    code);
	}
    }

    /*
     * We're all done.  Clean up, put the last nail in Rx, then
     * exit happily.
     */
    if (debugging_on)
	printf("\nYawn, main thread just woke up.  Cleaning things out...\n");
    code = xstat_cm_Cleanup(1);	/*Get rid of malloc'ed data */
    rx_Finalize();
    return (0);

}				/*RunTheTest */


#include "AFS_component_version_number.c"
int
main(int argc, char **argv)
{				/*Main routine */

    static char rn[] = "xstat_cm_test";	/*Routine name */
    afs_int32 code;	/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to cmd line syntax desc */

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", RunTheTest, NULL, "initialize the program");
    cmd_AddParm(ts, "-cmname", CMD_LIST, CMD_REQUIRED,
		"Cache Manager name(s) to monitor");
    cmd_AddParm(ts, "-collID", CMD_LIST, CMD_REQUIRED,
		"Collection(s) to fetch");
    cmd_AddParm(ts, "-onceonly", CMD_FLAG, CMD_OPTIONAL,
		"Collect results exactly once, then quit");
    cmd_AddParm(ts, "-frequency", CMD_SINGLE, CMD_OPTIONAL,
		"poll frequency, in seconds");
    cmd_AddParm(ts, "-period", CMD_SINGLE, CMD_OPTIONAL,
		"data collection time, in minutes");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL,
		"turn on debugging output");

    /*
     * Parse command-line switches & execute the test, then get the
     * heck out of here.
     */
    code = cmd_Dispatch(argc, argv);
    if (code) {
	fprintf(stderr, "[%s] Call to cmd_Dispatch() failed; code is %d\n",
		rn, code);
    }

    exit(code);

}				/*Main routine */
