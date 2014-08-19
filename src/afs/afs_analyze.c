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
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#ifndef UKERNEL
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV)
#include <net/if.h>
#include <netinet/in.h>
#endif

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV) && !defined(AFS_DARWIN_ENV)
#include <netinet/in_var.h>
#endif
#endif /* !UKERNEL */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/afs_util.h"
#include "afs/unified_afs.h"

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif


/* shouldn't do it this way, but for now will do */
#ifndef ERROR_TABLE_BASE_U
#define ERROR_TABLE_BASE_U	(5376L)
#endif /* ubik error base define */

/* shouldn't do it this way, but for now will do */
#ifndef ERROR_TABLE_BASE_uae
#define ERROR_TABLE_BASE_uae	(49733376L)
#endif /* unified afs error base define */

/* same hack for vlserver error base as for ubik error base */
#ifndef ERROR_TABLE_BASE_VL
#define ERROR_TABLE_BASE_VL	(363520L)
#define VL_NOENT		(363524L)
#endif /* vlserver error base define */


int afs_BusyWaitPeriod = 15;	/* poll every 15 seconds */

afs_int32 hm_retry_RO = 0;	/* don't wait */
afs_int32 hm_retry_RW = 0;	/* don't wait */
afs_int32 hm_retry_int = 0;	/* don't wait */

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
static int
VLDB_Same(struct VenusFid *afid, struct vrequest *areq)
{
    struct vrequest *treq = NULL;
    struct afs_conn *tconn;
    int i, type = 0;
    union {
	struct vldbentry tve;
	struct nvldbentry ntve;
	struct uvldbentry utve;
    } *v;
    struct volume *tvp;
    struct cell *tcell;
    char *bp, tbuf[CVBS];	/* biggest volume id is 2^32, ~ 4*10^9 */
    unsigned int changed;
    struct server *(oldhosts[NMAXNSERVERS]);
    struct rx_connection *rxconn;

    AFS_STATCNT(CheckVLDB);
    afs_FinalizeReq(areq);

    if ((i = afs_CreateReq(&treq, afs_osi_credp)))
	return DUNNO;
    v = afs_osi_Alloc(sizeof(*v));
    osi_Assert(v != NULL);
    tcell = afs_GetCell(afid->Cell, READ_LOCK);
    bp = afs_cv2string(&tbuf[CVBS], afid->Fid.Volume);
    do {
	VSleep(2);		/* Better safe than sorry. */
	tconn =
	    afs_ConnByMHosts(tcell->cellHosts, tcell->vlport, tcell->cellNum,
			     treq, SHARED_LOCK, 0, &rxconn);
	if (tconn) {
	    if (tconn->srvr->server->flags & SNO_LHOSTS) {
		type = 0;
		RX_AFS_GUNLOCK();
		i = VL_GetEntryByNameO(rxconn, bp, &v->tve);
		RX_AFS_GLOCK();
	    } else if (tconn->srvr->server->flags & SYES_LHOSTS) {
		type = 1;
		RX_AFS_GUNLOCK();
		i = VL_GetEntryByNameN(rxconn, bp, &v->ntve);
		RX_AFS_GLOCK();
	    } else {
		type = 2;
		RX_AFS_GUNLOCK();
		i = VL_GetEntryByNameU(rxconn, bp, &v->utve);
		RX_AFS_GLOCK();
		if (!(tconn->srvr->server->flags & SVLSRV_UUID)) {
		    if (i == RXGEN_OPCODE) {
			type = 1;
			RX_AFS_GUNLOCK();
			i = VL_GetEntryByNameN(rxconn, bp, &v->ntve);
			RX_AFS_GLOCK();
			if (i == RXGEN_OPCODE) {
			    type = 0;
			    tconn->srvr->server->flags |= SNO_LHOSTS;
			    RX_AFS_GUNLOCK();
			    i = VL_GetEntryByNameO(rxconn, bp, &v->tve);
			    RX_AFS_GLOCK();
			} else if (!i)
			    tconn->srvr->server->flags |= SYES_LHOSTS;
		    } else if (!i)
			tconn->srvr->server->flags |= SVLSRV_UUID;
		}
		lastcode = i;
	    }
	} else
	    i = -1;
    } while (afs_Analyze(tconn, rxconn, i, NULL, treq, -1,	/* no op code for this */
			 SHARED_LOCK, tcell));

    afs_PutCell(tcell, READ_LOCK);
    afs_Trace2(afs_iclSetp, CM_TRACE_CHECKVLDB, ICL_TYPE_FID, &afid,
	       ICL_TYPE_INT32, i);

    if (i) {
	afs_DestroyReq(treq);
	afs_osi_Free(v, sizeof(*v));
	return DUNNO;
    }
    /* have info, copy into serverHost array */
    changed = 0;
    tvp = afs_FindVolume(afid, WRITE_LOCK);
    if (tvp) {
	ObtainWriteLock(&tvp->lock, 107);
	for (i = 0; i < NMAXNSERVERS && tvp->serverHost[i]; i++) {
	    oldhosts[i] = tvp->serverHost[i];
	}
	ReleaseWriteLock(&tvp->lock);

	if (type == 2) {
	    LockAndInstallUVolumeEntry(tvp, &v->utve, afid->Cell, tcell, treq);
	} else if (type == 1) {
	    LockAndInstallNVolumeEntry(tvp, &v->ntve, afid->Cell);
	} else {
	    LockAndInstallVolumeEntry(tvp, &v->tve, afid->Cell);
	}

	if (i < NMAXNSERVERS && tvp->serverHost[i]) {
	    changed = 1;
	}
	for (--i; !changed && i >= 0; i--) {
	    if (tvp->serverHost[i] != oldhosts[i]) {
		changed = 1;	/* also happens if prefs change.  big deal. */
	    }
	}

	ReleaseWriteLock(&tvp->lock);
	afs_PutVolume(tvp, WRITE_LOCK);
    } else {			/* can't find volume */
	tvp = afs_GetVolume(afid, treq, WRITE_LOCK);
	if (tvp) {
	    afs_PutVolume(tvp, WRITE_LOCK);
	    afs_DestroyReq(treq);
	    afs_osi_Free(v, sizeof(*v));
	    return DIFFERENT;
	} else {
	    afs_DestroyReq(treq);
	    afs_osi_Free(v, sizeof(*v));
	    return DUNNO;
	}
    }

    afs_DestroyReq(treq);
    afs_osi_Free(v, sizeof(*v));
    return (changed ? DIFFERENT : SAME);
}				/*VLDB_Same */

/*------------------------------------------------------------------------
 * afs_BlackListOnce
 *
 * Description:
 *	Mark a server as invalid for further attempts of this request only.
 *
 * Arguments:
 *	areq  : The request record associated with this operation.
 *	afid  : The FID of the file involved in the action.  This argument
 *		may be null if none was involved.
 *      tsp   : pointer to a server struct for the server we wish to
 *              blacklist.
 *
 * Returns:
 *	Non-zero value if further servers are available to try,
 *	zero otherwise.
 *
 * Environment:
 *	This routine is typically called in situations where we believe
 *      one server out of a pool may have an error condition.
 *
 * Side Effects:
 *	As advertised.
 *
 * NOTE:
 *	The afs_Conn* routines use the list of invalidated servers to
 *      avoid reusing a server marked as invalid for this request.
 *------------------------------------------------------------------------*/
static afs_int32
afs_BlackListOnce(struct vrequest *areq, struct VenusFid *afid,
		  struct server *tsp)
{
    struct volume *tvp;
    afs_int32 i;
    afs_int32 serversleft = 0;

    if (afid) {
	tvp = afs_FindVolume(afid, READ_LOCK);
	if (tvp) {
	    for (i = 0; i < AFS_MAXHOSTS; i++) {
		if (tvp->serverHost[i] == tsp) {
		    areq->skipserver[i] = 1;
		}
		if (tvp->serverHost[i] &&
		    (tvp->serverHost[i]->addr->sa_flags &
		      SRVR_ISDOWN)) {
		    areq->skipserver[i] = 1;
		}
	    }
	    for (i = 0; i < AFS_MAXHOSTS; i++) {
	        if (tvp->serverHost[i] && areq->skipserver[i] == 0) {
		    serversleft = 1;
		    break;
		}
	    }
	    afs_PutVolume(tvp, READ_LOCK);
	    return serversleft;
	}
    }
    return serversleft;
}

/*------------------------------------------------------------------------
 * afs_ClearStatus
 *
 * Description:
 *	Analyze the outcome of an RPC operation, taking whatever support
 *	actions are necessary.
 *
 * Arguments:
 *	afid  : The FID of the file involved in the action.  This argument
 *		may be null if none was involved.
 *      op    : which RPC we are analyzing.
 *      avp   : A pointer to the struct volume, if we already have one.
 *
 * Returns:
 *	Non-zero value if the related RPC operation can be retried,
 *	zero otherwise.
 *
 * Environment:
 *	This routine is called when we got a network error,
 *      and discards state if the operation was a data-mutating
 *      operation.
 *------------------------------------------------------------------------*/
static int
afs_ClearStatus(struct VenusFid *afid, int op, struct volume *avp)
{
    struct volume *tvp = NULL;

    /* if it's not a write op, we have nothing to veto and shouldn't clear. */
    if (!AFS_STATS_FS_RPCIDXES_ISWRITE(op)) {
	return 1;
    }

    if (avp)
	tvp = avp;
    else if (afid)
	tvp = afs_FindVolume(afid, READ_LOCK);

    /* don't assume just discarding will fix if no cached volume */
    if (tvp) {
	struct vcache *tvc;
	ObtainReadLock(&afs_xvcache);
	if ((tvc = afs_FindVCache(afid, 0, 0))) {
	    ReleaseReadLock(&afs_xvcache);
	    tvc->f.states &= ~(CStatd | CUnique);
	    afs_PutVCache(tvc);
	} else {
	    ReleaseReadLock(&afs_xvcache);
	}
	if (!avp)
	    afs_PutVolume(tvp, READ_LOCK);
    }

    if (AFS_STATS_FS_RPCIDXES_WRITE_RETRIABLE(op))
	return 1;

    /* not retriable: we may have raced ourselves */
    return 0;
}

/*!
 * \brief
 *      Print the last errors from the servers for the volume on
 *      this request.
 *
 * \param[in] areq   The request record associated with this operation.
 * \param[in] afid   The FID of the file involved in the action.  This argument
 *		     may be null if none was involved.
 *
 * \return
 *      None
 *
 * \note
 *      This routine is called before a hard-mount retry, to display
 *      the servers by primary address and the errors encountered.
 */
static void
afs_PrintServerErrors(struct vrequest *areq, struct VenusFid *afid)
{
    int i;
    struct volume *tvp;
    struct srvAddr *sa;
    afs_uint32 address;
    char *sep = " (";
    char *term = "";

    if (afid) {
	tvp = afs_FindVolume(afid, READ_LOCK);
	if (tvp) {
	    for (i = 0; i < AFS_MAXHOSTS; i++) {
		if (areq->lasterror[i] && tvp->serverHost[i]) {
		    sa = tvp->serverHost[i]->addr;
		    if (sa) {
			address = ntohl(sa->sa_ip);
			afs_warnuser("%s%d.%d.%d.%d code=%d", sep,
				     (address >> 24), (address >> 16) & 0xff,
				     (address >> 8) & 0xff, (address) & 0xff,
				     areq->lasterror[i]);
			sep = ", ";
			term = ")";
		    }
		}
	    }
	    afs_PutVolume(tvp, READ_LOCK);
	}
    }
    afs_warnuser("%s\n", term);
}

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
int
afs_Analyze(struct afs_conn *aconn, struct rx_connection *rxconn,
	    afs_int32 acode,
	    struct VenusFid *afid, struct vrequest *areq, int op,
	    afs_int32 locktype, struct cell *cellp)
{
    afs_int32 i;
    struct srvAddr *sa;
    struct server *tsp;
    struct volume *tvp = NULL;
    afs_int32 shouldRetry = 0;
    afs_int32 serversleft = 1;
    struct afs_stats_RPCErrors *aerrP;
    afs_uint32 address;

    if (AFS_IS_DISCONNECTED && !AFS_IN_SYNC) {
	/* On reconnection, act as connected. XXX: for now.... */
        /* SXW - This may get very tired after a while. We should try and
	 *       intercept all RPCs before they get here ... */
	/*printf("afs_Analyze: disconnected\n");*/
	afs_FinalizeReq(areq);
	if (aconn) {
	    /* SXW - I suspect that this will _never_ happen - we shouldn't
	     *       get a connection because we're disconnected !!!*/
	    afs_PutConn(aconn, rxconn, locktype);
	}
	return 0;
    }

    AFS_STATCNT(afs_Analyze);
    afs_Trace4(afs_iclSetp, CM_TRACE_ANALYZE, ICL_TYPE_INT32, op,
	       ICL_TYPE_POINTER, aconn, ICL_TYPE_INT32, acode, ICL_TYPE_LONG,
	       areq->uid);

    aerrP = (struct afs_stats_RPCErrors *)0;

    if ((op >= 0) && (op < AFS_STATS_NUM_FS_RPC_OPS))
	aerrP = &(afs_stats_cmfullperf.rpc.fsRPCErrors[op]);

    afs_FinalizeReq(areq);
    if (!aconn && areq->busyCount) {	/* one RPC or more got VBUSY/VRESTARTING */

	tvp = afs_FindVolume(afid, READ_LOCK);
	if (tvp) {
	    afs_warnuser("afs: Waiting for busy volume %u (%s) in cell %s\n",
			 (afid ? afid->Fid.Volume : 0),
			 (tvp->name ? tvp->name : ""),
			 ((tvp->serverHost[0]
			   && tvp->serverHost[0]->cell) ? tvp->serverHost[0]->
			  cell->cellName : ""));

	    for (i = 0; i < AFS_MAXHOSTS; i++) {
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
	    VSleep(afs_BusyWaitPeriod);	/* poll periodically */
	}
	if (shouldRetry != 0)
	    areq->busyCount++;

	return shouldRetry;	/* should retry */
    }

    if (!aconn || !aconn->srvr) {
	if (!areq->volumeError) {
	    if (aerrP)
		(aerrP->err_Network)++;
	    if (hm_retry_int && !(areq->flags & O_NONBLOCK) &&	/* "hard" mount */
		((afid && afs_IsPrimaryCellNum(afid->Cell))
		 || (cellp && afs_IsPrimaryCell(cellp)))) {
		if (!afid) {
		    static int afs_vl_hm = 0;
		    int warn = 0;
		    if (!afs_vl_hm) {
			afs_vl_hm = warn = 1;
		    }
		    if (warn) {
			afs_warnuser
		            ("afs: hard-mount waiting for a vlserver to return to service\n");
		    }
		    VSleep(hm_retry_int);
		    afs_CheckServers(1, cellp);
		    shouldRetry = 1;

		    if (warn) {
			afs_vl_hm = 0;
		    }
		} else {
		    static int afs_unknown_vhm = 0;
		    int warn = 0, vp_vhm = 0;

		    tvp = afs_FindVolume(afid, READ_LOCK);
		    if (!tvp || (tvp->states & VRO)) {
			shouldRetry = hm_retry_RO;
		    } else {
			shouldRetry = hm_retry_RW;
		    }

		    /* Set 'warn' if we should afs_warnuser. Only let one
		     * caller call afs_warnuser per hm_retry_int interval per
		     * volume. */
		    if (shouldRetry) {
			if (tvp) {
			    if (!(tvp->states & VHardMount)) {
				tvp->states |= VHardMount;
				warn = vp_vhm = 1;
			    }
			} else {
			    if (!afs_unknown_vhm) {
				afs_unknown_vhm = 1;
				warn = 1;
			    }
			}
		    }

		    if (tvp)
			afs_PutVolume(tvp, READ_LOCK);

		    if (shouldRetry) {
			if (warn) {
			    afs_warnuser
			        ("afs: hard-mount waiting for volume %u",
			         afid->Fid.Volume);
			    afs_PrintServerErrors(areq, afid);
			}

			VSleep(hm_retry_int);
			afs_CheckServers(1, cellp);
			/* clear the black listed servers on this request. */
			memset(areq->skipserver, 0, sizeof(areq->skipserver));

			if (vp_vhm) {
			    tvp = afs_FindVolume(afid, READ_LOCK);
			    if (tvp) {
				tvp->states &= ~VHardMount;
				afs_PutVolume(tvp, READ_LOCK);
			    }
			} else if (warn) {
			    afs_unknown_vhm = 0;
			}
		    }
		}
	    } /* if (hm_retry_int ... */
	    else {
		if (acode == RX_MSGSIZE)
		    shouldRetry = 1;
		else {
		    areq->networkError = 1;
		    /* do not promote to shouldRetry if not already */
		    if (afs_ClearStatus(afid, op, NULL) == 0)
			shouldRetry = 0;
		}
	    }
	}
	if (aconn) /* simply lacking aconn->server doesn't absolve this */
	    afs_PutConn(aconn, rxconn, locktype);
	return shouldRetry;
    }

    /* Find server associated with this connection. */
    sa = aconn->srvr;
    tsp = sa->server;
    address = ntohl(sa->sa_ip);

    /* Before we do anything with acode, make sure we translate it back to
     * a system error */
    if ((acode & ~0xff) == ERROR_TABLE_BASE_uae)
	acode = et_to_sys_error(acode);

    if (acode == 0) {
	/* If we previously took an error, mark this volume not busy */
	if (areq->volumeError) {
	    tvp = afs_FindVolume(afid, READ_LOCK);
	    if (tvp) {
		for (i = 0; i < AFS_MAXHOSTS; i++) {
		    if (tvp->serverHost[i] == tsp) {
			tvp->status[i] = not_busy;
		    }
		}
		afs_PutVolume(tvp, READ_LOCK);
	    }
	}

	afs_PutConn(aconn, rxconn, locktype);
	return 0;
    }

    /* Save the last code of this server on this request. */
    tvp = afs_FindVolume(afid, READ_LOCK);
    if (tvp) {
	for (i = 0; i < AFS_MAXHOSTS; i++) {
	    if (tvp->serverHost[i] == tsp) {
		areq->lasterror[i] = acode;
	    }
	}
	afs_PutVolume(tvp, READ_LOCK);
    }

#ifdef AFS_64BIT_CLIENT
    if (acode == -455)
	acode = 455;
#endif /* AFS_64BIT_CLIENT */
    if (acode == RX_MSGSIZE || acode == RX_CALL_BUSY) {
	shouldRetry = 1;
	goto out;
    }
    if (acode == RX_CALL_TIMEOUT || acode == RX_CALL_IDLE || acode == VNOSERVICE) {
	serversleft = afs_BlackListOnce(areq, afid, tsp);
	if (afid)
	    tvp = afs_FindVolume(afid, READ_LOCK);
	if ((serversleft == 0) && tvp &&
	    ((tvp->states & VRO) || (tvp->states & VBackup))) {
	    shouldRetry = 0;
	} else {
	    shouldRetry = 1;
	}
	if (!afid || !tvp || (tvp->states & VRO))
	    areq->idleError++;
	else if (afs_ClearStatus(afid, op, tvp) == 0)
	    shouldRetry = 0;

	if (tvp)
	    afs_PutVolume(tvp, READ_LOCK);
	/* By doing this, we avoid ever marking a server down
	 * in an idle timeout case. That's because the server is
	 * still responding and may only be letting a single vnode
	 * time out. We otherwise risk having the server continually
	 * be marked down, then up, then down again...
	 */
	goto out;
    }
    /* If network troubles, mark server as having bogued out again. */
    /* VRESTARTING is < 0 because of backward compatibility issues
     * with 3.4 file servers and older cache managers */
    if ((acode < 0) && (acode != VRESTARTING)) {
	afs_ServerDown(sa, acode);
	ForceNewConnections(sa); /**multi homed clients lock:afs_xsrvAddr? */
	if (aerrP)
	    (aerrP->err_Server)++;
#if 0
	/* retry *once* when the server is timed out in case of NAT */
	if (markeddown && acode == RX_CALL_DEAD) {
	    aconn->forceConnectFS = 1;
	    shouldRetry = 1;
	}
#endif
    }

    if (acode == VBUSY || acode == VRESTARTING) {
	if (acode == VBUSY) {
	    areq->busyCount++;
	    if (aerrP)
		(aerrP->err_VolumeBusies)++;
	} else
	    areq->busyCount = 1;

	tvp = afs_FindVolume(afid, READ_LOCK);
	if (tvp) {
	    for (i = 0; i < AFS_MAXHOSTS; i++) {
		if (tvp->serverHost[i] == tsp) {
		    tvp->status[i] = rdwr_busy;	/* can't tell which yet */
		    /* to tell which, have to look at the op code. */
		}
	    }
	    afs_PutVolume(tvp, READ_LOCK);
	} else {
	    afs_warnuser("afs: Waiting for busy volume %u in cell %s (server %d.%d.%d.%d)\n",
			 (afid ? afid->Fid.Volume : 0), tsp->cell->cellName,
			 (address >> 24), (address >> 16) & 0xff,
			 (address >> 8) & 0xff, (address) & 0xff);
	    VSleep(afs_BusyWaitPeriod);	/* poll periodically */
	}
	shouldRetry = 1;
	acode = 0;
    } else if (acode == VICETOKENDEAD
	       || (acode & ~0xff) == ERROR_TABLE_BASE_RXK) {
	/* any rxkad error is treated as token expiration */
	struct unixuser *tu;
	/*
	 * I'm calling these errors protection errors, since they involve
	 * faulty authentication.
	 */
	if (aerrP)
	    (aerrP->err_Protection)++;

	tu = afs_FindUser(areq->uid, tsp->cell->cellNum, READ_LOCK);
	if (tu) {
	    if (acode == VICETOKENDEAD) {
		aconn->forceConnectFS = 1;
	    } else if (acode == RXKADEXPIRED) {
		aconn->forceConnectFS = 0;	/* don't check until new tokens set */
		aconn->user->states |= UTokensBad;
		afs_NotifyUser(tu, UTokensDropped);
		afs_warnuser
		    ("afs: Tokens for user of AFS id %d for cell %s have expired (server %d.%d.%d.%d)\n",
		     tu->vid, aconn->srvr->server->cell->cellName,
		     (address >> 24), (address >> 16) & 0xff,
		     (address >> 8) & 0xff, (address) & 0xff);
	    } else {
		serversleft = afs_BlackListOnce(areq, afid, tsp);
		areq->tokenError++;

		if (serversleft) {
		    afs_warnuser
			("afs: Tokens for user of AFS id %d for cell %s: rxkad error=%d (server %d.%d.%d.%d)\n",
			 tu->vid, aconn->srvr->server->cell->cellName, acode,
			 (address >> 24), (address >> 16) & 0xff,
			 (address >> 8) & 0xff, (address) & 0xff);
		    shouldRetry = 1;
		} else {
		    areq->tokenError = 0;
		    aconn->forceConnectFS = 0;	/* don't check until new tokens set */
		    aconn->user->states |= UTokensBad;
		    afs_NotifyUser(tu, UTokensDropped);
		    afs_warnuser
			("afs: Tokens for user of AFS id %d for cell %s are discarded (rxkad error=%d, server %d.%d.%d.%d)\n",
			 tu->vid, aconn->srvr->server->cell->cellName, acode,
			 (address >> 24), (address >> 16) & 0xff,
			 (address >> 8) & 0xff, (address) & 0xff);
		}
	    }
	    afs_PutUser(tu, READ_LOCK);
	} else {
	    /* The else case shouldn't be possible and should probably be replaced by a panic? */
	    if (acode == VICETOKENDEAD) {
		aconn->forceConnectFS = 1;
	    } else if (acode == RXKADEXPIRED) {
		aconn->forceConnectFS = 0;	/* don't check until new tokens set */
		aconn->user->states |= UTokensBad;
		afs_NotifyUser(tu, UTokensDropped);
		afs_warnuser
		    ("afs: Tokens for user %d for cell %s have expired (server %d.%d.%d.%d)\n",
		     areq->uid, aconn->srvr->server->cell->cellName,
		     (address >> 24), (address >> 16) & 0xff,
		     (address >> 8) & 0xff, (address) & 0xff);
	    } else {
		aconn->forceConnectFS = 0;	/* don't check until new tokens set */
		aconn->user->states |= UTokensBad;
		afs_NotifyUser(tu, UTokensDropped);
		afs_warnuser
		    ("afs: Tokens for user %d for cell %s are discarded (rxkad error = %d, server %d.%d.%d.%d)\n",
		     areq->uid, aconn->srvr->server->cell->cellName, acode,
		     (address >> 24), (address >> 16) & 0xff,
		     (address >> 8) & 0xff, (address) & 0xff);

	    }
	}
	shouldRetry = 1;	/* Try again (as root). */
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
    else if (acode >= ERROR_TABLE_BASE_U && acode < ERROR_TABLE_BASE_U + 255) {
	afs_ServerDown(sa, acode);
	if (aerrP)
	    (aerrP->err_Server)++;
	shouldRetry = 1;	/* retryable (maybe one is working) */
	VSleep(1);		/* just in case */
    }
    /* Check for bad volume data base / missing volume. */
    else if (acode == VSALVAGE || acode == VOFFLINE || acode == VNOVOL
	     || acode == VMOVED) {
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
		for (i = 0; i < AFS_MAXHOSTS && tvp->serverHost[i]; i++) {
		    if (tvp->serverHost[i] == tsp) {
			if (tvp->status[i] == end_not_busy)
			    tvp->status[i] = offline;
			else
			    tvp->status[i]++;
		    } else if (!same) {
			tvp->status[i] = not_busy;	/* reset the others */
		    }
		}
		afs_PutVolume(tvp, READ_LOCK);
	    }
	}
    } else if (acode >= ERROR_TABLE_BASE_VL && acode <= ERROR_TABLE_BASE_VL + 255) {	/* vlserver errors */
	shouldRetry = 0;
	areq->volumeError = VOLMISSING;
    } else if (acode >= 0) {
	if (aerrP)
	    (aerrP->err_Other)++;
	if (op == AFS_STATS_FS_RPCIDX_STOREDATA)
	    areq->permWriteError = 1;
	shouldRetry = 0;	/* Other random Vice error. */
    } else if (acode == RX_MSGSIZE) {	/* same meaning as EMSGSIZE... */
	afs_warnuser
	    ("afs: Path MTU may have been exceeded, retrying (server %d.%d.%d.%d)\n",
	     (address >> 24), (address >> 16) & 0xff,
	     (address >> 8) & 0xff, (address) & 0xff);

	VSleep(1);		/* Just a hack for desperate times. */
	if (aerrP)
	    (aerrP->err_Other)++;
	shouldRetry = 1;	/* packet was too big, please retry call */
    }

    if (acode < 0 && acode != RX_MSGSIZE && acode != VRESTARTING) {
	/* If we get here, code < 0 and we have network/Server troubles.
	 * areq->networkError is not set here, since we always
	 * retry in case there is another server.  However, if we find
	 * no connection (aconn == 0) we set the networkError flag.
	 */
	afs_ServerDown(sa, acode);
	if (aerrP)
	    (aerrP->err_Server)++;
	VSleep(1);		/* Just a hack for desperate times. */
	shouldRetry = 1;
    }
out:
    /* now unlock the connection and return */
    afs_PutConn(aconn, rxconn, locktype);
    return (shouldRetry);
}				/*afs_Analyze */
