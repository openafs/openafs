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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */


int afs_osicred_initialized = 0;
#ifndef AFS_FBSD80_ENV	/* cr_groups is now malloc()'d */
afs_ucred_t afs_osi_cred;
#endif
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct mount *afs_cacheVfsp;


void *
osi_UFSOpen(afs_dcache_id_t *ainode)
{
    struct osi_file *afile;
    struct vnode *vp;
    extern int cacheDiskType;
    afs_int32 code;

    AFS_STATCNT(osi_UFSOpen);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
	osi_Panic("UFSOpen called for non-UFS cache\n");
    afile = osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    code = VFS_VGET(afs_cacheVfsp, (ino_t) ainode->ufs, LK_EXCLUSIVE, &vp);
    AFS_GLOCK();
    if (code == 0 && vp->v_type == VNON)
	code = ENOENT;
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UFSOpen: igetinode failed");
    }
#if defined(AFS_FBSD80_ENV)
    VOP_UNLOCK(vp, 0);
#else
    VOP_UNLOCK(vp, 0, curthread);
#endif
    afile->vnode = vp;
    afile->size = VTOI(vp)->i_size;
    afile->offset = 0;
    afile->proc = NULL;
    return (void *)afile;
}

int
afs_osi_Stat(struct osi_file *afile, struct osi_stat *astat)
{
    afs_int32 code;
    struct vattr tvattr;
    AFS_STATCNT(osi_Stat);
    ObtainWriteLock(&afs_xosi, 320);
    AFS_GUNLOCK();
#if defined(AFS_FBSD80_ENV)
    vn_lock(afile->vnode, LK_EXCLUSIVE | LK_RETRY);
    code = VOP_GETATTR(afile->vnode, &tvattr, afs_osi_credp);
    VOP_UNLOCK(afile->vnode, 0);
#else
    vn_lock(afile->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
    code = VOP_GETATTR(afile->vnode, &tvattr, afs_osi_credp, curthread);
    VOP_UNLOCK(afile->vnode, LK_EXCLUSIVE, curthread);
#endif
    AFS_GLOCK();
    if (code == 0) {
	astat->size = tvattr.va_size;
	astat->mtime = tvattr.va_mtime.tv_sec;
	astat->atime = tvattr.va_atime.tv_sec;
    }
    ReleaseWriteLock(&afs_xosi);
    return code;
}

int
osi_UFSClose(struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);
    if (afile->vnode) {
	AFS_RELE(afile->vnode);
    }

    osi_FreeSmallSpace(afile);
    return 0;
}

int
osi_UFSTruncate(struct osi_file *afile, afs_int32 asize)
{
    struct vattr tvattr;
    struct vnode *vp;
    afs_int32 code, glocked;
    AFS_STATCNT(osi_Truncate);

    ObtainWriteLock(&afs_xosi, 321);
    vp = afile->vnode;
    /*
     * This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    glocked = ISAFS_GLOCK();
    if (glocked)
      AFS_GUNLOCK();
#if defined(AFS_FBSD80_ENV)
    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
    code = VOP_GETATTR(afile->vnode, &tvattr, afs_osi_credp);
#else
    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
    code = VOP_GETATTR(afile->vnode, &tvattr, afs_osi_credp, curthread);
#endif
    if (code != 0 || tvattr.va_size <= asize)
	goto out;

    VATTR_NULL(&tvattr);
    tvattr.va_size = asize;
#if defined(AFS_FBSD80_ENV)
    code = VOP_SETATTR(vp, &tvattr, afs_osi_credp);
#else
    code = VOP_SETATTR(vp, &tvattr, afs_osi_credp, curthread);
#endif

out:
#if defined(AFS_FBSD80_ENV)
    VOP_UNLOCK(vp, 0);
#else
    VOP_UNLOCK(vp, LK_EXCLUSIVE, curthread);
#endif
    if (glocked)
      AFS_GLOCK();
    ReleaseWriteLock(&afs_xosi);
    return code;
}

void
osi_DisableAtimes(struct vnode *avp)
{
    struct inode *ip = VTOI(avp);
    ip->i_flag &= ~IN_ACCESS;
}


/* Generic read interface */
int
afs_osi_Read(struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
#if (__FreeBSD_version >= 900505 && __FreeBSD_Version < 1000000) ||__FreeBSD_version >= 1000009
    ssize_t resid;
#else
    int resid;
#endif
    afs_int32 code;
    AFS_STATCNT(osi_Read);

    /**
      * If the osi_file passed in is NULL, panic only if AFS is not shutting
      * down. No point in crashing when we are already shutting down
      */
    if (!afile) {
	if (afs_shuttingdown == AFS_RUNNING)
	    osi_Panic("osi_Read called with null param");
	else
	    return -EIO;
    }

    if (offset != -1)
	afile->offset = offset;
    AFS_GUNLOCK();
    code =
	gop_rdwr(UIO_READ, afile->vnode, (caddr_t) aptr, asize, afile->offset,
		 AFS_UIOSYS, IO_UNIT, afs_osi_credp, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	osi_DisableAtimes(afile->vnode);
    } else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, (int)resid,
		   ICL_TYPE_INT32, code);
	if (code > 0) {
	    code = -code;
	}
    }
    return code;
}

/* Generic write interface */
int
afs_osi_Write(struct osi_file *afile, afs_int32 offset, void *aptr,
	      afs_int32 asize)
{
#if (__FreeBSD_version >= 900505 && __FreeBSD_Version < 1000000) ||__FreeBSD_version >= 1000009
    ssize_t resid;
#else
    int resid;
#endif
    afs_int32 code;
    AFS_STATCNT(osi_Write);
    if (!afile)
	osi_Panic("afs_osi_Write called with null param");
    if (offset != -1)
	afile->offset = offset;
    {
	AFS_GUNLOCK();
	code =
	    gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize,
		     afile->offset, AFS_UIOSYS, IO_UNIT, afs_osi_credp,
		     &resid);
	AFS_GLOCK();
    }
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    } else {
	if (code > 0) {
	    code = -code;
	}
    }
    if (afile->proc) {
	(*afile->proc) (afile, code);
    }
    return code;
}


/*  This work should be handled by physstrat in ca/machdep.c.
    This routine written from the RT NFS port strategy routine.
    It has been generalized a bit, but should still be pretty clear. */
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
    if (afs_cold_shutdown) {
	afs_osicred_initialized = 0;
    }
}
