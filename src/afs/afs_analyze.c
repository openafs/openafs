/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include "../afs/param.h"	/* Should be always first */
#include "../afs/stds.h"
#include "../afs/sysincludes.h"	/* Standard vendor system headers */

#ifndef UKERNEL
#ifndef AFS_LINUX20_ENV
#include <net/if.h>
#include <netinet/in.h>
#endif

#ifdef AFS_SGI62_ENV
#include "../h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV)
#include <netinet/in_var.h>
#endif
#endif /* !UKERNEL */

#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */
#include "../afs/afs_util.h"
#include "../afs/afs_prototypes.h"

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif


/* shouldn't do it this way, but for now will do */
#ifndef ERROR_TABLE_BASE_u
#define ERROR_TABLE_BASE_u	(5376L)
#endif /* ubik error base define */

/* same hack for vlserver error base as for ubik error base */
#ifndef ERROR_TABLE_BASE_vl
#define ERROR_TABLE_BASE_vl	(363520L)
#define VL_NOENT		(363524L)
#endif /* vlserver error base define */


int afs_BusyWaitPeriod = 15; /* poll every 15 seconds */


afs_int32 hm_retry_RO=0;    /* don't wait */
afs_int32 hm_retry_RW=0;    /* don't wait */
afs_int32 hm_retry_int=0;   /* don't wait */

void afs_CopyError(afrom, ato)
    register struct vrequest *afrom;
    register struct vrequest *ato;

{
    AFS_STATCNT(afs_CopyError);
    if (!afrom->initd)
	return;
    afs_FinalizeReq(ato);
    if (afrom->accessError)
	ato->accessError = 1;
    if (afrom->volumeError)
	ato->volumeError = 1;
    if (afrom->networkError)
	ato->networkError = 1;
    if (afrom->permWriteError)
	ato->permWriteError = 1;

} /*afs_CopyError*/


void afs_FinalizeReq(areq)
    register struct vrequest *areq;

{
    AFS_STATCNT(afs_FinalizeReq);
    if (areq->initd)
	return;
    areq->busyCount = 0;
    areq->accessError = 0;
    areq->volumeError = 0;
    areq->networkError = 0;
    areq->permWriteError = 0;
    areq->initd = 1;

} /*afs_FinalizeReq*/


afs_CheckCode(acode, areq, where)
    afs_int32 acode;
    struct vrequest *areq;
    int where;

{
    AFS_STATCNT(afs_CheckCode);
    if (acode) { 
	afs_Trace2(afs_iclSetp, CM_TRACE_CHECKCODE,
		   ICL_TYPE_INT32, acode, ICL_TYPE_INT32, where);
    }
    if (!areq || !areq->initd)
	return acode;
    if (areq->networkError)
	return ETIMEDOUT;
    if (acode == 0)
	return 0;
    if (areq->accessError)
	return EACCES;
    if (areq->volumeError == VOLMISSING)
	return ENODEV;
    if (areq->volumeError == VOLBUSY)
	return EWOULDBLOCK;
    if (acode == VNOVNODE)
	return ENOENT;
    return acode;

} /*afs_CheckCode*/


#define	VSleep(at)	afs_osi_Wait((at)*1000, 0, 0)


int lastcode;
/* returns:
 * 0   if the vldb record for a specific volume is different from what
 *     we have cached -- perhaps the volume has moved.
 * 1   if the vldb record is the same
 * 2   if we can't tell if it's the same or not. 
 *
 * If 0, the caller will probably start over at the beginning of our
 * list of servers for this volume and try to find one that is up.  If
 * not 0, we will probably just keep plugging with what we have
 * cached.   If we fail to contact the VL server, we  should just keep
 * trying with the information we have, rather than failing. */
#define DIFFERENT 0
#define SAME 1
#define DUNNO 2
static int VLDB_Same (afid, areq)
    struct VenusFid *afid;
    struct vrequest *areq;
{
    struct vrequest treq;
    struct conn *tconn;
    int i, type=0;
    union { 
      struct vldbentry tve;
      struct nvldbentry ntve;
      struct uvldbentry utve;
    } v;
    struct volume *tvp;
    struct cell *tcell;
    char *bp, tbuf[CVBS]; /* biggest volume id is 2^32, ~ 4*10^9 */
    unsigned int changed;
    struct server *(oldhosts[NMAXNSERVERS]);

    AFS_STATCNT(CheckVLDB);
    afs_FinalizeReq(areq);

    if (i = afs_InitReq(&treq, &afs_osi_cred)) return DUNNO;
    tcell = afs_GetCell(afid->Cell, READ_LOCK);
    bp = afs_cv2string(&tbuf[CVBS], afid->Fid.Volume);
    do {
        VSleep(2);	/* Better safe than sorry. */
	tconn = afs_ConnByMHosts(tcell->cellHosts, tcell->vlport,
				 tcell->cell, &treq, SHARED_LOCK);
	if (tconn) {
	    if (tconn->srvr->server->flags & SNO_LHOSTS) {
		type = 0;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		i = VL_GetEntryByNameO(tconn->id, bp, &v.tve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    } else if (tconn->srvr->server->flags & SYES_LHOSTS) {
		type = 1;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		i = VL_GetEntryByNameN(tconn->id, bp, &v.ntve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    } else {
		type = 2;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		i = VL_GetEntryByNameU(tconn->id, bp, &v.utve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		if (!(tconn->srvr->server->flags & SVLSRV_UUID)) {
		    if (i == RXGEN_OPCODE) {
			type = 1;
#ifdef RX_ENABLE_LOCKS
			AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
                        i = VL_GetEntryByNameN(tconn->id, bp, &v.ntve);
#ifdef RX_ENABLE_LOCKS
			AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
			if (i == RXGEN_OPCODE) {
			    type = 0;
			    tconn->srvr->server->flags |= SNO_LHOSTS;
#ifdef RX_ENABLE_LOCKS
			    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
			    i = VL_GetEntryByNameO(tconn->id, bp, &v.tve);
#ifdef RX_ENABLE_LOCKS
			    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
			} else if (!i)
			    tconn->srvr->server->flags |= SYES_LHOSTS;
		    } else if (!i)
			    tconn->srvr->server->flags |= SVLSRV_UUID;
		}
	    lastcode = i;
	    }
	} else
	    i = -1;
    } while (afs_Analyze(tconn, i, (struct VenusFid *) 0, &treq,
			 -1, /* no op code for this */
			 SHARED_LOCK, tcell));

    afs_PutCell(tcell, READ_LOCK);
    afs_Trace2(afs_iclSetp, CM_TRACE_CHECKVLDB, ICL_TYPE_FID, &afid,
	       ICL_TYPE_INT32, i);

    if (i) {
	return DUNNO;
    }
    /* have info, copy into serverHost array */
    changed = 0;
    tvp = afs_FindVolume(afid, WRITE_LOCK);
    if (tvp) {
       ObtainWriteLock(&tvp->lock,107);	
       for (i=0; i < NMAXNSERVERS && tvp->serverHost[i]; i++) {
	   oldhosts[i] = tvp->serverHost[i];
       }

       if (type == 2) {
	  InstallUVolumeEntry(tvp, &v.utve, afid->Cell, tcell, &treq);
       }
       else if (type == 1) {
	  InstallNVolumeEntry(tvp, &v.ntve, afid->Cell);
       }
       else {
	  InstallVolumeEntry(tvp, &v.tve, afid->Cell);
       }

       if (i < NMAXNSERVERS && tvp->serverHost[i]) {
	    changed = 1;
       }
       for (--i;!changed && i >= 0; i--) {
	  if (tvp->serverHost[i] != oldhosts[i]) {
	     changed = 1; /* also happens if prefs change.  big deal. */
	  }
       }

       ReleaseWriteLock(&tvp->lock);
       afs_PutVolume(tvp, WRITE_LOCK);
    }
    else {	/* can't find volume */
      tvp = afs_GetVolume(afid, &treq, WRITE_LOCK);
      if (tvp) {
	afs_PutVolume(tvp, WRITE_LOCK);
	return DIFFERENT;
      }
      else return DUNNO;
    }

    return (changed ? DIFFERENT : SAME);
} /*VLDB_Same */


/*------------------------------------------------------------------------
 * EXPORTED afs_Analyze
 *
 * Description:
 *	Analyze the outcome of an RPC operation, taking whatever support
 *	actions are necessary.
 *
 * Arguments:
 *	aconn : Ptr to the relevant connection on which the call was made.
 *	acode : The return code experienced by the RPC.
 *	afid  : The FID of the file involved in the action.  This argument
 *		may be null if none was involved.
 *	areq  : The request record associated with this operation.
 *      op    : which RPC we are analyzing.
 *      cellp : pointer to a cell struct.  Must provide either fid or cell.
 *
 * Returns:
 *	Non-zero value if the related RPC operation should be retried,
 *	zero otherwise.
 *
 * Environment:
 *	This routine is typically called in a do-while loop, causing the
 *	embedded RPC operation to be called repeatedly if appropriate
 *	until whatever error condition (if any) is intolerable.
 *
 * Side Effects:
 *	As advertised.
 *
 * NOTE:
 *	The retry return value is used by afs_StoreAllSegments to determine
 *	if this is a temporary or permanent error.
 *------------------------------------------------------------------------*/
int afs_Analyze(aconn, acode, afid, areq, op, locktype, cellp)
    register struct conn *aconn;
    afs_int32 acode;
    register struct vrequest *areq;
    struct VenusFid *afid;
    int op;
    afs_int32 locktype;
    struct cell *cellp;
{ /*afs_Analyze*/

   afs_int32 i, code;
   struct srvAddr *sa;
   struct server *tsp;
   struct volume *tvp;
   afs_int32 shouldRetry = 0;
   struct afs_stats_RPCErrors *aerrP;
   XSTATS_DECLS;

    AFS_STATCNT(afs_Analyze);
    afs_Trace4(afs_iclSetp, CM_TRACE_ANALYZE, ICL_TYPE_INT32, op,
	       ICL_TYPE_POINTER, aconn,
	       ICL_TYPE_INT32, acode, ICL_TYPE_LONG, areq->uid);

    aerrP = (struct afs_stats_RPCErrors *) 0;

    if ((op >= 0) && (op < AFS_STATS_NUM_FS_RPC_OPS))
      aerrP = &(afs_stats_cmfullperf.rpc.fsRPCErrors[op]);

    afs_FinalizeReq(areq);
    if (!aconn && areq->busyCount) { /* one RPC or more got VBUSY/VRESTARTING */

      tvp = afs_FindVolume(afid, READ_LOCK);
      if (tvp) {
	 afs_warnuser("afs: Waiting for busy volume %u (%s) in cell %s\n", 
		      (afid ? afid->Fid.Volume : 0),
		      (tvp->name ? tvp->name : ""),
		      ((tvp->serverHost[0] && tvp->serverHost[0]->cell) ?
		       tvp->serverHost[0]->cell->cellName : ""));

	 for (i=0; i < MAXHOSTS; i++) {
	    if (tvp->status[i] != not_busy && tvp->status[i] != offline) {
	       tvp->status[i] = not_busy; 
	    }
	    if (tvp->status[i] == not_busy)
		 shouldRetry = 1;
	 }
	 afs_PutVolume(tvp, READ_LOCK);
      } else {
	 afs_warnuser("afs: Waiting for busy volume %u\n", 
		      (afid ? afid->Fid.Volume : 0));
      }

      if (areq->busyCount > 100) {
	if (aerrP)
	  (aerrP->err_Volume)++;
	areq->volumeError = VOLBUSY;
	shouldRetry = 0;
      } else {
	VSleep(afs_BusyWaitPeriod);	    /* poll periodically */
      }
      return shouldRetry; /* should retry */
    }
	  
    if (!aconn) {
	if (!areq->volumeError) {
	    if (aerrP)
		(aerrP->err_Network)++;
	    if (hm_retry_int && !(areq->flags & O_NONBLOCK) &&  /* "hard" mount */
		((afid && afid->Cell == LOCALCELL) || 
		 (cellp && cellp->cell == LOCALCELL))) { 
		if (!afid) {
		    afs_warnuser("afs: hard-mount waiting for a vlserver to return to service\n");
		    VSleep(hm_retry_int);
		    afs_CheckServers(1,cellp);
		    shouldRetry=1;
		} else {
		    tvp = afs_FindVolume(afid, READ_LOCK);
		    if (!tvp || (tvp->states & VRO)) {
			   shouldRetry = hm_retry_RO;
		    } else { 
			   shouldRetry = hm_retry_RW;
		    }
		    if (tvp)
			afs_PutVolume(tvp, READ_LOCK);
		    if (shouldRetry) {
		        afs_warnuser("afs: hard-mount waiting for volume %u\n",
				 afid->Fid.Volume);
   		        VSleep(hm_retry_int);
		        afs_CheckServers(1,cellp);
		    }
		}
	    } /* if (hm_retry_int ... */
	    else {
		areq->networkError = 1;
	    }
	}
	return shouldRetry;
    }

    /* Find server associated with this connection. */
    sa = aconn->srvr;
    tsp = sa->server;

    if (acode == 0) {
       /* If we previously took an error, mark this volume not busy */
       if (areq->volumeError) {
	  tvp = afs_FindVolume(afid, READ_LOCK);
	  if (tvp) {
	     for (i=0; i<MAXHOSTS ; i++) {
	        if (tvp->serverHost[i] == tsp) {
		   tvp->status[i] = not_busy ;
		}
	     }
	     afs_PutVolume(tvp, READ_LOCK);
	  }
       }

       afs_PutConn(aconn, locktype);
       return 0;
    }

    /* If network troubles, mark server as having bogued out again. */
    /* VRESTARTING is < 0 because of backward compatibility issues 
     * with 3.4 file servers and older cache managers */
    if ((acode < 0) && (acode != VRESTARTING)) { 
	afs_ServerDown(sa);
	ForceNewConnections(sa); /*multi homed clients lock:afs_xsrvAddr?*/
	if (aerrP)
	    (aerrP->err_Server)++;
    }

    if (acode == VBUSY || acode == VRESTARTING) {
	if (acode == VBUSY) {
	  areq->busyCount++;
	  if (aerrP)
	    (aerrP->err_VolumeBusies)++;
	}
	else areq->busyCount = 1;

	tvp = afs_FindVolume(afid, READ_LOCK);
	if (tvp) {
	  for (i=0; i < MAXHOSTS ; i++ ) {
	    if (tvp->serverHost[i] == tsp) {
	      tvp->status[i] = rdwr_busy ; /* can't tell which yet */
	      /* to tell which, have to look at the op code. */
	    }
	  }
	  afs_PutVolume(tvp, READ_LOCK);
	}
	else {
	  afs_warnuser("afs: Waiting for busy volume %u in cell %s\n",
		       (afid? afid->Fid.Volume : 0), tsp->cell->cellName);
	  VSleep(afs_BusyWaitPeriod);	    /* poll periodically */
	}
	shouldRetry = 1;
	acode = 0;
    }
    else if (acode == VICETOKENDEAD || (acode & ~0xff) == ERROR_TABLE_BASE_rxk) {
	/* any rxkad error is treated as token expiration */
	struct unixuser *tu;

	/*
	 * I'm calling these errors protection errors, since they involve
	 * faulty authentication.
	 */
	if (aerrP)
	    (aerrP->err_Protection)++;

	tu = afs_FindUser(areq->uid, tsp->cell->cell, READ_LOCK);
	if (tu) {
	    if ((acode == VICETOKENDEAD) || (acode == RXKADEXPIRED))
		afs_warnuser("afs: Tokens for user of AFS id %d for cell %s have expired\n", 
			tu->vid, aconn->srvr->server->cell->cellName);
	    else
		afs_warnuser("afs: Tokens for user of AFS id %d for cell %s are discarded (rxkad error=%d)\n", 
			tu->vid, aconn->srvr->server->cell->cellName, acode);
	    afs_PutUser(tu, READ_LOCK);	
	} else {
	    /* The else case shouldn't be possible and should probably be replaced by a panic? */
	    if ((acode == VICETOKENDEAD) || (acode == RXKADEXPIRED))
		afs_warnuser("afs: Tokens for user %d for cell %s have expired\n", 
			areq->uid, aconn->srvr->server->cell->cellName);
	    else
		afs_warnuser("afs: Tokens for user %d for cell %s are discarded (rxkad error = %d)\n", 
			areq->uid, aconn->srvr->server->cell->cellName, acode);
	}
	aconn->forceConnectFS = 0;	 /* don't check until new tokens set */
	aconn->user->states |= UTokensBad;
	shouldRetry = 1;		     	/* Try again (as root). */
    }
    /* Check for access violation. */
    else if (acode == EACCES) {
	/* should mark access error in non-existent per-user global structure */
	if (aerrP)
	    (aerrP->err_Protection)++;
	areq->accessError = 1;
	if (op == AFS_STATS_FS_RPCIDX_STOREDATA)
	    areq->permWriteError = 1;
	shouldRetry = 0;
    }
    /* check for ubik errors; treat them like crashed servers */
    else if (acode >= ERROR_TABLE_BASE_u && acode < ERROR_TABLE_BASE_u+255) {
	afs_ServerDown(sa);
	if (aerrP)
	    (aerrP->err_Server)++;
	shouldRetry = 1;		 /* retryable (maybe one is working) */
	VSleep(1);			/* just in case */
    }
    /* Check for bad volume data base / missing volume. */
    else if (acode == VSALVAGE || acode == VOFFLINE 
	     || acode == VNOVOL || acode == VNOSERVICE || acode == VMOVED) {
	struct cell *tcell;
	int same; 

	shouldRetry = 1;
	areq->volumeError = VOLMISSING;
	if (aerrP)
	     (aerrP->err_Volume)++;
	if (afid && (tcell = afs_GetCell(afid->Cell, 0))) {
	   same = VLDB_Same(afid, areq);
	   tvp = afs_FindVolume(afid, READ_LOCK);
	   if (tvp) {
	      for (i=0; i < MAXHOSTS && tvp->serverHost[i]; i++ ) {
		 if (tvp->serverHost[i] == tsp) {
		    if (tvp->status[i] == end_not_busy)
		       tvp->status[i] = offline ;
		    else
		       tvp->status[i]++;
		 }
		 else if (!same) {
		    tvp->status[i] = not_busy; /* reset the others */
		 }
	      }
	      afs_PutVolume(tvp, READ_LOCK);
	   }
	}
     }
    else if (acode >= ERROR_TABLE_BASE_vl
	     && acode <= ERROR_TABLE_BASE_vl + 255) /* vlserver errors */ {
       shouldRetry = 0;
       areq->volumeError = VOLMISSING;
    }
    else if (acode >= 0) {
	if (aerrP)
	    (aerrP->err_Other)++;
	if (op == AFS_STATS_FS_RPCIDX_STOREDATA)
	    areq->permWriteError = 1;
	shouldRetry = 0;		/* Other random Vice error. */
    } else if (acode == RX_MSGSIZE) {   /* same meaning as EMSGSIZE... */
	VSleep(1);		   /* Just a hack for desperate times. */
	if (aerrP)
	    (aerrP->err_Other)++;
	shouldRetry = 1;	   /* packet was too big, please retry call */
    } 

    if (acode < 0  && acode != RX_MSGSIZE && acode != VRESTARTING) {
      	/* If we get here, code < 0 and we have network/Server troubles.
	 * areq->networkError is not set here, since we always
	 * retry in case there is another server.  However, if we find
	 * no connection (aconn == 0) we set the networkError flag.
	 */
	afs_MarkServerUpOrDown(sa, SRVR_ISDOWN);
	if (aerrP)
	    (aerrP->err_Server)++;
	VSleep(1);		/* Just a hack for desperate times. */
	shouldRetry = 1;
    }
    
    /* now unlock the connection and return */
    afs_PutConn(aconn, locktype);
    return (shouldRetry);
} /*afs_Analyze*/

