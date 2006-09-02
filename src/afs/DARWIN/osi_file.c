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
    ("$Header: /cvs/openafs/src/afs/DARWIN/osi_file.c,v 1.8.2.5 2006/08/20 22:16:47 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/osi_inode.h"


int afs_osicred_initialized = 0;
struct AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct mount *afs_cacheVfsp;
int afs_CacheFSType = -1;

/* Initialize the cache operations. Called while initializing cache files. */
void
afs_InitDualFSCacheOps(struct vnode *vp)
{
    int code;
    static int inited = 0;
#ifdef AFS_DARWIN80_ENV
    char *buffer = (char*)_MALLOC(MFSNAMELEN, M_TEMP, M_WAITOK);
#endif

    if (inited)
	return;
    inited = 1;

    if (vp == NULL)
	return;
#ifdef AFS_DARWIN80_ENV
    vfs_name(vnode_mount(vp), buffer);
    if (strncmp("hfs", buffer, 3) == 0)
#else
    if (strncmp("hfs", vp->v_mount->mnt_vfc->vfc_name, 3) == 0)
#endif
	afs_CacheFSType = AFS_APPL_HFS_CACHE;
#ifdef AFS_DARWIN80_ENV
    else if (strncmp("ufs", buffer, 3) == 0)
#else
    else if (strncmp("ufs", vp->v_mount->mnt_vfc->vfc_name, 3) == 0)
#endif
	afs_CacheFSType = AFS_APPL_UFS_CACHE;
    else
	osi_Panic("Unknown cache vnode type\n");
#ifdef AFS_DARWIN80_ENV
    _FREE(buffer, M_TEMP);
#endif
}

ino_t
VnodeToIno(vnode_t avp)
{
    unsigned long ret;

#ifndef AFS_DARWIN80_ENV
    if (afs_CacheFSType == AFS_APPL_UFS_CACHE) {
	struct inode *ip = VTOI(avp);
	ret = ip->i_number;
    } else if (afs_CacheFSType == AFS_APPL_HFS_CACHE) {
#endif
#if defined(AFS_DARWIN80_ENV) 
	struct vattr va;
	VATTR_INIT(&va);
        VATTR_WANTED(&va, va_fileid);
	if (vnode_getattr(avp, &va, afs_osi_ctxtp))
	    osi_Panic("VOP_GETATTR failed in VnodeToIno\n");
        if (!VATTR_ALL_SUPPORTED(&va))
	    osi_Panic("VOP_GETATTR unsupported fileid in VnodeToIno\n");
	ret = va.va_fileid;
#elif !defined(VTOH)
	struct vattr va;
	if (VOP_GETATTR(avp, &va, &afs_osi_cred, current_proc()))
	    osi_Panic("VOP_GETATTR failed in VnodeToIno\n");
	ret = va.va_fileid;
#else
	struct hfsnode *hp = VTOH(avp);
	ret = H_FILEID(hp);
#endif
#ifndef AFS_DARWIN80_ENV
    } else
	osi_Panic("VnodeToIno called before cacheops initialized\n");
#endif
    return ret;
}


dev_t
VnodeToDev(vnode_t avp)
{
#ifndef AFS_DARWIN80_ENV
    if (afs_CacheFSType == AFS_APPL_UFS_CACHE) {
	struct inode *ip = VTOI(avp);
	return ip->i_dev;
    } else if (afs_CacheFSType == AFS_APPL_HFS_CACHE) {
#endif
#if defined(AFS_DARWIN80_ENV) 
	struct vattr va;
        VATTR_INIT(&va);
        VATTR_WANTED(&va, va_fsid);
	if (vnode_getattr(avp, &va, afs_osi_ctxtp))
	    osi_Panic("VOP_GETATTR failed in VnodeToDev\n");
        if (!VATTR_ALL_SUPPORTED(&va))
	    osi_Panic("VOP_GETATTR unsupported fsid in VnodeToIno\n");
	return va.va_fsid;	/* XXX they say it's the dev.... */
#elif !defined(VTOH)
	struct vattr va;
	if (VOP_GETATTR(avp, &va, &afs_osi_cred, current_proc()))
	    osi_Panic("VOP_GETATTR failed in VnodeToDev\n");
	return va.va_fsid;	/* XXX they say it's the dev.... */
#else
	struct hfsnode *hp = VTOH(avp);
	return H_DEV(hp);
#endif
#ifndef AFS_DARWIN80_ENV
    } else
	osi_Panic("VnodeToDev called before cacheops initialized\n");
#endif
}

void *
osi_UFSOpen(afs_int32 ainode)
{
    struct vnode *vp;
    struct vattr va;
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
	afs_osi_cred.cr_ref++;
	afs_osi_cred.cr_ngroups = 1;
	afs_osicred_initialized = 1;
    }
    afile = (struct osi_file *)osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
    if (afs_CacheFSType == AFS_APPL_HFS_CACHE)
	code = igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, &ainode, &vp, &va, &dummy);	/* XXX hfs is broken */
    else if (afs_CacheFSType == AFS_APPL_UFS_CACHE)
#endif
	code =
	    igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, (ino_t) ainode,
		      &vp, &va, &dummy);
#ifndef AFS_DARWIN80_ENV
    else
	panic("osi_UFSOpen called before cacheops initialized\n");
#endif
    AFS_GLOCK();
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UFSOpen: igetinode failed");
    }
    afile->vnode = vp;
    afile->size = va.va_size;
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
#ifdef AFS_DARWIN80_ENV
    VATTR_INIT(&tvattr);
    VATTR_WANTED(&tvattr, va_size);
    VATTR_WANTED(&tvattr, va_blocksize);
    VATTR_WANTED(&tvattr, va_mtime);
    VATTR_WANTED(&tvattr, va_atime);
    code = vnode_getattr(afile->vnode, &tvattr, afs_osi_ctxtp);
    if (code == 0 && !VATTR_ALL_SUPPORTED(&tvattr))
       code = EINVAL;
#else
    code = VOP_GETATTR(afile->vnode, &tvattr, &afs_osi_cred, current_proc());
#endif
    AFS_GLOCK();
    if (code == 0) {
	astat->size = tvattr.va_size;
	astat->blksize = tvattr.va_blocksize;
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
#ifdef AFS_DARWIN80_ENV
        vnode_close(afile->vnode, O_RDWR, afs_osi_ctxtp);
#else
	AFS_RELE(afile->vnode);
#endif
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
#ifdef AFS_DARWIN80_ENV
    VATTR_INIT(&tvattr);
    VATTR_SET(&tvattr, va_size, asize);
    code = vnode_setattr(afile->vnode, &tvattr, afs_osi_ctxtp);
#else
    VATTR_NULL(&tvattr);
    tvattr.va_size = asize;
    code = VOP_SETATTR(afile->vnode, &tvattr, &afs_osi_cred, current_proc());
#endif
    AFS_GLOCK();
    MReleaseWriteLock(&afs_xosi);
    return code;
}

void
#ifdef AFS_DARWIN80_ENV
osi_DisableAtimes(vnode_t avp)
#else
osi_DisableAtimes(struct vnode *avp)
#endif
{
#ifdef AFS_DARWIN80_ENV
    struct vnode_attr vap;

    VATTR_INIT(&vap);
    VATTR_CLEAR_SUPPORTED(&vap, va_access_time);
    vnode_setattr(avp, &vap, afs_osi_ctxtp);
#else
    if (afs_CacheFSType == AFS_APPL_UFS_CACHE) {
	struct inode *ip = VTOI(avp);
	ip->i_flag &= ~IN_ACCESS;
    }
#ifdef VTOH			/* can't do this without internals */
    else if (afs_CacheFSType == AFS_APPL_HFS_CACHE) {
	struct hfsnode *hp = VTOH(avp);
	hp->h_nodeflags &= ~IN_ACCESS;
    }
#endif
#endif
}


/* Generic read interface */
int
afs_osi_Read(register struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
    struct AFS_UCRED *oldCred;
    afs_size_t resid;
    register afs_int32 code;
#ifdef AFS_DARWIN80_ENV
    uio_t uio;
#endif
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
#ifdef AFS_DARWIN80_ENV
    uio=uio_create(1, afile->offset, AFS_UIOSYS, UIO_READ);
    uio_addiov(uio, CAST_USER_ADDR_T(aptr), asize);
    code = VNOP_READ(afile->vnode, uio, IO_UNIT, afs_osi_ctxtp);
    resid = AFS_UIO_RESID(uio);
    uio_free(uio);
#else
    code =
	gop_rdwr(UIO_READ, afile->vnode, (caddr_t) aptr, asize, afile->offset,
		 AFS_UIOSYS, IO_UNIT, &afs_osi_cred, &resid);
#endif
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
afs_osi_Write(register struct osi_file *afile, afs_int32 offset, void *aptr,
	      afs_int32 asize)
{
    struct AFS_UCRED *oldCred;
    afs_size_t resid;
    register afs_int32 code;
#ifdef AFS_DARWIN80_ENV
    uio_t uio;
#endif
    AFS_STATCNT(osi_Write);
    if (!afile)
	osi_Panic("afs_osi_Write called with null param");
    if (offset != -1)
	afile->offset = offset;
    {
	AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
        uio=uio_create(1, afile->offset, AFS_UIOSYS, UIO_WRITE);
        uio_addiov(uio, CAST_USER_ADDR_T(aptr), asize);
        code = VNOP_WRITE(afile->vnode, uio, IO_UNIT, afs_osi_ctxtp);
        resid = AFS_UIO_RESID(uio);
        uio_free(uio);
#else
	code =
	    gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize,
		     afile->offset, AFS_UIOSYS, IO_UNIT, &afs_osi_cred,
		     &resid);
#endif
	AFS_GLOCK();
    }
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    } else {
	code = -1;
    }
    if (afile->proc) {
	(*afile->proc) (afile, code);
    }
    return code;
}





void
shutdown_osifile(void)
{
    extern int afs_cold_shutdown;

    AFS_STATCNT(shutdown_osifile);
    if (afs_cold_shutdown) {
	afs_osicred_initialized = 0;
    }
}
