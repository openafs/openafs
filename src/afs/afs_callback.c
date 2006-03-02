/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_callback.c:
 *	Exported routines (and their private support) to implement
 *	the callback RPC interface.
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs/afs_stats.h"	/*Cache Manager stats */
#include "afs/afs_args.h"

afs_int32 afs_allCBs = 0;	/*Break callbacks on all objects */
afs_int32 afs_oddCBs = 0;	/*Break callbacks on dirs */
afs_int32 afs_evenCBs = 0;	/*Break callbacks received on files */
afs_int32 afs_allZaps = 0;	/*Objects entries deleted */
afs_int32 afs_oddZaps = 0;	/*Dir cache entries deleted */
afs_int32 afs_evenZaps = 0;	/*File cache entries deleted */
afs_int32 afs_connectBacks = 0;

/*
 * Some debugging aids.
 */
static struct ltable {
    char *name;
    char *addr;
} ltable[] = {
    {
    "afs_xvcache", (char *)&afs_xvcache}, {
    "afs_xdcache", (char *)&afs_xdcache}, {
    "afs_xserver", (char *)&afs_xserver}, {
    "afs_xvcb", (char *)&afs_xvcb}, {
    "afs_xbrs", (char *)&afs_xbrs}, {
    "afs_xcell", (char *)&afs_xcell}, {
    "afs_xconn", (char *)&afs_xconn}, {
    "afs_xuser", (char *)&afs_xuser}, {
    "afs_xvolume", (char *)&afs_xvolume}, {
    "puttofile", (char *)&afs_puttofileLock}, {
    "afs_ftf", (char *)&afs_ftf}, {
    "afs_xcbhash", (char *)&afs_xcbhash}, {
    "afs_xaxs", (char *)&afs_xaxs}, {
    "afs_xinterface", (char *)&afs_xinterface},
#ifndef UKERNEL
    {
    "afs_xosi", (char *)&afs_xosi},
#endif
    {
      "afs_xsrvAddr", (char *)&afs_xsrvAddr},
    {
    "afs_xvreclaim", (char *)&afs_xvreclaim}
};
unsigned long lastCallBack_vnode;
unsigned int lastCallBack_dv;
osi_timeval_t lastCallBack_time;

/* these are for storing alternate interface addresses */
struct interfaceAddr afs_cb_interface;

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCE
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the contents of the i'th cache entry.
 *
 * Arguments:
 *	a_call   : Ptr to Rx call on which this request came in.
 *	a_index  : Index of desired cache entry.
 *	a_result : Ptr to a buffer for the given cache entry.
 *
 * Returns:
 *	0 if everything went fine,
 *	1 if we were given a bad index.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetCE(struct rx_call *a_call, afs_int32 a_index,
	       struct AFSDBCacheEntry *a_result)
{

    register int i;		/*Loop variable */
    register struct vcache *tvc;	/*Ptr to current cache entry */
    int code;			/*Return code */
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETCE);

    AFS_STATCNT(SRXAFSCB_GetCE);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    if (a_index == 0)
		goto searchDone;
	    a_index--;
	}			/*Zip through current hash chain */
    }				/*Zip through hash chains */

  searchDone:
    if (tvc == NULL) {
	/*Past EOF */
	code = 1;
	goto fcnDone;
    }

    /*
     * Copy out the located entry.
     */
    a_result->addr = afs_data_pointer_to_int32(tvc);
    a_result->cell = tvc->fid.Cell;
    a_result->netFid.Volume = tvc->fid.Fid.Volume;
    a_result->netFid.Vnode = tvc->fid.Fid.Vnode;
    a_result->netFid.Unique = tvc->fid.Fid.Unique;
    a_result->lock.waitStates = tvc->lock.wait_states;
    a_result->lock.exclLocked = tvc->lock.excl_locked;
    a_result->lock.readersReading = tvc->lock.readers_reading;
    a_result->lock.numWaiting = tvc->lock.num_waiting;
#if defined(INSTRUMENT_LOCKS)
    a_result->lock.pid_last_reader = tvc->lock.pid_last_reader;
    a_result->lock.pid_writer = tvc->lock.pid_writer;
    a_result->lock.src_indicator = tvc->lock.src_indicator;
#else
    /* On osf20 , the vcache does not maintain these three fields */
    a_result->lock.pid_last_reader = 0;
    a_result->lock.pid_writer = 0;
    a_result->lock.src_indicator = 0;
#endif /* AFS_OSF20_ENV */
#ifdef AFS_64BIT_CLIENT
    a_result->Length = (afs_int32) tvc->m.Length & 0xffffffff;
#else /* AFS_64BIT_CLIENT */
    a_result->Length = tvc->m.Length;
#endif /* AFS_64BIT_CLIENT */
    a_result->DataVersion = hgetlo(tvc->m.DataVersion);
    a_result->callback = afs_data_pointer_to_int32(tvc->callback);	/* XXXX Now a pointer; change it XXXX */
    a_result->cbExpires = tvc->cbExpires;
    if (tvc->states & CVInit) {
        a_result->refCount = 1;
    } else {
#ifdef AFS_DARWIN80_ENV
    a_result->refCount = vnode_isinuse(AFSTOV(tvc),0)?1:0; /* XXX fix */
#else
    a_result->refCount = VREFCOUNT(tvc);
#endif
    }
    a_result->opens = tvc->opens;
    a_result->writers = tvc->execsOrWriters;
    a_result->mvstat = tvc->mvstat;
    a_result->states = tvc->states;
    code = 0;

    /*
     * Return our results.
     */
  fcnDone:
    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (code);

}				/*SRXAFSCB_GetCE */

int
SRXAFSCB_GetCE64(struct rx_call *a_call, afs_int32 a_index,
		 struct AFSDBCacheEntry64 *a_result)
{
    register int i;		/*Loop variable */
    register struct vcache *tvc;	/*Ptr to current cache entry */
    int code;			/*Return code */
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETCE);

    AFS_STATCNT(SRXAFSCB_GetCE64);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    if (a_index == 0)
		goto searchDone;
	    a_index--;
	}			/*Zip through current hash chain */
    }				/*Zip through hash chains */

  searchDone:
    if (tvc == NULL) {
	/*Past EOF */
	code = 1;
	goto fcnDone;
    }

    /*
     * Copy out the located entry.
     */
    a_result->addr = afs_data_pointer_to_int32(tvc);
    a_result->cell = tvc->fid.Cell;
    a_result->netFid.Volume = tvc->fid.Fid.Volume;
    a_result->netFid.Vnode = tvc->fid.Fid.Vnode;
    a_result->netFid.Unique = tvc->fid.Fid.Unique;
    a_result->lock.waitStates = tvc->lock.wait_states;
    a_result->lock.exclLocked = tvc->lock.excl_locked;
    a_result->lock.readersReading = tvc->lock.readers_reading;
    a_result->lock.numWaiting = tvc->lock.num_waiting;
#if defined(INSTRUMENT_LOCKS)
    a_result->lock.pid_last_reader = tvc->lock.pid_last_reader;
    a_result->lock.pid_writer = tvc->lock.pid_writer;
    a_result->lock.src_indicator = tvc->lock.src_indicator;
#else
    /* On osf20 , the vcache does not maintain these three fields */
    a_result->lock.pid_last_reader = 0;
    a_result->lock.pid_writer = 0;
    a_result->lock.src_indicator = 0;
#endif /* AFS_OSF20_ENV */
#if !defined(AFS_64BIT_ENV) 
    a_result->Length.high = 0;
    a_result->Length.low = tvc->m.Length;
#else
    a_result->Length = tvc->m.Length;
#endif
    a_result->DataVersion = hgetlo(tvc->m.DataVersion);
    a_result->callback = afs_data_pointer_to_int32(tvc->callback);	/* XXXX Now a pointer; change it XXXX */
    a_result->cbExpires = tvc->cbExpires;
    if (tvc->states & CVInit) {
        a_result->refCount = 1;
    } else {
#ifdef AFS_DARWIN80_ENV
    a_result->refCount = vnode_isinuse(AFSTOV(tvc),0)?1:0; /* XXX fix */
#else
    a_result->refCount = VREFCOUNT(tvc);
#endif
    }
    a_result->opens = tvc->opens;
    a_result->writers = tvc->execsOrWriters;
    a_result->mvstat = tvc->mvstat;
    a_result->states = tvc->states;
    code = 0;

    /*
     * Return our results.
     */
  fcnDone:
    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (code);

}				/*SRXAFSCB_GetCE64 */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLock
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the contents of a lock in the lock
 *	table.
 *
 * Arguments:
 *	a_call   : Ptr to Rx call on which this request came in.
 *	a_index  : Index of desired lock.
 *	a_result : Ptr to a buffer for the given lock.
 *
 * Returns:
 *	0 if everything went fine,
 *	1 if we were given a bad index.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetLock(struct rx_call *a_call, afs_int32 a_index,
		 struct AFSDBLock *a_result)
{
    struct ltable *tl;		/*Ptr to lock table entry */
    int nentries;		/*Num entries in table */
    int code;			/*Return code */
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETLOCK);

    AFS_STATCNT(SRXAFSCB_GetLock);
    nentries = sizeof(ltable) / sizeof(struct ltable);
    if (a_index < 0 || a_index >= nentries) {
	/*
	 * Past EOF
	 */
	code = 1;
    } else {
	/*
	 * Found it - copy out its contents.
	 */
	tl = &ltable[a_index];
	strcpy(a_result->name, tl->name);
	a_result->lock.waitStates =
	    ((struct afs_lock *)(tl->addr))->wait_states;
	a_result->lock.exclLocked =
	    ((struct afs_lock *)(tl->addr))->excl_locked;
	a_result->lock.readersReading =
	    ((struct afs_lock *)(tl->addr))->readers_reading;
	a_result->lock.numWaiting =
	    ((struct afs_lock *)(tl->addr))->num_waiting;
#ifdef INSTRUMENT_LOCKS
	a_result->lock.pid_last_reader =
	    ((struct afs_lock *)(tl->addr))->pid_last_reader;
	a_result->lock.pid_writer =
	    ((struct afs_lock *)(tl->addr))->pid_writer;
	a_result->lock.src_indicator =
	    ((struct afs_lock *)(tl->addr))->src_indicator;
#else
	a_result->lock.pid_last_reader = 0;
	a_result->lock.pid_writer = 0;
	a_result->lock.src_indicator = 0;
#endif
	code = 0;
    }

    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (code);

}				/*SRXAFSCB_GetLock */


/*------------------------------------------------------------------------
 * static ClearCallBack
 *
 * Description:
 *	Clear out callback information for the specified file, or
 *	even a whole volume.  Used to worry about callback was from 
 *      within the particular cell or not.  Now we don't bother with
 *      that anymore; it's not worth the time.
 *
 * Arguments:
 *	a_conn : Ptr to Rx connection involved.
 *	a_fid  : Ptr to AFS fid being cleared.
 *
 * Returns:
 *	0 (always)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.

Appears to need to be called with GLOCK held, as the icl_Event4 stuff asserts otherwise

 *------------------------------------------------------------------------*/

static int
ClearCallBack(register struct rx_connection *a_conn,
	      register struct AFSFid *a_fid)
{
    register struct vcache *tvc;
    register int i;
    struct VenusFid localFid;
    struct volume *tv;
#ifdef AFS_DARWIN80_ENV
    vnode_t vp;
#endif

    AFS_STATCNT(ClearCallBack);

    AFS_ASSERT_GLOCK();

    /*
     * XXXX Don't hold any server locks here because of callback protocol XXX
     */
    localFid.Cell = 0;
    localFid.Fid.Volume = a_fid->Volume;
    localFid.Fid.Vnode = a_fid->Vnode;
    localFid.Fid.Unique = a_fid->Unique;

    /*
     * Volume ID of zero means don't do anything.
     */
    if (a_fid->Volume != 0) {
	if (a_fid->Vnode == 0) {
		struct afs_q *tq, *uq;
	    /*
	     * Clear callback for the whole volume.  Zip through the
	     * hash chain, nullifying entries whose volume ID matches.
	     */
loop1:
		ObtainReadLock(&afs_xvcache);
		i = VCHashV(&localFid);
		for (tq = afs_vhashTV[i].prev; tq != &afs_vhashTV[i]; tq = uq) {
		    uq = QPrev(tq);
		    tvc = QTOVH(tq);      
		    if (tvc->fid.Fid.Volume == a_fid->Volume) {
			tvc->callback = NULL;
			if (!localFid.Cell)
			    localFid.Cell = tvc->fid.Cell;
			tvc->dchint = NULL;	/* invalidate hints */
			if (tvc->states & CVInit) {
			    ReleaseReadLock(&afs_xvcache);
			    afs_osi_Sleep(&tvc->states);
			    goto loop1;
			}
#ifdef AFS_DARWIN80_ENV
			if (tvc->states & CDeadVnode) {
			    ReleaseReadLock(&afs_xvcache);
			    afs_osi_Sleep(&tvc->states);
			    goto loop1;
			}
#endif
#if     defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)  || defined(AFS_SUN5_ENV)  || defined(AFS_HPUX_ENV) || defined(AFS_LINUX20_ENV)
			VN_HOLD(AFSTOV(tvc));
#else
#ifdef AFS_DARWIN80_ENV
			vp = AFSTOV(tvc);
			if (vnode_get(vp))
			    continue;
			if (vnode_ref(vp)) {
			    AFS_GUNLOCK();
			    vnode_put(vp);
			    AFS_GLOCK();
			    continue;
			}
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
			osi_vnhold(tvc, 0);
#else
			VREFCOUNT_INC(tvc); /* AIX, apparently */
#endif
#endif
#endif
			ReleaseReadLock(&afs_xvcache);
			ObtainWriteLock(&afs_xcbhash, 449);
			afs_DequeueCallback(tvc);
			tvc->states &= ~(CStatd | CUnique | CBulkFetching);
			afs_allCBs++;
			if (tvc->fid.Fid.Vnode & 1)
			    afs_oddCBs++;
			else
			    afs_evenCBs++;
			ReleaseWriteLock(&afs_xcbhash);
			if ((tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR)))
			    osi_dnlc_purgedp(tvc);
			afs_Trace3(afs_iclSetp, CM_TRACE_CALLBACK,
				   ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32,
				   tvc->states, ICL_TYPE_INT32,
				   a_fid->Volume);
#ifdef AFS_DARWIN80_ENV
			vnode_put(AFSTOV(tvc));
#endif
			ObtainReadLock(&afs_xvcache);
			uq = QPrev(tq);
			AFS_FAST_RELE(tvc);
		    } else if ((tvc->states & CMValid)
			       && (tvc->mvid->Fid.Volume == a_fid->Volume)) {
			tvc->states &= ~CMValid;
			if (!localFid.Cell)
			    localFid.Cell = tvc->mvid->Cell;
		    }
		}
		ReleaseReadLock(&afs_xvcache);

	    /*
	     * XXXX Don't hold any locks here XXXX
	     */
	    tv = afs_FindVolume(&localFid, 0);
	    if (tv) {
		afs_ResetVolumeInfo(tv);
		afs_PutVolume(tv, 0);
		/* invalidate mtpoint? */
	    }
	} /*Clear callbacks for whole volume */
	else {
	    /*
	     * Clear callbacks just for the one file.
	     */
	    struct vcache *uvc;
	    afs_allCBs++;
	    if (a_fid->Vnode & 1)
		afs_oddCBs++;	/*Could do this on volume basis, too */
	    else
		afs_evenCBs++;	/*A particular fid was specified */
loop2:
	    ObtainReadLock(&afs_xvcache);
	    i = VCHash(&localFid);
	    for (tvc = afs_vhashT[i]; tvc; tvc = uvc) {
		uvc = tvc->hnext;
		if (tvc->fid.Fid.Vnode == a_fid->Vnode
		    && tvc->fid.Fid.Volume == a_fid->Volume
		    && tvc->fid.Fid.Unique == a_fid->Unique) {
		    tvc->callback = NULL;
		    tvc->dchint = NULL;	/* invalidate hints */
		    if (tvc->states & CVInit) {
			ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->states);
			goto loop2;
		    }
#ifdef AFS_DARWIN80_ENV
		    if (tvc->states & CDeadVnode) {
			ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->states);
			goto loop2;
		    }
#endif
#if     defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)  || defined(AFS_SUN5_ENV)  || defined(AFS_HPUX_ENV) || defined(AFS_LINUX20_ENV)
		    VN_HOLD(AFSTOV(tvc));
#else
#ifdef AFS_DARWIN80_ENV
		    vp = AFSTOV(tvc);
		    if (vnode_get(vp))
			continue;
		    if (vnode_ref(vp)) {
			AFS_GUNLOCK();
			vnode_put(vp);
			AFS_GLOCK();
			continue;
		    }
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
		    osi_vnhold(tvc, 0);
#else
		    VREFCOUNT_INC(tvc); /* AIX, apparently */
#endif
#endif
#endif
		    ReleaseReadLock(&afs_xvcache);
		    ObtainWriteLock(&afs_xcbhash, 450);
		    afs_DequeueCallback(tvc);
		    tvc->states &= ~(CStatd | CUnique | CBulkFetching);
		    ReleaseWriteLock(&afs_xcbhash);
		    if ((tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR)))
			osi_dnlc_purgedp(tvc);
		    afs_Trace3(afs_iclSetp, CM_TRACE_CALLBACK,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32,
			       tvc->states, ICL_TYPE_LONG, 0);
#ifdef CBDEBUG
		    lastCallBack_vnode = afid->Vnode;
		    lastCallBack_dv = tvc->mstat.DataVersion.low;
		    osi_GetuTime(&lastCallBack_time);
#endif /* CBDEBUG */
#ifdef AFS_DARWIN80_ENV
		    vnode_put(AFSTOV(tvc));
#endif
		    ObtainReadLock(&afs_xvcache);
		    uvc = tvc->hnext;
		    AFS_FAST_RELE(tvc);
		}
	    }			/*Walk through hash table */
	    ReleaseReadLock(&afs_xvcache);
	}			/*Clear callbacks for one file */
    }

    /*Fid has non-zero volume ID */
    /*
     * Always return a predictable value.
     */
    return (0);

}				/*ClearCallBack */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_CallBack
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement passing in callback information.
 *	table.
 *
 * Arguments:
 *	a_call      : Ptr to Rx call on which this request came in.
 *	a_fids      : Ptr to array of fids involved.
 *	a_callbacks : Ptr to matching callback info for the fids.
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

int
SRXAFSCB_CallBack(struct rx_call *a_call, register struct AFSCBFids *a_fids,
		  struct AFSCBs *a_callbacks)
{
    register int i;		/*Loop variable */
    struct AFSFid *tfid;	/*Ptr to current fid */
    register struct rx_connection *tconn;	/*Call's connection */
    int code = 0;
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_CALLBACK);

    AFS_STATCNT(SRXAFSCB_CallBack);
    if (!(tconn = rx_ConnectionOf(a_call)))
	return (0);
    tfid = (struct AFSFid *)a_fids->AFSCBFids_val;

    /*
     * For now, we ignore callbacks, since the File Server only *breaks*
     * callbacks at present.
     */
    for (i = 0; i < a_fids->AFSCBFids_len; i++)
	ClearCallBack(tconn, &tfid[i]);

    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (0);

}				/*SRXAFSCB_CallBack */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_Probe
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement ``probing'' the Cache Manager, just making sure it's
 *	still there.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
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

int
SRXAFSCB_Probe(struct rx_call *a_call)
{
    int code = 0;
    XSTATS_DECLS;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_Probe);

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_PROBE);
    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (0);

}				/*SRXAFSCB_Probe */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement clearing all callbacks from this host.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
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

int
SRXAFSCB_InitCallBackState(struct rx_call *a_call)
{
    register int i;
    register struct vcache *tvc;
    register struct rx_connection *tconn;
    register struct rx_peer *peer;
    struct server *ts;
    int code = 0;
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_INITCALLBACKSTATE);
    AFS_STATCNT(SRXAFSCB_InitCallBackState);

    /*
     * Find the address of the host making this call
     */
    if ((tconn = rx_ConnectionOf(a_call)) && (peer = rx_PeerOf(tconn))) {

	afs_allCBs++;
	afs_oddCBs++;		/*Including any missed via create race */
	afs_evenCBs++;		/*Including any missed via create race */

	ts = afs_FindServer(rx_HostOf(peer), rx_PortOf(peer), (afsUUID *) 0,
			    0);
	if (ts) {
	    for (i = 0; i < VCSIZE; i++)
		for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
		    if (tvc->callback == ts) {
			ObtainWriteLock(&afs_xcbhash, 451);
			afs_DequeueCallback(tvc);
			tvc->callback = NULL;
			tvc->states &= ~(CStatd | CUnique | CBulkFetching);
			ReleaseWriteLock(&afs_xcbhash);
		    }
		}
	}



	/* find any volumes residing on this server and flush their state */
	{
	    register struct volume *tv;
	    register int j;

	    for (i = 0; i < NVOLS; i++)
		for (tv = afs_volumes[i]; tv; tv = tv->next) {
		    for (j = 0; j < MAXHOSTS; j++)
			if (tv->serverHost[j] == ts)
			    afs_ResetVolumeInfo(tv);
		}
	}
	osi_dnlc_purge();	/* may be a little bit extreme */
    }

    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (0);

}				/*SRXAFSCB_InitCallBackState */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_XStatsVersion
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the xstat version number for the Cache
 *	Manager.
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

int
SRXAFSCB_XStatsVersion(struct rx_call *a_call, afs_int32 * a_versionP)
{
    int code = 0;

    XSTATS_DECLS;

    RX_AFS_GLOCK();
    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_XSTATSVERSION);

    *a_versionP = AFSCB_XSTAT_VERSION;

    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (0);
}				/*SRXAFSCB_XStatsVersion */


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetXStats
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement getting the given data collection from the extended
 *	Cache Manager statistics.
 *
 * Arguments:
 *	a_call		    : Ptr to Rx call on which this request came in.
 *	a_clientVersionNum  : Client version number.
 *	a_opCode	    : Desired operation.
 *	a_serverVersionNumP : Ptr to version number to set.
 *	a_timeP		    : Ptr to time value (seconds) to set.
 *	a_dataArray	    : Ptr to variable array structure to return
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

int
SRXAFSCB_GetXStats(struct rx_call *a_call, afs_int32 a_clientVersionNum,
		   afs_int32 a_collectionNumber, afs_int32 * a_srvVersionNumP,
		   afs_int32 * a_timeP, AFSCB_CollData * a_dataP)
{
    register int code;		/*Return value */
    afs_int32 *dataBuffP;	/*Ptr to data to be returned */
    afs_int32 dataBytes;	/*Bytes in data buffer */
    XSTATS_DECLS;

    RX_AFS_GLOCK();

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETXSTATS);

    /*
     * Record the time of day and the server version number.
     */
    *a_srvVersionNumP = AFSCB_XSTAT_VERSION;
    *a_timeP = osi_Time();

    /*
     * Stuff the appropriate data in there (assume victory)
     */
    code = 0;

#ifdef AFS_NOSTATS
    /*
     * We're not keeping stats, so just return successfully with
     * no data.
     */
    a_dataP->AFSCB_CollData_len = 0;
    a_dataP->AFSCB_CollData_val = NULL;
#else
    switch (a_collectionNumber) {
    case AFSCB_XSTATSCOLL_CALL_INFO:
	/*
	 * Pass back all the call-count-related data.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	dataBytes = sizeof(struct afs_CMStats);
	dataBuffP = (afs_int32 *) afs_osi_Alloc(dataBytes);
	memcpy((char *)dataBuffP, (char *)&afs_cmstats, dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes >> 2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

    case AFSCB_XSTATSCOLL_PERF_INFO:
	/*
	 * Update and then pass back all the performance-related data.
	 * Note: the only performance fields that need to be computed
	 * at this time are the number of accesses for this collection
	 * and the current server record info.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	afs_stats_cmperf.numPerfCalls++;
	afs_CountServers();
	dataBytes = sizeof(afs_stats_cmperf);
	dataBuffP = (afs_int32 *) afs_osi_Alloc(dataBytes);
	memcpy((char *)dataBuffP, (char *)&afs_stats_cmperf, dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes >> 2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

    case AFSCB_XSTATSCOLL_FULL_PERF_INFO:
	/*
	 * Pass back the full range of performance and statistical
	 * data available.  We have to bring the normal performance
	 * data collection up to date, then copy that data into
	 * the full collection.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	afs_stats_cmperf.numPerfCalls++;
	afs_CountServers();
	memcpy((char *)(&(afs_stats_cmfullperf.perf)),
	       (char *)(&afs_stats_cmperf), sizeof(struct afs_stats_CMPerf));
	afs_stats_cmfullperf.numFullPerfCalls++;

	dataBytes = sizeof(afs_stats_cmfullperf);
	dataBuffP = (afs_int32 *) afs_osi_Alloc(dataBytes);
	memcpy((char *)dataBuffP, (char *)(&afs_stats_cmfullperf), dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes >> 2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

    default:
	/*
	 * Illegal collection number.
	 */
	a_dataP->AFSCB_CollData_len = 0;
	a_dataP->AFSCB_CollData_val = NULL;
	code = 1;
    }				/*Switch on collection number */
#endif /* AFS_NOSTATS */

    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return (code);

}				/*SRXAFSCB_GetXStats */


/*------------------------------------------------------------------------
 * EXPORTED afs_RXCallBackServer
 *
 * Description:
 *	Body of the thread supporting callback services.
 *
 * Arguments:
 *	None.
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

int
afs_RXCallBackServer(void)
{
    AFS_STATCNT(afs_RXCallBackServer);

    while (1) {
	if (afs_server)
	    break;
	afs_osi_Sleep(&afs_server);
    }

    /*
     * Donate this process to Rx.
     */
    rx_ServerProc();
    return (0);

}				/*afs_RXCallBackServer */


/*------------------------------------------------------------------------
 * EXPORTED shutdown_CB
 *
 * Description:
 *	Zero out important Cache Manager data structures.
 *
 * Arguments:
 *	None.
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

int
shutdown_CB(void)
{
    AFS_STATCNT(shutdown_CB);

    if (afs_cold_shutdown) {
	afs_oddCBs = afs_evenCBs = afs_allCBs = afs_allZaps = afs_oddZaps =
	    afs_evenZaps = afs_connectBacks = 0;
    }

    return (0);

}				/*shutdown_CB */

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState2
 *
 * Description:
 *      This routine was used in the AFS 3.5 beta release, but not anymore.
 *      It has since been replaced by SRXAFSCB_InitCallBackState3.
 *
 * Arguments:
 *      a_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *      RXGEN_OPCODE (always). 
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      None
 *------------------------------------------------------------------------*/

int
SRXAFSCB_InitCallBackState2(struct rx_call *a_call,
			    struct interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_WhoAreYou
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      obtain a unique identifier for the client. The server uses
 *      this identifier to figure out whether or not two RX connections
 *      are from the same client, and to find out which addresses go
 *      with which clients.
 *
 * Arguments:
 *      a_call : Ptr to Rx call on which this request came in.
 *      addr: Ptr to return the list of interfaces for this client.
 *
 * Returns:
 *      0 (Always)
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_WhoAreYou(struct rx_call *a_call, struct interfaceAddr *addr)
{
    int i;
    int code = 0;

    RX_AFS_GLOCK();

    AFS_STATCNT(SRXAFSCB_WhoAreYou);

    memset(addr, 0, sizeof(*addr));

    ObtainReadLock(&afs_xinterface);

    /* return all network interface addresses */
    addr->numberOfInterfaces = afs_cb_interface.numberOfInterfaces;
    addr->uuid = afs_cb_interface.uuid;
    for (i = 0; i < afs_cb_interface.numberOfInterfaces; i++) {
	addr->addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
	addr->subnetmask[i] = ntohl(afs_cb_interface.subnetmask[i]);
	addr->mtu[i] = ntohl(afs_cb_interface.mtu[i]);
    }

    ReleaseReadLock(&afs_xinterface);

    RX_AFS_GUNLOCK();

    return code;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState3
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement clearing all callbacks from this host.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
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

int
SRXAFSCB_InitCallBackState3(struct rx_call *a_call, afsUUID * a_uuid)
{
    int code;

    /*
     * TBD: Lookup the server by the UUID instead of its IP address.
     */
    code = SRXAFSCB_InitCallBackState(a_call);

    return code;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_ProbeUuid
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement ``probing'' the Cache Manager, just making sure it's
 *	still there is still the same client it used to be.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
 *	a_uuid : Ptr to UUID that must match the client's UUID.
 *
 * Returns:
 *	0 if a_uuid matches the UUID for this client
 *      Non-zero otherwize
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_ProbeUuid(struct rx_call *a_call, afsUUID * a_uuid)
{
    int code = 0;
    XSTATS_DECLS;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_Probe);

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_PROBE);
    if (!afs_uuid_equal(a_uuid, &afs_cb_interface.uuid))
	code = 1;		/* failure */
    XSTATS_END_TIME;

    RX_AFS_GUNLOCK();

    return code;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetServerPrefs
 *
 * Description:
 *	Routine to list server preferences used by this client.
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_index : Input server index
 *	a_srvr_addr  : Output server address in host byte order
 *                     (0xffffffff on last server)
 *	a_srvr_rank  : Output server rank
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetServerPrefs(struct rx_call *a_call, afs_int32 a_index,
			afs_int32 * a_srvr_addr, afs_int32 * a_srvr_rank)
{
    int i, j;
    struct srvAddr *sa;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_GetServerPrefs);

    ObtainReadLock(&afs_xserver);

    /* Search the hash table for the server with this index */
    *a_srvr_addr = 0xffffffff;
    *a_srvr_rank = 0xffffffff;
    for (i = 0, j = 0; j < NSERVERS && i <= a_index; j++) {
	for (sa = afs_srvAddrs[j]; sa && i <= a_index; sa = sa->next_bkt, i++) {
	    if (i == a_index) {
		*a_srvr_addr = ntohl(sa->sa_ip);
		*a_srvr_rank = sa->sa_iprank;
	    }
	}
    }

    ReleaseReadLock(&afs_xserver);

    RX_AFS_GUNLOCK();

    return 0;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCellServDB
 *
 * Description:
 *	Routine to list cells configured for this client
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_index : Input cell index
 *	a_name  : Output cell name ("" on last cell)
 *	a_hosts : Output cell database servers in host byte order.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetCellServDB(struct rx_call *a_call, afs_int32 a_index,
		       char **a_name, serverList * a_hosts)
{
    afs_int32 i, j = 0;
    struct cell *tcell;
    char *t_name, *p_name = NULL;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_GetCellServDB);

    tcell = afs_GetCellByIndex(a_index, READ_LOCK);

    if (!tcell) {
	i = 0;
	a_hosts->serverList_val = 0;
	a_hosts->serverList_len = 0;
    } else {
	p_name = tcell->cellName;
	for (j = 0; j < AFSMAXCELLHOSTS && tcell->cellHosts[j]; j++);
	i = strlen(p_name);
	a_hosts->serverList_val =
	    (afs_int32 *) afs_osi_Alloc(j * sizeof(afs_int32));
	a_hosts->serverList_len = j;
	for (j = 0; j < AFSMAXCELLHOSTS && tcell->cellHosts[j]; j++)
	    a_hosts->serverList_val[j] =
		ntohl(tcell->cellHosts[j]->addr->sa_ip);
	afs_PutCell(tcell, READ_LOCK);
    }

    t_name = (char *)afs_osi_Alloc(i + 1);
    if (t_name == NULL) {
	afs_osi_Free(a_hosts->serverList_val, (j * sizeof(afs_int32)));
	RX_AFS_GUNLOCK();
	return ENOMEM;
    }

    t_name[i] = '\0';
    if (p_name)
	memcpy(t_name, p_name, i);

    RX_AFS_GUNLOCK();

    *a_name = t_name;
    return 0;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLocalCell
 *
 * Description:
 *	Routine to return name of client's local cell
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_name  : Output cell name
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetLocalCell(struct rx_call *a_call, char **a_name)
{
    int plen;
    struct cell *tcell;
    char *t_name, *p_name = NULL;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_GetLocalCell);

    /* Search the list for the primary cell. Cell number 1 is only
     * the primary cell is when no other cell is explicitly marked as
     * the primary cell.  */
    tcell = afs_GetPrimaryCell(READ_LOCK);
    if (tcell)
	p_name = tcell->cellName;
    if (p_name)
	plen = strlen(p_name);
    else
	plen = 0;
    t_name = (char *)afs_osi_Alloc(plen + 1);
    if (t_name == NULL) {
	if (tcell)
	    afs_PutCell(tcell, READ_LOCK);
	RX_AFS_GUNLOCK();
	return ENOMEM;
    }

    t_name[plen] = '\0';
    if (p_name)
	memcpy(t_name, p_name, plen);

    RX_AFS_GUNLOCK();

    *a_name = t_name;
    if (tcell)
	afs_PutCell(tcell, READ_LOCK);
    return 0;
}


/*
 * afs_MarshallCacheConfig - marshall client cache configuration
 *
 * PARAMETERS
 *
 * IN callerVersion - the rpc stat version of the caller.
 *
 * IN config - client cache configuration.
 *
 * OUT ptr - buffer where configuration is marshalled.
 *
 * RETURN CODES
 *
 * Returns void.
 */
static void
afs_MarshallCacheConfig(afs_uint32 callerVersion, cm_initparams_v1 * config,
			afs_uint32 * ptr)
{
    AFS_STATCNT(afs_MarshallCacheConfig);
    /*
     * We currently only support version 1.
     */
    *(ptr++) = config->nChunkFiles;
    *(ptr++) = config->nStatCaches;
    *(ptr++) = config->nDataCaches;
    *(ptr++) = config->nVolumeCaches;
    *(ptr++) = config->firstChunkSize;
    *(ptr++) = config->otherChunkSize;
    *(ptr++) = config->cacheSize;
    *(ptr++) = config->setTime;
    *(ptr++) = config->memCache;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCacheConfig
 *
 * Description:
 *	Routine to return parameters used to initialize client cache.
 *      Client may request any format version. Server may not return
 *      format version greater than version requested by client.
 *
 * Arguments:
 *	a_call:        Ptr to Rx call on which this request came in.
 *	callerVersion: Data format version desired by the client.
 *	serverVersion: Data format version of output data.
 *      configCount:   Number bytes allocated for output data.
 *      config:        Client cache configuration.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetCacheConfig(struct rx_call *a_call, afs_uint32 callerVersion,
			afs_uint32 * serverVersion, afs_uint32 * configCount,
			cacheConfig * config)
{
    afs_uint32 *t_config;
    size_t allocsize;
    cm_initparams_v1 cm_config;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_GetCacheConfig);

    /*
     * Currently only support version 1
     */
    allocsize = sizeof(cm_initparams_v1);
    t_config = (afs_uint32 *) afs_osi_Alloc(allocsize);
    if (t_config == NULL) {
	RX_AFS_GUNLOCK();
	return ENOMEM;
    }

    cm_config.nChunkFiles = cm_initParams.cmi_nChunkFiles;
    cm_config.nStatCaches = cm_initParams.cmi_nStatCaches;
    cm_config.nDataCaches = cm_initParams.cmi_nDataCaches;
    cm_config.nVolumeCaches = cm_initParams.cmi_nVolumeCaches;
    cm_config.firstChunkSize = cm_initParams.cmi_firstChunkSize;
    cm_config.otherChunkSize = cm_initParams.cmi_otherChunkSize;
    cm_config.cacheSize = cm_initParams.cmi_cacheSize;
    cm_config.setTime = cm_initParams.cmi_setTime;
    cm_config.memCache = cm_initParams.cmi_memCache;

    afs_MarshallCacheConfig(callerVersion, &cm_config, t_config);

    *serverVersion = AFS_CLIENT_RETRIEVAL_FIRST_EDITION;
    *configCount = allocsize;
    config->cacheConfig_val = t_config;
    config->cacheConfig_len = allocsize / sizeof(afs_uint32);

    RX_AFS_GUNLOCK();

    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_FetchData
 *
 * Description:
 *      Routine to do third party move from a remioserver to the original
 *      issuer of an ArchiveData request. Presently supported only by the
 *      "fs" command, not by the AFS client.
 *
 * Arguments:
 *      rxcall:        Ptr to Rx call on which this request came in.
 *      Fid:           pointer to AFSFid structure.
 *      Fd:            File descriptor inside fs command.
 *      Position:      Offset in the file.
 *      Length:        Data length to transfer.
 *      TotalLength:   Pointer to total file length field
 *
 * Returns:
 *      0 on success
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *------------------------------------------------------------------------*/
int
SRXAFSCB_FetchData(struct rx_call *rxcall, struct AFSFid *Fid, afs_int32 Fd,
		   afs_int64 Position, afs_int64 Length,
		   afs_int64 * TotalLength)
{
    return ENOSYS;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_StoreData
 *
 * Description:
 *      Routine to do third party move from a remioserver to the original
 *      issuer of a RetrieveData request. Presently supported only by the
 *      "fs" command, not by the AFS client.
 *
 * Arguments:
 *      rxcall:        Ptr to Rx call on which this request came in.
 *      Fid:           pointer to AFSFid structure.
 *      Fd:            File descriptor inside fs command.
 *      Position:      Offset in the file.
 *      Length:        Data length to transfer.
 *      TotalLength:   Pointer to total file length field
 *
 * Returns:
 *      0 on success
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/
int
SRXAFSCB_StoreData(struct rx_call *rxcall, struct AFSFid *Fid, afs_int32 Fd,
		   afs_int64 Position, afs_int64 Length,
		   afs_int64 * TotalLength)
{
    return ENOSYS;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCellByNum
 *
 * Description:
 *	Routine to get information about a cell specified by its
 *	cell number (returned by GetCE/GetCE64).
 *
 * Arguments:
 *	a_call    : Ptr to Rx call on which this request came in.
 *	a_cellnum : Input cell number
 *	a_name    : Output cell name (one zero byte when no such cell).
 *	a_hosts   : Output cell database servers in host byte order.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
SRXAFSCB_GetCellByNum(struct rx_call *a_call, afs_int32 a_cellnum,
		      char **a_name, serverList * a_hosts)
{
    afs_int32 i, sn;
    struct cell *tcell;

    RX_AFS_GLOCK();
    AFS_STATCNT(SRXAFSCB_GetCellByNum);

    a_hosts->serverList_val = 0;
    a_hosts->serverList_len = 0;

    tcell = afs_GetCellStale(a_cellnum, READ_LOCK);
    if (!tcell) {
	*a_name = afs_strdup("");
	RX_AFS_GUNLOCK();
	return 0;
    }

    ObtainReadLock(&tcell->lock);
    *a_name = afs_strdup(tcell->cellName);

    for (sn = 0; sn < AFSMAXCELLHOSTS && tcell->cellHosts[sn]; sn++);
    a_hosts->serverList_len = sn;
    a_hosts->serverList_val =
	(afs_int32 *) afs_osi_Alloc(sn * sizeof(afs_int32));

    for (i = 0; i < sn; i++)
	a_hosts->serverList_val[i] = ntohl(tcell->cellHosts[i]->addr->sa_ip);
    ReleaseReadLock(&tcell->lock);
    afs_PutCell(tcell, READ_LOCK);

    RX_AFS_GUNLOCK();
    return 0;
}

int
SRXAFSCB_TellMeAboutYourself(struct rx_call *a_call,
			     struct interfaceAddr *addr,
			     Capabilities * capabilities)
{
    int i;
    int code = 0;
    afs_int32 *dataBuffP;
    afs_int32 dataBytes;

    RX_AFS_GLOCK();

    AFS_STATCNT(SRXAFSCB_WhoAreYou);

    ObtainReadLock(&afs_xinterface);

    /* return all network interface addresses */
    addr->numberOfInterfaces = afs_cb_interface.numberOfInterfaces;
    addr->uuid = afs_cb_interface.uuid;
    for (i = 0; i < afs_cb_interface.numberOfInterfaces; i++) {
	addr->addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
	addr->subnetmask[i] = ntohl(afs_cb_interface.subnetmask[i]);
	addr->mtu[i] = ntohl(afs_cb_interface.mtu[i]);
    }

    ReleaseReadLock(&afs_xinterface);

    RX_AFS_GUNLOCK();

    dataBytes = 1 * sizeof(afs_int32);
    dataBuffP = (afs_int32 *) afs_osi_Alloc(dataBytes);
    dataBuffP[0] = CLIENT_CAPABILITY_ERRORTRANS;
    capabilities->Capabilities_len = dataBytes / sizeof(afs_int32);
    capabilities->Capabilities_val = dataBuffP;

    return code;
}


#ifdef AFS_LINUX24_ENV
extern struct vcache *afs_globalVp;

int recurse_dcache_parent(parent, a_index, addr, inode, flags, time, fileName)
     struct dentry * parent;
    afs_int32 a_index;
    afs_int32 *addr;
    afs_int32 *inode;
    afs_int32 *flags;
    afs_int32 *time;
    char ** fileName;
{
	struct dentry *this_parent = parent;
	struct list_head *next;
	int found = 0;
	struct dentry *dentry;

repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		dentry = list_entry(tmp, struct dentry, d_child);
		if (a_index == 0)
		  goto searchdone3;
		a_index--;
		next = tmp->next;
		/*
		 * Descend a level if the d_subdirs list is non-empty.
		 */
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}
	}
	/*
	 * All done at this level ... ascend and resume the search.
	 */
	if (this_parent != parent) {
		next = this_parent->d_child.next; 
		this_parent = this_parent->d_parent;
		goto resume;
	}
	goto ret;

 searchdone3:
    if (d_unhashed(dentry))
      *flags = 1;
    else 
      *flags = 0;

    *fileName = afs_strdup(dentry->d_name.name?dentry->d_name.name:"");
    *inode = ITOAFS(dentry->d_inode);
    *addr = atomic_read(&(dentry)->d_count);
    *time = dentry->d_time;

    return 0;
 ret:
    return 1;
}
#endif

int SRXAFSCB_GetDE(a_call, a_index, addr, inode, flags, time, fileName)
    struct rx_call *a_call;
    afs_int32 a_index;
    afs_int32 *addr;
    afs_int32 *inode;
    afs_int32 *flags;
    afs_int32 *time;
    char ** fileName;
{ /*SRXAFSCB_GetDE*/
    int code = 0;				/*Return code*/
#ifdef AFS_LINUX24_ENV
    register int i;			/*Loop variable*/
    register struct vcache *tvc = afs_globalVp;
    struct dentry *dentry;
    struct list_head *cur, *head = &(AFSTOI(tvc))->i_dentry;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

#if defined(AFS_LINUX24_ENV)
    spin_lock(&dcache_lock);
#endif

    cur = head;
    while ((cur = cur->next) != head) {
      dentry = list_entry(cur, struct dentry, d_alias);
      
      dget_locked(dentry);
      
#if defined(AFS_LINUX24_ENV)
      spin_unlock(&dcache_lock);
#endif
      if (a_index == 0)
	goto searchdone2;
      a_index--;

      if (recurse_dcache_parent(dentry, a_index, addr, inode, flags, time, fileName) == 0) {
	dput(dentry);
	code = 0;
	goto fcnDone;
      }
      dput(dentry);
    }                   
 searchdone2:
    if (cur == head) {
	/*Past EOF*/
	code = 1;
	*fileName = afs_strdup("");
	goto fcnDone;
    }

    if (d_unhashed(dentry))
      *flags = 1;
    else 
      *flags = 0;

    *fileName = afs_strdup(dentry->d_name.name?dentry->d_name.name:"");
    *inode = ITOAFS(dentry->d_inode);
    *addr = atomic_read(&(dentry)->d_count);
    *time = dentry->d_time;

    dput(dentry);
    code = 0;

fcnDone:

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
#endif
    return(code);

} /*SRXAFSCB_GetDE*/
