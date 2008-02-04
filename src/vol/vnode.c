/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2005-2008 Sine Nomine Associates
 */

/*
	System:		VICE-TWO
	Module:		vnode.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */
#include <afsconfig.h>
#include <afs/param.h>
#define MAXINT     (~(1<<((sizeof(int)*8)-1)))

RCSID
    ("$Header$");

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */

#include <rx/xdr.h>
#include "rx/rx_queue.h"
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "volume_inline.h"
#include "vnode_inline.h"
#include "partition.h"
#include "salvsync.h"
#if defined(AFS_SGI_ENV)
#include "sys/types.h"
#include "fcntl.h"
#undef min
#undef max
#include "stdlib.h"
#endif
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include "ntops.h"
#else
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fcntl.h>
#endif
#include <unistd.h>
#endif /* AFS_NT40_ENV */
#include <sys/stat.h>

/*@printflike@*/ extern void Log(const char *format, ...);

/*@printflike@*/ extern void Abort(const char *format, ...);


struct VnodeClassInfo VnodeClassInfo[nVNODECLASSES];

private void StickOnLruChain_r(register Vnode * vnp,
			       register struct VnodeClassInfo *vcp);

extern int LogLevel;




#define BAD_IGET	-1000

/* There are two separate vnode queue types defined here:
 * Each hash conflict chain -- is singly linked, with a single head
 * pointer. New entries are added at the beginning. Old
 * entries are removed by linear search, which generally
 * only occurs after a disk read).
 * LRU chain -- is doubly linked, single head pointer.
 * Entries are added at the head, reclaimed from the tail,
 * or removed from anywhere in the queue.
 */


/* Vnode hash table.  Find hash chain by taking lower bits of
 * (volume_hash_offset + vnode).
 * This distributes the root inodes of the volumes over the
 * hash table entries and also distributes the vnodes of
 * volumes reasonably fairly.  The volume_hash_offset field
 * for each volume is established as the volume comes on line
 * by using the VOLUME_HASH_OFFSET macro.  This distributes the
 * volumes fairly among the cache entries, both when servicing
 * a small number of volumes and when servicing a large number.
 */

/* logging stuff for finding bugs */
#define	THELOGSIZE	5120
static afs_int32 theLog[THELOGSIZE];
static afs_int32 vnLogPtr = 0;
void
VNLog(aop, anparms, av1, av2, av3, av4)
     afs_int32 aop, anparms;
     afs_int32 av1, av2, av3, av4;
{
    register afs_int32 temp;
    afs_int32 data[4];

    /* copy data to array */
    data[0] = av1;
    data[1] = av2;
    data[2] = av3;
    data[3] = av4;
    if (anparms > 4)
	anparms = 4;		/* do bounds checking */

    temp = (aop << 16) | anparms;
    theLog[vnLogPtr++] = temp;
    if (vnLogPtr >= THELOGSIZE)
	vnLogPtr = 0;
    for (temp = 0; temp < anparms; temp++) {
	theLog[vnLogPtr++] = data[temp];
	if (vnLogPtr >= THELOGSIZE)
	    vnLogPtr = 0;
    }
}

/* VolumeHashOffset -- returns a new value to be stored in the
 * volumeHashOffset of a Volume structure.  Called when a
 * volume is initialized.  Sets the volumeHashOffset so that
 * vnode cache entries are distributed reasonably between
 * volumes (the root vnodes of the volumes will hash to
 * different values, and spacing is maintained between volumes
 * when there are not many volumes represented), and spread
 * equally amongst vnodes within a single volume.
 */
int
VolumeHashOffset_r(void)
{
    static int nextVolumeHashOffset = 0;
    /* hashindex Must be power of two in size */
#   define hashShift 3
#   define hashMask ((1<<hashShift)-1)
    static byte hashindex[1 << hashShift] =
	{ 0, 128, 64, 192, 32, 160, 96, 224 };
    int offset;
    offset = hashindex[nextVolumeHashOffset & hashMask]
	+ (nextVolumeHashOffset >> hashShift);
    nextVolumeHashOffset++;
    return offset;
}

/* Change hashindex (above) if you change this constant */
#define VNODE_HASH_TABLE_SIZE 256
private Vnode *VnodeHashTable[VNODE_HASH_TABLE_SIZE];
#define VNODE_HASH(volumeptr,vnodenumber)\
    ((volumeptr->vnodeHashOffset + vnodenumber)&(VNODE_HASH_TABLE_SIZE-1))


/**
 * add a vnode to the volume's vnode list.
 *
 * @param[in] vp   volume object pointer
 * @param[in] vnp  vnode object pointer
 *
 * @note for DAFS, it may seem like we should be acquiring a lightweight ref
 *       on vp, but this would actually break things.  Right now, this is ok
 *       because we destroy all vnode cache contents during during volume
 *       detach.
 *
 * @pre VOL_LOCK held
 *
 * @internal volume package internal use only
 */
void
AddToVVnList(Volume * vp, Vnode * vnp)
{
    if (queue_IsOnQueue(vnp))
	return;

    Vn_volume(vnp) = vp;
    Vn_cacheCheck(vnp) = vp->cacheCheck;
    queue_Append(&vp->vnode_list, vnp);
    Vn_stateFlags(vnp) |= VN_ON_VVN;
}

/**
 * delete a vnode from the volume's vnode list.
 *
 * @pre VOL_LOCK held
 *
 * @internal volume package internal use only
 */
void
DeleteFromVVnList(register Vnode * vnp)
{
    Vn_volume(vnp) = NULL;

    if (!queue_IsOnQueue(vnp))
	return;

    queue_Remove(vnp);
    Vn_stateFlags(vnp) &= ~(VN_ON_VVN);
}

/**
 * add a vnode to the end of the lru.
 *
 * @param[in] vcp  vnode class info object pointer
 * @param[in] vnp  vnode object pointer
 *
 * @internal vnode package internal use only
 */
void
AddToVnLRU(struct VnodeClassInfo * vcp, Vnode * vnp)
{
    if (Vn_stateFlags(vnp) & VN_ON_LRU) {
	return;
    }

    /* Add it to the circular LRU list */
    if (vcp->lruHead == NULL)
	Abort("VPutVnode: vcp->lruHead==NULL");
    else {
	vnp->lruNext = vcp->lruHead;
	vnp->lruPrev = vcp->lruHead->lruPrev;
	vcp->lruHead->lruPrev = vnp;
	vnp->lruPrev->lruNext = vnp;
	vcp->lruHead = vnp;
    }

    /* If the vnode was just deleted, put it at the end of the chain so it
     * will be reused immediately */
    if (vnp->delete)
	vcp->lruHead = vnp->lruNext;

    Vn_stateFlags(vnp) |= VN_ON_LRU;
}

/**
 * delete a vnode from the lru.
 *
 * @param[in] vcp  vnode class info object pointer
 * @param[in] vnp  vnode object pointer
 *
 * @internal vnode package internal use only
 */
void
DeleteFromVnLRU(struct VnodeClassInfo * vcp, Vnode * vnp)
{
    if (!(Vn_stateFlags(vnp) & VN_ON_LRU)) {
	return;
    }

    if (vnp == vcp->lruHead)
	vcp->lruHead = vcp->lruHead->lruNext;

    if ((vnp == vcp->lruHead) || 
	(vcp->lruHead == NULL))
	Abort("DeleteFromVnLRU: lru chain addled!\n");

    vnp->lruPrev->lruNext = vnp->lruNext;
    vnp->lruNext->lruPrev = vnp->lruPrev;

    Vn_stateFlags(vnp) &= ~(VN_ON_LRU);
}

/**
 * add a vnode to the vnode hash table.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held
 *
 * @post vnode on hash
 *
 * @internal vnode package internal use only
 */
void
AddToVnHash(Vnode * vnp)
{
    unsigned int newHash;

    if (!(Vn_stateFlags(vnp) & VN_ON_HASH)) {
	newHash = VNODE_HASH(Vn_volume(vnp), Vn_id(vnp));
	vnp->hashNext = VnodeHashTable[newHash];
	VnodeHashTable[newHash] = vnp;
	vnp->hashIndex = newHash;

	Vn_stateFlags(vnp) |= VN_ON_HASH;
    }
}

/**
 * delete a vnode from the vnode hash table.
 *
 * @param[in] vnp
 * @param[in] hash
 *
 * @pre VOL_LOCK held
 *
 * @post vnode removed from hash
 *
 * @internal vnode package internal use only
 */
void
DeleteFromVnHash(Vnode * vnp)
{
    Vnode * tvnp;

    if (Vn_stateFlags(vnp) & VN_ON_HASH) {
	tvnp = VnodeHashTable[vnp->hashIndex];
	if (tvnp == vnp)
	    VnodeHashTable[vnp->hashIndex] = vnp->hashNext;
	else {
	    while (tvnp && tvnp->hashNext != vnp)
		tvnp = tvnp->hashNext;
	    if (tvnp)
		tvnp->hashNext = vnp->hashNext;
	}

	vnp->hashNext = NULL;
	vnp->hashIndex = 0;
	Vn_stateFlags(vnp) &= ~(VN_ON_HASH);
    }
}


/**
 * invalidate a vnode cache entry.
 *
 * @param[in] avnode   vnode object pointer
 *
 * @pre VOL_LOCK held
 *
 * @post vnode metadata invalidated.
 *       vnode removed from hash table.
 *       DAFS: vnode state set to VN_STATE_INVALID.
 *
 * @internal vnode package internal use only
 */
void
VInvalidateVnode_r(register struct Vnode *avnode)
{
    avnode->changed_newTime = 0;	/* don't let it get flushed out again */
    avnode->changed_oldTime = 0;
    avnode->delete = 0;		/* it isn't deleted, really */
    avnode->cacheCheck = 0;	/* invalid: prevents future vnode searches from working */
    DeleteFromVnHash(avnode);
#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(avnode, VN_STATE_INVALID);
#endif
}


/**
 * initialize vnode cache for a given vnode class.
 *
 * @param[in] class    vnode class
 * @param[in] nVnodes  size of cache
 *
 * @post vnode cache allocated and initialized
 *
 * @internal volume package internal use only
 *
 * @note generally called by VInitVolumePackage_r
 *
 * @see VInitVolumePackage_r
 */
int
VInitVnodes(VnodeClass class, int nVnodes)
{
    byte *va;
    register struct VnodeClassInfo *vcp = &VnodeClassInfo[class];

    vcp->allocs = vcp->gets = vcp->reads = vcp->writes = 0;
    vcp->cacheSize = nVnodes;
    switch (class) {
    case vSmall:
	assert(CHECKSIZE_SMALLVNODE);
	vcp->lruHead = NULL;
	vcp->residentSize = SIZEOF_SMALLVNODE;
	vcp->diskSize = SIZEOF_SMALLDISKVNODE;
	vcp->magic = SMALLVNODEMAGIC;
	break;
    case vLarge:
	vcp->lruHead = NULL;
	vcp->residentSize = SIZEOF_LARGEVNODE;
	vcp->diskSize = SIZEOF_LARGEDISKVNODE;
	vcp->magic = LARGEVNODEMAGIC;
	break;
    }
    {
	int s = vcp->diskSize - 1;
	int n = 0;
	while (s)
	    s >>= 1, n++;
	vcp->logSize = n;
    }

    if (nVnodes == 0)
	return 0;

    va = (byte *) calloc(nVnodes, vcp->residentSize);
    assert(va != NULL);
    while (nVnodes--) {
	Vnode *vnp = (Vnode *) va;
	Vn_refcount(vnp) = 0;	/* no context switches */
	Vn_stateFlags(vnp) |= VN_ON_LRU;
#ifdef AFS_DEMAND_ATTACH_FS
	assert(pthread_cond_init(&Vn_stateCV(vnp), NULL) == 0);
	Vn_state(vnp) = VN_STATE_INVALID;
	Vn_readers(vnp) = 0;
#else /* !AFS_DEMAND_ATTACH_FS */
	Lock_Init(&vnp->lock);
#endif /* !AFS_DEMAND_ATTACH_FS */
	vnp->changed_oldTime = 0;
	vnp->changed_newTime = 0;
	Vn_volume(vnp) = NULL;
	Vn_cacheCheck(vnp) = 0;
	vnp->delete = Vn_id(vnp) = 0;
#ifdef AFS_PTHREAD_ENV
	vnp->writer = (pthread_t) 0;
#else /* AFS_PTHREAD_ENV */
	vnp->writer = (PROCESS) 0;
#endif /* AFS_PTHREAD_ENV */
	vnp->hashIndex = 0;
	vnp->handle = NULL;
	Vn_class(vnp) = vcp;
	if (vcp->lruHead == NULL)
	    vcp->lruHead = vnp->lruNext = vnp->lruPrev = vnp;
	else {
	    vnp->lruNext = vcp->lruHead;
	    vnp->lruPrev = vcp->lruHead->lruPrev;
	    vcp->lruHead->lruPrev = vnp;
	    vnp->lruPrev->lruNext = vnp;
	    vcp->lruHead = vnp;
	}
	va += vcp->residentSize;
    }
    return 0;
}


/**
 * allocate an unused vnode from the lru chain.
 *
 * @param[in] vcp  vnode class info object pointer
 *
 * @pre VOL_LOCK is held
 *
 * @post vnode object is removed from lru, and vnode hash table.
 *       vnode is disassociated from volume object.
 *       state is set to VN_STATE_INVALID.
 *       inode handle is released.
 *
 * @note we traverse backwards along the lru circlist.  It shouldn't 
 *       be necessary to specify that nUsers == 0 since if it is in the list, 
 *       nUsers should be 0.  Things shouldn't be in lruq unless no one is 
 *       using them.
 *
 * @warning DAFS: VOL_LOCK is dropped while doing inode handle release
 *
 * @return vnode object pointer
 */
Vnode *
VGetFreeVnode_r(struct VnodeClassInfo * vcp)
{
    register Vnode *vnp;

    vnp = vcp->lruHead->lruPrev;
#ifdef AFS_DEMAND_ATTACH_FS
    if (Vn_refcount(vnp) != 0 || VnIsExclusiveState(Vn_state(vnp)) ||
	Vn_readers(vnp) != 0)
	Abort("VGetFreeVnode_r: in-use vnode in lruq");
#else
    if (Vn_refcount(vnp) != 0 || CheckLock(&vnp->lock))
	Abort("VGetFreeVnode_r: locked vnode in lruq");
#endif
    VNLog(1, 2, Vn_id(vnp), (afs_int32) vnp);

    /* 
     * it's going to be overwritten soon enough.
     * remove from LRU, delete hash entry, and 
     * disassociate from old parent volume before
     * we have a chance to drop the vol glock
     */
    DeleteFromVnLRU(vcp, vnp);
    DeleteFromVnHash(vnp);
    if (Vn_volume(vnp)) {
	DeleteFromVVnList(vnp);
    }

    /* drop the file descriptor */
    if (vnp->handle) {
#ifdef AFS_DEMAND_ATTACH_FS
	VnChangeState_r(vnp, VN_STATE_RELEASING);
	VOL_UNLOCK;
#endif
	IH_RELEASE(vnp->handle);
#ifdef AFS_DEMAND_ATTACH_FS
	VOL_LOCK;
#endif
    }

#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, VN_STATE_INVALID);
#endif

    return vnp;
}


/**
 * lookup a vnode in the vnode cache hash table.
 *
 * @param[in] vp       pointer to volume object
 * @param[in] vnodeId  vnode id
 *
 * @pre VOL_LOCK held
 *
 * @post matching vnode object or NULL is returned
 *
 * @return vnode object pointer
 *   @retval NULL   no matching vnode object was found in the cache
 *
 * @internal vnode package internal use only
 *
 * @note this symbol is exported strictly for fssync debug protocol use
 */
Vnode *
VLookupVnode(Volume * vp, VnodeId vnodeId)
{
    Vnode * vnp;
    unsigned int newHash;

    newHash = VNODE_HASH(vp, vnodeId);
    for (vnp = VnodeHashTable[newHash];
	 (vnp && 
	  ((Vn_id(vnp) != vnodeId) || 
	   (Vn_volume(vnp) != vp) ||
	   (vp->cacheCheck != Vn_cacheCheck(vnp))));
	 vnp = vnp->hashNext);

    return vnp;
}


Vnode *
VAllocVnode(Error * ec, Volume * vp, VnodeType type)
{
    Vnode *retVal;
    VOL_LOCK;
    retVal = VAllocVnode_r(ec, vp, type);
    VOL_UNLOCK;
    return retVal;
}

/**
 * allocate a new vnode.
 *
 * @param[out] ec    error code return
 * @param[in]  vp    volume object pointer
 * @param[in]  type  desired vnode type
 *
 * @return vnode object pointer
 *
 * @pre VOL_LOCK held;
 *      heavyweight ref held on vp
 *
 * @post vnode allocated and returned
 */
Vnode *
VAllocVnode_r(Error * ec, Volume * vp, VnodeType type)
{
    register Vnode *vnp;
    VnodeId vnodeNumber;
    int bitNumber, code;
    register struct VnodeClassInfo *vcp;
    VnodeClass class;
    Unique unique;
#ifdef AFS_DEMAND_ATTACH_FS
    VolState vol_state_save;
#endif

    *ec = 0;

#ifdef AFS_DEMAND_ATTACH_FS
    /*
     * once a volume has entered an error state, don't permit
     * further operations to proceed
     *  -- tkeiser 11/21/2007
     */
    VWaitExclusiveState_r(vp);
    if (VIsErrorState(V_attachState(vp))) {
	/* XXX is VSALVAGING acceptable here? */
	*ec = DAFS_VSALVAGE;
	return NULL;
    }
#endif

    if (programType == fileServer && !V_inUse(vp)) {
	if (vp->specialStatus) {
	    *ec = vp->specialStatus;
	} else {
	    *ec = VOFFLINE;
	}
	return NULL;
    }
    class = vnodeTypeToClass(type);
    vcp = &VnodeClassInfo[class];

    if (!VolumeWriteable(vp)) {
	*ec = (bit32) VREADONLY;
	return NULL;
    }

    unique = vp->nextVnodeUnique++;
    if (!unique)
	unique = vp->nextVnodeUnique++;

    if (vp->nextVnodeUnique > V_uniquifier(vp)) {
	VUpdateVolume_r(ec, vp, 0);
	if (*ec)
	    return NULL;
    }

    if (programType == fileServer) {
	VAddToVolumeUpdateList_r(ec, vp);
	if (*ec)
	    return NULL;
    }

    /* Find a slot in the bit map */
    bitNumber = VAllocBitmapEntry_r(ec, vp, &vp->vnodeIndex[class],
				    VOL_ALLOC_BITMAP_WAIT);
    if (*ec)
	return NULL;
    vnodeNumber = bitNumberToVnodeNumber(bitNumber, class);

    /*
     * DAFS:
     * at this point we should be assured that V_attachState(vp) is non-exclusive
     */

 vnrehash:
    VNLog(2, 1, vnodeNumber);
    /* Prepare to move it to the new hash chain */
    vnp = VLookupVnode(vp, vnodeNumber);
    if (vnp) {
	/* slot already exists.  May even not be in lruq (consider store file locking a file being deleted)
	 * so we may have to wait for it below */
	VNLog(3, 2, vnodeNumber, (afs_int32) vnp);

	VnCreateReservation_r(vnp);
	if (Vn_refcount(vnp) == 1) {
	    /* we're the only user */
	    /* This won't block */
	    VnLock(vnp, WRITE_LOCK, VOL_LOCK_HELD, WILL_NOT_DEADLOCK);
	} else {
	    /* other users present; follow locking hierarchy */
	    VnLock(vnp, WRITE_LOCK, VOL_LOCK_HELD, MIGHT_DEADLOCK);

#ifdef AFS_DEMAND_ATTACH_FS
	    /*
	     * DAFS:
	     * vnode was cached, wait for any existing exclusive ops to finish.
	     * once we have reacquired the lock, re-verify volume state.
	     *
	     * note: any vnode error state is related to the old vnode; disregard.
	     */
	    VnWaitQuiescent_r(vnp);
	    if (VIsErrorState(V_attachState(vp))) {
		VnUnlock(vnp, WRITE_LOCK);
		VnCancelReservation_r(vnp);
		*ec = DAFS_VSALVAGE;
		return NULL;
	    }
#endif

	    /*
	     * verify state of the world hasn't changed
	     *
	     * (technically, this should never happen because cachecheck
	     *  is only updated during a volume attach, which should not
	     *  happen when refs are held)
	     */
	    if (Vn_volume(vnp)->cacheCheck != Vn_cacheCheck(vnp)) {
		VnUnlock(vnp, WRITE_LOCK);
		VnCancelReservation_r(vnp);
		goto vnrehash;
	    }
	}

    } else {
	/* no such vnode in the cache */

	vnp = VGetFreeVnode_r(vcp);

	/* Initialize the header fields so noone allocates another
	 * vnode with the same number */
	Vn_id(vnp) = vnodeNumber;
	VnCreateReservation_r(vnp);
	AddToVVnList(vp, vnp);
#ifdef AFS_DEMAND_ATTACH_FS
	AddToVnHash(vnp);
#endif

	/* This will never block (guaranteed by check in VGetFreeVnode_r() */
	VnLock(vnp, WRITE_LOCK, VOL_LOCK_HELD, WILL_NOT_DEADLOCK);

#ifdef AFS_DEMAND_ATTACH_FS
	VnChangeState_r(vnp, VN_STATE_ALLOC);
#endif

	/* Sanity check:  is this vnode really not in use? */
	{
	    int size;
	    IHandle_t *ihP = vp->vnodeIndex[class].handle;
	    FdHandle_t *fdP;
	    off_t off = vnodeIndexOffset(vcp, vnodeNumber);

	    /* XXX we have a potential race here if two threads
	     * allocate new vnodes at the same time, and they
	     * both decide it's time to extend the index
	     * file size...
	     */
#ifdef AFS_DEMAND_ATTACH_FS
	    /*
	     * this race has been eliminated for the DAFS case
	     * using exclusive state VOL_STATE_VNODE_ALLOC
	     *
	     * if this becomes a bottleneck, there are ways to
	     * improve parallelism for this code path
	     *   -- tkeiser 11/28/2007 
	     */
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);
	    vol_state_save = VChangeState_r(vp, VOL_STATE_VNODE_ALLOC);
#endif

	    VOL_UNLOCK;
	    fdP = IH_OPEN(ihP);
	    if (fdP == NULL) {
		Log("VAllocVnode: can't open index file!\n");
		goto error_encountered;
	    }
	    if ((size = FDH_SIZE(fdP)) < 0) {
		Log("VAllocVnode: can't stat index file!\n");
		goto error_encountered;
	    }
	    if (FDH_SEEK(fdP, off, SEEK_SET) < 0) {
		Log("VAllocVnode: can't seek on index file!\n");
		goto error_encountered;
	    }
	    if (off + vcp->diskSize <= size) {
		if (FDH_READ(fdP, &vnp->disk, vcp->diskSize) != vcp->diskSize) {
		    Log("VAllocVnode: can't read index file!\n");
		    goto error_encountered;
		}
		if (vnp->disk.type != vNull) {
		    Log("VAllocVnode:  addled bitmap or index!\n");
		    goto error_encountered;
		}
	    } else {
		/* growing file - grow in a reasonable increment */
		char *buf = (char *)malloc(16 * 1024);
		if (!buf)
		    Abort("VAllocVnode: malloc failed\n");
		memset(buf, 0, 16 * 1024);
		(void)FDH_WRITE(fdP, buf, 16 * 1024);
		free(buf);
	    }
	    FDH_CLOSE(fdP);
	    VOL_LOCK;

#ifdef AFS_DEMAND_ATTACH_FS
	    VChangeState_r(vp, vol_state_save);
	    VCancelReservation_r(vp);
#endif
	    goto sane;


	error_encountered:
#ifdef AFS_DEMAND_ATTACH_FS
	    /* 
	     * close the file handle
	     * acquire VOL_LOCK
	     * invalidate the vnode
	     * free up the bitmap entry (although salvager should take care of it)
	     * salvage the volume
	     * drop vnode lock and refs
	     */
	    if (fdP)
		FDH_CLOSE(fdP);
	    VOL_LOCK;
	    VFreeBitMapEntry_r(ec, &vp->vnodeIndex[class], bitNumber);
	    VInvalidateVnode_r(vnp);
	    VnUnlock(vnp, WRITE_LOCK);
	    VnCancelReservation_r(vnp);
	    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, 0);
	    VCancelReservation_r(vp);
	    return NULL;
#else
	    assert(1 == 2);
#endif

	}
    sane:
	VNLog(4, 2, vnodeNumber, (afs_int32) vnp);
#ifndef AFS_DEMAND_ATTACH_FS
	AddToVnHash(vnp);
#endif
    }

    VNLog(5, 1, (afs_int32) vnp);
    memset(&vnp->disk, 0, sizeof(vnp->disk));
    vnp->changed_newTime = 0;	/* set this bit when vnode is updated */
    vnp->changed_oldTime = 0;	/* set this on CopyOnWrite. */
    vnp->delete = 0;
    vnp->disk.vnodeMagic = vcp->magic;
    vnp->disk.type = type;
    vnp->disk.uniquifier = unique;
    vnp->handle = NULL;
    vcp->allocs++;
    vp->header->diskstuff.filecount++;
#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, VN_STATE_EXCLUSIVE);
#endif
    return vnp;
}

/**
 * load a vnode from disk.
 *
 * @param[out] ec     client error code return
 * @param[in]  vp     volume object pointer
 * @param[in]  vnp    vnode object pointer
 * @param[in]  vcp    vnode class info object pointer
 * @param[in]  class  vnode class enumeration
 *
 * @pre vnode is registered in appropriate data structures;
 *      caller holds a ref on vnode; VOL_LOCK is held
 *
 * @post vnode data is loaded from disk.
 *       vnode state is set to VN_STATE_ONLINE.
 *       on failure, vnode is invalidated.
 *
 * @internal vnode package internal use only
 */
static void
VnLoad(Error * ec, Volume * vp, Vnode * vnp, 
       struct VnodeClassInfo * vcp, VnodeClass class)
{
    /* vnode not cached */
    Error error;
    int n, dosalv = 1;
    IHandle_t *ihP = vp->vnodeIndex[class].handle;
    FdHandle_t *fdP;

    *ec = 0;
    vcp->reads++;

#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, VN_STATE_LOAD);
#endif

    /* This will never block */
    VnLock(vnp, WRITE_LOCK, VOL_LOCK_HELD, WILL_NOT_DEADLOCK);

    VOL_UNLOCK;
    fdP = IH_OPEN(ihP);
    if (fdP == NULL) {
	Log("VnLoad: can't open index dev=%u, i=%s\n", vp->device,
	    PrintInode(NULL, vp->vnodeIndex[class].handle->ih_ino));
	*ec = VIO;
	goto error_encountered_nolock;
    } else if (FDH_SEEK(fdP, vnodeIndexOffset(vcp, Vn_id(vnp)), SEEK_SET)
	       < 0) {
	Log("VnLoad: can't seek on index file vn=%u\n", Vn_id(vnp));
	*ec = VIO;
	goto error_encountered_nolock;
    } else if ((n = FDH_READ(fdP, (char *)&vnp->disk, vcp->diskSize))
	       != vcp->diskSize) {
	/* Don't take volume off line if the inumber is out of range
	 * or the inode table is full. */
	if (n == BAD_IGET) {
	    Log("VnLoad: bad inumber %s\n",
		PrintInode(NULL, vp->vnodeIndex[class].handle->ih_ino));
	    *ec = VIO;
	    dosalv = 0;
	} else if (n == -1 && errno == EIO) {
	    /* disk error; salvage */
	    Log("VnLoad: Couldn't read vnode %u, volume %u (%s); volume needs salvage\n", Vn_id(vnp), V_id(vp), V_name(vp));
	} else {
	    /* vnode is not allocated */
	    if (LogLevel >= 5) 
		Log("VnLoad: Couldn't read vnode %u, volume %u (%s); read %d bytes, errno %d\n", 
		    Vn_id(vnp), V_id(vp), V_name(vp), n, errno);
	    *ec = VIO;
	    dosalv = 0;
	}
	goto error_encountered_nolock;
    }
    FDH_CLOSE(fdP);
    VOL_LOCK;

    /* Quick check to see that the data is reasonable */
    if (vnp->disk.vnodeMagic != vcp->magic || vnp->disk.type == vNull) {
	if (vnp->disk.type == vNull) {
	    *ec = VNOVNODE;
	    dosalv = 0;
	} else {
	    struct vnodeIndex *index = &vp->vnodeIndex[class];
	    unsigned int bitNumber = vnodeIdToBitNumber(Vn_id(vnp));
	    unsigned int offset = bitNumber >> 3;

	    /* Test to see if vnode number is valid. */
	    if ((offset >= index->bitmapSize)
		|| ((*(index->bitmap + offset) & (1 << (bitNumber & 0x7)))
		    == 0)) {
		Log("VnLoad: Request for unallocated vnode %u, volume %u (%s) denied.\n", Vn_id(vnp), V_id(vp), V_name(vp));
		*ec = VNOVNODE;
		dosalv = 0;
	    } else {
		Log("VnLoad: Bad magic number, vnode %u, volume %u (%s); volume needs salvage\n", Vn_id(vnp), V_id(vp), V_name(vp));
	    }
	}
	goto error_encountered;
    }

    IH_INIT(vnp->handle, V_device(vp), V_parentId(vp), VN_GET_INO(vnp));
    VnUnlock(vnp, WRITE_LOCK);
#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, VN_STATE_ONLINE);
#endif
    return;


 error_encountered_nolock:
    if (fdP) {
	FDH_REALLYCLOSE(fdP);
    }
    VOL_LOCK;

 error_encountered:
    if (dosalv) {
#ifdef AFS_DEMAND_ATTACH_FS
	VRequestSalvage_r(&error, vp, SALVSYNC_ERROR, 0);
#else
	VForceOffline_r(vp, 0);
	error = VSALVAGE;
#endif
	if (!*ec)
	    *ec = error;
    }

    VInvalidateVnode_r(vnp);
    VnUnlock(vnp, WRITE_LOCK);
}

/**
 * store a vnode to disk.
 *
 * @param[out] ec     error code output
 * @param[in]  vp     volume object pointer
 * @param[in]  vnp    vnode object pointer
 * @param[in]  vcp    vnode class info object pointer
 * @param[in]  class  vnode class enumeration
 *
 * @pre VOL_LOCK held.
 *      caller holds refs to volume and vnode.
 *      DAFS: caller is responsible for performing state sanity checks.
 *
 * @post vnode state is stored to disk.
 *
 * @internal vnode package internal use only
 */
static void
VnStore(Error * ec, Volume * vp, Vnode * vnp, 
	struct VnodeClassInfo * vcp, VnodeClass class)
{
    int offset, code;
    IHandle_t *ihP = vp->vnodeIndex[class].handle;
    FdHandle_t *fdP;
#ifdef AFS_DEMAND_ATTACH_FS
    VnState vn_state_save;
#endif

    *ec = 0;

#ifdef AFS_DEMAND_ATTACH_FS
    vn_state_save = VnChangeState_r(vnp, VN_STATE_STORE);
#endif

    offset = vnodeIndexOffset(vcp, Vn_id(vnp));
    VOL_UNLOCK;
    fdP = IH_OPEN(ihP);
    if (fdP == NULL) {
	Log("VnStore: can't open index file!\n");
	goto error_encountered;
    }
    if (FDH_SEEK(fdP, offset, SEEK_SET) < 0) {
	Log("VnStore: can't seek on index file! fdp=0x%x offset=%d, errno=%d\n",
	    fdP, offset, errno);
	goto error_encountered;
    }

    code = FDH_WRITE(fdP, &vnp->disk, vcp->diskSize);
    if (code != vcp->diskSize) {
	/* Don't force volume offline if the inumber is out of
	 * range or the inode table is full.
	 */
	FDH_REALLYCLOSE(fdP);
	if (code == BAD_IGET) {
	    Log("VnStore: bad inumber %s\n",
		PrintInode(NULL,
			   vp->vnodeIndex[class].handle->ih_ino));
	    *ec = VIO;
	    VOL_LOCK;
#ifdef AFS_DEMAND_ATTACH_FS
	    VnChangeState_r(vnp, VN_STATE_ERROR);
#endif
	} else {
	    Log("VnStore: Couldn't write vnode %u, volume %u (%s) (error %d)\n", Vn_id(vnp), V_id(Vn_volume(vnp)), V_name(Vn_volume(vnp)), code);
#ifdef AFS_DEMAND_ATTACH_FS
	    goto error_encountered;
#else
	    VOL_LOCK;
	    VForceOffline_r(vp, 0);
	    *ec = VSALVAGE;
#endif
	}
	return;
    } else {
	FDH_CLOSE(fdP);
    }

    VOL_LOCK;
#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, vn_state_save);
#endif
    return;
    
 error_encountered:
#ifdef AFS_DEMAND_ATTACH_FS
    /* XXX instead of dumping core, let's try to request a salvage
     * and just fail the putvnode */
    if (fdP)
	FDH_CLOSE(fdP);
    VOL_LOCK;
    VnChangeState_r(vnp, VN_STATE_ERROR);
    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, 0);
#else
    assert(1 == 2);
#endif
}

/**
 * get a handle to a vnode object.
 *
 * @param[out] ec           error code
 * @param[in]  vp           volume object
 * @param[in]  vnodeNumber  vnode id
 * @param[in]  locktype     type of lock to acquire
 *
 * @return vnode object pointer
 *
 * @see VGetVnode_r
 */
Vnode *
VGetVnode(Error * ec, Volume * vp, VnodeId vnodeNumber, int locktype)
{				/* READ_LOCK or WRITE_LOCK, as defined in lock.h */
    Vnode *retVal;
    VOL_LOCK;
    retVal = VGetVnode_r(ec, vp, vnodeNumber, locktype);
    VOL_UNLOCK;
    return retVal;
}

/**
 * get a handle to a vnode object.
 *
 * @param[out] ec           error code
 * @param[in]  vp           volume object
 * @param[in]  vnodeNumber  vnode id
 * @param[in]  locktype     type of lock to acquire
 *
 * @return vnode object pointer
 *
 * @internal vnode package internal use only
 *
 * @pre VOL_LOCK held.
 *      heavyweight ref held on volume object.
 */
Vnode *
VGetVnode_r(Error * ec, Volume * vp, VnodeId vnodeNumber, int locktype)
{				/* READ_LOCK or WRITE_LOCK, as defined in lock.h */
    register Vnode *vnp;
    int code;
    VnodeClass class;
    struct VnodeClassInfo *vcp;
    Volume * oldvp = NULL;

    *ec = 0;

    if (vnodeNumber == 0) {
	*ec = VNOVNODE;
	return NULL;
    }

    VNLog(100, 1, vnodeNumber);

#ifdef AFS_DEMAND_ATTACH_FS
    /*
     * once a volume has entered an error state, don't permit
     * further operations to proceed
     *  -- tkeiser 11/21/2007
     */
    VWaitExclusiveState_r(vp);
    if (VIsErrorState(V_attachState(vp))) {
	/* XXX is VSALVAGING acceptable here? */
	*ec = VSALVAGING;
	return NULL;
    }
#endif

    if (programType == fileServer && !V_inUse(vp)) {
	*ec = (vp->specialStatus ? vp->specialStatus : VOFFLINE);

	/* If the volume is VBUSY (being cloned or dumped) and this is
	 * a READ operation, then don't fail.
	 */
	if ((*ec != VBUSY) || (locktype != READ_LOCK)) {
	    return NULL;
	}
	*ec = 0;
    }
    class = vnodeIdToClass(vnodeNumber);
    vcp = &VnodeClassInfo[class];
    if (locktype == WRITE_LOCK && !VolumeWriteable(vp)) {
	*ec = (bit32) VREADONLY;
	return NULL;
    }

    if (locktype == WRITE_LOCK && programType == fileServer) {
	VAddToVolumeUpdateList_r(ec, vp);
	if (*ec) {
	    return NULL;
	}
    }

    vcp->gets++;

    /* See whether the vnode is in the cache. */
    vnp = VLookupVnode(vp, vnodeNumber);
    if (vnp) {
	/* vnode is in cache */

	VNLog(101, 2, vnodeNumber, (afs_int32) vnp);
	VnCreateReservation_r(vnp);

#ifdef AFS_DEMAND_ATTACH_FS
	/*
	 * this is the one DAFS case where we may run into contention.
	 * here's the basic control flow:
	 *
	 * if locktype is READ_LOCK:
	 *   wait until vnode is not exclusive
	 *   set to VN_STATE_READ
	 *   increment read count
	 *   done
	 * else
	 *   wait until vnode is quiescent
	 *   set to VN_STATE_EXCLUSIVE
	 *   done
	 */
	if (locktype == READ_LOCK) {
	    VnWaitExclusiveState_r(vnp);
	} else {
	    VnWaitQuiescent_r(vnp);
	}

	if (VnIsErrorState(Vn_state(vnp))) {
	    VnCancelReservation_r(vnp);
	    *ec = VSALVAGE;
	    return NULL;
	}
#endif /* AFS_DEMAND_ATTACH_FS */
    } else {
	/* vnode not cached */

	/* Not in cache; tentatively grab most distantly used one from the LRU
	 * chain */
	vcp->reads++;
	vnp = VGetFreeVnode_r(vcp);

	/* Initialize */
	vnp->changed_newTime = vnp->changed_oldTime = 0;
	vnp->delete = 0;
	Vn_id(vnp) = vnodeNumber;
	VnCreateReservation_r(vnp);
	AddToVVnList(vp, vnp);
#ifdef AFS_DEMAND_ATTACH_FS
	AddToVnHash(vnp);
#endif

	/*
	 * XXX for non-DAFS, there is a serious
	 * race condition here:
	 *
	 * two threads can race to load a vnode.  the net
	 * result is two struct Vnodes can be allocated
	 * and hashed, which point to the same underlying
	 * disk data store.  conflicting vnode locks can
	 * thus be held concurrently.
	 *
	 * for non-DAFS to be safe, VOL_LOCK really shouldn't
	 * be dropped in VnLoad.  Of course, this would likely
	 * lead to an unacceptable slow-down.
	 */

	VnLoad(ec, vp, vnp, vcp, class);
	if (*ec) {
	    VnCancelReservation_r(vnp);
	    return NULL;
	}
#ifndef AFS_DEMAND_ATTACH_FS
	AddToVnHash(vnp);
#endif
	/*
	 * DAFS:
	 * there is no possibility for contention. we "own" this vnode.
	 */
    }

    /*
     * DAFS:
     * it is imperative that nothing drop vol lock between here
     * and the VnBeginRead/VnChangeState stanza below
     */

    VnLock(vnp, locktype, VOL_LOCK_HELD, MIGHT_DEADLOCK);

    /* Check that the vnode hasn't been removed while we were obtaining
     * the lock */
    VNLog(102, 2, vnodeNumber, (afs_int32) vnp);
    if ((vnp->disk.type == vNull) || (Vn_cacheCheck(vnp) == 0)) {
	VnUnlock(vnp, locktype);
	VnCancelReservation_r(vnp);
	*ec = VNOVNODE;
	/* vnode is labelled correctly by now, so we don't have to invalidate it */
	return NULL;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (locktype == READ_LOCK) {
	VnBeginRead_r(vnp);
    } else {
	VnChangeState_r(vnp, VN_STATE_EXCLUSIVE);
    }
#endif

    if (programType == fileServer)
	VBumpVolumeUsage_r(Vn_volume(vnp));	/* Hack; don't know where it should be
						 * called from.  Maybe VGetVolume */
    return vnp;
}


int TrustVnodeCacheEntry = 1;
/* This variable is bogus--when it's set to 0, the hash chains fill
   up with multiple versions of the same vnode.  Should fix this!! */
void
VPutVnode(Error * ec, register Vnode * vnp)
{
    VOL_LOCK;
    VPutVnode_r(ec, vnp);
    VOL_UNLOCK;
}

/**
 * put back a handle to a vnode object.
 *
 * @param[out] ec   client error code
 * @param[in]  vnp  vnode object pointer
 *
 * @pre VOL_LOCK held.
 *      ref held on vnode.
 *
 * @post ref dropped on vnode.
 *       if vnode was modified or deleted, it is written out to disk
 *       (assuming a write lock was held).
 *
 * @internal volume package internal use only
 */
void
VPutVnode_r(Error * ec, register Vnode * vnp)
{
    int writeLocked;
    VnodeClass class;
    struct VnodeClassInfo *vcp;
    int code;

    *ec = 0;
    assert(Vn_refcount(vnp) != 0);
    class = vnodeIdToClass(Vn_id(vnp));
    vcp = &VnodeClassInfo[class];
    assert(vnp->disk.vnodeMagic == vcp->magic);
    VNLog(200, 2, Vn_id(vnp), (afs_int32) vnp);

#ifdef AFS_DEMAND_ATTACH_FS
    writeLocked = (Vn_state(vnp) == VN_STATE_EXCLUSIVE);
#else
    writeLocked = WriteLocked(&vnp->lock);
#endif

    if (writeLocked) {
	/* sanity checks */
#ifdef AFS_PTHREAD_ENV
	pthread_t thisProcess = pthread_self();
#else /* AFS_PTHREAD_ENV */
	PROCESS thisProcess;
	LWP_CurrentProcess(&thisProcess);
#endif /* AFS_PTHREAD_ENV */
	VNLog(201, 2, (afs_int32) vnp,
	      ((vnp->changed_newTime) << 1) | ((vnp->
						changed_oldTime) << 1) | vnp->
	      delete);
	if (thisProcess != vnp->writer)
	    Abort("VPutVnode: Vnode at 0x%x locked by another process!\n",
		  vnp);


	if (vnp->changed_oldTime || vnp->changed_newTime || vnp->delete) {
	    Volume *vp = Vn_volume(vnp);
	    afs_uint32 now = FT_ApproxTime();
	    assert(Vn_cacheCheck(vnp) == vp->cacheCheck);

	    if (vnp->delete) {
		/* No longer any directory entries for this vnode. Free the Vnode */
		memset(&vnp->disk, 0, sizeof(vnp->disk));
		/* delete flag turned off further down */
		VNLog(202, 2, Vn_id(vnp), (afs_int32) vnp);
	    } else if (vnp->changed_newTime) {
		vnp->disk.serverModifyTime = now;
	    }
	    if (vnp->changed_newTime)
	    {
		V_updateDate(vp) = vp->updateTime = now;
		if(V_volUpCounter(vp)<MAXINT)
			V_volUpCounter(vp)++;
	    }

	    /* The vnode has been changed. Write it out to disk */
	    if (!V_inUse(vp)) {
#ifdef AFS_DEMAND_ATTACH_FS
		VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, 0);
#else
		assert(V_needsSalvaged(vp));
		*ec = VSALVAGE;
#endif
	    } else {
		VnStore(ec, vp, vnp, vcp, class);

		/* If the vnode is to be deleted, and we wrote the vnode out,
		 * free its bitmap entry. Do after the vnode is written so we
		 * don't allocate from bitmap before the vnode is written
		 * (doing so could cause a "addled bitmap" message).
		 */
		if (vnp->delete && !*ec) {
		    if (Vn_volume(vnp)->header->diskstuff.filecount-- < 1)
			Vn_volume(vnp)->header->diskstuff.filecount = 0;
		    VFreeBitMapEntry_r(ec, &vp->vnodeIndex[class],
				       vnodeIdToBitNumber(Vn_id(vnp)));
		}
	    }
	    vcp->writes++;
	    vnp->changed_newTime = vnp->changed_oldTime = 0;
	}
#ifdef AFS_DEMAND_ATTACH_FS
	VnChangeState_r(vnp, VN_STATE_ONLINE);
#endif
    } else {			/* Not write locked */
	if (vnp->changed_newTime || vnp->changed_oldTime || vnp->delete)
	    Abort
		("VPutVnode: Change or delete flag for vnode 0x%x is set but vnode is not write locked!\n",
		 vnp);
#ifdef AFS_DEMAND_ATTACH_FS
	VnEndRead_r(vnp);
#endif
    }

    /* Do not look at disk portion of vnode after this point; it may
     * have been deleted above */
    vnp->delete = 0;
    VnUnlock(vnp, ((writeLocked) ? WRITE_LOCK : READ_LOCK));
    VnCancelReservation_r(vnp);
}

/*
 * Make an attempt to convert a vnode lock from write to read.
 * Do nothing if the vnode isn't write locked or the vnode has
 * been deleted.
 */
int
VVnodeWriteToRead(Error * ec, register Vnode * vnp)
{
    int retVal;
    VOL_LOCK;
    retVal = VVnodeWriteToRead_r(ec, vnp);
    VOL_UNLOCK;
    return retVal;
}

/**
 * convert vnode handle from mutually exclusive to shared access.
 *
 * @param[out] ec   client error code
 * @param[in]  vnp  vnode object pointer
 *
 * @return unspecified use (see out argument 'ec' for error code return)
 *
 * @pre VOL_LOCK held.
 *      ref held on vnode.
 *      write lock held on vnode.
 *
 * @post read lock held on vnode.
 *       if vnode was modified, it has been written to disk.
 *
 * @internal volume package internal use only
 */
int
VVnodeWriteToRead_r(Error * ec, register Vnode * vnp)
{
    int writeLocked;
    VnodeClass class;
    struct VnodeClassInfo *vcp;
    int code;
#ifdef AFS_PTHREAD_ENV
    pthread_t thisProcess;
#else /* AFS_PTHREAD_ENV */
    PROCESS thisProcess;
#endif /* AFS_PTHREAD_ENV */

    *ec = 0;
    assert(Vn_refcount(vnp) != 0);
    class = vnodeIdToClass(Vn_id(vnp));
    vcp = &VnodeClassInfo[class];
    assert(vnp->disk.vnodeMagic == vcp->magic);
    VNLog(300, 2, Vn_id(vnp), (afs_int32) vnp);

#ifdef AFS_DEMAND_ATTACH_FS
    writeLocked = (Vn_state(vnp) == VN_STATE_EXCLUSIVE);
#else
    writeLocked = WriteLocked(&vnp->lock);
#endif
    if (!writeLocked) {
	return 0;
    }


    VNLog(301, 2, (afs_int32) vnp,
	  ((vnp->changed_newTime) << 1) | ((vnp->
					    changed_oldTime) << 1) | vnp->
	  delete);

    /* sanity checks */
#ifdef AFS_PTHREAD_ENV
    thisProcess = pthread_self();
#else /* AFS_PTHREAD_ENV */
    LWP_CurrentProcess(&thisProcess);
#endif /* AFS_PTHREAD_ENV */
    if (thisProcess != vnp->writer)
	Abort("VPutVnode: Vnode at 0x%x locked by another process!\n",
	      (int)vnp);

    if (vnp->delete) {
	return 0;
    }
    if (vnp->changed_oldTime || vnp->changed_newTime) {
	Volume *vp = Vn_volume(vnp);
	afs_uint32 now = FT_ApproxTime();
	assert(Vn_cacheCheck(vnp) == vp->cacheCheck);
	if (vnp->changed_newTime)
	    vnp->disk.serverModifyTime = now;
	if (vnp->changed_newTime)
	    V_updateDate(vp) = vp->updateTime = now;

	/* The inode has been changed.  Write it out to disk */
	if (!V_inUse(vp)) {
#ifdef AFS_DEMAND_ATTACH_FS
	    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, 0);
#else
	    assert(V_needsSalvaged(vp));
	    *ec = VSALVAGE;
#endif
	} else {
	    VnStore(ec, vp, vnp, vcp, class);
	}
    sane:
	vcp->writes++;
	vnp->changed_newTime = vnp->changed_oldTime = 0;
    }

    vnp->writer = 0;
#ifdef AFS_DEMAND_ATTACH_FS
    VnChangeState_r(vnp, VN_STATE_ONLINE);
    VnBeginRead_r(vnp);
#else
    ConvertWriteToReadLock(&vnp->lock);
#endif
    return 0;
}

/* VCloseVnodeFiles - called when a volume is going off line. All open
 * files for vnodes in that volume are closed. This might be excessive,
 * since we may only be taking one volume of a volume group offline.
 */
void
VCloseVnodeFiles_r(Volume * vp)
{
    int i;
    Vnode *vnp, *nvnp;
#ifdef AFS_DEMAND_ATTACH_FS
    VolState vol_state_save;

    vol_state_save = VChangeState_r(vp, VOL_STATE_VNODE_CLOSE);
    VOL_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */

    for (queue_Scan(&vp->vnode_list, vnp, nvnp, Vnode)) {
	IH_REALLYCLOSE(vnp->handle);
	DeleteFromVVnList(vnp);
    }

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
    VChangeState_r(vp, vol_state_save);
#endif /* AFS_DEMAND_ATTACH_FS */
}

/**
 * shut down all vnode cache state for a given volume.
 *
 * @param[in] vp  volume object pointer
 *
 * @pre VOL_LOCK is held
 *
 * @post all file descriptors closed.
 *       all inode handles released.
 *       all vnode cache objects disassociated from volume.
 *
 * @note for DAFS, these operations are performed outside the vol glock under
 *       volume exclusive state VOL_STATE_VNODE_RELEASE.  Please further note
 *       that it would be a bug to acquire and release a volume reservation
 *       during this exclusive operation.  This is due to the fact that we are
 *       generally called during the refcount 1->0 transition.
 *
 * @internal this routine is internal to the volume package
 */
void
VReleaseVnodeFiles_r(Volume * vp)
{
    int i;
    Vnode *vnp, *nvnp;
#ifdef AFS_DEMAND_ATTACH_FS
    VolState vol_state_save;

    vol_state_save = VChangeState_r(vp, VOL_STATE_VNODE_RELEASE);
    VOL_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */

    for (queue_Scan(&vp->vnode_list, vnp, nvnp, Vnode)) {
	IH_RELEASE(vnp->handle);
	DeleteFromVVnList(vnp);
    }

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
    VChangeState_r(vp, vol_state_save);
#endif /* AFS_DEMAND_ATTACH_FS */
}
