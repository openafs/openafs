/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * SGI specific vnodeops + other misc interface glue
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#ifdef	AFS_SGI62_ENV
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "sys/flock.h"
#include "afs/nfsclient.h"

/* AFSBSIZE must be at least the size of a page, else the client will hang.
 * For 64 bit platforms, the page size is more than 8K.
 */
#define AFSBSIZE	_PAGESZ
extern struct afs_exporter *root_exported;
extern void afs_chkpgoob(vnode_t *, pgno_t);

static void afs_strategy();
static int afs_xread(), afs_xwrite();
static int afs_xbmap(), afs_map(), afs_reclaim();
#ifndef AFS_SGI65_ENV
static int afs_addmap(), afs_delmap();
#endif
extern int afs_open(), afs_close(), afs_ioctl(), afs_getattr(), afs_setattr();
extern int afs_access(), afs_lookup();
extern int afs_create(), afs_remove(), afs_link(), afs_rename();
extern int afs_mkdir(), afs_rmdir(), afs_readdir();
extern int afs_symlink(), afs_readlink(), afs_fsync(), afs_fid(),
afs_frlock();
static int afs_seek(OSI_VC_DECL(a), off_t b, off_t * c);
#ifdef AFS_SGI64_ENV
extern int afs_xinactive();
#else
extern void afs_xinactive();
#endif

extern void afs_rwlock(OSI_VN_DECL(vp), AFS_RWLOCK_T b);
extern void afs_rwunlock(OSI_VN_DECL(vp), AFS_RWLOCK_T b);

extern int afs_fid2();

static int afsrwvp(register struct vcache *avc, register struct uio *uio,
		   enum uio_rw rw, int ioflag,
#ifdef AFS_SGI64_ENV
		   struct cred *cr, struct flid *flp);
#else
		   struct cred *cr);
#endif
#ifdef MP
static void mp_afs_rwlock(OSI_VN_DECL(a), AFS_RWLOCK_T b);
static void mp_afs_rwunlock(OSI_VN_DECL(a), AFS_RWLOCK_T b);
struct vnodeops afs_lockedvnodeops =
#else
struct vnodeops Afs_vnodeops =
#endif
{
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
#else
    VNODE_POSITION_BASE,
#endif
#endif
    afs_open,
    afs_close,
    afs_xread,
    afs_xwrite,
    afs_ioctl,
    fs_setfl,
    afs_getattr,
    afs_setattr,
    afs_access,
    afs_lookup,
    afs_create,
    afs_remove,
    afs_link,
    afs_rename,
    afs_mkdir,
    afs_rmdir,
    afs_readdir,
    afs_symlink,
    afs_readlink,
    afs_fsync,
    afs_xinactive,
    afs_fid,
    afs_fid2,
    afs_rwlock,
    afs_rwunlock,
    afs_seek,
    fs_cmp,
    afs_frlock,
    fs_nosys,			/* realvp */
    afs_xbmap,
    afs_strategy,
    afs_map,
#ifdef AFS_SGI65_ENV
    fs_noerr,			/* addmap - devices only */
    fs_noerr,			/* delmap - devices only */
#else
    afs_addmap,
    afs_delmap,
#endif
    fs_poll,			/* poll */
    fs_nosys,			/* dump */
    fs_pathconf,
    fs_nosys,			/* allocstore */
    fs_nosys,			/* fcntl */
    afs_reclaim,		/* reclaim */
    fs_nosys,			/* attr_get */
    fs_nosys,			/* attr_set */
    fs_nosys,			/* attr_remove */
    fs_nosys,			/* attr_list */
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    fs_cover,
    (vop_link_removed_t) fs_noval,
    fs_vnode_change,
    fs_tosspages,
    fs_flushinval_pages,
    fs_flush_pages,
    fs_invalfree_pages,
    fs_pages_sethole,
    (vop_commit_t) fs_nosys,
    (vop_readbuf_t) fs_nosys,
    fs_strgetmsg,
    fs_strputmsg,
#else
    fs_mount,
#endif
#endif
};

#ifndef MP
struct vnodeops *afs_ops = &Afs_vnodeops;
#endif

int
afs_frlock(OSI_VN_DECL(vp), int cmd, struct flock *lfp, int flag,
	   off_t offset,
#ifdef AFS_SGI65_ENV
	   vrwlock_t vrwlock,
#endif
	   cred_t * cr)
{
    int error;
    OSI_VN_CONVERT(vp);
#ifdef AFS_SGI65_ENV
    struct flid flid;
    int pid;
    get_current_flid(&flid);
    pid = flid.fl_pid;
#endif

    /*
     * Since AFS doesn't support byte-wise locks (and simply
     * says yes! we handle byte locking locally only.
     * This makes lots of things work much better
     * XXX This doesn't properly handle moving from a
     * byte-wise lock up to a full file lock (we should
     * remove the byte locks ..) Of course neither did the
     * regular AFS way ...
     *
     * For GETLK we do a bit more - we first check any byte-wise
     * locks - if none then check for full AFS file locks
     */
    if (cmd == F_GETLK || lfp->l_whence != 0 || lfp->l_start != 0
	|| (lfp->l_len != MAXEND && lfp->l_len != 0)) {
	AFS_RWLOCK(vp, VRWLOCK_WRITE);
	AFS_GUNLOCK();
#ifdef AFS_SGI65_ENV
	error =
	    fs_frlock(OSI_VN_ARG(vp), cmd, lfp, flag, offset, vrwlock, cr);
#else
	error = fs_frlock(vp, cmd, lfp, flag, offset, cr);
#endif
	AFS_GLOCK();
	AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	if (error || cmd != F_GETLK)
	    return error;
	if (lfp->l_type != F_UNLCK)
	    /* found some blocking lock */
	    return 0;
	/* fall through to check for full AFS file locks */
    }

    /* map BSD style to plain - we don't call reclock() 
     * and its only there that the difference is important
     */
    switch (cmd) {
    case F_GETLK:
    case F_RGETLK:
	break;
    case F_SETLK:
    case F_RSETLK:
	break;
    case F_SETBSDLK:
	cmd = F_SETLK;
	break;
    case F_SETLKW:
    case F_RSETLKW:
	break;
    case F_SETBSDLKW:
	cmd = F_SETLKW;
	break;
    default:
	return EINVAL;
    }

    AFS_GUNLOCK();

    error = convoff(vp, lfp, 0, offset, SEEKLIMIT
#ifdef AFS_SGI64_ENV
		    , OSI_GET_CURRENT_CRED()
#endif /* AFS_SGI64_ENV */
	);

    AFS_GLOCK();
    if (!error) {
#ifdef AFS_SGI65_ENV
	error = afs_lockctl(vp, lfp, cmd, cr, pid);
#else
	error = afs_lockctl(vp, lfp, cmd, cr, OSI_GET_CURRENT_PID());
#endif
    }
    return error;
}


/*
 * We need to get the cache hierarchy right.
 * First comes the page cache - pages are hashed based on afs
 * vnode and offset. It is important to have things hashed here
 * for the VM/paging system to work.
 * Note that the paging system calls VOP_READ with the UIO_NOSPACE -
 * it simply requires that somehow the page is hashed
 * upon successful return.
 * This means in afs_read we
 * must call the 'chunk' code that handles page insertion. In order
 * to actually get the data, 'chunk' calls the VOP_STRATEGY routine.
 * This is basically the std afs_read routine - validating and
 * getting the info into the Dcache, then calling VOP_READ.
 * The only bad thing here is that by calling VOP_READ (and VOP_WRITE
 * to fill the cache) we will get 2 copies of these pages into the
 * page cache - one hashed on afs vnode and one on efs vnode. THis
 * is wasteful but does no harm. A potential solution involves
 * causing an ASYNC flush of the newly fetched cache data and
 * doing direct I/O on the read side....
 */
/* ARGSUSED */
#ifdef AFS_SGI64_ENV
static int
afs_xread(OSI_VC_ARG(avc), uiop, ioflag, cr, flp)
     struct flid *flp;
#else
static int
afs_xread(OSI_VC_ARG(avc), uiop, ioflag, cr)
#endif
OSI_VC_DECL(avc);
     struct uio *uiop;
     int ioflag;
     struct cred *cr;
{
    int code;
    OSI_VC_CONVERT(avc);

    osi_Assert(avc->v.v_count > 0);
    if (avc->v.v_type != VREG)
	return EISDIR;

#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    if (!(ioflag & IO_ISLOCKED))
	AFS_RWLOCK((vnode_t *) avc, VRWLOCK_READ);
#endif
    code = afsrwvp(avc, uiop, UIO_READ, ioflag, cr, flp);
#ifdef AFS_SGI65_ENV
    if (!(ioflag & IO_ISLOCKED))
	AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_READ);
#endif
#else
    code = afsrwvp(avc, uiop, UIO_READ, ioflag, cr);
#endif
    return code;
}

/* ARGSUSED */
#ifdef AFS_SGI64_ENV
static int
afs_xwrite(OSI_VC_ARG(avc), uiop, ioflag, cr, flp)
     struct flid *flp;
#else
static int
afs_xwrite(OSI_VC_ARG(avc), uiop, ioflag, cr)
#endif
OSI_VC_DECL(avc);
     struct uio *uiop;
     int ioflag;
     struct cred *cr;
{
    int code;
    OSI_VC_CONVERT(avc);

    osi_Assert(avc->v.v_count > 0);
    if (avc->v.v_type != VREG)
	return EISDIR;

    if (ioflag & IO_APPEND)
	uiop->uio_offset = avc->m.Length;
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    if (!(ioflag & IO_ISLOCKED))
	AFS_RWLOCK(((vnode_t *) avc), VRWLOCK_WRITE);
#endif
    code = afsrwvp(avc, uiop, UIO_WRITE, ioflag, cr, flp);
#ifdef AFS_SGI65_ENV
    if (!(ioflag & IO_ISLOCKED))
	AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
#else
    code = afsrwvp(avc, uiop, UIO_WRITE, ioflag, cr);
#endif
    return code;
}

static int prra = 0;
static int prnra = 0;
static int acchk = 0;
static int acdrop = 0;

static int
afsrwvp(register struct vcache *avc, register struct uio *uio, enum uio_rw rw,
	int ioflag,
#ifdef AFS_SGI64_ENV
	struct cred *cr, struct flid *flp)
#else
	struct cred *cr)
#endif
{
    register struct vnode *vp = AFSTOV(avc);
    struct buf *bp;
    daddr_t bn;
    off_t acnt, cnt;
    off_t off, newoff;
    off_t bsize, rem, len;
    int error;
    struct bmapval bmv[2];
    int nmaps, didFakeOpen = 0;
    struct vrequest treq;
    struct dcache *tdc;
    int counter = 0;

    osi_Assert((valusema(&avc->vc_rwlock) <= 0)
	       && (OSI_GET_LOCKID() == avc->vc_rwlockid));


    newoff = uio->uio_resid + uio->uio_offset;
    if (uio->uio_resid <= 0) {
	return (0);
    }
    if (uio->uio_offset < 0 || newoff < 0)  {
	return (EINVAL);
    }
    if (ioflag & IO_DIRECT)
	return EINVAL;

    if (rw == UIO_WRITE && vp->v_type == VREG && newoff > uio->uio_limit) {
	return (EFBIG);
    }

    afs_Trace4(afs_iclSetp, CM_TRACE_GRDWR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, ioflag, ICL_TYPE_INT32, rw, ICL_TYPE_INT32, 0);

    /* get a validated vcache entry */
    afs_InitReq(&treq, cr);
    error = afs_VerifyVCache(avc, &treq);
    if (error)
	return afs_CheckCode(error, &treq, 51);

    /*
     * flush any stale pages - this will unmap
     * and invalidate all pages for vp (NOT writing them back!)
     */
    osi_FlushPages(avc, cr);

    if (cr && AFS_NFSXLATORREQ(cr) && rw == UIO_READ) {
	if (!afs_AccessOK
	    (avc, PRSFS_READ, &treq,
	     CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ))
	    return EACCES;
    }
    /*
     * To handle anonymous calls to VOP_STRATEGY from afs_sync/sync/bdflush
     * we need better than the callers credentials. So we squirrel away
     * the last writers credentials
     */
    if (rw == UIO_WRITE || (rw == UIO_READ && avc->cred == NULL)) {
	ObtainWriteLock(&avc->lock, 92);
	if (avc->cred)
	    crfree(avc->cred);
	crhold(cr);
	avc->cred = cr;
	ReleaseWriteLock(&avc->lock);
    }

    /*
     * We have to bump the open/exwriters field here
     * courtesy of the nfs xlator
     * because there're no open/close nfs rpc's to call our afs_open/close.
     */
    if (root_exported && rw == UIO_WRITE) {
	ObtainWriteLock(&avc->lock, 234);
	if (root_exported) {
	    didFakeOpen = 1;
	    afs_FakeOpen(avc);
	}
	ReleaseWriteLock(&avc->lock);
    }
    error = 0;

    if (rw == UIO_WRITE) {
	ObtainWriteLock(&avc->lock, 330);
	avc->states |= CDirty;
	ReleaseWriteLock(&avc->lock);
    }

    AFS_GUNLOCK();

    do {
	/* If v_dpages is set SGI 5.3 will convert those pages to
	 * B_DELWRI in chunkread and getchunk. Write the pages out
	 * before we trigger that behavior. For 6.1, dirty pages stay
	 * around too long and we should get rid of them as quickly
	 * as possible.
	 */
	while (VN_GET_DPAGES(vp))
	    pdflush(vp, 0);

	if (avc->vc_error) {
	    error = avc->vc_error;
	    break;
	}
	bsize = AFSBSIZE;	/* why not?? */
	off = uio->uio_offset % bsize;
	bn = BTOBBT(uio->uio_offset - off);
	/*
	 * decrease bsize - otherwise we will
	 * get 'extra' pages in the cache for this
	 * vnode that we would need to flush when
	 * calling e.g. ptossvp.
	 * So we can use Length in ptossvp,
	 * we make sure we never go more than to the file size
	 * rounded up to a page boundary.
	 * That doesn't quite work, since we may get a page hashed to
	 * the vnode w/o updating the length. Thus we always use
	 * MAXLONG in ptossvp to be safe.
	 */
	if (rw == UIO_READ) {
	    /*
	     * read/paging in a normal file
	     */
	    rem = avc->m.Length - uio->uio_offset;
	    if (rem <= 0)
		/* EOF */
		break;
	    /*
	     * compute minimum of rest of block and rest of file
	     */
	    cnt = MIN(bsize - off, rem);
	    osi_Assert((off + cnt) <= bsize);
	    bsize = ctob(btoc(off + cnt));
	    len = BTOBBT(bsize);
	    nmaps = 1;
	    bmv[0].bn = bmv[0].offset = bn;
	    bmv[0].length = len;
	    bmv[0].bsize = bsize;
	    bmv[0].pboff = off;
	    bmv[0].pbsize = MIN(cnt, uio->uio_resid);
	    bmv[0].eof = 0;
#ifdef AFS_SGI64_ENV
	    bmv[0].pbdev = vp->v_rdev;
	    bmv[0].pmp = uio->uio_pmp;
#endif
	    osi_Assert(cnt > 0);
	    /*
	     * initiate read-ahead if it looks like
	     * we are reading sequentially OR they want
	     * more than one 'bsize' (==AFSBSIZE) worth
	     * XXXHack - to avoid DELWRI buffers we can't
	     * do read-ahead on any file that has potentially
	     * dirty mmap pages.
	     */
	    if ((avc->lastr + BTOBB(AFSBSIZE) == bn
		 || uio->uio_resid > AFSBSIZE)
#ifdef AFS_SGI61_ENV
		&& (!AFS_VN_MAPPED(vp))
#else /* AFS_SGI61_ENV */
		&& ((vp->v_flag & VWASMAP) == 0)
#endif /* AFS_SGI61_ENV */
		) {
		rem -= cnt;
		if (rem > 0) {
		    bsize = AFSBSIZE;
		    bmv[1].bn = bmv[1].offset = bn + len;
		    osi_Assert((BBTOB(bn + len) % bsize) == 0);
		    acnt = MIN(bsize, rem);
		    bsize = ctob(btoc(acnt));
		    len = BTOBBT(bsize);
		    nmaps = 2;
		    bmv[1].length = len;
		    bmv[1].eof = 0;
		    bmv[1].bsize = bsize;
		    bmv[1].pboff = 0;
		    bmv[1].pbsize = acnt;
#ifdef AFS_SGI64_ENV
		    bmv[1].pmp = uio->uio_pmp;
		    bmv[1].pbdev = vp->v_rdev;
#endif
		}
	    }
#ifdef DEBUG
	    else if (prnra)
		printf
		    ("NRA:vp 0x%x lastr %d bn %d len %d cnt %d bsize %d rem %d resid %d\n",
		     vp, avc->lastr, bn, len, cnt, bsize, rem,
		     uio->uio_resid);
#endif

	    avc->lastr = bn;
	    bp = chunkread(vp, bmv, nmaps, cr);
	    /*
	     * If at a chunk boundary, start prefetch of next chunk.
	     */
	    if (counter == 0 || AFS_CHUNKOFFSET(off) == 0) {
		AFS_GLOCK();
		ObtainWriteLock(&avc->lock, 562);
		tdc = afs_FindDCache(avc, off);
		if (tdc) {
		    if (!(tdc->mflags & DFNextStarted))
			afs_PrefetchChunk(avc, tdc, cr, &treq);
		    afs_PutDCache(tdc);
		}
		ReleaseWriteLock(&avc->lock);
		AFS_GUNLOCK();
	    }
	    counter++;
	} else {
	    /*
	     * writing a normal file
	     */
	    /*
	     * Purge dirty chunks of file if there are too many dirty chunks.
	     * Inside the write loop, we only do this at a chunk boundary.
	     * Clean up partial chunk if necessary at end of loop.
	     */
	    if (counter > 0 && AFS_CHUNKOFFSET(uio->uio_offset) == 0) {
		AFS_GLOCK();
		ObtainWriteLock(&avc->lock, 90);
		error = afs_DoPartialWrite(avc, &treq);
		if (error == 0)
		    avc->states |= CDirty;
		ReleaseWriteLock(&avc->lock);
		AFS_GUNLOCK();
		if (error)
		    break;
	    }
	    counter++;

	    cnt = MIN(bsize - off, uio->uio_resid);
	    bsize = ctob(btoc(off + cnt));
	    len = BTOBBT(bsize);
	    bmv[0].bn = bn;
	    bmv[0].offset = bn;
	    bmv[0].length = len;
	    bmv[0].eof = 0;
	    bmv[0].bsize = bsize;
	    bmv[0].pboff = off;
	    bmv[0].pbsize = cnt;
#ifdef AFS_SGI64_ENV
	    bmv[0].pmp = uio->uio_pmp;
#endif

	    if (cnt == bsize)
		bp = getchunk(vp, bmv, cr);
	    else
		bp = chunkread(vp, bmv, 1, cr);

	    avc->m.Date = osi_Time();	/* Set file date (for ranlib) */
	}
	if (bp->b_flags & B_ERROR) {
	    /*
	     * Since we compile -signed, b_error is a signed
	     * char when it should be an unsigned char.
	     * This can cause some errors codes to be interpreted
	     * as negative #s
	     */
	    error = (unsigned char)(bp->b_error);
	    if (!error)
		error = EIO;
#ifdef DEBUG
	    if (acchk && error) {
		cmn_err(CE_WARN, "bp 0x%x has error %d\n", bp, error);
		if (acdrop)
		    debug("AFS");
	    }
#endif
	    brelse(bp);
	    break;
	}

	osi_Assert(bp->b_error == 0);

	if (uio->uio_segflg != UIO_NOSPACE)
	    (void)bp_mapin(bp);
	AFS_UIOMOVE(bp->b_un.b_addr + bmv[0].pboff, cnt, rw, uio, error);
	if (rw == UIO_READ || error) {
	    if (bp->b_flags & B_DELWRI) {
		bawrite(bp);
	    } else
		brelse(bp);
	} else {
	    /*
	     * m.Length is the maximum number of bytes known to be in the file.
	     * Make sure it is at least as high as the last byte we just wrote
	     * into the buffer.
	     */
	    if (avc->m.Length < uio->uio_offset)  {
		AFS_GLOCK();
		ObtainWriteLock(&avc->lock, 235);
		avc->m.Length = uio->uio_offset;
		ReleaseWriteLock(&avc->lock);
		AFS_GUNLOCK();
	    }
	    if (uio->uio_fmode & FSYNC) {
		error = bwrite(bp);
	    } else if (off + cnt < bsize) {
		bawrite(bp);	/* was bdwrite */
	    } else {
		bp->b_flags |= B_AGE;
		bawrite(bp);
	    }
	    /*
	     * Since EIO on an unlinked file is non-intuitive - give some
	     * explanation
	     */
	    if (error) {
		if (avc->m.LinkCount == 0)
		    cmn_err(CE_WARN,
			    "AFS: Process pid %d write error %d writing to unlinked file.",
			    OSI_GET_CURRENT_PID(), error);
	    }
	}
    } while (!error && uio->uio_resid > 0);
    afs_chkpgoob(&avc->v, btoc(avc->m.Length));

    AFS_GLOCK();

    if (rw == UIO_WRITE && error == 0 && (avc->states & CDirty)) {
	ObtainWriteLock(&avc->lock, 405);
	error = afs_DoPartialWrite(avc, &treq);
	ReleaseWriteLock(&avc->lock);
    }

    if (!error) {
#ifdef AFS_SGI61_ENV
	if (((ioflag & IO_SYNC) || (ioflag & IO_DSYNC)) && (rw == UIO_WRITE)
	    && !AFS_NFSXLATORREQ(cr)) {
	    error = afs_fsync(avc, 0, cr);
	}
#else /* AFS_SGI61_ENV */
	if ((ioflag & IO_SYNC) && (rw == UIO_WRITE) && !AFS_NFSXLATORREQ(cr)) {
	    error = afs_fsync(avc, 0, cr);
	}
#endif /* AFS_SGI61_ENV */
    }
    if (didFakeOpen) {
	ObtainWriteLock(&avc->lock, 236);
	afs_FakeClose(avc, cr);	/* XXXX For nfs trans XXXX */
	ReleaseWriteLock(&avc->lock);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GRDWR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, ioflag, ICL_TYPE_INT32, rw, ICL_TYPE_INT32,
	       error);

    return (error);
}

int
afs_xbmap(OSI_VC_ARG(avc), offset, count, flag, cr, bmv, nbmv)
OSI_VC_DECL(avc);
     off_t offset;
     ssize_t count;
     int flag;
     struct cred *cr;
     struct bmapval *bmv;
     int *nbmv;
{
    int bsize;			/* server's block size in bytes */
    off_t off;
    size_t rem, cnt;
    OSI_VC_CONVERT(avc);

    bsize = AFSBSIZE;
    off = offset % bsize;	/* offset into block */
    bmv->bn = BTOBBT(offset - off);
    bmv->offset = bmv->bn;
    bmv->pboff = off;
    rem = avc->m.Length - offset;
    if (rem <= 0)
	cnt = 0;		/* EOF */
    else
	cnt = MIN(bsize - off, rem);

    /*
     * It is benign to ignore *nbmv > 1, since it is only for requesting
     * readahead.
     */

    /*
     * Don't map more than up to next page if at end of file
     * See comment in afsrwvp
     */
    osi_Assert((off + cnt) <= bsize);
    bsize = ctob(btoc(off + cnt));
    bmv->pbsize = MIN(cnt, count);
    bmv->eof = 0;
#ifdef AFS_SGI64_ENV
    bmv->pmp = NULL;
    bmv->pbdev = avc->v.v_rdev;
#endif
    bmv->bsize = bsize;
    bmv->length = BTOBBT(bsize);
    *nbmv = 1;
    return (0);
}

/*
 * called out of chunkread from afs_xread & clusterwrite to push dirty
 * pages back - this routine
 * actually does the reading/writing by calling afs_read/afs_write
 * bp points to a set of pages that have been inserted into
 * the page cache hashed on afs vp.
 */
static void
afs_strategy(OSI_VC_ARG(avc), bp)
OSI_VC_DECL(avc);
     struct buf *bp;
{
    uio_t auio;
    uio_t *uio = &auio;
    iovec_t aiovec;
    int error;
    struct cred *cr;
    OSI_VC_CONVERT(avc);
    vnode_t *vp = (vnode_t *) avc;

    /*
     * We can't afford DELWRI buffers for 2 reasons:
     * 1) Since we can call underlying EFS, we can require a
     *  buffer to flush a buffer. This leads to 2 potential
     *  recursions/deadlocks
     *          a) if all buffers are DELWRI afs buffers, then
     *             ngeteblk -> bwrite -> afs_strategy -> afs_write ->
     *             UFS_Write -> efs_write -> ngeteblk .... could
     *             recurse a long ways!
     *          b) brelse -> chunkhold which can call dchunkpush
     *             will look for any DELWRI buffers and call strategy
     *             on them. This can then end up via UFS_Write
     *             recursing
     * Current hack:
     *  a) We never do bdwrite(s) on AFS buffers.
     *  b) We call pdflush with B_ASYNC
     *  c) in chunkhold where it can set a buffer DELWRI
     *     we immediatly do a clusterwrite for AFS vp's
     * XXX Alas, 'c' got dropped in 5.1 so its possible to get DELWRI
     *  buffers if someone has mmaped the file and dirtied it then
     *  reads/faults it again.
     *  Instead - wherever we call chunkread/getchunk we check for a
     *  returned bp with DELWRI set, and write it out immediately
     */
    if (CheckLock(&avc->lock) && VN_GET_DBUF(vp)) {
	printf("WARN: afs_strategy vp=%x, v_dbuf=%x bp=%x\n", vp,
	       VN_GET_DBUF(vp), bp);
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;
	iodone(bp);
	return;
    }
    if (bp->b_error != 0)
	printf("WARNING: afs_strategy3 vp=%x, bp=%x, err=%x\n", vp, bp,
	       bp->b_error);

    /*
     * To get credentials somewhat correct (we may be called from bdflush/
     * sync) we use saved credentials in Vcache.
     * We must hold them since someone else could change them
     */
    ObtainReadLock(&avc->lock);
    if (bp->b_flags & B_READ) {
	if (BBTOB(bp->b_blkno) >= avc->m.Length) {
	    /* we are responsible for zero'ing the page */
	    caddr_t c;
	    c = bp_mapin(bp);
	    memset(c, 0, bp->b_bcount);
	    iodone(bp);
	    ReleaseReadLock(&avc->lock);
	    return;
	}
    } else if ((avc->states & CWritingUFS) && (bp->b_flags & B_DELWRI)) {
	bp->b_ref = 3;
	ReleaseReadLock(&avc->lock);
	iodone(bp);
	return;
    }
    cr = avc->cred;
    osi_Assert(cr);
    crhold(cr);
    ReleaseReadLock(&avc->lock);

    aiovec.iov_base = bp_mapin(bp);
    uio->uio_iov = &aiovec;
    uio->uio_iovcnt = 1;
    uio->uio_resid = aiovec.iov_len = bp->b_bcount;
    uio->uio_offset = BBTOB(bp->b_blkno);
    uio->uio_segflg = UIO_SYSSPACE;
    uio->uio_limit = RLIM_INFINITY;	/* we checked the limit earlier */
#ifdef AFS_SGI64_ENV
    uio->uio_pmp = NULL;
#endif

    if (bp->b_flags & B_READ) {
	uio->uio_fmode = FREAD;
	error = afs_read(vp, uio, cr, 0, 0, 0);
    } else {
	uio->uio_fmode = FWRITE;
	error = afs_write(vp, uio, 0, cr, 0);
    }
    crfree(cr);

#ifdef DEBUG
    if (acchk && error) {
	cmn_err(CE_WARN, "vp 0x%x has error %d\n", vp, error);
	if (acdrop)
	    debug("AFS");
    }
#endif
    if (error) {
	bp->b_error = error;
	bp->b_flags |= B_ERROR;
	if ((uio->uio_fmode == FWRITE) && !avc->vc_error)
	    avc->vc_error = error;
    }
    iodone(bp);
    return;
}

/* ARGSUSED */
static int
afs_seek(OSI_VC_ARG(avc), ooff, noffp)
OSI_VC_DECL(avc);
     off_t ooff;
     off_t *noffp;
{
    return *noffp < 0 ? EINVAL : 0;
}

#if !defined(AFS_SGI65_ENV)
/* Irix 6.5 uses addmap/delmap only for devices. */
/* ARGSUSED */
static int
afs_addmap(OSI_VC_ARG(avc), off, prp, addr, len, prot, maxprot, flags, cr)
     off_t off;
OSI_VC_DECL(avc);
     struct pregion *prp;
     addr_t addr;
     size_t len;
     u_int prot, maxprot;
     u_int flags;
     struct cred *cr;
{
    OSI_VC_CONVERT(avc);
    struct vnode *vp = AFSTOV(avc);

    if (vp->v_flag & VNOMAP)
	return ENOSYS;
    if (len == 0)
	return 0;
    AFS_RWLOCK(vp, VRWLOCK_WRITE);
    if (avc->mapcnt == 0) {
	/* on first mapping add a open reference */
	ObtainWriteLock(&avc->lock, 237);
	avc->execsOrWriters++;
	avc->opens++;
	ReleaseWriteLock(&avc->lock);
    }
    avc->mapcnt += btoc(len);
    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
    return 0;
}

 /*ARGSUSED*/ static int
afs_delmap(OSI_VC_ARG(avc), off, prp, addr, len, prot, maxprot, flags, acred)
     off_t off;
OSI_VC_DECL(avc);
     struct pregion *prp;
     addr_t addr;
     size_t len;
     u_int prot, maxprot;
     u_int flags;
     struct cred *acred;
{
    OSI_VC_CONVERT(avc);
    struct vnode *vp = AFSTOV(avc);
    register struct brequest *tb;
    struct vrequest treq;
    afs_int32 code;

    if (vp->v_flag & VNOMAP)
	return ENOSYS;
    if (len == 0)
	return 0;
    AFS_RWLOCK(vp, VRWLOCK_WRITE);
    osi_Assert(avc->mapcnt > 0);
    avc->mapcnt -= btoc(len);
    osi_Assert(avc->mapcnt >= 0);
    if (avc->mapcnt == 0) {
	/* on last mapping push back and remove our reference */
	osi_Assert(avc->execsOrWriters > 0);
	osi_Assert(avc->opens > 0);
	if (avc->m.LinkCount == 0) {
	    ObtainWriteLock(&avc->lock, 238);
	    AFS_GUNLOCK();
	    PTOSSVP(vp, (off_t) 0, (off_t) MAXLONG);
	    AFS_GLOCK();
	    ReleaseWriteLock(&avc->lock);
	}
	/*
	 * mimic afs_close
	 */
	afs_InitReq(&treq, acred);
	if (afs_BBusy()) {
	    /* do it yourself if daemons are all busy */
	    ObtainWriteLock(&avc->lock, 239);
	    code = afs_StoreOnLastReference(avc, &treq);
	    ReleaseWriteLock(&avc->lock);
	    /* BStore does CheckCode so we should also */
	    /* VNOVNODE is "acceptable" error code from close, since
	     * may happen when deleting a file on another machine while
	     * it is open here. */
	    if (code == VNOVNODE)
		code = 0;
	    if (code) {
		afs_StoreWarn(code, avc->fid.Fid.Volume,	/* /dev/console */
			      1);
	    }
	    code = afs_CheckCode(code, &treq, 52);
	    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	} else {
	    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	    /* at least one daemon is idle, so ask it to do the store.
	     * Also, note that  we don't lock it any more... */
	    tb = afs_BQueue(BOP_STORE, avc, 0, 1, acred,
			    (afs_size_t) acred->cr_uid, 0L, (void *)0);
	    /* sleep waiting for the store to start, then retrieve error code */
	    while ((tb->flags & BUVALID) == 0) {
		tb->flags |= BUWAIT;
		afs_osi_Sleep(tb);
	    }
	    code = tb->code;
	    afs_BRelease(tb);
	}
    } else {
	AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
    }
    return 0;
}
#endif /* ! AFS_SGI65_ENV */


/* ARGSUSED */
/*
 * Note - if mapping in an ELF interpreter, one can get called without vp
 * ever having been 'opened'
 */
#ifdef AFS_SGI65_ENV
static int
afs_map(OSI_VC_ARG(avc), off, len, prot, flags, cr, vpp)
     off_t off;
OSI_VC_DECL(avc);
     size_t len;
     mprot_t prot;
     u_int flags;
     struct cred *cr;
     vnode_t **vpp;
#else
static int
afs_map(OSI_VC_ARG(avc), off, prp, addrp, len, prot, maxprot, flags, cr)
     off_t off;
OSI_VC_DECL(avc);
     struct pregion *prp;
     addr_t *addrp;
     size_t len;
     u_int prot, maxprot;
     u_int flags;
     struct cred *cr;
#endif
{
    OSI_VC_CONVERT(avc);
    struct vnode *vp = AFSTOV(avc);
    struct vrequest treq;
    int error;

    /* get a validated vcache entry */
    afs_InitReq(&treq, cr);
    error = afs_VerifyVCache(avc, &treq);
    if (error)
	return afs_CheckCode(error, &treq, 53);

    osi_FlushPages(avc, cr);	/* ensure old pages are gone */
#ifdef AFS_SGI65_ENV
    /* If the vnode is currently opened for write, there's the potential
     * that this mapping might (now or in the future) have PROT_WRITE.
     * So assume it does and we'll have to call afs_StoreOnLastReference.
     */
    AFS_RWLOCK(vp, VRWLOCK_WRITE);
    ObtainWriteLock(&avc->lock, 501);
    if (avc->execsOrWriters > 0) {
	avc->execsOrWriters++;
	avc->opens++;
	avc->mapcnt++;		/* count eow's due to mappings. */
    }
    ReleaseWriteLock(&avc->lock);
    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
#else
    AFS_RWLOCK(vp, VRWLOCK_WRITE);
    AFS_GUNLOCK();
    error =
	fs_map_subr(vp, (off_t) avc->m.Length, (u_int) avc->m.Mode, off, prp,
		    *addrp, len, prot, maxprot, flags, cr);
    AFS_GLOCK();
    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
#endif /* AFS_SGI65_ENV */
    afs_Trace4(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, vp,
#ifdef AFS_SGI65_ENV
	       ICL_TYPE_POINTER, NULL,
#else
	       ICL_TYPE_POINTER, *addrp,
#endif
	       ICL_TYPE_INT32, len, ICL_TYPE_INT32, off);
    return error;
}


extern afs_rwlock_t afs_xvcache;
extern afs_lock_t afs_xdcache;
#ifdef AFS_SGI64_ENV
int
#else
void
#endif
afs_xinactive(OSI_VC_ARG(avc), acred)
OSI_VC_DECL(avc);
     struct ucred *acred;
{
    int s;
    OSI_VC_CONVERT(avc);
    vnode_t *vp = (vnode_t *) avc;
    int mapcnt = avc->mapcnt;	/* We just clear off this many. */

    AFS_STATCNT(afs_inactive);

    s = VN_LOCK(vp);
    if (!(vp->v_flag & VINACT) || (vp->v_count > 0)) {
	/* inactive was already done, or someone did a VN_HOLD; just return */
	vp->v_flag &= ~VINACT;
	VN_UNLOCK(vp, s);
#ifdef AFS_SGI64_ENV
	return VN_INACTIVE_CACHE;
#else
	return;
#endif
    }
    osi_Assert((vp->v_flag & VSHARE) == 0);
    vp->v_flag &= ~VINACT;
    /* Removed broadcast to waiters, since no one ever will. Only for vnodes
     * in common pool.
     */
    VN_UNLOCK(vp, s);

#ifdef AFS_SGI65_ENV
    /* In Irix 6.5, the last unmap of a dirty mmap'd file does not
     * get an explicit vnode op. Instead we only find out at VOP_INACTIVE.
     */
    if (!afs_rwlock_nowait((vnode_t *) avc, VRWLOCK_WRITE)) {
	return VN_INACTIVE_CACHE;
    }
    if (NBObtainWriteLock(&avc->lock, 502)) {
	AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	return VN_INACTIVE_CACHE;
    }
    if (avc->states & CUnlinked) {
	if (CheckLock(&afs_xvcache) || CheckLock(&afs_xdcache)) {
	    avc->states |= CUnlinkedDel;
	    ReleaseWriteLock(&avc->lock);
	    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	} else {
	    ReleaseWriteLock(&avc->lock);
	    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	    afs_remunlink(avc, 1);	/* ignore any return code */
	}
	return VN_INACTIVE_CACHE;
    }
    if ((avc->states & CDirty) || (avc->execsOrWriters > 0)) {
	/* File either already has dirty chunks (CDirty) or was mapped at 
	 * time in its life with the potential for being written into. 
	 * Note that afs_close defers storebacks if the vnode's ref count
	 * if more than 1.
	 */
	int code;
	struct vrequest treq;
	if (!afs_InitReq(&treq, acred)) {
	    int s;

	    VN_HOLD(vp);
	    avc->execsOrWriters -= mapcnt - 1;
	    avc->opens -= mapcnt - 1;
	    avc->mapcnt -= mapcnt;
	    code = afs_StoreOnLastReference(avc, &treq);
	    /* The following behavior mimics the behavior in afs_close. */
	    if (code == VNOVNODE || code == ENOENT)
		code = 0;
	    if (code) {
		if (mapcnt) {
		    cmn_err(CE_WARN,
			    "AFS: Failed to store FID (%x:%lu.%lu.%lu) in VOP_INACTIVE, error = %d\n",
			    (int)(avc->fid.Cell) & 0xffffffff,
			    avc->fid.Fid.Volume, avc->fid.Fid.Vnode,
			    avc->fid.Fid.Unique, code);
		}
		afs_InvalidateAllSegments(avc);
	    }
	    s = VN_LOCK(vp);
	    vp->v_count--;
	    code = (vp->v_count == 0);
	    VN_UNLOCK(vp, s);
	    /* If the vnode is now in use by someone else, return early. */
	    if (!code) {
		ReleaseWriteLock(&avc->lock);
		AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
		return VN_INACTIVE_CACHE;
	    }
	}
    }
#endif

    osi_Assert((avc->states & (CCore | CMAPPED)) == 0);

    if (avc->cred) {
	crfree(avc->cred);
	avc->cred = NULL;
    }
    ReleaseWriteLock(&avc->lock);
    AFS_RWUNLOCK(vp, VRWLOCK_WRITE);

    /*
     * If someone unlinked a file and this is the last hurrah -
     * nuke all the pages.
     */
    if (avc->m.LinkCount == 0) {
	AFS_GUNLOCK();
	PTOSSVP(vp, (off_t) 0, (off_t) MAXLONG);
	AFS_GLOCK();
    }
#ifndef AFS_SGI65_ENV
    osi_Assert(avc->mapcnt == 0);
    afs_chkpgoob(&avc->v, btoc(avc->m.Length));

    avc->states &= ~CDirty;	/* Give up on store-backs */
    if (avc->states & CUnlinked) {
	if (CheckLock(&afs_xvcache) || CheckLock(&afs_xdcache)) {
	    avc->states |= CUnlinkedDel;
	} else {
	    afs_remunlink(avc, 1);	/* ignore any return code */
	}
    }
#endif
#ifdef AFS_SGI64_ENV
    return VN_INACTIVE_CACHE;
#endif
}

static int
afs_reclaim(OSI_VC_DECL(avc), int flag)
{
#ifdef AFS_SGI64_ENV
    /* Get's called via VOP_RELCAIM in afs_FlushVCache to clear repl_vnodeops */
    return 0;
#else
    panic("afs_reclaim");
#endif
}

void
afs_rwlock(OSI_VN_DECL(vp), AFS_RWLOCK_T flag)
{
    OSI_VN_CONVERT(vp);
    struct vcache *avc = VTOAFS(vp);

    if (OSI_GET_LOCKID() == avc->vc_rwlockid) {
	avc->vc_locktrips++;
	return;
    }
    AFS_GUNLOCK();
    psema(&avc->vc_rwlock, PINOD);
    AFS_GLOCK();
    avc->vc_rwlockid = OSI_GET_LOCKID();
}

void
afs_rwunlock(OSI_VN_DECL(vp), AFS_RWLOCK_T flag)
{
    OSI_VN_CONVERT(vp);
    struct vcache *avc = VTOAFS(vp);

    AFS_ASSERT_GLOCK();
    osi_Assert(OSI_GET_LOCKID() == avc->vc_rwlockid);
    if (avc->vc_locktrips > 0) {
	--avc->vc_locktrips;
	return;
    }
    avc->vc_rwlockid = OSI_NO_LOCKID;
    vsema(&avc->vc_rwlock);
}


/* The flag argument is for symmetry with the afs_rwlock and afs_rwunlock
 * calls. SGI currently only uses the flag to assert if the unlock flag
 * does not match the corresponding lock flag. But they may start using this
 * flag for a real rw lock at some time.
 */
int
afs_rwlock_nowait(vnode_t * vp, AFS_RWLOCK_T flag)
{
    struct vcache *avc = VTOAFS(vp);

    AFS_ASSERT_GLOCK();
    if (OSI_GET_LOCKID() == avc->vc_rwlockid) {
	avc->vc_locktrips++;
	return 1;
    }
    if (cpsema(&avc->vc_rwlock)) {
	avc->vc_rwlockid = OSI_GET_LOCKID();
	return 1;
    }
    return 0;
}

#if defined(AFS_SGI64_ENV) && defined(CKPT) && !defined(_R5000_CVT_WAR)
int
afs_fid2(OSI_VC_DECL(avc), struct fid *fidp)
{
    struct cell *tcell;
    afs_fid2_t *afid = (afs_fid2_t *) fidp;
    OSI_VC_CONVERT(avc);

    osi_Assert(sizeof(fid_t) >= sizeof(afs_fid2_t));
    afid->af_len = sizeof(afs_fid2_t) - sizeof(afid->af_len);

    tcell = afs_GetCell(avc->fid.Cell, READ_LOCK);
    afid->af_cell = tcell->cellIndex & 0xffff;
    afs_PutCell(tcell, READ_LOCK);

    afid->af_volid = avc->fid.Fid.Volume;
    afid->af_vno = avc->fid.Fid.Vnode;
    afid->af_uniq = avc->fid.Fid.Unique;

    return 0;
}
#else
/* Only use so far is in checkpoint/restart for IRIX 6.4. In ckpt_fid, a
 * return of ENOSYS would make the code fail over to VOP_FID. We can't let
 * that happen, since we do a VN_HOLD there in the expectation that 
 * posthandle will be called to release the vnode.
 *
 * afs_fid2 is used to support the R5000 workarounds (_R5000_CVT_WAR)
 */
int
afs_fid2(OSI_VC_DECL(avc), struct fid *fidp)
{
#if defined(_R5000_CVT_WAR)
    extern int R5000_cvt_war;

    if (R5000_cvt_war)
	return ENOSYS;
    else
	return EINVAL;
#else
    return EINVAL;
#endif
}
#endif /* AFS_SGI64_ENV && CKPT */


/*
 * check for any pages hashed that shouldn't be!
 * Only valid if PGCACHEDEBUG is set in os/page.c
 * Drop the global lock here, since we may not actually do the call.
 */
void
afs_chkpgoob(vnode_t * vp, pgno_t pgno)
{
#undef PGDEBUG
#ifdef PGDEBUG
    AFS_GUNLOCK();
    pfindanyoob(vp, pgno);
    AFS_GLOCK();
#endif
}


#ifdef MP

#ifdef AFS_SGI64_ENV
#define AFS_MP_VC_ARG(A) bhv_desc_t A
#else
#define AFS_MP_VC_ARG(A) vnode_t A
#endif

#ifdef AFS_SGI64_ENV
int
mp_afs_open(bhv_desc_t * bhp, vnode_t ** a, mode_t b, struct cred *c)
#else
int
mp_afs_open(vnode_t ** a, mode_t b, struct cred *c)
#endif
{
    int rv;
    AFS_GLOCK();
#ifdef AFS_SGI64_ENV
    rv = afs_lockedvnodeops.vop_open(bhp, a, b, c);
#else
    rv = afs_lockedvnodeops.vop_open(a, b, c);
#endif
    AFS_GUNLOCK();
    return rv;
}

#if defined(AFS_SGI64_ENV)
#if defined(AFS_SGI65_ENV)
int
mp_afs_close(AFS_MP_VC_ARG(*a), int b, lastclose_t c, struct cred *d)
#else
int
mp_afs_close(AFS_MP_VC_ARG(*a), int b, lastclose_t c, off_t d, struct cred *e,
	     struct flid *f)
#endif
#else
int
mp_afs_close(AFS_MP_VC_ARG(*a), int b, lastclose_t c, off_t d, struct cred *e)
#endif
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_close(a, b, c, d
#if !defined(AFS_SGI65_ENV)
				      , e
#if defined(AFS_SGI64_ENV)
				      , f
#endif
#endif
	);

    AFS_GUNLOCK();
    return rv;
}

#ifdef AFS_SGI64_ENV
int
mp_afs_read(AFS_MP_VC_ARG(*a), struct uio *b, int c, struct cred *d,
	    struct flid *f)
#else
int
mp_afs_read(AFS_MP_VC_ARG(*a), struct uio *b, int c, struct cred *d)
#endif
{
    int rv;
    AFS_GLOCK();
#ifdef AFS_SGI64_ENV
    rv = afs_lockedvnodeops.vop_read(a, b, c, d, f);
#else
    rv = afs_lockedvnodeops.vop_read(a, b, c, d);
#endif
    AFS_GUNLOCK();
    return rv;
}


#ifdef AFS_SGI64_ENV
int
mp_afs_write(AFS_MP_VC_ARG(*a), struct uio *b, int c, struct cred *d,
	     struct flid *f)
#else
int
mp_afs_write(AFS_MP_VC_ARG(*a), struct uio *b, int c, struct cred *d)
#endif
{
    int rv;
    AFS_GLOCK();
#ifdef AFS_SGI64_ENV
    rv = afs_lockedvnodeops.vop_write(a, b, c, d, f);
#else
    rv = afs_lockedvnodeops.vop_write(a, b, c, d);
#endif
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_ioctl(AFS_MP_VC_ARG(*a), int b, void *c, int d, struct cred *e, int *f
#ifdef AFS_SGI65_ENV
	     , struct vopbd *vbds
#endif
    )
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_ioctl(a, b, c, d, e, f
#ifdef AFS_SGI65_ENV
				      , vbds
#endif
	);
    AFS_GUNLOCK();
    return rv;
}

int
mp_fs_setfl(AFS_MP_VC_ARG(*a), int b, int c, struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_setfl(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_getattr(AFS_MP_VC_ARG(*a), struct vattr *b, int c, struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_getattr(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_setattr(AFS_MP_VC_ARG(*a), struct vattr *b, int c, struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_setattr(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_access(AFS_MP_VC_ARG(*a), int b,
#ifndef AFS_SGI65_ENV
	      int c,
#endif
	      struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_access(a, b,
#ifndef AFS_SGI65_ENV
				       c,
#endif
				       d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_lookup(AFS_MP_VC_ARG(*a), char *b, vnode_t ** c, struct pathname *d,
	      int e, vnode_t * f, struct cred *g)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_lookup(a, b, c, d, e, f, g);
    AFS_GUNLOCK();
    return rv;
}

#ifdef AFS_SGI64_ENV
int
mp_afs_create(AFS_MP_VC_ARG(*a), char *b, struct vattr *c, int d, int e,
	      vnode_t ** f, struct cred *g)
#else
int
mp_afs_create(AFS_MP_VC_ARG(*a), char *b, struct vattr *c, enum vcexcl d,
	      int e, vnode_t ** f, struct cred *g)
#endif
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_create(a, b, c, d, e, f, g);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_remove(AFS_MP_VC_ARG(*a), char *b, struct cred *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_remove(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_link(AFS_MP_VC_ARG(*a), vnode_t * b, char *c, struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_link(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_rename(AFS_MP_VC_ARG(*a), char *b, vnode_t * c, char *d,
	      struct pathname *e, struct cred *f)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_rename(a, b, c, d, e, f);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_mkdir(AFS_MP_VC_ARG(*a), char *b, struct vattr *c, vnode_t ** d,
	     struct cred *e)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_mkdir(a, b, c, d, e);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_rmdir(AFS_MP_VC_ARG(*a), char *b, vnode_t * c, struct cred *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_rmdir(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_readdir(AFS_MP_VC_ARG(*a), struct uio *b, struct cred *c, int *d)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_readdir(a, b, c, d);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_symlink(AFS_MP_VC_ARG(*a), char *b, struct vattr *c, char *d,
	       struct cred *e)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_symlink(a, b, c, d, e);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_readlink(AFS_MP_VC_ARG(*a), struct uio *b, struct cred *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_readlink(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_fsync(AFS_MP_VC_ARG(*a), int b, struct cred *c
#ifdef AFS_SGI65_ENV
	     , off_t start, off_t stop
#endif
    )
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_fsync(a, b, c
#ifdef AFS_SGI65_ENV
				      , start, stop
#endif
	);
    AFS_GUNLOCK();
    return rv;
}

void
mp_afs_inactive(AFS_MP_VC_ARG(*a), struct cred *b)
{
    AFS_GLOCK();
    afs_lockedvnodeops.vop_inactive(a, b);
    AFS_GUNLOCK();
    return;
}

int
mp_afs_fid(AFS_MP_VC_ARG(*a), struct fid **b)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_fid(a, b);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_fid2(AFS_MP_VC_ARG(*a), struct fid *b)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_fid2(a, b);
    AFS_GUNLOCK();
    return rv;
}

void
mp_afs_rwlock(AFS_MP_VC_ARG(*a), AFS_RWLOCK_T b)
{
    AFS_GLOCK();
    afs_rwlock(a, VRWLOCK_WRITE);
    AFS_GUNLOCK();
}

void
mp_afs_rwunlock(AFS_MP_VC_ARG(*a), AFS_RWLOCK_T b)
{
    AFS_GLOCK();
    afs_rwunlock(a, VRWLOCK_WRITE);
    AFS_GUNLOCK();
}

int
mp_afs_seek(AFS_MP_VC_ARG(*a), off_t b, off_t * c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_seek(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_fs_cmp(AFS_MP_VC_ARG(*a), vnode_t * b)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_cmp(a, b);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_frlock(AFS_MP_VC_ARG(*a), int b, struct flock *c, int d, off_t e,
#ifdef AFS_SGI65_ENV
	      vrwlock_t vrwlock,
#endif
	      struct cred *f)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_frlock(a, b, c, d, e,
#ifdef AFS_SGI65_ENV
				       vrwlock,
#endif
				       f);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_realvp(AFS_MP_VC_ARG(*a), vnode_t ** b)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_realvp(a, b);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_bmap(AFS_MP_VC_ARG(*a), off_t b, ssize_t c, int d, struct cred *e,
	    struct bmapval *f, int *g)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_bmap(a, b, c, d, e, f, g);
    AFS_GUNLOCK();
    return rv;
}

void
mp_afs_strategy(AFS_MP_VC_ARG(*a), struct buf *b)
{
    int rv;
    AFS_GLOCK();
    afs_lockedvnodeops.vop_strategy(a, b);
    AFS_GUNLOCK();
    return;
}

#ifdef AFS_SGI65_ENV
int
mp_afs_map(AFS_MP_VC_ARG(*a), off_t b, size_t c, mprot_t d, u_int e,
	   struct cred *f, vnode_t ** g)
#else
int
mp_afs_map(AFS_MP_VC_ARG(*a), off_t b, struct pregion *c, char **d, size_t e,
	   u_int f, u_int g, u_int h, struct cred *i)
#endif
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_map(a, b, c, d, e, f, g
#ifndef AFS_SGI65_ENV
				    , h, i
#endif
	);
    AFS_GUNLOCK();
    return rv;
}


#ifndef AFS_SGI65_ENV
/* As of Irix 6.5, addmap and delmap are only for devices */
int
mp_afs_addmap(AFS_MP_VC_ARG(*a), off_t b, struct pregion *c, addr_t d,
	      size_t e, u_int f, u_int g, u_int h, struct cred *i)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_addmap(a, b, c, d, e, f, g, h, i);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_delmap(AFS_MP_VC_ARG(*a), off_t b, struct pregion *c, addr_t d,
	      size_t e, u_int f, u_int g, u_int h, struct cred *i)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_delmap(a, b, c, d, e, f, g, h, i);
    AFS_GUNLOCK();
    return rv;
}
#endif /* ! AFS_SGI65_ENV */

int
mp_fs_poll(AFS_MP_VC_ARG(*a), short b, int c, short *d, struct pollhead **e
#ifdef AFS_SGI65_ENV
	   , unsigned int *f
#endif
    )
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvnodeops.vop_poll(a, b, c, d, e
#ifdef AFS_SGI65_ENV
				     , f
#endif
	);
    AFS_GUNLOCK();
    return rv;
}


struct vnodeops Afs_vnodeops = {
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
#else
    VNODE_POSITION_BASE,
#endif
#endif
    mp_afs_open,
    mp_afs_close,
    mp_afs_read,
    mp_afs_write,
    mp_afs_ioctl,
    mp_fs_setfl,
    mp_afs_getattr,
    mp_afs_setattr,
    mp_afs_access,
    mp_afs_lookup,
    mp_afs_create,
    mp_afs_remove,
    mp_afs_link,
    mp_afs_rename,
    mp_afs_mkdir,
    mp_afs_rmdir,
    mp_afs_readdir,
    mp_afs_symlink,
    mp_afs_readlink,
    mp_afs_fsync,
    mp_afs_inactive,
    mp_afs_fid,
    mp_afs_fid2,
    mp_afs_rwlock,
    mp_afs_rwunlock,
    mp_afs_seek,
    mp_fs_cmp,
    mp_afs_frlock,
    fs_nosys,			/* realvp */
    mp_afs_bmap,
    mp_afs_strategy,
    mp_afs_map,
#ifdef AFS_SGI65_ENV
    fs_noerr,			/* addmap - devices only */
    fs_noerr,			/* delmap - devices only */
#else
    mp_afs_addmap,
    mp_afs_delmap,
#endif
    mp_fs_poll,			/* poll */
    fs_nosys,			/* dump */
    fs_pathconf,
    fs_nosys,			/* allocstore */
    fs_nosys,			/* fcntl */
    afs_reclaim,		/* reclaim */
    fs_nosys,			/* attr_get */
    fs_nosys,			/* attr_set */
    fs_nosys,			/* attr_remove */
    fs_nosys,			/* attr_list */
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    fs_cover,
    (vop_link_removed_t) fs_noval,
    fs_vnode_change,
    fs_tosspages,
    fs_flushinval_pages,
    fs_flush_pages,
    fs_invalfree_pages,
    fs_pages_sethole,
    (vop_commit_t) fs_nosys,
    (vop_readbuf_t) fs_nosys,
    fs_strgetmsg,
    fs_strputmsg,
#else
    fs_mount,
#endif
#endif
};
struct vnodeops *afs_ops = &Afs_vnodeops;
#endif /* MP */


#if defined(AFS_SGI62_ENV) && defined(AFS_SGI_DUAL_FS_CACHE)
/* Support for EFS and XFS caches. The assumption here is that the size of
 * a cache file also does not exceed 32 bits. 
 */

/* Initialized in osi_InitCacheFSType(). Used to determine inode type. */
int afs_CacheFSType = -1;
vnodeops_t *afs_efs_vnodeopsp;
vnodeops_t *afs_xfs_vnodeopsp;
vnode_t *(*afs_IGetVnode) (ino_t);

extern vnode_t *afs_EFSIGetVnode(ino_t);	/* defined in osi_file.c */
extern vnode_t *afs_XFSIGetVnode(ino_t);	/* defined in osi_file.c */

extern afs_lock_t afs_xosi;	/* lock is for tvattr */

/* Initialize the cache operations. Called while initializing cache files. */
void
afs_InitDualFSCacheOps(struct vnode *vp)
{
    static int inited = 0;
    struct vfssw *swp;
    int found = 0;

    if (inited)
	return;
    inited = 1;

    swp = vfs_getvfssw("xfs");
    if (swp) {
	afs_xfs_vnodeopsp = swp->vsw_vnodeops;
	if (!found) {
	    if (vp && vp->v_op == afs_xfs_vnodeopsp) {
		afs_CacheFSType = AFS_SGI_XFS_CACHE;
		afs_IGetVnode = afs_XFSIGetVnode;
		found = 1;
	    }
	}
    }

    if (vp && !found)
	osi_Panic("osi_InitCacheFSType: Can't find fstype for vnode 0x%llx\n",
		  vp);
}

ino_t
VnodeToIno(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    MObtainWriteLock(&afs_xosi, 579);
    vattr.va_mask = AT_FSID | AT_NODEID;	/* quick return using this mask. */
    AFS_GUNLOCK();
    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    AFS_GLOCK();
    if (code) {
	osi_Panic("VnodeToIno");
    }
    MReleaseWriteLock(&afs_xosi);
    return vattr.va_nodeid;
}

dev_t
VnodeToDev(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    MObtainWriteLock(&afs_xosi, 580);
    vattr.va_mask = AT_FSID | AT_NODEID;	/* quick return using this mask. */
    AFS_GUNLOCK();
    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    AFS_GLOCK();
    if (code) {
	osi_Panic("VnodeToDev");
    }
    MReleaseWriteLock(&afs_xosi);
    return (dev_t) vattr.va_fsid;
}

off_t
VnodeToSize(vnode_t * vp)
{
    int code;
    struct vattr vattr;

    MObtainWriteLock(&afs_xosi, 581);
    vattr.va_mask = AT_SIZE;
    AFS_GUNLOCK();
    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    AFS_GLOCK();
    if (code) {
	osi_Panic("VnodeToSize");
    }
    MReleaseWriteLock(&afs_xosi);
    return vattr.va_size;
}
#endif /* 6.2 and dual fs cache */
#endif /* AFS_SGI62_ENV */
