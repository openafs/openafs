/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"  /* afs statistics */
#include "../h/smp_lock.h"


int afs_osicred_initialized=0;
struct  AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct super_block *afs_cacheSBp;

void *osi_UFSOpen(ainode)
    afs_int32 ainode;
{
    struct inode *ip;
    register struct osi_file *afile = NULL;
    extern int cacheDiskType;
    afs_int32 code = 0;
    int dummy;
    struct inode *tip = NULL;
    struct file *filp = NULL;
    AFS_STATCNT(osi_UFSOpen);
    if(cacheDiskType != AFS_FCACHE_TYPE_UFS) {
	osi_Panic("UFSOpen called for non-UFS cache\n");
    }
    if (!afs_osicred_initialized) {
	/* valid for alpha_osf, SunOS, Ultrix */
	bzero((char *)&afs_osi_cred, sizeof(struct AFS_UCRED));
	crhold(&afs_osi_cred);	/* don't let it evaporate, since it is static */
	afs_osicred_initialized = 1;
    }
    afile = (struct osi_file *) osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    if (!afile) {
	osi_Panic("osi_UFSOpen: Failed to allocate %d bytes for osi_file.\n",
		   sizeof(struct osi_file));
    }
    memset(afile, 0, sizeof(struct osi_file));
    filp = &afile->file;
    filp->f_dentry = &afile->dentry;
    tip = iget(afs_cacheSBp, (u_long)ainode);
    if (!tip)
	osi_Panic("Can't get inode %d\n", ainode);
    FILE_INODE(filp) = tip;
    tip->i_flags |= MS_NOATIME; /* Disable updating access times. */
    filp->f_flags = O_RDWR;
#if defined(AFS_LINUX24_ENV)
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
    afile->proc = (int (*)()) 0;
    afile->inum = ainode;        /* for hint validity checking */
    return (void *)afile;
}

afs_osi_Stat(afile, astat)
    register struct osi_file *afile;
    register struct osi_stat *astat; {
    register afs_int32 code;
    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi,320);
    astat->size = FILE_INODE(&afile->file)->i_size;
    astat->blksize = FILE_INODE(&afile->file)->i_blksize;
    astat->mtime = FILE_INODE(&afile->file)->i_mtime;
    astat->atime = FILE_INODE(&afile->file)->i_atime;
    code = 0;
    MReleaseWriteLock(&afs_xosi);
    return code;
}

osi_UFSClose(afile)
     register struct osi_file *afile;
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
      
      osi_FreeSmallSpace(afile);
      return 0;
  }

osi_UFSTruncate(afile, asize)
    register struct osi_file *afile;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    register afs_int32 code;
    struct osi_stat tstat;
    struct iattr newattrs;
    struct inode *inode = FILE_INODE(&afile->file);
    AFS_STATCNT(osi_Truncate);

    /* This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize) return code;
    MObtainWriteLock(&afs_xosi,321);    
    AFS_GUNLOCK();
    down(&inode->i_sem);
    inode->i_size = newattrs.ia_size = asize;
    newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;
#if defined(AFS_LINUX24_ENV)
    newattrs.ia_ctime = CURRENT_TIME;

    /* avoid notify_change() since it wants to update dentry->d_parent */
    lock_kernel();
    code = inode_change_ok(inode, &newattrs);
    if (!code)
	inode_setattr(inode, &newattrs);
    unlock_kernel();
    if (!code)
	truncate_inode_pages(&inode->i_data, asize);
#else
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
    up(&inode->i_sem);
    AFS_GLOCK();
    MReleaseWriteLock(&afs_xosi);
    return code;
}


/* Generic read interface */
afs_osi_Read(afile, offset, aptr, asize)
    register struct osi_file *afile;
    int offset;
    char *aptr;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    size_t resid;
    register afs_int32 code;
    register afs_int32 cnt1=0;
    AFS_STATCNT(osi_Read);

    /**
      * If the osi_file passed in is NULL, panic only if AFS is not shutting
      * down. No point in crashing when we are already shutting down
      */
    if ( !afile ) {
	if ( !afs_shuttingdown )
	    osi_Panic("osi_Read called with null param");
	else
	    return EIO;
    }

    if (offset != -1) afile->offset = offset;
    AFS_GUNLOCK();
    code = osi_rdwr(UIO_READ, afile, (caddr_t) aptr, asize, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    }
    else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
		 ICL_TYPE_INT32, code);
	code = -1;
    }
    return code;
}

/* Generic write interface */
afs_osi_Write(afile, offset, aptr, asize)
    register struct osi_file *afile;
    char *aptr;
    afs_int32 offset;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    size_t resid;
    register afs_int32 code;
    AFS_STATCNT(osi_Write);
    if ( !afile )
        osi_Panic("afs_osi_Write called with null param");
    if (offset != -1) afile->offset = offset;
    AFS_GUNLOCK();
    code = osi_rdwr(UIO_WRITE, afile, (caddr_t)aptr, asize, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    }
    else {
	if (code == ENOSPC) afs_warnuser("\n\n\n*** Cache partition is FULL - Decrease cachesize!!! ***\n\n");
	code = -1;
    }
    if (afile->proc) {
	(*afile->proc)(afile, code);
    }
    return code;
}


/*  This work should be handled by physstrat in ca/machdep.c.
    This routine written from the RT NFS port strategy routine.
    It has been generalized a bit, but should still be pretty clear. */
int afs_osi_MapStrategy(aproc, bp)
    int (*aproc)();
    register struct buf *bp;
{
    afs_int32 returnCode;

    AFS_STATCNT(osi_MapStrategy);
    returnCode = (*aproc) (bp);

    return returnCode;
}

void
shutdown_osifile()
{
  extern int afs_cold_shutdown;

  AFS_STATCNT(shutdown_osifile);
  if (afs_cold_shutdown) {
    afs_osicred_initialized = 0;
  }
}

