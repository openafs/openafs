/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * vnodeops structure and Digital Unix specific ops and support routines.
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DUX/Attic/osi_vnodeops.c,v 1.11.2.1 2005/01/31 03:49:11 shadow Exp $");


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include <vm/vm_mmap.h>
#include <vm/vm_ubc.h>
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"


extern int afs_lookup(), afs_create(), afs_noop(), afs_open(), afs_close();
extern int afs_access(), afs_getattr(), afs_setattr(), afs_badop();
extern int afs_fsync(), afs_seek(), afs_remove(), afs_link(), afs_rename();
extern int afs_mkdir(), afs_rmdir(), afs_symlink(), afs_readdir();
extern int afs_readlink(), afs_lockctl();
extern int vn_pathconf_default(), seltrue();

int mp_afs_lookup(), mp_afs_create(), mp_afs_open();
int mp_afs_access(), mp_afs_getattr(), mp_afs_setattr(), mp_afs_ubcrdwr();
int mp_afs_ubcrdwr(), mp_afs_mmap();
int mp_afs_fsync(), mp_afs_seek(), mp_afs_remove(), mp_afs_link();
int mp_afs_rename(), mp_afs_mkdir(), mp_afs_rmdir(), mp_afs_symlink();
int mp_afs_readdir(), mp_afs_readlink(), mp_afs_abortop(), mp_afs_inactive();
int mp_afs_reclaim(), mp_afs_bmap(), mp_afs_strategy(), mp_afs_print();
int mp_afs_page_read(), mp_afs_page_write(), mp_afs_swap(), mp_afs_bread();
int mp_afs_brelse(), mp_afs_lockctl(), mp_afs_syncdata(), mp_afs_close();
int mp_afs_closex();
int mp_afs_ioctl();

/* AFS vnodeops */
struct vnodeops Afs_vnodeops = {
    mp_afs_lookup,
    mp_afs_create,
    afs_noop,			/* vn_mknod */
    mp_afs_open,
    mp_afs_close,
    mp_afs_access,
    mp_afs_getattr,
    mp_afs_setattr,
    mp_afs_ubcrdwr,
    mp_afs_ubcrdwr,
    mp_afs_ioctl,		/* vn_ioctl */
    seltrue,			/* vn_select */
    mp_afs_mmap,
    mp_afs_fsync,
    mp_afs_seek,
    mp_afs_remove,
    mp_afs_link,
    mp_afs_rename,
    mp_afs_mkdir,
    mp_afs_rmdir,
    mp_afs_symlink,
    mp_afs_readdir,
    mp_afs_readlink,
    mp_afs_abortop,
    mp_afs_inactive,
    mp_afs_reclaim,
    mp_afs_bmap,
    mp_afs_strategy,
    mp_afs_print,
    mp_afs_page_read,
    mp_afs_page_write,
    mp_afs_swap,
    mp_afs_bread,
    mp_afs_brelse,
    mp_afs_lockctl,
    mp_afs_syncdata,
    afs_noop,			/* Lock */
    afs_noop,			/* unLock */
    afs_noop,			/* get ext attrs */
    afs_noop,			/* set ext attrs */
    afs_noop,			/* del ext attrs */
    vn_pathconf_default,
};
struct vnodeops *afs_ops = &Afs_vnodeops;

/* vnode file operations, and our own */
extern int vn_read();
extern int vn_write();
extern int vn_ioctl();
extern int vn_select();
extern int afs_closex();

struct fileops afs_fileops = {
    vn_read,
    vn_write,
    vn_ioctl,
    vn_select,
    mp_afs_closex,
};

mp_afs_lookup(adp, ndp)
     struct vcache *adp;
     struct nameidata *ndp;
{
    int code;
    char aname[MAXNAMLEN + 1];  /* XXX */
    struct vcache **avcp = (struct vcache **)&(ndp->ni_vp);
    struct ucred *acred = ndp->ni_cred;
    int wantparent = ndp->ni_nameiop & WANTPARENT;
    int opflag = ndp->ni_nameiop & OPFLAG;
    AFS_GLOCK();
    memcpy(aname, ndp->ni_ptr, ndp->ni_namelen);
    aname[ndp->ni_namelen] = '\0';
    code = afs_lookup(adp, aname, avcp, acred, opflag, wantparent);
    AFS_GUNLOCK();
    return code;
}

mp_afs_create(ndp, attrs)
     struct nameidata *ndp;
     struct vattr *attrs;
{
    int code;
    register struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    enum vcexcl aexcl = NONEXCL;        /* XXX - create called properly */
    int amode = 0;              /* XXX - checked in higher level */
    struct vcache **avcp = (struct vcache **)&(ndp->ni_vp);
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_create(adp, aname, attrs, aexcl, 
		      amode, avcp, acred);

    AFS_GUNLOCK();
    afs_PutVCache(adp);
    return code;
}

mp_afs_open(avcp, aflags, acred)
     struct vcache **avcp;
     afs_int32 aflags;
     struct AFS_UCRED *acred;
{
    int code;
    AFS_GLOCK();
    code = afs_open(avcp, aflags, acred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_access(avc, amode, acred)
     struct vcache *avc;
     afs_int32 amode;
     struct AFS_UCRED *acred;
{
    int code;
    AFS_GLOCK();
    code = afs_access(avc, amode, acred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_close(avc, flags, cred)
     struct vnode *avc;
     int flags;
     struct ucred *cred;
{
    int code;
    AFS_GLOCK();
    code = afs_close(avc, flags, cred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_getattr(avc, attrs, acred)
     struct vcache *avc;
     struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    int code;
    AFS_GLOCK();
    code = afs_getattr(avc, attrs, acred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_setattr(avc, attrs, acred)
     struct vcache *avc;
     struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    int code;
    AFS_GLOCK();
    code = afs_setattr(avc, attrs, acred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_fsync(avc, fflags, acred, waitfor)
     struct vcache *avc;
     int fflags;
     struct AFS_UCRED *acred;
     int waitfor;
{
    int code;
    AFS_GLOCK();
    code = afs_fsync(avc, fflags, acred, waitfor);
    AFS_GUNLOCK();
    return code;
}

mp_afs_remove(ndp)
     struct nameidata *ndp;
{
    int code;
    register struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_remove(adp, aname, acred);
    afs_PutVCache(adp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_link(avc, ndp)
     struct vcache *avc;
     struct nameidata *ndp;
{
    int code;
    struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_link(avc, adp, aname, acred);
    AFS_GUNLOCK();
    afs_PutVCache(adp);
    return code;
}

mp_afs_rename(fndp, tndp)
     struct nameidata *fndp, *tndp;
{
    int code;
    struct vcache *aodp = VTOAFS(fndp->ni_dvp);
    char *aname1 = fndp->ni_dent.d_name;
    struct vcache *andp = VTOAFS(tndp->ni_dvp);
    char *aname2 = tndp->ni_dent.d_name;
    struct ucred *acred = tndp->ni_cred;
    AFS_GLOCK();
    code = afs_rename(aodp, aname1, andp, aname2, acred);
    AFS_RELE(tndp->ni_dvp);
    if (tndp->ni_vp != NULL) {
        AFS_RELE(tndp->ni_vp);
    }
    AFS_RELE(fndp->ni_dvp);
    AFS_RELE(fndp->ni_vp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_mkdir(ndp, attrs)
     struct nameidata *ndp;
     struct vattr *attrs;
{
    int code;
    register struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    register struct vcache **avcp = (struct vcache **)&(ndp->ni_vp);
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_mkdir(adp, aname, attrs, avcp, acred);
    AFS_RELE(adp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_rmdir(ndp)
     struct nameidata *ndp;
{
    int code;
    register struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_rmdir(adp, aname, acred);
    afs_PutVCache(adp);
    afs_PutVCache(ndp->ni_vp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_symlink(ndp, attrs, atargetName)
     struct nameidata *ndp;
     struct vattr *attrs;
     register char *atargetName;
{
    int code;
    register struct vcache *adp = VTOAFS(ndp->ni_dvp);
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
    AFS_GLOCK();
    code = afs_symlink(adp, aname, attrs, atargetName, acred);
    AFS_RELE(ndp->ni_dvp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_readdir(avc, auio, acred, eofp)
     struct vcache *avc;
     struct uio *auio;
     struct AFS_UCRED *acred;
     int *eofp;
{
    int code;
    AFS_GLOCK();
    code = afs_readdir(avc, auio, acred, eofp);
    AFS_GUNLOCK();
    return code;
}

mp_afs_readlink(avc, auio, acred)
     struct vcache *avc;
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    int code;
    AFS_GLOCK();
    code = afs_readlink(avc, auio, acred);
    AFS_GUNLOCK();
    return code;
}

mp_afs_lockctl(avc, af, flag, acred, clid, offset)
     struct vcache *avc;
     struct eflock *af;
     struct AFS_UCRED *acred;
     int flag;
     pid_t clid;
     off_t offset;
{
    int code;
    AFS_GLOCK();
    code = afs_lockctl(avc, af, flag, acred, clid, offset);
    AFS_GUNLOCK();
    return code;
}

mp_afs_closex(afd)
     struct file *afd;
{
    int code;
    AFS_GLOCK();
    code = afs_closex(afd);
    AFS_GUNLOCK();
    return code;
}

mp_afs_seek(avc, oldoff, newoff, cred)
     struct vcache *avc;
     off_t oldoff, newoff;
     struct ucred *cred;
{
    if ((int)newoff < 0)
	return (EINVAL);
    else
	return (0);
}

mp_afs_abortop(ndp)
     struct nameidata *ndp;
{
    return (0);
}

mp_afs_inactive(avc, acred)
     register struct vcache *avc;
     struct AFS_UCRED *acred;
{
    AFS_GLOCK();
    afs_InactiveVCache(avc, acred);
    AFS_GUNLOCK();
}


mp_afs_reclaim(avc)
     struct vcache *avc;
{
    return (0);
}

mp_afs_print(avc)
     struct vcache *avc;
{
    return (0);
}

mp_afs_page_read(avc, uio, acred)
     struct vcache *avc;
     struct uio *uio;
     struct ucred *acred;
{
    int error;
    struct vrequest treq;

    AFS_GLOCK();
    error = afs_rdwr(avc, uio, UIO_READ, 0, acred);
    afs_Trace3(afs_iclSetp, CM_TRACE_PAGE_READ, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, error, ICL_TYPE_INT32, avc->states);
    if (error) {
	error = EIO;
    } else if ((avc->states & CWired) == 0) {
	afs_InitReq(&treq, acred);
	ObtainWriteLock(&avc->lock, 161);
	afs_Wire(avc, &treq);
	ReleaseWriteLock(&avc->lock);
    }
    AFS_GUNLOCK();
    return (error);
}


mp_afs_page_write(avc, uio, acred, pager, offset)
     struct vcache *avc;
     struct uio *uio;
     struct ucred *acred;
     memory_object_t pager;
     vm_offset_t offset;
{
    int error;

    AFS_GLOCK();
    error = afs_rdwr(avc, uio, UIO_WRITE, 0, acred);
    afs_Trace3(afs_iclSetp, CM_TRACE_PAGE_WRITE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, error, ICL_TYPE_INT32, avc->states);
    if (error) {
	error = EIO;
    }
    AFS_GUNLOCK();
    return (error);
}


int DO_FLUSH = 1;
mp_afs_ubcrdwr(avc, uio, ioflag, cred)
     struct vcache *avc;
     struct uio *uio;
     int ioflag;
     struct ucred *cred;
{
    register afs_int32 code;
    register char *data;
    afs_int32 fileBase, size, cnt = 0;
    afs_int32 pageBase;
    register afs_int32 tsize;
    register afs_int32 pageOffset;
    int eof;
    struct vrequest treq;
    int rw = uio->uio_rw;
    int rv, flags;
    int newpage = 0;
    vm_page_t page;
    afs_int32 save_resid;
    struct dcache *tdc;
    int didFakeOpen = 0;
    int counter = 0;

    AFS_GLOCK();
    afs_InitReq(&treq, cred);
    if (AFS_NFSXLATORREQ(cred) && rw == UIO_READ) {
	if (!afs_AccessOK
	    (avc, PRSFS_READ, &treq,
	     CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
	    AFS_GUNLOCK();
	    return EACCES;
	}
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_VMRW, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, (rw == UIO_WRITE ? 1 : 0), ICL_TYPE_LONG,
	       uio->uio_offset, ICL_TYPE_LONG, uio->uio_resid);
    code = afs_VerifyVCache(avc, &treq);
    if (code) {
	code = afs_CheckCode(code, &treq, 35);
	AFS_GUNLOCK();
	return code;
    }
    if (vType(avc) != VREG) {
	AFS_GUNLOCK();
	return EISDIR;		/* can't read or write other things */
    }
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc, cred);	/* hold bozon lock, but not basic vnode lock */
    ObtainWriteLock(&avc->lock, 162);
    /* adjust parameters when appending files */
    if ((ioflag & IO_APPEND) && uio->uio_rw == UIO_WRITE)
	uio->uio_offset = avc->m.Length;	/* write at EOF position */
    if (uio->uio_rw == UIO_WRITE) {
	avc->states |= CDirty;
	afs_FakeOpen(avc);
	didFakeOpen = 1;
	/*
	 * before starting any I/O, we must ensure that the file is big enough
	 * to hold the results (since afs_putpage will be called to force
	 * the I/O.
	 */
	size = uio->afsio_resid + uio->afsio_offset;	/* new file size */
	if (size > avc->m.Length)
	    avc->m.Length = size;	/* file grew */
	avc->m.Date = osi_Time();	/* Set file date (for ranlib) */
	if (uio->afsio_resid > PAGE_SIZE)
	    cnt = uio->afsio_resid / PAGE_SIZE;
	save_resid = uio->afsio_resid;
    }

    while (1) {
	/*
	 * compute the amount of data to move into this block,
	 * based on uio->afsio_resid.
	 */
	size = uio->afsio_resid;	/* transfer size */
	fileBase = uio->afsio_offset;	/* start file position */
	pageBase = fileBase & ~(PAGE_SIZE - 1);	/* file position of the page */
	pageOffset = fileBase & (PAGE_SIZE - 1);	/* start offset within page */
	tsize = PAGE_SIZE - pageOffset;	/* amount left in this page */
	/*
	 * we'll read tsize bytes,
	 * but first must make sure tsize isn't too big
	 */
	if (tsize > size)
	    tsize = size;	/* don't read past end of request */
	eof = 0;		/* flag telling us if we hit the EOF on the read */
	if (uio->uio_rw == UIO_READ) {	/* we're doing a read operation */
	    /* don't read past EOF */
	    if (tsize + fileBase > avc->m.Length) {
		tsize = avc->m.Length - fileBase;
		eof = 1;	/* we did hit the EOF */
		if (tsize < 0)
		    tsize = 0;	/* better safe than sorry */
	    }
	}
	if (tsize <= 0)
	    break;		/* nothing to transfer, we're done */

	/* Purge dirty chunks of file if there are too many dirty chunks.
	 * Inside the write loop, we only do this at a chunk boundary.
	 * Clean up partial chunk if necessary at end of loop.
	 */
	if (uio->uio_rw == UIO_WRITE && counter > 0
	    && AFS_CHUNKOFFSET(fileBase) == 0) {
	    code = afs_DoPartialWrite(avc, &treq);
	    avc->states |= CDirty;
	}

	if (code) {
	    break;
	}

	flags = 0;
	ReleaseWriteLock(&avc->lock);
	AFS_GUNLOCK();
#ifdef AFS_DUX50_ENV
	code =
	    ubc_lookup(AFSTOV(avc)->v_object, pageBase, PAGE_SIZE, PAGE_SIZE,
		       &page, &flags, NULL);
#else
	code =
	    ubc_lookup(AFSTOV(avc)->v_object, pageBase, PAGE_SIZE, PAGE_SIZE,
		       &page, &flags);
#endif
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 163);

	if (code) {
	    break;
	}
	if (flags & B_NOCACHE) {
	    /*
	     * No page found. We should not read the page in if
	     * 1. the write starts on a page edge (ie, pageoffset == 0)
	     * and either
	     * 1. we will fill the page  (ie, size == PAGESIZE), or
	     * 2. we are writing past eof
	     */
	    if ((uio->uio_rw == UIO_WRITE)
		&&
		((pageOffset == 0
		  && (size == PAGE_SIZE || fileBase >= avc->m.Length)))) {
		struct vnode *vp = AFSTOV(avc);
		/* we're doing a write operation past eof; no need to read it */
		newpage = 1;
		AFS_GUNLOCK();
		ubc_page_zero(page, 0, PAGE_SIZE);
		ubc_page_release(page, B_DONE);
		AFS_GLOCK();
	    } else {
		/* page wasn't cached, read it in. */
		struct buf *bp;

		AFS_GUNLOCK();
		bp = ubc_bufalloc(page, 1, PAGE_SIZE, 1, B_READ);
		AFS_GLOCK();
		bp->b_dev = 0;
		bp->b_vp = AFSTOV(avc);
		bp->b_blkno = btodb(pageBase);
		ReleaseWriteLock(&avc->lock);
		code = afs_ustrategy(bp, cred);	/* do the I/O */
		ObtainWriteLock(&avc->lock, 164);
		AFS_GUNLOCK();
		ubc_sync_iodone(bp);
		AFS_GLOCK();
		if (code) {
		    AFS_GUNLOCK();
		    ubc_page_release(page, 0);
		    AFS_GLOCK();
		    break;
		}
	    }
	}
	AFS_GUNLOCK();
	ubc_page_wait(page);
	data = ubc_load(page, pageOffset, page_size);
	AFS_GLOCK();
	ReleaseWriteLock(&avc->lock);	/* uiomove may page fault */
	AFS_GUNLOCK();
	code = uiomove(data + pageOffset, tsize, uio);
	ubc_unload(page, pageOffset, page_size);
	if (uio->uio_rw == UIO_WRITE) {
	    vm_offset_t toffset;

	    /* Mark the page dirty and release it to avoid a deadlock
	     * in ubc_dirty_kluster when more than one process writes
	     * this page at the same time. */
	    toffset = page->pg_offset;
	    flags |= B_DIRTY;
	    ubc_page_release(page, flags);

	    if (cnt > 10) {
		vm_page_t pl;
		int kpcnt;
		struct buf *bp;

		/* We released the page, so we can get a null page
		 * list if another thread calls the strategy routine.
		 */
		pl = ubc_dirty_kluster(AFSTOV(avc)->v_object, NULL, toffset,
				       0, B_WANTED, FALSE, &kpcnt);
		if (pl) {
		    bp = ubc_bufalloc(pl, 1, PAGE_SIZE, 1, B_WRITE);
		    bp->b_dev = 0;
		    bp->b_vp = AFSTOV(avc);
		    bp->b_blkno = btodb(pageBase);
		    AFS_GLOCK();
		    code = afs_ustrategy(bp, cred);	/* do the I/O */
		    AFS_GUNLOCK();
		    ubc_sync_iodone(bp);
		    if (code) {
			AFS_GLOCK();
			ObtainWriteLock(&avc->lock, 415);
			break;
		    }
		}
	    }
	} else {
	    ubc_page_release(page, flags);
	}
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 165);
	/*
	 * If reading at a chunk boundary, start prefetch of next chunk.
	 */
	if (uio->uio_rw == UIO_READ
	    && (counter == 0 || AFS_CHUNKOFFSET(fileBase) == 0)) {
	    tdc = afs_FindDCache(avc, fileBase);
	    if (tdc) {
		if (!(tdc->mflags & DFNextStarted))
		    afs_PrefetchChunk(avc, tdc, cred, &treq);
		afs_PutDCache(tdc);
	    }
	}
	counter++;
	if (code)
	    break;
    }
    if (didFakeOpen)
	afs_FakeClose(avc, cred);
    if (uio->uio_rw == UIO_WRITE && code == 0 && (avc->states & CDirty)) {
	code = afs_DoPartialWrite(avc, &treq);
    }
    ReleaseWriteLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    if (DO_FLUSH || (!newpage && (cnt < 10))) {
	AFS_GUNLOCK();
	ubc_flush_dirty(AFSTOV(avc)->v_object, flags);
	AFS_GLOCK();
    }

    ObtainSharedLock(&avc->lock, 409);
    if (!code) {
	if (avc->vc_error) {
	    code = avc->vc_error;
	}
    }
    /* This is required since we may still have dirty pages after the write.
     * I could just let close do the right thing, but stat's before the close
     * return the wrong length.
     */
    if (code == EDQUOT || code == ENOSPC) {
	uio->uio_resid = save_resid;
	UpgradeSToWLock(&avc->lock, 410);
	osi_ReleaseVM(avc, cred);
	ConvertWToSLock(&avc->lock);
    }
    ReleaseSharedLock(&avc->lock);

    if (!code && (ioflag & IO_SYNC) && (uio->uio_rw == UIO_WRITE)
	&& !AFS_NFSXLATORREQ(cred)) {
	code = afs_fsync(avc, 0, cred, 0);
    }
  out:
    code = afs_CheckCode(code, &treq, 36);
    AFS_GUNLOCK();
    return code;
}

int
mp_afs_ioctl(struct vnode *vp, int com, caddr_t data, int fflag,
	     struct ucred *cred, int *retval)
{
    return ENOSYS;
}

/*
 * Now for some bad news.  Since we artificially hold on to vnodes by doing
 * and extra VNHOLD in afs_NewVCache(), there is no way for us to know
 * when we need to flush the pages when a program exits.  Particularly
 * if it closes the file after mapping it R/W.
 *
 */

mp_afs_mmap(avc, offset, map, addrp, len, prot, maxprot, flags, cred)
     register struct vcache *avc;
     vm_offset_t offset;
     vm_map_t map;
     vm_offset_t *addrp;
     vm_size_t len;
     vm_prot_t prot;
     vm_prot_t maxprot;
     int flags;
     struct ucred *cred;
{
    struct vp_mmap_args args;
    register struct vp_mmap_args *ap = &args;
    struct vnode *vp = AFSTOV(avc);
    int code;
    struct vrequest treq;
#if	!defined(DYNEL)
    extern kern_return_t u_vp_create();
#endif

    AFS_GLOCK();
    afs_InitReq(&treq, cred);
    code = afs_VerifyVCache(avc, &treq);
    if (code) {
	code = afs_CheckCode(code, &treq, 37);
	AFS_GUNLOCK();
	return code;
    }
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc, cred);	/* ensure old pages are gone */
    afs_BozonUnlock(&avc->pvnLock, avc);
    ObtainWriteLock(&avc->lock, 166);
    avc->states |= CMAPPED;
    ReleaseWriteLock(&avc->lock);
    ap->a_offset = offset;
    ap->a_vaddr = addrp;
    ap->a_size = len;
    ap->a_prot = prot, ap->a_maxprot = maxprot;
    ap->a_flags = flags;
    AFS_GUNLOCK();
    code = u_vp_create(map, vp->v_object, (vm_offset_t) ap);
    AFS_GLOCK();
    code = afs_CheckCode(code, &treq, 38);
    AFS_GUNLOCK();
    return code;
}


int
mp_afs_getpage(vop, offset, len, protp, pl, plsz,
#ifdef AFS_DUX50_ENV
	       policy,
#else
	       mape, addr,
#endif
	       rw, cred)
     vm_ubc_object_t vop;
     vm_offset_t offset;
     vm_size_t len;
     vm_prot_t *protp;
     vm_page_t *pl;
     int plsz;
#ifdef AFS_DUX50_ENV
     struct vm_policy *policy;
#else
     vm_map_entry_t mape;
     vm_offset_t addr;
#endif
     int rw;
     struct ucred *cred;
{
    register afs_int32 code;
    struct vrequest treq;
    int flags = 0;
    int i, pages = (len + PAGE_SIZE - 1) >> page_shift;
    vm_page_t *pagep;
    vm_offset_t off;

    struct vcache *avc = VTOAFS(vop->vu_vp);

    /* first, obtain the proper lock for the VM system */

    AFS_GLOCK();
    afs_InitReq(&treq, cred);
    code = afs_VerifyVCache(avc, &treq);
    if (code) {
	*pl = VM_PAGE_NULL;
	code = afs_CheckCode(code, &treq, 39);	/* failed to get it */
	AFS_GUNLOCK();
	return code;
    }

    /* clean all dirty pages for this vnode */
    AFS_GUNLOCK();
    ubc_flush_dirty(vop, 0);
    AFS_GLOCK();

    afs_BozonLock(&avc->pvnLock, avc);
    ObtainWriteLock(&avc->lock, 167);
    afs_Trace4(afs_iclSetp, CM_TRACE_PAGEIN, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_LONG, offset, ICL_TYPE_LONG, len, ICL_TYPE_INT32,
	       (int)rw);
    for (i = 0; i < pages; i++) {
	pagep = &pl[i];
	off = offset + PAGE_SIZE * i;
	if (protp)
	    protp[i] = 0;
	flags = 0;
	ReleaseWriteLock(&avc->lock);
	AFS_GUNLOCK();
#ifdef AFS_DUX50_ENV
	code =
	    ubc_lookup(AFSTOV(avc)->v_object, off, PAGE_SIZE, PAGE_SIZE,
		       pagep, &flags, NULL);
#else
	code =
	    ubc_lookup(AFSTOV(avc)->v_object, off, PAGE_SIZE, PAGE_SIZE,
		       pagep, &flags);
#endif
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 168);
	if (code) {
	    goto out;
	}
	if (flags & B_NOCACHE) {	/* if (page) */
	    if ((rw & B_WRITE) && (offset + len >= avc->m.Length)) {
		struct vnode *vp = AFSTOV(avc);
		/* we're doing a write operation past eof; no need to read it */
		AFS_GUNLOCK();
		ubc_page_zero(*pagep, 0, PAGE_SIZE);
		ubc_page_release(*pagep, B_DONE);
		AFS_GLOCK();
	    } else {
		/* page wasn't cached, read it in. */
		struct buf *bp;

		AFS_GUNLOCK();
		bp = ubc_bufalloc(*pagep, 1, PAGE_SIZE, 1, B_READ);
		AFS_GLOCK();
		bp->b_dev = 0;
		bp->b_vp = AFSTOV(avc);
		bp->b_blkno = btodb(off);
		ReleaseWriteLock(&avc->lock);
		code = afs_ustrategy(bp, cred);	/* do the I/O */
		ObtainWriteLock(&avc->lock, 169);
		AFS_GUNLOCK();
		ubc_sync_iodone(bp);
		AFS_GLOCK();
		if (code) {
		    AFS_GUNLOCK();
		    ubc_page_release(pl[i], 0);
		    AFS_GLOCK();
		    goto out;
		}
	    }
	}
	if ((rw & B_READ) == 0) {
	    AFS_GUNLOCK();
#ifdef AFS_DUX50_ENV
	    ubc_page_dirty(pl[i], 0);
#else
	    ubc_page_dirty(pl[i]);
#endif
	    AFS_GLOCK();
	} else {
	    if (protp && (flags & B_DIRTY) == 0) {
		protp[i] = VM_PROT_WRITE;
	    }
	}
    }
  out:
    pl[i] = VM_PAGE_NULL;
    ReleaseWriteLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    afs_Trace3(afs_iclSetp, CM_TRACE_PAGEINDONE, ICL_TYPE_INT32, code,
	       ICL_TYPE_POINTER, *pagep, ICL_TYPE_INT32, flags);
    code = afs_CheckCode(code, &treq, 40);
    AFS_GUNLOCK();
    return code;
}


int
mp_afs_putpage(vop, pl, pcnt, flags, cred)
     vm_ubc_object_t vop;
     vm_page_t *pl;
     int pcnt;
     int flags;
     struct ucred *cred;
{
    register afs_int32 code = 0;
    struct vnode *vp = vop->vu_vp;
    struct vcache *avc = VTOAFS(vp);
    int i;

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_PAGEOUT, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, pcnt, ICL_TYPE_INT32, vp->v_flag,
	       ICL_TYPE_INT32, flags);
    if (flags & B_UBC) {
	AFS_GUNLOCK();
	VN_LOCK(vp);
	if (vp->v_flag & VXLOCK) {
	    VN_UNLOCK(vp);
	    for (i = 0; i < pcnt; i++) {
		ubc_page_release(pl[i], B_DONE | B_DIRTY);
		pl[i] = VM_PAGE_NULL;
	    }
	    return (0);
	} else {
	    VN_UNLOCK(vp);
	}
	AFS_GLOCK();
    }

    /* first, obtain the proper lock for the VM system */
    afs_BozonLock(&avc->pvnLock, avc);
    ObtainWriteLock(&avc->lock, 170);
    for (i = 0; i < pcnt; i++) {
	vm_page_t page = pl[i];
	struct buf *bp;

	/* write it out */
	AFS_GUNLOCK();
	bp = ubc_bufalloc(page, 1, PAGE_SIZE, 1, B_WRITE);
	AFS_GLOCK();
	bp->b_dev = 0;
	bp->b_vp = AFSTOV(avc);
	bp->b_blkno = btodb(page->pg_offset);
	ReleaseWriteLock(&avc->lock);
	code = afs_ustrategy(bp, cred);	/* do the I/O */
	ObtainWriteLock(&avc->lock, 171);
	AFS_GUNLOCK();
	ubc_sync_iodone(bp);
	AFS_GLOCK();
	if (code) {
	    goto done;
	} else {
	    pl[i] = VM_PAGE_NULL;
	}
    }
  done:
    ReleaseWriteLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    afs_Trace2(afs_iclSetp, CM_TRACE_PAGEOUTDONE, ICL_TYPE_INT32, code,
	       ICL_TYPE_INT32, avc->m.Length);
    AFS_GUNLOCK();
    return code;
}


int
mp_afs_swap(avc, swapop, argp)
     struct vcache *avc;
     vp_swap_op_t swapop;
     vm_offset_t argp;
{
    return EIO;
}

int
mp_afs_syncdata(avc, flag, offset, length, cred)
     struct vcache *avc;
     int flag;
     vm_offset_t offset;
     vm_size_t length;
     struct ucred *cred;
{
    /* NFS V3 makes this call, ignore it. We'll sync the data in afs_fsync. */
    if (AFS_NFSXLATORREQ(cred))
	return 0;
    else
	return EINVAL;
}

/* a freelist of one */
struct buf *afs_bread_freebp = 0;

/*
 *  Only rfs_read calls this, and it only looks at bp->b_un.b_addr.
 *  Thus we can use fake bufs (ie not from the real buffer pool).
 */
mp_afs_bread(vp, lbn, bpp, cred)
     struct ucred *cred;
     struct vnode *vp;
     daddr_t lbn;
     struct buf **bpp;
{
    int offset, fsbsize, error;
    struct buf *bp;
    struct iovec iov;
    struct uio uio;

    AFS_GLOCK();
    AFS_STATCNT(afs_bread);
    fsbsize = vp->v_vfsp->vfs_bsize;
    offset = lbn * fsbsize;
    if (afs_bread_freebp) {
	bp = afs_bread_freebp;
	afs_bread_freebp = 0;
    } else {
	bp = (struct buf *)AFS_KALLOC(sizeof(*bp));
	bp->b_un.b_addr = (caddr_t) AFS_KALLOC(fsbsize);
    }

    iov.iov_base = bp->b_un.b_addr;
    iov.iov_len = fsbsize;
    uio.afsio_iov = &iov;
    uio.afsio_iovcnt = 1;
    uio.afsio_seg = AFS_UIOSYS;
    uio.afsio_offset = offset;
    uio.afsio_resid = fsbsize;
    *bpp = 0;
    error = afs_read(VTOAFS(vp), &uio, cred, lbn, bpp, 0);
    if (error) {
	afs_bread_freebp = bp;
	AFS_GUNLOCK();
	return error;
    }
    if (*bpp) {
	afs_bread_freebp = bp;
    } else {
	*(struct buf **)&bp->b_vp = bp;	/* mark as fake */
	*bpp = bp;
    }
    AFS_GUNLOCK();
    return 0;
}


mp_afs_brelse(vp, bp)
     struct vnode *vp;
     struct buf *bp;
{
    AFS_GLOCK();
    AFS_STATCNT(afs_brelse);
    if ((struct buf *)bp->b_vp != bp) {	/* not fake */
	brelse(bp);
    } else if (afs_bread_freebp) {
	AFS_KFREE(bp->b_un.b_addr, vp->v_vfsp->vfs_bsize);
	AFS_KFREE(bp, sizeof(*bp));
    } else {
	afs_bread_freebp = bp;
    }
    AFS_GUNLOCK();
}


mp_afs_bmap(avc, abn, anvp, anbn)
     register struct vcache *avc;
     afs_int32 abn, *anbn;
     struct vcache **anvp;
{
    AFS_GLOCK();
    AFS_STATCNT(afs_bmap);
    if (anvp)
	*anvp = avc;
    if (anbn)
	*anbn = abn * (8192 / DEV_BSIZE);	/* in 512 byte units */
    AFS_GUNLOCK();
    return 0;
}


/* real strategy */
mp_afs_strategy(abp)
     register struct buf *abp;
{
    register afs_int32 code;

    AFS_GLOCK();
    AFS_STATCNT(afs_strategy);
    code = afs_osi_MapStrategy(afs_ustrategy, abp);
    AFS_GUNLOCK();
    return code;
}


mp_afs_refer(vm_ubc_object_t vop)
{
    VREF(vop->vu_vp);
}


mp_afs_release(vm_ubc_object_t vop)
{
    vrele(vop->vu_vp);
}


mp_afs_write_check(vm_ubc_object_t vop, vm_page_t pp)
{
    return TRUE;
}

#ifdef AFS_DUX50_ENV
int
mp_afs_objtovp(vm_ubc_object_t vop, struct vnode **vp)
{
    *vp = vop->vu_vp;
    return 0;
}

int
mp_afs_setpgstamp(vm_page_t pp, unsigned int tick)
{
    pp->pg_stamp = tick;
    return 0;
}
#endif


struct vfs_ubcops afs_ubcops = {
    mp_afs_refer,		/* refer vnode */
    mp_afs_release,		/* release vnode */
    mp_afs_getpage,		/* get page */
    mp_afs_putpage,		/* put page */
    mp_afs_write_check,		/* check writablity */
#ifdef AFS_DUX50_ENV
    mp_afs_objtovp,		/* get vnode pointer */
    mp_afs_setpgstamp		/* set page stamp */
#endif
};


/*
 * Cover function for lookup name using OSF equivalent, namei()
 *
 * Note, the result vnode (ni_vp) in the namei data structure is remains
 * locked after return.
 */
lookupname(namep, seg, follow, dvpp, cvpp)
     char *namep;		/* path name */
     int seg;			/* address space containing name */
     int follow;		/* follow symbolic links */
     struct vnode **dvpp;	/* result, containing parent vnode */
     struct vnode **cvpp;	/* result, containing final component vnode */
{
    /* Should I use free-bee in u-area? */
    struct nameidata *ndp = &u.u_nd;
    int error;

    ndp->ni_nameiop = ((follow) ? (LOOKUP | FOLLOW) : (LOOKUP));
    ndp->ni_segflg = seg;
    ndp->ni_dirp = namep;
    error = namei(ndp);
    if (dvpp != NULL)
	*dvpp = ndp->ni_dvp;
    if (cvpp != NULL)
	*cvpp = ndp->ni_vp;
    return (error);
}
