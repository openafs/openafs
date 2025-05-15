/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
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
 * BreakVolumeCallBacksLater(volume)
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
#include <afs/stds.h>

#include <roken.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <afs/opr.h>
#include <opr/lock.h>
#include <afs/nfs.h>		/* yuck.  This is an abomination. */
#include <rx/rx.h>
#include <rx/rx_queue.h>
#include <afs/afscbint.h>
#include <afs/afsutil.h>
#include <afs/ihandle.h>
#include <afs/partition.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include "viced_prototypes.h"
#include "viced.h"

#include <afs/ptclient.h>	/* need definition of prlist for host.h */
#include "host.h"
#include "callback.h"
#ifdef AFS_DEMAND_ATTACH_FS
#include "serialize_state.h"
#endif /* AFS_DEMAND_ATTACH_FS */


extern afsUUID FS_HostUUID;
extern int hostCount;

#ifndef INTERPRET_DUMP
static int ShowProblems = 1;
#endif

struct cbcounters cbstuff;

static struct FileEntry * FE = NULL;    /* don't use FE[0] */
static struct CallBack * CB = NULL;     /* don't use CB[0] */

static struct CallBack * CBfree = NULL;
static struct FileEntry * FEfree = NULL;


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
#ifndef INTERPRET_DUMP
static int MinTimeOut = (7 * 60);
#endif

/* Heads of CB queues; a timeout index is 1+index into this array */
static afs_uint32 timeout[CB_NUM_TIMEOUT_QUEUES];

static afs_int32 tfirst;	/* cbtime of oldest unexpired call back time queue */


/* 16 byte object get/free routines */
struct object {
    struct object *next;
};

/* Prototypes for static routines */
static struct FileEntry *FindFE(AFSFid * fid);

#ifndef INTERPRET_DUMP
static struct CallBack *iGetCB(int *nused);
static int iFreeCB(struct CallBack *cb, int *nused);
static struct FileEntry *iGetFE(int *nused);
static int iFreeFE(struct FileEntry *fe, int *nused);
static int TAdd(struct CallBack *cb, afs_uint32 * thead);
static int TDel(struct CallBack *cb);
static int HAdd(struct CallBack *cb, struct host *host);
static int HDel(struct CallBack *cb);
static int CDel(struct CallBack *cb, int deletefe);
static int CDelPtr(struct FileEntry *fe, afs_uint32 * cbp,
		   int deletefe);
static afs_uint32 *FindCBPtr(struct FileEntry *fe, struct host *host);
static int FDel(struct FileEntry *fe);
static int AddCallBack1_r(struct host *host, AFSFid * fid, afs_uint32 * thead,
			  int type, int locked);
static void MultiBreakCallBack_r(struct cbstruct cba[], int ncbas,
				 struct AFSCBFids *afidp);
static int MultiBreakVolumeCallBack_r(struct host *host,
				      struct VCBParams *parms, int deletefe);
static int MultiBreakVolumeLaterCallBack(struct host *host, void *rock);
static int GetSomeSpace_r(struct host *hostp, int locked);
static int ClearHostCallbacks_r(struct host *hp, int locked);
static int DumpCallBackState_r(void);
#endif

#define GetCB() ((struct CallBack *)iGetCB(&cbstuff.nCBs))
#define GetFE() ((struct FileEntry *)iGetFE(&cbstuff.nFEs))
#define FreeCB(cb) iFreeCB((struct CallBack *)cb, &cbstuff.nCBs)
#define FreeFE(fe) iFreeFE((struct FileEntry *)fe, &cbstuff.nFEs)

static afs_uint32 *HashTable;	/* File entry hash table */
static afs_int32 FEhashsize;	/* number of buckets in HashTable */

#define FEHASH_SIZE_OLD 512 /* Historical hard-coded FEhashsize */
#define FE_CHAIN_TARGET 16  /* Target chain length for dynamic FEhashsize */
#define FEHash(volume, unique) (((volume)+(unique))&(FEhashsize - 1))

/* Other protos - move out sometime */
void PrintCB(struct CallBack *cb, afs_uint32 now);

static struct FileEntry *
FindFE(AFSFid * fid)
{
    int hash;
    int fei;
    struct FileEntry *fe;

    hash = FEHash(fid->Volume, fid->Unique);
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
iGetCB(int *nused)
{
    struct CallBack *ret;

    if ((ret = CBfree)) {
	CBfree = (struct CallBack *)(((struct object *)ret)->next);
	(*nused)++;
    }
    return ret;
}

static int
iFreeCB(struct CallBack *cb, int *nused)
{
    ((struct object *)cb)->next = (struct object *)CBfree;
    CBfree = cb;
    (*nused)--;
    return 0;
}

static struct FileEntry *
iGetFE(int *nused)
{
    struct FileEntry *ret;

    if ((ret = FEfree)) {
	FEfree = (struct FileEntry *)(((struct object *)ret)->next);
	(*nused)++;
    }
    return ret;
}

static int
iFreeFE(struct FileEntry *fe, int *nused)
{
    ((struct object *)fe)->next = (struct object *)FEfree;
    FEfree = fe;
    (*nused)--;
    return 0;
}

/* Add cb to end of specified timeout list */
static int
TAdd(struct CallBack *cb, afs_uint32 * thead)
{
    if (!*thead) {
	(*thead) = cb->tnext = cb->tprev = cbtoi(cb);
    } else {
	struct CallBack *thp = itocb(*thead);

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
TDel(struct CallBack *cb)
{
    afs_uint32 *thead = itot(cb->thead);

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
HAdd(struct CallBack *cb, struct host *host)
{
    cb->hhead = h_htoi(host);
    if (!host->z.cblist) {
	host->z.cblist = cb->hnext = cb->hprev = cbtoi(cb);
    } else {
	struct CallBack *fcb = itocb(host->z.cblist);

	cb->hprev = fcb->hprev;
	cb->hnext = cbtoi(fcb);
	fcb->hprev = (itocb(fcb->hprev)->hnext = cbtoi(cb));
    }
    return 0;
}

/* Delete call back entry from host list */
static int
HDel(struct CallBack *cb)
{
    afs_uint32 *hhead = &h_itoh(cb->hhead)->z.cblist;

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
    afs_uint32 *cbp;
    int safety;

    for (safety = 0, cbp = &fe->firstcb; *cbp && *cbp != cbi;
	 cbp = &itocb(*cbp)->cnext, safety++) {
	if (safety > cbstuff.nblks + 10) {
	    ViceLogThenPanic(0, ("CDel: Internal Error -- shutting down: "
			         "wanted %d from %d, now at %d\n",
			         cbi, fe->firstcb, *cbp));
	    DumpCallBackState_r();
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
static int Ccdelpt = 0, CcdelB = 0;

static int
CDelPtr(struct FileEntry *fe, afs_uint32 * cbp,
	int deletefe)
{
    struct CallBack *cb;

    if (!*cbp)
	return 0;
    Ccdelpt++;
    cb = itocb(*cbp);
    if (cb != &CB[*cbp])
	CcdelB++;
    *cbp = cb->cnext;
    FreeCB(cb);
    if ((--fe->ncbs == 0) && deletefe)
	FDel(fe);
    return 0;
}

static afs_uint32 *
FindCBPtr(struct FileEntry *fe, struct host *host)
{
    afs_uint32 hostindex = h_htoi(host);
    struct CallBack *cb;
    afs_uint32 *cbp;
    int safety;

    for (safety = 0, cbp = &fe->firstcb; *cbp; cbp = &cb->cnext, safety++) {
	if (safety > cbstuff.nblks) {
	    ViceLog(0, ("FindCBPtr: Internal Error -- shutting down.\n"));
	    DumpCallBackState_r();
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
FDel(struct FileEntry *fe)
{
    int fei = fetoi(fe);
    afs_uint32 *p = &HashTable[FEHash(fe->volid, fe->unique)];

    while (*p && *p != fei)
	p = &itofe(*p)->fnext;
    opr_Assert(*p);
    *p = fe->fnext;
    FreeFE(fe);
    return 0;
}

/* initialize the callback package */
int
InitCallBack(int nblks)
{
    afs_int32 workingHashSize;

    opr_Assert(nblks > 0);

    H_LOCK;
    tfirst = CBtime(time(NULL));
    /* N.B. The "-1", below, is because
     * FE[0] and CB[0] are not used--and not allocated */
    FE = calloc(nblks, sizeof(struct FileEntry));
    if (!FE) {
	ViceLogThenPanic(0, ("Failed malloc in InitCallBack\n"));
    }
    FE--;  /* FE[0] is supposed to point to junk */
    cbstuff.nFEs = nblks;
    while (cbstuff.nFEs)
	FreeFE(&FE[cbstuff.nFEs]);	/* This is correct */
    CB = calloc(nblks, sizeof(struct CallBack));
    if (!CB) {
	ViceLogThenPanic(0, ("Failed malloc in InitCallBack\n"));
    }
    CB--;  /* CB[0] is supposed to point to junk */
    cbstuff.nCBs = nblks;
    while (cbstuff.nCBs)
	FreeCB(&CB[cbstuff.nCBs]);	/* This is correct */

    if (nblks > 64000) {
	/*
	 * We may have a large number of callbacks to keep track of (more than
	 * 64000, the default with the '-L' fileserver switch). Figure out a
	 * hashtable size so our hash chains are around FE_CHAIN_TARGET long.
	 * Start at the old historical hashtable size (FEHASH_SIZE_OLD), and
	 * count up until we get a reasonable number.
	 */
	workingHashSize = FEHASH_SIZE_OLD;
	while (nblks / workingHashSize > FE_CHAIN_TARGET) {
	    opr_Assert(workingHashSize < MAX_AFS_INT32 / 2);
	    workingHashSize *= 2;
	}

    } else {
	/*
	 * It looks like we're using one of the historical default values for
	 * our callback limit (64000 is the amount given by '-L' in the
	 * fileserver; all other defaults are smaller).
	 *
	 * In this case, we're not using a huge number of callbacks, so the
	 * size of the hashtable is not a big concern. Use the old hard-coded
	 * hashtable size of 512 (FEHASH_SIZE_OLD), so any fsstate.dat file we
	 * may save to disk is understandable by old fileservers and other
	 * tools.
	 */
	workingHashSize = FEHASH_SIZE_OLD;
    }

    /* hashtable size must be a power of 2 */
    opr_Assert(workingHashSize > 0);
    opr_Assert((workingHashSize & (workingHashSize - 1)) == 0);

    HashTable = calloc(workingHashSize, sizeof(HashTable[0]));
    if (HashTable == NULL) {
	ViceLogThenPanic(0, ("Failed malloc in InitCallBack\n"));
    }
    FEhashsize = workingHashSize;
    cbstuff.nblks = nblks;
    cbstuff.nbreakers = 0;
    H_UNLOCK;
    return 0;
}

afs_int32
XCallBackBulk_r(struct host * ahost, struct AFSFid * fids, afs_int32 nfids)
{
    struct AFSCallBack tcbs[AFSCBMAX];
    int i;
    struct AFSCBFids tf;
    struct AFSCBs tc;
    int code;
    int j;
    struct rx_connection *cb_conn = NULL;

    rx_SetConnDeadTime(ahost->z.callback_rxcon, 4);
    rx_SetConnHardDeadTime(ahost->z.callback_rxcon, AFS_HARDDEADTIME);

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

	cb_conn = ahost->z.callback_rxcon;
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
    int retVal = 0;
    H_LOCK;
    if (!locked) {
	h_Lock_r(host);
    }
    if (!(host->z.hostFlags & HOSTDELETED))
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
    afs_uint32 time_out = 0;
    afs_uint32 *Thead = thead;
    struct CallBack *newcb = 0;
    int safety;

    cbstuff.AddCallBacks++;

    host->z.Console |= 2;

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
	if (host->z.hostFlags & HOSTDELETED) {
	    host->z.Console &= ~2;
            h_Unlock_r(host);
            return 0;
        }
    }

    fe = FindFE(fid);
    if (type == CB_NORMAL) {
	time_out =
	    TimeCeiling(time(NULL) + TimeOut(fe ? fe->ncbs : 0) +
			ServerBias);
	Thead = THead(CBtime(time_out));
    } else if (type == CB_VOLUME) {
	time_out = TimeCeiling((60 * 120 + time(NULL)) + ServerBias);
	Thead = THead(CBtime(time_out));
    } else if (type == CB_BULK) {
	/* bulk status can get so many callbacks all at once, and most of them
	 * are probably not for things that will be used for long.
	 */
	time_out =
	    TimeCeiling(time(NULL) + ServerBias +
			TimeOut(22 + (fe ? fe->ncbs : 0)));
	Thead = THead(CBtime(time_out));
    }

    host->z.Console &= ~2;

    if (!fe) {
	afs_uint32 hash;

	fe = newfe;
	newfe = NULL;
	fe->firstcb = 0;
	fe->volid = fid->Volume;
	fe->vnode = fid->Vnode;
	fe->unique = fid->Unique;
	fe->ncbs = 0;
	fe->status = 0;
	hash = FEHash(fid->Volume, fid->Unique);
	fe->fnext = HashTable[hash];
	HashTable[hash] = fetoi(fe);
    }
    for (safety = 0, lastcb = cb = itocb(fe->firstcb); cb;
	 lastcb = cb, cb = itocb(cb->cnext), safety++) {
	if (safety > cbstuff.nblks) {
	    ViceLog(0, ("AddCallBack1: Internal Error -- shutting down.\n"));
	    DumpCallBackState_r();
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
	cb->flags = 0;
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

static int
CompareCBA(const void *e1, const void *e2)
{
    const struct cbstruct *cba1 = (const struct cbstruct *)e1;
    const struct cbstruct *cba2 = (const struct cbstruct *)e2;
    return ((cba1->hp)->index - (cba2->hp)->index);
}

/* Take an array full of hosts, all held.  Break callbacks to them, and
 * release the holds once you're done.
 * Currently only works for a single Fid in afidp array.
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
		     struct AFSCBFids *afidp)
{
    int i, j;
    struct rx_connection *conns[MAX_CB_HOSTS];
    static struct AFSCBs tc = { 0, 0 };
    int multi_to_cba_map[MAX_CB_HOSTS];

    opr_Assert(ncbas <= MAX_CB_HOSTS);

    /*
     * When we issue a multi_Rx callback break, we must rx_NewCall a call for
     * each host before we do anything. If there are no call channels
     * available on the conn, we must wait for one of the existing calls to
     * finish. If another thread is breaking callbacks at the same time, it is
     * possible for us to be waiting on NewCall for one of their multi_Rx
     * CallBack calls to finish, but they are waiting on NewCall for one of
     * our calls to finish. So we deadlock.
     *
     * This can be thought of as similar to obtaining multiple locks at the
     * same time. So if we establish an ordering, the possibility of deadlock
     * goes away. Here we provide such an ordering, by sorting our CBAs
     * according to CompareCBA.
     */
    qsort(cba, ncbas, sizeof(struct cbstruct), CompareCBA);

    /* set up conns for multi-call */
    for (i = 0, j = 0; i < ncbas; i++) {
	struct host *thishost = cba[i].hp;
	if (!thishost || (thishost->z.hostFlags & HOSTDELETED)) {
	    continue;
	}
	rx_GetConnection(thishost->z.callback_rxcon);
	multi_to_cba_map[j] = i;
	conns[j++] = thishost->z.callback_rxcon;

	rx_SetConnDeadTime(thishost->z.callback_rxcon, 4);
	rx_SetConnHardDeadTime(thishost->z.callback_rxcon, AFS_HARDDEADTIME);
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
			    ("BCB: INTERNAL ERROR: hp=%p, cba=%p, thead=%u\n",
			     hp, cba, idx));
		} else {
		    /*
		     ** try breaking callbacks on alternate interface addresses
		     */
		    if (MultiBreakCallBackAlternateAddress(hp, afidp)) {
			if (ShowProblems) {
			    ViceLog(7,
				    ("BCB: Failed on file %u.%u.%u, "
				     "Host %p (%s:%d) is down\n",
				     afidp->AFSCBFids_val->Volume,
				     afidp->AFSCBFids_val->Vnode,
				     afidp->AFSCBFids_val->Unique,
                                     hp,
				     afs_inet_ntoa_r(hp->z.host, hoststr),
				     ntohs(hp->z.port)));
			}

			H_LOCK;
			h_Lock_r(hp);
			if (!(hp->z.hostFlags & HOSTDELETED)) {
			    hp->z.hostFlags |= VENUSDOWN;
                            /**
                             * We always go into AddCallBack1_r with the host locked
                             */
                            AddCallBack1_r(hp, afidp->AFSCBFids_val, itot(idx),
                                           CB_DELAYED, 1);
                        }
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
	if (hp) {
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

    if (xhost)
	ViceLog(7,
		("BCB: BreakCallBack(Host %p all but %s:%d, (%u,%u,%u))\n",
		 xhost, afs_inet_ntoa_r(xhost->z.host, hoststr), ntohs(xhost->z.port),
		 fid->Volume, fid->Vnode, fid->Unique));
    else
	ViceLog(7,
		("BCB: BreakCallBack(No Host, (%u,%u,%u))\n",
		fid->Volume, fid->Vnode, fid->Unique));

    H_LOCK;
    cbstuff.BreakCallBacks++;
    fe = FindFE(fid);
    if (!fe) {
	goto done;
    }
    hostindex = xhost ? h_htoi(xhost) : 0;
    cb = itocb(fe->firstcb);
    if (!cb || ((fe->ncbs == 1) && (cb->hhead == hostindex) && !flag)) {
	/* the most common case is what follows the || */
	goto done;
    }
    tf.AFSCBFids_len = 1;
    tf.AFSCBFids_val = fid;

    /* Set CBFLAG_BREAKING flag on all CBs we're looking at. We do this so we
     * can loop through all relevant CBs while dropping H_LOCK, and not lose
     * track of which CBs we want to look at. If we look at all CBs over and
     * over again, we can loop indefinitely as new CBs are added. */
    for (; cb; cb = nextcb) {
	nextcb = itocb(cb->cnext);

	if ((cb->hhead != hostindex || flag)
	    && (cb->status == CB_BULK || cb->status == CB_NORMAL
	        || cb->status == CB_VOLUME)) {
	    cb->flags |= CBFLAG_BREAKING;
	}
    }

    cb = itocb(fe->firstcb);
    opr_Assert(cb);

    /* loop through all CBs, only looking at ones with the CBFLAG_BREAKING
     * flag set */
    for (; cb;) {
	for (ncbas = 0; cb && ncbas < MAX_CB_HOSTS; cb = nextcb) {
	    nextcb = itocb(cb->cnext);
	    if ((cb->flags & CBFLAG_BREAKING)) {
		struct host *thishost = h_itoh(cb->hhead);
		cb->flags &= ~CBFLAG_BREAKING;
		if (!thishost) {
		    ViceLog(0, ("BCB: BOGUS! cb->hhead is NULL!\n"));
		} else if (thishost->z.hostFlags & VENUSDOWN) {
		    ViceLog(7,
			    ("BCB: %p (%s:%d) is down; delaying break call back\n",
			     thishost, afs_inet_ntoa_r(thishost->z.host, hoststr),
			     ntohs(thishost->z.port)));
		    cb->status = CB_DELAYED;
		} else {
		    if (!(thishost->z.hostFlags & HOSTDELETED)) {
			h_Hold_r(thishost);
			cba[ncbas].hp = thishost;
			cba[ncbas].thead = cb->thead;
			ncbas++;
		    }
		    TDel(cb);
		    HDel(cb);
		    CDel(cb, 1);	/* Usually first; so this delete
					 * is reasonably inexpensive */
		}
	    }
	}

	if (ncbas) {
	    MultiBreakCallBack_r(cba, ncbas, &tf);

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
    struct FileEntry *fe;
    afs_uint32 *pcb;
    char hoststr[16];

    H_LOCK;
    cbstuff.DeleteCallBacks++;

    h_Lock_r(host);
    /* do not care if the host has been HOSTDELETED */
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
		("DCB: No call back for host %p (%s:%d), (%u, %u, %u)\n",
		 host, afs_inet_ntoa_r(host->z.host, hoststr), ntohs(host->z.port),
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
    struct FileEntry *fe;
    struct CallBack *cb;
    afs_uint32 cbi;

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
    cbi = fe->firstcb;
    while (cbi != 0) {
	cb = itocb(cbi);
	cbi = cb->cnext;
	TDel(cb);
	HDel(cb);
	FreeCB(cb);
	fe->ncbs--;
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
    struct CallBack *cb;
    int cbi, first;

    cbstuff.DeleteAllCallBacks++;
    cbi = first = host->z.cblist;
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
    host->z.cblist = 0;
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
    int cbi, first, nfids;
    struct CallBack *cb;
    int code;
    char hoststr[16];
    struct rx_connection *cb_conn;

    cbstuff.nbreakers++;
    if (!(host->z.hostFlags & RESETDONE) && !(host->z.hostFlags & HOSTDELETED)) {
	host->z.hostFlags &= ~ALTADDR;	/* alternate addresses are invalid */
	host->z.hostFlags |= HWHO_INPROGRESS; /* ICBS(3) invokes thread quota */
	cb_conn = host->z.callback_rxcon;
	rx_GetConnection(cb_conn);
	if (host->z.interface) {
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
	host->z.hostFlags |= ALTADDR;	/* alternate addresses are valid */
	host->z.hostFlags &= ~HWHO_INPROGRESS;
	if (code) {
	    if (ShowProblems) {
		ViceLog(0,
			("CB: Call back connect back failed (in break delayed) "
			 "for Host %p (%s:%d)\n",
			 host, afs_inet_ntoa_r(host->z.host, hoststr),
			 ntohs(host->z.port)));
	    }
	    host->z.hostFlags |= VENUSDOWN;
	} else {
	    ViceLog(25,
		    ("InitCallBackState success on %p (%s:%d)\n",
		     host, afs_inet_ntoa_r(host->z.host, hoststr),
		     ntohs(host->z.port)));
	    /* reset was done successfully */
	    host->z.hostFlags |= RESETDONE;
	    host->z.hostFlags &= ~VENUSDOWN;
	}
    } else
	while (!(host->z.hostFlags & HOSTDELETED)) {
	    nfids = 0;
	    host->z.hostFlags &= ~VENUSDOWN;	/* presume up */
	    cbi = first = host->z.cblist;
	    if (!cbi)
		break;
	    do {
		first = host->z.cblist;
		cb = itocb(cbi);
		cbi = cb->hnext;
		if (cb->status == CB_DELAYED) {
		    struct FileEntry *fe = itofe(cb->fhead);
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
			    ("CB: XCallBackBulk failed, Host %p (%s:%d); "
			     "callback list follows:\n",
			     host, afs_inet_ntoa_r(host->z.host, hoststr),
			     ntohs(host->z.port)));
		}
		for (i = 0; i < nfids; i++) {
		    if (ShowProblems) {
			ViceLog(0,
				("CB: Host %p (%s:%d), file %u.%u.%u "
				 "(part of bulk callback)\n",
				 host, afs_inet_ntoa_r(host->z.host, hoststr),
				 ntohs(host->z.port), fids[i].Volume,
				 fids[i].Vnode, fids[i].Unique));
		    }
		    /* used to do this:
		     * AddCallBack1_r(host, &fids[i], itot(thead[i]), CB_DELAYED, 1);
		     * * but it turns out to cause too many tricky locking problems.
		     * * now, if break delayed fails, screw it. */
		}
		host->z.hostFlags |= VENUSDOWN;	/* Failed */
		ClearHostCallbacks_r(host, 1 /* locked */ );
		break;
	    }
	    if (nfids < AFSCBMAX)
		break;
	}

    cbstuff.nbreakers--;
    /* If we succeeded it's always ok to unset HFE_LATER */
    if (!(host->z.hostFlags & VENUSDOWN))
	host->z.hostFlags &= ~HFE_LATER;
    return (host->z.hostFlags & VENUSDOWN);
}

static int
MultiBreakVolumeCallBack_r(struct host *host,
			   struct VCBParams *parms, int deletefe)
{
    char hoststr[16];

    if (host->z.hostFlags & HOSTDELETED)
	return 0;

    if (!(host->z.hostFlags & HCBREAK))
	return 0;		/* host is not flagged to notify */

    if (host->z.hostFlags & VENUSDOWN) {
	h_Lock_r(host);
        /* Do not care if the host is now HOSTDELETED */
	if (ShowProblems) {
	    ViceLog(0,
		    ("BVCB: volume callback for Host %p (%s:%d) failed\n",
		     host, afs_inet_ntoa_r(host->z.host, hoststr),
		     ntohs(host->z.port)));
	}
	DeleteAllCallBacks_r(host, deletefe);	/* Delete all callback state
						 * rather than attempting to
						 * selectively remember to
						 * delete the volume callbacks
						 * later */
	host->z.hostFlags &= ~(RESETDONE|HCBREAK);	/* Do InitCallBackState when host returns */
	h_Unlock_r(host);
	return 0;
    }
    opr_Assert(parms->ncbas <= MAX_CB_HOSTS);

    /* Do not call MultiBreakCallBack on the current host structure
     ** because it would prematurely release the hold on the host
     */
    if (parms->ncbas == MAX_CB_HOSTS) {
	struct AFSCBFids tf;

	tf.AFSCBFids_len = 1;
	tf.AFSCBFids_val = parms->fid;

	/* this releases all the hosts */
	MultiBreakCallBack_r(parms->cba, parms->ncbas, &tf);

	parms->ncbas = 0;
    }
    parms->cba[parms->ncbas].hp = host;
    parms->cba[(parms->ncbas)++].thead = parms->thead;
    host->z.hostFlags &= ~HCBREAK;

    /* we have more work to do on this host, so make sure we keep a reference
     * to it */
    h_Hold_r(host);

    return 0;
}

static int
MultiBreakVolumeLaterCallBack(struct host *host, void *rock)
{
    struct VCBParams *parms = (struct VCBParams *)rock;
    int retval;
    H_LOCK;
    retval = MultiBreakVolumeCallBack_r(host, parms, 0);
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
 * Now uses multi-RX for CallBack RPC in a different thread,
 * only marking them here.
 */
extern pthread_cond_t fsync_cond;

int
BreakVolumeCallBacksLater(VolumeId volume)
{
    int hash;
    afs_uint32 *feip;
    struct FileEntry *fe;
    struct CallBack *cb;
    struct host *host;
    int found = 0;

    ViceLog(25, ("Setting later on volume %" AFS_VOLID_FMT "\n",
		 afs_printable_VolumeId_lu(volume)));
    H_LOCK;
    for (hash = 0; hash < FEhashsize; hash++) {
	for (feip = &HashTable[hash]; (fe = itofe(*feip)) != NULL; ) {
	    if (fe->volid == volume) {
		struct CallBack *cbnext;
		for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
		    host = h_itoh(cb->hhead);
		    host->z.hostFlags |= HFE_LATER;
		    cb->status = CB_DELAYED;
		    cbnext = itocb(cb->cnext);
		}
		FSYNC_LOCK;
		fe->status |= FE_LATER;
		FSYNC_UNLOCK;
		found = 1;
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
    FSYNC_LOCK;
    opr_cv_broadcast(&fsync_cond);
    FSYNC_UNLOCK;
    return 0;
}

int
BreakLaterCallBacks(void)
{
    struct AFSFid fid;
    int hash;
    afs_uint32 *feip;
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

    for (hash = 0; hash < FEhashsize; hash++) {
	for (feip = &HashTable[hash]; (fe = itofe(*feip)) != NULL; ) {
	    if (fe && (fe->status & FE_LATER)
		&& (fid.Volume == 0 || fid.Volume == fe->volid)) {
		/* Ugly, but used to avoid left side casting */
		struct object *tmpfe;
		ViceLog(125,
			("Unchaining for %u:%u:%" AFS_VOLID_FMT "\n", fe->vnode,
			 fe->unique, afs_printable_VolumeId_lu(fe->volid)));
		fid.Volume = fe->volid;
		*feip = fe->fnext;
		fe->status &= ~FE_LATER; /* not strictly needed */
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
	struct CallBack *cbnext;
	for (cb = itocb(fe->firstcb); cb; cb = cbnext) {
	    cbnext = itocb(cb->cnext);
	    host = h_itoh(cb->hhead);
	    if (cb->status == CB_DELAYED) {
		if (!(host->z.hostFlags & HOSTDELETED)) {
		    /* mark this host for notification */
		    host->z.hostFlags |= HCBREAK;
		    if (!tthead || (TNorm(tthead) < TNorm(cb->thead))) {
			tthead = cb->thead;
		    }
		}
		TDel(cb);
		HDel(cb);
		CDel(cb, 0);	/* Don't let CDel clean up the fe */
		/* leave flag for MultiBreakVolumeCallBack to clear */
	    } else {
		ViceLog(125,
			("Found host %p (%s:%d) non-DELAYED cb for %u:%u:%" AFS_VOLID_FMT "\n",
			 host, afs_inet_ntoa_r(host->z.host, hoststr),
			 ntohs(host->z.port), fe->vnode, fe->unique,
			 afs_printable_VolumeId_lu(fe->volid)));
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

	    MultiBreakCallBack_r(henumParms.cba, henumParms.ncbas, &tf);
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
    afs_uint32 now = CBtime(time(NULL));
    afs_uint32 *thead;
    struct CallBack *cb;
    int ntimedout = 0;
    char hoststr[16];

    while (tfirst <= now) {
	int cbi;
	cbi = *(thead = THead(tfirst));
	if (cbi) {
	    do {
		cb = itocb(cbi);
		cbi = cb->tnext;
		ViceLog(8,
			("CCB: deleting timed out call back %x (%s:%d), (%" AFS_VOLID_FMT ",%u,%u)\n",
			 h_itoh(cb->hhead)->z.host,
			 afs_inet_ntoa_r(h_itoh(cb->hhead)->z.host, hoststr),
			 h_itoh(cb->hhead)->z.port,
			 afs_printable_VolumeId_lu(itofe(cb->fhead)->volid),
			 itofe(cb->fhead)->vnode, itofe(cb->fhead)->unique));
		HDel(cb);
		CDel(cb, 1);
		ntimedout++;
		if (ntimedout > cbstuff.nblks) {
		    ViceLog(0, ("CCB: Internal Error -- shutting down...\n"));
		    DumpCallBackState_r();
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

/**
 * parameters to pass to lih*_r from h_Enumerate_r when trying to find a host
 * from which to clear callbacks.
 */
struct lih_params {
    /**
     * Points to the least interesting host found; try to clear callbacks on
     * this host after h_Enumerate_r(lih*_r)'ing.
     */
    struct host *lih;

    /**
     * The last host we got from lih*_r, but we couldn't clear its callbacks
     * for some reason. Choose the next-best host after this one (with the
     * current lih*_r, this means to only select hosts that have an ActiveCall
     * newer than lastlih).
     */
    struct host *lastlih;
};

/* Value of host->z.refCount that allows us to reliably infer that
 * host may be held by some other thread */
#define OTHER_MUSTHOLD_LIH 2

/* This version does not allow 'host' to be selected unless its ActiveCall
 * is newer than 'params->lastlih' which is the host with the oldest
 * ActiveCall from the last pass (if it is provided).  We filter out any hosts
 * that are are held by other threads.
 *
 * There is a small problem here, but it may not be easily fixable. Say we
 * select some host A, and give it back to GetSomeSpace_r. GSS_r for some
 * reason cannot clear the callbacks on A, and so calls us again with
 * lastlih = A. Suppose there is another host B that has the same ActiveCall
 * time as A. We will now skip over host B, since
 * 'hostB->z.ActiveCall > hostA->z.ActiveCall' is not true. This could result in
 * us prematurely going to the GSS_r 2nd or 3rd pass, and making us a little
 * inefficient. This should be pretty rare, though, except perhaps in cases
 * with very small numbers of hosts.
 *
 * Also filter out any hosts with HOSTDELETED set. h_Enumerate_r should in
 * theory not give these to us anyway, but be paranoid.
 */
static int
lih0_r(struct host *host, void *rock)
{
    struct lih_params *params = (struct lih_params *)rock;

    /* OTHER_MUSTHOLD_LIH is because the h_Enum loop holds us once */
    if (host->z.cblist
	&& (!(host->z.hostFlags & HOSTDELETED))
	&& (host->z.refCount < OTHER_MUSTHOLD_LIH)
	&& (!params->lih || host->z.ActiveCall < params->lih->z.ActiveCall)
	&& (!params->lastlih || host->z.ActiveCall > params->lastlih->z.ActiveCall)) {

	if (params->lih) {
	    h_Release_r(params->lih); /* release prev host */
	}

	h_Hold_r(host);
	params->lih = host;
    }
    return 0;
}

/* same as lih0_r, except we do not prevent held hosts from being selected. */
static int
lih1_r(struct host *host, void *rock)
{
    struct lih_params *params = (struct lih_params *)rock;

    if (host->z.cblist
	&& (!(host->z.hostFlags & HOSTDELETED))
	&& (!params->lih || host->z.ActiveCall < params->lih->z.ActiveCall)
	&& (!params->lastlih || host->z.ActiveCall > params->lastlih->z.ActiveCall)) {

	if (params->lih) {
	    h_Release_r(params->lih); /* release prev host */
	}

	h_Hold_r(host);
	params->lih = host;
    }
    return 0;
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
    struct host *hp;
    struct lih_params params;
    int i = 0;

    if (cbstuff.GotSomeSpaces == 0) {
	/* only log this once; if GSS is getting called constantly, that's not
	 * good but don't make things worse by spamming the log. */
	ViceLog(0, ("We have run out of callback space; forcing callback revocation. "
	            "This suggests the fileserver is configured with insufficient "
	            "callbacks; you probably want to increase the -cb fileserver "
	            "parameter (current setting: %u). The fileserver will continue "
	            "to operate, but this may indicate a severe performance problem\n",
	            cbstuff.nblks));
	ViceLog(0, ("This message is logged at most once; for more information "
	            "see the OpenAFS documentation and fileserver xstat collection 3\n"));
    }

    cbstuff.GotSomeSpaces++;
    ViceLog(5,
	    ("GSS: First looking for timed out call backs via CleanupCallBacks\n"));
    if (CleanupTimedOutCallBacks_r()) {
	cbstuff.GSS3++;
	return 0;
    }

    i = 0;
    params.lastlih = NULL;

    do {
	params.lih = NULL;

	h_Enumerate_r(i == 0 ? lih0_r : lih1_r, hostList, &params);

	hp = params.lih;
	if (params.lastlih) {
	    h_Release_r(params.lastlih);
	    params.lastlih = NULL;
	}

	if (hp) {
	    /* note that 'hp' was held by lih*_r; we will need to release it */
	    cbstuff.GSS4++;
	    if ((hp != hostp) && !ClearHostCallbacks_r(hp, 0 /* not locked or held */ )) {
                h_Release_r(hp);
		return 0;
	    }

	    params.lastlih = hp;
	    /* params.lastlih will be released on the next iteration, after
	     * h_Enumerate_r */

	} else {
	    /*
	     * Next time try getting callbacks from any host even if
	     * it's held, since the only other option is starvation for
	     * the file server (i.e. until the callback timeout arrives).
	     */
	    i++;
	    params.lastlih = NULL;
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
    char hoststr[16];
    struct rx_connection *cb_conn = NULL;

    ViceLog(5,
	    ("GSS: Delete longest inactive host %p (%s:%d)\n",
	     hp, afs_inet_ntoa_r(hp->z.host, hoststr), ntohs(hp->z.port)));

    if ((hp->z.hostFlags & HOSTDELETED)) {
	/* hp could go away after reacquiring H_LOCK in h_NBLock_r, so we can't
	 * really use it; its callbacks will get cleared anyway when
	 * h_TossStuff_r gets its hands on it */
	return 1;
    }

    h_Hold_r(hp);

    /** Try a non-blocking lock. If the lock is already held return
      * after releasing hold on hp
      */
    if (!locked) {
        if (h_NBLock_r(hp)) {
            h_Release_r(hp);
            return 1;
        }
    }
    if (hp->z.Console & 2) {
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
    if (hp->z.hostFlags & VENUSDOWN) {
	hp->z.hostFlags &= ~RESETDONE;	/* remember that we must do a reset */
    } else if (!(hp->z.hostFlags & HOSTDELETED)) {
	/* host is up, try a call */
	hp->z.hostFlags &= ~ALTADDR;	/* alternate addresses are invalid */
	hp->z.hostFlags |= HWHO_INPROGRESS;   /* enable host thread quota enforcement */
	cb_conn = hp->z.callback_rxcon;
	rx_GetConnection(hp->z.callback_rxcon);
	if (hp->z.interface) {
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
	hp->z.hostFlags |= ALTADDR;	/* alternate addresses are valid */
	hp->z.hostFlags &= ~HWHO_INPROGRESS;
	if (code) {
	    /* failed, mark host down and need reset */
	    hp->z.hostFlags |= VENUSDOWN;
	    hp->z.hostFlags &= ~RESETDONE;
	} else {
	    /* reset succeeded, we're done */
	    hp->z.hostFlags |= RESETDONE;
	}
    }
    if (!locked)
        h_Unlock_r(hp);
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
    fprintf(stderr, "%d CBs, %d FEs, (%d of total of %d %d-byte blocks)\n",
	    cbstuff.nCBs, cbstuff.nFEs, cbstuff.nCBs + cbstuff.nFEs,
	    cbstuff.nblks, (int) sizeof(struct CallBack));
    fprintf(stderr, "%d GSS1, %d GSS2, %d GSS3, %d GSS4, %d GSS5 (internal counters)\n",
	    cbstuff.GSS1, cbstuff.GSS2, cbstuff.GSS3, cbstuff.GSS4, cbstuff.GSS5);
    fprintf(stderr, "%d FEhashsize\n",
	    FEhashsize);
    return 0;
}

#define MAGIC 0x12345678	/* To check byte ordering of dump when it is read in */
#define MAGICV2 0x12345679      /* To check byte ordering & version of dump when it is read in */
#define MAGICV3 0x1234567A


#ifndef INTERPRET_DUMP

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * callback state serialization
 */
static int cb_stateSaveTimeouts(struct fs_dump_state * state);
static int cb_stateSaveFEHash(struct fs_dump_state * state);
static int cb_stateSaveFEs(struct fs_dump_state * state);
static int cb_stateSaveFE(struct fs_dump_state * state, struct FileEntry * fe);
static int cb_stateRestoreTimeouts(struct fs_dump_state * state);
static int cb_stateRestoreFEs(struct fs_dump_state * state);
static int cb_stateRestoreFE(struct fs_dump_state * state);
static int cb_stateRestoreCBs(struct fs_dump_state * state, struct FileEntry * fe,
			      struct iovec * iov, int niovecs);

static int cb_stateVerifyFEHash(struct fs_dump_state * state);
static int cb_stateVerifyFE(struct fs_dump_state * state, struct FileEntry * fe);
static int cb_stateVerifyFCBList(struct fs_dump_state * state, struct FileEntry * fe);
static int cb_stateVerifyTimeoutQueues(struct fs_dump_state * state);

static int cb_stateFEToDiskEntry(struct FileEntry *, struct FEDiskEntry *);
static int cb_stateDiskEntryToFE(struct fs_dump_state * state,
				 struct FEDiskEntry *, struct FileEntry *);

static int cb_stateCBToDiskEntry(struct CallBack *, struct CBDiskEntry *);
static int cb_stateDiskEntryToCB(struct fs_dump_state * state,
				 struct CBDiskEntry *, struct CallBack *);

static int cb_stateFillHeader(struct callback_state_header * hdr);
static int cb_stateCheckHeader(struct callback_state_header * hdr);

static int cb_stateAllocMap(struct fs_dump_state * state);

int
cb_stateSave(struct fs_dump_state * state)
{
    int ret = 0;

    AssignInt64(state->eof_offset, &state->hdr->cb_offset);

    /* invalidate callback state header */
    memset(state->cb_hdr, 0, sizeof(struct callback_state_header));
    if (fs_stateWriteHeader(state, &state->hdr->cb_offset, state->cb_hdr,
			    sizeof(struct callback_state_header))) {
	ret = 1;
	goto done;
    }

    fs_stateIncEOF(state, sizeof(struct callback_state_header));

    /* dump timeout state */
    if (cb_stateSaveTimeouts(state)) {
	ret = 1;
	goto done;
    }

    /* dump fe hashtable state */
    if (cb_stateSaveFEHash(state)) {
	ret = 1;
	goto done;
    }

    /* dump callback state */
    if (cb_stateSaveFEs(state)) {
	ret = 1;
	goto done;
    }

    /* write the callback state header to disk */
    cb_stateFillHeader(state->cb_hdr);
    if (fs_stateWriteHeader(state, &state->hdr->cb_offset, state->cb_hdr,
			    sizeof(struct callback_state_header))) {
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

int
cb_stateRestore(struct fs_dump_state * state)
{
    int ret = 0;

    if (fs_stateReadHeader(state, &state->hdr->cb_offset, state->cb_hdr,
			   sizeof(struct callback_state_header))) {
	ViceLog(0, ("cb_stateRestore: failed to read cb_hdr\n"));
	ret = 1;
	goto done;
    }

    if (cb_stateCheckHeader(state->cb_hdr)) {
	ViceLog(0, ("cb_stateRestore: failed check of cb_hdr\n"));
	ret = 1;
	goto done;
    }

    if (cb_stateAllocMap(state)) {
	ViceLog(0, ("cb_stateRestore: failed to allocate map\n"));
	ret = 1;
	goto done;
    }

    if (cb_stateRestoreTimeouts(state)) {
	ViceLog(0, ("cb_stateRestore: failed to restore timeouts\n"));
	ret = 1;
	goto done;
    }

    /*
     * Note that we do not look at fehash_offset, and we skip reading in
     * state->cb_fehash_hdr and all of the FE hash entries. Instead of loading
     * the FE hashtable from disk, we'll just populate our hashtable ourselves
     * as we load in the FEs. Loading the hashtable from disk currently isn't
     * any faster (since we need to fixup the indices regardless), and
     * populating the hashtable ourselves is a bit simpler and makes it easier
     * to alter our hashing scheme.
     */

    /* restore FEs and CBs from disk */
    if (cb_stateRestoreFEs(state)) {
	ViceLog(0, ("cb_stateRestore: failed to restore FEs and CBs\n"));
	ret = 1;
	goto done;
    }

    /* restore the timeout queue heads */
    tfirst = state->cb_hdr->tfirst;

 done:
    return ret;
}

int
cb_stateRestoreIndices(struct fs_dump_state * state)
{
    int i, ret = 0;
    struct FileEntry * fe;
    struct CallBack * cb;

    /* restore indices in the FileEntry structures */
    for (i = 1; i < state->fe_map.len; i++) {
	if (state->fe_map.entries[i].new_idx) {
	    fe = itofe(state->fe_map.entries[i].new_idx);

	    /* restore the fe->firstcb entry */
	    if (cb_OldToNew(state, fe->firstcb, &fe->firstcb)) {
		ret = 1;
		goto done;
	    }
	}
    }

    /* restore indices in the CallBack structures */
    for (i = 1; i < state->cb_map.len; i++) {
	if (state->cb_map.entries[i].new_idx) {
	    cb = itocb(state->cb_map.entries[i].new_idx);

	    /* restore the cb->cnext entry */
	    if (cb_OldToNew(state, cb->cnext, &cb->cnext)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->fhead entry */
	    if (fe_OldToNew(state, cb->fhead, &cb->fhead)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->hhead entry */
	    if (h_OldToNew(state, cb->hhead, &cb->hhead)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->tprev entry */
	    if (cb_OldToNew(state, cb->tprev, &cb->tprev)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->tnext entry */
	    if (cb_OldToNew(state, cb->tnext, &cb->tnext)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->hprev entry */
	    if (cb_OldToNew(state, cb->hprev, &cb->hprev)) {
		ret = 1;
		goto done;
	    }

	    /* restore the cb->hnext entry */
	    if (cb_OldToNew(state, cb->hnext, &cb->hnext)) {
		ret = 1;
		goto done;
	    }
	}
    }

    /* restore the timeout queue head indices */
    for (i = 0; i < state->cb_timeout_hdr->records; i++) {
	if (cb_OldToNew(state, timeout[i], &timeout[i])) {
	    ret = 1;
	    goto done;
	}
    }

 done:
    return ret;
}

int
cb_stateVerify(struct fs_dump_state * state)
{
    int ret = 0;

    if (cb_stateVerifyFEHash(state)) {
	ret = 1;
    }

    if (cb_stateVerifyTimeoutQueues(state)) {
	ret = 1;
    }

    return ret;
}

static int
cb_stateVerifyFEHash(struct fs_dump_state * state)
{
    int ret = 0, i;
    struct FileEntry * fe;
    afs_uint32 fei, chain_len, max_FEs;

    max_FEs = cbstuff.nblks;

    for (i = 0; i < FEhashsize; i++) {
	chain_len = 0;
	for (fei = HashTable[i], fe = itofe(fei);
	     fe;
	     fei = fe->fnext, fe = itofe(fei)) {
	    if (fei > cbstuff.nblks) {
		ViceLog(0, ("cb_stateVerifyFEHash: error: index out of range (fei=%d)\n", fei));
		ret = 1;
		break;
	    }
	    if (cb_stateVerifyFE(state, fe)) {
		ret = 1;
	    }
	    if (chain_len > max_FEs) {
		ViceLog(0, ("cb_stateVerifyFEHash: error: hash chain %d length exceeds %d; assuming there's a loop\n",
			    i, max_FEs));
		ret = 1;
		break;
	    }
	    chain_len++;
	}
    }

    return ret;
}

static int
cb_stateVerifyFE(struct fs_dump_state * state, struct FileEntry * fe)
{
    int ret = 0;

    if ((fe->firstcb && !fe->ncbs) ||
	(!fe->firstcb && fe->ncbs)) {
	ViceLog(0, ("cb_stateVerifyFE: error: fe->firstcb does not agree with fe->ncbs (fei=%lu, fe->firstcb=%lu, fe->ncbs=%lu)\n",
		    afs_printable_uint32_lu(fetoi(fe)),
		    afs_printable_uint32_lu(fe->firstcb),
		    afs_printable_uint32_lu(fe->ncbs)));
	ret = 1;
    }
    if (cb_stateVerifyFCBList(state, fe)) {
	ViceLog(0, ("cb_stateVerifyFE: error: FCBList failed verification (fei=%lu)\n",
		    afs_printable_uint32_lu(fetoi(fe))));
	ret = 1;
    }

    return ret;
}

static int
cb_stateVerifyFCBList(struct fs_dump_state * state, struct FileEntry * fe)
{
    int ret = 0;
    afs_uint32 cbi, fei, chain_len = 0, max_CBs;
    struct CallBack * cb;

    max_CBs = cbstuff.nblks;
    fei = fetoi(fe);

    for (cbi = fe->firstcb, cb = itocb(cbi);
	 cb;
	 cbi = cb->cnext, cb = itocb(cbi)) {
	if (cbi > cbstuff.nblks) {
	    ViceLog(0, ("cb_stateVerifyFCBList: error: list index out of range (cbi=%d, ncbs=%d)\n",
			cbi, cbstuff.nblks));
	    ret = 1;
	    goto done;
	}
	if (cb->fhead != fei) {
	    ViceLog(0, ("cb_stateVerifyFCBList: error: cb->fhead != fei (fei=%d, cb->fhead=%d)\n",
			fei, cb->fhead));
	    ret = 1;
	}
	if (chain_len > max_CBs) {
	    ViceLog(0, ("cb_stateVerifyFCBList: error: list length exceeds %d (fei=%d); assuming there's a loop\n",
			max_CBs, fei));
	    ret = 1;
	    goto done;
	}
	chain_len++;
    }

    if (fe->ncbs != chain_len) {
	ViceLog(0, ("cb_stateVerifyFCBList: error: list length mismatch (len=%d, fe->ncbs=%d)\n",
		    chain_len, fe->ncbs));
	ret = 1;
    }

 done:
    return ret;
}

int
cb_stateVerifyHCBList(struct fs_dump_state * state, struct host * host)
{
    int ret = 0;
    afs_uint32 hi, chain_len, cbi, max_CBs;
    struct CallBack *cb, *ncb;

    max_CBs = cbstuff.nblks;
    hi = h_htoi(host);
    chain_len = 0;

    for (cbi = host->z.cblist, cb = itocb(cbi);
	 cb;
	 cbi = cb->hnext, cb = ncb) {
	if (chain_len && (host->z.cblist == cbi)) {
	    /* we've wrapped around the circular list, and everything looks ok */
	    break;
	}
	if (cb->hhead != hi) {
	    ViceLog(0, ("cb_stateVerifyHCBList: error: incorrect cb->hhead (cbi=%d, h->index=%d, cb->hhead=%d)\n",
			cbi, hi, cb->hhead));
	    ret = 1;
	}
	if (!cb->hprev || !cb->hnext) {
	    ViceLog(0, ("cb_stateVerifyHCBList: error: null index in circular list (cbi=%d, h->index=%d)\n",
			cbi, hi));
	    ret = 1;
	    goto done;
	}
	if ((cb->hprev > cbstuff.nblks) ||
	    (cb->hnext > cbstuff.nblks)) {
	    ViceLog(0, ("cb_stateVerifyHCBList: error: list index out of range (cbi=%d, h->index=%d, cb->hprev=%d, cb->hnext=%d, nCBs=%d)\n",
			cbi, hi, cb->hprev, cb->hnext, cbstuff.nblks));
	    ret = 1;
	    goto done;
	}
	ncb = itocb(cb->hnext);
	if (cbi != ncb->hprev) {
	    ViceLog(0, ("cb_stateVerifyHCBList: error: corrupt linked list (cbi=%d, h->index=%d)\n",
			cbi, hi));
	    ret = 1;
	    goto done;
	}

	if (chain_len > max_CBs) {
	    ViceLog(0, ("cb_stateVerifyHCBList: error: list length exceeds %d (h->index=%d); assuming there's a loop\n",
			max_CBs, hi));
	    ret = 1;
	    goto done;
	}
	chain_len++;
    }

 done:
    return ret;
}

static int
cb_stateVerifyTimeoutQueues(struct fs_dump_state * state)
{
    int ret = 0, i;
    afs_uint32 cbi, chain_len, max_CBs;
    struct CallBack *cb, *ncb;

    max_CBs = cbstuff.nblks;

    for (i = 0; i < CB_NUM_TIMEOUT_QUEUES; i++) {
	chain_len = 0;
	for (cbi = timeout[i], cb = itocb(cbi);
	     cb;
	     cbi = cb->tnext, cb = ncb) {
	    if (chain_len && (cbi == timeout[i])) {
		/* we've wrapped around the circular list, and everything looks ok */
		break;
	    }
	    if (cbi > cbstuff.nblks) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: error: list index out of range (cbi=%d, tindex=%d)\n",
			    cbi, i));
		ret = 1;
		break;
	    }
	    if (itot(cb->thead) != &timeout[i]) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: error: cb->thead points to wrong timeout queue (tindex=%d, cbi=%d, cb->thead=%d)\n",
			    i, cbi, cb->thead));
		ret = 1;
	    }
	    if (!cb->tprev || !cb->tnext) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: null index in circular list (cbi=%d, tindex=%d)\n",
			    cbi, i));
		ret = 1;
		break;
	    }
	    if ((cb->tprev > cbstuff.nblks) ||
		(cb->tnext > cbstuff.nblks)) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: list index out of range (cbi=%d, tindex=%d, cb->tprev=%d, cb->tnext=%d, nCBs=%d)\n",
			    cbi, i, cb->tprev, cb->tnext, cbstuff.nblks));
		ret = 1;
		break;
	    }
	    ncb = itocb(cb->tnext);
	    if (cbi != ncb->tprev) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: corrupt linked list (cbi=%d, tindex=%d)\n",
			    cbi, i));
		ret = 1;
		break;
	    }

	    if (chain_len > max_CBs) {
		ViceLog(0, ("cb_stateVerifyTimeoutQueues: list length exceeds %d (tindex=%d); assuming there's a loop\n",
			    max_CBs, i));
		ret = 1;
		break;
	    }
	    chain_len++;
	}
    }

    return ret;
}

static int
cb_stateSaveTimeouts(struct fs_dump_state * state)
{
    int ret = 0;
    struct iovec iov[2];

    AssignInt64(state->eof_offset, &state->cb_hdr->timeout_offset);

    state->cb_timeout_hdr->magic = CALLBACK_STATE_TIMEOUT_MAGIC;
    state->cb_timeout_hdr->records = CB_NUM_TIMEOUT_QUEUES;
    state->cb_timeout_hdr->len = sizeof(struct callback_state_timeout_header) +
	(state->cb_timeout_hdr->records * sizeof(afs_uint32));

    iov[0].iov_base = (char *)state->cb_timeout_hdr;
    iov[0].iov_len = sizeof(struct callback_state_timeout_header);
    iov[1].iov_base = (char *)timeout;
    iov[1].iov_len = sizeof(timeout);

    if (fs_stateSeek(state, &state->cb_hdr->timeout_offset)) {
	ret = 1;
	goto done;
    }

    if (fs_stateWriteV(state, iov, 2)) {
	ret = 1;
	goto done;
    }

    fs_stateIncEOF(state, state->cb_timeout_hdr->len);

 done:
    return ret;
}

static int
cb_stateRestoreTimeouts(struct fs_dump_state * state)
{
    int ret = 0, len;

    if (fs_stateReadHeader(state, &state->cb_hdr->timeout_offset,
			   state->cb_timeout_hdr,
			   sizeof(struct callback_state_timeout_header))) {
	ViceLog(0, ("cb_stateRestoreTimeouts: failed to read cb_timeout_hdr\n"));
	ret = 1;
	goto done;
    }

    if (state->cb_timeout_hdr->magic != CALLBACK_STATE_TIMEOUT_MAGIC) {
	ViceLog(0, ("cb_stateRestoreTimeouts: bad header magic 0x%x != 0x%x\n",
		state->cb_timeout_hdr->magic, CALLBACK_STATE_TIMEOUT_MAGIC));
	ret = 1;
	goto done;
    }
    if (state->cb_timeout_hdr->records != CB_NUM_TIMEOUT_QUEUES) {
	ViceLog(0, ("cb_stateRestoreTimeouts: records %d != %d\n",
		state->cb_timeout_hdr->records, CB_NUM_TIMEOUT_QUEUES));
	ret = 1;
	goto done;
    }

    len = state->cb_timeout_hdr->records * sizeof(afs_uint32);

    if (state->cb_timeout_hdr->len !=
	(sizeof(struct callback_state_timeout_header) + len)) {
	ViceLog(0, ("cb_stateRestoreTimeouts: header len %d != %d + %d\n",
		state->cb_timeout_hdr->len,
		(int)sizeof(struct callback_state_timeout_header),
		len));
	ret = 1;
	goto done;
    }

    if (fs_stateRead(state, timeout, len)) {
	ViceLog(0, ("cb_stateRestoreTimeouts: failed read of timeout table\n"));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

static int
cb_stateSaveFEHash(struct fs_dump_state * state)
{
    int ret = 0;
    struct iovec iov[2];

    AssignInt64(state->eof_offset, &state->cb_hdr->fehash_offset);

    state->cb_fehash_hdr->magic = CALLBACK_STATE_FEHASH_MAGIC;

    if (FEhashsize != FEHASH_SIZE_OLD) {
	/*
	 * If our hashtable size is not the historical FEHASH_SIZE_OLD, don't
	 * write out the hashtable at all. The hashtable data on disk is not
	 * very useful; we only write it out because older fileservers or other
	 * utilities may need it for interpreting fsstate.dat. But if our
	 * hashtable size is not FEHASH_SIZE_OLD, then they won't be able to
	 * read it anwyay, since the hashtable size has changed. So just don't
	 * write out the data; just write the header that says we have 0
	 * hashtable buckets.
	 */
	state->cb_fehash_hdr->records = 0;
	state->cb_fehash_hdr->len = sizeof(struct callback_state_fehash_header);

	if (fs_stateWriteHeader(state, &state->cb_hdr->fehash_offset,
				state->cb_fehash_hdr,
				sizeof(*state->cb_fehash_hdr))) {
	    ret = 1;
	    goto done;
	}

    } else {
	/*
	 * Write out our HashTable data. This information is not terribly
	 * useful, but older fileservers and other utilities may need it.
	 * Someday this can probably be removed.
	 */
	state->cb_fehash_hdr->records = FEhashsize;
	state->cb_fehash_hdr->len = sizeof(struct callback_state_fehash_header) +
	    (state->cb_fehash_hdr->records * sizeof(afs_uint32));

	iov[0].iov_base = (char *)state->cb_fehash_hdr;
	iov[0].iov_len = sizeof(struct callback_state_fehash_header);
	iov[1].iov_base = (char *)HashTable;
	iov[1].iov_len = sizeof(HashTable[0]) * FEhashsize;

	if (fs_stateSeek(state, &state->cb_hdr->fehash_offset)) {
	    ret = 1;
	    goto done;
	}

	if (fs_stateWriteV(state, iov, 2)) {
	    ret = 1;
	    goto done;
	}
    }

    fs_stateIncEOF(state, state->cb_fehash_hdr->len);

 done:
    return ret;
}

static int
cb_stateSaveFEs(struct fs_dump_state * state)
{
    int ret = 0;
    int fei, hash;
    struct FileEntry *fe;

    AssignInt64(state->eof_offset, &state->cb_hdr->fe_offset);

    for (hash = 0; hash < FEhashsize ; hash++) {
	for (fei = HashTable[hash]; fei; fei = fe->fnext) {
	    fe = itofe(fei);
	    if (cb_stateSaveFE(state, fe)) {
		ret = 1;
		goto done;
	    }
	}
    }

 done:
    return ret;
}

static int
cb_stateRestoreFEs(struct fs_dump_state * state)
{
    int count, nFEs, ret = 0;

    if (fs_stateSeek(state, &state->cb_hdr->fe_offset)) {
	ViceLog(0, ("cb_stateRestoreFEs: error seeking to FE offset\n"));
	ret = 1;
	goto done;
    }

    nFEs = state->cb_hdr->nFEs;

    for (count = 0; count < nFEs; count++) {
	if (cb_stateRestoreFE(state)) {
	    ret = 1;
	    goto done;
	}
    }

 done:
    return ret;
}

static int
cb_stateSaveFE(struct fs_dump_state * state, struct FileEntry * fe)
{
    int ret = 0, iovcnt, cbi, written = 0;
    afs_uint32 fei;
    struct callback_state_entry_header hdr;
    struct FEDiskEntry fedsk;
    struct CBDiskEntry cbdsk[16];
    struct iovec iov[16];
    struct CallBack *cb;

    fei = fetoi(fe);
    if (fei > state->cb_hdr->fe_max) {
	state->cb_hdr->fe_max = fei;
    }

    memset(&hdr, 0, sizeof(struct callback_state_entry_header));

    if (cb_stateFEToDiskEntry(fe, &fedsk)) {
	ret = 1;
	goto done;
    }

    iov[0].iov_base = (char *)&hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = (char *)&fedsk;
    iov[1].iov_len = sizeof(struct FEDiskEntry);
    iovcnt = 2;

    for (cbi = fe->firstcb, cb = itocb(cbi);
	 cb != NULL;
	 cbi = cb->cnext, cb = itocb(cbi), hdr.nCBs++) {
	if (cbi > state->cb_hdr->cb_max) {
	    state->cb_hdr->cb_max = cbi;
	}
	if (cb_stateCBToDiskEntry(cb, &cbdsk[iovcnt])) {
	    ret = 1;
	    goto done;
	}
	cbdsk[iovcnt].index = cbi;
	iov[iovcnt].iov_base = (char *)&cbdsk[iovcnt];
	iov[iovcnt].iov_len = sizeof(struct CBDiskEntry);
	iovcnt++;
	if ((iovcnt == 16) || (!cb->cnext)) {
	    if (fs_stateWriteV(state, iov, iovcnt)) {
		ret = 1;
		goto done;
	    }
	    written = 1;
	    iovcnt = 0;
	}
    }

    hdr.magic = CALLBACK_STATE_ENTRY_MAGIC;
    hdr.len = sizeof(hdr) + sizeof(struct FEDiskEntry) +
	(hdr.nCBs * sizeof(struct CBDiskEntry));

    if (!written) {
	if (fs_stateWriteV(state, iov, iovcnt)) {
	    ret = 1;
	    goto done;
	}
    } else {
	if (fs_stateWriteHeader(state, &state->eof_offset, &hdr, sizeof(hdr))) {
	    ret = 1;
	    goto done;
	}
    }

    fs_stateIncEOF(state, hdr.len);

    if (written) {
	if (fs_stateSeek(state, &state->eof_offset)) {
	    ret = 1;
	    goto done;
	}
    }

    state->cb_hdr->nFEs++;
    state->cb_hdr->nCBs += hdr.nCBs;

 done:
    return ret;
}

static int
cb_stateRestoreFE(struct fs_dump_state * state)
{
    int ret = 0, iovcnt, nCBs;
    struct callback_state_entry_header hdr;
    struct FEDiskEntry fedsk;
    struct CBDiskEntry cbdsk[16];
    struct iovec iov[16];
    struct FileEntry * fe;

    iov[0].iov_base = (char *)&hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = (char *)&fedsk;
    iov[1].iov_len = sizeof(fedsk);
    iovcnt = 2;

    if (fs_stateReadV(state, iov, iovcnt)) {
	ret = 1;
	goto done;
    }

    if (hdr.magic != CALLBACK_STATE_ENTRY_MAGIC) {
	ViceLog(0, ("cb_stateRestoreFE: magic 0x%x != 0x%x\n",
		hdr.magic, CALLBACK_STATE_ENTRY_MAGIC));
	ret = 1;
	goto done;
    }

    fe = GetFE();
    if (fe == NULL) {
	ViceLog(0, ("cb_stateRestoreFE: ran out of free FileEntry structures\n"));
	ret = 1;
	goto done;
    }

    if (cb_stateDiskEntryToFE(state, &fedsk, fe)) {
	ret = 1;
	goto done;
    }

    if (hdr.nCBs) {
	for (iovcnt = 0, nCBs = 0;
	     nCBs < hdr.nCBs;
	     nCBs++) {
	    iov[iovcnt].iov_base = (char *)&cbdsk[iovcnt];
	    iov[iovcnt].iov_len = sizeof(struct CBDiskEntry);
	    iovcnt++;
	    if ((iovcnt == 16) || (nCBs == hdr.nCBs - 1)) {
		if (fs_stateReadV(state, iov, iovcnt)) {
		    ret = 1;
		    goto done;
		}
		if (cb_stateRestoreCBs(state, fe, iov, iovcnt)) {
		    ret = 1;
		    goto done;
		}
		iovcnt = 0;
	    }
	}
    }

 done:
    return ret;
}

static int
cb_stateRestoreCBs(struct fs_dump_state * state, struct FileEntry * fe,
		   struct iovec * iov, int niovecs)
{
    int ret = 0, idx;
    struct CallBack * cb;
    struct CBDiskEntry * cbdsk;

    for (idx = 0; idx < niovecs; idx++) {
	cbdsk = (struct CBDiskEntry *) iov[idx].iov_base;

	if (cbdsk->cb.hhead < state->h_map.len &&
	    state->h_map.entries[cbdsk->cb.hhead].valid == FS_STATE_IDX_SKIPPED) {
	    continue;
	}

	if ((cb = GetCB()) == NULL) {
	    ViceLog(0, ("cb_stateRestoreCBs: ran out of free CallBack structures\n"));
	    ret = 1;
	    goto done;
	}
	if (cb_stateDiskEntryToCB(state, cbdsk, cb)) {
	    ViceLog(0, ("cb_stateRestoreCBs: corrupt CallBack disk entry\n"));
	    ret = 1;
	    goto done;
	}
    }

 done:
    return ret;
}


static int
cb_stateFillHeader(struct callback_state_header * hdr)
{
    hdr->stamp.magic = CALLBACK_STATE_MAGIC;
    hdr->stamp.version = CALLBACK_STATE_VERSION;
    hdr->tfirst = tfirst;
    return 0;
}

static int
cb_stateCheckHeader(struct callback_state_header * hdr)
{
    int ret = 0;

    if (hdr->stamp.magic != CALLBACK_STATE_MAGIC) {
	ViceLog(0, ("cb_stateCheckHeader: magic 0x%x != 0x%x\n",
		hdr->stamp.magic, CALLBACK_STATE_MAGIC));
	ret = 1;
    } else if (hdr->stamp.version != CALLBACK_STATE_VERSION) {
	ViceLog(0, ("cb_stateCheckHeader: version %d != %d\n",
		hdr->stamp.version, CALLBACK_STATE_VERSION));
	ret = 1;
    } else if ((hdr->nFEs > cbstuff.nblks) || (hdr->nCBs > cbstuff.nblks)) {
	ViceLog(0, ("cb_stateCheckHeader: saved callback state larger than "
		"callback memory allocation (%d FEs, %d CBs > %d)\n",
		hdr->nFEs, hdr->nCBs, cbstuff.nblks));
	ret = 1;
    }
    return ret;
}

/* disk entry conversion routines */
static int
cb_stateFEToDiskEntry(struct FileEntry * in, struct FEDiskEntry * out)
{
    memcpy(&out->fe, in, sizeof(struct FileEntry));
    out->index = fetoi(in);
    return 0;
}

static int
cb_stateDiskEntryToFE(struct fs_dump_state * state,
		      struct FEDiskEntry * in, struct FileEntry * out)
{
    int ret = 0;
    int hash;

    memcpy(out, &in->fe, sizeof(struct FileEntry));

    /* setup FE map entry */
    if (!in->index || (in->index >= state->fe_map.len)) {
	ViceLog(0, ("cb_stateDiskEntryToFE: index (%d) out of range\n",
		    in->index));
	ret = 1;
	goto done;
    }
    state->fe_map.entries[in->index].valid = FS_STATE_IDX_VALID;
    state->fe_map.entries[in->index].old_idx = in->index;
    state->fe_map.entries[in->index].new_idx = fetoi(out);

    /* Add this new FE to the HashTable. */
    hash = FEHash(out->volid, out->unique);
    out->fnext = HashTable[hash];
    HashTable[hash] = fetoi(out);

 done:
    return ret;
}

static int
cb_stateCBToDiskEntry(struct CallBack * in, struct CBDiskEntry * out)
{
    memcpy(&out->cb, in, sizeof(struct CallBack));
    out->index = cbtoi(in);
    return 0;
}

static int
cb_stateDiskEntryToCB(struct fs_dump_state * state,
		      struct CBDiskEntry * in, struct CallBack * out)
{
    int ret = 0;

    memcpy(out, &in->cb, sizeof(struct CallBack));

    /* setup CB map entry */
    if (!in->index || (in->index >= state->cb_map.len)) {
	ViceLog(0, ("cb_stateDiskEntryToCB: index (%d) out of range\n",
		    in->index));
	ret = 1;
	goto done;
    }
    state->cb_map.entries[in->index].valid = FS_STATE_IDX_VALID;
    state->cb_map.entries[in->index].old_idx = in->index;
    state->cb_map.entries[in->index].new_idx = cbtoi(out);

 done:
    return ret;
}

/* index map routines */
static int
cb_stateAllocMap(struct fs_dump_state * state)
{
    state->fe_map.len = state->cb_hdr->fe_max + 1;
    state->cb_map.len = state->cb_hdr->cb_max + 1;
    state->fe_map.entries = (struct idx_map_entry_t *)
	calloc(state->fe_map.len, sizeof(struct idx_map_entry_t));
    state->cb_map.entries = (struct idx_map_entry_t *)
	calloc(state->cb_map.len, sizeof(struct idx_map_entry_t));
    return ((state->fe_map.entries != NULL) && (state->cb_map.entries != NULL)) ? 0 : 1;
}

int
fe_OldToNew(struct fs_dump_state * state, afs_uint32 old, afs_uint32 * new)
{
    int ret = 0;

    /* FEs use a one-based indexing system, so old==0 implies no mapping */
    if (!old) {
	*new = 0;
	goto done;
    }

    if (old >= state->fe_map.len) {
	ViceLog(0, ("fe_OldToNew: index %d is out of range\n", old));
	ret = 1;
    } else if (state->fe_map.entries[old].valid != FS_STATE_IDX_VALID ||
               state->fe_map.entries[old].old_idx != old) { /* sanity check */
	ViceLog(0, ("fe_OldToNew: index %d points to an invalid FileEntry record\n", old));
	ret = 1;
    } else {
	*new = state->fe_map.entries[old].new_idx;
    }

 done:
    return ret;
}

int
cb_OldToNew(struct fs_dump_state * state, afs_uint32 old, afs_uint32 * new)
{
    int ret = 0;

    /* CBs use a one-based indexing system, so old==0 implies no mapping */
    if (!old) {
	*new = 0;
	goto done;
    }

    if (old >= state->cb_map.len) {
	ViceLog(0, ("cb_OldToNew: index %d is out of range\n", old));
	ret = 1;
    } else if (state->cb_map.entries[old].valid != FS_STATE_IDX_VALID ||
               state->cb_map.entries[old].old_idx != old) { /* sanity check */
	ViceLog(0, ("cb_OldToNew: index %d points to an invalid CallBack record\n", old));
	ret = 1;
    } else {
	*new = state->cb_map.entries[old].new_idx;
    }

 done:
    return ret;
}
#endif /* AFS_DEMAND_ATTACH_FS */

#define DumpBytes(fd,buf,req) if (write(fd, buf, req) < 0) {} /* don't care */

static int
DumpCallBackState_r(void)
{
    int fd, oflag;
    afs_uint32 magic = MAGICV3, now = (afs_int32) time(NULL), freelisthead;

    oflag = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef AFS_NT40_ENV
    oflag |= O_BINARY;
#endif
    fd = open(AFSDIR_SERVER_CBKDUMP_FILEPATH, oflag, 0666);
    if (fd < 0) {
	ViceLog(0,
		("Couldn't create callback dump file %s\n",
		 AFSDIR_SERVER_CBKDUMP_FILEPATH));
	return 0;
    }
    /*
     * Collect but ignoring the return value of write(2) here,
     * to avoid compiler warnings on some platforms.
     */
    DumpBytes(fd, &magic, sizeof(magic));
    DumpBytes(fd, &now, sizeof(now));
    DumpBytes(fd, &cbstuff, sizeof(cbstuff));
    DumpBytes(fd, &FEhashsize, sizeof(FEhashsize));
    DumpBytes(fd, TimeOuts, sizeof(TimeOuts));
    DumpBytes(fd, timeout, sizeof(timeout));
    DumpBytes(fd, &tfirst, sizeof(tfirst));
    freelisthead = cbtoi((struct CallBack *)CBfree);
    DumpBytes(fd, &freelisthead, sizeof(freelisthead));	/* This is a pointer */
    freelisthead = fetoi((struct FileEntry *)FEfree);
    DumpBytes(fd, &freelisthead, sizeof(freelisthead));	/* This is a pointer */
    DumpBytes(fd, HashTable, sizeof(HashTable[0]) * FEhashsize);
    DumpBytes(fd, &CB[1], sizeof(CB[1]) * cbstuff.nblks);	/* CB stuff */
    DumpBytes(fd, &FE[1], sizeof(FE[1]) * cbstuff.nblks);	/* FE stuff */
    close(fd);

    return 0;
}

int
DumpCallBackState(void) {
    int rc;

    H_LOCK;
    rc = DumpCallBackState_r();
    H_UNLOCK;

    return(rc);
}

#endif /* !INTERPRET_DUMP */

#ifdef INTERPRET_DUMP

static void
ReadBytes(int fd, void *buf, size_t req)
{
    ssize_t count;

    count = read(fd, buf, req);
    if (count < 0) {
	perror("read");
	exit(-1);
    } else if (count != req) {
	fprintf(stderr, "read: premature EOF (expected %lu, got %lu)\n",
		(unsigned long)req, (unsigned long)count);
	exit(-1);
    }
}

/* This is only compiled in for the callback analyzer program */
/* Returns the time of the dump */
time_t
ReadDump(char *file, int timebits)
{
    int fd, oflag;
    afs_uint32 magic, freelisthead;
    afs_uint32 now;
    afs_int64 now64;

    oflag = O_RDONLY;
#ifdef AFS_NT40_ENV
    oflag |= O_BINARY;
#endif
    fd = open(file, oflag);
    if (fd < 0) {
	fprintf(stderr, "Couldn't read dump file %s\n", file);
	exit(1);
    }
    ReadBytes(fd, &magic, sizeof(magic));
    if (magic == MAGICV3) {
	/* V3 contains a new field for FEhashsize */
	timebits = 32;
	FEhashsize = 0;
    } else if (magic == MAGICV2) {
	timebits = 32;
	FEhashsize = FEHASH_SIZE_OLD;
    } else if (magic == MAGIC) {
	FEhashsize = FEHASH_SIZE_OLD;
    } else {
	fprintf(stderr,
		"Magic number of %s is invalid (0x%x).  You might be trying to\n",
		file, magic);
	fprintf(stderr,
		"run this program on a machine type with a different byte ordering.\n");
	exit(1);
    }
    if (timebits == 64) {
	ReadBytes(fd, &now64, sizeof(afs_int64));
	now = (afs_int32) now64;
    } else
	ReadBytes(fd, &now, sizeof(afs_int32));

    ReadBytes(fd, &cbstuff, sizeof(cbstuff));
    if (FEhashsize == 0) {
	ReadBytes(fd, &FEhashsize, sizeof(FEhashsize));
	if (FEhashsize == 0) {
	    fprintf(stderr, "FEhashsize of 0 is not supported.\n");
	    exit(1);
	}
    }
    ReadBytes(fd, TimeOuts, sizeof(TimeOuts));
    ReadBytes(fd, timeout, sizeof(timeout));
    ReadBytes(fd, &tfirst, sizeof(tfirst));
    ReadBytes(fd, &freelisthead, sizeof(freelisthead));
    CB = ((struct CallBack
	   *)(calloc(cbstuff.nblks, sizeof(struct CallBack)))) - 1;
    FE = ((struct FileEntry
	   *)(calloc(cbstuff.nblks, sizeof(struct FileEntry)))) - 1;
    CBfree = (struct CallBack *)itocb(freelisthead);
    ReadBytes(fd, &freelisthead, sizeof(freelisthead));
    FEfree = (struct FileEntry *)itofe(freelisthead);
    HashTable = calloc(FEhashsize, sizeof(HashTable[0]));
    opr_Assert(HashTable != NULL);
    ReadBytes(fd, HashTable, sizeof(HashTable[0]) * FEhashsize);
    ReadBytes(fd, &CB[1], sizeof(CB[1]) * cbstuff.nblks);	/* CB stuff */
    ReadBytes(fd, &FE[1], sizeof(FE[1]) * cbstuff.nblks);	/* FE stuff */
    if (close(fd)) {
	perror("Error reading dumpfile");
	exit(1);
    }
    return now;
}

#ifdef AFS_NT40_ENV
#include "AFS_component_version_number.h"
#else
#include "AFS_component_version_number.c"
#endif

static afs_uint32 *cbTrack;

int
main(int argc, char **argv)
{
    int err = 0, cbi = 0, stats = 0, noptions = 0, all = 0, vol = 0, raw = 0;
    static AFSFid fid;
    struct FileEntry *fe;
    struct CallBack *cb;
    time_t now;
    int timebits = 32;

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
	} else if (!strcmp(*argv, "-timebits")) {
	    if (argc < 1) {
		err++;
		break;
	    }
	    argc--;
	    timebits = atoi(*++argv);
	    if ((timebits != 32)
		&& (timebits != 64)
		)
		err++;
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
		"Usage: cbd [-host cbid] [-fid volume vnode] [-stats] [-all] [-timebits 32"
		"|64"
		"] callbackdumpfile\n");
	fprintf(stderr,
		"[cbid is shown for each host in the hosts.dump file]\n");
	exit(1);
    }
    now = ReadDump(*argv, timebits);
    if (stats || noptions == 0) {
	time_t uxtfirst = UXtime(tfirst), tnow = now;
	printf("The time of the dump was %u %s", (unsigned int) now, ctime(&tnow));
	printf("The last time cleanup ran was %u %s", (unsigned int) uxtfirst,
	       ctime(&uxtfirst));
	PrintCallBackStats();
    }

    cbTrack = calloc(cbstuff.nblks, sizeof(cbTrack[0]));

    if (all || vol) {
	int hash;
	afs_uint32 *feip;
	struct CallBack *cb;
	struct FileEntry *fe;

	for (hash = 0; hash < FEhashsize; hash++) {
	    for (feip = &HashTable[hash]; (fe = itofe(*feip));) {
		if (!vol || (fe->volid == vol)) {
		    afs_uint32 fe_i = fetoi(fe);

		    for (cb = itocb(fe->firstcb); cb; cb = itocb(cb->cnext)) {
			afs_uint32 cb_i = cbtoi(cb);

			if (cb_i > cbstuff.nblks) {
			    printf("CB index out of range (%u > %d), stopped for this FE\n",
				cb_i, cbstuff.nblks);
			    break;
			}

			if (cbTrack[cb_i]) {
			    printf("CB entry already claimed for FE[%u] (this is FE[%u]), stopped\n",
				cbTrack[cb_i], fe_i);
			    break;
			}
			cbTrack[cb_i] = fe_i;

			PrintCB(cb, now);
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
	afs_int32 *p, i;
	for (i = 1; i < cbstuff.nblks; i++) {
	    p = (afs_int32 *) & FE[i];
	    printf("%d:%12x%12x%12x%12x\n", i, p[0], p[1], p[2], p[3]);
	}
    }

    free(cbTrack);
    exit(0);
}

void
PrintCB(struct CallBack *cb, afs_uint32 now)
{
    struct FileEntry *fe = itofe(cb->fhead);
    time_t expires = TIndexToTime(cb->thead);

    if (fe == NULL)
	return;

    printf("vol=%" AFS_VOLID_FMT " vn=%u cbs=%d hi=%d st=%d fest=%d, exp in %lu secs at %s",
	   afs_printable_VolumeId_lu(fe->volid), fe->vnode, fe->ncbs,
	   cb->hhead, cb->status, fe->status, (unsigned long)(expires - now),
	   ctime(&expires));
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
    if (!host->z.interface)
	return 1;		/* failure */

    /* the only address is the primary interface */
    if (host->z.interface->numberOfInterfaces <= 1)
	return 1;		/* failure */

    /* initialise a security object only once */
    if (!sc)
	sc = rxnull_NewClientSecurityObject();

    i = host->z.interface->numberOfInterfaces;
    interfaces = calloc(i, sizeof(struct AddrPort));
    conns = calloc(i, sizeof(struct rx_connection *));
    if (!interfaces || !conns) {
	ViceLogThenPanic(0, ("Failed malloc in "
			     "MultiBreakCallBackAlternateAddress_r\n"));
    }

    /* initialize alternate rx connections */
    for (i = 0, j = 0; i < host->z.interface->numberOfInterfaces; i++) {
	/* this is the current primary address */
	if (host->z.host == host->z.interface->interface[i].addr &&
	    host->z.port == host->z.interface->interface[i].port)
	    continue;

	interfaces[j] = host->z.interface->interface[i];
	conns[j] =
	    rx_NewConnection(interfaces[j].addr,
			     interfaces[j].port, 1, sc, 0);
	rx_SetConnDeadTime(conns[j], 2);
	rx_SetConnHardDeadTime(conns[j], AFS_HARDDEADTIME);
	j++;
    }

    opr_Assert(j);			/* at least one alternate address */
    ViceLog(125,
	    ("Starting multibreakcall back on all addr for host %p (%s:%d)\n",
	     host, afs_inet_ntoa_r(host->z.host, hoststr), ntohs(host->z.port)));
    H_UNLOCK;
    multi_Rx(conns, j) {
	multi_RXAFSCB_CallBack(afidp, &tc);
	if (!multi_error) {
	    /* first success */
	    H_LOCK;
	    if (host->z.callback_rxcon)
		rx_DestroyConnection(host->z.callback_rxcon);
	    host->z.callback_rxcon = conns[multi_i];
	    /* add then remove */
	    addInterfaceAddr_r(host, interfaces[multi_i].addr,
	                             interfaces[multi_i].port);
	    removeInterfaceAddr_r(host, host->z.host, host->z.port);
	    host->z.host = interfaces[multi_i].addr;
	    host->z.port = interfaces[multi_i].port;
	    connSuccess = conns[multi_i];
	    rx_SetConnDeadTime(host->z.callback_rxcon, 50);
	    rx_SetConnHardDeadTime(host->z.callback_rxcon, AFS_HARDDEADTIME);
	    ViceLog(125,
		    ("multibreakcall success with addr %s:%d\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr),
                     ntohs(interfaces[multi_i].port)));
	    H_UNLOCK;
	    multi_Abort;
	}
    }
    multi_End;
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
    if (!host->z.interface)
	return 1;		/* failure */

    /* the only address is the primary interface */
    if (host->z.interface->numberOfInterfaces <= 1)
        return 1;               /* failure */

    /* initialise a security object only once */
    if (!sc)
	sc = rxnull_NewClientSecurityObject();

    i = host->z.interface->numberOfInterfaces;
    interfaces = calloc(i, sizeof(struct AddrPort));
    conns = calloc(i, sizeof(struct rx_connection *));
    if (!interfaces || !conns) {
	ViceLogThenPanic(0, ("Failed malloc in "
			     "MultiProbeAlternateAddress_r\n"));
    }

    /* initialize alternate rx connections */
    for (i = 0, j = 0; i < host->z.interface->numberOfInterfaces; i++) {
	/* this is the current primary address */
	if (host->z.host == host->z.interface->interface[i].addr &&
	    host->z.port == host->z.interface->interface[i].port)
	    continue;

	interfaces[j] = host->z.interface->interface[i];
	conns[j] =
	    rx_NewConnection(interfaces[j].addr,
			     interfaces[j].port, 1, sc, 0);
	rx_SetConnDeadTime(conns[j], 2);
	rx_SetConnHardDeadTime(conns[j], AFS_HARDDEADTIME);
	j++;
    }

    opr_Assert(j);			/* at least one alternate address */
    ViceLog(125,
	    ("Starting multiprobe on all addr for host %p (%s:%d)\n",
	     host, afs_inet_ntoa_r(host->z.host, hoststr),
	     ntohs(host->z.port)));
    H_UNLOCK;
    multi_Rx(conns, j) {
	multi_RXAFSCB_ProbeUuid(&host->z.interface->uuid);
	if (!multi_error) {
	    /* first success */
	    H_LOCK;
	    if (host->z.callback_rxcon)
		rx_DestroyConnection(host->z.callback_rxcon);
	    host->z.callback_rxcon = conns[multi_i];
	    /* add then remove */
	    addInterfaceAddr_r(host, interfaces[multi_i].addr,
	                             interfaces[multi_i].port);
	    removeInterfaceAddr_r(host, host->z.host, host->z.port);
	    host->z.host = interfaces[multi_i].addr;
	    host->z.port = interfaces[multi_i].port;
	    connSuccess = conns[multi_i];
	    rx_SetConnDeadTime(host->z.callback_rxcon, 50);
	    rx_SetConnHardDeadTime(host->z.callback_rxcon, AFS_HARDDEADTIME);
	    ViceLog(125,
		    ("multiprobe success with addr %s:%d\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr),
                     ntohs(interfaces[multi_i].port)));
	    H_UNLOCK;
	    multi_Abort;
	} else {
	    ViceLog(125,
		    ("multiprobe failure with addr %s:%d\n",
		     afs_inet_ntoa_r(interfaces[multi_i].addr, hoststr),
                     ntohs(interfaces[multi_i].port)));

            /* This is less than desirable but its the best we can do.
             * The AFS Cache Manager will return either 0 for a Uuid
             * match and a 1 for a non-match.   If the error is 1 we
             * therefore know that our mapping of IP address to Uuid
             * is wrong.   We should attempt to find the correct
             * Uuid and fix the host tables.
             */
            if (multi_error == 1) {
                /* remove the current alternate address from this host */
                H_LOCK;
                removeInterfaceAddr_r(host, interfaces[multi_i].addr, interfaces[multi_i].port);
                H_UNLOCK;
            }
        }
#ifdef AFS_DEMAND_ATTACH_FS
	/* try to bail ASAP if the fileserver is shutting down */
	FS_STATE_RDLOCK;
	if (fs_state.mode == FS_MODE_SHUTDOWN) {
	    FS_STATE_UNLOCK;
	    multi_Abort;
	}
	FS_STATE_UNLOCK;
#endif
    }
    multi_End;
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
