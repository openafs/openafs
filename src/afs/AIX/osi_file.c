/* Copyright (C) 1995, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"  /* afs statistics */


int afs_osicred_initialized=0;
struct  AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct vfs *afs_cacheVfsp;


void *osi_UFSOpen(ainode)
    afs_int32 ainode;
{
    struct inode *ip;
    register struct osi_file *afile = NULL;
    extern struct vfs *rootvfs;
    struct vnode *vp = (struct vnode *)0;
    extern int cacheDiskType;
    afs_int32 code = 0;
    int dummy;
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
    setuerror(0);
    AFS_GUNLOCK();
    ip = (struct inode *) igetinode((dev_t) cacheDev.dev, rootvfs, (ino_t)ainode, &vp,&dummy);
    AFS_GLOCK();
    if (getuerror()) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UFSOpen: igetinode failed");
    }
    afile->vnode = vp;		    /* Save the vnode pointer for the inode ip; also ip is already prele'ed in igetinode */
    afile->size = VTOI(afile->vnode)->i_size;
    afile->offset = 0;
    afile->proc = (int (*)()) 0;
    afile->inum = ainode;        /* for hint validity checking */
    return (void *)afile;
}

afs_osi_Stat(afile, astat)
    register struct osi_file *afile;
    register struct osi_stat *astat; {
    register afs_int32 code;
    struct vattr tvattr;
    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi,320);
    AFS_GUNLOCK();
    code = VNOP_GETATTR(afile->vnode, &tvattr, &afs_osi_cred);
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

osi_UFSClose(afile)
     register struct osi_file *afile;
  {
      AFS_STATCNT(osi_Close);
      if(afile->vnode) {
	/* AIX writes entire data regions at a time when dumping core. We've
	 * seen a 26M write go through the system. When this happens, we run
	 * out of available pages. So, we'll flush the vnode's vm if we're short
	 * on space.
	 */
	if (vmPageHog) {
	    int code;
	    if (afile->vnode->v_gnode->gn_seg) {
		/* 524287 is the max number of pages for a file. See test in
		 * vm_writep.
		 */
		code = vm_writep(afile->vnode->v_gnode->gn_seg, 0, 524287);
	    }
	}
	AFS_RELE(afile->vnode);
      }
      
      osi_FreeSmallSpace(afile);
      return 0;
  }

osi_UFSTruncate(afile, asize)
    register struct osi_file *afile;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    struct vattr tvattr;
    register afs_int32 code;
    struct osi_stat tstat;
    afs_int32 mode=FWRITE|FSYNC;
    AFS_STATCNT(osi_Truncate);

    /* This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize) return code;
    MObtainWriteLock(&afs_xosi,321);    
    /* 
     * If we're truncating an unopened file to a non-zero length,
     * we need to bind it to a vm segment    
     * Note that  that the binding will actually happen inside
     * jfs by xix_ftrunc; setting mode to 0 will enable that.
     */
    if (asize && !VTOGP(afile->vnode)->gn_seg) 
	mode = 0;
    AFS_GUNLOCK();
    code = VNOP_FTRUNC(afile->vnode, mode, asize, (caddr_t)0, &afs_osi_cred);
    AFS_GLOCK();
    MReleaseWriteLock(&afs_xosi);
    return code;
}

void osi_DisableAtimes(avp)
struct vnode *avp;
{
   struct inode *ip = VTOIP(avp);
   ip->i_flag &= ~IACC;
}


/* Generic read interface */
afs_osi_Read(afile, offset, aptr, asize)
    register struct osi_file *afile;
    int offset;
    char *aptr;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    unsigned int resid;
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
retry_IO:
    /* Note the difference in the way the afile->offset is passed (see comments in gop_rdwr() in afs_aix_subr.c for comments) */
    AFS_GUNLOCK();
    code = gop_rdwr(UIO_READ, afile->vnode, (caddr_t) aptr, asize,
		    (off_t)&afile->offset, AFS_UIOSYS, NULL, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	osi_DisableAtimes(afile->vnode);
    }
    else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
		 ICL_TYPE_INT32, code);
	/*
	 * To handle periodic low-level EFAULT failures that we've seen with the
	 * Weitek chip; in all observed failed cases a second read succeeded.
	 */
	if ((code == EFAULT) && (cnt1++ < 5)) {
	    afs_stats_cmperf.osiread_efaults++;
	    goto retry_IO;
	}
        setuerror(code);
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
    unsigned int resid;
    register afs_int32 code;
    AFS_STATCNT(osi_Write);
    if ( !afile )
        osi_Panic("afs_osi_Write called with null param");
    if (offset != -1) afile->offset = offset;
    /* Note the difference in the way the afile->offset is passed (see comments in gop_rdwr() in afs_aix_subr.c for comments) */
    AFS_GUNLOCK();
    code = gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize,
		    (off_t)&afile->offset, AFS_UIOSYS, NULL, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    }
    else {
	if (code == ENOSPC) afs_warnuser("\n\n\n*** Cache partition is FULL - Decrease cachesize!!! ***\n\n");
	setuerror(code);
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

