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

RCSID
    ("$Header$");

#include "xstat_cm.h"		/*Interface for xstat_cm module */
#include <cmd.h>		/*Command line interpreter */
#include <time.h>
#include <string.h>

/*
 * External routines that don't have explicit include file definitions.
 */
extern struct hostent *hostutil_GetHostByName();

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
PrintCallInfo()
{				/*PrintCallInfo */

    static char rn[] = "PrintCallInfo";	/*Routine name */
    register int i;		/*Loop variable */
    int numInt32s;		/*# int32words returned */
    afs_int32 *currInt32;	/*Ptr to current afs_int32 value */
    char *printableTime;	/*Ptr to printable time string */

    /*
     * Just print out the results of the particular probe.
     */
    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    currInt32 = (afs_int32 *) (xstat_cm_Results.data.AFSCB_CollData_val);
    printableTime = ctime((time_t *) & (xstat_cm_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Poll %d, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    if (debugging_on)
	printf("\n[%d entries returned at 0x%x]\n\n", numInt32s, currInt32);

    for (i = 0; i < numInt32s; i++)
	printf("%d ", *currInt32++);
    printf("\n");


}				/*PrintCallInfo */

/* Print detailed functional call statistics */

print_cmCallStats()
{
    static char rn[] = "print_cmCallStats";	/*Routine name */
    char *printableTime;	/*Ptr to printable time string */
    struct afs_CMStats *cmp;

    printableTime = ctime((time_t *) & (xstat_cm_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Probe %d, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    cmp = (struct afs_CMStats *)(xstat_cm_Results.data.AFSCB_CollData_val);

    printf("\t%10d afs_init\n", cmp->callInfo.C_afs_init);
    printf("\t%10d gop_rdwr\n", cmp->callInfo.C_gop_rdwr);
    printf("\t%10d aix_gnode_rele\n", cmp->callInfo.C_aix_gnode_rele);
    printf("\t%10d gettimeofday\n", cmp->callInfo.C_gettimeofday);
    printf("\t%10d m_cpytoc\n", cmp->callInfo.C_m_cpytoc);
    printf("\t%10d aix_vattr_null\n", cmp->callInfo.C_aix_vattr_null);
    printf("\t%10d afs_gn_frunc\n", cmp->callInfo.C_afs_gn_ftrunc);
    printf("\t%10d afs_gn_rdwr\n", cmp->callInfo.C_afs_gn_rdwr);
    printf("\t%10d afs_gn_ioctl\n", cmp->callInfo.C_afs_gn_ioctl);
    printf("\t%10d afs_gn_locktl\n", cmp->callInfo.C_afs_gn_lockctl);
    printf("\t%10d afs_gn_readlink\n", cmp->callInfo.C_afs_gn_readlink);
    printf("\t%10d afs_gn_readdir\n", cmp->callInfo.C_afs_gn_readdir);
    printf("\t%10d afs_gn_select\n", cmp->callInfo.C_afs_gn_select);
    printf("\t%10d afs_gn_strategy\n", cmp->callInfo.C_afs_gn_strategy);
    printf("\t%10d afs_gn_symlink\n", cmp->callInfo.C_afs_gn_symlink);
    printf("\t%10d afs_gn_revoke\n", cmp->callInfo.C_afs_gn_revoke);
    printf("\t%10d afs_gn_link\n", cmp->callInfo.C_afs_gn_link);
    printf("\t%10d afs_gn_mkdir\n", cmp->callInfo.C_afs_gn_mkdir);
    printf("\t%10d afs_gn_mknod\n", cmp->callInfo.C_afs_gn_mknod);
    printf("\t%10d afs_gn_remove\n", cmp->callInfo.C_afs_gn_remove);
    printf("\t%10d afs_gn_rename\n", cmp->callInfo.C_afs_gn_rename);
    printf("\t%10d afs_gn_rmdir\n", cmp->callInfo.C_afs_gn_rmdir);
    printf("\t%10d afs_gn_fid\n", cmp->callInfo.C_afs_gn_fid);
    printf("\t%10d afs_gn_lookup\n", cmp->callInfo.C_afs_gn_lookup);
    printf("\t%10d afs_gn_open\n", cmp->callInfo.C_afs_gn_open);
    printf("\t%10d afs_gn_create\n", cmp->callInfo.C_afs_gn_create);
    printf("\t%10d afs_gn_hold\n", cmp->callInfo.C_afs_gn_hold);
    printf("\t%10d afs_gn_rele\n", cmp->callInfo.C_afs_gn_rele);
    printf("\t%10d afs_gn_unmap\n", cmp->callInfo.C_afs_gn_unmap);
    printf("\t%10d afs_gn_access\n", cmp->callInfo.C_afs_gn_access);
    printf("\t%10d afs_gn_getattr\n", cmp->callInfo.C_afs_gn_getattr);
    printf("\t%10d afs_gn_setattr\n", cmp->callInfo.C_afs_gn_setattr);
    printf("\t%10d afs_gn_fclear\n", cmp->callInfo.C_afs_gn_fclear);
    printf("\t%10d afs_gn_fsync\n", cmp->callInfo.C_afs_gn_fsync);
    printf("\t%10d phash\n", cmp->callInfo.C_pHash);
    printf("\t%10d DInit\n", cmp->callInfo.C_DInit);
    printf("\t%10d DRead\n", cmp->callInfo.C_DRead);
    printf("\t%10d FixupBucket\n", cmp->callInfo.C_FixupBucket);
    printf("\t%10d afs_newslot\n", cmp->callInfo.C_afs_newslot);
    printf("\t%10d DRelease\n", cmp->callInfo.C_DRelease);
    printf("\t%10d DFlush\n", cmp->callInfo.C_DFlush);
    printf("\t%10d DFlushEntry\n", cmp->callInfo.C_DFlushEntry);
    printf("\t%10d DVOffset\n", cmp->callInfo.C_DVOffset);
    printf("\t%10d DZap\n", cmp->callInfo.C_DZap);
    printf("\t%10d DNew\n", cmp->callInfo.C_DNew);
    printf("\t%10d afs_RemoveVCB\n", cmp->callInfo.C_afs_RemoveVCB);
    printf("\t%10d afs_NewVCache\n", cmp->callInfo.C_afs_NewVCache);
    printf("\t%10d afs_FlushActiveVcaches\n",
	   cmp->callInfo.C_afs_FlushActiveVcaches);
    printf("\t%10d afs_VerifyVCache\n", cmp->callInfo.C_afs_VerifyVCache);
    printf("\t%10d afs_WriteVCache\n", cmp->callInfo.C_afs_WriteVCache);
    printf("\t%10d afs_GetVCache\n", cmp->callInfo.C_afs_GetVCache);
    printf("\t%10d afs_StuffVcache\n", cmp->callInfo.C_afs_StuffVcache);
    printf("\t%10d afs_FindVCache\n", cmp->callInfo.C_afs_FindVCache);
    printf("\t%10d afs_PutDCache\n", cmp->callInfo.C_afs_PutDCache);
    printf("\t%10d afs_PutVCache\n", cmp->callInfo.C_afs_PutVCache);
    printf("\t%10d CacheStoreProc\n", cmp->callInfo.C_CacheStoreProc);
    printf("\t%10d afs_FindDcache\n", cmp->callInfo.C_afs_FindDCache);
    printf("\t%10d afs_TryToSmush\n", cmp->callInfo.C_afs_TryToSmush);
    printf("\t%10d afs_AdjustSize\n", cmp->callInfo.C_afs_AdjustSize);
    printf("\t%10d afs_CheckSize\n", cmp->callInfo.C_afs_CheckSize);
    printf("\t%10d afs_StoreWarn\n", cmp->callInfo.C_afs_StoreWarn);
    printf("\t%10d CacheFetchProc\n", cmp->callInfo.C_CacheFetchProc);
    printf("\t%10d UFS_CacheStoreProc\n", cmp->callInfo.C_UFS_CacheStoreProc);
    printf("\t%10d UFS_CacheFetchProc\n", cmp->callInfo.C_UFS_CacheFetchProc);
    printf("\t%10d afs_GetDCache\n", cmp->callInfo.C_afs_GetDCache);
    printf("\t%10d afs_SimpleVStat\n", cmp->callInfo.C_afs_SimpleVStat);
    printf("\t%10d afs_ProcessFS\n", cmp->callInfo.C_afs_ProcessFS);
    printf("\t%10d afs_InitCacheInfo\n", cmp->callInfo.C_afs_InitCacheInfo);
    printf("\t%10d afs_InitVolumeInfo\n", cmp->callInfo.C_afs_InitVolumeInfo);
    printf("\t%10d afs_InitCacheFile\n", cmp->callInfo.C_afs_InitCacheFile);
    printf("\t%10d afs_CacheInit\n", cmp->callInfo.C_afs_CacheInit);
    printf("\t%10d afs_GetDSlot\n", cmp->callInfo.C_afs_GetDSlot);
    printf("\t%10d afs_WriteThroughDSlots\n",
	   cmp->callInfo.C_afs_WriteThroughDSlots);
    printf("\t%10d afs_MemGetDSlot\n", cmp->callInfo.C_afs_MemGetDSlot);
    printf("\t%10d afs_UFSGetDSlot\n", cmp->callInfo.C_afs_UFSGetDSlot);
    printf("\t%10d afs_StoreDCache\n", cmp->callInfo.C_afs_StoreDCache);
    printf("\t%10d afs_StoreMini\n", cmp->callInfo.C_afs_StoreMini);
    printf("\t%10d afs_StoreAllSegments\n",
	   cmp->callInfo.C_afs_StoreAllSegments);
    printf("\t%10d afs_InvalidateAllSegments\n",
	   cmp->callInfo.C_afs_InvalidateAllSegments);
    printf("\t%10d afs_TruncateAllSegments\n",
	   cmp->callInfo.C_afs_TruncateAllSegments);
    printf("\t%10d afs_CheckVolSync\n", cmp->callInfo.C_afs_CheckVolSync);
    printf("\t%10d afs_wakeup\n", cmp->callInfo.C_afs_wakeup);
    printf("\t%10d afs_CFileOpen\n", cmp->callInfo.C_afs_CFileOpen);
    printf("\t%10d afs_CFileTruncate\n", cmp->callInfo.C_afs_CFileTruncate);
    printf("\t%10d afs_GetDownD\n", cmp->callInfo.C_afs_GetDownD);
    printf("\t%10d afs_WriteDCache\n", cmp->callInfo.C_afs_WriteDCache);
    printf("\t%10d afs_FlushDCache\n", cmp->callInfo.C_afs_FlushDCache);
    printf("\t%10d afs_GetDownDSlot\n", cmp->callInfo.C_afs_GetDownDSlot);
    printf("\t%10d afs_FlushVCache\n", cmp->callInfo.C_afs_FlushVCache);
    printf("\t%10d afs_GetDownV\n", cmp->callInfo.C_afs_GetDownV);
    printf("\t%10d afs_QueueVCB\n", cmp->callInfo.C_afs_QueueVCB);
    printf("\t%10d afs_call\n", cmp->callInfo.C_afs_call);
    printf("\t%10d afs_syscall_call\n", cmp->callInfo.C_afs_syscall_call);
    printf("\t%10d afs_syscall_icreate\n",
	   cmp->callInfo.C_afs_syscall_icreate);
    printf("\t%10d afs_syscall_iopen\n", cmp->callInfo.C_afs_syscall_iopen);
    printf("\t%10d afs_syscall_iincdec\n",
	   cmp->callInfo.C_afs_syscall_iincdec);
    printf("\t%10d afs_syscall_ireadwrite\n",
	   cmp->callInfo.C_afs_syscall_ireadwrite);
    printf("\t%10d afs_syscall\n", cmp->callInfo.C_afs_syscall);
    printf("\t%10d lpioctl\n", cmp->callInfo.C_lpioctl);
    printf("\t%10d lsetpag\n", cmp->callInfo.C_lsetpag);
    printf("\t%10d afs_CheckInit\n", cmp->callInfo.C_afs_CheckInit);
    printf("\t%10d ClearCallback\n", cmp->callInfo.C_ClearCallBack);
    printf("\t%10d SRXAFSCB_GetCE\n", cmp->callInfo.C_SRXAFSCB_GetCE);
    printf("\t%10d SRXAFSCB_GetLock\n", cmp->callInfo.C_SRXAFSCB_GetLock);
    printf("\t%10d SRXAFSCB_CallBack\n", cmp->callInfo.C_SRXAFSCB_CallBack);
    printf("\t%10d SRXAFSCB_InitCallBackState\n",
	   cmp->callInfo.C_SRXAFSCB_InitCallBackState);
    printf("\t%10d SRXAFSCB_Probe\n", cmp->callInfo.C_SRXAFSCB_Probe);
    printf("\t%10d afs_Chunk\n", cmp->callInfo.C_afs_Chunk);
    printf("\t%10d afs_ChunkBase\n", cmp->callInfo.C_afs_ChunkBase);
    printf("\t%10d afs_ChunkOffset\n", cmp->callInfo.C_afs_ChunkOffset);
    printf("\t%10d afs_ChunkSize\n", cmp->callInfo.C_afs_ChunkSize);
    printf("\t%10d afs_ChunkToBase\n", cmp->callInfo.C_afs_ChunkToBase);
    printf("\t%10d afs_ChunkToSize\n", cmp->callInfo.C_afs_ChunkToSize);
    printf("\t%10d afs_SetChunkSize\n", cmp->callInfo.C_afs_SetChunkSize);
    printf("\t%10d afs_config\n", cmp->callInfo.C_afs_config);
    printf("\t%10d mem_freebytes\n", cmp->callInfo.C_mem_freebytes);
    printf("\t%10d mem_getbytes\n", cmp->callInfo.C_mem_getbytes);
    printf("\t%10d afs_Daemon\n", cmp->callInfo.C_afs_Daemon);
    printf("\t%10d afs_CheckRootVolume\n",
	   cmp->callInfo.C_afs_CheckRootVolume);
    printf("\t%10d BPath\n", cmp->callInfo.C_BPath);
    printf("\t%10d BPrefetch\n", cmp->callInfo.C_BPrefetch);
    printf("\t%10d BStore\n", cmp->callInfo.C_BStore);
    printf("\t%10d afs_BBusy\n", cmp->callInfo.C_afs_BBusy);
    printf("\t%10d afs_BQueue\n", cmp->callInfo.C_afs_BQueue);
    printf("\t%10d afs_BRelease\n", cmp->callInfo.C_afs_BRelease);
    printf("\t%10d afs_BackgroundDaemon\n",
	   cmp->callInfo.C_afs_BackgroundDaemon);
    printf("\t%10d exporter_add\n", cmp->callInfo.C_exporter_add);
    printf("\t%10d exporter_find\n", cmp->callInfo.C_exporter_find);
    printf("\t%10d afs_gfs_kalloc\n", cmp->callInfo.C_afs_gfs_kalloc);
    printf("\t%10d afs_gfs_kfree\n", cmp->callInfo.C_afs_gfs_kfree);
    printf("\t%10d gop_lookupname\n", cmp->callInfo.C_gop_lookupname);
    printf("\t%10d afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10d gfs_vattr_null\n", cmp->callInfo.C_gfs_vattr_null);
    printf("\t%10d afs_lock\n", cmp->callInfo.C_afs_lock);
    printf("\t%10d afs_unlock\n", cmp->callInfo.C_afs_unlock);
    printf("\t%10d afs_update\n", cmp->callInfo.C_afs_update);
    printf("\t%10d afs_gclose\n", cmp->callInfo.C_afs_gclose);
    printf("\t%10d afs_gopen\n", cmp->callInfo.C_afs_gopen);
    printf("\t%10d afs_greadlink\n", cmp->callInfo.C_afs_greadlink);
    printf("\t%10d afs_select\n", cmp->callInfo.C_afs_select);
    printf("\t%10d afs_gbmap\n", cmp->callInfo.C_afs_gbmap);
    printf("\t%10d afs_getfsdata\n", cmp->callInfo.C_afs_getfsdata);
    printf("\t%10d afs_gsymlink\n", cmp->callInfo.C_afs_gsymlink);
    printf("\t%10d afs_namei\n", cmp->callInfo.C_afs_namei);
    printf("\t%10d afs_gmount\n", cmp->callInfo.C_afs_gmount);
    printf("\t%10d afs_gget\n", cmp->callInfo.C_afs_gget);
    printf("\t%10d afs_glink\n", cmp->callInfo.C_afs_glink);
    printf("\t%10d afs_gmkdir\n", cmp->callInfo.C_afs_gmkdir);
    printf("\t%10d afs_unlink\n", cmp->callInfo.C_afs_unlink);
    printf("\t%10d afs_grmdir\n", cmp->callInfo.C_afs_grmdir);
    printf("\t%10d afs_makenode\n", cmp->callInfo.C_afs_makenode);
    printf("\t%10d afs_grename\n", cmp->callInfo.C_afs_grename);
    printf("\t%10d afs_rele\n", cmp->callInfo.C_afs_rele);
    printf("\t%10d afs_syncgp\n", cmp->callInfo.C_afs_syncgp);
    printf("\t%10d afs_getval\n", cmp->callInfo.C_afs_getval);
    printf("\t%10d afs_trunc\n", cmp->callInfo.C_afs_trunc);
    printf("\t%10d afs_rwgp\n", cmp->callInfo.C_afs_rwgp);
    printf("\t%10d afs_stat\n", cmp->callInfo.C_afs_stat);
    printf("\t%10d afsc_link\n", cmp->callInfo.C_afsc_link);
    printf("\t%10d afs_vfs_mount\n", cmp->callInfo.C_afs_vfs_mount);
    printf("\t%10d afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10d iopen\n", cmp->callInfo.C_iopen);
    printf("\t%10d idec\n", cmp->callInfo.C_idec);
    printf("\t%10d iinc\n", cmp->callInfo.C_iinc);
    printf("\t%10d ireadwrite\n", cmp->callInfo.C_ireadwrite);
    printf("\t%10d iread\n", cmp->callInfo.C_iread);
    printf("\t%10d iwrite\n", cmp->callInfo.C_iwrite);
    printf("\t%10d iforget\n", cmp->callInfo.C_iforget);
    printf("\t%10d icreate\n", cmp->callInfo.C_icreate);
    printf("\t%10d igetinode\n", cmp->callInfo.C_igetinode);
    printf("\t%10d osi_SleepR\n", cmp->callInfo.C_osi_SleepR);
    printf("\t%10d osi_SleepS\n", cmp->callInfo.C_osi_SleepS);
    printf("\t%10d osi_SleepW\n", cmp->callInfo.C_osi_SleepW);
    printf("\t%10d osi_Sleep\n", cmp->callInfo.C_osi_Sleep);
    printf("\t%10d afs_LookupMCE\n", cmp->callInfo.C_afs_LookupMCE);
    printf("\t%10d afs_MemReadBlk\n", cmp->callInfo.C_afs_MemReadBlk);
    printf("\t%10d afs_MemReadUIO\n", cmp->callInfo.C_afs_MemReadUIO);
    printf("\t%10d afs_MemWriteBlk\n", cmp->callInfo.C_afs_MemWriteBlk);
    printf("\t%10d afs_MemWriteUIO\n", cmp->callInfo.C_afs_MemWriteUIO);
    printf("\t%10d afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10d afs_MemCacheFetchProc\n",
	   cmp->callInfo.C_afs_MemCacheFetchProc);
    printf("\t%10d afs_MemCacheTruncate\n",
	   cmp->callInfo.C_afs_MemCacheTruncate);
    printf("\t%10d afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10d afs_GetNfsClientPag\n",
	   cmp->callInfo.C_afs_GetNfsClientPag);
    printf("\t%10d afs_FindNfsClientPag\n",
	   cmp->callInfo.C_afs_FindNfsClientPag);
    printf("\t%10d afs_PutNfsClientPag\n",
	   cmp->callInfo.C_afs_PutNfsClientPag);
    printf("\t%10d afs_nfsclient_reqhandler\n",
	   cmp->callInfo.C_afs_nfsclient_reqhandler);
    printf("\t%10d afs_nfsclient_GC\n", cmp->callInfo.C_afs_nfsclient_GC);
    printf("\t%10d afs_nfsclient_hold\n", cmp->callInfo.C_afs_nfsclient_hold);
    printf("\t%10d afs_nfsclient_stats\n",
	   cmp->callInfo.C_afs_nfsclient_stats);
    printf("\t%10d afs_nfsclient_sysname\n",
	   cmp->callInfo.C_afs_nfsclient_sysname);
    printf("\t%10d afs_rfs_dispatch\n", cmp->callInfo.C_afs_rfs_dispatch);
    printf("\t%10d afs_nfs2afscall\n", cmp->callInfo.C_Nfs2AfsCall);
    printf("\t%10d afs_sun_xuntext\n", cmp->callInfo.C_afs_sun_xuntext);
    printf("\t%10d osi_Active\n", cmp->callInfo.C_osi_Active);
    printf("\t%10d osi_FlushPages\n", cmp->callInfo.C_osi_FlushPages);
    printf("\t%10d osi_FlushText\n", cmp->callInfo.C_osi_FlushText);
    printf("\t%10d osi_CallProc\n", cmp->callInfo.C_osi_CallProc);
    printf("\t%10d osi_CancelProc\n", cmp->callInfo.C_osi_CancelProc);
    printf("\t%10d osi_Invisible\n", cmp->callInfo.C_osi_Invisible);
    printf("\t%10d osi_Time\n", cmp->callInfo.C_osi_Time);
    printf("\t%10d osi_Alloc\n", cmp->callInfo.C_osi_Alloc);
    printf("\t%10d osi_SetTime\n", cmp->callInfo.C_osi_SetTime);
    printf("\t%10d osi_Dump\n", cmp->callInfo.C_osi_Dump);
    printf("\t%10d osi_Free\n", cmp->callInfo.C_osi_Free);
    printf("\t%10d osi_UFSOpen\n", cmp->callInfo.C_osi_UFSOpen);
    printf("\t%10d osi_Close\n", cmp->callInfo.C_osi_Close);
    printf("\t%10d osi_Stat\n", cmp->callInfo.C_osi_Stat);
    printf("\t%10d osi_Truncate\n", cmp->callInfo.C_osi_Truncate);
    printf("\t%10d osi_Read\n", cmp->callInfo.C_osi_Read);
    printf("\t%10d osi_Write\n", cmp->callInfo.C_osi_Write);
    printf("\t%10d osi_MapStrategy\n", cmp->callInfo.C_osi_MapStrategy);
    printf("\t%10d osi_AllocLargeSpace\n",
	   cmp->callInfo.C_osi_AllocLargeSpace);
    printf("\t%10d osi_FreeLargeSpace\n", cmp->callInfo.C_osi_FreeLargeSpace);
    printf("\t%10d osi_AllocSmallSpace\n",
	   cmp->callInfo.C_osi_AllocSmallSpace);
    printf("\t%10d osi_FreeSmallSpace\n", cmp->callInfo.C_osi_FreeSmallSpace);
    printf("\t%10d osi_CloseToTheEdge\n", cmp->callInfo.C_osi_CloseToTheEdge);
    printf("\t%10d osi_xgreedy\n", cmp->callInfo.C_osi_xgreedy);
    printf("\t%10d osi_FreeSocket\n", cmp->callInfo.C_osi_FreeSocket);
    printf("\t%10d osi_NewSocket\n", cmp->callInfo.C_osi_NewSocket);
    printf("\t%10d osi_NetSend\n", cmp->callInfo.C_osi_NetSend);
    printf("\t%10d WaitHack\n", cmp->callInfo.C_WaitHack);
    printf("\t%10d osi_CancelWait\n", cmp->callInfo.C_osi_CancelWait);
    printf("\t%10d osi_Wakeup\n", cmp->callInfo.C_osi_Wakeup);
    printf("\t%10d osi_Wait\n", cmp->callInfo.C_osi_Wait);
    printf("\t%10d dirp_Read\n", cmp->callInfo.C_dirp_Read);
    printf("\t%10d dirp_Cpy\n", cmp->callInfo.C_dirp_Cpy);
    printf("\t%10d dirp_Eq\n", cmp->callInfo.C_dirp_Eq);
    printf("\t%10d dirp_Write\n", cmp->callInfo.C_dirp_Write);
    printf("\t%10d dirp_Zap\n", cmp->callInfo.C_dirp_Zap);
    printf("\t%10d afs_ioctl\n", cmp->callInfo.C_afs_ioctl);
    printf("\t%10d handleIoctl\n", cmp->callInfo.C_HandleIoctl);
    printf("\t%10d afs_xioctl\n", cmp->callInfo.C_afs_xioctl);
    printf("\t%10d afs_pioctl\n", cmp->callInfo.C_afs_pioctl);
    printf("\t%10d HandlePioctl\n", cmp->callInfo.C_HandlePioctl);
    printf("\t%10d PGetVolumeStatus\n", cmp->callInfo.C_PGetVolumeStatus);
    printf("\t%10d PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10d PFlush\n", cmp->callInfo.C_PFlush);
    printf("\t%10d PFlushVolumeData\n", cmp->callInfo.C_PFlushVolumeData);
    printf("\t%10d PNewStatMount\n", cmp->callInfo.C_PNewStatMount);
    printf("\t%10d PGetTokens\n", cmp->callInfo.C_PGetTokens);
    printf("\t%10d PSetTokens\n", cmp->callInfo.C_PSetTokens);
    printf("\t%10d PUnlog\n", cmp->callInfo.C_PUnlog);
    printf("\t%10d PCheckServers\n", cmp->callInfo.C_PCheckServers);
    printf("\t%10d PCheckAuth\n", cmp->callInfo.C_PCheckAuth);
    printf("\t%10d PCheckVolNames\n", cmp->callInfo.C_PCheckVolNames);
    printf("\t%10d PFindVolume\n", cmp->callInfo.C_PFindVolume);
    printf("\t%10d Prefetch\n", cmp->callInfo.C_Prefetch);
    printf("\t%10d PGetCacheSize\n", cmp->callInfo.C_PGetCacheSize);
    printf("\t%10d PSetCacheSize\n", cmp->callInfo.C_PSetCacheSize);
    printf("\t%10d PSetSysName\n", cmp->callInfo.C_PSetSysName);
    printf("\t%10d PExportAfs\n", cmp->callInfo.C_PExportAfs);
    printf("\t%10d HandleClientContext\n",
	   cmp->callInfo.C_HandleClientContext);
    printf("\t%10d PViceAccess\n", cmp->callInfo.C_PViceAccess);
    printf("\t%10d PRemoveCallBack\n", cmp->callInfo.C_PRemoveCallBack);
    printf("\t%10d PRemoveMount\n", cmp->callInfo.C_PRemoveMount);
    printf("\t%10d PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10d PListCells\n", cmp->callInfo.C_PListCells);
    printf("\t%10d PNewCell\n", cmp->callInfo.C_PNewCell);
    printf("\t%10d PGetUserCell\n", cmp->callInfo.C_PGetUserCell);
    printf("\t%10d PGetCellStatus\n", cmp->callInfo.C_PGetCellStatus);
    printf("\t%10d PSetCellStatus\n", cmp->callInfo.C_PSetCellStatus);
    printf("\t%10d PVenusLogging\n", cmp->callInfo.C_PVenusLogging);
    printf("\t%10d PGetAcl\n", cmp->callInfo.C_PGetAcl);
    printf("\t%10d PGetFID\n", cmp->callInfo.C_PGetFID);
    printf("\t%10d PSetAcl\n", cmp->callInfo.C_PSetAcl);
    printf("\t%10d PGetFileCell\n", cmp->callInfo.C_PGetFileCell);
    printf("\t%10d PGetWSCell\n", cmp->callInfo.C_PGetWSCell);
    printf("\t%10d PGetSPrefs\n", cmp->callInfo.C_PGetSPrefs);
    printf("\t%10d PSetSPrefs\n", cmp->callInfo.C_PSetSPrefs);
    printf("\t%10d afs_ResetAccessCache\n",
	   cmp->callInfo.C_afs_ResetAccessCache);
    printf("\t%10d afs_FindUser\n", cmp->callInfo.C_afs_FindUser);
    printf("\t%10d afs_GetUser\n", cmp->callInfo.C_afs_GetUser);
    printf("\t%10d afs_GCUserData\n", cmp->callInfo.C_afs_GCUserData);
    printf("\t%10d afs_PutUser\n", cmp->callInfo.C_afs_PutUser);
    printf("\t%10d afs_SetPrimary\n", cmp->callInfo.C_afs_SetPrimary);
    printf("\t%10d afs_ResetUserConns\n", cmp->callInfo.C_afs_ResetUserConns);
    printf("\t%10d afs_RemoveUserConns\n", cmp->callInfo.C_RemoveUserConns);
    printf("\t%10d afs_ResourceInit\n", cmp->callInfo.C_afs_ResourceInit);
    printf("\t%10d afs_GetCell\n", cmp->callInfo.C_afs_GetCell);
    printf("\t%10d afs_GetCellByIndex\n", cmp->callInfo.C_afs_GetCellByIndex);
    printf("\t%10d afs_GetCellByName\n", cmp->callInfo.C_afs_GetCellByName);
    printf("\t%10d afs_GetRealCellByIndex\n",
	   cmp->callInfo.C_afs_GetRealCellByIndex);
    printf("\t%10d afs_NewCell\n", cmp->callInfo.C_afs_NewCell);
    printf("\t%10d CheckVLDB\n", cmp->callInfo.C_CheckVLDB);
    printf("\t%10d afs_GetVolume\n", cmp->callInfo.C_afs_GetVolume);
    printf("\t%10d afs_PutVolume\n", cmp->callInfo.C_afs_PutVolume);
    printf("\t%10d afs_GetVolumeByName\n",
	   cmp->callInfo.C_afs_GetVolumeByName);
    printf("\t%10d afs_random\n", cmp->callInfo.C_afs_random);
    printf("\t%10d InstallVolumeEntry\n", cmp->callInfo.C_InstallVolumeEntry);
    printf("\t%10d InstallVolumeInfo\n", cmp->callInfo.C_InstallVolumeInfo);
    printf("\t%10d afs_ResetVolumeInfo\n",
	   cmp->callInfo.C_afs_ResetVolumeInfo);
    printf("\t%10d afs_FindServer\n", cmp->callInfo.C_afs_FindServer);
    printf("\t%10d afs_GetServer\n", cmp->callInfo.C_afs_GetServer);
    printf("\t%10d afs_SortServers\n", cmp->callInfo.C_afs_SortServers);
    printf("\t%10d afs_CheckServers\n", cmp->callInfo.C_afs_CheckServers);
    printf("\t%10d ServerDown\n", cmp->callInfo.C_ServerDown);
    printf("\t%10d afs_Conn\n", cmp->callInfo.C_afs_Conn);
    printf("\t%10d afs_PutConn\n", cmp->callInfo.C_afs_PutConn);
    printf("\t%10d afs_ConnByHost\n", cmp->callInfo.C_afs_ConnByHost);
    printf("\t%10d afs_ConnByMHosts\n", cmp->callInfo.C_afs_ConnByMHosts);
    printf("\t%10d afs_Analyze\n", cmp->callInfo.C_afs_Analyze);
    printf("\t%10d afs_CheckLocks\n", cmp->callInfo.C_afs_CheckLocks);
    printf("\t%10d CheckVLServer\n", cmp->callInfo.C_CheckVLServer);
    printf("\t%10d afs_CheckCacheResets\n",
	   cmp->callInfo.C_afs_CheckCacheResets);
    printf("\t%10d afs_CheckVolumeNames\n",
	   cmp->callInfo.C_afs_CheckVolumeNames);
    printf("\t%10d afs_CheckCode\n", cmp->callInfo.C_afs_CheckCode);
    printf("\t%10d afs_CopyError\n", cmp->callInfo.C_afs_CopyError);
    printf("\t%10d afs_FinalizeReq\n", cmp->callInfo.C_afs_FinalizeReq);
    printf("\t%10d afs_GetVolCache\n", cmp->callInfo.C_afs_GetVolCache);
    printf("\t%10d afs_GetVolSlot\n", cmp->callInfo.C_afs_GetVolSlot);
    printf("\t%10d afs_UFSGetVolSlot\n", cmp->callInfo.C_afs_UFSGetVolSlot);
    printf("\t%10d afs_MemGetVolSlot\n", cmp->callInfo.C_afs_MemGetVolSlot);
    printf("\t%10d afs_WriteVolCache\n", cmp->callInfo.C_afs_WriteVolCache);
    printf("\t%10d haveCallbacksfrom\n", cmp->callInfo.C_HaveCallBacksFrom);
    printf("\t%10d afs_getpage\n", cmp->callInfo.C_afs_getpage);
    printf("\t%10d afs_putpage\n", cmp->callInfo.C_afs_putpage);
    printf("\t%10d afs_nfsrdwr\n", cmp->callInfo.C_afs_nfsrdwr);
    printf("\t%10d afs_map\n", cmp->callInfo.C_afs_map);
    printf("\t%10d afs_cmp\n", cmp->callInfo.C_afs_cmp);
    printf("\t%10d afs_PageLeft\n", cmp->callInfo.C_afs_PageLeft);
    printf("\t%10d afs_mount\n", cmp->callInfo.C_afs_mount);
    printf("\t%10d afs_unmount\n", cmp->callInfo.C_afs_unmount);
    printf("\t%10d afs_root\n", cmp->callInfo.C_afs_root);
    printf("\t%10d afs_statfs\n", cmp->callInfo.C_afs_statfs);
    printf("\t%10d afs_sync\n", cmp->callInfo.C_afs_sync);
    printf("\t%10d afs_vget\n", cmp->callInfo.C_afs_vget);
    printf("\t%10d afs_index\n", cmp->callInfo.C_afs_index);
    printf("\t%10d afs_setpag\n", cmp->callInfo.C_afs_setpag);
    printf("\t%10d genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10d getpag\n", cmp->callInfo.C_getpag);
    printf("\t%10d genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10d afs_GetMariner\n", cmp->callInfo.C_afs_GetMariner);
    printf("\t%10d afs_AddMarinerName\n", cmp->callInfo.C_afs_AddMarinerName);
    printf("\t%10d afs_open\n", cmp->callInfo.C_afs_open);
    printf("\t%10d afs_close\n", cmp->callInfo.C_afs_close);
    printf("\t%10d afs_closex\n", cmp->callInfo.C_afs_closex);
    printf("\t%10d afs_write\n", cmp->callInfo.C_afs_write);
    printf("\t%10d afs_UFSwrite\n", cmp->callInfo.C_afs_UFSWrite);
    printf("\t%10d afs_Memwrite\n", cmp->callInfo.C_afs_MemWrite);
    printf("\t%10d afs_rdwr\n", cmp->callInfo.C_afs_rdwr);
    printf("\t%10d afs_read\n", cmp->callInfo.C_afs_read);
    printf("\t%10d afs_UFSread\n", cmp->callInfo.C_afs_UFSRead);
    printf("\t%10d afs_Memread\n", cmp->callInfo.C_afs_MemRead);
    printf("\t%10d afs_CopyOutAttrs\n", cmp->callInfo.C_afs_CopyOutAttrs);
    printf("\t%10d afs_access\n", cmp->callInfo.C_afs_access);
    printf("\t%10d afs_getattr\n", cmp->callInfo.C_afs_getattr);
    printf("\t%10d afs_setattr\n", cmp->callInfo.C_afs_setattr);
    printf("\t%10d afs_VAttrToAS\n", cmp->callInfo.C_afs_VAttrToAS);
    printf("\t%10d EvalMountPoint\n", cmp->callInfo.C_EvalMountPoint);
    printf("\t%10d afs_lookup\n", cmp->callInfo.C_afs_lookup);
    printf("\t%10d afs_create\n", cmp->callInfo.C_afs_create);
    printf("\t%10d afs_LocalHero\n", cmp->callInfo.C_afs_LocalHero);
    printf("\t%10d afs_remove\n", cmp->callInfo.C_afs_remove);
    printf("\t%10d afs_link\n", cmp->callInfo.C_afs_link);
    printf("\t%10d afs_rename\n", cmp->callInfo.C_afs_rename);
    printf("\t%10d afs_InitReq\n", cmp->callInfo.C_afs_InitReq);
    printf("\t%10d afs_mkdir\n", cmp->callInfo.C_afs_mkdir);
    printf("\t%10d afs_rmdir\n", cmp->callInfo.C_afs_rmdir);
    printf("\t%10d afs_readdir\n", cmp->callInfo.C_afs_readdir);
    printf("\t%10d afs_read1dir\n", cmp->callInfo.C_afs_read1dir);
    printf("\t%10d afs_readdir_move\n", cmp->callInfo.C_afs_readdir_move);
    printf("\t%10d afs_readdir_iter\n", cmp->callInfo.C_afs_readdir_iter);
    printf("\t%10d afs_symlink\n", cmp->callInfo.C_afs_symlink);
    printf("\t%10d afs_HandleLink\n", cmp->callInfo.C_afs_HandleLink);
    printf("\t%10d afs_MemHandleLink\n", cmp->callInfo.C_afs_MemHandleLink);
    printf("\t%10d afs_UFSHandleLink\n", cmp->callInfo.C_afs_UFSHandleLink);
    printf("\t%10d HandleFlock\n", cmp->callInfo.C_HandleFlock);
    printf("\t%10d afs_readlink\n", cmp->callInfo.C_afs_readlink);
    printf("\t%10d afs_fsync\n", cmp->callInfo.C_afs_fsync);
    printf("\t%10d afs_inactive\n", cmp->callInfo.C_afs_inactive);
    printf("\t%10d afs_ustrategy\n", cmp->callInfo.C_afs_ustrategy);
    printf("\t%10d afs_strategy\n", cmp->callInfo.C_afs_strategy);
    printf("\t%10d afs_bread\n", cmp->callInfo.C_afs_bread);
    printf("\t%10d afs_brelse\n", cmp->callInfo.C_afs_brelse);
    printf("\t%10d afs_bmap\n", cmp->callInfo.C_afs_bmap);
    printf("\t%10d afs_fid\n", cmp->callInfo.C_afs_fid);
    printf("\t%10d afs_FakeOpen\n", cmp->callInfo.C_afs_FakeOpen);
    printf("\t%10d afs_FakeClose\n", cmp->callInfo.C_afs_FakeClose);
    printf("\t%10d afs_StoreOnLastReference\n",
	   cmp->callInfo.C_afs_StoreOnLastReference);
    printf("\t%10d afs_AccessOK\n", cmp->callInfo.C_afs_AccessOK);
    printf("\t%10d afs_GetAccessBits\n", cmp->callInfo.C_afs_GetAccessBits);
    printf("\t%10d afsio_copy\n", cmp->callInfo.C_afsio_copy);
    printf("\t%10d afsio_trim\n", cmp->callInfo.C_afsio_trim);
    printf("\t%10d afsio_skip\n", cmp->callInfo.C_afsio_skip);
    printf("\t%10d afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10d afs_page_write\n", cmp->callInfo.C_afs_page_write);
    printf("\t%10d afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10d afs_get_groups_from_pag\n",
	   cmp->callInfo.C_afs_get_groups_from_pag);
    printf("\t%10d afs_get_pag_from_groups\n",
	   cmp->callInfo.C_afs_get_pag_from_groups);
    printf("\t%10d AddPag\n", cmp->callInfo.C_AddPag);
    printf("\t%10d PagInCred\n", cmp->callInfo.C_PagInCred);
    printf("\t%10d afs_getgroups\n", cmp->callInfo.C_afs_getgroups);
    printf("\t%10d afs_page_in\n", cmp->callInfo.C_afs_page_in);
    printf("\t%10d afs_page_out\n", cmp->callInfo.C_afs_page_out);
    printf("\t%10d afs_AdvanceFD\n", cmp->callInfo.C_afs_AdvanceFD);
    printf("\t%10d afs_lockf\n", cmp->callInfo.C_afs_lockf);
    printf("\t%10d afs_xsetgroups\n", cmp->callInfo.C_afs_xsetgroups);
    printf("\t%10d afs_nlinks\n", cmp->callInfo.C_afs_nlinks);
    printf("\t%10d afs_lockctl\n", cmp->callInfo.C_afs_lockctl);
    printf("\t%10d afs_xflock\n", cmp->callInfo.C_afs_xflock);
    printf("\t%10d PGetCPrefs\n", cmp->callInfo.C_PGetCPrefs);
    printf("\t%10d PSetCPrefs\n", cmp->callInfo.C_PSetCPrefs);
#ifdef	AFS_HPUX_ENV
    printf("\t%10d afs_pagein\n", cmp->callInfo.C_afs_pagein);
    printf("\t%10d afs_pageout\n", cmp->callInfo.C_afs_pageout);
    printf("\t%10d afs_hp_strategy\n", cmp->callInfo.C_afs_hp_strategy);
#endif
    printf("\t%10d PFlushMount\n", cmp->callInfo.C_PFlushMount);
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
PrintUpDownStats(a_upDownP)
     struct afs_stats_SrvUpDownInfo *a_upDownP;	/*Ptr to server up/down info */

{				/*PrintUpDownStats */

    /*
     * First, print the simple values.
     */
    printf("\t\t%10d numTtlRecords\n", a_upDownP->numTtlRecords);
    printf("\t\t%10d numUpRecords\n", a_upDownP->numUpRecords);
    printf("\t\t%10d numDownRecords\n", a_upDownP->numDownRecords);
    printf("\t\t%10d sumOfRecordAges\n", a_upDownP->sumOfRecordAges);
    printf("\t\t%10d ageOfYoungestRecord\n", a_upDownP->ageOfYoungestRecord);
    printf("\t\t%10d ageOfOldestRecord\n", a_upDownP->ageOfOldestRecord);
    printf("\t\t%10d numDowntimeIncidents\n",
	   a_upDownP->numDowntimeIncidents);
    printf("\t\t%10d numRecordsNeverDown\n", a_upDownP->numRecordsNeverDown);
    printf("\t\t%10d maxDowntimesInARecord\n",
	   a_upDownP->maxDowntimesInARecord);
    printf("\t\t%10d sumOfDowntimes\n", a_upDownP->sumOfDowntimes);
    printf("\t\t%10d shortestDowntime\n", a_upDownP->shortestDowntime);
    printf("\t\t%10d longestDowntime\n", a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    printf("\t\tDowntime duration distribution:\n");
    printf("\t\t\t%8d: 0 min .. 10 min\n", a_upDownP->downDurations[0]);
    printf("\t\t\t%8d: 10 min .. 30 min\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8d: 30 min .. 1 hr\n", a_upDownP->downDurations[2]);
    printf("\t\t\t%8d: 1 hr .. 2 hr\n", a_upDownP->downDurations[3]);
    printf("\t\t\t%8d: 2 hr .. 4 hr\n", a_upDownP->downDurations[4]);
    printf("\t\t\t%8d: 4 hr .. 8 hr\n", a_upDownP->downDurations[5]);
    printf("\t\t\t%8d: > 8 hr\n", a_upDownP->downDurations[6]);

    printf("\t\tDowntime incident distribution:\n");
    printf("\t\t\t%8d: 0 times\n", a_upDownP->downIncidents[0]);
    printf("\t\t\t%8d: 1 time\n", a_upDownP->downIncidents[1]);
    printf("\t\t\t%8d: 2 .. 5 times\n", a_upDownP->downIncidents[2]);
    printf("\t\t\t%8d: 6 .. 10 times\n", a_upDownP->downIncidents[3]);
    printf("\t\t\t%8d: 10 .. 50 times\n", a_upDownP->downIncidents[4]);
    printf("\t\t\t%8d: > 50 times\n", a_upDownP->downIncidents[5]);

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
PrintOverallPerfInfo(a_ovP)
     struct afs_stats_CMPerf *a_ovP;

{				/*PrintOverallPerfInfo */

    printf("\t%10d numPerfCalls\n", a_ovP->numPerfCalls);

    printf("\t%10d epoch\n", a_ovP->epoch);
    printf("\t%10d numCellsVisible\n", a_ovP->numCellsVisible);
    printf("\t%10d numCellsContacted\n", a_ovP->numCellsContacted);
    printf("\t%10d dlocalAccesses\n", a_ovP->dlocalAccesses);
    printf("\t%10d vlocalAccesses\n", a_ovP->vlocalAccesses);
    printf("\t%10d dremoteAccesses\n", a_ovP->dremoteAccesses);
    printf("\t%10d vremoteAccesses\n", a_ovP->vremoteAccesses);
    printf("\t%10d cacheNumEntries\n", a_ovP->cacheNumEntries);
    printf("\t%10d cacheBlocksTotal\n", a_ovP->cacheBlocksTotal);
    printf("\t%10d cacheBlocksInUse\n", a_ovP->cacheBlocksInUse);
    printf("\t%10d cacheBlocksOrig\n", a_ovP->cacheBlocksOrig);
    printf("\t%10d cacheMaxDirtyChunks\n", a_ovP->cacheMaxDirtyChunks);
    printf("\t%10d cacheCurrDirtyChunks\n", a_ovP->cacheCurrDirtyChunks);
    printf("\t%10d dcacheHits\n", a_ovP->dcacheHits);
    printf("\t%10d vcacheHits\n", a_ovP->vcacheHits);
    printf("\t%10d dcacheMisses\n", a_ovP->dcacheMisses);
    printf("\t%10d vcacheMisses\n", a_ovP->vcacheMisses);
    printf("\t%10d cacheFilesReused\n", a_ovP->cacheFilesReused);
    printf("\t%10d vcacheXAllocs\n", a_ovP->vcacheXAllocs);
    printf("\t%10d dcacheXAllocs\n", a_ovP->dcacheXAllocs);

    printf("\t%10d bufAlloced\n", a_ovP->bufAlloced);
    printf("\t%10d bufHits\n", a_ovP->bufHits);
    printf("\t%10d bufMisses\n", a_ovP->bufMisses);
    printf("\t%10d bufFlushDirty\n", a_ovP->bufFlushDirty);

    printf("\t%10d LargeBlocksActive\n", a_ovP->LargeBlocksActive);
    printf("\t%10d LargeBlocksAlloced\n", a_ovP->LargeBlocksAlloced);
    printf("\t%10d SmallBlocksActive\n", a_ovP->SmallBlocksActive);
    printf("\t%10d SmallBlocksAlloced\n", a_ovP->SmallBlocksAlloced);
    printf("\t%10d OutStandingMemUsage\n", a_ovP->OutStandingMemUsage);
    printf("\t%10d OutStandingAllocs\n", a_ovP->OutStandingAllocs);
    printf("\t%10d CallBackAlloced\n", a_ovP->CallBackAlloced);
    printf("\t%10d CallBackFlushes\n", a_ovP->CallBackFlushes);
    printf("\t%10d CallBackLoops\n", a_ovP->cbloops);

    printf("\t%10d srvRecords\n", a_ovP->srvRecords);
    printf("\t%10d srvNumBuckets\n", a_ovP->srvNumBuckets);
    printf("\t%10d srvMaxChainLength\n", a_ovP->srvMaxChainLength);
    printf("\t%10d srvMaxChainLengthHWM\n", a_ovP->srvMaxChainLengthHWM);
    printf("\t%10d srvRecordsHWM\n", a_ovP->srvRecordsHWM);

    printf("\t%10d sysName_ID\n", a_ovP->sysName_ID);

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
PrintPerfInfo()
{				/*PrintPerfInfo */

    static char rn[] = "PrintPerfInfo";	/*Routine name */
    static afs_int32 perfInt32s = (sizeof(struct afs_stats_CMPerf) >> 2);	/*Correct # int32s to rcv */
    afs_int32 numInt32s;	/*# int32words received */
    struct afs_stats_CMPerf *perfP;	/*Ptr to performance stats */
    char *printableTime;	/*Ptr to printable time string */

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != perfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %d, got %d\n", perfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime((time_t *) & (xstat_cm_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';
    perfP = (struct afs_stats_CMPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_PERF_INFO (coll %d) for CM %s\n[Probe %d, %s]\n\n",
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
PrintOpTiming(a_opIdx, a_opNames, a_opTimeP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_opTimingData *a_opTimeP;

{				/*PrintOpTiming */

    printf
	("%15s: %d ops (%d OK); sum=%d.%06d, sqr=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	 a_opNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	 a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	 a_opTimeP->sqrTime.tv_sec, a_opTimeP->sqrTime.tv_usec,
	 a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	 a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);

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
PrintXferTiming(a_opIdx, a_opNames, a_xferP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_xferData *a_xferP;

{				/*PrintXferTiming */

    printf
	("%s: %d xfers (%d OK), time sum=%d.%06d, sqr=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	 a_opNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	 a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	 a_xferP->sqrTime.tv_sec, a_xferP->sqrTime.tv_usec,
	 a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	 a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%d, min=%d, max=%d]\n", a_xferP->sumBytes,
	   a_xferP->minBytes, a_xferP->maxBytes);
    printf
	("\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d, 6: %d, 7: %d, 8: %d]\n",
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
PrintErrInfo(a_opIdx, a_opNames, a_opErrP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_RPCErrors *a_opErrP;

{				/*PrintErrInfo */

    printf
	("%15s: %d server, %d network, %d prot, %d vol, %d busies, %d other\n",
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
PrintRPCPerfInfo(a_rpcP)
     struct afs_stats_RPCOpInfo *a_rpcP;

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
PrintFullPerfInfo()
{				/*PrintFullPerfInfo */

    struct afs_stats_AuthentInfo *authentP;	/*Ptr to authentication stats */
    struct afs_stats_AccessInfo *accessinfP;	/*Ptr to access stats */
    struct afs_stats_AuthorInfo *authorP;	/*Ptr to authorship stats */
    static afs_int32 fullPerfInt32s = (sizeof(struct afs_stats_CMFullPerf) >> 2);	/*Correct #int32s */
    afs_int32 numInt32s;	/*# int32s actually received */
    struct afs_stats_CMFullPerf *fullP;	/*Ptr to full perf info */

    char *printableTime;	/*Ptr to printable time string */

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != fullPerfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %d, got %d\n", fullPerfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime((time_t *) & (xstat_cm_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';
    fullP = (struct afs_stats_CMFullPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_FULL_PERF_INFO (coll %d) for CM %s\n[Probe %d, %s]\n\n",
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
	("\t%d PAGS, %d records (%d auth, %d unauth), %d max in PAG, chain max: %d\n",
	 authentP->curr_PAGs, authentP->curr_Records,
	 authentP->curr_AuthRecords, authentP->curr_UnauthRecords,
	 authentP->curr_MaxRecordsInPAG, authentP->curr_LongestChain);
    printf("\t%d PAG creations, %d tkt updates\n", authentP->PAGCreations,
	   authentP->TicketUpdates);
    printf("\t[HWMs: %d PAGS, %d records, %d max in PAG, chain max: %d]\n",
	   authentP->HWM_PAGs, authentP->HWM_Records,
	   authentP->HWM_MaxRecordsInPAG, authentP->HWM_LongestChain);

    accessinfP = &(fullP->accessinf);
    printf("\n[Un]replicated accesses:\n------------------------\n");
    printf
	("\t%d unrep, %d rep, %d reps accessed, %d max reps/ref, %d first OK\n\n",
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
CM_Handler()
{				/*CM_Handler */

    static char rn[] = "CM_Handler";	/*Routine name */

    printf("\n-----------------------------------------------------------\n");

    /*
     * If the probe failed, there isn't much we can do except gripe.
     */
    if (xstat_cm_Results.probeOK) {
	printf("%s: Probe %d, collection %d to CM on '%s' failed, code=%d\n",
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
CountListItems(a_firstItem)
     struct cmd_item *a_firstItem;

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
RunTheTest(a_s)
     struct cmd_syndesc *a_s;

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
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
	CMSktArray[currCM].sin_family = AF_INET;	/*Internet family */
#else
	CMSktArray[currCM].sin_family = htons(AF_INET);	/*Internet family */
#endif
	CMSktArray[currCM].sin_port = htons(7001);	/*Cache Manager port */
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
	    printf("[%s] Calling LWP_WaitProcess() on event 0x%x\n", rn,
		   &terminationEvent);
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

main(argc, argv)
     int argc;
     char **argv;

{				/*Main routine */

    static char rn[] = "xstat_cm_test";	/*Routine name */
    register afs_int32 code;	/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to cmd line syntax desc */

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", RunTheTest, 0, "initialize the program");
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
