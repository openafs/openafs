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

#ifdef AFS_LINUX24_ENV
#include "h/module.h" /* early to avoid printf->printk mapping */
#endif
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "h/smp_lock.h"


int afs_osicred_initialized = 0;
struct AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
#if defined(AFS_LINUX24_ENV)
extern struct vfsmount *afs_cacheMnt;
#endif
extern struct super_block *afs_cacheSBp;

#if defined(AFS_LINUX26_ENV) 
void *
osi_UFSOpen(afs_int32 ainode)
{
    register struct osi_file *afile = NULL;
    extern int cacheDiskType;
    struct inode *tip = NULL;
    struct dentry *dp = NULL;
    struct file *filp = NULL;
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
    afile = (struct osi_file *)osi_AllocLargeSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    if (!afile) {
	osi_Panic("osi_UFSOpen: Failed to allocate %d bytes for osi_file.\n",
		  sizeof(struct osi_file));
    }
    memset(afile, 0, sizeof(struct osi_file));
    tip = iget(afs_cacheSBp, (u_long) ainode);
    if (!tip)
	osi_Panic("Can't get inode %d\n", ainode);
    tip->i_flags |= MS_NOATIME;	/* Disable updating access times. */

    dp = d_alloc_anon(tip);
    if (!dp) 
           osi_Panic("Can't get dentry for inode %d\n", ainode);          

    filp = dentry_open(dp, mntget(afs_cacheMnt), O_RDWR);
    if (IS_ERR(filp))
	osi_Panic("Can't open inode %d\n", ainode);
    afile->filp = filp;
    afile->size = FILE_INODE(filp)->i_size;
    AFS_GLOCK();
    afile->offset = 0;
    afile->proc = (int (*)())0;
    afile->inum = ainode;	/* for hint validity checking */
    return (void *)afile;
}
#else
void *
osi_UFSOpen(afs_int32 ainode)
{
    register struct osi_file *afile = NULL;
    extern int cacheDiskType;
    afs_int32 code = 0;
    struct inode *tip = NULL;
    struct file *filp = NULL;
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
    afile = (struct osi_file *)osi_AllocLargeSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    if (!afile) {
	osi_Panic("osi_UFSOpen: Failed to allocate %d bytes for osi_file.\n",
		  sizeof(struct osi_file));
    }
    memset(afile, 0, sizeof(struct osi_file));
    filp = &afile->file;
    filp->f_dentry = &afile->dentry;
    tip = iget(afs_cacheSBp, (u_long) ainode);
    if (!tip)
	osi_Panic("Can't get inode %d\n", ainode);
    FILE_INODE(filp) = tip;
    tip->i_flags |= MS_NOATIME;	/* Disable updating access times. */
    filp->f_flags = O_RDWR;
#if defined(AFS_LINUX24_ENV)
    filp->f_mode = FMODE_READ|FMODE_WRITE;
    filp->f_op = fops_get(tip->i_fop);
#else
    filp->f_op = tip->i_op->default_file_ops;
#endif
    if (filp->f_op && filp->f_op->open)
	code = filp->f_op->open(tip, filp);
    if (code)
	osi_Panic("Can't open inode %d\n", ainode);
    afile->size = tip->i_size;
    AFS_GLOCK();
    afile->offset = 0;
    afile->proc = (int (*)())0;
    afile->inum = ainode;	/* for hint validity checking */
    return (void *)afile;
}
#endif

int
afs_osi_Stat(register struct osi_file *afile, register struct osi_stat *astat)
{
    register afs_int32 code;
    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi, 320);
    astat->size = OSIFILE_INODE(afile)->i_size;
#ifdef STRUCT_INODE_HAS_I_BLKSIZE
    astat->blksize = OSIFILE_INODE(afile)->i_blksize;
#endif
#if defined(AFS_LINUX26_ENV)
    astat->mtime = OSIFILE_INODE(afile)->i_mtime.tv_sec;
    astat->atime = OSIFILE_INODE(afile)->i_atime.tv_sec;
#else
    astat->mtime = OSIFILE_INODE(afile)->i_mtime;
    astat->atime = OSIFILE_INODE(afile)->i_atime;
#endif
    code = 0;
    MReleaseWriteLock(&afs_xosi);
    return code;
}

#ifdef AFS_LINUX26_ENV
int
osi_UFSClose(register struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);
    if (afile) {
	if (OSIFILE_INODE(afile)) {
	    filp_close(afile->filp, NULL);
	}
    }

    osi_FreeLargeSpace(afile);
    return 0;
}
#else
int
osi_UFSClose(register struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);
    if (afile) {
	if (FILE_INODE(&afile->file)) {
	    struct file *filp = &afile->file;
	    if (filp->f_op && filp->f_op->release)
		filp->f_op->release(FILE_INODE(filp), filp);
	    iput(FILE_INODE(filp));
	}
    }

    osi_FreeLargeSpace(afile);
    return 0;
}
#endif

int
osi_UFSTruncate(register struct osi_file *afile, afs_int32 asize)
{
    register afs_int32 code;
    struct osi_stat tstat;
    struct iattr newattrs;
    struct inode *inode = OSIFILE_INODE(afile);
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
#ifdef STRUCT_INODE_HAS_I_ALLOC_SEM
    down_write(&inode->i_alloc_sem);
#endif
#ifdef STRUCT_INODE_HAS_I_MUTEX
    mutex_lock(&inode->i_mutex);
#else
    down(&inode->i_sem);
#endif
    newattrs.ia_size = asize;
    newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;
#if defined(AFS_LINUX24_ENV)
    newattrs.ia_ctime = CURRENT_TIME;

    /* avoid notify_change() since it wants to update dentry->d_parent */
    lock_kernel();
    code = inode_change_ok(inode, &newattrs);
    if (!code)
#ifdef INODE_SETATTR_NOT_VOID
	code = inode_setattr(inode, &newattrs);
#else
	inode_setattr(inode, &newattrs);
#endif
    unlock_kernel();
    if (!code)
	truncate_inode_pages(&inode->i_data, asize);
#else
    inode->i_size = asize;
    if (inode->i_sb->s_op && inode->i_sb->s_op->notify_change) {
	code = inode->i_sb->s_op->notify_change(&afile->dentry, &newattrs);
    }
    if (!code) {
	truncate_inode_pages(inode, asize);
	if (inode->i_op && inode->i_op->truncate)
	    inode->i_op->truncate(inode);
    }
#endif
    code = -code;
#ifdef STRUCT_INODE_HAS_I_MUTEX
    mutex_unlock(&inode->i_mutex);
#else
    up(&inode->i_sem);
#endif
#ifdef STRUCT_INODE_HAS_I_ALLOC_SEM
    up_write(&inode->i_alloc_sem);
#endif
    AFS_GLOCK();
    MReleaseWriteLock(&afs_xosi);
    return code;
}


/* Generic read interface */
int
afs_osi_Read(register struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
    struct uio auio;
    struct iovec iov;
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
    setup_uio(&auio, &iov, aptr, afile->offset, asize, UIO_READ, AFS_UIOSYS);
    AFS_GUNLOCK();
    code = osi_rdwr(afile, &auio, UIO_READ);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - auio.uio_resid;
	afile->offset += code;
    } else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, auio.uio_resid,
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
    struct uio auio;
    struct iovec iov;
    afs_int32 code;

    AFS_STATCNT(osi_Write);

    if (!afile) {
	if (!afs_shuttingdown)
	    osi_Panic("afs_osi_Write called with null param");
	else
	    return EIO;
    }

    if (offset != -1)
	afile->offset = offset;
    setup_uio(&auio, &iov, aptr, afile->offset, asize, UIO_WRITE, AFS_UIOSYS);
    AFS_GUNLOCK();
    code = osi_rdwr(afile, &auio, UIO_WRITE);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - auio.uio_resid;
	afile->offset += code;
    } else {
	if (code == ENOSPC)
	    afs_warnuser
		("\n\n\n*** Cache partition is FULL - Decrease cachesize!!! ***\n\n");
	code = -1;
    }

    if (afile->proc)
	(*afile->proc)(afile, code);

    return code;
}


/*  This work should be handled by physstrat in ca/machdep.c.
    This routine written from the RT NFS port strategy routine.
    It has been generalized a bit, but should still be pretty clear. */
int
afs_osi_MapStrategy(int (*aproc) (struct buf * bp), register struct buf *bp)
{
    afs_int32 returnCode;

    AFS_STATCNT(osi_MapStrategy);
    returnCode = (*aproc) (bp);

    return returnCode;
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
