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
#include "afs/osi_inode.h"


int afs_osicred_initialized = 0;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct vfs *afs_cacheVfsp;


#ifdef AFS_HAVE_VXFS

/* Support for UFS and VXFS caches. The assumption here is that the size of
 * a cache file also does not exceed 32 bits. 
 */

/* Initialized in osi_InitCacheFSType(). Used to determine inode type. */
int afs_CacheFSType = -1;

/* pointer to VXFS routine to access vnodes by inode number */
int (*vxfs_vx_vp_byino) ();

/* Initialize the cache operations. Called while initializing cache files. */
void
afs_InitDualFSCacheOps(struct vnode *vp)
{
    int code;
    static int inited = 0;
    struct vfs *vfsp;
    struct statvfs64 vfst;

    if (inited)
	return;
    inited = 1;

    if (vp == NULL)
	return;

    vfsp = vp->v_vfsp;
    if (vfsp == NULL)
	osi_Panic("afs_InitDualFSCacheOps: vp->v_vfsp is NULL");
    code = VFS_STATVFS(vfsp, &vfst);
    if (code)
	osi_Panic("afs_InitDualFSCacheOps: statvfs failed");

    if (strcmp(vfst.f_basetype, "vxfs") == 0) {
	vxfs_vx_vp_byino = (int (*)())modlookup("vxfs", "vx_vp_byino");
	if (vxfs_vx_vp_byino == NULL)
	    osi_Panic
		("afs_InitDualFSCacheOps: modlookup(vx_vp_byino) failed");

	afs_CacheFSType = AFS_SUN_VXFS_CACHE;
	return;
    }

    afs_CacheFSType = AFS_SUN_UFS_CACHE;
    return;
}

ino_t
VnodeToIno(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    vattr.va_mask = AT_FSID | AT_NODEID;	/* quick return using this mask. */
#ifdef AFS_SUN511_ENV
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp, NULL);
#else
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp);
#endif
    if (code) {
	osi_Panic("VnodeToIno");
    }
    return vattr.va_nodeid;
}

dev_t
VnodeToDev(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    vattr.va_mask = AT_FSID | AT_NODEID;	/* quick return using this mask. */
    AFS_GUNLOCK();
#ifdef AFS_SUN511_ENV
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp, NULL);
#else
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp);
#endif
    AFS_GLOCK();
    if (code) {
	osi_Panic("VnodeToDev");
    }
    return (dev_t) vattr.va_fsid;
}

afs_int32
VnodeToSize(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    /*
     * We lock xosi in osi_Stat, so we probably should
     * lock it here too - RWH.
     */
    ObtainWriteLock(&afs_xosi, 578);
    vattr.va_mask = AT_SIZE;
    AFS_GUNLOCK();
#ifdef AFS_SUN511_ENV
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp, NULL);
#else
    code = VOP_GETATTR(vp, &vattr, 0, afs_osi_credp);
#endif
    AFS_GLOCK();
    if (code) {
	osi_Panic("VnodeToSize");
    }
    ReleaseWriteLock(&afs_xosi);
    return (afs_int32) (vattr.va_size);
}

void *
osi_VxfsOpen(afs_dcache_id_t *ainode)
{
    struct vnode *vp;
    struct osi_file *afile = NULL;
    afs_int32 code = 0;
    int dummy;
    afile = osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    code = (*vxfs_vx_vp_byino) (afs_cacheVfsp, &vp, (unsigned int)ainode->ufs);
    AFS_GLOCK();
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("VxfsOpen: vx_vp_byino failed");
    }
    afile->vnode = vp;
    afile->size = VnodeToSize(afile->vnode);
    afile->offset = 0;
    afile->proc = (int (*)())0;
    return (void *)afile;
}
#endif /* AFS_HAVE_VXFS */

void *
osi_UfsOpen(afs_dcache_id_t *ainode)
{
#ifdef AFS_CACHE_VNODE_PATH
    struct vnode *vp;
#else
    struct inode *ip;
#endif
    struct osi_file *afile = NULL;
    afs_int32 code = 0;
    int dummy;
#ifdef AFS_CACHE_VNODE_PATH
    char namebuf[1024];
    struct pathname lookpn;
#endif
    struct osi_stat tstat;
    afile = osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();

    /*
     * AFS_CACHE_VNODE_PATH can be used with any file system, including ZFS or tmpfs.
     * The ainode is not an inode number but a path.
     */
#ifdef AFS_CACHE_VNODE_PATH
    /* Can not use vn_open or lookupname, they use user's CRED()
     * We need to run as root So must use low level lookuppnvp
     * assume fname starts with /
     */

    code = pn_get_buf(ainode->ufs, AFS_UIOSYS, &lookpn, namebuf, sizeof(namebuf));
    if (code != 0) 
        osi_Panic("UfsOpen: pn_get_buf failed %ld %s", code, ainode->ufs);
 
	VN_HOLD(rootdir); /* released in loopuppnvp */
	code = lookuppnvp(&lookpn, NULL, FOLLOW, NULL, &vp, 
           rootdir, rootdir, afs_osi_credp);
    if (code != 0)  
        osi_Panic("UfsOpen: lookuppnvp failed %ld %s", code, ainode->ufs);
	
#ifdef AFS_SUN511_ENV
    code = VOP_OPEN(&vp, FREAD|FWRITE, afs_osi_credp, NULL);
#else
    code = VOP_OPEN(&vp, FREAD|FWRITE, afs_osi_credp);
#endif

    if (code != 0)
        osi_Panic("UfsOpen: VOP_OPEN failed %ld %s", code, ainode->ufs);

#else
    code =
	igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, ainode->ufs, &ip,
		  CRED(), &dummy);
#endif
    AFS_GLOCK();
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UfsOpen: igetinode failed %ld %s", code, ainode->ufs);
    }
#ifdef AFS_CACHE_VNODE_PATH
    afile->vnode = vp;
    code = afs_osi_Stat(afile, &tstat);
    afile->size = tstat.size;
#else
    afile->vnode = ITOV(ip);
    afile->size = VTOI(afile->vnode)->i_size;
#endif
    afile->offset = 0;
    afile->proc = (int (*)())0;
    return (void *)afile;
}

/**
  * In Solaris 7 we use 64 bit inode numbers
  */
void *
osi_UFSOpen(afs_dcache_id_t *ainode)
{
    extern int cacheDiskType;
    AFS_STATCNT(osi_UFSOpen);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS) {
	osi_Panic("UFSOpen called for non-UFS cache\n");
    }
    if (!afs_osicred_initialized) {
	afs_osi_credp = kcred;
	afs_osicred_initialized = 1;
    }
#ifdef AFS_HAVE_VXFS
    if (afs_CacheFSType == AFS_SUN_VXFS_CACHE)
	return osi_VxfsOpen(ainode);
#endif
    return osi_UfsOpen(ainode);
}

int
afs_osi_Stat(struct osi_file *afile, struct osi_stat *astat)
{
    afs_int32 code;
    struct vattr tvattr;
    AFS_STATCNT(osi_Stat);
    ObtainWriteLock(&afs_xosi, 320);
    /* Ufs doesn't seem to care about the flags so we pass 0 for now */
    tvattr.va_mask = AT_ALL;
    AFS_GUNLOCK();
#ifdef AFS_SUN511_ENV 
    code = VOP_GETATTR(afile->vnode, &tvattr, 0, afs_osi_credp, NULL);
#else
    code = VOP_GETATTR(afile->vnode, &tvattr, 0, afs_osi_credp);
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
    afs_ucred_t *oldCred;
    struct vattr tvattr;
    afs_int32 code;
    struct osi_stat tstat;
    AFS_STATCNT(osi_Truncate);

    /* This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize)
	return code;
    ObtainWriteLock(&afs_xosi, 321);
    tvattr.va_mask = AT_SIZE;
    tvattr.va_size = asize;
    /*
     * The only time a flag is used (ATTR_UTIME) is when we're changing the time 
     */
    AFS_GUNLOCK();
#ifdef AFS_SUN510_ENV
    code = VOP_SETATTR(afile->vnode, &tvattr, 0, afs_osi_credp, NULL);
#else
    code = VOP_SETATTR(afile->vnode, &tvattr, 0, afs_osi_credp);
#endif
    AFS_GLOCK();
    ReleaseWriteLock(&afs_xosi);
    return code;
}

void
osi_DisableAtimes(struct vnode *avp)
{
    if (afs_CacheFSType == AFS_SUN_UFS_CACHE) {
#ifndef AFS_CACHE_VNODE_PATH 
	struct inode *ip = VTOI(avp);
	rw_enter(&ip->i_contents, RW_READER);
	mutex_enter(&ip->i_tlock);
	ip->i_flag &= ~IACC;
	mutex_exit(&ip->i_tlock);
	rw_exit(&ip->i_contents);
#endif
    }
}


/* Generic read interface */
int
afs_osi_Read(struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
    afs_ucred_t *oldCred;
    ssize_t resid;
    afs_int32 code;
    afs_int32 cnt1 = 0;
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
		 AFS_UIOSYS, 0, 0, afs_osi_credp, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	osi_DisableAtimes(afile->vnode);
    } else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
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
    afs_ucred_t *oldCred;
    ssize_t resid;
    afs_int32 code;
    AFS_STATCNT(osi_Write);
    if (!afile)
	osi_Panic("afs_osi_Write called with null param");
    if (offset != -1)
	afile->offset = offset;
    AFS_GUNLOCK();
    code =
	gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize,
		 afile->offset, AFS_UIOSYS, 0, RLIM64_INFINITY, afs_osi_credp,
		 &resid);
    AFS_GLOCK();
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
