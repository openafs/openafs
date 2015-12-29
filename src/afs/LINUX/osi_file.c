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


#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include <linux/namei.h>

#if defined(HAVE_LINUX_EXPORTFS_H)
#include <linux/exportfs.h>
#endif
#include "osi_compat.h"

int cache_fh_type = -1;
int cache_fh_len = -1;

afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct vfsmount *afs_cacheMnt;
extern struct super_block *afs_cacheSBp;
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
extern struct cred *cache_creds;
#endif

/* Old export ops: decode_fh will call back here. Accept any dentry it suggests */
int
afs_fh_acceptable(void *context, struct dentry *dp)
{
    return 1;
}

struct file *
afs_linux_raw_open(afs_dcache_id_t *ainode)
{
    struct inode *tip = NULL;
    struct dentry *dp = NULL;
    struct file* filp;

    dp = afs_get_dentry_from_fh(afs_cacheSBp, ainode, cache_fh_len, cache_fh_type,
		afs_fh_acceptable);
    if ((!dp) || IS_ERR(dp))
           osi_Panic("Can't get dentry\n");
    tip = dp->d_inode;
    tip->i_flags |= S_NOATIME;	/* Disable updating access times. */

#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    /* Use stashed credentials - prevent selinux/apparmor problems  */
    filp = afs_dentry_open(dp, afs_cacheMnt, O_RDWR, cache_creds);
    if (IS_ERR(filp))
	filp = afs_dentry_open(dp, afs_cacheMnt, O_RDWR, current_cred());
#else
    filp = dentry_open(dget(dp), mntget(afs_cacheMnt), O_RDWR);
#endif
    if (IS_ERR(filp))
	osi_Panic("Can't open file: %d\n", (int) PTR_ERR(filp));

    dput(dp);

    return filp;
}

void *
osi_UFSOpen(afs_dcache_id_t *ainode)
{
    struct osi_file *afile = NULL;
    extern int cacheDiskType;

    AFS_STATCNT(osi_UFSOpen);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS) {
	osi_Panic("UFSOpen called for non-UFS cache\n");
    }
    if (!afs_osicred_initialized) {
	/* valid for alpha_osf, SunOS, Ultrix */
	memset(&afs_osi_cred, 0, sizeof(afs_ucred_t));
	crhold(&afs_osi_cred);	/* don't let it evaporate, since it is static */
	afs_osicred_initialized = 1;
    }
    AFS_GUNLOCK();
    afile = kmalloc(sizeof(struct osi_file), GFP_NOFS);
    if (!afile) {
	osi_Panic("osi_UFSOpen: Failed to allocate %d bytes for osi_file.\n",
		  (int)sizeof(struct osi_file));
    }
    memset(afile, 0, sizeof(struct osi_file));

    afile->filp = afs_linux_raw_open(ainode);
    afile->size = i_size_read(FILE_INODE(afile->filp));
    AFS_GLOCK();
    afile->offset = 0;
    afile->proc = (int (*)())0;
    return (void *)afile;
}

/*
 * Given a dentry, return the file handle as encoded by the filesystem.
 * We can't assume anything about the length (words, not bytes).
 * The cache has to live on a single filesystem with uniform file 
 * handles, otherwise we panic.
 */
void osi_get_fh(struct dentry *dp, afs_ufs_dcache_id_t *ainode) {
    int max_len;
    int type;

    if (cache_fh_len > 0)
	max_len = cache_fh_len;
    else
	max_len = MAX_FH_LEN;
    type = afs_get_fh_from_dentry(dp, ainode, &max_len);
    if (type == 255) {
	osi_Panic("File handle encoding failed\n");
    }
    if (cache_fh_type < 0)
	cache_fh_type = type;
    if (cache_fh_len < 0) {
	cache_fh_len = max_len;
    }
    if (type != cache_fh_type || max_len != cache_fh_len) {
	osi_Panic("Inconsistent file handles within cache\n");
    }
}

int
afs_osi_Stat(struct osi_file *afile, struct osi_stat *astat)
{
    AFS_STATCNT(osi_Stat);
    ObtainWriteLock(&afs_xosi, 320);
    astat->size = i_size_read(OSIFILE_INODE(afile));
    astat->mtime = OSIFILE_INODE(afile)->i_mtime.tv_sec;
    astat->atime = OSIFILE_INODE(afile)->i_atime.tv_sec;

    ReleaseWriteLock(&afs_xosi);
    return 0;
}

int
osi_UFSClose(struct osi_file *afile)
{
    AFS_STATCNT(osi_Close);
    if (afile) {
	if (OSIFILE_INODE(afile)) {
	    filp_close(afile->filp, NULL);
	}
    }
    kfree(afile);
    return 0;
}

int
osi_UFSTruncate(struct osi_file *afile, afs_int32 asize)
{
    afs_int32 code;
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
    ObtainWriteLock(&afs_xosi, 321);
    AFS_GUNLOCK();
    afs_linux_lock_inode(inode);
#ifdef STRUCT_INODE_HAS_I_ALLOC_SEM
    down_write(&inode->i_alloc_sem);
#endif
    newattrs.ia_size = asize;
    newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;
    newattrs.ia_ctime = CURRENT_TIME;

    /* avoid notify_change() since it wants to update dentry->d_parent */
    code = inode_change_ok(inode, &newattrs);
    if (!code)
	code = afs_inode_setattr(afile, &newattrs);
    if (!code)
	truncate_inode_pages(&inode->i_data, asize);
    code = -code;
#ifdef STRUCT_INODE_HAS_I_ALLOC_SEM
    up_write(&inode->i_alloc_sem);
#endif
    afs_linux_unlock_inode(inode);
    AFS_GLOCK();
    ReleaseWriteLock(&afs_xosi);
    return code;
}


/* Generic read interface */
int
afs_osi_Read(struct osi_file *afile, int offset, void *aptr,
	     afs_int32 asize)
{
    struct uio auio;
    struct iovec iov;
    afs_int32 code;

    memset(&auio, 0, sizeof(auio));
    memset(&iov, 0, sizeof(iov));

    AFS_STATCNT(osi_Read);

    /*
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
    struct uio auio;
    struct iovec iov;
    afs_int32 code;

    memset(&auio, 0, sizeof(auio));
    memset(&iov, 0, sizeof(iov));

    AFS_STATCNT(osi_Write);

    if (!afile) {
	if (afs_shuttingdown == AFS_RUNNING)
	    osi_Panic("afs_osi_Write called with null param");
	else
	    return -EIO;
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
	if (code > 0) {
	    code = -code;
	}
    }

    if (afile->proc)
	(*afile->proc)(afile, code);

    return code;
}


/*  This work should be handled by physstrat in ca/machdep.c.
    This routine written from the RT NFS port strategy routine.
    It has been generalized a bit, but should still be pretty clear. */
int
afs_osi_MapStrategy(int (*aproc) (struct buf * bp), struct buf *bp)
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

/* Intialize cache device info and fragment size for disk cache partition. */
int
osi_InitCacheInfo(char *aname)
{
    int code;
    extern afs_dcache_id_t cacheInode;
    struct dentry *dp;
    extern struct osi_dev cacheDev;
    extern afs_int32 afs_fsfragsize;
    extern struct super_block *afs_cacheSBp;
    extern struct vfsmount *afs_cacheMnt;
    code = osi_lookupname_internal(aname, 1, &afs_cacheMnt, &dp);
    if (code)
	return ENOENT;

    osi_get_fh(dp, &cacheInode.ufs);
    cacheDev.dev = dp->d_inode->i_sb->s_dev;
    afs_fsfragsize = dp->d_inode->i_sb->s_blocksize - 1;
    afs_cacheSBp = dp->d_inode->i_sb;

    dput(dp);

    afs_init_sb_export_ops(afs_cacheSBp);

    return 0;
}


/* osi_rdwr
 * seek, then read or write to an open inode. addrp points to data in
 * kernel space.
 */
int
osi_rdwr(struct osi_file *osifile, struct uio *uiop, int rw)
{
    struct file *filp = osifile->filp;
    mm_segment_t old_fs = {0};
    int code = 0;
    struct iovec *iov;
    size_t count;
    unsigned long savelim;
    loff_t pos;

    savelim = current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur;
    current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;

    if (uiop->uio_seg == AFS_UIOSYS) {
	/* Switch into user space */
	old_fs = get_fs();
	set_fs(get_ds());
    }

    while (code == 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;
	if (count == 0) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}

	pos = uiop->uio_offset;
	if (rw == UIO_READ)
	    code = afs_file_read(filp, iov->iov_base, count, &pos);
	else
	    code = afs_file_write(filp, iov->iov_base, count, &pos);

	if (code < 0) {
	    code = -code;
	    break;
	} else if (code == 0) {
	    /*
	     * This is bad -- we can't read any more data from the
	     * file, but we have no good way of signaling a partial
	     * read either.
	     */
	    code = EIO;
	    break;
	}

	iov->iov_base += code;
	iov->iov_len -= code;
	uiop->uio_resid -= code;
	uiop->uio_offset += code;
	code = 0;
    }

    if (uiop->uio_seg == AFS_UIOSYS) {
	/* Switch back into kernel space */
	set_fs(old_fs);
    }

    current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur = savelim;

    return code;
}

/* setup_uio 
 * Setup a uio struct.
 */
void
setup_uio(struct uio *uiop, struct iovec *iovecp, const char *buf, afs_offs_t pos,
	  int count, uio_flag_t flag, uio_seg_t seg)
{
    iovecp->iov_base = (char *)buf;
    iovecp->iov_len = count;
    uiop->uio_iov = iovecp;
    uiop->uio_iovcnt = 1;
    uiop->uio_offset = pos;
    uiop->uio_seg = seg;
    uiop->uio_resid = count;
    uiop->uio_flag = flag;
}


/* uiomove
 * UIO_READ : dp -> uio
 * UIO_WRITE : uio -> dp
 */
int
uiomove(char *dp, int length, uio_flag_t rw, struct uio *uiop)
{
    int count;
    struct iovec *iov;
    int code;

    while (length > 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;

	if (!count) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}

	if (count > length)
	    count = length;

	switch (uiop->uio_seg) {
	case AFS_UIOSYS:
	    switch (rw) {
	    case UIO_READ:
		memcpy(iov->iov_base, dp, count);
		break;
	    case UIO_WRITE:
		memcpy(dp, iov->iov_base, count);
		break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	case AFS_UIOUSER:
	    switch (rw) {
	    case UIO_READ:
		AFS_COPYOUT(dp, iov->iov_base, count, code);
		break;
	    case UIO_WRITE:
		AFS_COPYIN(iov->iov_base, dp, count, code);
		break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	default:
	    printf("uiomove: Bad seg = %d\n", uiop->uio_seg);
	    return -EINVAL;
	}

	dp += count;
	length -= count;
	iov->iov_base += count;
	iov->iov_len -= count;
	uiop->uio_offset += count;
	uiop->uio_resid -= count;
    }
    return 0;
}

