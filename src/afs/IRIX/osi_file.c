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
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

struct  AFS_UCRED afs_osi_cred;
int afs_osicred_initialized = 0;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct vfs *afs_cacheVfsp;


/* As of 6.2, we support either XFS or EFS clients. osi_UFSOpen
 * now vectors to the correct EFS or XFS function. If new functionality is
 * added which accesses the inode, that will also need EFS/XFS variants.
 */
#ifdef AFS_SGI_EFS_IOPS_ENV
vnode_t *
afs_EFSIGetVnode(ino_t ainode)
{
    struct inode *ip;
    int error;

    if ((error = igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, ainode, &ip))) {
	osi_Panic("afs_EFSIGetVnode: igetinode failed, error=%d", error);
    }
    /* We don't care about atimes on the cache files, so disable them.  I'm not
     * sure that this is the right place to do this: it should be *after* readi 
     * and getattr and stuff. 
     */
    ip->i_flags &= ~(ISYN | IACC);
    iunlock(ip);
    return (EFS_ITOV(ip));
}
#endif /* AFS_SGI_EFS_IOPS_ENV */

vnode_t *
afs_XFSIGetVnode(ino_t ainode)
{
    struct xfs_inode *ip;
    int error;
    vnode_t *vp;

    if ((error =
	 xfs_igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, ainode, &ip))) {
	osi_Panic("afs_XFSIGetVnode: xfs_igetinode failed, error=%d", error);
    }
    vp = XFS_ITOV(ip);
    return vp;
}

/* Force to 64 bits, even for EFS filesystems. */
void *
osi_UFSOpen(ino_t ainode)
{
    struct inode *ip;
    register struct osi_file *afile = NULL;
    extern int cacheDiskType;
    afs_int32 code = 0;
    int dummy;
    AFS_STATCNT(osi_UFSOpen);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS) {
	osi_Panic("UFSOpen called for non-UFS cache\n");
    }
    if (!afs_osicred_initialized) {
	/* valid for alpha_osf, SunOS, Ultrix */
	memset((char *)&afs_osi_cred, 0, sizeof(struct AFS_UCRED));
	crhold(&afs_osi_cred);	/* don't let it evaporate, since it is static */
	afs_osicred_initialized = 1;
    }
    afile = (struct osi_file *)osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    afile->vnode = AFS_SGI_IGETVNODE(ainode);
    AFS_GLOCK();
    afile->size = VnodeToSize(afile->vnode);
    afile->offset = 0;
    afile->proc = (int (*)())0;
    afile->inum = ainode;	/* for hint validity checking */
    return (void *)afile;
}

int
afs_osi_Stat(register struct osi_file *afile, register struct osi_stat *astat)
{
    register afs_int32 code;
    struct vattr tvattr;
    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi, 320);
    AFS_GUNLOCK();
    tvattr.va_mask = AT_SIZE | AT_BLKSIZE | AT_MTIME | AT_ATIME;
    AFS_VOP_GETATTR(afile->vnode, &tvattr, 0, &afs_osi_cred, code);
    AFS_GLOCK();
    if (code == 0) {
	astat->size = tvattr.va_size;
	astat->mtime = tvattr.va_mtime.tv_sec;
	astat->atime = tvattr.va_atime.tv_sec;
    }
    MReleaseWriteLock(&afs_xosi);
    return code;
}

int
osi_UFSClose(register struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);
    if (afile->vnode) {
	VN_RELE(afile->vnode);
    }

    osi_FreeSmallSpace(afile);
    return 0;
}

int
osi_UFSTruncate(register struct osi_file *afile, afs_int32 asize)
{
    struct AFS_UCRED *oldCred;
    struct vattr tvattr;
    register afs_int32 code;
    struct osi_stat tstat;
    mon_state_t ms;
    AFS_STATCNT(osi_Truncate);

    /* This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize)
	return code;
    MObtainWriteLock(&afs_xosi, 321);
    AFS_GUNLOCK();
    tvattr.va_mask = AT_SIZE;
    tvattr.va_size = asize;
    AFS_VOP_SETATTR(afile->vnode, &tvattr, 0, &afs_osi_cred, code);
    AFS_GLOCK();
    MReleaseWriteLock(&afs_xosi);
    return code;
}

#ifdef AFS_SGI_EFS_IOPS_ENV
void
osi_DisableAtimes(struct vnode *avp)
{
    if (afs_CacheFSType == AFS_SGI_EFS_CACHE) {
	struct inode *ip = EFS_VTOI(avp);
	ip->i_flags &= ~IACC;
    }

}
#endif /* AFS_SGI_EFS_IOPS_ENV */


/* Generic read interface */
int
afs_osi_Read(register struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
    struct AFS_UCRED *oldCred;
    ssize_t resid;
    register afs_int32 code;
    register afs_int32 cnt1 = 0;
    AFS_STATCNT(osi_Read);

    /**
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
	gop_rdwr(UIO_READ, afile->vnode, (caddr_t) aptr, asize, afile->offset,
		 AFS_UIOSYS, 0, 0x7fffffff, &afs_osi_cred, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
#ifdef AFS_SGI_EFS_IOPS_ENV
	osi_DisableAtimes(afile->vnode);
#endif /* AFS_SGI_EFS_IOPS_ENV */
    } else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
		   ICL_TYPE_INT32, code);
	code = -1;
    }
    return code;
}

/* Generic write interface */
int
afs_osi_Write(register struct osi_file *afile, afs_int32 offset, void *aptr,
	      afs_int32 asize)
{
    struct AFS_UCRED *oldCred;
    ssize_t resid;
    register afs_int32 code;
    AFS_STATCNT(osi_Write);
    if (!afile)
	osi_Panic("afs_osi_Write called with null param");
    if (offset != -1)
	afile->offset = offset;
    AFS_GUNLOCK();
    code =
	gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize,
		 afile->offset, AFS_UIOSYS, 0, 0x7fffffff, &afs_osi_cred,
		 &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    } else {
	if (code == ENOSPC)
	    afs_warnuser
		("\n\n\n*** Cache partition is FULL - Decrease cachesize!!! ***\n\n");
	code = -1;
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
afs_osi_MapStrategy(int (*aproc) (), register struct buf *bp)
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
