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

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header$");

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
#if ! defined(AFS_SGI_ENV) && ! defined(AFS_AIX32_ENV) && ! defined(AFS_NT40_ENV) && ! defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
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
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
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

void	ResetDebug(), SetDebug(), Terminate();
int	CopyOnWrite();		/* returns 0 on success */


void SetSystemStats(), SetAFSStats(), SetVolumeStats();
int	LogLevel = 0;
int	supported = 1;
int	Console = 0;
afs_int32 BlocksSpare = 1024;	/* allow 1 MB overruns */
afs_int32 PctSpare;
extern afs_int32 implicitAdminRights;

static TryLocalVLServer();

void GetStatus(Vnode *targetptr, AFSFetchStatus *status, afs_int32 rights, afs_int32 anyrights, Vnode *parentptr);

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
 * Note that this function always returns a held host, so
 * that CallPostamble can block without the host's disappearing.
 * Call returns rx connection in passed in *tconn
 */
static CallPreamble(acall, activecall, tconn)
    register struct rx_call *acall;
    int activecall;
    struct rx_connection **tconn;

{
    struct host *thost;
    struct client *tclient;
    int retry_flag=1;
    int code = 0;
    char hoststr[16];
    if (!tconn) {
	ViceLog (0, ("CallPreamble: unexpected null tconn!\n"));
	return -1;
    }
    *tconn = rx_ConnectionOf(acall);

    H_LOCK
retry:
    tclient = h_FindClient_r(*tconn);
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
      ViceLog(3,("Discarded a packet for deleted host %s\n",afs_inet_ntoa_r(thost->host,hoststr)));
      code = VBUSY; /* raced, so retry */
    }
    else if (thost->hostFlags & VENUSDOWN) {
      if (BreakDelayedCallBacks_r(thost)) {
	ViceLog(0,("BreakDelayedCallbacks FAILED for host %s which IS UP.  Possible network or routing failure.\n",
		afs_inet_ntoa_r(thost->host, hoststr)));
	if ( MultiProbeAlternateAddress_r (thost) ) {
	    ViceLog(0, ("MultiProbe failed to find new address for host %s:%d\n",
			afs_inet_ntoa_r(thost->host, hoststr), thost->port));
	    code = -1;
	} else {
	    ViceLog(0, ("MultiProbe found new address for host %s:%d\n",
			afs_inet_ntoa_r(thost->host, hoststr), thost->port));
	    if (BreakDelayedCallBacks_r(thost)) {
		ViceLog(0,("BreakDelayedCallbacks FAILED AGAIN for host %s which IS UP.  Possible network or routing failure.\n",
			afs_inet_ntoa_r(thost->host, hoststr)));
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


SRXAFS_FetchData (acall, Fid, Pos, Len, OutStatus, CallBack, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Fid of file to fetch */
    afs_int32 Pos, Len;	   		/* Not implemented yet */
    struct AFSFetchStatus *OutStatus;	/* Returned status for Fid */
    struct AFSCallBack *CallBack;	/* If r/w return CB for Fid */
    struct AFSVolSync *Sync;		/* synchronization info */

{
    int code;

    code = common_FetchData (acall, Fid, Pos, Len, OutStatus,
			     CallBack, Sync, 0);
    return code;
}

SRXAFS_FetchData64 (acall, Fid, Pos, Len, OutStatus, CallBack, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;                 /* Fid of file to fetch */
    afs_int64 Pos, Len;                 /* Not implemented yet */
    struct AFSFetchStatus *OutStatus;   /* Returned status for Fid */
    struct AFSCallBack *CallBack;       /* If r/w return CB for Fid */
    struct AFSVolSync *Sync;            /* synchronization info */
{
    int code;
    afs_int32 tPos, tLen;

#ifdef AFS_64BIT_ENV
    if (Pos + Len > 0x7fffffff)
        return E2BIG;
    tPos = Pos;
    tLen = Len;
#else /* AFS_64BIT_ENV */
    if (Pos.high || Len.high)
        return E2BIG;
    tPos = Pos.low;
    tLen = Len.low;
#endif /* AFS_64BIT_ENV */

    code = common_FetchData (acall, Fid, tPos, tLen, OutStatus,
			     CallBack, Sync, 1);
    return code;
}

common_FetchData (acall, Fid, Pos, Len, OutStatus, CallBack, Sync, type)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;                 /* Fid of file to fetch */
    afs_int32 Pos, Len;                 /* Not implemented yet */
    struct AFSFetchStatus *OutStatus;   /* Returned status for Fid */
    struct AFSCallBack *CallBack;       /* If r/w return CB for Fid */
    struct AFSVolSync *Sync;            /* synchronization info */
    int type;                           /* 32 bit or 64 bit call */

{ 
    Vnode * targetptr =	0;		    /* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	    /* parent vnode if vptr is a file */
    Vnode   tparentwhentargetnotdir;	    /* parent vnode for GetStatus */
    int	    errorCode =	0;		    /* return code to caller */
    int	    fileCode =	0;		    /* return code from vol package */
    Volume * volptr = 0;		    /* pointer to the volume */
    struct client *client;		    /* pointer to the client data */
    struct rx_connection *tcon;		    /* the connection we're part of */
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

    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon))
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
    errorCode = FetchData_RXStyle(volptr, targetptr, acall, Pos, Len, type,
				  &bytesToXfer, &bytesXferred);
#else
    if (errorCode = FetchData_RXStyle(volptr, targetptr, acall, Pos, Len, type))
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
      memset(&myFid, 0, sizeof(struct AFSFid));
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

    osi_auditU (acall, FetchDataEvent, errorCode, AUD_FID, Fid, AUD_END);
    return(errorCode);

} /*SRXAFS_FetchData*/


SRXAFS_FetchACL (acall, Fid, AccessList, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Fid of target dir */
    struct AFSOpaque *AccessList;	/* Returned contents of dir's ACL */
    struct AFSFetchStatus *OutStatus;	/* Returned status for the dir */
    struct AFSVolSync *Sync;
{
    Vnode * targetptr =	0;		/* pointer to vnode to fetch */
    Vnode * parentwhentargetnotdir = 0;	/* parent vnode if targetptr is a file */
    int	    errorCode =	0;		/* return error code to caller */
    Volume * volptr = 0;		/* pointer to the volume */
    struct client *client;		/* pointer to the client data */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct rx_connection *tcon = rx_ConnectionOf(acall);
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
    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, FetchACLEvent, errorCode, AUD_FID, Fid, AUD_END);
    return errorCode;

} /*SRXAFS_FetchACL*/


/*
 * This routine is called exclusively by SRXAFS_FetchStatus(), and should be
 * merged into it when possible.
 */
SAFSS_FetchStatus (acall, Fid, OutStatus, CallBack, Sync)
    struct rx_call *acall;
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
					     CHK_FETCHSTATUS, 0)) {
	  if (rx_GetCallAbortCode(acall) == errorCode) 
	      rx_SetCallAbortCode(acall, 0);
	  goto Bad_FetchStatus;
      }
    }

    /* set OutStatus From the Fid  */
    GetStatus(targetptr, OutStatus, rights, anyrights, parentwhentargetnotdir);

    /* If a r/w volume, also set the CallBack state */
    if (VolumeWriteable(volptr))
	SetCallBackStruct(AddCallBack(client->host, Fid), CallBack);
    else {
      struct AFSFid myFid;		
      memset(&myFid, 0, sizeof(struct AFSFid));
      myFid.Volume = Fid->Volume;
      SetCallBackStruct(AddVolCallBack(client->host, &myFid), CallBack);
      }

Bad_FetchStatus: 
    /* Update and store volume/vnode and parent vnodes back */
    PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *)0, volptr);
    ViceLog(2, ("SAFS_FetchStatus returns %d\n", errorCode)); 
    return errorCode;

} /*SAFSS_FetchStatus*/


SRXAFS_BulkStatus(acall, Fids, OutStats, CallBacks, Sync)
    struct rx_call *acall;
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);
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

    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon))
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
						   CHK_FETCHSTATUS, 0)) {
		if (rx_GetCallAbortCode(acall) == errorCode) 
		    rx_SetCallAbortCode(acall, 0);
		goto Bad_BulkStatus;
	    }
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
	  memset(&myFid, 0, sizeof(struct AFSFid));
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
    osi_auditU (acall, BulkFetchStatusEvent, errorCode, AUD_FIDS, Fids, AUD_END);
    return errorCode;

} /*SRXAFS_BulkStatus*/


SRXAFS_InlineBulkStatus(acall, Fids, OutStats, CallBacks, Sync)
    struct rx_call *acall;
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
    struct rx_connection *tcon;
    AFSFetchStatus *tstatus;
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

    ViceLog(1, ("SAFS_InlineBulkStatus\n"));
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

    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon)) {
	goto Bad_InlineBulkStatus;
    }

    tfid = Fids->AFSCBFids_val;
    for (i=0; i<nfiles; i++, tfid++) {
	/*
	 * Get volume/vnode for the fetched file; caller's rights to it
	 * are also returned
	 */
	if (errorCode =
	    GetVolumePackage(tcon, tfid, &volptr, &targetptr,
			     DONTCHECK, &parentwhentargetnotdir, &client,
			     READ_LOCK, &rights, &anyrights)) {
	    tstatus = &OutStats->AFSBulkStats_val[i];
	    tstatus->errorCode = errorCode;
	    parentwhentargetnotdir = (Vnode *) 0;
	    targetptr = (Vnode *) 0;
	    volptr = (Volume *) 0;
	    continue;
	}

	/* set volume synchronization information, but only once per call */
	if (i == nfiles)
	    SetVolumeSync(Sync, volptr);

	/* Are we allowed to fetch Fid's status? */
	if (targetptr->disk.type != vDirectory) {
	    if (errorCode = Check_PermissionRights(targetptr, client, rights,
						   CHK_FETCHSTATUS, 0)) {
		tstatus = &OutStats->AFSBulkStats_val[i];
		tstatus->errorCode = errorCode;
		PutVolumePackage(parentwhentargetnotdir, targetptr, (Vnode *) 0, volptr);
		parentwhentargetnotdir = (Vnode *) 0;
		targetptr = (Vnode *) 0;
		volptr = (Volume *) 0;
		continue;
	    }
	}

	/* set OutStatus From the Fid  */
	GetStatus(targetptr, (struct AFSFetchStatus *) &OutStats->AFSBulkStats_val[i], 
	  rights, anyrights, parentwhentargetnotdir);

	/* If a r/w volume, also set the CallBack state */
	if (VolumeWriteable(volptr))
	    SetCallBackStruct(AddBulkCallBack(client->host, tfid),
			      &CallBacks->AFSCBs_val[i]);
	else {
	  struct AFSFid myFid;		
	  memset(&myFid, 0, sizeof(struct AFSFid));
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

Bad_InlineBulkStatus: 
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
    ViceLog(2, ("SAFS_InlineBulkStatus	returns	%d\n", errorCode)); 
    osi_auditU (acall, InlineBulkFetchStatusEvent, errorCode, AUD_FIDS, Fids, AUD_END);
    return 0;

} /*SRXAFS_InlineBulkStatus*/


SRXAFS_FetchStatus (acall, Fid, OutStatus, CallBack, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Fid of target file */
    struct AFSFetchStatus *OutStatus;	/* Returned status for the fid */
    struct AFSCallBack *CallBack;	/* if r/w, callback promise for Fid */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_FetchStatus;

    code = SAFSS_FetchStatus (acall, Fid, OutStatus, CallBack, Sync);

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

    osi_auditU (acall, FetchStatusEvent, code, AUD_FID, Fid, AUD_END);
    return code;

} /*SRXAFS_FetchStatus*/


SRXAFS_StoreData (acall, Fid, InStatus, Pos, Length, FileLength, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Fid of taret file */
    struct AFSStoreStatus *InStatus;	/* Input Status for Fid */
    afs_uint32 Pos;                    /* Not implemented yet */
    afs_uint32 Length;                 /* Length of data to store */
    afs_uint32 FileLength;             /* Length of file after store */
    struct AFSFetchStatus *OutStatus;	/* Returned status for target fid */
    struct AFSVolSync *Sync;
{
    Vnode * targetptr =	0;		/* pointer to input fid */
    Vnode * parentwhentargetnotdir = 0;	/* parent of Fid to get ACL */
    Vnode   tparentwhentargetnotdir;	/* parent vnode for GetStatus */
    int	    errorCode =	0;		/* return code for caller */
    int	    fileCode =	0;		/* return code from vol package */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct client *t_client;            /* tmp ptr to client data */
    struct in_addr logHostAddr;		    /* host ip holder for inet_ntoa */
    struct rx_connection *tcon;
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

    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon))
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
    errorCode = StoreData_RXStyle(volptr, targetptr, Fid, client, acall,
				  Pos, Length, FileLength,
				  (InStatus->Mask & AFS_FSYNC),
				  &bytesToXfer, &bytesXferred);
#else
    errorCode = StoreData_RXStyle(volptr, targetptr, Fid, client,
				      acall, Pos, Length, FileLength,
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

    osi_auditU (acall, StoreDataEvent, errorCode, AUD_FID, Fid, AUD_END);
    return(errorCode);

} /*SRXAFS_StoreData*/

SRXAFS_StoreData64 (acall, Fid, InStatus, Pos, Length, FileLength, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;                 /* Fid of taret file */
    struct AFSStoreStatus *InStatus;    /* Input Status for Fid */
    afs_uint64 Pos;                     /* Not implemented yet */
    afs_uint64 Length;                  /* Length of data to store */
    afs_uint64 FileLength;              /* Length of file after store */
    struct AFSFetchStatus *OutStatus;   /* Returned status for target fid */
    struct AFSVolSync *Sync;
{
    int code;
    afs_int32 tPos;
    afs_int32 tLength;
    afs_int32 tFileLength;

#ifdef AFS_64BIT_ENV
    if (FileLength > 0x7fffffff)
        return E2BIG;
    tPos = Pos;
    tLength = Length;
    tFileLength = FileLength;
#else /* AFS_64BIT_ENV */
    if (FileLength.high)
        return E2BIG;
    tPos = Pos.low;
    tLength = Length.low;
    tFileLength = FileLength.low;
#endif /* AFS_64BIT_ENV */

    code = SRXAFS_StoreData (acall, Fid, InStatus, tPos, tLength, tFileLength,
                             OutStatus, Sync);
    return code;
}

SRXAFS_StoreACL (acall, Fid, AccessList, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Target dir's fid */
    struct AFSOpaque *AccessList;	/* Access List's contents */
    struct AFSFetchStatus *OutStatus;	/* Returned status of fid */
    struct AFSVolSync *Sync;
{
    Vnode * targetptr =	0;		/* pointer to input fid */
    Vnode * parentwhentargetnotdir = 0;	/* parent of Fid to get ACL */
    int	    errorCode =	0;		/* return code for caller */
    struct AFSStoreStatus InStatus;	/* Input status for fid */
    Volume * volptr = 0;		/* pointer to the volume header */
    struct client * client;		/* pointer to client structure */
    afs_int32 rights, anyrights;		/* rights for this and any user */
    struct rx_connection *tcon;
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
    if (errorCode = CallPreamble(acall, ACTIVECALL, &tcon))
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
    GetStatus(targetptr, OutStatus, rights, anyrights, 0);

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

    osi_auditU (acall, StoreACLEvent, errorCode, AUD_FID, Fid, AUD_END);
    return errorCode;

} /*SRXAFS_StoreACL*/


/*
 * Note: This routine is called exclusively from SRXAFS_StoreStatus(), and
 * should be merged when possible.
 */
SAFSS_StoreStatus (acall, Fid, InStatus, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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


SRXAFS_StoreStatus (acall, Fid, InStatus, OutStatus, Sync)
    struct rx_call *acall;		/* Rx call */
    struct AFSFid *Fid;			/* Target file's fid */
    struct AFSStoreStatus *InStatus;	/* Input status for Fid */
    struct AFSFetchStatus *OutStatus;	/* Output status for fid */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_StoreStatus;

    code = SAFSS_StoreStatus (acall, Fid, InStatus, OutStatus, Sync);

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

    osi_auditU (acall, StoreStatusEvent, code, AUD_FID, Fid, AUD_END);
    return code;

} /*SRXAFS_StoreStatus*/


/*
 * This routine is called exclusively by SRXAFS_RemoveFile(), and should be
 * merged in when possible.
 */
SAFSS_RemoveFile (acall, DirFid, Name, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
    GetStatus(parentptr, OutDirStatus, rights, anyrights, 0);

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


SRXAFS_RemoveFile (acall, DirFid, Name, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Dir fid for file to remove */
    char *Name;				 /* File name to remove */
    struct AFSFetchStatus *OutDirStatus; /* Output status for dir fid's */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_RemoveFile;

    code = SAFSS_RemoveFile (acall, DirFid, Name, OutDirStatus, Sync);

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

    osi_auditU (acall, RemoveFileEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_RemoveFile*/


/*
 * This routine is called exclusively from SRXAFS_CreateFile(), and should
 * be merged in when possible.
 */
SAFSS_CreateFile (acall, DirFid, Name, InStatus, OutFid, OutFidStatus,
		 OutDirStatus, CallBack, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
    GetStatus(parentptr, OutDirStatus, rights, anyrights, 0);

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


SRXAFS_CreateFile (acall, DirFid, Name, InStatus, OutFid, OutFidStatus, OutDirStatus, CallBack, Sync)
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Parent Dir fid */
    char *Name;				 /* File name to be created */
    struct AFSStoreStatus *InStatus;	 /* Input status for newly created file */
    struct AFSFid *OutFid;		 /* Fid for newly created file */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new file */
    struct AFSFetchStatus *OutDirStatus; /* Ouput status for the parent dir */
    struct AFSCallBack *CallBack;	 /* Return callback promise for new file */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_CreateFile;

    code = SAFSS_CreateFile (acall, DirFid, Name, InStatus, OutFid,
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

    osi_auditU (acall, CreateFileEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_CreateFile*/


/*
 * This routine is called exclusively from SRXAFS_Rename(), and should be
 * merged in when possible.
 */
SAFSS_Rename (acall, OldDirFid, OldName, NewDirFid, NewName, OutOldDirStatus,
	     OutNewDirStatus, Sync)
    struct rx_call *acall;		    /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
		    ViceLog(0, ("Del: inode=%s, name=%s, errno=%d\n",
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
    GetStatus(oldvptr, OutOldDirStatus, rights, anyrights, 0);
    GetStatus(newvptr, OutNewDirStatus, newrights, newanyrights, 0);
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


SRXAFS_Rename (acall, OldDirFid, OldName, NewDirFid, NewName, OutOldDirStatus, OutNewDirStatus, Sync)
    struct rx_call *acall;		     /* Rx call */
    struct AFSFid *OldDirFid;		     /* From parent dir's fid */
    char *OldName;			     /* From file name */
    struct AFSFid *NewDirFid;		     /* To parent dir's fid */
    char *NewName;	   		     /* To new file name */
    struct AFSFetchStatus *OutOldDirStatus;  /* Output status for From parent dir */
    struct AFSFetchStatus *OutNewDirStatus;  /* Output status for To parent dir */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_Rename;

    code = SAFSS_Rename (acall, OldDirFid, OldName, NewDirFid, NewName,
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

    osi_auditU (acall, RenameFileEvent, code, AUD_FID, OldDirFid, AUD_STR, OldName, AUD_FID, NewDirFid, AUD_STR, NewName, AUD_END);
    return code;

} /*SRXAFS_Rename*/


/*
 * This routine is called exclusively by SRXAFS_Symlink(), and should be
 * merged into it when possible.
 */
SAFSS_Symlink (acall, DirFid, Name, LinkContents, InStatus, OutFid, OutFidStatus, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
     * If we're creating a mount point (any x bits clear), we must have
     * administer access to the directory, too.  Always allow sysadmins
     * to do this.
     */
    if ((InStatus->Mask & AFS_SETMODE) && !(InStatus->UnixModeBits & 0111)) {
	/*
	 * We have a mountpoint, 'cause we're trying to set the Unix mode
	 * bits to something with some x bits missing (default mode bits
	 * if AFS_SETMODE is false is 0777)
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
    GetStatus(parentptr, OutDirStatus, rights, anyrights, 0);

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


SRXAFS_Symlink (acall, DirFid, Name, LinkContents, InStatus, OutFid, OutFidStatus, OutDirStatus, Sync)
    struct AFSVolSync *Sync;
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    char *LinkContents;			 /* Contents of the new created file */
    struct AFSStoreStatus *InStatus;	 /* Input status for the new symbolic link */
    struct AFSFid *OutFid;		 /* Fid for newly created symbolic link */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new symbolic link */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */

{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_Symlink;

    code = SAFSS_Symlink (acall, DirFid, Name, LinkContents, InStatus, OutFid,
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

    osi_auditU (acall, SymlinkEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_Symlink*/


/*
 * This routine is called exclusively by SRXAFS_Link(), and should be
 * merged into it when possible.
 */
SAFSS_Link (acall, DirFid, Name, ExistingFid, OutFidStatus, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
    GetStatus(parentptr, OutDirStatus, rights, anyrights, 0);

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


SRXAFS_Link (acall, DirFid, Name, ExistingFid, OutFidStatus, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* File name to create */
    struct AFSFid *ExistingFid;		 /* Fid of existing fid we'll make link to */
    struct AFSFetchStatus *OutFidStatus; /* Output status for newly created file */
    struct AFSFetchStatus *OutDirStatus; /* Outpout status for parent dir */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
	goto Bad_Link;

    code = SAFSS_Link (acall, DirFid, Name, ExistingFid, OutFidStatus,
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

    osi_auditU (acall, LinkEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_FID, ExistingFid, AUD_END);
    return code;

} /*SRXAFS_Link*/


/*
 * This routine is called exclusively by SRXAFS_MakeDir(), and should be
 * merged into it when possible.
 */
SAFSS_MakeDir (acall, DirFid, Name, InStatus, OutFid, OutFidStatus,
	      OutDirStatus, CallBack, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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
    memcpy((char *)newACL, (char *)VVnodeACL(parentptr), VAclSize(parentptr));

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


SRXAFS_MakeDir (acall, DirFid, Name, InStatus, OutFid, OutFidStatus, OutDirStatus, CallBack, Sync)
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* Name of dir to be created */
    struct AFSStoreStatus *InStatus;	 /* Input status for new dir */
    struct AFSFid *OutFid;		 /* Fid of new dir */
    struct AFSFetchStatus *OutFidStatus; /* Output status for new directory */
    struct AFSFetchStatus *OutDirStatus; /* Output status for parent dir */
    struct AFSCallBack *CallBack;	 /* Returned callback promise for new dir */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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
    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, MakeDirEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_MakeDir*/


/*
 * This routine is called exclusively by SRXAFS_RemoveDir(), and should be
 * merged into it when possible.
 */
SAFSS_RemoveDir (acall, DirFid, Name, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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


SRXAFS_RemoveDir (acall, DirFid, Name, OutDirStatus, Sync)
    struct rx_call *acall;		 /* Rx call */
    struct AFSFid *DirFid;		 /* Parent dir's fid */
    char *Name;				 /* (Empty) dir's name to be removed */
    struct AFSFetchStatus *OutDirStatus; /* Output status for the parent dir */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, RemoveDirEvent, code, AUD_FID, DirFid, AUD_STR, Name, AUD_END);
    return code;

} /*SRXAFS_RemoveDir*/


/*
 * This routine is called exclusively by SRXAFS_SetLock(), and should be
 * merged into it when possible.
 */
SAFSS_SetLock (acall, Fid, type, Sync)
    struct rx_call *acall; /* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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


SRXAFS_OldSetLock(acall, Fid, type, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file to lock */
    ViceLockType type;		/* Type of lock (Read or write) */
    struct AFSVolSync *Sync;
{
    return SRXAFS_SetLock(acall, Fid, type, Sync);

} /*SRXAFS_OldSetLock*/


SRXAFS_SetLock (acall, Fid, type, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file to lock */
    ViceLockType type;		/* Type of lock (Read or write) */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, SetLockEvent, code, AUD_FID, Fid, AUD_LONG, type, AUD_END);
    return code;

} /*SRXAFS_SetLock*/


/*
 * This routine is called exclusively by SRXAFS_ExtendLock(), and should be
 * merged into it when possible.
 */
SAFSS_ExtendLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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


SRXAFS_OldExtendLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file whose lock we extend */
    struct AFSVolSync *Sync;
{
    return SRXAFS_ExtendLock(acall, Fid, Sync);

} /*SRXAFS_OldExtendLock*/


SRXAFS_ExtendLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file whose lock we extend */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, ExtendLockEvent, code, AUD_FID, Fid , AUD_END);
    return code;

} /*SRXAFS_ExtendLock*/


/*
 * This routine is called exclusively by SRXAFS_ReleaseLock(), and should be
 * merged into it when possible.
 */
SAFSS_ReleaseLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
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
    struct rx_connection *tcon = rx_ConnectionOf(acall);

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


SRXAFS_OldReleaseLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file to release lock */
    struct AFSVolSync *Sync;
{
    return SRXAFS_ReleaseLock(acall, Fid, Sync);

} /*SRXAFS_OldReleaseLock*/


SRXAFS_ReleaseLock (acall, Fid, Sync)
    struct rx_call *acall;	/* Rx call */
    struct AFSFid *Fid;		/* Fid of file to release lock */
    struct AFSVolSync *Sync;
{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, ACTIVECALL, &tcon))
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

    osi_auditU (acall, ReleaseLockEvent, code, AUD_FID, Fid , AUD_END);
    return code;

} /*SRXAFS_ReleaseLock*/


/*
 * This routine is called exclusively by SRXAFS_GetStatistics(), and should be
 * merged into it when possible.
 */
static GetStatistics (acall, Statistics)
    struct rx_call *acall;	      /* Rx call */
    struct AFSStatistics *Statistics; /* Placeholder for returned AFS statistics */
{
    ViceLog(1, ("SAFS_GetStatistics Received\n"));
    FS_LOCK
    AFSCallStats.GetStatistics++, AFSCallStats.TotalCalls++;
    FS_UNLOCK
    memset(Statistics, 0, sizeof(*Statistics));
    SetAFSStats(Statistics);
    SetVolumeStats(Statistics);
    SetSystemStats(Statistics);

    return(0);

} /*GetStatistics*/


SRXAFS_GetStatistics (acall, Statistics)
    struct rx_call *acall;	      /* Rx call */
    struct ViceStatistics *Statistics; /* Placeholder for returned AFS statistics */

{
    afs_int32 code;
    struct rx_connection *tcon;
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

    if (code = CallPreamble(acall, NOTACTIVECALL, &tcon))
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

    int dir_Buffers;		/*# buffers in use by 