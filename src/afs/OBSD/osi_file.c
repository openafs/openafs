/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/OBSD/osi_file.c,v 1.10.2.1 2006/06/23 14:21:12 rees Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */


int afs_osicred_initialized;
struct AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct mount *afs_cacheVfsp;


void *
osi_UFSOpen(afs_int32 ainode)
{
    struct osi_file *afile;
    struct vnode *vp;
    extern int cacheDiskType;
    afs_int32 code;

    AFS_STATCNT(osi_UFSOpen);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
	osi_Panic("UFSOpen called for non-UFS cache\n");
    afile = (struct osi_file *)osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    code = VFS_VGET(cacheDev.mp, (ino_t) ainode, &vp);
    AFS_GLOCK();
    if (code == 0 && vp->v_type == VNON)
	code = ENOENT;
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UFSOpen: igetinode failed");
    }
    VOP_UNLOCK(vp, 0, curproc);
    afile->vnode = vp;
#ifdef AFS_OBSD39_ENV
    afile->size = VTOI(vp)->i_ffs1_size;
#else
    afile->size = VTOI(vp)->i_ffs_size;
#endif
    afile->offset = 0;
    afile->proc = NULL;
    afile->inum = ainode;	/* for hint validity checking */
    return (void *)afile;
}

int
afs_osi_Stat(struct osi_file *afile, struct osi_stat *astat)
{
    afs_int32 code;
    struct vattr tvattr;

    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi, 320);
    AFS_GUNLOCK();
    code = VOP_GETATTR(afile->vnode, &tvattr, afs_osi_credp, curproc);
    AFS_GLOCK();
    if (code == 0) {
	astat->size = afile->size = tvattr.va_size;
	astat->blksize = tvattr.va_blocksize;
	astat->mtime = tvattr.va_mtime.tv_sec;
	astat->atime = tvattr.va_atime.tv_sec;
    }
    MReleaseWriteLock(&afs_xosi);
    return code;
}

int
osi_UFSClose(struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);

    if (afile->vnode)
	AFS_RELE(afile->vnode);

    osi_FreeSmallSpace(afile);
    return 0;
}

int
osi_UFSTruncate(struct osi_file *afile, afs_int32 asize)
{
    struct vattr tvattr;
    afs_int32 code;
    struct osi_stat tstat;

    AFS_STATCNT(osi_Truncate);

    /*
     * This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize)
	return code;

    MObtainWriteLock(&afs_xosi, 321);
    VATTR_NULL(&tvattr);
    tvattr.va_size = asize;
    AFS_GUNLOCK();
    VOP_LOCK(afile->vnode, LK_EXCLUSIVE | LK_RETRY, curproc);
    code = VOP_SETATTR(afile->vnode, &tvattr, afs_osi_credp, curproc);
    VOP_UNLOCK(afile->vnode, 0, curproc);
    AFS_GLOCK();
    if (code == 0)
	afile->size = asize;
    MReleaseWriteLock(&afs_xosi);
    return code;
}

void
osi_DisableAtimes(struct vnode *avp)
{
#if 0
    VTOI(avp)->i_flag &= ~IN_ACCESS;
#endif
}


/* Generic read interface */
int
afs_osi_Read(struct osi_file *afile, int offset, void *aptr, afs_int32 asize)
{
    unsigned int resid;
    afs_int32 code;

    AFS_STATCNT(osi_Read);

    /*
     * If the osi_file passed in is NULL, panic only if AFS is not shutting
     * down. No point in crashing when we are already shutting down
     */
    if (!afile) {
	if (!afs_shuttingdown)
	    osi_Panic("osi_Read called with null param");
	else
	    return EIO;
    }

    if (offset != -1)
	afile->offset = offset;
    AFS_GUNLOCK();
    code =
	vn_rdwr(UIO_READ, afile->vnode, aptr, asize, afile->offset,
		AFS_UIOSYS, IO_UNIT, afs_osi_credp, &resid, curproc);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	osi_DisableAtimes(afile->vnode);
    } else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
		   ICL_TYPE_INT32, code);
	code = -1;
    }
    return code;
}

/* Generic write interface */
int
afs_osi_Write(struct osi_file *afile, afs_int32 offset, void *aptr,
	      afs_int32 asize)
{
    unsigned int resid;
    afs_int32 code;

    AFS_STATCNT(osi_Write);
    if (!afile)
	osi_Panic("afs_osi_Write called with null afile");
    if (offset != -1)
	afile->offset = offset;

    AFS_GUNLOCK();
    VOP_LOCK(afile->vnode, LK_EXCLUSIVE | LK_RETRY, curproc);
    code =
	vn_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize, afile->offset,
		AFS_UIOSYS, IO_UNIT, afs_osi_credp, &resid, curproc);
    VOP_UNLOCK(afile->vnode, 0, curproc);
    AFS_GLOCK();

    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	if (afile->offset > afile->size)
	    afile->size = afile->offset;
    } else
	code = -1;

    if (afile->proc)
	(*afile->proc) (afile, code);

    return code;
}

/*
 * This work should be handled by physstrat in ca/machdep.c.  This routine
 * written from the RT NFS port strategy routine.  It has been generalized a
 * bit, but should still be pretty clear.
 */
int
afs_osi_MapStrategy(int (*aproc) (), struct buf *bp)
{
    afs_int32 returnCode;

    AFS_STATCNT(osi_MapStrategy);
    returnCode = (*aproc) (bp);

    return returnCode;
}

void
shutdown_osifile(void)
{
    AFS_STATCNT(shutdown_osifile);
    if (afs_cold_shutdown)
	afs_osicred_initialized = 0;
}
