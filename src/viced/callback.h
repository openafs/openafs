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

#ifndef _AFS_VICED_CALLBACK_H
#define _AFS_VICED_CALLBACK_H

/* Maximum number of call backs to break at once, single fid
 * There is some debate as to just how large this value should be
 * Ideally, it would be very very large, but I am afraid that the
 * cache managers will all send in their responses simultaneously,
 * thereby swamping the file server.  As a result, something like
 * 10 or 15 might be a better bet.
 */
#define MAX_CB_HOSTS	10

/* max time to break a callback, otherwise client is dead or net is hosed */
#define MAXCBT 25

#define u_byte	unsigned char

struct cbcounters {
    afs_int32 DeleteFiles;
    afs_int32 DeleteCallBacks;
    afs_int32 BreakCallBacks;
    afs_int32 AddCallBacks;
    afs_int32 GotSomeSpaces;
    afs_int32 DeleteAllCallBacks;
    afs_int32 nFEs, nCBs, nblks;
    afs_int32 CBsTimedOut;
    afs_int32 nbreakers;
    afs_int32 GSS1, GSS2, GSS3, GSS4, GSS5;
};
extern struct cbcounters cbstuff;

struct cbstruct {
    struct host *hp;
    afs_uint32 thead;
};

/* structure MUST be multiple of 8 bytes, otherwise the casts to
 * struct object will have alignment issues on *P64 userspaces */
struct FileEntry {
    afs_uint32 vnode;
    afs_uint32 unique;
    afs_uint32 volid;
    afs_uint32 fnext;           /* index of next FE in hash chain */
    afs_uint32 ncbs;            /* number of callbacks for this FE */
    afs_uint32 firstcb;         /* index of first cb in per-FE list */
    afs_uint32 status;          /* status bits for this FE */
    afs_uint32 spare;
};
#define FE_LATER 0x1

/* structure MUST be multiple of 8 bytes, otherwise the casts to
 * struct object will have alignment issues on *P64 userspaces */
struct CallBack {
    afs_uint32 cnext;		/* index of next cb in per-FE list */
    afs_uint32 fhead;		/* index of associated FE */
    u_byte thead;		/* Head of timeout chain */
    u_byte status;		/* Call back status; see definitions, below */
    unsigned short spare;	/* ensure proper alignment */
    afs_uint32 hhead;		/* Head of host table chain */
    afs_uint32 tprev, tnext;	/* per-timeout circular list of callbacks */
    afs_uint32 hprev, hnext;	/* per-host circular list of callbacks */
};

struct VCBParams {
    struct cbstruct cba[MAX_CB_HOSTS];	/* re-entrant storage */
    unsigned int ncbas;
    afs_uint32 thead;		/* head of timeout queue for youngest callback */
    struct AFSFid *fid;
};


/* callback hash macros */
#define FEHASH_SIZE 512		/* Power of 2 */
#define FEHASH_MASK (FEHASH_SIZE-1)
#define FEHash(volume, unique) (((volume)+(unique))&(FEHASH_MASK))

#define CB_NUM_TIMEOUT_QUEUES 128


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

#define TimeOutCutoff   ((sizeof(TimeOuts)/sizeof(TimeOuts[0]))*8)
#define TimeOut(nusers)  ((nusers)>=TimeOutCutoff? MinTimeOut: TimeOuts[(nusers)>>3])

/* time out at server is 3 minutes more than ws */
#define ServerBias	  (3*60)

/* Convert cbtime to timeout queue index */
#define TIndex(cbtime)  (((cbtime)&127)+1)

/* Convert cbtime to pointer to timeout queue head */
#define THead(cbtime)	(&timeout[TIndex(cbtime)-1])

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

#endif /* _AFS_VICED_CALLBACK_H */
