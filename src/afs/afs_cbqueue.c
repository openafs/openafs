/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This package is used to actively manage the expiration of callbacks,
 * so that the rest of the cache manager doesn't need to compute
 * whether a callback has expired or not, but can tell with one simple
 * check, that is, whether the CStatd bit is on or off.
 *
 * The base of the hash table moves periodically (every 128 seconds)
 * QueueCallback rarely touches the first 3 slots in the hash table
 * (only when called from CheckCallbacks) since MinTimeOut in
 * viced/callback.c is currently 7 minutes. 
 * Therefore, CheckCallbacks should be able to run concurrently with
 * QueueCallback, given the proper locking, of course.
 *
 * Note:
 * 1. CheckCallbacks and BumpBase never run simultaneously.  This is because
 * they are only called from afs_Daemon.  Therefore, base and basetime will 
 * always be consistent during CheckCallbacks.
 * 2. cbHashT [base] rarely (if ever) gets stuff queued in it.  The only way 
 * that could happen is CheckCallbacks might fencepost and move something in
 * place, or BumpBase might push some stuff up.
 * 3. Hash chains aren't particularly sorted. 
 * 4. The file server keeps its callback state around for 3 minutes
 * longer than it promises the cache manager in order to account for
 * clock skew, network delay, and other bogeymen.
 *
 * For now I just use one large lock, which is fine on a uniprocessor,
 * since it's not held during any RPCs or low-priority I/O operations. 
 * To make this code MP-fast, you need no more locks than processors, 
 * but probably more than one.  In measurements on MP-safe implementations, 
 * I have never seen any contention over the xcbhash lock.
 *
 * Incompatible operations:
 * Enqueue and "dequeue of first vcache" in same slot
 * dequeue and "dequeue of preceding vcache" in same slot
 * dequeue and "dequeue of successive vcache" in same slot
 * BumpBase pushing a list and enqueue in the new base slot
 * Two enqueues in same slot
 * more...
 *
 * Certain invariants exist:
 *    1  Callback expiration times granted by a file server will never
 *       decrease for a particular vnode UNLESS a CallBack RPC is invoked
 *       by the server in the interim.  
 *    2  A vcache will always expire no sooner than the slot in which it is
 *       currently enqueued.  Callback times granted by the server may 
 *       increase, in which case the vcache will be updated in-place.  As a 
 *       result, it may expire later than the slot in which it is enqueued.  
 *       Not to worry, the CheckCallbacks code will move it if neccessary.
 *       This approach means that busy vnodes won't be continually moved 
 *       around within the expiry queue: they are only moved when they
 *       finally advance to the lead bucket.
 *    3  Anything which has a callback on it must be in the expiry
 *       queue.  In AFS 3.3, that means everything but symlinks (which
 *       are immutable), including contents of Read-Only volumes
 *       (which have callbacks by virtue of the whole-volume callback)
 *
 * QueueCallback only checks that its vcache is in the list
 * somewhere, counting on invariant #1 to guarantee that the vcache
 * won't be in a slot later than QueueCallback would otherwise place
 * it. Therefore, whenever we turn off the CStatd bit on the vcache, we
 * *must* remove the vcache from the expiry queue.  Otherwise, we
 * might have missed a CallBack RPC, and a subsequent callback might be
 * granted with a shorter expiration time.
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_cbqueue.c,v 1.9.2.1 2005/05/30 04:05:40 shadow Exp $");

#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs/afs_cbqueue.h"
#include "afs/afs.h"
#include "afs/lock.h"
#include "afs/afs_stats.h"

static unsigned int base = 0;
static unsigned int basetime = 0;
static struct vcache *debugvc;	/* used only for post-mortem debugging */
struct bucket {
    struct afs_q head;
    /*  struct afs_lock lock;  only if you want lots of locks... */
};
static struct bucket cbHashT[CBHTSIZE];
struct afs_lock afs_xcbhash;

/* afs_QueueCallback
 * Takes a write-locked vcache pointer and a callback expiration time
 * as returned by the file server (ie, in units of 128 seconds from "now").
 * 
 * Uses the time as an index into a hash table, and inserts the vcache
 * structure into the overflow chain.
 * 
 * If the vcache is already on some hash chain, leave it there.
 * CheckCallbacks will get to it eventually.  In the meantime, it
 * might get flushed, or it might already be on the right hash chain, 
 * so why bother messing with it now?
 *
 * NOTE: The caller must hold a write lock on afs_xcbhash
 */

void
afs_QueueCallback(struct vcache *avc, unsigned int atime, struct volume *avp)
{
    if (avp && (avp->expireTime < avc->cbExpires))
	avp->expireTime = avc->cbExpires;
    if (!(avc->callsort.next)) {
	atime = (atime + base) % CBHTSIZE;
	QAdd(&(cbHashT[atime].head), &(avc->callsort));
    }

    return;
}				/* afs_QueueCallback */

/* afs_DequeueCallback
 * Takes a write-locked vcache pointer and removes it from the callback
 * hash table, without knowing beforehand which slot it was in.
 *
 * for now, just get a lock on everything when doing the dequeue, don't
 * worry about getting a lock on the individual slot.
 * 
 * the only other places that do anything like dequeues are CheckCallbacks
 * and BumpBase.
 *
 * NOTE: The caller must hold a write lock on afs_xcbhash
 */
void
afs_DequeueCallback(struct vcache *avc)
{

    debugvc = avc;
    if (avc->callsort.prev) {
	QRemove(&(avc->callsort));
	avc->callsort.prev = avc->callsort.next = NULL;
    } else;			/* must have got dequeued in a race */

    return;
}				/* afs_DequeueCallback */

/* afs_CheckCallbacks
 * called periodically to determine which callbacks are likely to
 * expire in the next n second interval.  Preemptively marks them as
 * expired.  Rehashes items which are now in the wrong hash bucket.
 * Preemptively renew recently-accessed items.  Only removes things
 * from the first and second bucket (as long as secs < 128), and
 * inserts things into other, later buckets.  either need to advance
 * to the second bucket if secs spans two intervals, or else be
 * certain to call afs_CheckCallbacks immediately after calling
 * BumpBase (allows a little more slop but it's ok because file server
 * keeps 3 minutes of slop time)
 *
 * There is a little race between CheckCallbacks and any code which
 * updates cbExpires, always just prior to calling QueueCallback. We
 * don't lock the vcache struct here (can't, or we'd risk deadlock),
 * so GetVCache (for example) may update cbExpires before or after #1
 * below.  If before, CheckCallbacks moves this entry to its proper
 * slot.  If after, GetVCache blocks in the call to QueueCallbacks,
 * this code dequeues the vcache, and then QueueCallbacks re-enqueues it. 
 *
 * XXX to avoid the race, make QueueCallback take the "real" time
 * and update cbExpires under the xcbhash lock. 
 *
 * NB #1: There's a little optimization here: if I go to invalidate a
 * RO vcache or volume, first check to see if the server is down.  If
 * it _is_, don't invalidate it, cuz we might just as well keep using
 * it.  Possibly, we could do the same thing for items in RW volumes,
 * but that bears some drinking about.
 *
 * Don't really need to invalidate the hints, we could just wait to see if
 * the dv has changed after a subsequent FetchStatus, but this is safer.
 */

/* Sanity check on the callback queue. Allow for slop in the computation. */
#ifdef AFS_OSF_ENV
#define CBQ_LIMIT (afs_maxvcount + 10)
#else
#define CBQ_LIMIT (afs_cacheStats + afs_stats_cmperf.vcacheXAllocs + 10)
#endif

void
afs_CheckCallbacks(unsigned int secs)
{
    struct vcache *tvc;
    register struct afs_q *tq;
    struct afs_q *uq;
    afs_uint32 now;
    struct volume *tvp;
    register int safety;

    ObtainWriteLock(&afs_xcbhash, 85);	/* pretty likely I'm going to remove something */
    now = osi_Time();
    for (safety = 0, tq = cbHashT[base].head.prev;
	 (safety <= CBQ_LIMIT) && (tq != &(cbHashT[base].head));
	 tq = uq, safety++) {

	uq = QPrev(tq);
	tvc = CBQTOV(tq);
	if (tvc->cbExpires < now + secs) {	/* race #1 here */
	    /* Get the volume, and if its callback expiration time is more than secs
	     * seconds into the future, update this vcache entry and requeue it below
	     */
	    if ((tvc->states & CRO)
		&& (tvp = afs_FindVolume(&(tvc->fid), READ_LOCK))) {
		if (tvp->expireTime > now + secs) {
		    tvc->cbExpires = tvp->expireTime;	/* XXX race here */
		} else {
		    int i;
		    for (i = 0; i < MAXHOSTS && tvp->serverHost[i]; i++) {
			if (!(tvp->serverHost[i]->flags & SRVR_ISDOWN)) {
			    /* What about locking xvcache or vrefcount++ or
			     * write locking tvc? */
			    QRemove(tq);
			    tq->prev = tq->next = NULL;
			    tvc->states &= ~(CStatd | CMValid | CUnique);
			    if ((tvc->fid.Fid.Vnode & 1)
				|| (vType(tvc) == VDIR))
				osi_dnlc_purgedp(tvc);
			    tvc->dchint = NULL;	/*invalidate em */
			    afs_ResetVolumeInfo(tvp);
			    break;
			}
		    }
		}
		afs_PutVolume(tvp, READ_LOCK);
	    } else {
		/* Do I need to worry about things like execsorwriters?
		 * What about locking xvcache or vrefcount++ or write locking tvc?
		 */
		QRemove(tq);
		tq->prev = tq->next = NULL;
		tvc->states &= ~(CStatd | CMValid | CUnique);
		if ((tvc->fid.Fid.Vnode & 1) || (vType(tvc) == VDIR))
		    osi_dnlc_purgedp(tvc);
	    }
	}

	if ((tvc->cbExpires > basetime) && CBHash(tvc->cbExpires - basetime)) {
	    /* it's been renewed on us.  Have to be careful not to put it back
	     * into this slot, or we may never get out of here.
	     */
	    int slot;
	    slot = (CBHash(tvc->cbExpires - basetime) + base) % CBHTSIZE;
	    if (slot != base) {
		if (QPrev(tq))
		    QRemove(&(tvc->callsort));
		QAdd(&(cbHashT[slot].head), &(tvc->callsort));
		/* XXX remember to update volume expiration time */
		/* -- not needed for correctness, though */
	    }
	}
    }

    if (safety > CBQ_LIMIT) {
	afs_stats_cmperf.cbloops++;
	if (afs_paniconwarn)
	    osi_Panic("CheckCallbacks");

	afs_warn
	    ("AFS Internal Error (minor): please contact AFS Product Support.\n");
	ReleaseWriteLock(&afs_xcbhash);
	afs_FlushCBs();
	return;
    } else
	ReleaseWriteLock(&afs_xcbhash);


/* XXX future optimization:
   if this item has been recently accessed, queue up a stat for it.
   {
   struct dcache * adc;

   ObtainReadLock(&afs_xdcache);
   if ((adc = tvc->quick.dc) && (adc->stamp == tvc->quick.stamp)
   && (afs_indexTimes[adc->index] > afs_indexCounter - 20)) {
   queue up the stat request
   }
   ReleaseReadLock(&afs_xdcache);
   }
   */

    return;
}				/* afs_CheckCallback */

/* afs_FlushCBs 
 * to be used only in dire circumstances, this drops all callbacks on
 * the floor, without giving them back to the server.  It's ok, the server can 
 * deal with it, but it is a little bit rude.
 */
void
afs_FlushCBs(void)
{
    register int i;
    register struct vcache *tvc;

    ObtainWriteLock(&afs_xcbhash, 86);	/* pretty likely I'm going to remove something */

    for (i = 0; i < VCSIZE; i++)	/* reset all the vnodes */
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    tvc->callback = 0;
	    tvc->dchint = NULL;	/* invalidate hints */
	    tvc->states &= ~(CStatd);
	    if ((tvc->fid.Fid.Vnode & 1) || (vType(tvc) == VDIR))
		osi_dnlc_purgedp(tvc);
	    tvc->callsort.prev = tvc->callsort.next = NULL;
	}

    afs_InitCBQueue(0);

    ReleaseWriteLock(&afs_xcbhash);
}

/* afs_FlushServerCBs
 * to be used only in dire circumstances, this drops all callbacks on
 * the floor for a specific server, without giving them back to the server.
 * It's ok, the server can deal with it, but it is a little bit rude.
 */
void
afs_FlushServerCBs(struct server *srvp)
{
    register int i;
    register struct vcache *tvc;

    ObtainWriteLock(&afs_xcbhash, 86);	/* pretty likely I'm going to remove something */

    for (i = 0; i < VCSIZE; i++) {	/* reset all the vnodes */
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    if (tvc->callback == srvp) {
		tvc->callback = 0;
		tvc->dchint = NULL;	/* invalidate hints */
		tvc->states &= ~(CStatd);
		if ((tvc->fid.Fid.Vnode & 1) || (vType(tvc) == VDIR)) {
		    osi_dnlc_purgedp(tvc);
		}
		afs_DequeueCallback(tvc);
	    }
	}
    }

    ReleaseWriteLock(&afs_xcbhash);
}

/* afs_InitCBQueue
 *  called to initialize static and global variables associated with
 *  the Callback expiration management mechanism.
 */
void
afs_InitCBQueue(int doLockInit)
{
    register int i;

    memset((char *)cbHashT, 0, CBHTSIZE * sizeof(struct bucket));
    for (i = 0; i < CBHTSIZE; i++) {
	QInit(&(cbHashT[i].head));
	/* Lock_Init(&(cbHashT[i].lock)); only if you want lots of locks, which 
	 * don't seem too useful at present.  */
    }
    base = 0;
    basetime = osi_Time();
    if (doLockInit)
	Lock_Init(&afs_xcbhash);
}

/* Because there are no real-time guarantees, and especially because a
 * thread may wait on a lock indefinitely, this routine has to be
 * careful that it doesn't get permanently out-of-date.  Important
 * assumption: this routine is only called from afs_Daemon, so there
 * can't be more than one instance of this running at any one time.
 * Presumes that basetime is never 0, and is always sane. 
 *
 * Before calling this routine, be sure that the first slot is pretty
 * empty.  This -20 is because the granularity of the checks in
 * afs_Daemon is pretty large, so I'd rather err on the side of safety
 * sometimes.  The fact that I only bump basetime by CBHTSLOTLEN-1
 * instead of the whole CBHTSLOTLEN is also for "safety".
 * Conceptually, it makes this clock run just a little faster than the
 * clock governing which slot a callback gets hashed into.  Both of these 
 * things make CheckCallbacks work a little harder than it would have to 
 * if I wanted to cut things finer.
 * Everything from the old first slot is carried over into the new first
 * slot.  Thus, if there were some things that ought to have been invalidated,
 * but weren't (say, if the server was down), they will be examined at every
 * opportunity thereafter.
 */
int
afs_BumpBase(void)
{
    afs_uint32 now;
    int didbump;
    u_int oldbase;

    ObtainWriteLock(&afs_xcbhash, 87);
    didbump = 0;
    now = osi_Time();
    while (basetime + (CBHTSLOTLEN - 20) <= now) {
	oldbase = base;
	basetime += CBHTSLOTLEN - 1;
	base = (base + 1) % CBHTSIZE;
	didbump++;
	if (!QEmpty(&(cbHashT[oldbase].head))) {
	    QCat(&(cbHashT[oldbase].head), &(cbHashT[base].head));
	}
    }
    ReleaseWriteLock(&afs_xcbhash);

    return didbump;
}
