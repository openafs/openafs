/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * System independent part of vget VFS call.
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* statistics stuff */



extern int afs_NFSRootOnly;
int afs_rootCellIndex = 0;
#if !defined(AFS_LINUX20_ENV)
/* This is the common part of the vget VFS call. */
int afs_osi_vget(struct vcache **avcpp, struct fid *afidp,
		 struct vrequest *areqp)
{
    struct VenusFid vfid;
    struct SmallFid Sfid;
    extern struct cell *afs_GetCellByIndex();
    register struct cell *tcell;
    struct vrequest treq;
    register afs_int32 code = 0, cellindex;
    afs_int32 ret;

    bcopy(afidp->fid_data, (char *)&Sfid, SIZEOF_SMALLFID);
#ifdef AFS_OSF_ENV
    Sfid.Vnode = afidp->fid_reserved;
#endif
    if (afs_NFSRootOnly &&
	Sfid.Volume == afs_rootFid.Fid.Volume &&
	Sfid.Vnode == afs_rootFid.Fid.Vnode &&
	(Sfid.CellAndUnique & 0xffffff) ==
	(afs_rootFid.Fid.Unique & 0xffffff) &&
	((Sfid.CellAndUnique >> 24) & 0xff) == afs_rootCellIndex) {
	vfid = afs_rootFid;
    }
    else {
	/* Need to extract fid from SmallFid. Will need a wild card option for
	 * finding the right vcache entry.
	 */
	struct cell *tcell;
	cellindex = (Sfid.CellAndUnique >> 24) & 0xff;
	tcell = afs_GetCellByIndex(cellindex, READ_LOCK);
	if (!tcell) {
	    return ENOENT;
        }
	vfid.Cell = tcell->cell;
	afs_PutCell(tcell, WRITE_LOCK);
	vfid.Fid.Volume = Sfid.Volume;
	vfid.Fid.Vnode = Sfid.Vnode;
	vfid.Fid.Unique = Sfid.CellAndUnique & 0xffffff;
    }


    /* First attempt to find in cache using wildcard. If that fails,
     * try the usual route to try to get the vcache from the server.
     * This could be done better by splitting out afs_FindVCache from
     * afs_GetVCache.
     */

    ret = afs_NFSFindVCache(avcpp, &vfid, 1);
    if (ret > 1) {
	/* More than one entry matches. */
	code = ENOENT;
    }
    else if (ret == 0) {
	/* didn't find an entry. */
	*avcpp = afs_GetVCache(&vfid, &treq, (afs_int32 *)0, (struct vcache*)0, 0);
    }
    if (! *avcpp) {
	code = ENOENT;
    }

    return code;
}
#endif /* AFS_LINUX20_ENV */
