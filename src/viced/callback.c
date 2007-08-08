/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * NEW callback package callback.c (replaces vicecb.c)
 * Updated call back routines, NOW with:
 * 
 *     Faster DeleteVenus (Now called DeleteAllCallBacks)
 *     Call back breaking for volumes
 *     Adaptive timeouts on call backs
 *     Architected for Multi RPC
 *     No locks (currently implicit vnode locks--these will go, to)
 *     Delayed call back when rpc connection down.
 *     Bulk break of delayed call backs when rpc connection
 *         reestablished
 *     Strict limit on number of call backs.
 *
 * InitCallBack(nblocks)
 *     Initialize: nblocks is max number # of file entries + # of callback entries
 *     nblocks must be < 65536
 *     Space used is nblocks*16 bytes
 *     Note that space will be reclaimed by breaking callbacks of old hosts
 * 
 * time = AddCallBack(host, fid)
 *     Add a call back.
 *     Returns the expiration time at the workstation.
 * 
 * BreakCallBack(host, fid)
 *     Break all call backs for fid, except for the specified host.
 *     Delete all of them.
 * 
 * BreakVolumeCallBacks(volume)
 *     Break all call backs on volume, using single call to each host
 *     Delete all the call backs.
 * 
 * DeleteCallBack(host,fid)
 *     Delete (do not break) single call back for fid.
 * 
 * DeleteFileCallBacks(fid)
 *     Delete (do not break) all call backs for fid.
 *
 * DeleteAllCallBacks(host)
 *     Delete (do not break) all call backs for host.
 * 
 * CleanupTimedOutCallBacks()
 *     Delete all timed out call back entries
 *     Must be called periodically by file server.
 * 
 * BreakDelayedCallBacks(host)
 *     Break all delayed call backs for host.
 *     Returns 1: one or more failed, 0: success.
 * 
 * PrintCallBackStats()
 *     Print statistics about call backs to stdout.
 * 
 * DumpCallBacks() ---wishful thinking---
 *     Dump call back state to /tmp/callback.state.
 *     This is separately interpretable by the program pcb.
 *
 * Notes:  In general, if a call back to a host doesn't get through,
 * then HostDown, supplied elsewhere, is called.  BreakDelayedCallBacks,
 * however, does not call HostDown, but instead returns an indication of
 * success if all delayed call backs were finally broken.
 *
 * BreakDelayedCallBacks MUST be called at the first sign of activity
 * from the host after HostDown has been called (or a previous
 * BreakDelayedCallBacks failed). The BreakDelayedCallBacks must be
 * allowed to complete before any requests from that host are handled.
 * If BreakDelayedCallBacks fails, then the host should remain
 * down (and the request should be failed).

 * CleanupCallBacks MUST be called periodically by the file server for
 * this package to work correctly.  Every 5 minutes is suggested.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <stdlib.h>		/* for malloc() */
#include <time.h>		/* ANSI standard location for time stuff */
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/assert.h>

#include <afs/stds.h>

#include <afs/nfs.h>		/* yuck.  This is an abomination. */
#include <lwp.h>
#include <rx/rx.h>
#include <afscbint.h>
#include <afs/afsutil.h>
#include <lock.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include "viced_prototypes.h"
#include "viced.h"

#include <afs/ptclient.h>	/* need definition of prlist for host.h */
#include "host.h"

extern afsUUID FS_HostUUID;
extern int hostCount;
int ShowProblems = 1;

/* Maximum number of call backs to break at once, single fid */
/* There is some debate as to just how large this value should be */
/* Ideally, it would be very very large, but I am afraid that the */
/* cache managers will all send in their responses simultaneously, */
/* thereby swamping the file server.  As a result, something like */
/* 10 or 15 might be a better bet. */
#define MAX_CB_HOSTS	10

/* max time to break a callback, otherwise client is dead or net is hosed */
#define MAXCBT 25

#define u_byte	unsigned char

struct cbcounters cbstuff;

struct cbstruct {
    struct host *hp;
    afs_uint32 thead;
};

struct FileEntry {
    afs_uint32 vnode;
    afs_uint32 unique;
    afs_uint32 volid;
    afs_uint32 fnext;
    afs_uint32 ncbs;
    afs_uint32 firstcb;
    afs_uint32 status;
    afs_uint32 spare;
} *FE;				/* Don't use FE[0] */
#define FE_LATER 0x1

struct CallBack {
    afs_uint32 cnext;		/* Next call back entry */
    afs_uint32 fhead;		/* Head of this call back chain */
    u_byte thead;		/* Head of timeout chain */
    u_byte status;		/* Call back status; see definitions, below */
    afs_uint32 hhead;		/* Head of host table chain */
    afs_uint32 tprev, tnext;	/* Timeout chain */
    afs_uint32 hprev, hnext;	/* Chain from host table */
    unsigned short spare;	/* make it a multiple of 32 bits. */
} *CB;				/* Don't use CB[0] */

/* status values for status field of CallBack structure */
#define CB_NORMAL   1		/* Normal call back */
#define CB_DELAYED  2		/* Delayed call back due to rpc problems.
				 * The call back entry will be added back to the
				 * host list at the END of the list, so that
				 * searching backwards in the list will find all
				 * the (consecutive)host. delayed call back entries */
#define CB_VOLUME   3		/* Callback for a volume */
#define CB_BULK     4		/* Normal callbacks, handed out from FetchBulkStatus */

/* call back indices to pointers, and vice-versa */
#define itocb(i)    ((i)?CB+(i):0)
#define cbtoi(cbp)  (!(cbp)?0:(cbp)-CB)

/* file entry indices to pointers, and vice-versa */
#define itofe(i)    ((i)?FE+(i):0)
#define fetoi(fep)  (!(fep)?0:(fep)-FE)

/* Timeouts:  there are 128 possible timeout values in effect at any
 * given time.  Each timeout represents timeouts in an interval of 128
 * seconds.  So the maximum timeout for a call back is 128*128=16384
 * seconds, or 4 1/2 hours.  The timeout cleanup stuff is called only
 * if space runs out or by the file server every 5 minutes.  This 5
 * minute slack should be allowed for--so a maximum time of 4 hours
 * is safer.
 *
 * Timeouts must be chosen to correspond to an exact multiple
 * of 128, because all times are truncated to a 128 multiple, and
 * timed out if the current truncated time is <= to the truncated time
 * corresponding to the timeout queue.
 */

/* Unix time to Call Back time, and vice-versa.  Call back time is
   in units of 128 seconds, corresponding to time queues. */
#define CBtime(uxtime)	((uxtime)>>7)
#define UXtime(cbtime)	((cbtime)<<7)

/* Given a Unix time, compute the closest Unix time that corresponds to
   a time queue, rounding up */
#define TimeCeiling(uxtime)	(((uxtime)+127)&~127)

/* Time to live for call backs depends upon number of users of the file.
 * TimeOuts is indexed by this number/8 (using TimeOut macro).  Times
 * in this table are for the workstation; server timeouts, add
 * ServerBias */

static int TimeOuts[] = {
/* Note: don't make the first entry larger than 4 hours (see above) */
    4 * 60 * 60,		/* 0-7 users */
    1 * 60 * 60,		/* 8-15 users */
    30 * 60,			/* 16-23 users */
    15 * 60,			/* 24-31 users */
    15 * 60,			/* 32-39 users */
    10 * 60,			/* 40-47 users */
    10 * 60,			/* 48-55 users */
    10 * 60,			/* 56-63 users */
};				/* Anything more: MinTimeOut */

/* minimum time given for a call back */
static int MinTimeOut = (7 * 60);

#define TimeOutCutoff   ((sizeof(TimeOuts)/sizeof(TimeOuts[0]))*8)
#define TimeOut(nusers)  ((nusers)>=TimeOutCutoff? MinTimeOut: TimeOuts[(nusers)>>3])

/* time out at server is 3 minutes more than ws */
#define ServerBias	  (3*60)

/* Heads of CB queues; a timeout index is 1+index into this array */
static afs_uint32 timeout[128];

/* Convert cbtime to timeout queue index */
#define TIndex(cbtime)  (((cbtime)&127)+1)

/* Convert cbtime to pointer to timeout queue head */
#define THead(cbtime)	(&timeout[TIndex(cbtime)-1])

static afs_int32 tfirst;	/* cbtime of oldest unexpired call back time queue */

/* Normalize index into timeout array so that two such indices will be
   ordered correctly, so that they can be compared to see which times
   sooner, or so that the difference in time out times between them
   can be computed. */
#define TNorm(index)   ((index)<TIndex(tfirst)?(index)+128:(index))

/* This converts a timeout index into the actual time it will expire */
#define TIndexToTime(index) (UXtime(TNorm(index) - TIndex(tfirst) + tfirst))


/* Convert pointer to timeout queue head to index, and vice versa */
#define ttoi(t)		((t-timeout)+1)
#define itot(i)		((timeout)+(i-1))

/* 16 byte object get/free routines */
struct object {
    struct object *next;
};

struct VCBParams {
    struct cbstruct cba[MAX_CB_HOSTS];	/* re-entrant storage */
    unsigned int ncbas;
    afs_uint32 thead;		/* head of timeout queue for youngest callback */
    struct AFSFid *fid;
};

struct CallBack *CBfree = 0;
struct FileEntry *FEfree = 0;

/* Prototypes for static routines */
static struct FileEntry *FindFE(register AFSFid * fid);
static struct CallBack *iGetCB(register int *nused);
static int iFreeCB(register struct CallBack *cb, register int *nused);
static struct FileEntry *iGetFE(register int *nused);
static int iFreeFE(register struct FileEntry *fe, register int *nused);
static int TAdd(register struct CallBack *cb, register afs_uint32 * thead);
static int TDel(register struct CallBack *cb);
static int HAdd(register struct CallBack *cb, register struct host *host);
static int HDel(register struct CallBack *cb);
static int CDel(struct CallBack *cb, int deletefe);
static int CDelPtr(register struct FileEntry *fe, register afs_uint32 * cbp,
		   int deletefe);
static afs_uint32 *FindCBPtr(struct FileEntry *fe, struct host *host);
static int FDel(register struct FileEntry *fe);
static int AddCallBack1_r(struct host *host, AFSFid * fid, afs_uint32 * thead,
			  int type, int locked);
static void MultiBreakCallBack_r(struct cbstruct cba[], int ncbas,
				 struct AFSCBFids *afidp, struct host *xhost);
static int MultiBreakVolumeCallBack_r(struct host *host, int isheld,
				      struct VCBParams *parms, int deletefe);
static int MultiBreakVolumeCallBack(struct host *host, int isheld,
				    struct VCBParams *parms);
static int MultiBreakVolumeLaterCallBack(struct host *host, int isheld,
					 struct VCBParams *parms);
static int lih_r(register struct host *host, register int held,
		 register struct host *hostp);
static int GetSomeSpace_r(struct host *hostp, int locked);
static int ClearHostCallbacks_r(struct host *hp, int locked);

#define GetCB() ((struct CallBack *)iGetCB(&cbstuff.nCBs))
#define GetFE() ((struct FileEntry *)iGetFE(&cbstuff.nFEs))
#define FreeCB(cb) iFreeCB((struct CallBack *)cb, &cbstuff.nCBs)
#define FreeFE(fe) iFreeFE((struct FileEntry *)fe, &cbstuff.nFEs)

/* Other protos - move out sometime */
extern void ShutDown();

#define VHASH 512		/* Power of 2 */
static afs_uint32 HashTable[VHASH];	/* File entry hash table */
#define VHash(volume, unique) (((volume)+(unique))&(VHASH-1))

static struct FileEntry *
FindFE(register AFSFid * fid)
{
    int hash;
    register int fei;
    register struct FileEntry *fe;

    hash = VHash(fid->Volume, fid->Unique);
    for (fei = HashTable[hash]; fei; fei = fe->fnext) {
	fe = itofe(fei);
	if (fe->volid == fid->Volume && fe->unique == fid->Unique
	    && fe->vnode == fid->Vnode && (fe->status & FE_LATER) != FE_LATER)
	    return fe;
    }
    return 0;
}

#ifndef INTERPRET_DUMP

static struct CallBack *
iGetCB(register int *nused)
{
    register struct CallBack *ret;

    if ((ret = CBfree)) {
	CBfree = (struct CallBack *)(((struct object *)ret)->next);
	(*nused)++;
    }
    return ret;
}

static int
iFreeCB(register struct CallBack *cb, register int *nused)
{
    ((struct object *)cb)->next = (struct object *)CBfree;
    CBfree = cb;
    (*nused)--;
    return 0;
}

static struct FileEntry *
iGetFE(register int *nused)
{
    register struct FileEntry *ret;

    if ((ret = FEfree)) {
	FEfree = (struct FileEntry *)(((struct object *)ret)->next);
	(*nused)++;
    }
    return ret;
}

static int
iFreeFE(register struct FileEntry *fe, register int *nused)
{
    ((struct object *)fe)->next = (struct object *)FEfree;
    FEfree = fe;
    (*nused)--;
    return 0;
}

/* Add cb to end of specified timeout list */
static int
TAdd(register struct CallBack *cb, register afs_uint32 * thead)
{
    if (!*thead) {
	(*thead) = cb->tnext = cb->tprev = cbtoi(cb);
    } else {
	register struct CallBack *thp = itocb(*thead);

	cb->tprev = thp->tprev;
	cb->tnext = *thead;
	if (thp) {
	    if (thp->tprev)
		thp->tprev = (itocb(thp->tprev)->tnext = cbtoi(cb));
	    else
		thp->tprev = cbtoi(cb);
	}
    }
    cb->thead = ttoi(thead);
    return 0;
}

/* Delete call back entry from timeout list */
static int
TDel(register struct CallBack *cb)
{
    register afs_uint32 *thead = itot(cb->thead);

    if (*thead == cbtoi(cb))
	*thead = (*thead == cb->tnext ? 0 : cb->tnext);
    if (itocb(cb->tprev))
	itocb(cb->tprev)->tnext = cb->tnext;
    if (itocb(cb->tnext))
	itocb(cb->tnext)->tprev = cb->tprev;
    return 0;
}

/* Add cb to end of specified host list */
static int
HAdd(register struct CallBack *cb, register struct host *host)
{
    cb->hhead = h_htoi(host);
    if (!host->cblist) {
	host->cblist = cb->hnext = cb->hprev = cbtoi(cb);
    } else {
	register struct CallBack *hhp = itocb(host->cblist);

	cb->hprev = hhp->hprev;
	cb->hnext = host->cblist;
	hhp->hprev = (itocb(hhp->hprev)->hnext = cbtoi(cb));
    }
    return 0;
}

/* Delete call back entry from host list */
static int
HDel(register struct CallBack *cb)
{
    register afs_uint32 *hhead = &h_itoh(cb->hhead)->cblist;

    if (*hhead == cbtoi(cb))
	*hhead = (*hhead == cb->hnext ? 0 : cb->hnext);
    itocb(cb->hprev)->hnext = cb->hnext;
    itocb(cb->hnext)->hprev = cb->hprev;
    return 0;
}

/* Delete call back entry from fid's chain of cb's */
/* N.B.  This one also deletes the CB, and also possibly parent FE, so
 * make sure that it is not on any other list before calling this
 * routine */
static int
CDel(struct CallBack *cb, int deletefe)
{
    int cbi = cbtoi(cb);
    struct FileEntry *fe = itofe(cb->fhead);
    register afs_uint32 *cbp;
    register int safety;

    for (safety = 0, cbp = &fe->firstcb; *cbp && *cbp != cbi;
	 cbp = &itocb(*cbp)->cnext, safety++) {
	if (safety > cbstuff.nblks + 10) {
	    assert(0);
	    ViceLog(0,
		    ("CDel: Internal Error -- shutting down: wanted %d from %d, now at %d\n",
		     cbi, fe->firstcb, *cbp));
	    DumpCallBackState();
	    ShutDownAndCore(PANIC);
	}
    }
    CDelPtr(fe, cbp, deletefe);
    return 0;
}

/* Same as CDel, but pointer to parent pointer to CB entry is passed,
 * as well as file entry */
/* N.B.  This one also deletes the CB, and also possibly parent FE, so
 * make sure that it is not on any other list before calling this
 * routine */
int Ccdelpt = 0, CcdelB = 0;

static int
CDelPtr(register struct FileEntry *fe, register afs_uint32 * cbp,
	int deletefe)
{
    register struct CallBack *cb;

    if (!*cbp)
	return 0;
    Ccdelpt++;
    cb = itocb(*cbp);
    if (cb != &CB[*cbp])
	CcdelB++;
    *cbp = cb->cnext;
    FreeCB(cb);
    if (deletefe && (--fe->ncbs == 0))
	FDel(fe);
    return 0;
}

static afs_uint32 *
FindCBPtr(struct FileEntry *fe, struct host *host)
{
    register afs_uint32 hostindex = h_htoi(host);
    register struct CallBack *cb;
    register afs_uint32 *cbp;
    register int safety;

    for (safety = 0, cbp = &fe->firstcb; *cbp; cbp = &cb->cnext, safety++) {
	if (safety > cbstuff.nblks) {
	    ViceLog(0, ("FindCBPtr: Internal Error -- shutting down.\n"));
	    DumpCallBackState();
	    ShutDownAndCore(PANIC);
	}
	cb = itocb(*cbp);
	if (cb->hhead == hostindex)
	    break;
    }
    return cbp;
}

/* Delete file entry from hash table */
static int
FDel(register struct FileEntry *fe)
{
    register int fei = fetoi(fe);
    register afs_uint32 *p = &HashTable[VHash(fe->volid, fe->unique)];

    while (*p && *p != fei)
	p = &itofe(*p)->fnext;
    assert(*p);
    *p = fe->fnext;
    FreeFE(fe);
    return 0;
}

int
InitCallBack(int nblks)
{
    H_LOCK;
    tfirst = CBtime(FT_ApproxTime());
    /* N.B. The "-1", below, is because
     * FE[0] and CB[0] are not used--and not allocated */
    FE = ((struct FileEntry *)(calloc(nblks, sizeof(struct FileEntry)))) - 1;
    if (!FE) {
	ViceLog(0, ("Failed malloc in InitCallBack\n"));
	assert(0);
    }
    cbstuff.nFEs = nblks;
    while (cbstuff.nFEs)
	FreeFE(&FE[cbstuff.nFEs]);	/* This is correct */
    CB = ((struct CallBack *)(calloc(nblks, sizeof(struct CallBack)))) - 1;
    if (!CB) {
	ViceLog(0, ("Failed malloc in InitCallBack\n"));
	assert(0);
    }
    cbstuff.nCBs = nblks;
    while (cbstuff.nCBs)
	FreeCB(&CB[cbstuff.nCBs]);	/* This is correct */
    cbstuff.nblks = nblks;
    cbstuff.nbreakers = 0;
    H_UNLOCK;
    return 0;
}

afs_int32
XCallBackBulk_r(struct host * ahost, struct AFSFid * fids, afs_int32 nfids)
{
    struct AFSCallBack tcbs[AFSCBMAX];
    register int i;
    struct AFSCBFids tf;
    struct AFSCBs tc;
    int code;
    int j;
    struct rx_connection *cb_conn = NULL;

#ifdef	ADAPT_MTU
    rx_SetConnDeadTime(ahost->callback_rxcon, 4);
    rx_SetConnHardDeadTime(ahost->callback_rxcon, AFS_HARDDEADTIME);
#endif

    code = 0;
    j = 0;
    while (nfids > 0) {

	for (i = 0; i < nfids && i < AFSCBMAX; i++) {
	    tcbs[i].CallBackVersion = CALLBACK_VERSION;
	    tcbs[i].ExpirationTime = 0;
	    tcbs[i].CallBackType = CB_DROPPED;
	}
	tf.AFSCBFids_len = i;
	tf.AFSCBFids_val = &(fids[j]);
	nfids -= i;
	j += i;
	tc.AFSCBs_len = i;
	tc.AFSCBs_val = tcbs;

	cb_conn = ahost->callback_rxcon;
	rx_GetConnection(cb_conn);
	H_UNLOCK;
	code |= RXAFSCB_CallBack(cb_conn, &tf, &tc);
	rx_PutConnection(cb_conn);
	cb_conn = NULL;
	H_LOCK;
    }

    return code;
}

/* the locked flag tells us if the host entry has already been locked 
 * by our parent.  I don't think anybody actually calls us with the
 * host locked, but here's how to make that work:  GetSomeSpace has to
 * change so that it doesn't attempt to lock any hosts < "host".  That
 * means that it might be unable to free any objects, so it has to
 * return an exit status.  If it fails, then AddCallBack1 might fail,
 * as well. If so, the host->ResetDone should probably be set to 0,
 * and we probably don't want to return a callback promise to the
 * cache manager, either. */
int
AddCallBack1(struct host *host, AFSFid * fid, afs_uint32 * thead, int type,
	     int locked)
{
    int retVal;
    H_LOCK;
    if (!locked) {
	h_Lock_r(host);
    }
    retVal = AddCallBack1_r(host, fid, thead, type, 1);

    if (!locked) {
	h_Unlock_r(host);
    }
    H_UNLOCK;
    return retVal;
}

static int
AddCallBack1_r(struct host *host, AFSFid * fid, afs_uint32 * thead, int type,
	       int locked)
{
    struct FileEntry *fe;
    struct CallBack *cb = 0, *lastcb = 0;
    struct FileEntry *newfe = 0;
    afs_uint32 time_out;
    afs_uint32 *Thead = thead;
    struct CallBack *newcb = 0;
    int safety;

    host->Console |= 2;

    /* allocate these guys first, since we can't call the allocator with
     * the host structure locked -- or we might deadlock. However, we have 
     * to avoid races with FindFE... */
    while (!(newcb = GetCB())) {
	GetSomeSpace_r(host, locked);
    }
    while (!(newfe = GetFE())) {	/* Get it now, so we don't have to call */
	/* GetSomeSpace with the host locked, later.  This might turn out to */
	/* have been unneccessary, but that's actually kind of unlikely, since */
	/* most files are not shared. */
	GetSomeSpace_r(host, locked);
    }

    if (!locked) {
	h_Lock_r(host);		/* this can yield, so do it before we get any */
	/* fragile info */
    }

    fe = FindFE(fid);
    if (type == CB_NORMAL) {
	time_out =
	    TimeCeiling(FT_ApproxTime() + TimeOut(fe ? fe->ncbs : 0) +
			ServerBias);
	Thead = THead(CBtime(time_out));
    } else if (type == CB_VOLUME) {
	time_out = TimeCeiling((60 * 120 + FT_ApproxTime()) + ServerBias);
	Thead = THead(CBtime(time_out));
    } else if (type == CB_BULK) {
	/* bulk status can get so many callbacks all at once, and most of them
	 * are probably not for things that will be used for long.
	 */
	time_out =
	    TimeCeiling(FT_ApproxTime() + ServerBias +
			TimeOut(22 + (fe ? fe->ncbs : 0)));
	Thead = THead(CBtime(time_out));
    }

    host->Console &= ~2;

    if (!fe) {
	register afs_uint32 hash;

	fe = newfe;
	newfe = NULL;
	fe->firstcb = 0;
	fe->volid = fid->Volume;
	fe->vnode = fid->Vnode;
	fe->unique = fid->Unique;
	fe->ncbs = 0;
	fe->status = 0;
	hash = VHash(fid->Volume, fid->Unique);
	fe->fnext = HashTable[hash];
	HashTable[hash] = fetoi(fe);
    }
    for (safety = 0, lastcb = cb = itocb(fe->firstcb); cb;
	 lastcb = cb, cb = itocb(cb->cnext), safety++) {
	if (safety > cbstuff.nblks) {
	    ViceLog(0, ("AddCallBack1: Internal Error -- shutting down.\n"));
	    DumpCallBackState();
	    ShutDownAndCore(PANIC);
	}
	if (cb->hhead == h_htoi(host))
	    break;
    }
    if (cb) {			/* Already have call back:  move to new timeout list */
	/* don't change delayed callbacks back to normal ones */
	if (cb->status != CB_DELAYED)
	    cb->status = type;
	/* Only move if new timeout is longer */
	if (TNorm(ttoi(Thead)) > TNorm(cb->thead)) {
	    TDel(cb);
	    TAdd(cb, Thead);
	}
	if (newfe == NULL) {    /* we are using the new FE */
            fe->firstcb = cbtoi(cb);
            fe->ncbs++;
            cb->fhead = fetoi(fe);
        }
    } else {
	cb = newcb;
	newcb = NULL;
	*(lastcb ? &lastcb->cnext : &fe->firstcb) = cbtoi(cb);
	fe->ncbs++;
	cb->cnext = 0;
	cb->fhead = fetoi(fe);
	cb->status = type;
	HAdd(cb, host);
	TAdd(cb, Thead);
    }

    /* now free any still-unused callback or host entries */
    if (newcb)
	FreeCB(newcb);
    if (newfe)
	FreeFE(newfe);

    if (!locked)		/* freecb and freefe might(?) yield */
	h_Unlock_r(host);

    if (type == CB_NORMAL || type == CB_VOLUME || type == CB_BULK)
	return time_out - ServerBias;	/* Expires sooner at workstation */

    return 0;
}

/* Take an array full of hosts, all held.  Break callbacks to them, and 
 * release the holds once you're done, except don't release xhost.  xhost 
 * may be NULL.  Currently only works for a single Fid in afidp array.
 * If you want to make this work with multiple fids, you need to fix
 * the error handling.  One approach would be to force a reset if a
 * multi-fid call fails, or you could add delayed callbacks for each
 * fid.   You probably also need to sort and remove duplicate hosts.
 * When this is called from the BreakVolumeCallBacks path, it does NOT 
 * force a reset if the RPC fails, it just marks the host down and tries 
 * to create a delayed callback. */
/* N.B.  be sure that code works when ncbas == 0 */
/* N.B.  requires all the cba[*].hp pointers to be valid... */
/* This routine does not hold a lock on the host for the duration of 
 * the BreakCallBack RPC, which is a significant deviation from tradition.
 * It _does_ get a lock on the host before setting VenusDown = 1,
 * which is sufficient only if VenusDown = 0 only happens when the
 * lock is held over the RPC and the subsequent VenusDown == 0
 * wherever that is done. */
static void
MultiBreakCallBack_r(struct cbstruct cba[], int ncbas,
		     struct AFSCBFids *afidp, struct host *xhost)
{
    int i, j;
    struct rx_connection *conns[MAX_CB_HOSTS];
    static struct AFSCBs tc = { 0, 0 };
    int multi_to_cba_map[MAX_CB_HOSTS];

    assert(ncbas <= MAX_CB_HOSTS);

    /* set up conns for multi-call */
    for (i = 0, j = 0; i < ncbas; i++) {
	struct host *thishost = cba[i].hp;
	if (!thishost || (thishost->hostFlags & HOSTDELETED)) {
	    continue;
	}
	rx_GetConnection(thishost->callback_rxcon);
	multi_to_cba_map[j] = i;
	conns[j++] = thishost->callback_rxcon;

#ifdef	ADAPT_MTU
	rx_SetConnDeadTime(thishost->callback_rxcon, 4);
	rx_SetConnHardDeadTime(thishost->callback_rxcon, AFS_HARDDEADTIME);
#endif
    }

    if (j) {			/* who knows what multi would do with 0 conns? */
	cbstuff.nbreakers++;
	H_UNLOCK;
	multi_Rx(conns, j) {
	    multi_RXAFSCB_CallBack(afidp, &tc);
	    if (multi_error) {
		afs_uint32 idx;
		struct host *hp;
		char hoststr[16];

		i = multi_to_cba_map[multi_i];
		hp = cba[i].hp;
		idx = cba[i].thead;

		if (!hp || !idx) {
		    ViceLog(0,
			    ("BCB: INTERNAL ERROR: hp=%x, cba=%x, thead=%u\n", 
			     hp, cba, idx));
		} else {
		    /* 
		     ** try breaking callbacks on alternate interface addresses
		     */
		    if (MultiBreakCallBackAlternateAddress(hp, afidp)) {
			if (ShowProblems) {
			    ViceLog(7,
				    ("BCB: Failed on file %u.%u.%u, Host %s:%d is down\n",
				     afidp->AFSCBFids_val->Volume,
				     afidp->AFSCBFids_val->Vnode,
				     afidp->AFSCBFids_val->Unique,
				     afs_inet_ntoa_r(hp->host, hoststr),
				     ntohs(hp->port)));
			}

			H_LOCK;
			h_Lock_r(hp); 
			hp->hostFlags |= VENUSDOWN;
		/**
		  * We always go into AddCallBack1_r with the host locked
		  */
			AddCallBack1_r(hp, afidp->AFSCBFids_val, itot(idx),
				       CB_DELAYED, 1);
			h_Unlock_r(hp); 
			H_UNLOCK;
		    }
		}
	    }
	}
	multi_End;
	H_LOCK;
	cbstuff.nbreakers--;
    }

    for (i = 0; i < ncbas; i++) {
	struct host *hp;
	hp = cba[i].hp;
	if (hp && xhost != hp) {
	    h_Release_r(hp);
	}
    }

    /* H_UNLOCK around this so h_FreeConnection does not deadlock.
       h_FreeConnection should *never* be called on a callback connection,
       but on 10/27/04 a deadlock occurred where it was, when we know why,
       this should be reverted. -- shadow */
    H_UNLOCK;
    for (i = 0; i < j; i++) {
	rx_PutConnection(conns[i]);
    }
    H_LOCK;

    return;
}

/*
 * Break all call backs for fid, except for the specified host (unless flag
 * is true, in which case all get a callback message. Assumption: the specified
 * host is h_Held, by the caller; the others aren't.
 * Specified host may be bogus, that's ok.  This used to check to see if the 
 * host was down in two places, once right after the host was h_held, and 
 * again after it was locked.  That race condition is incredibly rare and
 * relatively harmless even when it does occur, so we don't check for it now. 
 */
/* if flag is true, send a break callback msg to "host", too */
int
BreakCallBack(struct host *xhost, AFSFid * fid, int flag)
{
    struct FileEntry *fe;
    struct CallBack *cb, *nextcb;
    struct cbstruct cba[MAX_CB_HOSTS];
    int ncbas;
    struct AFSCBFids tf;
    int hostindex;
    char hoststr[16];

    ViceLog(7,
	    ("BCB: BreakCallBack(all but %s:%d, (%u,%u,%u))\n",
	     afs_inet_ntoa_r(xhost->host, hoststr), ntohs(xhost->port),
	     fid->Volume, fid->Vnode, fid->Unique));

    H_LOCK;
    cbstuff.BreakCallBacks++;
    fe = FindFE(fid);
    if (!fe) {
	goto done;
    }
    hostindex = h_htoi(xhost);
    cb = itocb(fe->firstcb);
    if (!cb || ((fe->ncbs == 1) && (cb->hhead == hostindex) && !flag)) {
	/* the most common case is what follows the || */
	goto done;
    }
    tf.AFSCBFids_len = 1;
    tf.AFSCBFids_val = fid;

    for (; cb;) {
	for (ncbas = 0; cb && ncbas < MAX_CB_HOSTS; cb = nextcb) {
	    nextcb = itocb(cb->cnext);
	    if ((cb->hhead != hostindex || flag)
		&& (cb->status == CB_BULK || cb->status == CB_NORMAL
		    || cb->status == CB_VOLUME)) {
		struct host *thishost = h_itoh(cb->hhead);
		if (!thishost) {
		    ViceLog(0, ("BCB: BOGUS! cb->hhead is NULL!\n"));
		} else if (thishost->hostFlags & VENUSDOWN) {
		    ViceLog(7,
			    ("BCB: %s:%d is down; delaying break call back\n",
			     afs_inet_ntoa_r(thishost->host, hoststr),
			     ntohs(thishost->port)));
		    cb->status = CB_DELAYED;
		} else {
		    h_Hold_r(thishost);
		    cba[ncbas].hp = thishost;
		    cba[ncbas].thead = cb->thead;
		    ncbas++;
		    TDel(cb);
		    HDel(cb);
		    CDel(cb, 1);	/* Usually first; so this delete 
					 * is reasonably inexpensive */
		}
	    }
	}

	if (ncbas) {
	    MultiBreakCallBack_r(cba, ncbas, &tf, xhost);

	    /* we need to to all these initializations again because MultiBreakCallBack may block */
	    fe = FindFE(fid);
	    if (!fe) {
		goto done;
	    }
	    cb = itocb(fe->firstcb);
	    if (!cb || ((fe->ncbs == 1) && (cb->hhead == hostindex) && !flag)) {
		/* the most common case is what follows the || */
		goto done;
	    }
	}
    }

  done:
    H_UNLOCK;
    return 0;
}

/* Delete (do not break) single call back for fid */
int
DeleteCallBack(struct host *host, AFSFid * fid)
{
    register struct FileEntry *fe;
    register afs_uint32 *pcb;
    char hoststr[16];

    cbstuff.DeleteCallBacks++;

    H_LOCK;
    h_Lock_r(host);
    fe = FindFE(fid);
    if (!fe) {
	h_Unlock_r(host);
	H_UNLOCK;
	ViceLog(8,
		("DCB: No call backs for fid (%u, %u, %u)\n", fid->Volume,
		 fid->Vnode, fid->Unique));
	return 0;
    }
    pcb = FindCBPtr(fe, host);
    if (!*pcb) {
	ViceLog(8,
		("DCB: No call back for host %s:%d, (%u, %u, %u)\n",
		 afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port),
		 fid->Volume, fid->Vnode, fid->Unique));
	h_Unlock_r(host);
	H_UNLOCK;
	return 0;
    }
    HDel(itocb(*pcb));
    TDel(itocb(*pcb));
    CDelPtr(fe, pcb, 1);
    h_Unlock_r(host);
    H_UNLOCK;
    return 0;
}

/*
 * Delete (do not break) all call backs for fid.  This call doesn't
 * set all of the various host locks, but it shouldn't really matter
 * since we're not adding callbacks, but deleting them.  I'm not sure
 * why it doesn't set the lock, however; perhaps it should.
 */
int
DeleteFileCallBacks(AFSFid * fid)
{
    register struct FileEntry *fe;
    register struct CallBack *cb;
    register afs_uint32 cbi;
    register int n;

    H_LOCK;
    cbstuff.DeleteFiles++;
    fe = FindFE(fid);
    if (!fe) {
	H_UNLOCK;
	ViceLog(8,
		("DF: No fid (%u,%u,%u) to delete\n", fid->Volume, fid->Vnode,
		 fid->Unique));
	return 0;
    }
    for (n = 0, cbi = fe->firstcb; cbi; n++) {
	cb = itocb(cbi);
	cbi = cb->cnext;
	TDel(cb);
	HDel(cb);
	FreeCB(cb);
    }
    FDel(fe);
    H_UNLOCK;
    return 0;
}

/* Delete (do not break) all call backs for host.  The host should be
 * locked. */
int
DeleteAllCallBacks_r(struct host *host, int deletefe)
{
    register struct CallBack *cb;
    register int cbi, first;

    cbstuff.DeleteAllCallBacks++;
    cbi = first = host->cblist;
    if (!cbi) {
	ViceLog(8, ("DV: no call backs\n"));
	return 0;
    }
    do {
	cb = itocb(cbi);
	cbi = cb->hnext;
	TDel(cb);
	CDel(cb, deletefe);
    } while (cbi != first);
    host->cblist = 0;
    return 0;
}

/*
 * Break all delayed call backs for host.  Returns 1 if all call backs
 * successfully broken; 0 otherwise.  Assumes host is h_Held and h_Locked.
 * Must be called with VenusDown set for this host
 */
int
BreakDelayedCallBacks(struct host *host)
{
    int retVal;
    H_LOCK;
    retVal = BreakDelayedCallBacks_r(host);
    H_UNLOCK;
    return retVal;
}

int
BreakDelayedCallBacks_r(struct host *host)
{
    struct AFSFid fids[AFSCBMAX];
    u_byte thead[AFSCBMAX];	/* This should match thead in struct Callback */
    int cbi, first, nfids;
    struct CallBack *cb;
    int code;
    char hoststr[16];
    struct rx_connection *cb_conn;

    cbstuff.nbreakers++;
    if (!(host->hostFlags & RESETDONE) && !(host->hostFlags & HOSTDELETED)) {
	host->hostFlags &= ~ALTADDR;	/* alternate addresses are invalid */
	cb_conn = host->callback_rxcon;
	rx_GetConnection(cb_conn);
	if (host->interface) {
	    H_UNLOCK;
	    code =
		RXAFSCB_InitCallBackState3(cb_conn, &FS_HostUUID);
	} else {
	    H_UNLOCK;
	    code = RXAFSCB_InitCallBackState(cb_conn);
	}
	rx_PutConnection(cb_conn);
	cb_conn = NULL;
	H_LOCK;
	host->hostFlags |= ALTADDR;	/* alternate addresses are valid */
	if (code) {
	    if (ShowProblems) {
		ViceLog(0,
			("CB: Call back connect back failed (in break delayed) for Host %s:%d\n",
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port)));
	    }
	    host->hostFlags |= VENUSDOWN;
	} else {
	    ViceLog(25,
		    ("InitCallBackState success on %s\n",
		     afs_inet_ntoa_r(host->host, hoststr)));
	    /* reset was done successfully */
	    host->hostFlags |= RESETDONE;
	    host->hostFlags &= ~VENUSDOWN;
	}
    } else
	while (!(host->hostFlags & HOSTDELETED)) {
	    nfids = 0;
	    host->hostFlags &= ~VENUSDOWN;	/* presume up */
	    cbi = first = host->cblist;
	    if (!cbi)
		break;
	    do {
		first = host->cblist;
		cb = itocb(cbi);
		cbi = cb->hnext;
		if (cb->status == CB_DELAYED) {
		    register struct FileEntry *fe = itofe(cb->fhead);
		    thead[nfids] = cb->thead;
		    fids[nfids].Volume = fe->volid;
		    fids[nfids].Vnode = fe->vnode;
		    fids[nfids].Unique = fe->unique;
		    nfids++;
		    HDel(cb);
		    TDel(cb);
		    CDel(cb, 1);
		}
	    } while (cbi && cbi != first && nfids < AFSCBMAX);

	    if (nfids == 0) {
		break;
	    }

	    if (XCallBackBulk_r(host, fids, nfids)) {
		/* Failed, again:  put them back, probably with old
		 * timeout values */
		int i;
		if (ShowProblems) {
		    ViceLog(0,
			    ("CB: XCallBackBulk failed, Host %s:%d; callback list follows:\n",
			     afs_inet_ntoa_r(host->host, hoststr),
			     ntohs(host->port)));
		}
		for (i = 0; i < nfids; i++) {
		    if (ShowProblems) {
			ViceLog(0,
				("CB: Host %s:%d, file %u.%u.%u (part of bulk callback)\n",
				 afs_inet_ntoa_r(host->host, hoststr),
				 ntohs(host->port), fids[i].Volume,
				 fids[i].Vnode, fids[i].Unique));
		    }
		    /* used to do this:
		     * AddCallBack1_r(host, &fids[i], itot(thead[i]), CB_DELAYED, 1);
		     * * but it turns out to cause too many tricky locking problems.
		     * * now, if break delayed fails, screw it. */
		}
		host->hostFlags |= VENUSDOWN;	/* Failed */
		ClearHostCallbacks_r(host, 1 /* locked */ );
		nfids = 0;
		break;
	    }
	    if (nfids < AFSCBMAX)
		break;
	}

    cbstuff.nbreakers--;
    /* If we succeeded it's always ok to unset HFE_LATER */
    if (!host->hostFlags & VENUSDOWN)
	host->hostFlags &= ~HFE_LATER;
    return (host->hostFlags & VENUSDOWN);
}

/*
** isheld is 0 if the host is held in h_Enumerate
** isheld is 1 if the host is held in BreakVolumeCallBacks
*/
static int
MultiBreakVolumeCallBack_r(struct host *host, int isheld,
			   struct VCBParams *parms, int deletefe)
{
    char hoststr[16];

    if (!isheld)
	return isheld;		/* host is held only by h_Enumerate, do nothing */
    if (host->hostFlags & HOSTDELETED)
	return 0;		/* host is deleted, release hold */

    if (host->hostFlags & VENUSDOWN) {
	h_Lock_r(host);
	if (host->hostFlags & HOSTDELETED) {
	    h_Unlock_r(host);
	    return 0;		/* Release hold */
	}
	ViceLog(8,
		("BVCB: volume call back for Host %s:%d failed\n",
		 afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port)));
	if (ShowProblems) {
	    ViceLog(0,
		    ("CB: volume callback for Host %s:%d failed\n",
		     afs_inet_ntoa_r(host->host, hoststr),
		     ntohs(host->port)));
	}
	DeleteAllCallBacks_r(host, deletefe);	/* Delete all callback state 
						 * rather than attempting to 
						 * selectively remember to
						 * delete the volume callbacks
						 * later */
	host->hostFlags &= ~RESETDONE;	/* Do InitCallBackState when host returns */
	h_Unlock_r(host);
	return 0;		/* release hold */
    }
    assert(parms->ncbas <= MAX_CB_HOSTS);

    /* Do not call MultiBreakCallBack on the current host structure
     ** because it would prematurely release the hold on the host
     */
    if (parms->ncbas == MAX_CB_HOSTS) {
	struct AFSCBFids tf;

	tf.AFSCBFids_len = 1;
	tf.AFSCBFids_val = parms->fid;

	/* this releases all the hosts */
	MultiBreakCallBack_r(parms->cba, parms->ncbas, &tf, 0 /* xhost */ );

	parms->ncbas = 0;
    }
    parms->cba[parms->ncbas].hp = host;
    parms->cba[(parms->ncbas)++].thead = parms->thead;
    return 1;			/* DON'T release hold, because we still need it. */
}

/*
** isheld is 0 if the host is held in h_Enumerate
** isheld is 1 if the host is held in BreakVolumeCallBacks
*/
static int
MultiBreakVolumeCallBack(struct host *host, int isheld,
			 struct VCBParams *parms)
{
    int retval;
    H_LOCK;
    retval = MultiBreakVolumeCallBack_r(host, isheld, parms, 1);
    H_UNLOCK;
    return retval;
}

/*
** isheld is 0 if the host is held in h_Enumerate
** isheld is 1 if the host is held in BreakVolumeCallBacks
*/
static int
MultiBreakVolumeLaterCallBack(struct host *host, int isheld,
			      struct VCBParams *parms)
{
    int retval;
    H_LOCK;
    retval = MultiBreakVolumeCallBack_r(host, isheld, parms, 0);
    H_UNLOCK;
    return retval;
}

/*
 * Break all call backs on a single volume.  Don't call this with any
 * hosts h_held.  Note that this routine clears the callbacks before
 * actually breaking them, and that the vnode isn't locked during this
 * operation, so that people might see temporary callback loss while
 * this function is executing.  It is just a temporary state, however,
 * since the callback will be broken later by this same function.
 *
 * Now uses multi-RX for CallBack RPC.  Note that the
 * multiBreakCallBacks routine does not force a reset if the RPC
 * fails, unlike the previous version of this routine, but does create
 * a delayed callback.  Resets will be forced if the host is
 * determined to be down before the RPC is executed.
 */
int
BreakVolumeCallBacks(afs_uint32 volume)
{
    struct AFSFid fid;
    int hash;
    afs_uint32 *feip;
    struct CallBack *cb;
    struct FileEntry *fe;
    struct host *host;
    struct VCBParams henumParms;
    afs_uint32 tthead = 0;	/* zero is illegal value */

    H_LOCK;
    fid.Volume = volume, fid.Vnode = fid.Unique = 0;
    for (hash = 0; hash < VHASH; hash++) {
	for (feip = &HashTable[hash]; (fe = itofe(*feip));) {
	    if (fe->volid == volume) {
		register struct CallBack *cbnext;
		for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
		    host = h_itoh(cb->hhead);
		    h_Hold_r(host);
		    cbnext = itocb(cb->cnext);
		    if (!tthead || (TNorm(tthead) < TNorm(cb->thead))) {
			tthead = cb->thead;
		    }
		    TDel(cb);
		    HDel(cb);
		    FreeCB(cb);
		    /* leave hold for MultiBreakVolumeCallBack to clear */
		}
		*feip = fe->fnext;
		FreeFE(fe);
	    } else {
		feip = &fe->fnext;
	    }
	}
    }

    if (!tthead) {
	/* didn't find any callbacks, so return right away. */
	H_UNLOCK;
	return 0;
    }
    henumParms.ncbas = 0;
    henumParms.fid = &fid;
    henumParms.thead = tthead;
    H_UNLOCK;
    h_Enumerate(MultiBreakVolumeCallBack, (char *)&henumParms);
    H_LOCK;
    if (henumParms.ncbas) {	/* do left-overs */
	struct AFSCBFids tf;
	tf.AFSCBFids_len = 1;
	tf.AFSCBFids_val = &fid;

	MultiBreakCallBack_r(henumParms.cba, henumParms.ncbas, &tf, 0);

	henumParms.ncbas = 0;
    }
    H_UNLOCK;
    return 0;
}

#ifdef AFS_PTHREAD_ENV
extern pthread_cond_t fsync_cond;
#else
extern char fsync_wait[];
#endif

int
BreakVolumeCallBacksLater(afs_uint32 volume)
{
    int hash;
    afs_int32 *feip;
    struct FileEntry *fe;
    struct CallBack *cb;
    struct host *host;
    int found = 0;

    ViceLog(25, ("Setting later on volume %u\n", volume));
    H_LOCK;
    for (hash = 0; hash < VHASH; hash++) {
	for (feip = &HashTable[hash]; fe = itofe(*feip);) {
	    if (fe->volid == volume) {
		register struct CallBack *cbnext;
		for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
		    host = h_itoh(cb->hhead);
		    host->hostFlags |= HFE_LATER;
		    cb->status = CB_DELAYED;
		    cbnext = itocb(cb->cnext);
		}
		FSYNC_LOCK;
		fe->status |= FE_LATER;
		FSYNC_UNLOCK;
		found++;
	    }
	    feip = &fe->fnext;
	}
    }
    H_UNLOCK;
    if (!found) {
	/* didn't find any callbacks, so return right away. */
	return 0;
    }

    ViceLog(25, ("Fsync thread wakeup\n"));
#ifdef AFS_PTHREAD_ENV
    FSYNC_LOCK;
    assert(pthread_cond_broadcast(&fsync_cond) == 0);
    FSYNC_UNLOCK;
#else
    LWP_NoYieldSignal(fsync_wait);
#endif
    return 0;
}

int
BreakLaterCallBacks(void)
{
    struct AFSFid fid;
    int hash;
    afs_int32 *feip;
    struct CallBack *cb;
    struct FileEntry *fe = NULL;
    struct FileEntry *myfe = NULL;
    struct host *host;
    struct VCBParams henumParms;
    unsigned short tthead = 0;	/* zero is illegal value */
    char hoststr[16];

    /* Unchain first */
    ViceLog(25, ("Looking for FileEntries to unchain\n"));
    H_LOCK;
    FSYNC_LOCK;
    /* Pick the first volume we see to clean up */
    fid.Volume = fid.Vnode = fid.Unique = 0;

    for (hash = 0; hash < VHASH; hash++) {
	for (feip = &HashTable[hash]; fe = itofe(*feip);) {
	    if (fe && (fe->status & FE_LATER)
		&& (fid.Volume == 0 || fid.Volume == fe->volid)) {
		/* Ugly, but used to avoid left side casting */
		struct object *tmpfe;
		ViceLog(125,
			("Unchaining for %u:%u:%u\n", fe->vnode, fe->unique,
			 fe->volid));
		fid.Volume = fe->volid;
		*feip = fe->fnext;
		/* Works since volid is deeper than the largest pointer */
		tmpfe = (struct object *)fe;
		tmpfe->next = (struct object *)myfe;
		myfe = fe;
	    } else
		feip = &fe->fnext;
	}
    }
    FSYNC_UNLOCK;

    if (!myfe) {
	H_UNLOCK;
	return 0;
    }

    /* loop over FEs from myfe and free/break */
    tthead = 0;
    for (fe = myfe; fe;) {
	register struct CallBack *cbnext;
	for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
	    cbnext = itocb(cb->cnext);
	    host = h_itoh(cb->hhead);
	    if (cb->status == CB_DELAYED) {
		h_Hold_r(host);
		if (!tthead || (TNorm(tthead) < TNorm(cb->thead))) {
		    tthead = cb->thead;
		}
		TDel(cb);
		HDel(cb);
		CDel(cb, 0);	/* Don't let CDel clean up the fe */
		/* leave hold for MultiBreakVolumeCallBack to clear */
	    } else {
		ViceLog(125,
			("Found host %s:%d non-DELAYED cb for %u:%u:%u\n", 
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port), fe->vnode, fe->unique, fe->volid));
	    }
	}
	myfe = fe;
	fe = (struct FileEntry *)((struct object *)fe)->next;
	FreeFE(myfe);
    }

    if (tthead) {
	ViceLog(125, ("Breaking volume %u\n", fid.Volume));
	henumParms.ncbas = 0;
	henumParms.fid = &fid;
	henumParms.thead = tthead;
	H_UNLOCK;
	h_Enumerate(MultiBreakVolumeLaterCallBack, (char *)&henumParms);
	H_LOCK;
	if (henumParms.ncbas) {	/* do left-overs */
	    struct AFSCBFids tf;
	    tf.AFSCBFids_len = 1;
	    tf.AFSCBFids_val = &fid;

	    MultiBreakCallBack_r(henumParms.cba, henumParms.ncbas, &tf, 0);
	    henumParms.ncbas = 0;
	}
    }
    H_UNLOCK;

    /* Arrange to be called again */
    return 1;
}

/*
 * Delete all timed-out call back entries (to be called periodically by file
 * server)
 */
int
CleanupTimedOutCallBacks(void)
{
    H_LOCK;
    CleanupTimedOutCallBacks_r();
    H_UNLOCK;
    return 0;
}

int
CleanupTimedOutCallBacks_r(void)
{
    afs_uint32 now = CBtime(FT_ApproxTime());
    register afs_uint32 *thead;
    register struct CallBack *cb;
    register int ntimedout = 0;
    char hoststr[16];

    while (tfirst <= now) {
	register int cbi;
	cbi = *(thead = THead(tfirst));
	if (cbi) {
	    do {
		cb = itocb(cbi);
		cbi = cb->tnext;
		ViceLog(8,
			("CCB: deleting timed out call back %s:%d, (%u,%u,%u)\n",
			 afs_inet_ntoa_r(h_itoh(cb->hhead)->host, hoststr),
			 h_itoh(cb->hhead)->port, itofe(cb->fhead)->volid,
			 itofe(cb->fhead)->vnode, itofe(cb->fhead)->unique));
		HDel(cb);
		CDel(cb, 1);
		ntimedout++;
		if (ntimedout > cbstuff.nblks) {
		    ViceLog(0, ("CCB: Internal Error -- shutting down...\n"));
		    DumpCallBackState();
		    ShutDownAndCore(PANIC);
		}
	    } while (cbi != *thead);
	    *thead = 0;
	}
	tfirst++;
    }
    cbstuff.CBsTimedOut += ntimedout;
    ViceLog(7, ("CCB: deleted %d timed out callbacks\n", ntimedout));
    return (ntimedout > 0);
}

static struct host *lih_host;
static int lih_host_held;

static int
lih_r(register struct host *host, register int held,
      register struct host *hostp)
{
    if (host->cblist
	&& ((hostp && host != hostp) || (!held && !h_OtherHolds_r(host)))
	&& (!lih_host || host->ActiveCall < lih_host->ActiveCall)) {
	if (lih_host != NULL && lih_host_held) {
	    h_Release_r(lih_host);
	}
	lih_host = host;
	lih_host_held = !held;
	held = 1;
    }
    return held;
}

/* This version does not allow 'host' to be selected unless its ActiveCall 
 * is newer than 'hostp' which is the host with the oldest ActiveCall from
 * the last pass (if it is provided).  We filter out any hosts that are
 * are held by other threads.
 */
static int
lih0_r(register struct host *host, register int held,
      register struct host *hostp)
{
    if (host->cblist
	&& (hostp && host != hostp) 
	&& (!held && !h_OtherHolds_r(host))
	&& (!lih_host || host->ActiveCall < lih_host->ActiveCall) 
	&& (!hostp || host->ActiveCall > hostp->ActiveCall)) {
	if (lih_host != NULL && lih_host_held) {
	    h_Release_r(lih_host);
	}
	lih_host = host;
	lih_host_held = !held;
	held = 1;
    }
    return held;
}

/* This version does not allow 'host' to be selected unless its ActiveCall 
 * is newer than 'hostp' which is the host with the oldest ActiveCall from
 * the last pass (if it is provided).  In this second varient, we do not 
 * prevent held hosts from being selected.
 */
static int
lih1_r(register struct host *host, register int held,
      register struct host *hostp)
{
    if (host->cblist
	&& (hostp && host != hostp) 
	&& (!lih_host || host->ActiveCall < lih_host->ActiveCall) 
	&& (!hostp || host->ActiveCall > hostp->ActiveCall)) {
	if (lih_host != NULL && lih_host_held) {
	    h_Release_r(lih_host);
	}
	lih_host = host;
	lih_host_held = !held;
	held = 1;
    }
    return held;
}

/* This could be upgraded to get more space each time */
/* first pass: sequentially find the oldest host which isn't held by
               anyone for which we can clear callbacks;
	       skipping 'hostp' */
/* second pass: sequentially find the oldest host regardless of 
               whether or not the host is held; skipping 'hostp' */
/* third pass: attempt to clear callbacks from 'hostp' */
/* always called with hostp unlocked */

/* Note: hostlist is ordered most recently created host first and 
 * its order has no relationship to the most recently used. */
extern struct host *hostList;
static int
GetSomeSpace_r(struct host *hostp, int locked)
{
    register struct host *hp, *hp1, *hp2;
    int i = 0;

    cbstuff.GotSomeSpaces++;
    ViceLog(5,
	    ("GSS: First looking for timed out call backs via CleanupCallBacks\n"));
    if (CleanupTimedOutCallBacks_r()) {
	cbstuff.GSS3++;
	return 0;
    }

    i = 0;
    hp1 = NULL;
    hp2 = hostList;
    do {
	lih_host = 0;
	h_Enumerate_r(i == 0 ? lih0_r : lih1_r, hp2, (char *)hp1);
	hp = lih_host;
	if (hp) {
	    /* set in lih_r! private copy before giving up H_LOCK */
	    int lih_host_held2=lih_host_held;   
	    cbstuff.GSS4++;
	    if ((hp != hostp) && !ClearHostCallbacks_r(hp, 0 /* not locked or held */ )) {
		if (lih_host_held2)
		    h_Release_r(hp);
		return 0;
	    }
	    if (lih_host_held2)
		h_Release_r(hp);
	    hp1 = hp;
	    hp2 = hostList;
	} else {
	    /*
	     * Next time try getting callbacks from any host even if
	     * it's deleted (that's actually great since we can freely
	     * remove its callbacks) or it's held since the only other
	     * option is starvation for the file server (i.e. until the
	     * callback timeout arrives).
	     */
	    i++;
	    hp1 = NULL;
	    hp2 = hostList;
	    cbstuff.GSS1++;
	    ViceLog(5,
		    ("GSS: Try harder for longest inactive host cnt= %d\n",
		     i));
	}
    } while (i < 2);

    /* Could not obtain space from other hosts, clear hostp's callback state */
    cbstuff.GSS2++;
    if (!locked) {
	h_Lock_r(hostp);
    }
    ClearHostCallbacks_r(hostp, 1 /*already locked */ );
    if (!locked) {
	h_Unlock_r(hostp);
    }
    return 0;
}

/* locked - set if caller has already locked the host */
static int
ClearHostCallbacks_r(struct host *hp, int locked)
{
    int code;
    int held = 0;
    char hoststr[16];
    struct rx_connection *cb_conn = NULL;

    ViceLog(5,
	    ("GSS: Delete longest inactive host %s\n",
	     afs_inet_ntoa_r(hp->host, hoststr)));
    if (!(held = h_Held_r(hp)))
	h_Hold_r(hp);

    /** Try a non-blocking lock. If the lock is already held return
      * after releasing hold on hp
      */
    if (!locked) {
	if (h_NBLock_r(hp)) {
	    if (!held)
		h_Release_r(hp);
	    return 1;
	}
    }
    if (hp->Console & 2) {
	/*
	 * If the special console field is set it means that a thread
	 * is waiting in AddCallBack1 after it set pointers to the
	 * file entry and/or callback entry. Because of the bogus
	 * usage of h_hold it won't prevent from another thread, this
	 * one, to remove all the callbacks so just to be safe we keep
	 * a reference. NOTE, on the last phase we'll free the calling
	 * host's callbacks but that's ok...
	 */
	cbstuff.GSS5++;
    }
    DeleteAllCallBacks_r(hp, 1);
    if (hp->hostFlags & VENUSDOWN) {
	hp->hostFlags &= ~RESETDONE;	/* remember that we must do a reset */
    } else {
	/* host is up, try a call */
	hp->hostFlags &= ~ALTADDR;	/* alternate addresses are invalid */
	cb_conn = hp->callback_rxcon;
	rx_GetConnection(hp->callback_rxcon);
	if (hp->interface) {
	    H_UNLOCK;
	    code =
		RXAFSCB_InitCallBackState3(cb_conn, &FS_HostUUID);
	} else {
	    H_UNLOCK;
	    code = RXAFSCB_InitCallBackState(cb_conn);
	}
	rx_PutConnection(cb_conn);
	cb_conn = NULL;
	H_LOCK;
	hp->hostFlags |= ALTADDR;	/* alternate addresses are valid */
	if (code) {
	    /* failed, mark host down and need reset */
	    hp->hostFlags |= VENUSDOWN;
	    hp->hostFlags &= ~RESETDONE;
	} else {
	    /* reset succeeded, we're done */
	    hp->hostFlags |= RESETDONE;
	}
    }
    if (!locked) {
	h_Unlock_r(hp);
    }
    if (!held)
	h_Release_r(hp);

    return 0;
}
#endif /* INTERPRET_DUMP */


int
PrintCallBackStats(void)
{
    fprintf(stderr,
	    "%d add CB, %d break CB, %d del CB, %d del FE, %d CB's timed out, %d space reclaim, %d del host\n",
	    cbstuff.AddCallBacks, cbstuff.BreakCallBacks,
	    cbstuff.DeleteCallBacks, cbstuff.DeleteFiles, cbstuff.CBsTimedOut,
	    cbstuff.GotSomeSpaces, cbstuff.DeleteAllCallBacks);
    fprintf(stderr, "%d CBs, %d FEs, (%d of total of %d 16-byte blocks)\n",
	    cbstuff.nCBs, cbstuff.nFEs, cbstuff.nCBs + cbstuff.nFEs,
	    cbstuff.nblks);

    return 0;
}

#define MAGIC 0x12345678	/* To check byte ordering of dump when it is read in */

#ifndef INTERPRET_DUMP

int
DumpCallBackState(void)
{
    int fd;
    afs_uint32 magic = MAGIC, now = FT_ApproxTime(), freelisthead;

    fd = open(AFSDIR_SERVER_CBKDUMP_FILEPATH, O_WRONLY | O_CREAT | O_TRUNC,
	      0666);
    if (fd < 0) {
	ViceLog(0,
		("Couldn't create callback dump file %s\n",
		 AFSDIR_SERVER_CBKDUMP_FILEPATH));
	return 0;
    }
    (void)write(fd, &magic, sizeof(magic));
    (void)write(fd, &now, sizeof(now));
    (void)write(fd, &cbstuff, sizeof(cbstuff));
    (void)write(fd, TimeOuts, sizeof(TimeOuts));
    (void)write(fd, timeout, sizeof(timeout));
    (void)write(fd, &tfirst, sizeof(tfirst));
    freelisthead = cbtoi((struct CallBack *)CBfree);
    (void)write(fd, &freelisthead, sizeof(freelisthead));	/* This is a pointer */
    freelisthead = fetoi((struct FileEntry *)FEfree);
    (void)write(fd, &freelisthead, sizeof(freelisthead));	/* This is a pointer */
    (void)write(fd, HashTable, sizeof(HashTable));
    (void)write(fd, &CB[1], sizeof(CB[1]) * cbstuff.nblks);	/* CB stuff */
    (void)write(fd, &FE[1], sizeof(FE[1]) * cbstuff.nblks);	/* FE stuff */
    close(fd);

    return 0;
}

#endif

#ifdef INTERPRET_DUMP

/* This is only compiled in for the callback analyzer program */
/* Returns the time of the dump */
time_t
ReadDump(char *file)
{
    int fd;
    afs_uint32 magic, freelisthead;
    time_t now;

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Couldn't read dump file %s\n", file);
	exit(1);
    }
    read(fd, &magic, sizeof(magic));
    if (magic != MAGIC) {
	fprintf(stderr,
		"Magic number of %s is invalid.  You might be trying to\n",
		file);
	fprintf(stderr,
		"run this program on a machine type with a different byte ordering.\n");
	exit(1);
    }
    read(fd, &now, sizeof(now));
    read(fd, &cbstuff, sizeof(cbstuff));
    read(fd, TimeOuts, sizeof(TimeOuts));
    read(fd, timeout, sizeof(timeout));
    read(fd, &tfirst, sizeof(tfirst));
    read(fd, &freelisthead, sizeof(freelisthead));
    CB = ((struct CallBack
	   *)(calloc(cbstuff.nblks, sizeof(struct FileEntry)))) - 1;
    FE = ((struct FileEntry
	   *)(calloc(cbstuff.nblks, sizeof(struct FileEntry)))) - 1;
    CBfree = (struct CallBack *)itocb(freelisthead);
    read(fd, &freelisthead, sizeof(freelisthead));
    FEfree = (struct FileEntry *)itofe(freelisthead);
    read(fd, HashTable, sizeof(HashTable));
    read(fd, &CB[1], sizeof(CB[1]) * cbstuff.nblks);	/* CB stuff */
    read(fd, &FE[1], sizeof(FE[1]) * cbstuff.nblks);	/* FE stuff */
    if (close(fd)) {
	perror("Error reading dumpfile");
	exit(1);
    }
    return now;
}

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    int err = 0, cbi = 0, stats = 0, noptions = 0, all = 0, vol = 0, raw = 0;
    static AFSFid fid;
    register struct FileEntry *fe;
    register struct CallBack *cb;
    time_t now;

    memset(&fid, 0, sizeof(fid));
    argc--;
    argv++;
    while (argc && **argv == '-') {
	noptions++;
	argc--;
	if (!strcmp(*argv, "-host")) {
	    if (argc < 1) {
		err++;
		break;
	    }
	    argc--;
	    cbi = atoi(*++argv);
	} else if (!strcmp(*argv, "-fid")) {
	    if (argc < 2) {
		err++;
		break;
	    }
	    argc -= 3;
	    fid.Volume = atoi(*++argv);
	    fid.Vnode = atoi(*++argv);
	    fid.Unique = atoi(*++argv);
	} else if (!strcmp(*argv, "-time")) {
	    fprintf(stderr, "-time not supported\n");
	    exit(1);
	} else if (!strcmp(*argv, "-stats")) {
	    stats = 1;
	} else if (!strcmp(*argv, "-all")) {
	    all = 1;
	} else if (!strcmp(*argv, "-raw")) {
	    raw = 1;
	} else if (!strcmp(*argv, "-volume")) {
	    if (argc < 1) {
		err++;
		break;
	    }
	    argc--;
	    vol = atoi(*++argv);
	} else
	    err++;
	argv++;
    }
    if (err || argc != 1) {
	fprintf(stderr,
		"Usage: cbd [-host cbid] [-fid volume vnode] [-stats] [-all] callbackdumpfile\n");
	fprintf(stderr,
		"[cbid is shown for each host in the hosts.dump file]\n");
	exit(1);
    }
    now = ReadDump(*argv);
    if (stats || noptions == 0) {
	time_t uxtfirst = UXtime(tfirst);
	printf("The time of the dump was %u %s", now, ctime(&now));
	printf("The last time cleanup ran was %u %s", uxtfirst,
	       ctime(&uxtfirst));
	PrintCallBackStats();
    }
    if (all || vol) {
	register hash;
	register afs_uint32 *feip;
	register struct CallBack *cb;
	register struct FileEntry *fe;

	for (hash = 0; hash < VHASH; hash++) {
	    for (feip = &HashTable[hash]; fe = itofe(*feip);) {
		if (!vol || (fe->volid == vol)) {
		    register struct CallBack *cbnext;
		    for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
			PrintCB(cb, now);
			cbnext = itocb(cb->cnext);
		    }
		    *feip = fe->fnext;
		} else {
		    feip = &fe->fnext;
		}
	    }
	}
    }
    if (cbi) {
	afs_uint32 cfirst = cbi;
	do {
	    cb = itocb(cbi);
	    PrintCB(cb, now);
	    cbi = cb->hnext;
	} while (cbi != cfirst);
    }
    if (fid.Volume) {
	fe = FindFE(&fid);
	if (!fe) {
	    printf("No callback entries for %u.%u\n", fid.Volume, fid.Vnode);
	    exit(1);
	}
	cb = itocb(fe->firstcb);
	while (cb) {
	    PrintCB(cb, now);
	    cb = itocb(cb->cnext);
	}
    }
    if (raw) {
	struct FileEntry *fe;
	afs_int32 *p, i;
	for (i = 1; i < cbstuff.nblks; i++) {
	    p = (afs_int32 *) & FE[i];
	    printf("%d:%12x%12x%12x%12x\n", i, p[0], p[1], p[2], p[3]);
	}
    }
}

int
PrintCB(register struct CallBack *cb, afs_uint32 now)
{
    struct FileEntry *fe = itofe(cb->fhead);
    time_t expires = TIndexToTime(cb->thead);

    printf("vol=%u vn=%u cbs=%d hi=%d st=%d fest=%d, exp in %d secs at %s",
	   fe->volid, fe->vnode, fe->ncbs, cb->hhead, cb->status, fe->status,
	   expires - now, ctime(&expires));
}

#endif

#if	!defined(INTERPRET_DUMP)
/*
** try breaking calbacks on afidp from host. Use multi_rx.
** return 0 on success, non-zero on failure
*/
int
MultiBreakCallBackAlternateAddress(struct host *host, struct AFSCBFids *afidp)
{
    int retVal;
    H_LOCK;
    retVal = MultiBreakCallBackAlternateAddress_r(host, afidp);
    H_UNLOCK;
    return retVal;
}

int
MultiBreakCallBackAlternateAddress_r(struct host *host,
				     struct AFSCBFids *afidp)
{
    int i, j;
    struct rx_connection **conns;
    struct rx_connection *connSuccess = 0;
    struct AddrPort *interfaces;
    static struct rx_securityClass *sc = 0;
    static struct AFSCBs tc = { 0, 0 };
    char hoststr[16];

    /* nothing more can be done */
    if (!host->interface)
	return 1;		/* failure */

    assert(host->interface->numberOfInterfaces > 0);

    /* the only address is the primary interface */
    if (host->interface->numberOfInterfaces == 1)
	return 1;		/* failure */

    /* initialise a security object only once */
    if (!sc)
	sc = rxnull_NewClientSecurityObject();

    i = host->interface->numberOfInterfaces;
    interfaces = calloc(i, sizeof(struct AddrPort));
    conns = calloc(i, sizeof(struct rx_connection *));
    if (!interfaces || !conns) {
	ViceLog(0,
		("Failed malloc in MultiBreakCallBackAlternateAddress_r\n"));
	assert(0);
    }

    /* initialize alternate rx connections */
    for (i = 0, j = 0; i < host->interface->numberOfInterfaces; i++) {
	/* this is the current primary address */
	if (host->host == host->interface->interface[i].addr &&
	    host->port == host->interface->interface[i].port)
	    continue;

	interfaces[j] = host->interface->interface[i];
	conns[j] =
	    rx_NewConnection(interfaces[j].addr, 
			     interfaces[j].port, 1, sc, 0);
	rx_SetConnDeadTime(conns[j], 2);
	rx_SetConnHardDeadTime(conns[j], AFS_HARDDEADTIME);
	j++;
    }

    assert(j);			/* at least one alternate address */
    ViceLog(125,
	    ("Starting multibreakcall back on all addr for host %s\n",
	     afs_inet_ntoa_r(host->host, hoststr)));
    H_UNLOCK;
    multi_Rx(conns, j) {
	multi_RXAFSCB_CallBack(afidp, &tc);
	if (!multi_error) {
	    /* first success */
	    H_LOCK;
	    if (host->callback_rxcon)
		rx_DestroyConnection(host->callback_rxcon);
	    host->callback_rxcon = conns[multi_i];
	    host->host = interfaces[multi_i].addr;
	    host->port = interfaces[multi_i].port;
	    connSuccess = conns[multi_i];
	    rx_SetConnDeadTime(host->callback_rxcon, 50);
	    rx_SetConnHardDeadTime(host->callback_rxcon, AFS_HARDDEADTIME);
	    ViceLog(125,
		    ("multibreakcall success with addr %s\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr)));
	    H_UNLOCK;
	    multi_Abort;
	}
    }
    multi_End_Ignore;
    H_LOCK;
    /* Destroy all connections except the one on which we succeeded */
    for (i = 0; i < j; i++)
	if (conns[i] != connSuccess)
	    rx_DestroyConnection(conns[i]);

    free(interfaces);
    free(conns);

    if (connSuccess)
	return 0;		/* success */
    else
	return 1;		/* failure */
}


/*
** try multi_RX probes to host. 
** return 0 on success, non-0 on failure
*/
int
MultiProbeAlternateAddress_r(struct host *host)
{
    int i, j;
    struct rx_connection **conns;
    struct rx_connection *connSuccess = 0;
    struct AddrPort *interfaces;
    static struct rx_securityClass *sc = 0;
    char hoststr[16];

    /* nothing more can be done */
    if (!host->interface)
	return 1;		/* failure */

    assert(host->interface->numberOfInterfaces > 0);

    /* the only address is the primary interface */
    if (host->interface->numberOfInterfaces == 1)
	return 1;		/* failure */

    /* initialise a security object only once */
    if (!sc)
	sc = rxnull_NewClientSecurityObject();

    i = host->interface->numberOfInterfaces;
    interfaces = calloc(i, sizeof(struct AddrPort));
    conns = calloc(i, sizeof(struct rx_connection *));
    if (!interfaces || !conns) {
	ViceLog(0, ("Failed malloc in MultiProbeAlternateAddress_r\n"));
	assert(0);
    }

    /* initialize alternate rx connections */
    for (i = 0, j = 0; i < host->interface->numberOfInterfaces; i++) {
	/* this is the current primary address */
	if (host->host == host->interface->interface[i].addr &&
	    host->port == host->interface->interface[i].port)
	    continue;

	interfaces[j] = host->interface->interface[i];
	conns[j] =
	    rx_NewConnection(interfaces[j].addr, 
			     interfaces[j].port, 1, sc, 0);
	rx_SetConnDeadTime(conns[j], 2);
	rx_SetConnHardDeadTime(conns[j], AFS_HARDDEADTIME);
	j++;
    }

    assert(j);			/* at least one alternate address */
    ViceLog(125,
	    ("Starting multiprobe on all addr for host %s\n",
	     afs_inet_ntoa_r(host->host, hoststr)));
    H_UNLOCK;
    multi_Rx(conns, j) {
	multi_RXAFSCB_ProbeUuid(&host->interface->uuid);
	if (!multi_error) {
	    /* first success */
	    H_LOCK;
	    if (host->callback_rxcon)
		rx_DestroyConnection(host->callback_rxcon);
	    host->callback_rxcon = conns[multi_i];
	    host->host = interfaces[multi_i].addr;
	    host->port = interfaces[multi_i].port;
	    connSuccess = conns[multi_i];
	    rx_SetConnDeadTime(host->callback_rxcon, 50);
	    rx_SetConnHardDeadTime(host->callback_rxcon, AFS_HARDDEADTIME);
	    ViceLog(125,
		    ("multiprobe success with addr %s\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr)));
	    H_UNLOCK;
	    multi_Abort;
	} else {
	    ViceLog(125,
		    ("multiprobe failure with addr %s\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr)));
            
            /* This is less than desirable but its the best we can do.
             * The AFS Cache Manager will return either 0 for a Uuid  
             * match and a 1 for a non-match.   If the error is 1 we 
             * therefore know that our mapping of IP address to Uuid 
             * is wrong.   We should attempt to find the correct
             * Uuid and fix the host tables.
             */
            if (multi_error == 1) {
                struct host * newhost;

                /* remove the current alternate address from this host */
                H_LOCK;
                for (i = 0, j = 0; i < host->interface->numberOfInterfaces; i++) {
                    if (interfaces[multi_i].addr != host->interface->interface[i].addr &&
			interfaces[multi_i].port != host->interface->interface[i].port) {
                        host->interface->interface[j] = host->interface->interface[i];
                        j++;
                    }
                }
                host->interface->numberOfInterfaces--;
                H_UNLOCK;
            }
        }
    }
    multi_End_Ignore;
    H_LOCK;
    /* Destroy all connections except the one on which we succeeded */
    for (i = 0; i < j; i++)
	if (conns[i] != connSuccess)
	    rx_DestroyConnection(conns[i]);

    free(interfaces);
    free(conns);

    if (connSuccess)
	return 0;		/* success */
    else
	return 1;		/* failure */
}

#endif /* !defined(INTERPRET_DUMP) */
