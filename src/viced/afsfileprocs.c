/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  afs_fileprocs.c - Complete File Server request routines		 */
/* 									 */
/*  Information Technology Center					 */
/*  Carnegie Mellon University						 */
/* 									 */
/*  Date: 8/10/88							 */
/* 									 */
/*  Function	- A set	of routines to handle the various file Server	 */
/*		    requests; these routines are invoked by rxgen.	 */
/* 									 */
/* ********************************************************************** */

/* 
 * in Check_PermissionRights, certain privileges are afforded to the owner 
 * of the volume, or the owner of a file.  Are these considered "use of 
 * privilege"? 
 */

#include <afs/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef	AFS_SGI_ENV
#undef SHARED		/* XXX */
#endif
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef AFS_LINUX20_ENV
#include <net/if.h>
#include <netinet/if_ether.h>
#endif
#ifdef notdef
#include <nlist.h>
#endif
#endif
#ifdef AFS_HPUX_ENV
/* included early because of name conflict on IOPEN */
#include <sys/inode.h>
#ifdef IOPEN
#undef IOPEN
#endif
#endif /* AFS_HPUX_ENV */
#include <afs/stds.h>
#include <rx/xdr.h>
#include <afs/nfs.h>
#include <afs/assert.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsint.h>
#include <afs/vldbint.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/acl.h>
#include <afs/ptclient.h>
#include <afs/prs_fs.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <sys/stat.h>
#if ! defined(AFS_SGI_ENV) && ! defined(AFS_AIX32_ENV) && ! defined(AFS_NT40_ENV) && ! defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_FBSD_ENV)
#include <sys/map.h>
#endif
#if !defined(AFS_NT40_ENV)
#include <unistd.h>
#endif
#if !defined(AFS_SGI_ENV) && !defined(AFS_NT40_ENV)
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#include <sys/lockf.h>
#else
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_FBSD_ENV)
#include <sys/dk.h>
#endif
#endif
#endif
#include <afs/cellconfig.h>
#include <afs/keys.h>

#include <afs/auth.h>
#include <signal.h>
#include <afs/partition.h>
#include "viced.h"
#include "host.h"
#include <afs/audit.h>
#include <afs/afsutil.h>

#ifdef AFS_PTHREAD_ENV
pthread_mutex_t fileproc_glock_mutex;
#endif /* AFS_PTHREAD_ENV */


/* Useful local defines used by this module */

#define	DONTCHECK	0
#define	MustNOTBeDIR	1
#define	MustBeDIR	2

#define	TVS_SDATA	1
#define	TVS_SSTATUS	2
#define	TVS_CFILE	4
#define	TVS_SLINK	8
#define	TVS_MKDIR	0x10

#define	CHK_FETCH	0x10
#define	CHK_FETCHDATA	0x10
#define	CHK_FETCHACL	0x11
#define	CHK_FETCHSTATUS	0x12
#define	CHK_STOREDATA	0x00
#define	CHK_STOREACL	0x01
#define	CHK_STORESTATUS	0x02

#define	OWNERREAD	0400
#define	OWNERWRITE	0200
#define	OWNEREXEC	0100
#ifdef USE_GROUP_PERMS
#define GROUPREAD       0040
#define GROUPWRITE      0020
#define	GROUPREXEC	0010
#endif

/* The following errors were not defined in NT. They are given unique
 * names here to avoid any potential collision.
 */
#define FSERR_ELOOP 		 90
#define FSERR_EOPNOTSUPP	122
#define FSERR_ECONNREFUSED	130

#define	NOTACTIVECALL	0
#define	ACTIVECALL	1

extern struct afsconf_dir *confDir;
extern afs_int32 dataVersionHigh;

extern	int	    SystemId;
extern	struct AFSCallStatistics AFSCallStats;
struct AFSCallStatistics AFSCallStats;
#if FS_STATS_DETAILED
struct fs_stats_FullPerfStats afs_FullPerfStats;
extern int AnonymousID;
#endif /* FS_STATS_DETAILED */
#if TRANSARC_VOL_STATS
static const char nullString[] = "";
#endif /* TRANSARC_VOL_STATS */

struct afs_FSStats {
    afs_int32 NothingYet;
};

struct afs_FSStats afs_fsstats;

void	ResetDebug(), SetDebug(), GetStatus(), Terminate();
int	CopyOnWrite();		/* returns 0 on success */


void SetSystemStats(), SetAFSStats(), SetVolumeStats();
int	LogLevel = 0;
int	supported = 1;
int	Console = 0;
afs_int32 BlocksSpare = 1024;	/* allow 1 MB overruns */
afs_int32 PctSpare;
extern afs_int32 implicitAdminRights;

static TryLocalVLServer();

/*
 * Externals used by the xstat code.
 */
extern int VolumeCacheSize, VolumeGets, VolumeReplacements;
extern int CEs, CEBlocks;

extern int HTs, HTBlocks;

#ifdef AFS_SGI_XFS_IOPS_ENV
#include <afs/xfsattrs.h>
static int GetLinkCount(avp, astat)
     Volume *avp;
     struct stat *astat;
{
    if (!strcmp("xfs", astat->st_fstype)) {
	return (astat->st_mode & AFS_XFS_MODE_LINK_MASK);
    }
    else
	return astat->st_nlink;
}
#else
#define GetLinkCount(V, S) (S)->st_nlink
#endif

afs_int32 SpareComp(avolp)
    Volume *avolp;

{
    register afs_int32 temp;

    FS_LOCK
    if (PctSpare) {
	temp = V_maxquota(avolp);
	if (temp == 0) {
	    /* no matter; doesn't check in this case */
	    FS_UNLOCK
	    return 0;
	}
	temp = (temp * PctSpare) / 100;
	FS_UNLOCK
	return temp;
    }
    else {
	FS_UNLOCK
	return BlocksSpare;
    }

} /*SpareComp*/


/*
 * Set the volume synchronization parameter for this volume.  If it changes,
 * the Cache Manager knows that the volume must be purged from the stat cache.
 */
static SetVolumeSync(async, avol)
    register struct AFSVolSync *async;
    register Volume *avol;

{
    FS_LOCK
    /* date volume instance was created */
    if (async) {
	if (avol)
	    async->spare1 = avol->header->diskstuff.creationDate;
	else
	    async->spare1 = 0;
	async->spare2 = 0;
	async->spare3 = 0;
	async->spare4 = 0;
	async->spare5 = 0;
	async->spare6 = 0;
    }
    FS_UNLOCK
} /*SetVolumeSync*/

/*
 * This call overwrites the pointed-to rx_call with an rx_connection.  This
 * is really bogus.  Note that this function always returns a held host, so
 * that CallPostamble can block without the host's disappearing.
 */
static CallPreamble(acall, activecall)
    register struct rx_call **acall;
    int activecall;

{
    struct host *thost;
    struct rx_connection *tconn;
    struct client *tclient;
    int retry_flag=1;
    int code = 0;
    tconn = rx_ConnectionOf(*acall);
    *acall = (struct rx_call *)tconn;	    /* change it! */

    H_LOCK
retry:
    tclient = h_FindClient_r(tconn);
    if (tclient->prfail == 1) {	/* couldn't get the CPS */
       if (!retry_flag) {
	  h_ReleaseClient_r(tclient);
	  ViceLog(0, ("CallPreamble: Couldn't get CPS. Fail\n"));
	  H_UNLOCK
	  return -1001;
       }
       retry_flag=0;	/* Retry once */

       /* Take down the old connection and re-read the key file */
       ViceLog(0, ("CallPreamble: Couldn't get CPS. Reconnect to ptserver\n"));
       H_UNLOCK
       code = pr_Initialize(2, AFSDIR_SERVER_ETC_DIRPATH, 0);
       H_LOCK
       if (code) {
	  h_ReleaseClient_r(tclient);
	  H_UNLOCK
	  ViceLog(0,("CallPreamble: couldn't reconnect to ptserver\n"));
	  return -1001;
       }

       tclient->prfail = 2;      /* Means re-eval client's cps */
       h_ReleaseClient_r(tclient);
       goto retry;
    }

    thost = tclient->host;
    tclient->LastCall = thost->LastCall = FT_ApproxTime();
    if (activecall) /* For all but "GetTime" calls */
	thost->ActiveCall = thost->LastCall;

    h_Lock_r(thost);
    if (thost->hostFlags & HOSTDELETED) {
      ViceLog(3,("Discarded a packet for deleted host %08x\n",thost->host));
      code = VBUSY; /* raced, so retry */
    }
    else if (thost->hostFlags & VENUSDOWN) {
      if (BreakDelayedCallBacks_r(thost)) {
	ViceLog(0,("BreakDelayedCallbacks FAILED for host %08x which IS UP.  Possible network or routing failure.\n",thost->host));
	if ( MultiProbeAlternateAddress_r (thost) ) {
	    ViceLog(0, ("MultiProbe failed to find new address for host %x.%d\n",
			thost->host, thost->port));
	    code = -1;
	} else {
	    ViceLog(0, ("MultiProbe found new address for host %x.%d\n",
			thost->host, thost->port));
	    if (BreakDelayedCallBacks_r(thost)) {
		ViceLog(0,("BreakDelayedCallbacks FAILED AGAIN for host %08x which IS UP.  Possible network or routing failure.\n",thost->host));
		code = -1;
	    }
	}
      }
    } else {
       code =  0;
    }

    h_ReleaseClient_r(tclient);
    h_Unlock_r(thost);
    H_UNLOCK
    return code;      

} /*CallPreamble*/


static CallPostamble(aconn)
    register struct rx_connection *aconn;

{
    struct host *thost;
    struct client *tclient;

    H_LOCK
    tclient = h_FindClient_r(aconn);
    thost = tclient->host;
    h_ReleaseClient_r(tclient);
    h_Release_r(thost);
    H_UNLOCK

} /*CallPostamble*/


#define	AFSV_BUFFERSIZE	16384 

static struct afs_buffer {
    struct afs_buffer *next;
} *freeBufferList = 0;
static int afs_buffersAlloced = 0;


static FreeSendBuffer(adata)
    register struct afs_buffer *adata;

{
    FS_LOCK
    afs_buffersAlloced--;
    adata->next = freeBufferList;
    freeBufferList = adata;
    FS_UNLOCK
    return 0;

} /*FreeSendBuffer*/


/* allocate space for sender */
static char *AllocSendBuffer()

{
    register struct afs_buffer *tp;

    FS_LOCK
    afs_buffersAlloced++;
    if (!freeBufferList) {
	FS_UNLOCK
	return malloc(AFSV_BUFFERSIZE);
    }
    tp = freeBufferList;
    freeBufferList = tp->next;
    FS_UNLOCK
    return (char *) tp;

} /*AllocSendBuffer*/


static int VolumeOwner (client, targetptr)
    register struct client *client;
    register Vnode *targetptr;

{
    afs_int32 owner = V_owner(targetptr->volumePtr);	/* get volume owner */

    if (owner >= 0)
	return (client->ViceId == owner);
    else {
	/* 
	 * We don't have to check for host's cps since only regular
	 * viceid are volume owners.
	 */
	return (acl_IsAMember(owner, &client->CPS));
    }

} /*VolumeOwner*/


static int VolumeRootVnode (targetptr)
    Vnode *targetptr;

{
    return ((targetptr->vnodeNumber == ROOTVNODE) &&
	    (targetptr->disk.uniquifier == 1));

} /*VolumeRootVnode*/


SRXAFS_FetchData (tcon, Fid, Pos, Len, OutStatus, CallBack, Sync)
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSFid *Fid;			/* Fid of file to fetch */
    afs_int32 Pos, Len;	   		/* Not implemented yet */
    struct AFSFetchStatus *OutStatus;	/* Returned status for Fid */
    struct AFSCallBack *CallBack;	/* If r/w return CB for Fid */
    struct AFSVolSync *Sync;		/* synchronization info */

{
    Vnode * targetptr =	0;		    /* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	    /* parent vnode if vptr is a file */
    Vnode   tparentwhentargetnotdir;	    /* parent vnode for GetStatus */
    int	    errorCode =	0;		    /* return code to caller */
    int	    fileCode =	0;		    /* return code from vol package */
    Volume * volptr = 0;		    /* pointer to the volume */
    struct client *client;		    /* pointer to the client data */
    struct rx_call *tcall;		    /* the call we're a part of */
    afs_int32 rights, anyrights;		    /* rights for this and any user */
    struct client *t_client;                /* tmp ptr to client data */
    struct in_addr logHostAddr;		    /* host ip holder for inet_ntoa */
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct fs_stats_xferData *xferP;	    /* Ptr to this op's byte size struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval xferStartTime,
                   xferStopTime;	    /* Start/stop times for xfer portion*/
    struct timeval elapsedTime;		    /* Transfer time */
    afs_int32 bytesToXfer;			    /* # bytes to xfer*/
    afs_int32 bytesXferred;			    /* # bytes actually xferred*/
    int readIdx;			    /* Index of read stats array to bump*/
    static afs_int32 tot_bytesXferred; /* shared access protected by FS_LOCK */

    /*
     * Set our stats pointers, remember when the RPC operation started, and
     * tally the operation.
     */
    opP   = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_FETCHDATA]);
    xferP = &(afs_FullPerfStats.det.xferOpTimes[FS_STATS_XFERIDX_FETCHDATA]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1,("SRXAFS_FetchData, Fid = %u.%d.%d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique));	
    FS_LOCK
    AFSCallStats.FetchData++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    /* CallPreamble changes tcon from a call to a conn */
    tcall = (struct rx_call *) tcon;
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_FetchData;

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *)  rx_GetSpecific(tcon, rxcon_client_key);
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(5,("SRXAFS_FetchData, Fid = %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));	
    /*
     * Get volume/vnode for the fetched file; caller's access rights to
     * it are also returned
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, READ_LOCK, &rights, &anyrights))
	goto Bad_FetchData;

    SetVolumeSync(Sync, volptr);

#if FS_STATS_DETAILED
    /*
     * Remember that another read operation was performed.
     */
    FS_LOCK
    if (client->InSameNetwork)
	readIdx = VOL_STATS_SAME_NET;
    else
	readIdx = VOL_STATS_DIFF_NET;
    V_stat_reads(volptr, readIdx)++;
    if (client->ViceId != AnonymousID) {
	V_stat_reads(volptr, readIdx+1)++;
    }
    FS_UNLOCK
#endif /* FS_STATS_DETAILED */

    /* Check whether the caller has permission access to fetch the data */
    if (errorCode = Check_PermissionRights(targetptr, client, rights,
					   CHK_FETCHDATA, 0))
	goto Bad_FetchData;

    /*
     * Drop the read lock on the parent directory after saving the parent
     * vnode information we need to pass to GetStatus
     */
    if (parentwhentargetnotdir != NULL) {
	tparentwhentargetnotdir = *parentwhentargetnotdir;
	VPutVnode(&fileCode, parentwhentargetnotdir);
	assert(!fileCode || (fileCode == VSALVAGE));
	parentwhentargetnotdir = NULL;
    }

#if FS_STATS_DETAILED
    /*
     * Remember when the data transfer started.
     */
    TM_GetTimeOfDay(&xferStartTime, 0);
#endif /* FS_STATS_DETAILED */

    /* actually do the data transfer */
#if FS_STATS_DETAILED
    errorCode = FetchData_RXStyle(volptr, targetptr, tcall, Pos, Len,
				  &bytesToXfer, &bytesXferred);
#else
    if (errorCode = FetchData_RXStyle(volptr, targetptr, tcall, Pos, Len))
	goto Bad_FetchData;
#endif /* FS_STATS_DETAILED */

#if FS_STATS_DETAILED
    /*
     * At this point, the data transfer is done, for good or ill.  Remember
     * when the transfer ended, bump the number of successes/failures, and
     * integrate the transfer size and elapsed time into the stats.  If the
     * operation failed, we jump to the appropriate point.
     */
    TM_GetTimeOfDay(&xferStopTime, 0);
    FS_LOCK
    (xferP->numXfers)++;
    if (!errorCode) {
	(xferP->numSuccesses)++;

        /*
         * Bump the xfer sum by the number of bytes actually sent, NOT the
         * target number.
         */
	tot_bytesXferred += bytesXferred;
        (xferP->sumBytes) += (tot_bytesXferred >> 10);
	tot_bytesXferred &= 0x3FF;
        if (bytesXferred < xferP->minBytes)
	    xferP->minBytes = bytesXferred;
        if (bytesXferred > xferP->maxBytes)
	    xferP->maxBytes = bytesXferred;

        /*
         * Tally the size of the object.  Note: we tally the actual size,
         * NOT the number of bytes that made it out over the wire.
         */
        if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET0)
	    (xferP->count[0])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET1)
	        (xferP->count[1])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET2)
	        (xferP->count[2])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET3)
	        (xferP->count[3])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET4)
	        (xferP->count[4])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET5)
                (xferP->count[5])++;
        else
            if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET6)
                (xferP->count[6])++;
        else
            if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET7)
                (xferP->count[7])++;
        else
            (xferP->count[8])++;

        fs_stats_GetDiff(elapsedTime, xferStartTime, xferStopTime);
        fs_stats_AddTo((xferP->sumTime), elapsedTime);
        fs_stats_SquareAddTo((xferP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (xferP->minTime))) {
	    fs_stats_TimeAssign((xferP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (xferP->maxTime))) {
	    fs_stats_TimeAssign((xferP->maxTime), elapsedTime);
        }
      }
    FS_UNLOCK
    /*
     * Finally, go off to tell our caller the bad news in case the
     * fetch failed.
     */
    if (errorCode)
	goto Bad_FetchData;
#endif /* FS_STATS_DETAILED */

    /* write back  the OutStatus from the target vnode  */
    GetStatus(targetptr, OutStatus, rights, anyrights, &tparentwhentargetnotdir);

    /* if a r/w volume, promise a callback to the caller */
    if (VolumeWriteable(volptr))
	SetCallBackStruct(AddCallBack(client->host, Fid), CallBack);
    else {
      struct AFSFid myFid;		
      bzero(&myFid, sizeof(struct AFSFid));
      myFid.Volume = Fid->Volume;
      SetCallBackStruct(AddVolCallBack(client->host, &myFid), CallBack);
      }

Bad_FetchData: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SRXAFS_FetchData returns %d\n", errorCode)); 
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
	FS_LOCK
	(opP->numSuccesses)++;
        fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
        fs_stats_AddTo((opP->sumTime), elapsedTime);
	fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	    fs_stats_TimeAssign((opP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	    fs_stats_TimeAssign((opP->maxTime), elapsedTime);
        }
	FS_UNLOCK
      }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, FetchDataEvent, errorCode, AUD_FID, Fid, AUD_END);
    return(errorCode);

} /*SRXAFS_FetchData*/


SRXAFS_FetchACL (tcon, Fid, AccessList, OutStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSFid *Fid;			/* Fid of target dir */
    struct AFSOpaque *AccessList;	/* Returned contents of dir's ACL */
    struct AFSFetchStatus *OutStatus;	/* Returned status for the dir */

{
    Vnode * targetptr =	0;		/* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	/* parent vnode if targetptr is a file */
    int	    errorCode =	0;		/* return error code to caller */
    Volume * volptr = 0;		/* pointer to the volume */
    struct client *client;		/* pointer to the client data */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct rx_call *tcall = (struct rx_call *) tcon; 
    struct client *t_client;                /* tmp ptr to client data */
    struct in_addr logHostAddr;		    /* host ip holder for inet_ntoa */
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_FETCHACL]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1, ("SAFS_FetchACL, Fid = %u.%d.%d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique));
    FS_LOCK
    AFSCallStats.FetchACL++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_FetchACL;

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *)  rx_GetSpecific(tcon, rxcon_client_key);
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(5, ("SAFS_FetchACL, Fid = %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));

    AccessList->AFSOpaque_len = 0;
    AccessList->AFSOpaque_val = malloc(AFSOPAQUEMAX);

    /*
     * Get volume/vnode for the fetched file; caller's access rights to it
     * are also returned
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, READ_LOCK, &rights, &anyrights))
	goto Bad_FetchACL;

    SetVolumeSync(Sync, volptr);

    /* Check whether we have permission to fetch the ACL */
    if (errorCode = Check_PermissionRights(targetptr, client, rights,
					   CHK_FETCHACL, 0))
	goto Bad_FetchACL;

    /* Get the Access List from the dir's vnode */
    if (errorCode = RXFetch_AccessList(targetptr, parentwhentargetnotdir,
				       AccessList))
	goto Bad_FetchACL;

    /* Get OutStatus back From the target Vnode  */
    GetStatus(targetptr, OutStatus, rights, anyrights, parentwhentargetnotdir);

Bad_FetchACL: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_FetchACL returns %d (ACL=%s)\n",
	    errorCode, AccessList->AFSOpaque_val));
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
 	FS_LOCK
        (opP->numSuccesses)++;
        fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
        fs_stats_AddTo((opP->sumTime), elapsedTime);
	fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	    fs_stats_TimeAssign((opP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	    fs_stats_TimeAssign((opP->maxTime), elapsedTime);
        }
	FS_UNLOCK
      }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, FetchACLEvent, errorCode, AUD_FID, Fid, AUD_END);
    return errorCode;

} /*SRXAFS_FetchACL*/


/*
 * This routine is called exclusively by SRXAFS_FetchStatus(), and should be
 * merged into it when possible.
 */
SAFSS_FetchStatus (tcon, Fid, OutStatus, CallBack, Sync)
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSFid *Fid;			/* Fid of target file */
    struct AFSFetchStatus *OutStatus;	/* Returned status for the fid */
    struct AFSCallBack *CallBack;	/* if r/w, callback promise for Fid */
    struct AFSVolSync *Sync;		/* volume synchronization parm */

{
    Vnode * targetptr =	0;		/* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	/* parent vnode if targetptr is a file */
    int	    errorCode =	0;		/* return code to caller */
    Volume * volptr = 0;		/* pointer to the volume */
    struct client *client;		/* pointer to the client data */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *)  rx_GetSpecific(tcon, rxcon_client_key);
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_FetchStatus,  Fid = %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.FetchStatus++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /*
     * Get volume/vnode for the fetched file; caller's rights to it are
     * also returned
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, READ_LOCK, &rights, &anyrights))
	goto Bad_FetchStatus;

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Are we allowed to fetch Fid's status? */
    if (targetptr->disk.type != vDirectory) {
      if (errorCode = Check_PermissionRights(targetptr, client, rights,
					     CHK_FETCHSTATUS, 0))
	goto Bad_FetchStatus;
    }

    /* set OutStatus From the Fid  */
    GetStatus(targetptr, OutStatus, rights, anyrights, parentwhentargetnotdir);

    /* If a r/w volume, also set the CallBack state */
    if (VolumeWriteable(volptr))
	SetCallBackStruct(AddCallBack(client->host, Fid), CallBack);
    else {
      struct AFSFid myFid;		
      bzero(&myFid, sizeof(struct AFSFid));
      myFid.Volume = Fid->Volume;
      SetCallBackStruct(AddVolCallBack(client->host, &myFid), CallBack);
      }

Bad_FetchStatus: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_FetchStatus returns %d\n", errorCode)); 
    return errorCode;

} /*SAFSS_FetchStatus*/


SRXAFS_BulkStatus(tcon, Fids, OutStats, CallBacks, Sync)
    struct rx_connection *tcon;
    struct AFSCBFids *Fids;
    struct AFSBulkStats *OutStats;
    struct AFSCBs *CallBacks;
    struct AFSVolSync *Sync;

{
    register int i;
    afs_int32 nfiles;
    Vnode * targetptr =	0;		/* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	/* parent vnode if targetptr is a file */
    int	    errorCode =	0;		/* return code to caller */
    Volume * volptr = 0;		/* pointer to the volume */
    struct client *client;		/* pointer to the client data */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    register struct AFSFid *tfid;	/* file id we're dealing with now */
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_BULKSTATUS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1, ("SAFS_BulkStatus\n"));
    FS_LOCK
    AFSCallStats.TotalCalls++;
    FS_UNLOCK

    nfiles = Fids->AFSCBFids_len;	/* # of files in here */
    if (nfiles <= 0) {                  /* Sanity check */
	errorCode = EINVAL;
	goto Audit_and_Return;
    }

    /* allocate space for return output parameters */
    OutStats->AFSBulkStats_val = (struct AFSFetchStatus *)
	malloc(nfiles * sizeof(struct AFSFetchStatus));
    OutStats->AFSBulkStats_len = nfiles;
    CallBacks->AFSCBs_val = (struct AFSCallBack *)
	malloc(nfiles * sizeof(struct AFSCallBack));
    CallBacks->AFSCBs_len = nfiles;

    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_BulkStatus;

    tfid = Fids->AFSCBFids_val;
    for (i=0; i<nfiles; i++, tfid++) {
	/*
	 * Get volume/vnode for the fetched file; caller's rights to it
	 * are also returned
	 */
	if (errorCode =
	    GetVolumePackage(tcon, tfid, &volptr, &targetptr,
			     DONTCHECK, &parentwhentargetnotdir, &client,
			     READ_LOCK, &rights, &anyrights))
	    	goto Bad_BulkStatus;
	/* set volume synchronization information, but only once per call */
	if (i == nfiles)
	    SetVolumeSync(Sync, volptr);

	/* Are we allowed to fetch Fid's status? */
	if (targetptr->disk.type != vDirectory) {
	  if (errorCode = Check_PermissionRights(targetptr, client, rights,
					       CHK_FETCHSTATUS, 0))
	     	goto Bad_BulkStatus;
	}

	/* set OutStatus From the Fid  */
	GetStatus(targetptr, &OutStats->AFSBulkStats_val[i],
		  rights, anyrights, parentwhentargetnotdir);

	/* If a r/w volume, also set the CallBack state */
	if (VolumeWriteable(volptr))
	    SetCallBackStruct(AddBulkCallBack(client->host, tfid),
			      &CallBacks->AFSCBs_val[i]);
	else {
	  struct AFSFid myFid;		
	  bzero(&myFid, sizeof(struct AFSFid));
	  myFid.Volume = tfid->Volume;
	  SetCallBackStruct(AddVolCallBack(client->host, &myFid),
			      &CallBacks->AFSCBs_val[i]);
	}

	/* put back the file ID and volume */
	PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *) 0, volptr);
	parentwhentargetnotdir = (Vnode *) 0;
	targetptr = (Vnode *) 0;
	volptr = (Volume *) 0;
    }

Bad_BulkStatus: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
	FS_LOCK
        (opP->numSuccesses)++;
        fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
        fs_stats_AddTo((opP->sumTime), elapsedTime);
        fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	    fs_stats_TimeAssign((opP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	    fs_stats_TimeAssign((opP->maxTime), elapsedTime);
        }
	FS_UNLOCK
    }	

#endif /* FS_STATS_DETAILED */

Audit_and_Return:
    ViceLog(2, ("SAFS_BulkStatus	returns	%d\n", errorCode)); 
    osi_auditU (tcall, BulkFetchStatusEvent, errorCode, AUD_FIDS, Fids, AUD_END);
    return errorCode;

} /*SRXAFS_BulkStatus*/


SRXAFS_FetchStatus (tcon, Fid, OutStatus, CallBack, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSFid *Fid;			/* Fid of target file */
    struct AFSFetchStatus *OutStatus;	/* Returned status for the fid */
    struct AFSCallBack *CallBack;	/* if r/w, callback promise for Fid */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_FETCHSTATUS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_FetchStatus;

    code = SAFSS_FetchStatus (tcon, Fid, OutStatus, CallBack, Sync);

Bad_FetchStatus:    
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
	FS_LOCK
        (opP->numSuccesses)++;
        fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
        fs_stats_AddTo((opP->sumTime), elapsedTime);
        fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	    fs_stats_TimeAssign((opP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	    fs_stats_TimeAssign((opP->maxTime), elapsedTime);
        }
	FS_UNLOCK
      }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, FetchStatusEvent, code, AUD_FID, Fid, AUD_END);
    return code;

} /*SRXAFS_FetchStatus*/


SRXAFS_StoreData (tcon, Fid, InStatus, Pos, Length, FileLength, OutStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		/* Rx connection Handle */
    struct AFSFid *Fid;			/* Fid of taret file */
    struct AFSStoreStatus *InStatus;	/* Input Status for Fid */
    afs_int32 Pos;				/* Not implemented yet */
    afs_int32 Length;			/* Length of data to store */
    afs_int32 FileLength;			/* Length of file after store */
    struct AFSFetchStatus *OutStatus;	/* Returned status for target fid */

{
    Vnode * targetptr =	0;		/* pointer to input fid */
    Vnode * parentwhentargetnotdir = 0;	/* parent of Fid to get ACL */
    Vnode   tparentwhentargetnotdir;	/* parent vnode for GetStatus */
    int	    errorCode =	0;		/* return code for caller */
    int	    fileCode =	0;		/* return code from vol package */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    struct rx_call *tcall;		/* remember the call structure for ftp */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		    /* host ip holder for inet_ntoa */
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct fs_stats_xferData *xferP;	    /* Ptr to this op's byte size struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval xferStartTime,
                   xferStopTime;	    /* Start/stop times for xfer portion*/
    struct timeval elapsedTime;		    /* Transfer time */
    afs_int32 bytesToXfer;			    /* # bytes to xfer */
    afs_int32 bytesXferred;			    /* # bytes actually xfer */
    static afs_int32 tot_bytesXferred; /* shared access protected by FS_LOCK */

    /*
     * Set our stats pointers, remember when the RPC operation started, and
     * tally the operation.
     */
    opP   = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_STOREDATA]);
    xferP = &(afs_FullPerfStats.det.xferOpTimes[FS_STATS_XFERIDX_STOREDATA]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK

    ViceLog(1, ("StoreData: Fid = %u.%d.%d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique));
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    FS_LOCK
    AFSCallStats.StoreData++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /* CallPreamble changes tcon from a call to a conn */
    tcall = (struct rx_call *) tcon;
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_StoreData;

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(5, ("StoreData: Fid = %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));

    /*
     * Get associated volume/vnode for the stored file; caller's rights
     * are also returned
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     MustNOTBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_StoreData;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    if ((targetptr->disk.type == vSymlink)) {
	/* Should we return a better error code here??? */
	errorCode = EISDIR;
	goto Bad_StoreData;
    }

    /* Check if we're allowed to store the data */
    if (errorCode = Check_PermissionRights(targetptr, client, rights,
					   CHK_STOREDATA, InStatus)) {
	goto Bad_StoreData;
    }

    /*
     * Drop the read lock on the parent directory after saving the parent
     * vnode information we need to pass to GetStatus
     */
    if (parentwhentargetnotdir != NULL) {
	tparentwhentargetnotdir = *parentwhentargetnotdir;
	VPutVnode(&fileCode, parentwhentargetnotdir);
	assert(!fileCode || (fileCode == VSALVAGE));
	parentwhentargetnotdir = NULL;
    }



#if FS_STATS_DETAILED
    /*
     * Remember when the data transfer started.
     */
    TM_GetTimeOfDay(&xferStartTime, 0);
#endif /* FS_STATS_DETAILED */

    /* Do the actual storing of the data */
#if FS_STATS_DETAILED
    errorCode = StoreData_RXStyle(volptr, targetptr, Fid, client, tcall,
				  Pos, Length, FileLength,
				  (InStatus->Mask & AFS_FSYNC),
				  &bytesToXfer, &bytesXferred);
#else
    errorCode = StoreData_RXStyle(volptr, targetptr, Fid, client,
				      tcall, Pos, Length, FileLength,
				      (InStatus->Mask & AFS_FSYNC));
    if (errorCode && (!targetptr->changed_newTime))
	    goto Bad_StoreData;
#endif /* FS_STATS_DETAILED */
#if FS_STATS_DETAILED
    /*
     * At this point, the data transfer is done, for good or ill.  Remember
     * when the transfer ended, bump the number of successes/failures, and
     * integrate the transfer size and elapsed time into the stats.  If the
     * operation failed, we jump to the appropriate point.
     */
    TM_GetTimeOfDay(&xferStopTime, 0);
    FS_LOCK
    (xferP->numXfers)++;
    if (!errorCode) {
	(xferP->numSuccesses)++;

        /*
         * Bump the xfer sum by the number of bytes actually sent, NOT the
         * target number.
         */
	tot_bytesXferred += bytesXferred;
        (xferP->sumBytes) += (tot_bytesXferred >> 10);
	tot_bytesXferred &= 0x3FF;
        if (bytesXferred < xferP->minBytes)
	    xferP->minBytes = bytesXferred;
        if (bytesXferred > xferP->maxBytes)
	    xferP->maxBytes = bytesXferred;
      
        /*
         * Tally the size of the object.  Note: we tally the actual size,
         * NOT the number of bytes that made it out over the wire.
         */
        if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET0)
	    (xferP->count[0])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET1)
	        (xferP->count[1])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET2)
	        (xferP->count[2])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET3)
	        (xferP->count[3])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET4)
	        (xferP->count[4])++;
        else
	    if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET5)
                (xferP->count[5])++;
        else
            if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET6)
                (xferP->count[6])++;
        else
            if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET7)
                (xferP->count[7])++;
        else
	    (xferP->count[8])++;
      
        fs_stats_GetDiff(elapsedTime, xferStartTime, xferStopTime);
        fs_stats_AddTo((xferP->sumTime), elapsedTime);
        fs_stats_SquareAddTo((xferP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (xferP->minTime))) {
	    fs_stats_TimeAssign((xferP->minTime), elapsedTime);
	}
        if (fs_stats_TimeGreaterThan(elapsedTime, (xferP->maxTime))) {
	    fs_stats_TimeAssign((xferP->maxTime), elapsedTime);
        }
    }
    FS_UNLOCK

    /*
     * Finally, go off to tell our caller the bad news in case the
     * store failed.
     */
    if (errorCode && (!targetptr->changed_newTime))
	    goto Bad_StoreData;
#endif /* FS_STATS_DETAILED */

    /* Update the status of the target's vnode */
    Update_TargetVnodeStatus(targetptr, TVS_SDATA, client, InStatus, targetptr,
			     volptr, 0);

    /* Get the updated File's status back to the caller */
    GetStatus(targetptr, OutStatus, rights, anyrights, &tparentwhentargetnotdir);

Bad_StoreData: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_StoreData	returns	%d\n", errorCode));

    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
	FS_LOCK
        (opP->numSuccesses)++;
	fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
        fs_stats_AddTo((opP->sumTime), elapsedTime);
        fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
        if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	    fs_stats_TimeAssign((opP->minTime), elapsedTime);
        }
        if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	    fs_stats_TimeAssign((opP->maxTime), elapsedTime);
        }
	FS_UNLOCK
      }
#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, StoreDataEvent, errorCode, AUD_FID, Fid, AUD_END);
    return(errorCode);

} /*SRXAFS_StoreData*/


SRXAFS_StoreACL (tcon, Fid, AccessList, OutStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSFid *Fid;			/* Target dir's fid */
    struct AFSOpaque *AccessList;	/* Access List's contents */
    struct AFSFetchStatus *OutStatus;	/* Returned status of fid */

{
    Vnode * targetptr =	0;		/* pointer to input fid */
    Vnode * parentwhentargetnotdir = 0;	/* parent of Fid to get ACL */
    int	    errorCode =	0;		/* return code for caller */
    struct AFSStoreStatus InStatus;	/* Input status for fid */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct rx_call *tcall = (struct rx_call *) tcon; 
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		    /* host ip holder for inet_ntoa */
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_STOREACL]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_StoreACL;

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_StoreACL, Fid = %u.%d.%d, ACL=%s, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique, AccessList->AFSOpaque_val,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.StoreACL++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    InStatus.Mask = 0;	    /* not storing any status */

    /*
     * Get associated volume/vnode for the target dir; caller's rights
     * are also returned.
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_StoreACL;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Check if we have permission to change the dir's ACL */
    if (errorCode = Check_PermissionRights(targetptr, client, rights,
					   CHK_STOREACL, &InStatus)) {
	goto Bad_StoreACL;
    }

    /* Build and store the new Access List for the dir */
    if (errorCode = RXStore_AccessList(targetptr, AccessList)) {
	goto Bad_StoreACL;
    }
    
    targetptr->changed_newTime = 1; /* status change of directory */

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, targetptr);
    assert(!errorCode || errorCode == VSALVAGE);

    /* break call backs on the directory  */
    BreakCallBack(client->host, Fid, 0);

    /* Get the updated dir's status back to the caller */
    GetStatus(targetptr, OutStatus, rights, anyrights, (struct vnode *)0);

Bad_StoreACL: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_StoreACL returns %d\n", errorCode)); 
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, StoreACLEvent, errorCode, AUD_FID, Fid, AUD_END);
    return errorCode;

} /*SRXAFS_StoreACL*/


/*
 * Note: This routine is called exclusively from SRXAFS_StoreStatus(), and
 * should be merged when possible.
 */
SAFSS_StoreStatus (tcon, Fid, InStatus, OutStatus, Sync)
    struct rx_connection *tcon;		/* Rx connection Handle */
    struct AFSFid *Fid;			/* Target file's fid */
    struct AFSStoreStatus *InStatus;	/* Input status for Fid */
    struct AFSFetchStatus *OutStatus;	/* Output status for fid */
    struct AFSVolSync *Sync;

{
    Vnode * targetptr =	0;		/* pointer to input fid */
    Vnode * parentwhentargetnotdir = 0;	/* parent of Fid to get ACL */
    int	    errorCode =	0;		/* return code for caller */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_StoreStatus,  Fid	= %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode,	Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.StoreStatus++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /*
     * Get volume/vnode for the target file; caller's rights to it are
     * also returned
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_StoreStatus;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Check if the caller has proper permissions to store status to Fid */
    if (errorCode = Check_PermissionRights(targetptr, client, rights,
					   CHK_STORESTATUS, InStatus)) {
	goto Bad_StoreStatus;
    }
    /*
     * Check for a symbolic link; we can't chmod these (otherwise could
     * change a symlink to a mt pt or vice versa)
     */
    if (targetptr->disk.type == vSymlink && (InStatus->Mask & AFS_SETMODE)) {
	errorCode = EINVAL;
	goto Bad_StoreStatus;
    }

    /* Update the status of the target's vnode */
    Update_TargetVnodeStatus(targetptr, TVS_SSTATUS, client, InStatus,
			     (parentwhentargetnotdir ?
			      parentwhentargetnotdir : targetptr), volptr, 0);

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, targetptr);
    assert(!errorCode || errorCode == VSALVAGE);

    /* Break call backs on Fid */
    BreakCallBack(client->host, Fid, 0);

    /* Return the updated status back to caller */
    GetStatus(targetptr, OutStatus, rights, anyrights, parentwhentargetnotdir);

Bad_StoreStatus: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_StoreStatus returns %d\n", errorCode));
    return errorCode;

} /*SAFSS_StoreStatus*/


SRXAFS_StoreStatus (tcon, Fid, InStatus, OutStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		/* Rx connection Handle */
    struct AFSFid *Fid;			/* Target file's fid */
    struct AFSStoreStatus *InStatus;	/* Input status for Fid */
    struct AFSFetchStatus *OutStatus;	/* Output status for fid */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_STORESTATUS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_StoreStatus;

    code = SAFSS_StoreStatus (tcon, Fid, InStatus, OutStatus, Sync);

Bad_StoreStatus:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, StoreStatusEvent, code, AUD_FID, Fid, AUD_END);
    return code;

} /*SRXAFS_StoreStatus*/


/*
 * This routine is called exclusively by SRXAFS_RemoveFile(), and should be
 * merged in when possible.
 */
SAFSS_RemoveFile (tcon, DirFid, Name, OutDirStatus, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Dir fid for file to remove */
    char *Name;				 /* File name to remove */
    struct AFSFetchStatus *OutDirStatus; /* Output status for dir fid's */
    struct AFSVolSync *Sync;

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    Vnode * targetptr =	0;		/* file to be deleted */
    Volume * volptr = 0;		/* pointer to the volume header */
    AFSFid fileFid;			/* area for Fid from the directory */
    int	    errorCode =	0;		/* error code */
    DirHandle dir;			/* Handle for dir package I/O */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_RemoveFile %s,  Did = %u.%d.%d, Host %s, Id %d\n",
	    Name, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.RemoveFile++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /*
     * Get volume/vnode for the parent dir; caller's access rights are
     * also returned
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_RemoveFile;
    }
    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Does the caller has delete (& write) access to the parent directory? */
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_DELETE)) {
	goto Bad_RemoveFile;
    }

    /* Actually delete the desired file */
    if (errorCode = DeleteTarget(parentptr, volptr, &targetptr, &dir,
				 &fileFid, Name, MustNOTBeDIR)) {
	goto Bad_RemoveFile;
    }

    /* Update the vnode status of the parent dir */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount);
#endif /* FS_STATS_DETAILED */

    /* Return the updated parent dir's status back to caller */
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct vnode *)0);

    /* Handle internal callback state for the parent and the deleted file */
    if (targetptr->disk.linkCount == 0) {
	/* no references left, discard entry */
	DeleteFileCallBacks(&fileFid);
	/* convert the parent lock to a read lock before breaking callbacks */
	VVnodeWriteToRead(&errorCode, parentptr);
	assert(!errorCode || errorCode == VSALVAGE);
    } else {
	/* convert the parent lock to a read lock before breaking callbacks */
	VVnodeWriteToRead(&errorCode, parentptr);
	assert(!errorCode || errorCode == VSALVAGE);
	/* convert the target lock to a read lock before breaking callbacks */
	VVnodeWriteToRead(&errorCode, targetptr);
	assert(!errorCode || errorCode == VSALVAGE);
	/* tell all the file has changed */
	BreakCallBack(client->host, &fileFid, 1);
    }

    /* break call back on the directory */
    BreakCallBack(client->host, DirFid, 0);

Bad_RemoveFile: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_RemoveFile returns %d\n",	errorCode)); 
    return errorCode;

} /*SAFSS_RemoveFile*/


SRXAFS_RemoveFile (tcon, DirFid, Name, OutDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Dir fid for file to remove */
    char *Name;				 /* File name to remove */
    struct AFSFetchStatus *OutDirStatus; /* Output status for dir fid's */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_REMOVEFILE]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_RemoveFile;

    code = SAFSS_RemoveFile (tcon, DirFid, Name, OutDirStatus, Sync);

Bad_RemoveFile:    
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, RemoveFileEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_RemoveFile*/


/*
 * This routine is called exclusively from SRXAFS_CreateFile(), and should
 * be merged in when possible.
 */
SAFSS_CreateFile (tcon, DirFid, Name, InStatus, OutFid, OutFidStatus,
		 OutDirStatus, CallBack, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent Dir fid */
    char *Name;				 /* File name to be created */
    struct AFSStoreStatus *InStatus;	 /* Input status for newly created file */
    struct AFSFid *OutFid;		 /* Fid for newly created file */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new file */
    struct AFSFetchStatus *OutDirStatus; /* Ouput status for the parent dir */
    struct AFSCallBack *CallBack;	 /* Return callback promise for new file */
    struct AFSVolSync *Sync;

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * targetptr =	0;		/* vnode of the new file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    Volume * volptr = 0;		/* pointer to the volume header */
    int	    errorCode =	0;		/* error code */
    DirHandle dir;			/* Handle for dir package I/O */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_CreateFile %s,  Did = %u.%d.%d, Host %s, Id %d\n",
	    Name, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.CreateFile++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (!FileNameOK(Name)) {
      errorCode = EINVAL;
      goto Bad_CreateFile;
    }

    /*
     * Get associated volume/vnode for the parent dir; caller long are
     * also returned
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_CreateFile;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Can we write (and insert) onto the parent directory? */
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_INSERT)) {
	goto Bad_CreateFile;
    }
    /* get a new vnode for the file to be created and set it up */
    if (errorCode = Alloc_NewVnode(parentptr, &dir, volptr, &targetptr,
				   Name, OutFid, vFile, nBlocks(0))) {
	goto Bad_CreateFile;
    }

    /* update the status of the parent vnode */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount);
#endif /* FS_STATS_DETAILED */

    /* update the status of the new file's vnode */
    Update_TargetVnodeStatus(targetptr, TVS_CFILE, client, InStatus,
			     parentptr, volptr, 0);

    /* set up the return status for the parent dir and the newly created file */
    GetStatus(targetptr, OutFidStatus, rights, anyrights, parentptr);
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct vnode *)0);

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, parentptr);
    assert(!errorCode || errorCode == VSALVAGE);
    
    /* break call back on parent dir */
    BreakCallBack(client->host, DirFid, 0);

    /* Return a callback promise for the newly created file to the caller */
    SetCallBackStruct(AddCallBack(client->host, OutFid), CallBack);

Bad_CreateFile:
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_CreateFile returns %d\n",	errorCode)); 
    return errorCode;

} /*SAFSS_CreateFile*/


SRXAFS_CreateFile (tcon, DirFid, Name, InStatus, OutFid, OutFidStatus, OutDirStatus, CallBack, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent Dir fid */
    char *Name;				 /* File name to be created */
    struct AFSStoreStatus *InStatus;	 /* Input status for newly created file */
    struct AFSFid *OutFid;		 /* Fid for newly created file */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new file */
    struct AFSFetchStatus *OutDirStatus; /* Ouput status for the parent dir */
    struct AFSCallBack *CallBack;	 /* Return callback promise for new file */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_CREATEFILE]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_CreateFile;

    code = SAFSS_CreateFile (tcon, DirFid, Name, InStatus, OutFid,
			    OutFidStatus, OutDirStatus, CallBack, Sync);

Bad_CreateFile:    
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, CreateFileEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_CreateFile*/


/*
 * This routine is called exclusively from SRXAFS_Rename(), and should be
 * merged in when possible.
 */
SAFSS_Rename (tcon, OldDirFid, OldName, NewDirFid, NewName, OutOldDirStatus,
	     OutNewDirStatus, Sync)
    struct rx_connection *tcon;		    /* Rx connection handle */
    struct AFSFid *OldDirFid;		    /* From parent dir's fid */
    char *OldName;			    /* From file name */
    struct AFSFid *NewDirFid;		    /* To parent dir's fid */
    char *NewName;			    /* To new file name */
    struct AFSFetchStatus *OutOldDirStatus; /* Output status for From parent dir */
    struct AFSFetchStatus *OutNewDirStatus; /* Output status for To parent dir */
    struct AFSVolSync *Sync;

{
    Vnode * oldvptr = 0;	/* vnode of the old Directory */
    Vnode * newvptr = 0;	/* vnode of the new Directory */
    Vnode * fileptr = 0;	/* vnode of the file to move */
    Vnode * newfileptr = 0;	/* vnode of the file to delete */
    Vnode * testvptr = 0;	/* used in directory tree walk */
    Vnode * parent = 0;		/* parent for use in SetAccessList */
    int     errorCode = 0;	/* error code */
    int     fileCode = 0;	/* used when writing Vnodes */
    VnodeId testnode;		/* used in directory tree walk */
    AFSFid fileFid;		/* Fid of file to move */
    AFSFid newFileFid;		/* Fid of new file */
    DirHandle olddir;		/* Handle for dir package I/O */
    DirHandle newdir;		/* Handle for dir package I/O */
    DirHandle filedir;		/* Handle for dir package I/O */
    DirHandle newfiledir;	/* Handle for dir package I/O */
    Volume * volptr = 0;	/* pointer to the volume header */
    struct client * client;	/* pointer to client structure */
    afs_int32 rights, anyrights;	/* rights for this and any user */
    afs_int32 newrights;		/* rights for this user */
    afs_int32 newanyrights;		/* rights for any user */
    int	doDelete;		/* deleted the rename target (ref count now 0) */
    int code;
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_Rename %s	to %s,	Fid = %u.%d.%d to %u.%d.%d, Host %s, Id %d\n",
	    OldName, NewName, OldDirFid->Volume, OldDirFid->Vnode,
	    OldDirFid->Unique, NewDirFid->Volume, NewDirFid->Vnode,
	    NewDirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.Rename++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (!FileNameOK(NewName)) {
	errorCode = EINVAL;
	goto Bad_Rename;
    }
    if (OldDirFid->Volume != NewDirFid->Volume) {
	DFlush();
	errorCode = EXDEV;
	goto Bad_Rename;
    }
    if ( (strcmp(OldName, ".") == 0) || (strcmp(OldName, "..") == 0) ||
	 (strcmp(NewName, ".") == 0) || (strcmp(NewName, "..") == 0) || 
	 (strlen(NewName) == 0) || (strlen(OldName) == 0)  ) {
	DFlush();
	errorCode = EINVAL;
	goto Bad_Rename;
    }

    if (OldDirFid->Vnode <= NewDirFid->Vnode) {
	if  (errorCode = GetVolumePackage(tcon, OldDirFid, &volptr,
					  &oldvptr, MustBeDIR, &parent,
					  &client, WRITE_LOCK, &rights,
					  &anyrights)) {
	    DFlush();
	    goto Bad_Rename;
	}
	if (OldDirFid->Vnode == NewDirFid->Vnode) {
	    newvptr = oldvptr;
	    newrights = rights, newanyrights = anyrights;
	}
	else
	    if (errorCode = GetVolumePackage(tcon, NewDirFid, &volptr,
					     &newvptr, MustBeDIR, &parent,
					     &client, WRITE_LOCK, &newrights,
					     &newanyrights)) {
		DFlush();
		goto Bad_Rename;
	    }
    }
    else {
	if (errorCode = GetVolumePackage(tcon, NewDirFid, &volptr,
					 &newvptr, MustBeDIR, &parent,
					 &client, WRITE_LOCK, &newrights,
					 &newanyrights)) {
	    DFlush();
	    goto Bad_Rename;
	}
	if (errorCode = GetVolumePackage(tcon, OldDirFid, &volptr, &oldvptr,
					 MustBeDIR, &parent, &client, WRITE_LOCK,
					 &rights, &anyrights)) {
	    DFlush();
	    goto Bad_Rename;
	}
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    if (errorCode = CheckWriteMode(oldvptr, rights, PRSFS_DELETE)) {
	goto Bad_Rename;
    }
    if (errorCode = CheckWriteMode(newvptr, newrights, PRSFS_INSERT)) {
	goto Bad_Rename;
    }

    /* The CopyOnWrite might return ENOSPC ( disk full). Even if the second
    *  call to CopyOnWrite returns error, it is not necessary to revert back
    *  the effects of the first call because the contents of the volume is 
    *  not modified, it is only replicated.
    */
    if (oldvptr->disk.cloned)
    {
	ViceLog(25, ("Rename : calling CopyOnWrite on  old dir\n"));
	 if ( errorCode = CopyOnWrite(oldvptr, volptr) )
		goto Bad_Rename;
    }
    SetDirHandle(&olddir, oldvptr);
    if (newvptr->disk.cloned)
    {
	ViceLog(25, ("Rename : calling CopyOnWrite on  new dir\n"));
	if ( errorCode = CopyOnWrite(newvptr, volptr) )
		goto Bad_Rename;	
    }

    SetDirHandle(&newdir, newvptr);

    /* Lookup the file to delete its vnode */
    if (Lookup(&olddir, OldName, &fileFid)) {
	errorCode = ENOENT;
	goto Bad_Rename;
    }
    if (fileFid.Vnode == oldvptr->vnodeNumber ||
	fileFid.Vnode == newvptr->vnodeNumber) {
	errorCode = FSERR_ELOOP;
	goto Bad_Rename;
    }
    fileFid.Volume = V_id(volptr);
    fileptr = VGetVnode(&errorCode, volptr, fileFid.Vnode, WRITE_LOCK);
    if (errorCode != 0) {
        ViceLog (0, ("SAFSS_Rename(): Error in VGetVnode() for old file %s, code %d\n", OldName, errorCode));
        VTakeOffline (volptr);
        goto Bad_Rename;
    }
    if (fileptr->disk.uniquifier != fileFid.Unique) {
        ViceLog (0, ("SAFSS_Rename(): Old file %s uniquifier mismatch\n", OldName));
        VTakeOffline (volptr);
        errorCode = EIO;
        goto Bad_Rename;
    }

    if (fileptr->disk.type != vDirectory &&
	oldvptr != newvptr &&
	fileptr->disk.linkCount != 1) {
	/*
	 * Hard links exist to this file - cannot move one of the links to
	 * a new directory because of AFS restrictions (this is the same
	 * reason that links cannot be made across directories, i.e.
	 * access lists)
	 */
	errorCode = EXDEV;
	goto Bad_Rename;
    }

    /* Lookup the new file  */
    if (!(Lookup(&newdir, NewName, &newFileFid))) {
	if (!(newrights & PRSFS_DELETE)) {
	    errorCode = EACCES;
	    goto Bad_Rename;
	}
	if (newFileFid.Vnode == oldvptr->vnodeNumber ||
		newFileFid.Vnode == newvptr->vnodeNumber ||
		newFileFid.Vnode == fileFid.Vnode) {
	    errorCode = EINVAL;
	    goto Bad_Rename;
	}
	newFileFid.Volume = V_id(volptr);
	newfileptr = VGetVnode(&errorCode, volptr, newFileFid.Vnode, WRITE_LOCK);
        if (errorCode != 0) {
            ViceLog (0, ("SAFSS_Rename(): Error in VGetVnode() for new file %s, code %d\n", NewName, errorCode));
            VTakeOffline (volptr);
            goto Bad_Rename;
        }
        if (fileptr->disk.uniquifier != fileFid.Unique) {
            ViceLog (0, ("SAFSS_Rename(): New file %s uniquifier mismatch\n", NewName));
            VTakeOffline (volptr);
            errorCode = EIO;
            goto Bad_Rename;
        }
	SetDirHandle(&newfiledir, newfileptr);
	/* Now check that we're moving directories over directories properly, etc.
	 * return proper POSIX error codes:
	 * if fileptr is a file and new is a dir: EISDIR.
	 * if fileptr is a dir and new is a file: ENOTDIR.
	 * Also, dir to be removed must be empty, of course.
	 */
	if (newfileptr->disk.type == vDirectory) {
	    if (fileptr->disk.type != vDirectory) {
		errorCode = EISDIR;
		goto Bad_Rename;
	    }
	    if ((IsEmpty(&newfiledir))) {
		errorCode = EEXIST;
		goto Bad_Rename;
	    }
	}
	else {
	    if (fileptr->disk.type == vDirectory) {
		errorCode = ENOTDIR;
		goto Bad_Rename;
	    }
	}
    }

    /*
     * ok - now we check that the old name is not above new name in the
     * directory structure.  This is to prevent removing a subtree alltogether
     */
    if ((oldvptr != newvptr) && (fileptr->disk.type == vDirectory)) {
	for (testnode = newvptr->disk.parent; testnode != 0;) {
	    if (testnode == oldvptr->vnodeNumber) {
		testnode = oldvptr->disk.parent;
		continue;
	    }
	    if ((testnode == fileptr->vnodeNumber) ||
		(testnode == newvptr->vnodeNumber)) {
		errorCode = FSERR_ELOOP;
		goto Bad_Rename;
	    }
	    if ((newfileptr) && (testnode == newfileptr->vnodeNumber)) {
		errorCode = FSERR_ELOOP;
		goto Bad_Rename;
	    }
	    testvptr = VGetVnode(&errorCode, volptr, testnode, READ_LOCK);
	    assert(errorCode == 0);
	    testnode = testvptr->disk.parent;
	    VPutVnode(&errorCode, testvptr);
	    assert(errorCode == 0);
	}
    }
    /* Do the CopyonWrite first before modifying anything else. Copying is
     *  required because we may have to change entries for .. 
     */
    if ((fileptr->disk.type == vDirectory ) && (fileptr->disk.cloned) )
    {
	ViceLog(25, ("Rename : calling CopyOnWrite on  target dir\n"));
	if ( errorCode = CopyOnWrite(fileptr, volptr) )
		goto Bad_Rename;
    }

    /* If the new name exists already, delete it and the file it points to */
    doDelete = 0;
    if (newfileptr) {
        /* Delete NewName from its directory */
        code = Delete(&newdir, NewName);
	assert(code == 0);

	/* Drop the link count */
	newfileptr->disk.linkCount--;
	if (newfileptr->disk.linkCount == 0) {      /* Link count 0 - delete */
	    VAdjustDiskUsage(&errorCode, volptr,
			     -(int)nBlocks(newfileptr->disk.length), 0);
	    if (VN_GET_INO(newfileptr)) {
		IH_REALLYCLOSE(newfileptr->handle);
		errorCode = IH_DEC(V_linkHandle(volptr),
				 VN_GET_INO(newfileptr),
				 V_parentId(volptr));
		IH_RELEASE(newfileptr->handle);
		if (errorCode == -1) {
		    ViceLog(0, ("Del: inode=%d, name=%s, errno=%d\n",
				PrintInode(NULL, VN_GET_INO(newfileptr)),
				NewName, errno));
		    if ((errno != ENOENT) && (errno != EIO) && (errno != ENXIO))
			ViceLog(0, ("Do we need to fsck?"));
		} 
	    }
	    VN_SET_INO(newfileptr, (Inode)0);
	    newfileptr->delete = 1;         /* Mark NewName vnode to delete */
	    doDelete = 1;
	} else {
	    /* Link count did not drop to zero.
	     * Mark NewName vnode as changed - updates stime.
	     */
	    newfileptr->changed_newTime = 1;
	}
    }
    
    /*
     * If the create below fails, and the delete above worked, we have
     * removed the new name and not replaced it.  This is not very likely,
     * but possible.  We could try to put the old file back, but it is
     * highly unlikely that it would work since it would involve issuing
     * another create.
     */
    if (errorCode = Create(&newdir,(char *) NewName, &fileFid))
	goto Bad_Rename;

    /* Delete the old name */
    assert(Delete(&olddir,(char *) OldName) == 0);

    /* if the directory length changes, reflect it in the statistics */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(oldvptr, volptr, &olddir, client->ViceId,
			     oldvptr->disk.linkCount, client->InSameNetwork);
    Update_ParentVnodeStatus(newvptr, volptr, &newdir, client->ViceId,
			     newvptr->disk.linkCount, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(oldvptr, volptr, &olddir, client->ViceId,
			     oldvptr->disk.linkCount);
    Update_ParentVnodeStatus(newvptr, volptr, &newdir, client->ViceId,
			     newvptr->disk.linkCount);
#endif /* FS_STATS_DETAILED */

    if (oldvptr	== newvptr)
	oldvptr->disk.dataVersion--;    /* Since it was bumped by 2! */

    fileptr->disk.parent = newvptr->vnodeNumber;
    fileptr->changed_newTime = 1;       /* status change of moved file */

    /* if we are dealing with a rename of a directory */
    if (fileptr->disk.type == vDirectory) {
	assert(!fileptr->disk.cloned);		
	SetDirHandle(&filedir, fileptr);
	/* fix .. to point to the correct place */
	Delete(&filedir, ".."); /* No assert--some directories may be bad */
	assert(Create(&filedir, "..", NewDirFid) == 0);
	fileptr->disk.dataVersion++;
	/* if the parent directories are different the link counts have to be	*/
	/* changed due to .. in the renamed directory */
	if (oldvptr != newvptr) {
	    oldvptr->disk.linkCount--;
	    newvptr->disk.linkCount++;
	}
    }

    /* set up return status */
    GetStatus(oldvptr, OutOldDirStatus, rights, anyrights, (struct vnode *)0);
    GetStatus(newvptr, OutNewDirStatus, newrights, newanyrights, (struct vnode *)0);
    if (newfileptr && doDelete) {
	DeleteFileCallBacks(&newFileFid);	/* no other references */
    }

    DFlush();

    /* convert the write locks to a read locks before breaking callbacks */
    VVnodeWriteToRead(&errorCode, newvptr);
    assert(!errorCode || errorCode == VSALVAGE);
    if (oldvptr != newvptr) {
	VVnodeWriteToRead(&errorCode, oldvptr);
	assert(!errorCode || errorCode == VSALVAGE);
    }
    if (newfileptr && !doDelete) {
	/* convert the write lock to a read lock before breaking callbacks */
	VVnodeWriteToRead(&errorCode, newfileptr);
	assert(!errorCode || errorCode == VSALVAGE);
    }

    /* break call back on NewDirFid, OldDirFid, NewDirFid and newFileFid  */
    BreakCallBack(client->host, NewDirFid, 0);
    if (oldvptr != newvptr) {
	BreakCallBack(client->host, OldDirFid, 0);
	if (fileptr->disk.type == vDirectory) /* if a dir moved, .. changed */
	    BreakCallBack(client->host, &fileFid, 0);
    }
    if (newfileptr) {
	/* Note:  it is not necessary to break the callback */
	if (doDelete)
	    DeleteFileCallBacks(&newFileFid);	/* no other references */
	else
	    /* other's still exist (with wrong link count) */
	    BreakCallBack(client->host, &newFileFid, 1);
    }

Bad_Rename: 
    if (newfileptr) {
	VPutVnode(&fileCode, newfileptr);
	assert(fileCode == 0);
    }
    PutVolumePackage(fileptr, (newvptr && newvptr != oldvptr? newvptr : 0),
		     oldvptr, volptr);
    ViceLog(2, ("SAFS_Rename returns %d\n", errorCode));
    return errorCode;

} /*SAFSS_Rename*/


SRXAFS_Rename (tcon, OldDirFid, OldName, NewDirFid, NewName, OutOldDirStatus, OutNewDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		     /* Rx connection handle */
    struct AFSFid *OldDirFid;		     /* From parent dir's fid */
    char *OldName;			     /* From file name */
    struct AFSFid *NewDirFid;		     /* To parent dir's fid */
    char *NewName;	   		     /* To new file name */
    struct AFSFetchStatus *OutOldDirStatus;  /* Output status for From parent dir */
    struct AFSFetchStatus *OutNewDirStatus;  /* Output status for To parent dir */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_RENAME]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_Rename;

    code = SAFSS_Rename (tcon, OldDirFid, OldName, NewDirFid, NewName,
			 OutOldDirStatus, OutNewDirStatus, Sync);

Bad_Rename:    
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, RenameFileEvent, code, AUD_FID, OldDirFid, AUD_STR, OldName, AUD_FID, NewDirFid, AUD_STR, NewName, AUD_END);
    return code;

} /*SRXAFS_Rename*/


/*
 * This routine is called exclusively by SRXAFS_Symlink(), and should be
 * merged into it when possible.
 */
SAFSS_Symlink (tcon, DirFid, Name, LinkContents, InStatus, OutFid, OutFidStatus, OutDirStatus, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    char *LinkContents;			 /* Contents of the new created file */
    struct AFSStoreStatus *InStatus;	 /* Input status for the new symbolic link */
    struct AFSFid *OutFid;		 /* Fid for newly created symbolic link */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new symbolic link */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */
    struct AFSVolSync *Sync;		 /* volume synchronization information */

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * targetptr =	0;		/* vnode of the new link */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    int	    errorCode =	0;		/* error code */
    int code = 0;
    DirHandle dir;			/* Handle for dir package I/O */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights, fd;	/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */
    FdHandle_t *fdP;

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_Symlink %s to %s,  Did = %u.%d.%d, Host %s, Id %d\n", Name,
	    LinkContents, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.Symlink++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (!FileNameOK(Name)) {
	errorCode = EINVAL;
	goto Bad_SymLink;
    }

    /*
     * Get the vnode and volume for the parent dir along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_SymLink;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Does the caller has insert (and write) access to the parent directory? */
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_INSERT)) {
	goto Bad_SymLink;
    }

    /*
     * If we're creating a mount point (owner mode bits sans x bit), we must
     * have administer access to the directory, too.  Always allow sysadmins
     * to do this.
     */
    if ((InStatus->Mask & AFS_SETMODE) && !(InStatus->UnixModeBits & 0100)) {
	/*
	 * We have a symlink, 'cause we're trying to set the Unix mode bits
	 * to something without the owner x bits (default mode bits if
	 * AFS_SETMODE is false is 0777)
	 */
	if (VanillaUser(client) && !(rights & PRSFS_ADMINISTER)) {
	    errorCode = EACCES;
	    goto Bad_SymLink;
	}
    }
 
    /* get a new vnode for the symlink and set it up */
    if (errorCode = Alloc_NewVnode(parentptr, &dir, volptr, &targetptr,
				   Name, OutFid, vSymlink,
				   nBlocks(strlen((char *) LinkContents)))) {
	goto Bad_SymLink;
    }

    /* update the status of the parent vnode */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount);
#endif /* FS_STATS_DETAILED */

    /* update the status of the new symbolic link file vnode */
    Update_TargetVnodeStatus(targetptr, TVS_SLINK, client, InStatus, parentptr,
			     volptr, strlen((char *)LinkContents));

    /* Write the contents of the symbolic link name into the target inode */
    fdP = IH_OPEN(targetptr->handle);
    assert(fdP != NULL);
    assert(FDH_WRITE(fdP, (char *) LinkContents, strlen((char *) LinkContents)) == strlen((char *) LinkContents));
    FDH_CLOSE(fdP);
    /*
     * Set up and return modified status for the parent dir and new symlink
     * to caller.
     */
    GetStatus(targetptr, OutFidStatus, rights, anyrights, parentptr);
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct vnode *)0);

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, parentptr);
    assert(!errorCode || errorCode == VSALVAGE);

    /* break call back on the parent dir */
    BreakCallBack(client->host, DirFid, 0);

Bad_SymLink: 
    /* Write the all modified vnodes (parent, new files) and volume back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_Symlink returns %d\n", errorCode));
    return errorCode;

} /*SAFSS_Symlink*/


SRXAFS_Symlink (tcon, DirFid, Name, LinkContents, InStatus, OutFid, OutFidStatus, OutDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    char *LinkContents;			 /* Contents of the new created file */
    struct AFSStoreStatus *InStatus;	 /* Input status for the new symbolic link */
    struct AFSFid *OutFid;		 /* Fid for newly created symbolic link */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new symbolic link */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_SYMLINK]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_Symlink;

    code = SAFSS_Symlink (tcon, DirFid, Name, LinkContents, InStatus, OutFid,
			 OutFidStatus, OutDirStatus, Sync);

Bad_Symlink:    
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, SymlinkEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_Symlink*/


/*
 * This routine is called exclusively by SRXAFS_Link(), and should be
 * merged into it when possible.
 */
SAFSS_Link (tcon, DirFid, Name, ExistingFid, OutFidStatus, OutDirStatus, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    struct AFSFid *ExistingFid;		 /* Fid of existing fid we'll make link to */
    struct AFSFetchStatus *OutFidStatus; /* Output status for newly created file */
    struct AFSFetchStatus *OutDirStatus; /* Outpout status for parent dir */
    struct AFSVolSync *Sync;

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * targetptr =	0;		/* vnode of the new file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    Volume * volptr = 0;		/* pointer to the volume header */
    int	    errorCode =	0;		/* error code */
    DirHandle dir;			/* Handle for dir package I/O */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_Link %s,	Did = %u.%d.%d,	Fid = %u.%d.%d, Host %s, Id %d\n",
	    Name, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    ExistingFid->Volume, ExistingFid->Vnode, ExistingFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.Link++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (DirFid->Volume != ExistingFid->Volume) {
	errorCode = EXDEV;
	goto Bad_Link;
    }
    if (!FileNameOK(Name)) {
	errorCode = EINVAL;
	goto Bad_Link;
    }

    /*
     * Get the vnode and volume for the parent dir along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_Link;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Can the caller insert into the parent directory? */
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_INSERT)) {
	goto Bad_Link;
    }

    if (((DirFid->Vnode & 1) && (ExistingFid->Vnode & 1)) ||
        (DirFid->Vnode == ExistingFid->Vnode)) {  /* at present, */
      /* AFS fileservers always have directory vnodes that are odd.   */
      errorCode = EISDIR;
      goto Bad_Link;
    }

    /* get the file vnode  */
    if (errorCode = CheckVnode(ExistingFid, &volptr, &targetptr, WRITE_LOCK)) {
        goto Bad_Link;
    }
    if (targetptr->disk.type != vFile) {
	errorCode = EISDIR;
	goto Bad_Link;
    }
    if (targetptr->disk.parent != DirFid->Vnode) {
	errorCode = EXDEV;
	goto Bad_Link;
    }
    if (parentptr->disk.cloned)	
    {
	ViceLog(25, ("Link : calling CopyOnWrite on  target dir\n"));
	if ( errorCode = CopyOnWrite(parentptr, volptr))
		goto Bad_Link;		/* disk full error */
    }

    /* add the name to the directory */
    SetDirHandle(&dir, parentptr);
    if (errorCode = Create(&dir, (char *)Name, ExistingFid))
	goto Bad_Link;
    DFlush();

    /* update the status in the parent vnode */
    /**WARNING** --> disk.author SHOULDN'T be modified???? */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount);
#endif /* FS_STATS_DETAILED */

    targetptr->disk.linkCount++;
    targetptr->disk.author = client->ViceId;
    targetptr->changed_newTime = 1; /* Status change of linked-to file */

    /* set up return status */
    GetStatus(targetptr, OutFidStatus, rights, anyrights, parentptr);
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct vnode *)0);

    /* convert the write locks to read locks before breaking callbacks */
    VVnodeWriteToRead(&errorCode, targetptr);
    assert(!errorCode || errorCode == VSALVAGE);
    VVnodeWriteToRead(&errorCode, parentptr);
    assert(!errorCode || errorCode == VSALVAGE);
    
    /* break call back on DirFid */
    BreakCallBack(client->host, DirFid, 0);
    /*
     * We also need to break the callback for the file that is hard-linked since part 
     * of its status (like linkcount) is changed
     */
    BreakCallBack(client->host, ExistingFid, 0);

Bad_Link:
    /* Write the all modified vnodes (parent, new files) and volume back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_Link returns %d\n", errorCode));
    return errorCode;

} /*SAFSS_Link*/


SRXAFS_Link (tcon, DirFid, Name, ExistingFid, OutFidStatus, OutDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    struct AFSFid *ExistingFid;		 /* Fid of existing fid we'll make link to */
    struct AFSFetchStatus *OutFidStatus; /* Output status for newly created file */
    struct AFSFetchStatus *OutDirStatus; /* Outpout status for parent dir */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_LINK]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_Link;

    code = SAFSS_Link (tcon, DirFid, Name, ExistingFid, OutFidStatus,
		      OutDirStatus, Sync);
    
Bad_Link:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, LinkEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_FID, ExistingFid, AUD_END);
    return code;

} /*SRXAFS_Link*/


/*
 * This routine is called exclusively by SRXAFS_MakeDir(), and should be
 * merged into it when possible.
 */
SAFSS_MakeDir (tcon, DirFid, Name, InStatus, OutFid, OutFidStatus,
	      OutDirStatus, CallBack, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* Name of dir to be created */
    struct AFSStoreStatus *InStatus; 	 /* Input status for new dir */
    struct AFSFid *OutFid;		 /* Fid of new dir */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new directory */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */
    struct AFSCallBack *CallBack;	 /* Returned callback promise for new dir */
    struct AFSVolSync *Sync;

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * targetptr =	0;		/* vnode of the new file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    Volume * volptr = 0;		/* pointer to the volume header */
    int	    errorCode =	0;		/* error code */
    struct acl_accessList * newACL;	/* Access list */
    int	    newACLSize;			/* Size of access list */
    DirHandle dir;			/* Handle for dir package I/O */
    DirHandle parentdir;		/* Handle for dir package I/O */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_MakeDir %s,  Did = %u.%d.%d, Host %s, Id %d\n",
	    Name, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.MakeDir++, AFSCallStats.TotalCalls++;    
    FS_UNLOCK
    if (!FileNameOK(Name)) {
	errorCode = EINVAL;
	goto Bad_MakeDir;
    }

    /*
     * Get the vnode and volume for the parent dir along with the caller's
     * rights to it.
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_MakeDir;
    }
 
    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Write access to the parent directory? */
#ifdef DIRCREATE_NEED_WRITE
    /*
     * requires w access for the user to create a directory. this
     * closes a loophole in the current security arrangement, since a
     * user with i access only can create a directory and get the
     * implcit a access that goes with dir ownership, and proceed to 
     * subvert quota in the volume.
     */
    if ((errorCode = CheckWriteMode(parentptr, rights, PRSFS_INSERT)) ||
	(errorCode = CheckWriteMode(parentptr, rights, PRSFS_WRITE))) {
#else 
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_INSERT)) {
#endif /* DIRCREATE_NEED_WRITE */ 
	goto Bad_MakeDir;
    }

#define EMPTYDIRBLOCKS 2
    /* get a new vnode and set it up */
    if (errorCode = Alloc_NewVnode(parentptr, &parentdir, volptr, &targetptr,
				   Name, OutFid, vDirectory, EMPTYDIRBLOCKS)) {
	goto Bad_MakeDir;
    }

    /* Update the status for the parent dir */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &parentdir, client->ViceId,
			     parentptr->disk.linkCount+1, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &parentdir, client->ViceId,
			     parentptr->disk.linkCount+1);
#endif /* FS_STATS_DETAILED */

    /* Point to target's ACL buffer and copy the parent's ACL contents to it */
    assert((SetAccessList(&targetptr, &volptr, &newACL, &newACLSize,
			  &parentwhentargetnotdir, (AFSFid *)0, 0)) == 0);
    assert(parentwhentargetnotdir == 0);
    bcopy((char *)VVnodeACL(parentptr), (char *)newACL, VAclSize(parentptr));

    /* update the status for the target vnode */
    Update_TargetVnodeStatus(targetptr, TVS_MKDIR, client, InStatus,
			     parentptr, volptr, 0);

    /* Actually create the New directory in the directory package */
    SetDirHandle(&dir, targetptr);
    assert(!(MakeDir(&dir, OutFid, DirFid)));
    DFlush();
    targetptr->disk.length = Length(&dir);

    /* set up return status */
    GetStatus(targetptr, OutFidStatus, rights, anyrights, parentptr);
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct Vnode *)0);

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, parentptr);
    assert(!errorCode || errorCode == VSALVAGE);

    /* break call back on DirFid */
    BreakCallBack(client->host, DirFid, 0);

    /* Return a callback promise to caller */
    SetCallBackStruct(AddCallBack(client->host, OutFid), CallBack);

Bad_MakeDir: 
    /* Write the all modified vnodes (parent, new files) and volume back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_MakeDir returns %d\n", errorCode)); 
    return errorCode;

} /*SAFSS_MakeDir*/


SRXAFS_MakeDir (tcon, DirFid, Name, InStatus, OutFid, OutFidStatus, OutDirStatus, CallBack, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* Name of dir to be created */
    struct AFSStoreStatus *InStatus;	 /* Input status for new dir */
    struct AFSFid *OutFid;		 /* Fid of new dir */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new directory */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */
    struct AFSCallBack *CallBack;	 /* Returned callback promise for new dir */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_MAKEDIR]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */
    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_MakeDir;

    code = SAFSS_MakeDir (tcon, DirFid, Name, InStatus, OutFid,
			 OutFidStatus, OutDirStatus, CallBack, Sync);
    
Bad_MakeDir:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, MakeDirEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_MakeDir*/


/*
 * This routine is called exclusively by SRXAFS_RemoveDir(), and should be
 * merged into it when possible.
 */
SAFSS_RemoveDir (tcon, DirFid, Name, OutDirStatus, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* (Empty) dir's name to be removed */
    struct AFSFetchStatus *OutDirStatus; /* Output status for the parent dir */
    struct AFSVolSync *Sync;

{
    Vnode * parentptr =	0;		/* vnode of input Directory */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    Vnode * targetptr =	0;		/* file to be deleted */
    AFSFid fileFid;			/* area for Fid from the directory */
    int	    errorCode =	0;		/* error code */
    DirHandle dir;			/* Handle for dir package I/O */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    Vnode debugvnode1, debugvnode2;
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1, ("SAFS_RemoveDir	%s,  Did = %u.%d.%d, Host %s, Id %d\n",
	    Name, DirFid->Volume, DirFid->Vnode, DirFid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.RemoveDir++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /*
     * Get the vnode and volume for the parent dir along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, DirFid, &volptr, &parentptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_RemoveDir;
    }
    debugvnode1 = *parentptr;

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Does the caller has delete (&write) access to the parent dir? */
    if (errorCode = CheckWriteMode(parentptr, rights, PRSFS_DELETE)) {
	goto Bad_RemoveDir;
    }

    debugvnode2 = *parentptr;
    /* Do the actual delete of the desired (empty) directory, Name */
    if (errorCode = DeleteTarget(parentptr, volptr, &targetptr, &dir, &fileFid,
				 Name, MustBeDIR)) {
	goto Bad_RemoveDir;
    }

    /* Update the status for the parent dir; link count is also adjusted */
#if FS_STATS_DETAILED
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount-1, client->InSameNetwork);
#else
    Update_ParentVnodeStatus(parentptr, volptr, &dir, client->ViceId,
			     parentptr->disk.linkCount-1);
#endif /* FS_STATS_DETAILED */

    /* Return to the caller the updated parent dir status */
    GetStatus(parentptr, OutDirStatus, rights, anyrights, (struct Vnode *)0);

    /*
     * Note: it is not necessary to break the callback on fileFid, since
     * refcount is now 0, so no one should be able to refer to the dir
     * any longer
     */
    DeleteFileCallBacks(&fileFid);

    /* convert the write lock to a read lock before breaking callbacks */
    VVnodeWriteToRead(&errorCode, parentptr);
    assert(!errorCode || errorCode == VSALVAGE);

    /* break call back on DirFid and fileFid */
    BreakCallBack(client->host, DirFid, 0);

Bad_RemoveDir: 
    /* Write the all modified vnodes (parent, new files) and volume back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr);
    ViceLog(2, ("SAFS_RemoveDir	returns	%d\n", errorCode));
    return errorCode;

} /*SAFSS_RemoveDir*/


SRXAFS_RemoveDir (tcon, DirFid, Name, OutDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* (Empty) dir's name to be removed */
    struct AFSFetchStatus *OutDirStatus; /* Output status for the parent dir */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_REMOVEDIR]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_RemoveDir;

    code = SAFSS_RemoveDir (tcon, DirFid, Name, OutDirStatus, Sync);
    
Bad_RemoveDir:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, RemoveDirEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_RemoveDir*/


/*
 * This routine is called exclusively by SRXAFS_SetLock(), and should be
 * merged into it when possible.
 */
SAFSS_SetLock (tcon, Fid, type, Sync)
    struct rx_connection *tcon; /* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to lock */
    ViceLockType type;		/* Type of lock (Read or write) */
    struct AFSVolSync *Sync;

{
    Vnode * targetptr =	0;		/* vnode of input file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    int	    errorCode =	0;		/* error code */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */
    static char	* locktype[2] = {"LockRead","LockWrite"};

    if (type != LockRead && type != LockWrite) {
        errorCode = EINVAL;
	goto Bad_SetLock;
    }
    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1,("SAFS_SetLock type = %s Fid = %u.%d.%d, Host %s, Id %d\n",
	    locktype[(int)type], Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.SetLock++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    /*
     * Get the vnode and volume for the desired file along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_SetLock;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Handle the particular type of set locking, type */
    errorCode = HandleLocking(targetptr, rights, type);

Bad_SetLock: 
    /* Write the all modified vnodes (parent, new files) and volume back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);

    if ((errorCode == VREADONLY) && (type == LockRead))
       errorCode = 0;  /* allow read locks on RO volumes without saving state */

    ViceLog(2,("SAFS_SetLock returns %d\n", errorCode));
    return(errorCode);

}  /*SAFSS_SetLock*/


SRXAFS_OldSetLock(tcon, Fid, type, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to lock */
    ViceLockType type;		/* Type of lock (Read or write) */

{
    return SRXAFS_SetLock(tcon, Fid, type, Sync);

} /*SRXAFS_OldSetLock*/


SRXAFS_SetLock (tcon, Fid, type, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to lock */
    ViceLockType type;		/* Type of lock (Read or write) */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_SETLOCK]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_SetLock;

    code = SAFSS_SetLock (tcon, Fid, type, Sync);
    
Bad_SetLock:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0); 
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, SetLockEvent, code, AUD_FID, Fid, AUD_LONG, type, AUD_END);
    return code;

} /*SRXAFS_SetLock*/


/*
 * This routine is called exclusively by SRXAFS_ExtendLock(), and should be
 * merged into it when possible.
 */
SAFSS_ExtendLock (tcon, Fid, Sync)
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file whose lock we extend */
    struct AFSVolSync *Sync;

{
    Vnode * targetptr =	0;		/* vnode of input file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    int	    errorCode =	0;		/* error code */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1,("SAFS_ExtendLock Fid = %u.%d.%d, Host %s, Id %d\n", 
	       Fid->Volume, Fid->Vnode, Fid->Unique, 
	       inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.ExtendLock++, AFSCallStats.TotalCalls++;    
    FS_UNLOCK
    /*
     * Get the vnode and volume for the desired file along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_ExtendLock;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Handle the actual lock extension */
    errorCode = HandleLocking(targetptr, rights, LockExtend);

Bad_ExtendLock: 
    /* Put back file's vnode and volume */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);

    if ((errorCode == VREADONLY))  /* presumably, we already granted this lock */
       errorCode = 0;              /* under our generous policy re RO vols */

    ViceLog(2,("SAFS_ExtendLock returns %d\n", errorCode));
    return(errorCode);

} /*SAFSS_ExtendLock*/


SRXAFS_OldExtendLock (tcon, Fid, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file whose lock we extend */

{
    return SRXAFS_ExtendLock(tcon, Fid, Sync);

} /*SRXAFS_OldExtendLock*/


SRXAFS_ExtendLock (tcon, Fid, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file whose lock we extend */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_EXTENDLOCK]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_ExtendLock;

    code = SAFSS_ExtendLock (tcon, Fid, Sync);

Bad_ExtendLock:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, ExtendLockEvent, code, AUD_FID, Fid , AUD_END);
    return code;

} /*SRXAFS_ExtendLock*/


/*
 * This routine is called exclusively by SRXAFS_ReleaseLock(), and should be
 * merged into it when possible.
 */
SAFSS_ReleaseLock (tcon, Fid, Sync)
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to release lock */
    struct AFSVolSync *Sync;

{
    Vnode * targetptr =	0;		/* vnode of input file */
    Vnode * parentwhentargetnotdir = 0;	/* parent for use in SetAccessList */
    int	    errorCode =	0;		/* error code */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		/* host ip holder for inet_ntoa */

    /* Get ptr to client data for user Id for logging */
    t_client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key); 
    logHostAddr.s_addr =  rx_HostOf(rx_PeerOf(tcon));
    ViceLog(1,("SAFS_ReleaseLock Fid = %u.%d.%d, Host %s, Id %d\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique,
	    inet_ntoa(logHostAddr), t_client->ViceId));
    FS_LOCK
    AFSCallStats.ReleaseLock++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    /*
     * Get the vnode and volume for the desired file along with the caller's
     * rights to it
     */
    if (errorCode = GetVolumePackage(tcon, Fid, &volptr, &targetptr,
				     DONTCHECK, &parentwhentargetnotdir,
				     &client, WRITE_LOCK, &rights, &anyrights)) {
	goto Bad_ReleaseLock;
    }

    /* set volume synchronization information */
    SetVolumeSync(Sync, volptr);

    /* Handle the actual lock release */
    if (errorCode = HandleLocking(targetptr, rights, LockRelease))
	goto Bad_ReleaseLock;

    /* if no more locks left, a callback would be triggered here */
    if (targetptr->disk.lock.lockCount <= 0) {
	/* convert the write lock to a read lock before breaking callbacks */
	VVnodeWriteToRead(&errorCode, targetptr);
	assert(!errorCode || errorCode == VSALVAGE);
	BreakCallBack(client->host, Fid, 0);
    }

Bad_ReleaseLock: 
    /* Put back file's vnode and volume */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);

    if ((errorCode == VREADONLY))  /* presumably, we already granted this lock */
       errorCode = 0;              /* under our generous policy re RO vols */

    ViceLog(2,("SAFS_ReleaseLock returns %d\n", errorCode));
    return(errorCode);

} /*SAFSS_ReleaseLock*/


SRXAFS_OldReleaseLock (tcon, Fid, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to release lock */

{
    return SRXAFS_ReleaseLock(tcon, Fid, Sync);

} /*SRXAFS_OldReleaseLock*/


SRXAFS_ReleaseLock (tcon, Fid, Sync)
    struct AFSVolSync *Sync;
    struct rx_connection *tcon;	/* Rx connection handle */
    struct AFSFid *Fid;		/* Fid of file to release lock */

{
    afs_int32 code;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_RELEASELOCK]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_ReleaseLock;

    code = SAFSS_ReleaseLock (tcon, Fid, Sync);
    
Bad_ReleaseLock:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, ReleaseLockEvent, code, AUD_FID, Fid , AUD_END);
    return code;

} /*SRXAFS_ReleaseLock*/


/*
 * This routine is called exclusively by SRXAFS_GetStatistics(), and should be
 * merged into it when possible.
 */
static GetStatistics (tcon, Statistics)
    struct rx_connection *tcon;	      /* Rx connection handle */
    struct AFSStatistics *Statistics; /* Placeholder for returned AFS statistics */
{
    ViceLog(1, ("SAFS_GetStatistics Received\n"));
    FS_LOCK
    AFSCallStats.GetStatistics++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    bzero(Statistics, sizeof(*Statistics));
    SetAFSStats(Statistics);
    SetVolumeStats(Statistics);
    SetSystemStats(Statistics);

    return(0);

} /*GetStatistics*/


SRXAFS_GetStatistics (tcon, Statistics)
    struct rx_connection *tcon;	      /* Rx connection handle */
    struct AFSStatistics *Statistics; /* Placeholder for returned AFS statistics */

{
    afs_int32 code;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETSTATISTICS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, NOTACTIVECALL))
	goto Bad_GetStatistics;

    code = GetStatistics (tcon, Statistics);
    
Bad_GetStatistics:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */

    return code;

} /*SRXAFS_GetStatistics*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFS_XStatsVersion
 *
 * Description:
 *	Routine called by the server-side RPC interface to implement
 *	pulling out the xstat version number for the File Server.
 *
 * Arguments:
 *	a_versionP : Ptr to the version number variable to set.
 *
 * Returns:
 *	0 (always)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFS_XStatsVersion(a_call, a_versionP)
    struct rx_call *a_call;
    afs_int32 *a_versionP;

{ /*SRXAFS_XStatsVersion*/

#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_XSTATSVERSION]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    *a_versionP = AFS_XSTAT_VERSION;

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
    fs_stats_AddTo((opP->sumTime), elapsedTime);
    fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
    if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
    }
    if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
    }
    FS_LOCK
    (opP->numSuccesses)++;
    FS_UNLOCK
#endif /* FS_STATS_DETAILED */

    return(0);

}  /*SRXAFS_XStatsVersion*/


/*------------------------------------------------------------------------
 * PRIVATE FillPerfValues
 *
 * Description:
 *	Routine called to fill a regular performance data structure.
 *
 * Arguments:
 *	a_perfP : Ptr to perf structure to fill
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Various collections need this info, so the guts were put in
 *	this separate routine.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void FillPerfValues(a_perfP)
    struct afs_PerfStats *a_perfP;

{ /*FillPerfValues*/

    int dir_Buffers;		/*# buffers in use by dir package*/
    int dir_Calls;		/*# read calls in dir package*/
    int dir_IOs;		/*# I/O ops in dir package*/

    /*
     * Vnode cache section.
     */
    a_perfP->vcache_L_Entries = VnodeClassInfo[vLarge].cacheSize;
    a_perfP->vcache_L_Allocs = VnodeClassInfo[vLarge].allocs;
    a_perfP->vcache_L_Gets = VnodeClassInfo[vLarge].gets;
    a_perfP->vcache_L_Reads = VnodeClassInfo[vLarge].reads;
    a_perfP->vcache_L_Writes = VnodeClassInfo[vLarge].writes;
    a_perfP->vcache_S_Entries = VnodeClassInfo[vSmall].cacheSize;
    a_perfP->vcache_S_Allocs = VnodeClassInfo[vSmall].allocs;
    a_perfP->vcache_S_Gets = VnodeClassInfo[vSmall].gets;
    a_perfP->vcache_S_Reads = VnodeClassInfo[vSmall].reads;
    a_perfP->vcache_S_Writes = VnodeClassInfo[vSmall].writes;
    a_perfP->vcache_H_Entries = VolumeCacheSize;
    a_perfP->vcache_H_Gets = VolumeGets;
    a_perfP->vcache_H_Replacements = VolumeReplacements;
    
    /*
     * Directory section.
     */
    DStat(&dir_Buffers, &dir_Calls, &dir_IOs);
    a_perfP->dir_Buffers = (afs_int32) dir_Buffers;
    a_perfP->dir_Calls = (afs_int32 )dir_Calls;
    a_perfP->dir_IOs = (afs_int32) dir_IOs;
    
    /*
     * Rx section.
     */
    a_perfP->rx_packetRequests =
	(afs_int32) rx_stats.packetRequests;
    a_perfP->rx_noPackets_RcvClass =
	(afs_int32) rx_stats.receivePktAllocFailures;
    a_perfP->rx_noPackets_SendClass =
	(afs_int32) rx_stats.sendPktAllocFailures;
    a_perfP->rx_noPackets_SpecialClass =
	(afs_int32) rx_stats.specialPktAllocFailures;
    a_perfP->rx_socketGreedy =
	(afs_int32) rx_stats.socketGreedy;
    a_perfP->rx_bogusPacketOnRead =
	(afs_int32) rx_stats.bogusPacketOnRead;
    a_perfP->rx_bogusHost =
	(afs_int32) rx_stats.bogusHost;
    a_perfP->rx_noPacketOnRead =
	(afs_int32) rx_stats.noPacketOnRead;
    a_perfP->rx_noPacketBuffersOnRead =
	(afs_int32) rx_stats.noPacketBuffersOnRead;
    a_perfP->rx_selects =
	(afs_int32) rx_stats.selects;
    a_perfP->rx_sendSelects =
	(afs_int32) rx_stats.sendSelects;
    a_perfP->rx_packetsRead_RcvClass =
	(afs_int32) rx_stats.packetsRead[RX_PACKET_CLASS_RECEIVE];
    a_perfP->rx_packetsRead_SendClass =
	(afs_int32) rx_stats.packetsRead[RX_PACKET_CLASS_SEND];
    a_perfP->rx_packetsRead_SpecialClass =
	(afs_int32) rx_stats.packetsRead[RX_PACKET_CLASS_SPECIAL];
    a_perfP->rx_dataPacketsRead =
	(afs_int32) rx_stats.dataPacketsRead;
    a_perfP->rx_ackPacketsRead =
	(afs_int32) rx_stats.ackPacketsRead;
    a_perfP->rx_dupPacketsRead =
	(afs_int32) rx_stats.dupPacketsRead;
    a_perfP->rx_spuriousPacketsRead =
	(afs_int32) rx_stats.spuriousPacketsRead;
    a_perfP->rx_packetsSent_RcvClass =
	(afs_int32) rx_stats.packetsSent[RX_PACKET_CLASS_RECEIVE];
    a_perfP->rx_packetsSent_SendClass =
	(afs_int32) rx_stats.packetsSent[RX_PACKET_CLASS_SEND];
    a_perfP->rx_packetsSent_SpecialClass =
	(afs_int32) rx_stats.packetsSent[RX_PACKET_CLASS_SPECIAL];
    a_perfP->rx_ackPacketsSent =
	(afs_int32) rx_stats.ackPacketsSent;
    a_perfP->rx_pingPacketsSent =
	(afs_int32) rx_stats.pingPacketsSent;
    a_perfP->rx_abortPacketsSent =
	(afs_int32) rx_stats.abortPacketsSent;
    a_perfP->rx_busyPacketsSent =
	(afs_int32) rx_stats.busyPacketsSent;
    a_perfP->rx_dataPacketsSent =
	(afs_int32) rx_stats.dataPacketsSent;
    a_perfP->rx_dataPacketsReSent =
	(afs_int32) rx_stats.dataPacketsReSent;
    a_perfP->rx_dataPacketsPushed =
	(afs_int32) rx_stats.dataPacketsPushed;
    a_perfP->rx_ignoreAckedPacket =
	(afs_int32) rx_stats.ignoreAckedPacket;
    a_perfP->rx_totalRtt_Sec =
	(afs_int32) rx_stats.totalRtt.sec;
    a_perfP->rx_totalRtt_Usec =
	(afs_int32) rx_stats.totalRtt.usec;
    a_perfP->rx_minRtt_Sec =
	(afs_int32) rx_stats.minRtt.sec;
    a_perfP->rx_minRtt_Usec =
	(afs_int32) rx_stats.minRtt.usec;
    a_perfP->rx_maxRtt_Sec =
	(afs_int32) rx_stats.maxRtt.sec;
    a_perfP->rx_maxRtt_Usec =
	(afs_int32) rx_stats.maxRtt.usec;
    a_perfP->rx_nRttSamples =
	(afs_int32) rx_stats.nRttSamples;
    a_perfP->rx_nServerConns =
	(afs_int32) rx_stats.nServerConns;
    a_perfP->rx_nClientConns =
	(afs_int32) rx_stats.nClientConns;
    a_perfP->rx_nPeerStructs =
	(afs_int32) rx_stats.nPeerStructs;
    a_perfP->rx_nCallStructs =
	(afs_int32) rx_stats.nCallStructs;
    a_perfP->rx_nFreeCallStructs =
	(afs_int32) rx_stats.nFreeCallStructs;
    
    a_perfP->host_NumHostEntries = HTs;
    a_perfP->host_HostBlocks = HTBlocks;
    h_GetHostNetStats(&(a_perfP->host_NonDeletedHosts),
		      &(a_perfP->host_HostsInSameNetOrSubnet),
		      &(a_perfP->host_HostsInDiffSubnet),
		      &(a_perfP->host_HostsInDiffNetwork));
    a_perfP->host_NumClients = CEs;
    a_perfP->host_ClientBlocks = CEBlocks;
    
    a_perfP->sysname_ID = afs_perfstats.sysname_ID;

} /*FillPerfValues*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFS_GetXStats
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement getting the given data collection from the extended
 *	File Server statistics.
 *
 * Arguments:
 *	a_call		    : Ptr to Rx call on which this request came in.
 *	a_clientVersionNum  : Client version number.
 *	a_opCode	    : Desired operation.
 *	a_serverVersionNumP : Ptr to version number to set.
 *	a_timeP		    : Ptr to time value (seconds) to set.
 *	a_dataP		    : Ptr to variable array structure to return
 *			      stuff in.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFS_GetXStats(a_call, a_clientVersionNum, a_collectionNumber, a_srvVersionNumP, a_timeP, a_dataP)
    struct rx_call *a_call;
    afs_int32 a_clientVersionNum;
    afs_int32 a_collectionNumber;
    afs_int32 *a_srvVersionNumP;
    afs_int32 *a_timeP;
    AFS_CollData *a_dataP;

{ /*SRXAFS_GetXStats*/

    register int code;		/*Return value*/
    afs_int32 *dataBuffP;		/*Ptr to data to be returned*/
    afs_int32 dataBytes;		/*Bytes in data buffer*/
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETXSTATS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    /*
     * Record the time of day and the server version number.
     */
    *a_srvVersionNumP = AFS_XSTAT_VERSION;
    *a_timeP = FT_ApproxTime();

    /*
     * Stuff the appropriate data in there (assume victory)
     */
    code = 0;

    ViceLog(1, ("Received GetXStats call for collection %d\n",  a_collectionNumber));

#if 0
    /*
     * We're not keeping stats, so just return successfully with
     * no data.
     */
    a_dataP->AFS_CollData_len = 0;
    a_dataP->AFS_CollData_val = (afs_int32 *)0;
#endif /* 0 */

    switch(a_collectionNumber) {
      case AFS_XSTATSCOLL_CALL_INFO:
	/*
	 * Pass back all the call-count-related data.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
#if 0
	/*
	 * I don't think call-level stats are being collected yet
	 * for the File Server.
	 */
	dataBytes = sizeof(struct afs_Stats);
	dataBuffP = (afs_int32 *)malloc(dataBytes);
	bcopy(&afs_cmstats, dataBuffP, dataBytes);
	a_dataP->AFS_CollData_len = dataBytes>>2;
	a_dataP->AFS_CollData_val = dataBuffP;
#else
	a_dataP->AFS_CollData_len = 0;
	a_dataP->AFS_CollData_val = (afs_int32 *)0;
#endif /* 0 */
	break;

      case AFS_XSTATSCOLL_PERF_INFO:
	/*
	 * Pass back all the regular performance-related data.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */

	afs_perfstats.numPerfCalls++;
	FillPerfValues(&afs_perfstats);

	/*
	 * Don't overwrite the spares at the end.
	 */

	dataBytes = sizeof(struct afs_PerfStats);
	dataBuffP = (afs_int32 *)osi_Alloc(dataBytes);
	bcopy(&afs_perfstats, dataBuffP, dataBytes);
	a_dataP->AFS_CollData_len = dataBytes>>2;
	a_dataP->AFS_CollData_val = dataBuffP;
	break;

      case AFS_XSTATSCOLL_FULL_PERF_INFO:
	/*
	 * Pass back the full collection of performance-related data.
	 * We have to stuff the basic, overall numbers in, but the
	 * detailed numbers are kept in the structure already.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */

	afs_perfstats.numPerfCalls++;
#if FS_STATS_DETAILED
	afs_FullPerfStats.overall.numPerfCalls = afs_perfstats.numPerfCalls;
	FillPerfValues(&afs_FullPerfStats.overall);

	/*
	 * Don't overwrite the spares at the end.
	 */

	dataBytes = sizeof(struct fs_stats_FullPerfStats);
	dataBuffP = (afs_int32 *)osi_Alloc(dataBytes);
	bcopy(&afs_FullPerfStats, dataBuffP, dataBytes);
	a_dataP->AFS_CollData_len = dataBytes>>2;
	a_dataP->AFS_CollData_val = dataBuffP;
#endif
	break;

      default:
	/*
	 * Illegal collection number.
	 */
	a_dataP->AFS_CollData_len = 0;
	a_dataP->AFS_CollData_val = (afs_int32 *)0;
	code = 1;
    } /*Switch on collection number*/

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    return(code);

} /*SRXAFS_GetXStats*/


SRXAFS_GiveUpCallBacks (tcon, FidArray, CallBackArray)
    struct rx_connection *tcon;		/* Rx connection handle */
    struct AFSCBFids *FidArray;		/* Array of Fids entries */
    struct AFSCBs *CallBackArray;	/* array of callbacks */

{
    afs_int32 errorCode;
    register int i;
    struct client *client;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GIVEUPCALLBACKS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1, ("SAFS_GiveUpCallBacks (Noffids=%d)\n", FidArray->AFSCBFids_len));
    FS_LOCK
    AFSCallStats.GiveUpCallBacks++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_GiveUpCallBacks;

    if (FidArray->AFSCBFids_len < CallBackArray->AFSCBs_len) {
       ViceLog(0, ("GiveUpCallBacks: #Fids %d < #CallBacks %d, host=%x\n", 
		   FidArray->AFSCBFids_len, CallBackArray->AFSCBs_len, 
		   (tcon->peer ? tcon->peer->host : 0)));
       errorCode = EINVAL;
       goto Bad_GiveUpCallBacks;
    }

    errorCode = GetClient(tcon, &client);
    if (!errorCode) {
       for (i=0; i < FidArray->AFSCBFids_len; i++) {
	  register struct AFSFid *fid = &(FidArray->AFSCBFids_val[i]);
	  DeleteCallBack(client->host, fid);
       }
    }

Bad_GiveUpCallBacks:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */
out:
    return errorCode;

} /*SRXAFS_GiveUpCallBacks*/


SRXAFS_NGetVolumeInfo (tcon, avolid, avolinfo)
    struct rx_connection *tcon;		/* Rx connection handle */
    char *avolid;			/* Volume name/id */
    struct AFSVolumeInfo *avolinfo;	/* Returned volume's specific info */

{
    return(VNOVOL);		/* XXX Obsolete routine XXX */

} /*SRXAFS_NGetVolumeInfo*/


/*
 * Dummy routine. Should never be called (the cache manager should only 
 * invoke this interface when communicating with a AFS/DFS Protocol
 * Translator).
 */
SRXAFS_Lookup(call_p, afs_dfid_p, afs_name_p, afs_fid_p,
	      afs_status_p, afs_dir_status_p, afs_callback_p, afs_sync_p)
  struct rx_call *call_p;		/* Rx call handle */
  struct AFSFid *afs_dfid_p;		/* Directory */
  char *afs_name_p;			/* Name of file to lookup */
  struct AFSFid *afs_fid_p;		/* Place to return fid of file */
  struct AFSFetchStatus *afs_status_p;	/* Place to return file status */
  struct AFSFetchStatus *afs_dir_status_p;/* Place to return file status */
  struct AFSCallBack *afs_callback_p;	/* If r/w, callback promise for Fid */
  struct AFSVolSync *afs_sync_p;	/* Volume sync info */
{
    return EINVAL;
}


SRXAFS_FlushCPS(tcon, vids, addrs, spare1, spare2, spare3)
    struct rx_connection *tcon;
    struct ViceIds *vids;
    struct IPAddrs *addrs;
    afs_int32 spare1, *spare2, *spare3;
{
    int i;
    afs_int32 nids, naddrs;
    afs_int32 *vd, *addr;
    int	    errorCode =	0;		/* return code to caller */
    struct client *client;
    struct rx_call *tcall = (struct rx_call *) tcon; 

    ViceLog(1, ("SRXAFS_FlushCPS\n"));
    FS_LOCK
    AFSCallStats.TotalCalls++;
    FS_UNLOCK
    nids = vids->ViceIds_len;	/* # of users in here */
    naddrs = addrs->IPAddrs_len;	/* # of hosts in here */
    if (nids < 0 || naddrs < 0) {
	errorCode = EINVAL;
	goto Bad_FlushCPS;
    }

    vd = vids->ViceIds_val;
    for (i=0; i<nids; i++, vd++) {
      if (!*vd)
	continue;
      client = h_ID2Client(*vd);  /* returns client locked, or NULL */
      if (!client)
	continue;

      BoostSharedLock(&client->lock);
      client->prfail = 2;	/* Means re-eval client's cps */
#ifdef	notdef
      if (client->tcon) {
	rx_SetRock(((struct rx_connection *) client->tcon), 0);
      }
#endif
      if ((client->ViceId != ANONYMOUSID) && client->CPS.prlist_val) {
	free(client->CPS.prlist_val);
	client->CPS.prlist_val = (afs_int32 *)0;
      }
      ReleaseWriteLock(&client->lock);
    }

    addr = addrs->IPAddrs_val;
    for (i=0; i<naddrs; i++, addr++) {
      if (*addr)
	h_flushhostcps(*addr, 7001);
    }

Bad_FlushCPS:
    ViceLog(2, ("SAFS_FlushCPS	returns	%d\n", errorCode)); 
    return errorCode;
} /*SRXAFS_FlushCPS */



static GetVolumeInfo (tcon, avolid, avolinfo)
    struct rx_connection *tcon;		/* Rx Connection handle */
    char *avolid;			/* Volume name/id */
    struct VolumeInfo *avolinfo;	/* Returned volume's specific info */

{
    int errorCode = 0;			/* error code */

    FS_LOCK
    AFSCallStats.GetVolumeInfo++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    errorCode = TryLocalVLServer(avolid, avolinfo);
    ViceLog(1, ("SAFS_GetVolumeInfo returns %d, Volume %u, type %x, servers %x %x %x %x...\n",
	    errorCode, avolinfo->Vid, avolinfo->Type,
	    avolinfo->Server0, avolinfo->Server1, avolinfo->Server2,
	    avolinfo->Server3));
    return(errorCode);
}


/* worthless hack to let CS keep running ancient software */
static afs_vtoi(aname)
    register char *aname;

{
    register afs_int32 temp;
    register int tc;

    temp = 0;
    while(tc = *aname++) {
	if (tc > '9' ||	tc < '0') return 0; /* invalid name */
	temp *= 10;
	temp += tc - '0';
    }
    return temp;
}

/*
 * may get name or #, but must handle all weird cases (recognize readonly
 * or backup volumes by name or #
 */
static CopyVolumeEntry(aname, ave, av)
    char *aname;
    register struct VolumeInfo *av;
    register struct vldbentry *ave;

{
    register int i, j, vol;
    afs_int32 mask, whichType;
    afs_uint32 *serverHost, *typePtr;
    
    /* figure out what type we want if by name */
    i = strlen(aname);
    if (i >= 8 && strcmp(aname+i-7, ".backup") == 0)
	whichType = BACKVOL;
    else if (i >= 10 && strcmp(aname+i-9, ".readonly")==0)
	whichType = ROVOL;
    else whichType = RWVOL;
    
    vol = afs_vtoi(aname);
    if (vol == 0) vol = ave->volumeId[whichType];
    
    /*
     * Now vol has volume # we're interested in.  Next, figure out the type
     * of the volume by looking finding it in the vldb entry
     */
    if ((ave->flags&VLF_RWEXISTS) && vol == ave->volumeId[RWVOL]) {
	mask = VLSF_RWVOL;
	whichType = RWVOL;
    }
    else if ((ave->flags&VLF_ROEXISTS) && vol == ave->volumeId[ROVOL]) {
	mask = VLSF_ROVOL;
	whichType = ROVOL;
    }
    else if ((ave->flags&VLF_BACKEXISTS) && vol == ave->volumeId[BACKVOL]) {
	mask = VLSF_RWVOL;  /* backup always is on the same volume as parent */
	whichType = BACKVOL;
    }
    else
	return	EINVAL;	    /* error: can't find volume in vldb entry */
    
    typePtr = &av->Type0;
    serverHost = &av->Server0;
    av->Vid = vol;
    av->Type = whichType;
    av->Type0 = av->Type1 = av->Type2 = av->Type3 = av->Type4 = 0;
    if (ave->flags & VLF_RWEXISTS) typePtr[RWVOL] = ave->volumeId[RWVOL];
    if (ave->flags & VLF_ROEXISTS) typePtr[ROVOL] = ave->volumeId[ROVOL];
    if (ave->flags & VLF_BACKEXISTS) typePtr[BACKVOL] = ave->volumeId[BACKVOL];

    for(i=0,j=0; i<ave->nServers; i++) {
	if ((ave->serverFlags[i] & mask) == 0) continue;    /* wrong volume */
	serverHost[j] = ave->serverNumber[i];
	j++;
    }
    av->ServerCount = j;
    if (j < 8) serverHost[j++] = 0; /* bogus 8, but compat only now */
    return 0;
}


static TryLocalVLServer(avolid, avolinfo)
    char *avolid;
    struct VolumeInfo *avolinfo;

{
    static struct rx_connection *vlConn = 0;
    static int down = 0;
    static afs_int32 lastDownTime = 0;
    struct vldbentry tve;
    struct rx_securityClass *vlSec;
    register afs_int32 code;

    if (!vlConn) {
	vlSec = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
	vlConn = rx_NewConnection(htonl(0x7f000001), htons(7003), 52, vlSec, 0);
	rx_SetConnDeadTime(vlConn, 15);	/* don't wait long */
    }
    if (down && (FT_ApproxTime() < lastDownTime + 180)) {
	return 1;   /* failure */
    }

    code = VL_GetEntryByNameO(vlConn, avolid, &tve);
    if (code >=	0) down	= 0;	/* call worked */
    if (code) {
	if (code < 0) {
	    lastDownTime = FT_ApproxTime(); /* last time we tried an RPC */
	    down = 1;
	}
	return code;
    }

    /* otherwise convert to old format vldb entry */
    code = CopyVolumeEntry(avolid, &tve, avolinfo);
    return code;
}


SRXAFS_GetVolumeInfo (tcon, avolid, avolinfo)
    struct rx_connection *tcon;		/* Rx connection handle */
    char *avolid;			/* Volume name/id */
    struct VolumeInfo *avolinfo;	/* Returned volume's specific info */

{
    afs_int32 code;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETVOLUMEINFO]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */
    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_GetVolumeInfo;

    code = GetVolumeInfo (tcon, avolid, avolinfo);
    avolinfo->Type4	= 0xabcd9999;	/* tell us to try new vldb */

Bad_GetVolumeInfo:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    return code;

} /*SRXAFS_GetVolumeInfo*/


SRXAFS_GetVolumeStatus (tcon, avolid, FetchVolStatus, Name, OfflineMsg, Motd)
    struct rx_connection *tcon;		  /* Rx connection handle */
    afs_int32 avolid;			  /* Volume's id */
    AFSFetchVolumeStatus *FetchVolStatus; /* Place to hold volume's status info */
    char **Name;		    	  /* Returned volume's name */
    char **OfflineMsg;	    		  /* Returned offline msg, if any */
    char **Motd;			  /* Returned Motd msg, if any */

{
    Vnode * targetptr =	0;		/* vnode of the new file */
    Vnode * parentwhentargetnotdir = 0;	/* vnode of parent */
    int	    errorCode =	0;		/* error code */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client entry */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    AFSFid  dummyFid;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETVOLUMESTATUS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1,("SAFS_GetVolumeStatus for volume %u\n", avolid));
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_GetVolumeStatus;

    FS_LOCK
    AFSCallStats.GetVolumeStatus++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    if (avolid == 0) {
	errorCode = EINVAL;
	goto Bad_GetVolumeStatus;
    }
    dummyFid.Volume = avolid, dummyFid.Vnode = (afs_int32)ROOTVNODE, dummyFid.Unique = 1;

    if (errorCode = GetVolumePackage(tcon, &dummyFid, &volptr, &targetptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, READ_LOCK, &rights, &anyrights))
	goto Bad_GetVolumeStatus;

    if ((VanillaUser(client)) && (!(rights & PRSFS_READ))) {
	errorCode = EACCES;
	goto Bad_GetVolumeStatus;
    }
    RXGetVolumeStatus(FetchVolStatus, Name, OfflineMsg, Motd, volptr);

Bad_GetVolumeStatus:
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2,("SAFS_GetVolumeStatus returns %d\n",errorCode));
    /* next is to guarantee out strings exist for stub */
    if (*Name == 0) {*Name = (char *) malloc(1); **Name = 0;}
    if (*Motd == 0) {*Motd = (char *) malloc(1); **Motd = 0;}
    if (*OfflineMsg == 0) {*OfflineMsg = (char *) malloc(1); **OfflineMsg = 0;}
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */
    return(errorCode);

} /*SRXAFS_GetVolumeStatus*/


SRXAFS_SetVolumeStatus (tcon, avolid, StoreVolStatus, Name, OfflineMsg, Motd)
    struct rx_connection *tcon;		  /* Rx connection handle */
    afs_int32 avolid;			  /* Volume's id */
    AFSStoreVolumeStatus *StoreVolStatus; /* Adjusted output volume's status */
    char *Name;				  /* Set new volume's name, if applicable */
    char *OfflineMsg;			  /* Set new offline msg, if applicable */
    char *Motd;				  /* Set new motd msg, if applicable */

{
    Vnode * targetptr =	0;		/* vnode of the new file */
    Vnode * parentwhentargetnotdir = 0;	/* vnode of parent */
    int	    errorCode =	0;		/* error code */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client entry */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    AFSFid  dummyFid;
    struct rx_call *tcall = (struct rx_call *) tcon; 
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_SETVOLUMESTATUS]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    ViceLog(1,("SAFS_SetVolumeStatus for volume %u\n", avolid));
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_SetVolumeStatus;

    FS_LOCK
    AFSCallStats.SetVolumeStatus++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    if (avolid == 0) {
	errorCode = EINVAL;
	goto Bad_SetVolumeStatus;
    }
    dummyFid.Volume = avolid, dummyFid.Vnode = (afs_int32)ROOTVNODE, dummyFid.Unique = 1;

    if (errorCode = GetVolumePackage(tcon, &dummyFid, &volptr, &targetptr,
				     MustBeDIR, &parentwhentargetnotdir,
				     &client, READ_LOCK, &rights, &anyrights))
	goto Bad_SetVolumeStatus;

    if (VanillaUser(client)) {
	errorCode = EACCES;
	goto Bad_SetVolumeStatus;
    }

    errorCode = RXUpdate_VolumeStatus(volptr, StoreVolStatus, Name,
				      OfflineMsg, Motd);

 Bad_SetVolumeStatus:
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2,("SAFS_SetVolumeStatus returns %d\n",errorCode));
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    osi_auditU (tcall, SetVolumeStatusEvent, errorCode, AUD_LONG, avolid, AUD_STR, Name, AUD_END);
    return(errorCode);

} /*SRXAFS_SetVolumeStatus*/

#define	DEFAULTVOLUME	"root.afs"


SRXAFS_GetRootVolume (tcon, VolumeName)
    struct rx_connection * tcon; /* R-Connection handle */
    char **VolumeName;		 /* Returned AFS's root volume name */

{
    int fd;
    int len;
    char *temp;
    int errorCode = 0;		/* error code */
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETROOTVOLUME]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    return FSERR_EOPNOTSUPP;

#ifdef	notdef
    if (errorCode = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_GetRootVolume;
    FS_LOCK
    AFSCallStats.GetRootVolume++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    temp = malloc(256);
    fd = open(AFSDIR_SERVER_ROOTVOL_FILEPATH, O_RDONLY, 0666);
    if (fd <= 0)
	strcpy(temp, DEFAULTVOLUME);
    else {
#if defined (AFS_AIX_ENV) || defined (AFS_HPUX_ENV)
	lockf(fd, F_LOCK, 0);
#else
	flock(fd, LOCK_EX);
#endif
	len = read(fd, temp, 256);
#if defined (AFS_AIX_ENV) || defined (AFS_HPUX_ENV)
	lockf(fd, F_ULOCK, 0);
#else
	flock(fd, LOCK_UN);
#endif
	close(fd);
	if (temp[len-1] == '\n') len--;
	temp[len] = '\0';
    }
    *VolumeName	= temp;	    /* freed by rx server-side stub */

Bad_GetRootVolume:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (errorCode == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }
#endif /* FS_STATS_DETAILED */

    return(errorCode);
#endif	/* notdef */

} /*SRXAFS_GetRootVolume*/


/* still works because a struct CBS is the same as a struct AFSOpaque */
SRXAFS_CheckToken (tcon, AfsId, Token)
    struct rx_connection *tcon; /* Rx connection handle */
    afs_int32 AfsId;	    		/* AFS id whose token we verify */
    struct CBS *Token;		/* Token value for used Afsid */

{
    afs_int32 code;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_CHECKTOKEN]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, ACTIVECALL))
	goto Bad_CheckToken;

    code = FSERR_ECONNREFUSED;
    
Bad_CheckToken:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    return code;

} /*SRXAFS_CheckToken*/


static GetTime (tcon, Seconds, USeconds)
    struct rx_connection *tcon;	    /* R-Connection handle */
    afs_uint32 *Seconds;	    /* Returned time in seconds */
    afs_uint32 *USeconds;	    /* Returned leftovers in useconds */

{
    struct timeval tpl;

    FS_LOCK
    AFSCallStats.GetTime++, AFSCallStats.TotalCalls++;
    FS_UNLOCK

    TM_GetTimeOfDay(&tpl, 0);
    *Seconds = tpl.tv_sec;
    *USeconds = tpl.tv_usec;

    ViceLog(2, ("SAFS_GetTime returns %d, %d\n", *Seconds, *USeconds));
    return(0);

} /*GetTime*/


SRXAFS_GetTime (tcon, Seconds, USeconds)
    struct rx_connection *tcon;	    /* Rx connection handle */
    afs_uint32 *Seconds;	    /* Returned time in seconds */
    afs_uint32 *USeconds;	    /* Returned leftovers in useconds */
{
    afs_int32 code;
#if FS_STATS_DETAILED
    struct fs_stats_opTimingData *opP;      /* Ptr to this op's timing struct */
    struct timeval opStartTime,
                   opStopTime;		    /* Start/stop times for RPC op*/
    struct timeval elapsedTime;		    /* Transfer time */

    /*
     * Set our stats pointer, remember when the RPC operation started, and
     * tally the operation.
     */
    opP = &(afs_FullPerfStats.det.rpcOpTimes[FS_STATS_RPCIDX_GETTIME]);
    FS_LOCK
    (opP->numOps)++;
    FS_UNLOCK
    TM_GetTimeOfDay(&opStartTime, 0);
#endif /* FS_STATS_DETAILED */

    if (code = CallPreamble((struct rx_call **) &tcon, NOTACTIVECALL))
	goto Bad_GetTime;

    code = GetTime (tcon, Seconds, USeconds);
    
Bad_GetTime:
    CallPostamble(tcon);

#if FS_STATS_DETAILED
    TM_GetTimeOfDay(&opStopTime, 0);
    fs_stats_GetDiff(elapsedTime, opStartTime, opStopTime);
    if (code == 0) {
      FS_LOCK
      (opP->numSuccesses)++;
      fs_stats_AddTo((opP->sumTime), elapsedTime);
      fs_stats_SquareAddTo((opP->sqrTime), elapsedTime);
      if (fs_stats_TimeLessThan(elapsedTime, (opP->minTime))) {
	fs_stats_TimeAssign((opP->minTime), elapsedTime);
      }
      if (fs_stats_TimeGreaterThan(elapsedTime, (opP->maxTime))) {
	fs_stats_TimeAssign((opP->maxTime), elapsedTime);
      }
      FS_UNLOCK
    }

#endif /* FS_STATS_DETAILED */

    return code;

} /*SRXAFS_GetTime*/


/*=============================================================================*/
/*									       */
/* AUXILIARY functions that are used by the main AFS interface procedure calls */
/*									       */
/*=============================================================================*/


/*
 * This unusual afs_int32-parameter routine encapsulates all volume package related
 * operations together in a single function; it's called by almost all AFS
 * interface calls.
 */
GetVolumePackage(tcon, Fid, volptr, targetptr, chkforDir, parent, client, locktype, rights, anyrights)
    struct rx_connection *tcon; /* Rx connection */
    AFSFid *Fid;		/* Fid that we are dealing with */
    Volume **volptr;		/* Returns pointer to volume associated with Fid */
    Vnode **targetptr;		/* Returns pointer to vnode associated with Fid */
    int chkforDir;	   	 /* Flag testing whether Fid is/or not a dir */
    Vnode **parent;	    	/* If Fid not a dir, this points to the parent dir */
    struct client **client;	/* Returns the client associated with the conn */
    int locktype;	        /* locktype (READ or WRITE) for the Fid vnode */
    afs_int32 *rights, *anyrights;   /* Returns user's & any acl rights */

{
    struct acl_accessList * aCL;    /* Internal access List */
    int	aCLSize;	    /* size of the access list */
    int	errorCode = 0;	    /* return code to caller */

    if (errorCode = CheckVnode(Fid, volptr, targetptr, locktype))
	return(errorCode);
    if (chkforDir) {
	if (chkforDir == MustNOTBeDIR && ((*targetptr)->disk.type == vDirectory))
	    return(EISDIR);
	else if (chkforDir == MustBeDIR && ((*targetptr)->disk.type != vDirectory))
	    return(ENOTDIR);
    }
    if ((errorCode = SetAccessList(targetptr, volptr, &aCL, &aCLSize, parent, (chkforDir == MustBeDIR ? (AFSFid *)0 : Fid), (chkforDir == MustBeDIR ? 0 : locktype))) != 0)
	return(errorCode);
    if (chkforDir == MustBeDIR) assert((*parent) == 0);
    if ((errorCode = GetClient(tcon, client)) != 0)
	return(errorCode);
    if (!(*client))
	return(EINVAL);
    assert(GetRights(*client, aCL, rights, anyrights) == 0);
    /* ok, if this is not a dir, set the PRSFS_ADMINISTER bit iff we're the owner */
    if ((*targetptr)->disk.type != vDirectory) {
	/* anyuser can't be owner, so only have to worry about rights, not anyrights */
	if ((*targetptr)->disk.owner == (*client)->ViceId)
	    (*rights) |= PRSFS_ADMINISTER;
	else
	    (*rights) &= ~PRSFS_ADMINISTER;
    }
#ifdef ADMIN_IMPLICIT_LOOKUP
    /* admins get automatic lookup on everything */
    if (!VanillaUser(*client)) (*rights) |= PRSFS_LOOKUP;
#endif /* ADMIN_IMPLICIT_LOOKUP */
    return errorCode;

} /*GetVolumePackage*/


/*
 * This is the opposite of GetVolumePackage(), and is always used at the end of
 * AFS calls to put back all used vnodes and the volume in the proper order!
 */
PutVolumePackage(parentwhentargetnotdir, targetptr, parentptr, volptr)
    Vnode *parentwhentargetnotdir, *targetptr, *parentptr;
    Volume *volptr;

{
    int	fileCode = 0;	/* Error code returned by the volume package */

    if (parentwhentargetnotdir) {
	VPutVnode(&fileCode, parentwhentargetnotdir);
	assert(!fileCode || (fileCode == VSALVAGE));
    }
    if (targetptr) {
	VPutVnode(&fileCode, targetptr);
	assert(!fileCode || (fileCode == VSALVAGE));
    }
    if (parentptr) {
	VPutVnode(&fileCode, parentptr);
	assert(!fileCode || (fileCode == VSALVAGE));
    }
    if (volptr) {
	VPutVolume(volptr);
    }
} /*PutVolumePackage*/


#if FS_STATS_DETAILED
/*
 * FetchData_RXStyle
 *
 * Purpose:
 *	Implement a client's data fetch using Rx.
 *
 * Arguments:
 *	volptr		: Ptr to the given volume's info.
 *	targetptr	: Pointer to the vnode involved.
 *	Call		: Ptr to the Rx call involved.
 *	Pos		: Offset within the file.
 *	Len		: Length in bytes to read; this value is bogus!
 *	a_bytesToFetchP	: Set to the number of bytes to be fetched from
 *			  the File Server.
 *	a_bytesFetchedP	: Set to the actual number of bytes fetched from
 #			  the File Server.
 */

FetchData_RXStyle(volptr, targetptr, Call, Pos, Len, a_bytesToFetchP, a_bytesFetchedP)
#else
FetchData_RXStyle(volptr, targetptr, Call, Pos, Len)
#endif /* FS_STATS_DETAILED */
Volume			* volptr;
Vnode			* targetptr;
register struct rx_call		* Call;
afs_int32			Pos;
afs_int32			Len;
#if FS_STATS_DETAILED
afs_int32 *a_bytesToFetchP;
afs_int32 *a_bytesFetchedP;
#endif /* FS_STATS_DETAILED */
{
    struct timeval StartTime, StopTime; /* used to calculate file  transfer rates */
    int	errorCode = 0;			/* Returned error code to caller */
    int code;
    IHandle_t *ihP;
    FdHandle_t *fdP;
#ifdef AFS_NT40_ENV
    register char *tbuffer;
#else /* AFS_NT40_ENV */
    struct iovec tiov[RX_MAXIOVECS];
    int tnio;
#endif /* AFS_NT40_ENV */
    int tlen;
    afs_int32 optSize;
    struct stat tstat;
#ifdef	AFS_AIX_ENV
    struct statfs tstatfs;
#endif

#if FS_STATS_DETAILED
    /*
     * Initialize the byte count arguments.
     */
    (*a_bytesToFetchP) = 0;
    (*a_bytesFetchedP) = 0;
#endif /* FS_STATS_DETAILED */

    if (!VN_GET_INO(targetptr)) {
	/*
	 * This is used for newly created files; we simply send 0 bytes
	 * back to make the cache manager happy...
	 */
	tlen = htonl(0);
	rx_Write(Call, &tlen, sizeof(afs_int32));	/* send 0-length  */
	return (0);
    }
    TM_GetTimeOfDay(&StartTime, 0);
    ihP = targetptr->handle;
    fdP = IH_OPEN(ihP);
    if (fdP == NULL) return EIO;
    optSize = AFSV_BUFFERSIZE;
    tlen = FDH_SIZE(fdP);
    if (tlen < 0) {
	FDH_CLOSE(fdP);
	return EIO;
    }

    if (Pos + Len > tlen) Len =	tlen - Pos;	/* get length we should send */
    FDH_SEEK(fdP, Pos, 0);
    tlen = htonl(Len);
    rx_Write(Call, &tlen, sizeof(afs_int32));	/* send length on fetch */
#if FS_STATS_DETAILED
    (*a_bytesToFetchP) = Len;
#endif /* FS_STATS_DETAILED */
#ifdef AFS_NT40_ENV
    tbuffer = AllocSendBuffer();
#endif /* AFS_NT40_ENV */
    while (Len > 0) {
	if (Len > optSize) tlen = optSize;
	else tlen = Len;
#ifdef AFS_NT40_ENV
	errorCode = FDH_READ(fdP, tbuffer, tlen);
	if (errorCode != tlen) {
	    FDH_CLOSE(fdP);
	    FreeSendBuffer((struct afs_buffer *) tbuffer);
	    return EIO;
	}
	errorCode = rx_Write(Call, tbuffer, tlen);
#else /* AFS_NT40_ENV */
	errorCode = rx_WritevAlloc(Call, tiov, &tnio, RX_MAXIOVECS, tlen);
	if (errorCode <= 0) {
	    FDH_CLOSE(fdP);
	    return EIO;
	}
	tlen = errorCode;
	errorCode = FDH_READV(fdP, tiov, tnio);
	if (errorCode != tlen) {
	    FDH_CLOSE(fdP);
	    return EIO;
	}
	errorCode = rx_Writev(Call, tiov, tnio, tlen);
#endif /* AFS_NT40_ENV */
#if FS_STATS_DETAILED
	/*
	  * Bump the number of bytes actually sent by the number from this
	  * latest iteration
	  */
	(*a_bytesFetchedP) += errorCode;
#endif /* FS_STATS_DETAILED */
	if (errorCode != tlen) {
	    FDH_CLOSE(fdP);
#ifdef AFS_NT40_ENV
	    FreeSendBuffer((struct afs_buffer *) tbuffer);
#endif /* AFS_NT40_ENV */
	    return -31;
	}
	Len -= tlen;
    }
#ifdef AFS_NT40_ENV
    FreeSendBuffer((struct afs_buffer *) tbuffer);
#endif /* AFS_NT40_ENV */
    FDH_CLOSE(fdP);
    TM_GetTimeOfDay(&StopTime, 0);

    /* Adjust all Fetch Data related stats */
    FS_LOCK
    if (AFSCallStats.TotalFetchedBytes > 2000000000) /* Reset if over 2 billion */
	AFSCallStats.TotalFetchedBytes = AFSCallStats.AccumFetchTime = 0;
    AFSCallStats.AccumFetchTime += ((StopTime.tv_sec - StartTime.tv_sec) * 1000) +
      ((StopTime.tv_usec - StartTime.tv_usec) / 1000);
    AFSCallStats.TotalFetchedBytes += targetptr->disk.length;
    AFSCallStats.FetchSize1++;
    if (targetptr->disk.length < SIZE2)
	AFSCallStats.FetchSize2++;  
    else if (targetptr->disk.length < SIZE3)
	AFSCallStats.FetchSize3++;
    else if (targetptr->disk.length < SIZE4)
	AFSCallStats.FetchSize4++;  
    else
	AFSCallStats.FetchSize5++;
    FS_UNLOCK
    return (0);

} /*FetchData_RXStyle*/

static int GetLinkCountAndSize(Volume *vp, FdHandle_t *fdP, int *lc, int *size)
{
#ifdef AFS_NAMEI_ENV
    FdHandle_t *lhp;
    lhp = IH_OPEN(V_linkHandle(vp));
    if (!lhp)
	return EIO;
#ifdef AFS_NT40_ENV
    *lc = nt_GetLinkCount(lhp, fdP->fd_ih->ih_ino, 0);
#else
    *lc = namei_GetLinkCount(lhp, fdP->fd_ih->ih_ino, 0);
#endif
    FDH_CLOSE(lhp);
    if (*lc < 0 )
	return -1;
    *size = OS_SIZE(fdP->fd_fd);
    return (*size == -1) ? -1 : 0;
#else
    struct stat status;

    if (fstat(fdP->fd_fd, &status)<0) {
	return -1;
    }

    *lc = GetLinkCount(vp, &status);
    *size = status.st_size;
    return 0;
#endif   
}

#if FS_STATS_DETAILED
/*
 * StoreData_RXStyle
 *
 * Purpose:
 *	Implement a client's data store using Rx.
 *
 * Arguments:
 *	volptr		: Ptr to the given volume's info.
 *	targetptr	: Pointer to the vnode involved.
 *	Call		: Ptr to the Rx call involved.
 *	Pos		: Offset within the file.
 *	Len		: Length in bytes to store; this value is bogus!
 *	a_bytesToStoreP	: Set to the number of bytes to be stored to
 *			  the File Server.
 *	a_bytesStoredP	: Set to the actual number of bytes stored to
 #			  the File Server.
 */

StoreData_RXStyle(volptr, targetptr, Fid, client, Call, Pos, Length,
		  FileLength, sync, a_bytesToStoreP, a_bytesStoredP)
#else
StoreData_RXStyle(volptr, targetptr, Fid, client, Call, Pos, Length,
		  FileLength, sync)
#endif /* FS_STATS_DETAILED */

    Volume *volptr;
    Vnode *targetptr;
    struct AFSFid *Fid;
    struct client *client;
    register struct rx_call *Call;
    afs_int32 Pos;
    afs_int32 Length;
    afs_int32 FileLength;
    int sync;
#if FS_STATS_DETAILED
    afs_int32 *a_bytesToStoreP;
    afs_int32 *a_bytesStoredP;
#endif /* FS_STATS_DETAILED */

{
    int	    bytesTransfered;		/* number of bytes actually transfered */
    struct timeval StartTime, StopTime;	/* Used to measure how long the store takes */
    int	errorCode = 0;			/* Returned error code to caller */
#ifdef AFS_NT40_ENV
    register char *tbuffer;		/* data copying buffer */
#else /* AFS_NT40_ENV */
    struct iovec tiov[RX_MAXIOVECS];    /* no data copying with iovec */
    int tnio;				/* temp for iovec size */
#endif /* AFS_NT40_ENV */
    int	tlen;				/* temp for xfr length */
    Inode tinode;			/* inode for I/O */
    afs_int32 optSize;			/* optimal transfer size */
    int DataLength;			/* size of inode */
    afs_int32 TruncatedLength;		/* size after ftruncate */
    afs_int32 NewLength;			/* size after this store completes */
    afs_int32 adjustSize;			/* bytes to call VAdjust... with */
    int linkCount;			/* link count on inode */
    int code;
    FdHandle_t *fdP;
    struct in_addr logHostAddr;             /* host ip holder for inet_ntoa */

#if FS_STATS_DETAILED
    /*
     * Initialize the byte count arguments.
     */
    (*a_bytesToStoreP) = 0;
    (*a_bytesStoredP)  = 0;
#endif /* FS_STATS_DETAILED */

    /*
     * We break the callbacks here so that the following signal will not
     * leave a window.
     */
    BreakCallBack(client->host, Fid, 0);

    if (Pos == -1 || VN_GET_INO(targetptr) == 0) {
	/* the inode should have been created in Alloc_NewVnode */
	logHostAddr.s_addr = rx_HostOf(rx_PeerOf(rx_ConnectionOf(Call)));
	ViceLog(0, ("StoreData_RXStyle : Inode non-existent Fid = %u.%d.%d, inode = %d, Pos %d Host %s\n",
		Fid->Volume, Fid->Vnode, Fid->Unique, 
		VN_GET_INO(targetptr), Pos, inet_ntoa(logHostAddr) ));
	return ENOENT;  /* is this proper error code? */
    }
    else {
	/*
	 * See if the file has several links (from other volumes).  If it
	 * does, then we have to make a copy before changing it to avoid
	 *changing read-only clones of this dude
	 */
	ViceLog(25, ("StoreData_RXStyle : Opening inode %s\n",
		     PrintInode(NULL, VN_GET_INO(targetptr))));
	fdP = IH_OPEN(targetptr->handle);
	if (fdP == NULL) 
	    return ENOENT;
	if (GetLinkCountAndSize(volptr, fdP, &linkCount,
				&DataLength)<0) {
	    FDH_CLOSE(fdP);
	    return EIO;
	}
	
	if (linkCount != 1) {
	    int size;
	    ViceLog(25, ("StoreData_RXStyle : inode %s has more than onelink\n",
			 PrintInode(NULL, VN_GET_INO(targetptr))));
	    /* other volumes share this data, better copy it first */

	    /* Adjust the disk block count by the creation of the new inode.
	     * We call the special VDiskUsage so we don't adjust the volume's
	     * quota since we don't want to penalyze the user for afs's internal
	     * mechanisms (i.e. copy on write overhead.) Also the right size
	     * of the disk will be recorded...
	     */
	    FDH_CLOSE(fdP);
	    size = targetptr->disk.length;
	    volptr->partition->flags &= ~PART_DONTUPDATE;
	    VSetPartitionDiskUsage(volptr->partition);
	    volptr->partition->flags |= PART_DONTUPDATE;
	    if (errorCode = VDiskUsage(volptr, nBlocks(size))) {
		volptr->partition->flags &= ~PART_DONTUPDATE;
		return(errorCode);
	    }

	    ViceLog(25, ("StoreData : calling CopyOnWrite on  target dir\n"));
	    if ( errorCode = CopyOnWrite(targetptr, volptr))
	    {
	    	ViceLog(25, ("StoreData : CopyOnWrite failed\n"));
	    	volptr->partition->flags &= ~PART_DONTUPDATE;
		return (errorCode);
	    }
	    volptr->partition->flags &= ~PART_DONTUPDATE;
	    VSetPartitionDiskUsage(volptr->partition);
	    fdP = IH_OPEN(targetptr->handle);
	    if (fdP == NULL)  {
	    	ViceLog(25, ("StoreData : Reopen after CopyOnWrite failed\n"));
		return ENOENT;
	    }
	}
	tinode = VN_GET_INO(targetptr);
    }
    assert(VALID_INO(tinode));

    /* compute new file length */
    NewLength = DataLength;
    if (FileLength < NewLength)
	/* simulate truncate */
	NewLength = FileLength;	
    TruncatedLength = NewLength; /* remember length after possible ftruncate */
    if (Pos + Length > NewLength)
	NewLength = Pos+Length;   /* and write */

    /* adjust the disk block count by the difference in the files */
    adjustSize = (int) (nBlocks(NewLength) - nBlocks(targetptr->disk.length));
    if(errorCode = AdjustDiskUsage(volptr, adjustSize,
				   adjustSize - SpareComp(volptr))) {
	FDH_CLOSE(fdP);
	return(errorCode);
    }

    /* can signal cache manager to proceed from close now */
    /* this bit means that the locks are set and protections are OK */
    rx_SetLocalStatus(Call, 1);

    TM_GetTimeOfDay(&StartTime, 0);

    optSize = AFSV_BUFFERSIZE;

    /* truncate the file iff it needs it (ftruncate is slow even when its a noop) */
    if (FileLength < DataLength) FDH_TRUNC(fdP, FileLength);
    if (Pos > 0) FDH_SEEK(fdP, Pos, 0);
    bytesTransfered = 0;
#ifdef AFS_NT40_ENV
    tbuffer = AllocSendBuffer();
#endif /* AFS_NT40_ENV */
    /* if length == 0, the loop below isn't going to do anything, including
     * extend the length of the inode, which it must do, since the file system
     * assumes that the inode length == vnode's file length.  So, we extend
     * the file length manually if need be.  Note that if file is bigger than
     * Pos+(Length==0), we dont' have to do anything, and certainly shouldn't
     * do what we're going to do below.
     */
    if (Length == 0 && Pos > TruncatedLength) {
	/* Set the file's length; we've already done an lseek to the right
	 * spot above.
	 */
	errorCode = FDH_WRITE(fdP, &tlen, 1);
	if (errorCode != 1) goto done;
	errorCode = FDH_TRUNC(fdP, Pos);
    }
    else {
	/* have some data to copy */
#if FS_STATS_DETAILED
	(*a_bytesToStoreP) = Length;
#endif /* FS_STATS_DETAILED */
	while (1) {
	    if (bytesTransfered >= Length) {
		errorCode = 0;
		break;
	    }
	    tlen = Length -	bytesTransfered;    /* how much more to do */
	    if (tlen > optSize) tlen = optSize; /* bound by buffer size */
#ifdef AFS_NT40_ENV
	    errorCode = rx_Read(Call, tbuffer, tlen);
#else /* AFS_NT40_ENV */
	    errorCode = rx_Readv(Call, tiov, &tnio, RX_MAXIOVECS, tlen);
#endif /* AFS_NT40_ENV */
#if FS_STATS_DETAILED
	    (*a_bytesStoredP) += errorCode;
#endif /* FS_STATS_DETAILED */
	    if (errorCode <= 0) {
		errorCode = -32;
		break;
	    }
	    tlen = errorCode;
#ifdef AFS_NT40_ENV
	    errorCode = FDH_WRITE(fdP, tbuffer, tlen);
#else /* AFS_NT40_ENV */
	    errorCode = FDH_WRITEV(fdP, tiov, tnio);
#endif /* AFS_NT40_ENV */
	    if (errorCode != tlen) {
		errorCode = VDISKFULL;
		break;
	    }
	    bytesTransfered += tlen;
	}
    }
  done:
#ifdef AFS_NT40_ENV
    FreeSendBuffer((struct afs_buffer *) tbuffer);
#endif /* AFS_NT40_ENV */
    if (sync) {
      FDH_SYNC(fdP);
    }
    if (errorCode) {
	/* something went wrong: adjust size and return */
	targetptr->disk.length = FDH_SIZE(fdP); /* set new file size. */
	/* changed_newTime is tested in StoreData to detemine if we
	 * need to update the target vnode.
	 */
	targetptr->changed_newTime = 1;
	FDH_CLOSE(fdP);
	/* set disk usage to be correct */
	VAdjustDiskUsage(&errorCode, volptr,
			 (int)(nBlocks(targetptr->disk.length)
			       - nBlocks(NewLength)), 0);
	return errorCode;
    }
    FDH_CLOSE(fdP);

    TM_GetTimeOfDay(&StopTime, 0);

    targetptr->disk.length = NewLength;

    /* Update all StoreData related stats */
    FS_LOCK
    if (AFSCallStats.TotalStoredBytes >	2000000000)	/* reset if over 2 billion */
	AFSCallStats.TotalStoredBytes = AFSCallStats.AccumStoreTime = 0;
    AFSCallStats.StoreSize1++;			/* Piggybacked data */
    if (targetptr->disk.length < SIZE2)
	AFSCallStats.StoreSize2++;
    else if (targetptr->disk.length < SIZE3)
	AFSCallStats.StoreSize3++;
    else if (targetptr->disk.length < SIZE4)
	AFSCallStats.StoreSize4++;
    else
	AFSCallStats.StoreSize5++;
    FS_UNLOCK
    return(errorCode);

} /*StoreData_RXStyle*/


/*
 * Check if target file has the proper access permissions for the Fetch
 * (FetchData, FetchACL, FetchStatus) and Store (StoreData, StoreACL,
 * StoreStatus) related calls
 */
/* this code should probably just set a "priv" flag where all the audit events
 * are now, and only generate the audit event once at the end of the routine, 
 * thus only generating the event if all the checks succeed, but only because
 * of the privilege       XXX
 */
Check_PermissionRights(targetptr, client, rights, CallingRoutine, InStatus)
    Vnode *targetptr;
    struct client *client;
    afs_int32 rights;
    int CallingRoutine;
    AFSStoreStatus *InStatus;

{
    int errorCode = 0;
#define OWNSp(client, target) ((client)->ViceId == (target)->disk.owner)
#define CHOWN(i,t) (((i)->Mask & AFS_SETOWNER) &&((i)->Owner != (t)->disk.owner))
#define CHGRP(i,t) (((i)->Mask & AFS_SETGROUP) &&((i)->Group != (t)->disk.group))

    if (CallingRoutine & CHK_FETCH) {
#ifdef	CMUCS
	if (VanillaUser(client)) 
#else
	if (CallingRoutine == CHK_FETCHDATA || VanillaUser(client)) 
#endif
	  {
	    if (targetptr->disk.type == vDirectory || targetptr->disk.type == vSymlink) {
		if (   !(rights & PRSFS_LOOKUP)
#ifdef ADMIN_IMPLICIT_LOOKUP  
		    /* grant admins fetch on all directories */
		    && VanillaUser(client)
#endif /* ADMIN_IMPLICIT_LOOKUP */
		    && !OWNSp(client, targetptr)
		    && !acl_IsAMember(targetptr->disk.owner, &client->CPS)
		    && !VolumeOwner(client, targetptr))
		    return(EACCES);
	    } else {    /* file */
		/* must have read access, or be owner and have insert access */
		if (!(rights & PRSFS_READ) &&
		    !(OWNSp(client, targetptr) && (rights & PRSFS_INSERT)))
		    return(EACCES);
	    }
	    if (CallingRoutine == CHK_FETCHDATA && targetptr->disk.type == vFile)
#ifdef USE_GROUP_PERMS
		if (!OWNSp(client, targetptr) &&
		    !acl_IsAMember(targetptr->disk.owner, &client->CPS)) {
		    errorCode = (((GROUPREAD|GROUPEXEC) & targetptr->disk.modeBits)
				 ? 0: EACCES);
		} else {
		    errorCode =(((OWNERREAD|OWNEREXEC) & targetptr->disk.modeBits)
				? 0: EACCES);
		}
#else
		/*
		 * The check with the ownership below is a kludge to allow
		 * reading of files created with no read permission. The owner
		 * of the file is always allowed to read it.
		 */
		if ((client->ViceId != targetptr->disk.owner) && VanillaUser(client))
		    errorCode =(((OWNERREAD|OWNEREXEC) & targetptr->disk.modeBits) ? 0: EACCES);
#endif
	}
	else /*  !VanillaUser(client) && !FetchData */ {
	  osi_audit( PrivilegeEvent, 0, AUD_INT, (client ? client->ViceId : 0), 
                                        AUD_INT, CallingRoutine, AUD_END);
	}
    }
    else { /* a store operation */
      if ( (rights & PRSFS_INSERT) && OWNSp(client, targetptr)
	  && (CallingRoutine != CHK_STOREACL)
	  && (targetptr->disk.type == vFile))
	{
	  /* bypass protection checks on first store after a create
	   * for the creator; also prevent chowns during this time
	   * unless you are a system administrator */
	  /******  InStatus->Owner && UnixModeBits better be SET!! */
	  if ( CHOWN(InStatus, targetptr) || CHGRP(InStatus, targetptr)) {
	    if (VanillaUser (client)) 
	      return(EPERM);      /* Was EACCES */
	    else
	      osi_audit( PrivilegeEvent, 0, AUD_INT, (client ? client->ViceId : 0), 
                                            AUD_INT, CallingRoutine, AUD_END);
	  }
	} else {
	  if (CallingRoutine != CHK_STOREDATA && !VanillaUser(client)) {
	    osi_audit( PrivilegeEvent, 0, AUD_INT, (client ? client->ViceId : 0),
		                          AUD_INT, CallingRoutine, AUD_END);
	  }
	  else {
	    if (CallingRoutine == CHK_STOREACL) {
	      if (!(rights & PRSFS_ADMINISTER) &&
		  !OWNSp(client, targetptr) && 
		  !acl_IsAMember(targetptr->disk.owner, &client->CPS) &&
		  !VolumeOwner(client, targetptr)) return(EACCES);
	    }
	    else {	/* store data or status */
	      /* watch for chowns and chgrps */
	      if (CHOWN(InStatus, targetptr) || CHGRP(InStatus, targetptr))
		return(EPERM);      /* Was EACCES */
	      /* must be sysadmin to set suid/sgid bits */
	      if ((InStatus->Mask & AFS_SETMODE) &&
#ifdef AFS_NT40_ENV
		  (InStatus->UnixModeBits & 0xc00) != 0) {
#else
		  (InStatus->UnixModeBits & (S_ISUID|S_ISGID)) != 0) {
#endif
		if (VanillaUser(client))
		  return(EACCES);
		else osi_audit( PrivSetID, 0, AUD_INT, (client ? client->ViceId : 0),
			                      AUD_INT, CallingRoutine, AUD_END);
	      }
	      if (CallingRoutine == CHK_STOREDATA) {
		if (!(rights & PRSFS_WRITE))
		  return(EACCES);
		/* Next thing is tricky.  We want to prevent people
                 * from writing files sans 0200 bit, but we want
                 * creating new files with 0444 mode to work.  We
                 * don't check the 0200 bit in the "you are the owner"
                 * path above, but here we check the bit.  However, if
                 * you're a system administrator, we ignore the 0200
                 * bit anyway, since you may have fchowned the file,
                 * too */
#ifdef USE_GROUP_PERMS
			if ((targetptr->disk.type == vFile)
			    && VanillaUser(client)) {
			    if (!OWNSp(client, targetptr) &&
				!acl_IsAMember(targetptr->disk.owner,
					       &client->CPS)) {
				errorCode = ((GROUPWRITE & targetptr->disk.modeBits)
					     ? 0: EACCES);
			    } else {
				errorCode = ((OWNERWRITE & targetptr->disk.modeBits)
					     ? 0 : EACCES);
			    }
			} else
#endif
		if ((targetptr->disk.type != vDirectory)
		    && (!(targetptr->disk.modeBits & OWNERWRITE)))
		  if (VanillaUser(client))
		    return(EACCES);
		  else osi_audit( PrivilegeEvent, 0, AUD_INT, (client ? client->ViceId : 0),
				                     AUD_INT, CallingRoutine, AUD_END);
	      }
	      else {  /* a status store */
		if (targetptr->disk.type == vDirectory) {
		  if (!(rights & PRSFS_DELETE) && !(rights & PRSFS_INSERT))
		    return(EACCES);
		}
		else {	/* a file  or symlink */
		  if (!(rights & PRSFS_WRITE)) return(EACCES);
		}
	      }
	    }
	  }
	}
    }
    return(errorCode);

} /*Check_PermissionRights*/


/*
 * The Access List information is converted from its internal form in the
 * target's vnode buffer (or its parent vnode buffer if not a dir), to an
 * external form and returned back to the caller, via the AccessList
 * structure
 */
RXFetch_AccessList(targetptr, parentwhentargetnotdir, AccessList)
Vnode			* targetptr;
Vnode			* parentwhentargetnotdir;
struct AFSOpaque	* AccessList;
{
    char * eACL;	/* External access list placeholder */

    if (acl_Externalize((targetptr->disk.type == vDirectory ?
			 VVnodeACL(targetptr) :
			 VVnodeACL(parentwhentargetnotdir)), &eACL) != 0) {
	return EIO;
    }
    if ((strlen(eACL)+1) > AFSOPAQUEMAX) {
	acl_FreeExternalACL(&eACL);
	return(E2BIG);
    } else {
	strcpy((char *)(AccessList->AFSOpaque_val), (char *)eACL);
	AccessList->AFSOpaque_len = strlen(eACL) +1;
    }
    acl_FreeExternalACL(&eACL);
    return(0);

} /*RXFetch_AccessList*/


/*
 * The Access List information is converted from its external form in the
 * input AccessList structure to the internal representation and copied into
 * the target dir's vnode storage.
 */
RXStore_AccessList(targetptr, AccessList)
    Vnode *targetptr;
    struct AFSOpaque *AccessList;

{
    struct acl_accessList * newACL;	/* PlaceHolder for new access list */

    if (acl_Internalize(AccessList->AFSOpaque_val, &newACL) != 0)
	return(EINVAL);
    if ((newACL->size + 4) > VAclSize(targetptr))
	return(E2BIG);
    bcopy((char *) newACL,(char *) VVnodeACL(targetptr),(int)(newACL->size));
    acl_FreeACL(&newACL);
    return(0);

} /*RXStore_AccessList*/


Fetch_AccessList(targetptr, parentwhentargetnotdir, AccessList)
    Vnode *targetptr;
    Vnode *parentwhentargetnotdir;
    struct AFSAccessList *AccessList;

{
    char * eACL;	/* External access list placeholder */

    assert(acl_Externalize((targetptr->disk.type == vDirectory ?
			    VVnodeACL(targetptr) :
			    VVnodeACL(parentwhentargetnotdir)), &eACL) == 0);
    if ((strlen(eACL)+1) > AccessList->MaxSeqLen) {
	acl_FreeExternalACL(&eACL);
	return(E2BIG);
    } else {
	strcpy((char *)(AccessList->SeqBody), (char *)eACL);
	AccessList->SeqLen = strlen(eACL) +1;
    }
    acl_FreeExternalACL(&eACL);
    return(0);

} /*Fetch_AccessList*/


/*
 * The Access List information is converted from its external form in the
 * input AccessList structure to the internal representation and copied into
 * the target dir's vnode storage.
 */
Store_AccessList(targetptr, AccessList)
    Vnode *targetptr;
    struct AFSAccessList *AccessList;

{
    struct acl_accessList * newACL;	/* PlaceHolder for new access list */

    if (acl_Internalize(AccessList->SeqBody, &newACL) != 0)
	return(EINVAL);
    if ((newACL->size + 4) > VAclSize(targetptr))
	return(E2BIG);
    bcopy((char *) newACL,(char *) VVnodeACL(targetptr),(int)(newACL->size));
    acl_FreeACL(&newACL);
    return(0);

} /*Store_AccessList*/


/*
 * Common code to handle with removing the Name (file when it's called from
 * SAFS_RemoveFile() or an empty dir when called from SAFS_rmdir()) from a
 * given directory, parentptr.
 */
int DT1=0, DT0=0;
DeleteTarget(parentptr, volptr, targetptr, dir, fileFid, Name, ChkForDir)
    Vnode *parentptr;
    Volume *volptr;
    Vnode **targetptr;
    DirHandle *dir;
    AFSFid *fileFid;
    char *Name;
    int ChkForDir;
{
    DirHandle childdir;	    /* Handle for dir package I/O */
    int errorCode = 0;
    int code;

    /* watch for invalid names */
    if (!strcmp(Name, ".") || !strcmp(Name, ".."))
	return (EINVAL);
    if (parentptr->disk.cloned)	
    {
	ViceLog(25, ("DeleteTarget : CopyOnWrite called\n"));
	if ( errorCode = CopyOnWrite(parentptr, volptr))
	{
		ViceLog(20, ("DeleteTarget %s: CopyOnWrite failed %d\n",Name,errorCode));
		return errorCode;
	}
    }

    /* check that the file is in the directory */
    SetDirHandle(dir, parentptr);
    if (Lookup(dir, Name, fileFid))
	return(ENOENT);
    fileFid->Volume = V_id(volptr);

    /* just-in-case check for something causing deadlock */
    if (fileFid->Vnode == parentptr->vnodeNumber)
	return(EINVAL);

    *targetptr = VGetVnode(&errorCode, volptr, fileFid->Vnode, WRITE_LOCK);
    if (errorCode) {
	return (errorCode);
    }
    if (ChkForDir == MustBeDIR) {
	if ((*targetptr)->disk.type != vDirectory)
	    return(ENOTDIR);
    } else if ((*targetptr)->disk.type == vDirectory)
	return(EISDIR);
    
    /*assert((*targetptr)->disk.uniquifier == fileFid->Unique);*/
    /**
      * If the uniquifiers dont match then instead of asserting
      * take the volume offline and return VSALVAGE
      */
    if ( (*targetptr)->disk.uniquifier != fileFid->Unique ) {
	VTakeOffline(volptr);
	errorCode = VSALVAGE;
	return errorCode;
    }
	
    if (ChkForDir == MustBeDIR) {
	SetDirHandle(&childdir, *targetptr);
	if (IsEmpty(&childdir) != 0)
	    return(EEXIST);
	DZap(&childdir);
	(*targetptr)->delete = 1;
    } else if ((--(*targetptr)->disk.linkCount) == 0) 
	(*targetptr)->delete = 1;
    if ((*targetptr)->delete) {
	if (VN_GET_INO(*targetptr)) {
	    DT0++;
	    IH_REALLYCLOSE((*targetptr)->handle);
	    errorCode = IH_DEC(V_linkHandle(volptr),
			     VN_GET_INO(*targetptr),
			     V_parentId(volptr));
	    IH_RELEASE((*targetptr)->handle);
	    if (errorCode == -1) {
		ViceLog(0, ("DT: inode=%s, name=%s, errno=%d\n",
			    PrintInode(NULL, VN_GET_INO(*targetptr)),
			    Name, errno));
#ifdef	AFS_DEC_ENV
		if ((errno != ENOENT) && (errno != EIO) && (errno != ENXIO))
#else
		if (errno != ENOENT) 
#endif
		    {
		    ViceLog(0, ("Volume %u now offline, must be salvaged.\n", 
                                volptr->hashid));
                    VTakeOffline(volptr);
		    return (EIO);
		    }
		DT1++;
		errorCode = 0;
	    }
	}
	VN_SET_INO(*targetptr, (Inode)0);
	VAdjustDiskUsage(&errorCode, volptr,
			-(int)nBlocks((*targetptr)->disk.length), 0);
    }
    
    (*targetptr)->changed_newTime = 1; /* Status change of deleted file/dir */

    code = Delete(dir,(char *) Name);
    if (code) {
       ViceLog(0, ("Error %d deleting %s\n", code,
		   (((*targetptr)->disk.type== Directory)?"directory":"file")));
       ViceLog(0, ("Volume %u now offline, must be salvaged.\n", 
                  volptr->hashid));
       VTakeOffline(volptr);
       if (!errorCode) errorCode = code;
    }

    DFlush();
    return(errorCode);

} /*DeleteTarget*/


/*
 * This routine updates the parent directory's status block after the
 * specified operation (i.e. RemoveFile(), CreateFile(), Rename(),
 * SymLink(), Link(), MakeDir(), RemoveDir()) on one of its children has
 * been performed.
 */
#if FS_STATS_DETAILED
Update_ParentVnodeStatus(parentptr, volptr, dir, author, linkcount, a_inSameNetwork)
#else
Update_ParentVnodeStatus(parentptr, volptr, dir, author, linkcount)
#endif /* FS_STATS_DETAILED */
    Vnode *parentptr;
    Volume *volptr;
    DirHandle *dir;
    int author;
    int linkcount;
#if FS_STATS_DETAILED
    char a_inSameNetwork;	/*Client in the same net as File Server?*/
#endif /* FS_STATS_DETAILED */

{
    int	newlength;	    /* Holds new directory length */
    int errorCode;
#if FS_STATS_DETAILED
    Date currDate;		/*Current date*/
    int writeIdx;		/*Write index to bump*/
    int timeIdx;		/*Authorship time index to bump*/
#endif /* FS_STATS_DETAILED */

    parentptr->disk.dataVersion++;
    newlength = Length(dir);
    /* 
     * This is a called on both dir removals (i.e. remove, removedir, rename) but also in dir additions
     * (create, symlink, link, makedir) so we need to check if we have enough space
     * XXX But we still don't check the error since we're dealing with dirs here and really the increase
     * of a new entry would be too tiny to worry about failures (since we have all the existing cushion)
     */
    if (nBlocks(newlength) != nBlocks(parentptr->disk.length))
	VAdjustDiskUsage(&errorCode, volptr, 
			 (int)(nBlocks(newlength) - nBlocks(parentptr->disk.length)),
			 (int)(nBlocks(newlength) - nBlocks(parentptr->disk.length)));
    parentptr->disk.length = newlength;

#if FS_STATS_DETAILED
    /*
     * Update directory write stats for this volume.  Note that the auth
     * counter is located immediately after its associated ``distance''
     * counter.
     */
    if (a_inSameNetwork)
	writeIdx = VOL_STATS_SAME_NET;
    else
	writeIdx = VOL_STATS_DIFF_NET;
    V_stat_writes(volptr, writeIdx)++;
    if (author != AnonymousID) {
	V_stat_writes(volptr, writeIdx+1)++;
    }

    /*
     * Update the volume's authorship information in response to this
     * directory operation.  Get the current time, decide to which time
     * slot this operation belongs, and bump the appropriate slot.
     */
    currDate = (FT_ApproxTime() - parentptr->disk.unixModifyTime);
    timeIdx = (currDate < VOL_STATS_TIME_CAP_0 ? VOL_STATS_TIME_IDX_0 :
	       currDate < VOL_STATS_TIME_CAP_1 ? VOL_STATS_TIME_IDX_1 :
	       currDate < VOL_STATS_TIME_CAP_2 ? VOL_STATS_TIME_IDX_2 :
	       currDate < VOL_STATS_TIME_CAP_3 ? VOL_STATS_TIME_IDX_3 :
	       currDate < VOL_STATS_TIME_CAP_4 ? VOL_STATS_TIME_IDX_4 :
	       VOL_STATS_TIME_IDX_5);
    if (parentptr->disk.author == author) {
	V_stat_dirSameAuthor(volptr, timeIdx)++;
    }
    else {
	V_stat_dirDiffAuthor(volptr, timeIdx)++;
    }
#endif /* FS_STATS_DETAILED */

    parentptr->disk.author = author;
    parentptr->disk.linkCount = linkcount;
    parentptr->disk.unixModifyTime = FT_ApproxTime();	/* This should be set from CLIENT!! */
    parentptr->disk.serverModifyTime = FT_ApproxTime();
    parentptr->changed_newTime = 1; /* vnode changed, write it back. */
}


/*
 * Update the target file's (or dir's) status block after the specified
 * operation is complete. Note that some other fields maybe updated by
 * the individual module.
 */

/* INCOMPLETE - More attention is needed here! */

Update_TargetVnodeStatus(targetptr, Caller, client, InStatus, parentptr, volptr,
			 length)
    Vnode *targetptr;
    afs_uint32 Caller;
    struct client *client;
    AFSStoreStatus *InStatus;
    Vnode *parentptr;
    Volume *volptr;
    afs_int32 length;

{
#if FS_STATS_DETAILED
    Date currDate;		/*Current date*/
    int writeIdx;		/*Write index to bump*/
    int timeIdx;		/*Authorship time index to bump*/
#endif /* FS_STATS_DETAILED */

    if (Caller & (TVS_CFILE|TVS_SLINK|TVS_MKDIR))	{   /* initialize new file */
	targetptr->disk.parent = parentptr->vnodeNumber;
	targetptr->disk.length = length;
	/* targetptr->disk.group =	0;  save some cycles */
	targetptr->disk.modeBits = 0777;
	targetptr->disk.owner = client->ViceId;
	targetptr->disk.dataVersion =  0 ; /* consistent with the client */
	targetptr->disk.linkCount = (Caller & TVS_MKDIR ? 2 : 1);
	/* the inode was created in Alloc_NewVnode() */
    }

#if FS_STATS_DETAILED
    /*
     * Update file write stats for this volume.  Note that the auth
     * counter is located immediately after its associated ``distance''
     * counter.
     */
    if (client->InSameNetwork)
	writeIdx = VOL_STATS_SAME_NET;
    else
	writeIdx = VOL_STATS_DIFF_NET;
    V_stat_writes(volptr, writeIdx)++;
    if (client->ViceId != AnonymousID) {
	V_stat_writes(volptr, writeIdx+1)++;
    }

    /*
     * We only count operations that DON'T involve creating new objects
     * (files, symlinks, directories) or simply setting status as
     * authorship-change operations.
     */
    if (!(Caller & (TVS_CFILE | TVS_SLINK | TVS_MKDIR | TVS_SSTATUS))) {
	/*
	 * Update the volume's authorship information in response to this
	 * file operation.  Get the current time, decide to which time
	 * slot this operation belongs, and bump the appropriate slot.
	 */
	currDate = (FT_ApproxTime() - targetptr->disk.unixModifyTime);
	timeIdx = (currDate < VOL_STATS_TIME_CAP_0 ? VOL_STATS_TIME_IDX_0 :
		   currDate < VOL_STATS_TIME_CAP_1 ? VOL_STATS_TIME_IDX_1 :
		   currDate < VOL_STATS_TIME_CAP_2 ? VOL_STATS_TIME_IDX_2 :
		   currDate < VOL_STATS_TIME_CAP_3 ? VOL_STATS_TIME_IDX_3 :
		   currDate < VOL_STATS_TIME_CAP_4 ? VOL_STATS_TIME_IDX_4 :
		   VOL_STATS_TIME_IDX_5);
	if (targetptr->disk.author == client->ViceId) {
	    V_stat_fileSameAuthor(volptr, timeIdx)++;
	} else {
	    V_stat_fileDiffAuthor(volptr, timeIdx)++;
	}
      }
#endif /* FS_STATS_DETAILED */

    if (!(Caller & TVS_SSTATUS))
      targetptr->disk.author = client->ViceId;
    if (Caller & TVS_SDATA) {
      targetptr->disk.dataVersion++;
      if (VanillaUser(client))
	  targetptr->disk.modeBits &= ~04000; /* turn off suid for file. */
    }
    if (Caller & TVS_SSTATUS) {	/* update time on non-status change */
	/* store status, must explicitly request to change the date */
	if (InStatus->Mask & AFS_SETMODTIME)
	    targetptr->disk.unixModifyTime = InStatus->ClientModTime;
    }
    else {/* other: date always changes, but perhaps to what is specified by caller */
	targetptr->disk.unixModifyTime = (InStatus->Mask & AFS_SETMODTIME ? InStatus->ClientModTime : FT_ApproxTime());
    }
    if (InStatus->Mask & AFS_SETOWNER) {
	/* admin is allowed to do chmod, chown as well as chown, chmod. */
	if (VanillaUser(client))
	    targetptr->disk.modeBits &= ~04000; /* turn off suid for file. */
	targetptr->disk.owner = InStatus->Owner;
	if (VolumeRootVnode (targetptr)) {
	    Error errorCode = 0;	/* what should be done with this? */

	    V_owner(targetptr->volumePtr) = InStatus->Owner;
	    VUpdateVolume(&errorCode, targetptr->volumePtr);
	}
    }
    if (InStatus->Mask & AFS_SETMODE) {
	int modebits = InStatus->UnixModeBits;
#define	CREATE_SGUID_ADMIN_ONLY 1
#ifdef CREATE_SGUID_ADMIN_ONLY
	if (VanillaUser(client)) 
	    modebits = modebits & 0777;
#endif
	if (VanillaUser(client)) {
	  targetptr->disk.modeBits = modebits;
	}
	else {
	  targetptr->disk.modeBits = modebits;
	  switch ( Caller ) {
	  case TVS_SDATA: osi_audit( PrivSetID, 0, AUD_INT, client->ViceId,
				    AUD_INT, CHK_STOREDATA, AUD_END); break;
	  case TVS_CFILE:
	  case TVS_SSTATUS: osi_audit( PrivSetID, 0, AUD_INT, client->ViceId,
				    AUD_INT, CHK_STORESTATUS, AUD_END); break;
	  default: break;
	  }
	}
      }
    targetptr->disk.serverModifyTime = FT_ApproxTime();
    if (InStatus->Mask & AFS_SETGROUP)
	targetptr->disk.group = InStatus->Group;
    /* vnode changed : to be written back by VPutVnode */
    targetptr->changed_newTime = 1;

} /*Update_TargetVnodeStatus*/


/*
 * Fills the CallBack structure with the expiration time and type of callback
 * structure. Warning: this function is currently incomplete.
 */
SetCallBackStruct(CallBackTime, CallBack)
    afs_uint32 CallBackTime;
    struct AFSCallBack *CallBack;

{
    /* CallBackTime could not be 0 */
    if (CallBackTime == 0) {
	ViceLog(0, ("WARNING: CallBackTime == 0!\n"));
	CallBack->ExpirationTime = 0;
    } else
	CallBack->ExpirationTime = CallBackTime - FT_ApproxTime();	
    CallBack->CallBackVersion =	CALLBACK_VERSION;
    CallBack->CallBackType = CB_SHARED;		    /* The default for now */

} /*SetCallBackStruct*/


/*
 * Returns the volume and vnode pointers associated with file Fid; the lock
 * type on the vnode is set to lock. Note that both volume/vnode's ref counts
 * are incremented and they must be eventualy released.
 */
CheckVnode(fid, volptr, vptr, lock)
    AFSFid *fid;
    Volume **volptr;
    Vnode **vptr;
    int lock;

{
    int fileCode = 0;
    int errorCode = -1;
    static struct timeval restartedat = {0,0};

    if (fid->Volume == 0 || fid->Vnode == 0) /* not: || fid->Unique == 0) */
	return(EINVAL);
    if ((*volptr) == 0) {
      extern int VInit;

      while(1) {
	errorCode = 0;
	*volptr = VGetVolume(&errorCode, (afs_int32)fid->Volume);
	if (!errorCode) {
	  assert (*volptr);
	  break;
	}
	if ((errorCode == VOFFLINE) && (VInit < 2)) {
	    /* The volume we want may not be attached yet because
	     * the volume initialization is not yet complete.
             * We can do several things: 
	     *     1.  return -1, which will cause users to see
	     *         "connection timed out".  This is more or
	     *         less the same as always, except that the servers
	     *         may appear to bounce up and down while they
	     *         are actually restarting.
	     *     2.  return VBUSY which will cause clients to 
	     *         sleep and retry for 6.5 - 15 minutes, depending
	     *         on what version of the CM they are running.  If
	     *         the file server takes longer than that interval 
	     *         to attach the desired volume, then the application
	     *         will see an ENODEV or EIO.  This approach has 
	     *         the advantage that volumes which have been attached
	     *         are immediately available, it keeps the server's
	     *         immediate backlog low, and the call is interruptible
	     *         by the user.  Users see "waiting for busy volume."
	     *     3.  sleep here and retry.  Some people like this approach
	     *         because there is no danger of seeing errors.  However, 
	     *         this approach only works with a bounded number of 
	     *         clients, since the pending queues will grow without
	     *         stopping.  It might be better to find a way to take
	     *         this call and stick it back on a queue in order to
	     *         recycle this thread for a different request.    
	     *     4.  Return a new error code, which new cache managers will
	     *         know enough to interpret as "sleep and retry", without
	     *         the upper bound of 6-15 minutes that is imposed by the
	     *         VBUSY handling.  Users will see "waiting for
	     *         busy volume," so they know that something is
	     *         happening.  Old cache managers must be able to do  
	     *         something reasonable with this, for instance, mark the
	     *         server down.  Fortunately, any error code < 0
	     *         will elicit that behavior. See #1.
	     *     5.  Some combination of the above.  I like doing #2 for 10
	     *         minutes, followed by #4.  3.1b and 3.2 cache managers
	     *         will be fine as long as the restart period is
	     *         not longer than 6.5 minutes, otherwise they may
	     *         return ENODEV to users.  3.3 cache managers will be
	     *         fine for 10 minutes, then will return
	     *         ETIMEDOUT.  3.4 cache managers will just wait
	     *         until the call works or fails definitively.
	     *  NB. The problem with 2,3,4,5 is that old clients won't
	     *  fail over to an alternate read-only replica while this
	     *  server is restarting.  3.4 clients will fail over right away.
	     */
	  if (restartedat.tv_sec == 0) {
	    /* I'm not really worried about when we restarted, I'm   */
	    /* just worried about when the first VBUSY was returned. */
	    TM_GetTimeOfDay(&restartedat, 0);
	    return(VBUSY);
	  }
	  else {
	    struct timeval now;
	    TM_GetTimeOfDay(&now, 0);
	    if ((now.tv_sec - restartedat.tv_sec) < (11*60)) {
	      return(VBUSY);
	    }
	    else {
	      return (VRESTARTING);
	    }
	  }
	}
	  /* allow read operations on busy volume */
	else if(errorCode==VBUSY && lock==READ_LOCK) {
	  errorCode=0;
	  break;
	}
	else if (errorCode)
	  return(errorCode);
      }
    }
    assert (*volptr);

    /* get the vnode  */
    *vptr = VGetVnode(&errorCode, *volptr, fid->Vnode, lock);
    if (errorCode)
	return(errorCode);
    if ((*vptr)->disk.uniquifier != fid->Unique) {
	VPutVnode(&fileCode, *vptr);
	assert(fileCode == 0);
	*vptr = 0;
	return(VNOVNODE);   /* return the right error code, at least */
    }
    return(0);

} /*CheckVnode*/


/*
 * This routine returns the ACL associated with the targetptr. If the
 * targetptr isn't a directory, we access its parent dir and get the ACL
 * thru the parent; in such case the parent's vnode is returned in
 * READ_LOCK mode.
 */
SetAccessList(targetptr, volume, ACL, ACLSize, parent, Fid, Lock)
    Vnode **targetptr;		 /*Target vnode pointer; returned locked*/
    Volume **volume;		 /*Volume ptr associated with targetptr*/
    struct acl_accessList **ACL; /*The returned ACL for the vnode*/
    int * ACLSize;		 /*Returned ACL's size*/
    Vnode **parent;		 /*If target not Dir, it's its locked parent*/
    AFSFid *Fid;		 /*Fid associated with targetptr*/
    int Lock;			 /*Lock type to be applied to targetptr*/

{
    if ((*targetptr)->disk.type == vDirectory) {
	*parent = 0;
	*ACL = VVnodeACL(*targetptr);
	*ACLSize = VAclSize(*targetptr);
	return(0);
    }
    else {
	assert(Fid != 0);
	while(1) {
	    VnodeId parentvnode;
	    int errorCode = 0;
	    
	    parentvnode = (*targetptr)->disk.parent;
	    VPutVnode(&errorCode,*targetptr);
	    *targetptr = 0;
	    if (errorCode) return(errorCode);
	    *parent = VGetVnode(&errorCode, *volume, parentvnode, READ_LOCK);
	    if (errorCode) return(errorCode);
	    *ACL = VVnodeACL(*parent);
	    *ACLSize = VAclSize(*parent);
	    if ((errorCode = CheckVnode(Fid, volume, targetptr, Lock)) != 0)
		return(errorCode);
	    if ((*targetptr)->disk.parent != parentvnode) {
		VPutVnode(&errorCode, *parent);
		*parent = 0;
		if (errorCode) return(errorCode);
	    } else
		return(0);
	}
    }

} /*SetAccessList*/


/*
 * Common code that handles the creation of a new file (SAFS_CreateFile and
 * SAFS_Symlink) or a new dir (SAFS_MakeDir)
 */
Alloc_NewVnode(parentptr, dir, volptr, targetptr, Name, OutFid, FileType, BlocksPreallocatedForVnode)
    Vnode *parentptr;
    DirHandle *dir;
    Volume *volptr;
    Vnode **targetptr;
    char *Name;
    struct AFSFid *OutFid;
    int FileType;
    int BlocksPreallocatedForVnode;

{
    int	errorCode = 0;		/* Error code returned back */
    int temp;
    Inode inode=0;
    Inode nearInode;		/* hint for inode allocation in solaris */

    if (errorCode = AdjustDiskUsage(volptr, BlocksPreallocatedForVnode,
				    BlocksPreallocatedForVnode)) {
	ViceLog(25, ("Insufficient space to allocate %d blocks\n", 
		     BlocksPreallocatedForVnode));
	return(errorCode);
    }

    *targetptr = VAllocVnode(&errorCode, volptr, FileType);
    if (errorCode != 0) {
	VAdjustDiskUsage(&temp, volptr, - BlocksPreallocatedForVnode, 0);
	return(errorCode);
    }
    OutFid->Volume = V_id(volptr);
    OutFid->Vnode = (*targetptr)->vnodeNumber;
    OutFid->Unique = (*targetptr)->disk.uniquifier;

    nearInode = VN_GET_INO(parentptr);   /* parent is also in same vol */

    /* create the inode now itself */
    inode = IH_CREATE(V_linkHandle(volptr), V_device(volptr),
		      VPartitionPath(V_partition(volptr)), nearInode,
		      V_id(volptr), (*targetptr)->vnodeNumber,
		      (*targetptr)->disk.uniquifier, 1);

    /* error in creating inode */
    if (!VALID_INO(inode)) 
    {
               ViceLog(0, ("Volume : %d vnode = %d Failed to create inode: errno = %d\n", 
                         (*targetptr)->volumePtr->header->diskstuff.id,
                         (*targetptr)->vnodeNumber, 
                         errno));
		VAdjustDiskUsage(&temp, volptr, -BlocksPreallocatedForVnode,0);
		(*targetptr)->delete = 1; /* delete vnode */
		return ENOSPC;
    }
    VN_SET_INO(*targetptr, inode);
    IH_INIT(((*targetptr)->handle), V_device(volptr), V_id(volptr), inode);

    /* copy group from parent dir */
    (*targetptr)->disk.group = parentptr->disk.group;

    if (parentptr->disk.cloned)	
    {
	ViceLog(25, ("Alloc_NewVnode : CopyOnWrite called\n"));
	if ( errorCode = CopyOnWrite(parentptr, volptr))  /* disk full */
	{
		ViceLog(25, ("Alloc_NewVnode : CopyOnWrite failed\n"));
		/* delete the vnode previously allocated */
		(*targetptr)->delete = 1;
		VAdjustDiskUsage(&temp, volptr,
				 -BlocksPreallocatedForVnode, 0);
		IH_REALLYCLOSE((*targetptr)->handle);
		if ( IH_DEC(V_linkHandle(volptr), inode, V_parentId(volptr)) )
		    ViceLog(0,("Alloc_NewVnode: partition %s idec %s failed\n",
				volptr->partition->name,
			       PrintInode(NULL, inode)));
		IH_RELEASE((*targetptr)->handle);
			
		return errorCode;
	}
    }
    
    /* add the name to the directory */
    SetDirHandle(dir, parentptr);
    if (errorCode = Create(dir,(char *)Name, OutFid)) {
	(*targetptr)->delete = 1;
	VAdjustDiskUsage(&temp, volptr, - BlocksPreallocatedForVnode, 0);
	IH_REALLYCLOSE((*targetptr)->handle);
	if ( IH_DEC(V_linkHandle(volptr), inode, V_parentId(volptr)))
	    ViceLog(0,("Alloc_NewVnode: partition %s idec %s failed\n",
		       volptr->partition->name,
		       PrintInode(NULL, inode)));
	IH_RELEASE((*targetptr)->handle);
	return(errorCode);
    }
    DFlush();
    return(0);

} /*Alloc_NewVnode*/


/*
 * Handle all the lock-related code (SAFS_SetLock, SAFS_ExtendLock and
 * SAFS_ReleaseLock)
 */
HandleLocking(targetptr, rights, LockingType)
    Vnode *targetptr;
    afs_int32 rights;
    ViceLockType LockingType;

{
    int	Time;		/* Used for time */
    int writeVnode = targetptr->changed_oldTime; /* save original status */

    /* Does the caller has Lock priviledges; root extends locks, however */
    if (LockingType != LockExtend && !(rights & PRSFS_LOCK))
	return(EACCES);
    targetptr->changed_oldTime = 1; /* locking doesn't affect any time stamp */
    Time = FT_ApproxTime();
    switch (LockingType) {
	case LockRead:
	case LockWrite:
	    if (Time > targetptr->disk.lock.lockTime)
		targetptr->disk.lock.lockTime = targetptr->disk.lock.lockCount = 0;
	    Time += AFS_LOCKWAIT;
	    if (LockingType == LockRead) {
		if (targetptr->disk.lock.lockCount >= 0) {
		    ++(targetptr->disk.lock.lockCount);
		    targetptr->disk.lock.lockTime = Time;
		} else return(EAGAIN);
	    } else {
		if (targetptr->disk.lock.lockCount == 0) {
		    targetptr->disk.lock.lockCount = -1;
		    targetptr->disk.lock.lockTime = Time;
		} else return(EAGAIN);
	    }
	    break;
	case LockExtend:
	    Time += AFS_LOCKWAIT;
	    if (targetptr->disk.lock.lockCount != 0)
		targetptr->disk.lock.lockTime = Time;
	    else return(EINVAL);	    
	    break;
	case LockRelease:
	    if ((--targetptr->disk.lock.lockCount) <= 0)
		targetptr->disk.lock.lockCount = targetptr->disk.lock.lockTime = 0;
	    break;
	default:
	    targetptr->changed_oldTime = writeVnode; /* restore old status */
	    ViceLog(0, ("Illegal Locking type %d\n", LockingType));
    }
    return(0);

} /*HandleLocking*/


/*
 * This routine returns the status info associated with the targetptr vnode
 * in the AFSFetchStatus structure.  Some of the newer fields, such as
 * SegSize and Group are not yet implemented
 */
void GetStatus(targetptr, status, rights, anyrights, parentptr)
    Vnode *targetptr;		/*vnode of desired Fid*/
    AFSFetchStatus *status;	/*the status info is returned here*/
    afs_int32 rights;		/*Sets the 'CallerAccess' status field*/
    afs_int32 anyrights;		/*Sets the 'AnonymousAccess' status field*/
    Vnode *parentptr;		/*target's parent vnode*/

{
    /* initialize return status from a vnode  */
    status->InterfaceVersion = 1;
    status->SyncCounter = status->dataVersionHigh = status->spare2 =
    status->spare3 = status->spare4 = 0;
    if (targetptr->disk.type == vFile)
	status->FileType = File;
    else if (targetptr->disk.type == vDirectory)
	status->FileType = Directory;
    else if (targetptr->disk.type == vSymlink)
	status->FileType = SymbolicLink;
    else
	status->FileType = Invalid;			/*invalid type field */
    status->LinkCount = targetptr->disk.linkCount;
    status->Length = targetptr->disk.length;
    status->DataVersion = targetptr->disk.dataVersion;
    status->Author = targetptr->disk.author;
    status->Owner = targetptr->disk.owner;
    status->CallerAccess = rights;
    status->AnonymousAccess = anyrights;
    status->UnixModeBits = targetptr->disk.modeBits;
    status->ClientModTime = targetptr->disk.unixModifyTime;	/* This might need rework */
    status->ParentVnode = (status->FileType == Directory ? targetptr->vnodeNumber : parentptr->vnodeNumber);
    status->ParentUnique = (status->FileType == Directory ? targetptr->disk.uniquifier : parentptr->disk.uniquifier);
    status->SegSize = 0;
    status->ServerModTime = targetptr->disk.serverModifyTime;			
    status->Group = targetptr->disk.group;
    status->spare2 = targetptr->disk.lock.lockCount;

} /*GetStatus*/


/*
 * Compare the directory's ACL with the user's access rights in the client
 * connection and return the user's and everybody else's access permissions
 * in rights and anyrights, respectively
 */
GetRights (client, ACL, rights, anyrights)
    struct client *client;	/* Client struct */
    struct acl_accessList *ACL;	/* Access List for the current directory */
    afs_int32 *rights;		/* Returns access rights for caller */
    afs_int32 *anyrights;		/* Returns access rights for 'anyuser' */

{
    extern prlist SystemAnyUserCPS;
    afs_int32 hrights = 0;
    int code;

    if (acl_CheckRights(ACL, &SystemAnyUserCPS, anyrights) != 0) {

	ViceLog(0,("CheckRights failed\n"));
	*anyrights = 0;
    }
    *rights = 0;
    acl_CheckRights(ACL, &client->CPS, rights);

        /* wait if somebody else is already doing the getCPS call */
    H_LOCK
    while ( client->host->hostFlags & HPCS_INPROGRESS )
    {
	client->host->hostFlags |= HPCS_WAITING;  /* I am waiting */
#ifdef AFS_PTHREAD_ENV
	pthread_cond_wait(&client->host->cond, &host_glock_mutex);
#else /* AFS_PTHREAD_ENV */
        if ((code=LWP_WaitProcess( &(client->host->hostFlags))) !=LWP_SUCCESS)
                ViceLog(0, ("LWP_WaitProcess returned %d\n", code));
#endif /* AFS_PTHREAD_ENV */
    }

    if (client->host->hcps.prlist_len && !client->host->hcps.prlist_val) {
	ViceLog(0,("CheckRights: len=%d, for host=0x%x\n", client->host->hcps.prlist_len, client->host->host));
    } else
	acl_CheckRights(ACL, &client->host->hcps, &hrights);
    H_UNLOCK
    /* Allow system:admin the rights given with the -implicit option */
    if (acl_IsAMember(SystemId, &client->CPS))
        *rights |= implicitAdminRights;
    *rights |= hrights;
    *anyrights |= hrights;

    return(0);

} /*GetRights*/


/* Checks if caller has the proper AFS and Unix (WRITE) access permission to the target directory; Prfs_Mode refers to the AFS Mode operation while rights contains the caller's access permissions to the directory. */

CheckWriteMode(targetptr, rights, Prfs_Mode)
Vnode	    * targetptr;
afs_int32	    rights;
int	    Prfs_Mode;
{
    if (!(rights & Prfs_Mode))
	return(EACCES);
    if ((targetptr->disk.type != vDirectory) && (!(targetptr->disk.modeBits & OWNERWRITE)))
	return(EACCES);
    return(0);
}
	

/* In our current implementation, each successive data store (new file
 * data version) creates a new inode. This function creates the new
 * inode, copies the old inode's contents to the new one, remove the old
 * inode (i.e. decrement inode count -- if it's currently used the delete
 * will be delayed), and modify some fields (i.e. vnode's
 * disk.inodeNumber and cloned)
 */
#define	COPYBUFFSIZE	8192
int CopyOnWrite(targetptr, volptr)
    Vnode *targetptr;
    Volume *volptr;

{
    Inode	ino, nearInode;
    int		rdlen;
    int		wrlen;
    register int size, length;
    int ifd, ofd;
    char	*buff;
    int 	rc;		/* return code */
    IHandle_t	*newH;	/* Use until finished copying, then cp to vnode.*/
    FdHandle_t  *targFdP;  /* Source Inode file handle */
    FdHandle_t  *newFdP; /* Dest Inode file handle */

    if (targetptr->disk.type ==	vDirectory) DFlush();	/* just in case? */

    size = targetptr->disk.length;
    buff = (char *)malloc(COPYBUFFSIZE);
    if (buff == NULL) {
	return EIO;
    }

    ino = VN_GET_INO(targetptr);
    assert(VALID_INO(ino));
    targFdP = IH_OPEN(targetptr->handle);
    if (targFdP == NULL) {
	rc = errno;
	ViceLog(0, ("CopyOnWrite failed: Failed to open target vnode %u in volume %u (errno = %d)\n", targetptr->vnodeNumber, V_id(volptr), rc));
	free(buff);
	VTakeOffline (volptr);
	return rc;
    }

    nearInode = VN_GET_INO(targetptr);
    ino	= IH_CREATE(V_linkHandle(volptr), V_device(volptr),
		    VPartitionPath(V_partition(volptr)),nearInode, V_id(volptr),
		    targetptr->vnodeNumber, targetptr->disk.uniquifier,
		    (int)targetptr->disk.dataVersion);
    if (!VALID_INO(ino))
    {
	ViceLog(0,("CopyOnWrite failed: Partition %s that contains volume %u may be out of free inodes(errno = %d)\n", volptr->partition->name, V_id(volptr), errno));
	FDH_CLOSE(targFdP);
	free(buff);
	return ENOSPC;
    }
    IH_INIT(newH, V_device(volptr), V_id(volptr), ino);
    newFdP = IH_OPEN(newH);
    assert(newFdP != NULL);

    while(size > 0) {
	if (size > COPYBUFFSIZE) { /* more than a buffer */
	    length = COPYBUFFSIZE;
	    size -= COPYBUFFSIZE;
	} else {
	    length = size;
	    size = 0;
	}
	rdlen = FDH_READ(targFdP, buff, length); 
	if (rdlen == length)
	    wrlen = FDH_WRITE(newFdP, buff, length);
	else
	    wrlen = 0;
	/*  Callers of this function are not prepared to recover
	 *  from error that put the filesystem in an inconsistent
	 *  state. Make sure that we force the volume off-line if
	 *  we some error other than ENOSPC - 4.29.99)
	 *
	 *  In case we are unable to write the required bytes, and the
	 *  error code indicates that the disk is full, we roll-back to
	 *  the initial state.
	 */
	if((rdlen != length) || (wrlen != length))
		if ( (wrlen < 0) && (errno == ENOSPC) )	/* disk full */
		{
	    		ViceLog(0,("CopyOnWrite failed: Partition %s containing volume %u is full\n",
		    			volptr->partition->name, V_id(volptr)));
			/* remove destination inode which was partially copied till now*/
			FDH_REALLYCLOSE(newFdP);
			IH_RELEASE(newH);
			FDH_REALLYCLOSE(targFdP);
			rc = IH_DEC(V_linkHandle(volptr), ino,
				  V_parentId(volptr));
    			if (!rc ) {
		    	    ViceLog(0,("CopyOnWrite failed: volume %u in partition %s needs salvage\n",
			    	    V_id(volptr), volptr->partition->name));
			    VTakeOffline (volptr);
			}
			free(buff);
			return ENOSPC;
		}
		else {
	    	    ViceLog(0,("CopyOnWrite failed: volume %u in partition %s needs salvage\n",
		    	    V_id(volptr), volptr->partition->name));
		    /* Decrement this inode so salvager doesn't find it. */
		    FDH_REALLYCLOSE(newFdP);
		    IH_RELEASE(newH);
		    FDH_REALLYCLOSE(targFdP);
		    rc = IH_DEC(V_linkHandle(volptr), ino,
				V_parentId(volptr));
		    free(buff);
		    VTakeOffline (volptr);
		    return EIO;
		}
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */
    }
    FDH_REALLYCLOSE(targFdP);
    rc = IH_DEC(V_linkHandle(volptr), VN_GET_INO(targetptr),
	      V_parentId(volptr)) ;
    assert(!rc);
    IH_RELEASE(targetptr->handle);

    rc = FDH_SYNC(newFdP);
    assert(rc == 0);
    FDH_CLOSE(newFdP);
    targetptr->handle = newH;
    VN_SET_INO(targetptr, ino);
    targetptr->disk.cloned = 0;
    /* Internal change to vnode, no user level change to volume - def 5445 */
    targetptr->changed_oldTime = 1;
    free(buff);
    return 0;				/* success */
} /*CopyOnWrite*/


/*
 * VanillaUser returns 1 (true) if the user is a vanilla user (i.e., not
 * a System:Administrator)
 */
VanillaUser(client)
    struct client *client;

{
    if (acl_IsAMember(SystemId, &client->CPS))
	return(0);  /* not a system administrator, then you're "vanilla" */
    return(1);

} /*VanillaUser*/


/*
 * Adjusts (Subtract) "length" number of blocks from the volume's disk
 * allocation; if some error occured (exceeded volume quota or partition
 * was full, or whatever), it frees the space back and returns the code.
 * We usually pre-adjust the volume space to make sure that there's
 * enough space before consuming some.
 */
AdjustDiskUsage(volptr, length, checkLength)
    Volume	* volptr;
    afs_int32	checkLength;
    afs_int32	length;

{
    int rc;
    int nc;

    VAdjustDiskUsage(&rc, volptr, length, checkLength);
    if (rc) {
	VAdjustDiskUsage(&nc, volptr, -length, 0);
	if (rc == VOVERQUOTA) {
	    ViceLog(2,("Volume %u (%s) is full\n",
		    V_id(volptr), V_name(volptr)));
	    return(rc);
	}
	if (rc == VDISKFULL) {
	    ViceLog(0,("Partition %s that contains volume %u is full\n",
		    volptr->partition->name, V_id(volptr)));
	    return(rc);
	}
	ViceLog(0,("Got error return %d from VAdjustDiskUsage\n",rc));
	return(rc);
    }
    return(0);

} /*AdjustDiskUsage*/


/*
 * If some flags (i.e. min or max quota) are set, the volume's in disk
 * label is updated; Name, OfflineMsg, and Motd are also reflected in the
 * update, if applicable.
 */
RXUpdate_VolumeStatus(volptr, StoreVolStatus, Name, OfflineMsg, Motd)
    Volume *volptr;
    AFSStoreVolumeStatus* StoreVolStatus;
    char *Name;
    char *OfflineMsg;
    char *Motd;

{
    Error errorCode = 0;

    if (StoreVolStatus->Mask & AFS_SETMINQUOTA)
	V_minquota(volptr) = StoreVolStatus->MinQuota;
    if (StoreVolStatus->Mask & AFS_SETMAXQUOTA)
	V_maxquota(volptr) = StoreVolStatus->MaxQuota;
    if (strlen(OfflineMsg) > 0) {
	strcpy(V_offlineMessage(volptr), OfflineMsg);
    }
    if (strlen(Name) > 0) {
	strcpy(V_name(volptr), Name);
    }
#if TRANSARC_VOL_STATS
    /*
     * We don't overwrite the motd field, since it's now being used
     * for stats
     */
#else
    if (strlen(Motd) > 0) {
	strcpy(V_motd(volptr), Motd);
    }
#endif /* FS_STATS_DETAILED */
    VUpdateVolume(&errorCode, volptr);
    return(errorCode);

} /*RXUpdate_VolumeStatus*/


/* old interface */
Update_VolumeStatus(volptr, StoreVolStatus, Name, OfflineMsg, Motd)
    Volume *volptr;
    VolumeStatus *StoreVolStatus;
    struct BBS *Name;
    struct BBS *OfflineMsg;
    struct BBS *Motd;

{
    Error errorCode = 0;

    if (StoreVolStatus->MinQuota > -1)
	V_minquota(volptr) = StoreVolStatus->MinQuota;
    if (StoreVolStatus->MaxQuota > -1)
	V_maxquota(volptr) = StoreVolStatus->MaxQuota;
    if (OfflineMsg->SeqLen > 1)
	strcpy(V_offlineMessage(volptr), OfflineMsg->SeqBody);
    if (Name->SeqLen > 1)
	strcpy(V_name(volptr), Name->SeqBody);
#if TRANSARC_VOL_STATS
    /*
     * We don't overwrite the motd field, since it's now being used
     * for stats
     */
#else
    if (Motd->SeqLen > 1)
	strcpy(V_motd(volptr), Motd->SeqBody);
#endif /* FS_STATS_DETAILED */
    VUpdateVolume(&errorCode, volptr);
    return(errorCode);

} /*Update_VolumeStatus*/


/*
 * Get internal volume-related statistics from the Volume disk label
 * structure and put it into the VolumeStatus structure, status; it's
 * used by both SAFS_GetVolumeStatus and SAFS_SetVolumeStatus to return
 * the volume status to the caller.
 */
GetVolumeStatus(status, name, offMsg, motd, volptr)
VolumeStatus	* status;
struct BBS		* name;
struct BBS		* offMsg;
struct BBS		* motd;
Volume			* volptr;

{
    status->Vid = V_id(volptr);
    status->ParentId = V_parentId(volptr);
    status->Online = V_inUse(volptr);
    status->InService = V_inService(volptr);
    status->Blessed = V_blessed(volptr);
    status->NeedsSalvage = V_needsSalvaged(volptr);
    if (VolumeWriteable(volptr))
	status->Type = ReadWrite;
    else
	status->Type = ReadOnly;
    status->MinQuota = V_minquota(volptr);
    status->MaxQuota = V_maxquota(volptr);
    status->BlocksInUse = V_diskused(volptr);
    status->PartBlocksAvail = volptr->partition->free;
    status->PartMaxBlocks = volptr->partition->totalUsable;
    strncpy(name->SeqBody, V_name(volptr), (int)name->MaxSeqLen);
    name->SeqLen = strlen(V_name(volptr)) + 1;
    if (name->SeqLen > name->MaxSeqLen) name->SeqLen = name -> MaxSeqLen;
    strncpy(offMsg->SeqBody, V_offlineMessage(volptr), (int)name->MaxSeqLen);
    offMsg->SeqLen = strlen(V_offlineMessage(volptr)) + 1;
    if (offMsg->SeqLen > offMsg->MaxSeqLen)
	offMsg->SeqLen = offMsg -> MaxSeqLen;
#ifdef notdef
    /*Don't do anything with the motd field*/
    strncpy(motd->SeqBody, nullString, (int)offMsg->MaxSeqLen);
    motd->SeqLen = strlen(nullString) + 1;
#endif
    if (motd->SeqLen > motd->MaxSeqLen)
	motd->SeqLen = motd -> MaxSeqLen;

} /*GetVolumeStatus*/


RXGetVolumeStatus(status, name, offMsg, motd, volptr)
    AFSFetchVolumeStatus *status;
    char **name;
    char **offMsg;
    char **motd;
    Volume *volptr;

{
    int temp;

    status->Vid = V_id(volptr);
    status->ParentId = V_parentId(volptr);
    status->Online = V_inUse(volptr);
    status->InService = V_inService(volptr);
    status->Blessed = V_blessed(volptr);
    status->NeedsSalvage = V_needsSalvaged(volptr);
    if (VolumeWriteable(volptr))
	status->Type = ReadWrite;
    else
	status->Type = ReadOnly;
    status->MinQuota = V_minquota(volptr);
    status->MaxQuota = V_maxquota(volptr);
    status->BlocksInUse = V_diskused(volptr);
    status->PartBlocksAvail = volptr->partition->free;
    status->PartMaxBlocks = volptr->partition->totalUsable;

    /* now allocate and copy these things; they're freed by the RXGEN stub */
    temp = strlen(V_name(volptr)) + 1;
    *name = malloc(temp);
    strcpy(*name, V_name(volptr));
    temp = strlen(V_offlineMessage(volptr)) + 1;
    *offMsg = malloc(temp);
    strcpy(*offMsg, V_offlineMessage(volptr));
#if TRANSARC_VOL_STATS
    *motd = malloc(1);
    strcpy(*motd, nullString);
#else
    temp = strlen(V_motd(volptr)) + 1;
    *motd = malloc(temp);
    strcpy(*motd, V_motd(volptr));
#endif /* FS_STATS_DETAILED */

} /*RXGetVolumeStatus*/


/* Set AFS Data Fetch/Store related statistics. */

void SetAFSStats(stats)
    struct AFSStatistics *stats;

{
    extern afs_int32 StartTime, CurrentConnections;
    int	seconds;

    FS_LOCK
    stats->CurrentMsgNumber = 0;
    stats->OldestMsgNumber = 0;
    stats->StartTime = StartTime;
    stats->CurrentConnections = CurrentConnections;
    stats->TotalAFSCalls = AFSCallStats.TotalCalls;
    stats->TotalFetchs = AFSCallStats.FetchData+AFSCallStats.FetchACL+AFSCallStats.FetchStatus;
    stats->FetchDatas = AFSCallStats.FetchData;
    stats->FetchedBytes = AFSCallStats.TotalFetchedBytes;
    seconds = AFSCallStats.AccumFetchTime/1000;
    if (seconds <= 0) seconds = 1;
    stats->FetchDataRate = AFSCallStats.TotalFetchedBytes/seconds;
    stats->TotalStores = AFSCallStats.StoreData+AFSCallStats.StoreACL+AFSCallStats.StoreStatus;
    stats->StoreDatas = AFSCallStats.StoreData;
    stats->StoredBytes = AFSCallStats.TotalStoredBytes;
    seconds = AFSCallStats.AccumStoreTime/1000;
    if (seconds <= 0) seconds = 1;
    stats->StoreDataRate = AFSCallStats.TotalStoredBytes/seconds;
#ifdef AFS_NT40_ENV
    stats->ProcessSize = -1; /* TODO: */
#else
    stats->ProcessSize = (afs_int32)((long) sbrk(0) >> 10);
#endif
    FS_UNLOCK
    h_GetWorkStats((int *)&(stats->WorkStations),(int *)&(stats->ActiveWorkStations),
	    (int *)0, (afs_int32)(FT_ApproxTime())-(15*60));

} /*SetAFSStats*/


/* Get disk related information from all AFS partitions. */

void SetVolumeStats(stats)
    struct AFSStatistics *stats;

{
    struct DiskPartition * part;
    int i = 0;

    for (part = DiskPartitionList; part && i < AFS_MSTATDISKS; part = part->next) {
	stats->Disks[i].TotalBlocks = part->totalUsable;
	stats->Disks[i].BlocksAvailable = part->free;
	bzero(stats->Disks[i].Name, AFS_DISKNAMESIZE);
	strncpy(stats->Disks[i].Name, part->name, AFS_DISKNAMESIZE);
	i++;
    }
    while (i < AFS_MSTATDISKS) {
	stats->Disks[i].TotalBlocks = -1;
	i++;
    }
} /*SetVolumeStats*/


#ifdef notdef
struct nlist RawStats[] = {
#define CPTIME 0
    {	"_cp_time"    },
#define BOOT 1
    {	"_boottime"    },
#define DISK 2
    {	"_dk_xfer"    },
#ifndef	AFS_SUN_ENV
#define SWAPMAP 3
    {	"_swapmap"    },
#define NSWAPMAP 4
    {	"_nswapmap"    },
#define NSWAPBLKS 5
    {	"_nswap"    },
#define DMMAX 6
    {	"_dmmax"    },
#else /* AFS_SUN_ENV */
#define SANON 3
    {	"_anoninfo" },
#endif
    {	""   },

};
#endif

/* Get some kernel specific related statistics */

void SetSystemStats(stats)
    struct AFSStatistics * stats;

{
/* Fix this sometime soon.. */
#ifdef notdef
    static	int	kmem = 0;
    static	struct	mapent	*swapMap = 0;
    static	int	swapMapAddr = 0;
    static	int	swapMapSize = 0;
    static	int	numSwapBlks = 0;
    int		numSwapEntries,
		dmmax;
    register	int	i;
    struct	mapent	* sp;
    afs_int32	busy[CPUSTATES];
    afs_int32	xfer[DK_NDRIVE];
    struct	timeval	bootTime;
#endif
    struct	timeval	time;

    /* this works on all system types */
    TM_GetTimeOfDay(&time, 0);
    stats->CurrentTime = time.tv_sec;

#ifdef notdef
    stats->UserCPU =stats->SystemCPU =stats->NiceCPU =stats->IdleCPU =stats->BootTime =0;
    stats->TotalIO =stats->ActiveVM =stats->TotalVM = 0;
    for (i=0; i < AFS_MSTATSPARES; i++) stats->Spares[i] = 0;

    if (kmem ==	-1) return;
    if (kmem == 0) {
	nlist("/vmunix", RawStats);
	if (RawStats[0].n_type == 0) {
	    ViceLog(0, ("Could not get a namelist from VMUNIX\n"));
	    kmem = -1;
	    return;
	}
	kmem = open("/dev/kmem",O_RDONLY,0);
	if (kmem <= 0) {
	    ViceLog(0, ("Could not open /dev/kmem\n"));
	    kmem = -1;
	    return;
	}
    }
    lseek(kmem, (afs_int32) RawStats[CPTIME].n_value,0);
    read(kmem, (char *)busy, sizeof(busy));
    stats->SystemCPU = busy[CP_SYS];
    stats->UserCPU = busy[CP_USER];
    stats->NiceCPU = busy[CP_NICE];
    stats->IdleCPU = busy[CP_IDLE];
    lseek(kmem, (afs_int32) RawStats[BOOT].n_value,0);
    read(kmem, (char *)&bootTime, sizeof(bootTime));
    stats->BootTime = bootTime.tv_sec;
    lseek(kmem, (afs_int32) RawStats[DISK].n_value,0);
    read(kmem, (char *)xfer, sizeof(xfer));
    stats->TotalIO = 0;
    for(i = 0; i < DK_NDRIVE; i++) {
	stats->TotalIO += xfer[i];
    }
#ifdef	AFS_SUN_ENV
    {
#include <vm/anon.h>
	struct anoninfo ai;
#define	ctok(x)	    ((ctob(x)) >> 10)

	lseek(kmem, (afs_int32)RawStats[SANON].n_value,0);
	read(kmem, (char *)&ai, sizeof (struct anoninfo));
	stats->TotalVM = ctok(ai.ani_max - ai.ani_resv);    /* available */
	stats->ActiveVM	= ctok(ai.ani_resv);	/*  used */
    }
#else
    if (!swapMap) {
	lseek(kmem, (afs_int32)RawStats[SWAPMAP].n_value,0);
	read(kmem, (char *)&swapMapAddr, sizeof(swapMapAddr));
	swapMapAddr += sizeof(struct map);
	lseek(kmem, (afs_int32)RawStats[NSWAPMAP].n_value,0);
	read(kmem, (char *)&numSwapEntries, sizeof(numSwapEntries));
	swapMapSize = (--numSwapEntries)*sizeof(struct mapent);
	lseek(kmem, (afs_int32)RawStats[NSWAPBLKS].n_value,0);
	read(kmem, (char *)&numSwapBlks, sizeof(numSwapBlks));
	lseek(kmem, (afs_int32)RawStats[DMMAX].n_value,0);
	read(kmem, (char *)&dmmax, sizeof(dmmax));
	numSwapBlks -= dmmax/2;
	swapMap = (struct mapent *)malloc(swapMapSize);
    }
    sp = (struct mapent *)swapMap;
    lseek(kmem, (afs_int32)swapMapAddr, 0);
    read(kmem, (char *)sp, swapMapSize);
    for(stats->TotalVM = stats->ActiveVM = numSwapBlks; sp->m_size; sp++) {
	stats->ActiveVM -= sp->m_size;
    }
#endif /* AFS_SUN_ENV */
#endif

} /*SetSystemStats*/


/* Validate target file */


FileNameOK(aname)
    register char *aname;

{
    register afs_int32 i, tc;
    i = strlen(aname);
    if (i >= 4) {
	/* watch for @sys on the right */
	if (strcmp(aname+i-4, "@sys") == 0) return 0;
    }
    while (tc = *aname++) {
	if (tc == '/') return 0;    /* very bad character to encounter */
    }
    return 1;	/* file name is ok */

} /*FileNameOK*/


/* Debugging tool to print Volume Statu's contents */

PrintVolumeStatus(status)
    VolumeStatus *status;

{
    ViceLog(5,("Volume header contains:\n"));
    ViceLog(5,("Vid = %u, Parent = %u, Online = %d, InService = %d, Blessed = %d, NeedsSalvage = %d\n",
	    status->Vid, status->ParentId, status->Online, status->InService,
	    status->Blessed, status->NeedsSalvage));
    ViceLog(5,("MinQuota = %d, MaxQuota = %d\n", status->MinQuota, status->MaxQuota));
    ViceLog(5,("Type = %d, BlocksInUse = %d, PartBlocksAvail = %d, PartMaxBlocks = %d\n",
	    status->Type, status->BlocksInUse, status->PartBlocksAvail, status->PartMaxBlocks));

} /*PrintVolumeStatus*/


/*
 * This variant of symlink is expressly to support the AFS/DFS translator
 * and is not supported by the AFS fileserver. We just return EINVAL.
 * The cache manager should not generate this call to an AFS cache manager.
 */
SRXAFS_DFSSymlink (tcon, DirFid, Name, LinkContents, InStatus, OutFid, OutFidStatus, OutDirStatus, CallBack, Sync)
    struct rx_connection *tcon;		 /* Rx connection handle */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    char *LinkContents;			 /* Contents of the new created file */
    struct AFSStoreStatus *InStatus;	 /* Input status for the new symbolic link */
    struct AFSFid *OutFid;		 /* Fid for newly created symbolic link */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new symbolic link */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */
    struct AFSCallBack *CallBack;	 /* Callback on link */
    struct AFSVolSync *Sync;
{
    return EINVAL;
}

SRXAFS_ResidencyCmd (tcon, Fid, Inputs, Outputs)
    struct rx_connection *tcon;
    struct AFSFid *Fid;
    struct ResidencyCmdInputs *Inputs;
    struct ResidencyCmdOutputs *Outputs;
{
    return EINVAL;
}
