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

/*
 * SOLARIS/osi_vnodeops.c
 *
 * Implements:
 *
 * Functions: AFS_TRYUP, _init, _info, _fini, afs_addmap, afs_delmap,
 * afs_vmread, afs_vmwrite, afs_getpage, afs_GetOnePage, afs_putpage,
 * afs_putapage, afs_nfsrdwr, afs_map, afs_PageLeft, afs_pathconf/afs_cntl,
 * afs_ioctl, afs_rwlock, afs_rwunlock, afs_seek, afs_space, afs_dump,
 * afs_cmp, afs_realvp, afs_pageio, afs_dumpctl, afs_dispose, afs_setsecattr,
 * afs_getsecattr, gafs_open, gafs_close, gafs_getattr, gafs_setattr,
 * gafs_access, gafs_lookup, gafs_create, gafs_remove, gafs_link,
 * gafs_rename, gafs_mkdir, gafs_rmdir, gafs_readdir, gafs_symlink,
 * gafs_readlink, gafs_fsync, afs_inactive, gafs_inactive, gafs_fid
 *
 *
 * Variables: Afs_vnodeops
 *
 */
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/nfsclient.h"


#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>

/* 
 * XXX Temporary fix for problems with Solaris rw_tryupgrade() lock.
 * It isn't very persistent in getting the upgrade when others are
 * waiting for it and returns 0.  So the UpgradeSToW() macro that the
 * rw_tryupgrade used to map to wasn't good enough and we need to use
 * the following code instead.  Obviously this isn't the proper place
 * for it but it's only called from here for now
 * 
 */
#ifndef	AFS_SUN54_ENV
AFS_TRYUP(lock)
     afs_rwlock_t *lock;
{
    if (!rw_tryupgrade(lock)) {
	rw_exit(lock);
	rw_enter(lock, RW_WRITER);
    }
}
#endif


/* Translate a faultcode_t as returned by some of the vm routines
 * into a suitable errno value.
 */
static int
afs_fc2errno(faultcode_t fc)
{
    switch (FC_CODE(fc)) {
    case 0:
	return 0;

    case FC_OBJERR:
	return FC_ERRNO(fc);

    default:
	return EIO;
    }
}


extern struct as kas;		/* kernel addr space */
extern unsigned char *afs_indexFlags;
extern afs_lock_t afs_xdcache;

/* Additional vnodeops for SunOS 4.0.x */
int afs_nfsrdwr(), afs_getpage(), afs_putpage(), afs_map();
int afs_dump(), afs_cmp(), afs_realvp(), afs_GetOnePage();

int afs_pvn_vptrunc;

int
afs_addmap(avp, offset, asp, addr, length, prot, maxprot, flags, credp)
     register struct vnode *avp;
     offset_t offset;
     struct as *asp;
     caddr_t addr;
     int length, prot, maxprot, flags;
     struct AFS_UCRED *credp;
{
    /* XXX What should we do here?? XXX */
    return (0);
}

int
afs_delmap(avp, offset, asp, addr, length, prot, maxprot, flags, credp)
     register struct vnode *avp;
     offset_t offset;
     struct as *asp;
     caddr_t addr;
     int length, prot, maxprot, flags;
     struct AFS_UCRED *credp;
{
    /* XXX What should we do here?? XXX */
    return (0);
}

#ifdef AFS_SUN510_ENV
int
afs_vmread(avp, auio, ioflag, acred, ct)
     register struct vnode *avp;
     struct uio *auio;
     int ioflag;
     struct AFS_UCRED *acred;
     caller_context_t *ct;
#else
int
afs_vmread(avp, auio, ioflag, acred)
     register struct vnode *avp;
     struct uio *auio;
     int ioflag;
     struct AFS_UCRED *acred;
#endif
{
    register int code;

    if (!RW_READ_HELD(&(VTOAFS(avp))->rwlock))
	osi_Panic("afs_vmread: !rwlock");
    AFS_GLOCK();
    code = afs_nfsrdwr(VTOAFS(avp), auio, UIO_READ, ioflag, acred);
    AFS_GUNLOCK();
    return code;
}


#ifdef AFS_SUN510_ENV
int
afs_vmwrite(avp, auio, ioflag, acred, ct)
     register struct vnode *avp;
     struct uio *auio;
     int ioflag;
     struct AFS_UCRED *acred;
     caller_context_t *ct;
#else
int
afs_vmwrite(avp, auio, ioflag, acred)
     register struct vnode *avp;
     struct uio *auio;
     int ioflag;
     struct AFS_UCRED *acred;
#endif
{
    register int code;

    if (!RW_WRITE_HELD(&(VTOAFS(avp))->rwlock))
	osi_Panic("afs_vmwrite: !rwlock");
    AFS_GLOCK();
    code = afs_nfsrdwr(VTOAFS(avp), auio, UIO_WRITE, ioflag, acred);
    AFS_GUNLOCK();
    return code;
}

int
afs_getpage(vp, off, len, protp, pl, plsz, seg, addr, rw, acred)
     struct vnode *vp;
     u_int len;
     u_int *protp;
     struct page *pl[];
     u_int plsz;
     struct seg *seg;
     offset_t off;
     caddr_t addr;
     enum seg_rw rw;
     struct AFS_UCRED *acred;
{
    register afs_int32 code = 0;
    AFS_STATCNT(afs_getpage);

    if (vp->v_flag & VNOMAP)	/* File doesn't allow mapping */
	return (ENOSYS);

    AFS_GLOCK();

#if	defined(AFS_SUN56_ENV)
    if (len <= PAGESIZE)
	code =
	    afs_GetOnePage(vp, off, len, protp, pl, plsz, seg, addr, rw,
			   acred);
#else
    if (len <= PAGESIZE)
	code =
	    afs_GetOnePage(vp, (u_int) off, len, protp, pl, plsz, seg, addr,
			   rw, acred);
#endif
    else {
	struct vcache *vcp = VTOAFS(vp);
	ObtainWriteLock(&vcp->vlock, 548);
	vcp->multiPage++;
	ReleaseWriteLock(&vcp->vlock);
	afs_BozonLock(&vcp->pvnLock, vcp);
#if	defined(AFS_SUN56_ENV)
	code =
	    pvn_getpages(afs_GetOnePage, vp, off, len, protp, pl, plsz, seg,
			 addr, rw, acred);
#else
	code =
	    pvn_getpages(afs_GetOnePage, vp, (u_int) off, len, protp, pl,
			 plsz, seg, addr, rw, acred);
#endif
	afs_BozonUnlock(&vcp->pvnLock, vcp);
	ObtainWriteLock(&vcp->vlock, 549);
	vcp->multiPage--;
	ReleaseWriteLock(&vcp->vlock);
    }
    AFS_GUNLOCK();
    return code;
}

/* Return all the pages from [off..off+len) in file */
int
afs_GetOnePage(vp, off, alen, protp, pl, plsz, seg, addr, rw, acred)
     u_int alen;
     struct vnode *vp;
#if	defined(AFS_SUN56_ENV)
     u_offset_t off;
#else
     u_int off;
#endif
     u_int *protp;
     struct page *pl[];
     u_int plsz;
     struct seg *seg;
     caddr_t addr;
     enum seg_rw rw;
     struct AFS_UCRED *acred;
{
    register struct page *page;
    register afs_int32 code = 0;
    u_int len;
    struct buf *buf;
    afs_int32 tlen;
    register struct vcache *avc;
    register struct dcache *tdc;
    int i, s, pexists;
    int slot;
    afs_size_t offset = 0, nlen;
    struct vrequest treq;
    afs_int32 mapForRead = 0, Code = 0;
    u_offset_t toffset;

    if (!acred)
	osi_Panic("GetOnePage: !acred");

    avc = VTOAFS(vp);		/* cast to afs vnode */

    if (avc->credp		/*&& AFS_NFSXLATORREQ(acred) */
	&& AFS_NFSXLATORREQ(avc->credp)) {
	acred = avc->credp;
    }
    if (code = afs_InitReq(&treq, acred))
	return code;

    if (!pl) {
	/* This is a read-ahead request, e.g. due to madvise.  */
	int plen = alen;
	ObtainReadLock(&avc->lock);

	while (plen > 0 && !afs_BBusy()) {
	    /* Obtain a dcache entry at off.  2 means don't fetch data. */
	    tdc =
		afs_GetDCache(avc, (afs_offs_t) off, &treq, &offset, &nlen,
			      2);
	    if (!tdc)
		break;

	    /* Write-lock the dcache entry, if we don't succeed, just go on */
	    if (0 != NBObtainWriteLock(&tdc->lock, 642)) {
		afs_PutDCache(tdc);
		goto next_prefetch;
	    }

	    /* If we aren't already fetching this dcache entry, queue it */
	    if (!(tdc->mflags & DFFetchReq)) {
		struct brequest *bp;

		tdc->mflags |= DFFetchReq;
		bp = afs_BQueue(BOP_FETCH, avc, B_DONTWAIT, 0, acred,
				(afs_size_t) off, (afs_size_t) 1, tdc);
		if (!bp) {
		    /* Unable to start background fetch; might as well stop */
		    tdc->mflags &= ~DFFetchReq;
		    ReleaseWriteLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    break;
		}
		ReleaseWriteLock(&tdc->lock);
	    } else {
		ReleaseWriteLock(&tdc->lock);
		afs_PutDCache(tdc);
	    }

	  next_prefetch:
	    /* Adjust our offset and remaining length values */
	    off += nlen;
	    plen -= nlen;

	    /* If we aren't making progress for some reason, bail out */
	    if (nlen <= 0)
		break;
	}

	ReleaseReadLock(&avc->lock);
	return 0;
    }

    len = PAGESIZE;
    pl[0] = NULL;		/* Make sure it's empty */

    /* first, obtain the proper lock for the VM system */

    /* if this is a read request, map the page in read-only.  This will
     * allow us to swap out the dcache entry if there are only read-only
     * pages created for the chunk, which helps a *lot* when dealing
     * with small caches.  Otherwise, we have to invalidate the vm
     * pages for the range covered by a chunk when we swap out the
     * chunk.
     */
    if (rw == S_READ || rw == S_EXEC)
	mapForRead = 1;

    if (protp)
	*protp = PROT_ALL;

  retry:
    if (rw == S_WRITE || rw == S_CREATE)
	tdc = afs_GetDCache(avc, (afs_offs_t) off, &treq, &offset, &nlen, 5);
    else
	tdc = afs_GetDCache(avc, (afs_offs_t) off, &treq, &offset, &nlen, 1);
    if (!tdc)
	return EINVAL;
    code = afs_VerifyVCache(avc, &treq);
    if (code) {
	afs_PutDCache(tdc);
	return afs_CheckCode(code, &treq, 44);	/* failed to get it */
    }

    afs_BozonLock(&avc->pvnLock, avc);
    ObtainReadLock(&avc->lock);

    afs_Trace4(afs_iclSetp, CM_TRACE_PAGEIN, ICL_TYPE_POINTER, (afs_int32) vp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(off), ICL_TYPE_LONG, len,
	       ICL_TYPE_LONG, (int)rw);

    tlen = len;
    slot = 0;
    toffset = off;
    /* Check to see if we're in the middle of a VM purge, and if we are, release
     * the locks and try again when the VM purge is done. */
    ObtainWriteLock(&avc->vlock, 550);
    if (avc->activeV) {
	ReleaseReadLock(&avc->lock);
	ReleaseWriteLock(&avc->vlock);
	afs_BozonUnlock(&avc->pvnLock, avc);
	afs_PutDCache(tdc);
	/* Check activeV again, it may have been turned off
	 * while we were waiting for a lock in afs_PutDCache */
	ObtainWriteLock(&avc->vlock, 574);
	if (avc->activeV) {
	    avc->vstates |= VRevokeWait;
	    ReleaseWriteLock(&avc->vlock);
	    afs_osi_Sleep(&avc->vstates);
	} else {
	    ReleaseWriteLock(&avc->vlock);
	}
	goto retry;
    }
    ReleaseWriteLock(&avc->vlock);

    /* We're about to do stuff with our dcache entry..  Lock it. */
    ObtainReadLock(&tdc->lock);

    /* Check to see whether the cache entry is still valid */
    if (!(avc->states & CStatd)
	|| !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&avc->lock);
	afs_BozonUnlock(&avc->pvnLock, avc);
	afs_PutDCache(tdc);
	goto retry;
    }

    AFS_GUNLOCK();
    while (1) {			/* loop over all pages */
	/* now, try to find the page in memory (it may already be intransit or laying
	 * around the free list */
	page =
	    page_lookup(vp, toffset, (rw == S_CREATE ? SE_EXCL : SE_SHARED));
	if (page)
	    goto nextpage;

	/* if we make it here, we can't find the page in memory.  Do a real disk read
	 * from the cache to get the data */
	Code |= 0x200;		/* XXX */
#if	defined(AFS_SUN54_ENV)
	/* use PG_EXCL because we know the page does not exist already.  If it 
	 * actually does exist, we have somehow raced between lookup and create.
	 * As of 4/98, that shouldn't be possible, but we'll be defensive here
	 * in case someone tries to relax all the serialization of read and write
	 * operations with harmless things like stat. */
#if    defined(AFS_SUN58_ENV)
	page =
	    page_create_va(vp, toffset, PAGESIZE, PG_WAIT | PG_EXCL, seg,
			   addr);
#else
	page =
	    page_create_va(vp, toffset, PAGESIZE, PG_WAIT | PG_EXCL,
			   seg->s_as, addr);
#endif
#else
	page = page_create(vp, toffset, PAGESIZE, PG_WAIT);
#endif
	if (!page) {
	    continue;
	}
	if (alen < PAGESIZE)
	    pagezero(page, alen, PAGESIZE - alen);

	if (rw == S_CREATE) {
	    /* XXX Don't read from AFS in write only cases XXX */
	    page_io_unlock(page);
	} else
	{
	    /* now it is time to start I/O operation */
	    buf = pageio_setup(page, PAGESIZE, vp, B_READ);	/* allocate a buf structure */
	    buf->b_edev = 0;
	    buf->b_dev = 0;
#if defined(AFS_SUN56_ENV)
	    buf->b_lblkno = lbtodb(toffset);
#else
	    buf->b_blkno = btodb(toffset);
#endif
	    bp_mapin(buf);	/* map it in to our address space */

	    AFS_GLOCK();
	    /* afs_ustrategy will want to lock the dcache entry */
	    ReleaseReadLock(&tdc->lock);
	    code = afs_ustrategy(buf, acred);	/* do the I/O */
	    ObtainReadLock(&tdc->lock);
	    AFS_GUNLOCK();

	    /* Before freeing unmap the buffer */
	    bp_mapout(buf);
	    pageio_done(buf);
	    if (code) {
		goto bad;
	    }
	    page_io_unlock(page);
	}

	/* come here when we have another page (already held) to enter */
      nextpage:
	/* put page in array and continue */
	/* The p_selock must be downgraded to a shared lock after the page is read */
#if	defined(AFS_SUN56_ENV)
	if ((rw != S_CREATE) && !(PAGE_SHARED(page)))
#else
	if ((rw != S_CREATE) && !(se_shared_assert(&page->p_selock)))
#endif
	{
	    page_downgrade(page);
	}
	pl[slot++] = page;
	code = page_iolock_assert(page);
	code = 0;
	toffset += PAGESIZE;
	addr += PAGESIZE;
	tlen -= PAGESIZE;
	if (tlen <= 0)
	    break;		/* done all the pages */
    }				/* while (1) ... */

    AFS_GLOCK();
    pl[slot] = NULL;
    ReleaseReadLock(&tdc->lock);

    /* Prefetch next chunk if we're at a chunk boundary */
    if (AFS_CHUNKOFFSET(off) == 0) {
	if (!(tdc->mflags & DFNextStarted))
	    afs_PrefetchChunk(avc, tdc, acred, &treq);
    }

    ReleaseReadLock(&avc->lock);
    ObtainWriteLock(&afs_xdcache, 246);
    if (!mapForRead) {
	/* track that we have dirty (or dirty-able) pages for this chunk. */
	afs_indexFlags[tdc->index] |= IFDirtyPages;
    }
    afs_indexFlags[tdc->index] |= IFAnyPages;
    ReleaseWriteLock(&afs_xdcache);
    afs_BozonUnlock(&avc->pvnLock, avc);
    afs_PutDCache(tdc);
    afs_Trace3(afs_iclSetp, CM_TRACE_PAGEINDONE, ICL_TYPE_LONG, code,
	       ICL_TYPE_LONG, (int)page, ICL_TYPE_LONG, Code);
    return 0;

  bad:
    AFS_GLOCK();
    afs_Trace3(afs_iclSetp, CM_TRACE_PAGEINDONE, ICL_TYPE_LONG, code,
	       ICL_TYPE_LONG, (int)page, ICL_TYPE_LONG, Code);
    /* release all pages, drop locks, return code */
    if (page)
	pvn_read_done(page, B_ERROR);
    ReleaseReadLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    return code;
}

int
afs_putpage(vp, off, len, flags, cred)
     struct vnode *vp;
     offset_t off;
     u_int len;
     int flags;
     struct AFS_UCRED *cred;
{
    struct vcache *avc;
    struct page *pages;
    afs_int32 code = 0;
#if    defined(AFS_SUN58_ENV)
    size_t tlen;
#else
    afs_int32 tlen;
#endif
    afs_offs_t endPos;
    afs_int32 NPages = 0;
#if	defined(AFS_SUN56_ENV)
    u_offset_t toff = off;
#else
    int toff = (int)off;
#endif
    int didWriteLock;

    AFS_STATCNT(afs_putpage);
    if (vp->v_flag & VNOMAP)	/* file doesn't allow mapping */
	return (ENOSYS);

    /*
     * Putpage (ASYNC) is called every sec to flush out dirty vm pages 
     */
    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_PAGEOUT, ICL_TYPE_POINTER,
	       (afs_int32) vp, ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(off),
	       ICL_TYPE_INT32, (afs_int32) len, ICL_TYPE_LONG, (int)flags);
    avc = VTOAFS(vp);
    afs_BozonLock(&avc->pvnLock, avc);
    ObtainSharedLock(&avc->lock, 247);
    didWriteLock = 0;

    /* Get a list of modified (or whatever) pages */
    if (len) {
	endPos = (afs_offs_t) off + len;	/* position we're supposed to write up to */
	while ((afs_offs_t) toff < endPos
	       && (afs_offs_t) toff < avc->m.Length) {
	    /* If not invalidating pages use page_lookup_nowait to avoid reclaiming
	     * them from the free list
	     */
	    AFS_GUNLOCK();
	    if (flags & (B_FREE | B_INVAL))
		pages = page_lookup(vp, toff, SE_EXCL);
	    else
		pages = page_lookup_nowait(vp, toff, SE_SHARED);
	    if (!pages || !pvn_getdirty(pages, flags))
		tlen = PAGESIZE;
	    else {
		if (!didWriteLock) {
		    AFS_GLOCK();
		    didWriteLock = 1;
		    UpgradeSToWLock(&avc->lock, 671);
		    AFS_GUNLOCK();
		}
		NPages++;
		code = afs_putapage(vp, pages, &toff, &tlen, flags, cred);
		if (code) {
		    AFS_GLOCK();
		    break;
		}
	    }
	    toff += tlen;
	    AFS_GLOCK();
	}
    } else {
	if (!didWriteLock) {
	    UpgradeSToWLock(&avc->lock, 670);
	    didWriteLock = 1;
	}

	AFS_GUNLOCK();
#if	defined(AFS_SUN56_ENV)
	code = pvn_vplist_dirty(vp, toff, afs_putapage, flags, cred);
#else
	code = pvn_vplist_dirty(vp, (u_int) off, afs_putapage, flags, cred);
#endif
	AFS_GLOCK();
    }

    if (code && !avc->vc_error) {
	if (!didWriteLock) {
	    UpgradeSToWLock(&avc->lock, 669);
	    didWriteLock = 1;
	}
	avc->vc_error = code;
    }

    if (didWriteLock)
	ReleaseWriteLock(&avc->lock);
    else
	ReleaseSharedLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    afs_Trace2(afs_iclSetp, CM_TRACE_PAGEOUTDONE, ICL_TYPE_LONG, code,
	       ICL_TYPE_LONG, NPages);
    AFS_GUNLOCK();
    return (code);
}


int
afs_putapage(struct vnode *vp, struct page *pages,
#if	defined(AFS_SUN56_ENV)
	     u_offset_t * offp,
#else
	     u_int * offp,
#endif
#if    defined(AFS_SUN58_ENV)
	     size_t * lenp,
#else
	     u_int * lenp,
#endif
	     int flags, struct AFS_UCRED *credp)
{
    struct buf *tbuf;
    struct vcache *avc = VTOAFS(vp);
    afs_int32 code = 0;
    u_int tlen = PAGESIZE;
    afs_offs_t off = (pages->p_offset / PAGESIZE) * PAGESIZE;

    /*
     * Now we've got the modified pages.  All pages are locked and held 
     * XXX Find a kluster that fits in one block (or page). We also
     * adjust the i/o if the file space is less than a while page. XXX
     */
    if (off + tlen > avc->m.Length) {
	tlen = avc->m.Length - off;
    }
    /* can't call mapout with 0 length buffers (rmfree panics) */
    if (((tlen >> 24) & 0xff) == 0xff) {
	tlen = 0;
    }
    if ((int)tlen > 0) {
	/*
	 * Can't call mapout with 0 length buffers since we'll get rmfree panics
	 */
	tbuf = pageio_setup(pages, tlen, vp, B_WRITE | flags);
	if (!tbuf)
	    return (ENOMEM);

	tbuf->b_dev = 0;
#if defined(AFS_SUN56_ENV)
	tbuf->b_lblkno = lbtodb(pages->p_offset);
#else
	tbuf->b_blkno = btodb(pages->p_offset);
#endif
	bp_mapin(tbuf);
	AFS_GLOCK();
	afs_Trace4(afs_iclSetp, CM_TRACE_PAGEOUTONE, ICL_TYPE_LONG, avc,
		   ICL_TYPE_LONG, pages, ICL_TYPE_LONG, tlen, ICL_TYPE_OFFSET,
		   ICL_HANDLE_OFFSET(off));
	code = afs_ustrategy(tbuf, credp);	/* unlocks page */
	AFS_GUNLOCK();
	bp_mapout(tbuf);
    }
    pvn_write_done(pages, ((code) ? B_ERROR : 0) | B_WRITE | flags);
    if ((int)tlen > 0)
	pageio_done(tbuf);
    if (offp)
	*offp = off;
    if (lenp)
	*lenp = tlen;
    return code;
}

int
afs_nfsrdwr(avc, auio, arw, ioflag, acred)
     register struct vcache *avc;
     struct uio *auio;
     enum uio_rw arw;
     int ioflag;
     struct AFS_UCRED *acred;
{
    register afs_int32 code;
    afs_int32 code2;
    int counter;
    afs_int32 mode, sflags;
    register char *data;
    struct dcache *dcp, *dcp_newpage;
    afs_size_t fileBase, size;
    afs_size_t pageBase;
    register afs_int32 tsize;
    register afs_int32 pageOffset, extraResid = 0;
    register afs_size_t origLength;	/* length when reading/writing started */
    register long appendLength;	/* length when this call will finish */
    int created;		/* created pages instead of faulting them */
    int lockCode;
    int didFakeOpen, eof;
    struct vrequest treq;
    caddr_t raddr;
    u_int rsize;

    AFS_STATCNT(afs_nfsrdwr);

    /* can't read or write other things */
    if (vType(avc) != VREG)
	return EISDIR;

    if (auio->uio_resid == 0)
	return (0);

    afs_Trace4(afs_iclSetp, CM_TRACE_VMRW, ICL_TYPE_POINTER, (afs_int32) avc,
	       ICL_TYPE_LONG, (arw == UIO_WRITE ? 1 : 0), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(auio->uio_loffset), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(auio->uio_resid));

#ifndef AFS_64BIT_CLIENT
    if (AfsLargeFileUio(auio))	/* file is larger than 2 GB */
	return (EFBIG);
#endif

    if (!acred)
	osi_Panic("rdwr: !acred");

    if (code = afs_InitReq(&treq, acred))
	return code;

    /* It's not really possible to know if a write cause a growth in the
     * cache size, we we wait for a cache drain for any write.
     */
    afs_MaybeWakeupTruncateDaemon();
    while ((arw == UIO_WRITE)
	   && (afs_blocksUsed > PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks))) {
	if (afs_blocksUsed - afs_blocksDiscarded >
	    PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
	    afs_WaitForCacheDrain = 1;
	    afs_osi_Sleep(&afs_WaitForCacheDrain);
	}
	afs_MaybeFreeDiscardedDCache();
	afs_MaybeWakeupTruncateDaemon();
    }
    code = afs_VerifyVCache(avc, &treq);
    if (code)
	return afs_CheckCode(code, &treq, 45);

    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc, acred);	/* hold bozon lock, but not basic vnode lock */

    ObtainWriteLock(&avc->lock, 250);

    /* adjust parameters when appending files */
    if ((ioflag & IO_APPEND) && arw == UIO_WRITE) {
#if defined(AFS_SUN56_ENV)
	auio->uio_loffset = avc->m.Length;	/* write at EOF position */
#else
	auio->uio_offset = avc->m.Length;	/* write at EOF position */
#endif
    }
    if (auio->afsio_offset < 0 || (auio->afsio_offset + auio->uio_resid) < 0) {
	ReleaseWriteLock(&avc->lock);
	afs_BozonUnlock(&avc->pvnLock, avc);
	return EINVAL;
    }
#ifndef AFS_64BIT_CLIENT
    /* file is larger than 2GB */
    if (AfsLargeFileSize(auio->uio_offset, auio->uio_resid)) {
	ReleaseWriteLock(&avc->lock);
	afs_BozonUnlock(&avc->pvnLock, avc);
	return EFBIG;
    }
#endif

    didFakeOpen = 0;		/* keep track of open so we can do close */
    if (arw == UIO_WRITE) {
	/* do ulimit processing; shrink resid or fail */
#if	defined(AFS_SUN56_ENV)
	if (auio->uio_loffset + auio->afsio_resid > auio->uio_llimit) {
	    if (auio->uio_loffset >= auio->uio_llimit) {
		ReleaseWriteLock(&avc->lock);
		afs_BozonUnlock(&avc->pvnLock, avc);
		return EFBIG;
	    } else {
		/* track # of bytes we should write, but won't because of
		 * ulimit; we must add this into the final resid value
		 * so caller knows we punted some data.
		 */
		extraResid = auio->uio_resid;
		auio->uio_resid = auio->uio_llimit - auio->uio_loffset;
		extraResid -= auio->uio_resid;
	    }
	}
#else
#ifdef	AFS_SUN52_ENV
	if (auio->afsio_offset + auio->afsio_resid > auio->uio_limit) {
	    if (auio->afsio_offset >= auio->uio_limit) {
		ReleaseWriteLock(&avc->lock);
		afs_BozonUnlock(&avc->pvnLock, avc);
		return EFBIG;
	    } else {
		/* track # of bytes we should write, but won't because of
		 * ulimit; we must add this into the final resid value
		 * so caller knows we punted some data.
		 */
		extraResid = auio->uio_resid;
		auio->uio_resid = auio->uio_limit - auio->afsio_offset;
		extraResid -= auio->uio_resid;
	    }
	}
#endif
#endif /* SUN56 */
	mode = S_WRITE;		/* segment map-in mode */
	afs_FakeOpen(avc);	/* do this for writes, so data gets put back
				 * when we want it to be put back */
	didFakeOpen = 1;	/* we'll be doing a fake open */
	/* before starting any I/O, we must ensure that the file is big enough
	 * to hold the results (since afs_putpage will be called to force the I/O */
	size = auio->afsio_resid + auio->afsio_offset;	/* new file size */
	appendLength = size;
	origLength = avc->m.Length;
	if (size > avc->m.Length) {
	    afs_Trace4(afs_iclSetp, CM_TRACE_SETLENGTH, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_LONG, __LINE__, ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(avc->m.Length), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(size));
	    avc->m.Length = size;	/* file grew */
	}
	avc->states |= CDirty;	/* Set the dirty bit */
	avc->m.Date = osi_Time();	/* Set file date (for ranlib) */
    } else {
	mode = S_READ;		/* map-in read-only */
	origLength = avc->m.Length;
    }

    if (acred && AFS_NFSXLATORREQ(acred)) {
	if (arw == UIO_READ) {
	    if (!afs_AccessOK
		(avc, PRSFS_READ, &treq,
		 CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
		ReleaseWriteLock(&avc->lock);
		afs_BozonUnlock(&avc->pvnLock, avc);
		return EACCES;
	    }
	}
	crhold(acred);
	if (avc->credp) {
	    crfree(avc->credp);
	}
	avc->credp = acred;
    }
    counter = 0;		/* don't call afs_DoPartialWrite first time through. */
    while (1) {
	/* compute the amount of data to move into this block,
	 * based on auio->afsio_resid.  Note that we copy data in units of
	 * MAXBSIZE, not PAGESIZE.  This is because segmap_getmap panics if you
	 * call it with an offset based on blocks smaller than MAXBSIZE
	 * (implying that it should be named BSIZE, since it is clearly both a
	 * max and a min). */
	size = auio->afsio_resid;	/* transfer size */
	fileBase = auio->afsio_offset;	/* start file position for xfr */
	pageBase = fileBase & ~(MAXBSIZE - 1);	/* file position of the page */
	pageOffset = fileBase & (MAXBSIZE - 1);	/* xfr start's offset within page */
	tsize = MAXBSIZE - pageOffset;	/* how much more fits in this page */
	/* we'll read tsize bytes, but first must make sure tsize isn't too big */
	if (tsize > size)
	    tsize = size;	/* don't read past end of request */
	eof = 0;		/* flag telling us if we hit the EOF on the read */
	if (arw == UIO_READ) {	/* we're doing a read operation */
	    /* don't read past EOF */
	    if (fileBase + tsize > origLength) {
		tsize = origLength - fileBase;
		eof = 1;	/* we did hit the EOF */
		if (tsize < 0)
		    tsize = 0;	/* better safe than sorry */
	    }
	    sflags = 0;
	} else {
	    /* Purge dirty chunks of file if there are too many dirty
	     * chunks. Inside the write loop, we only do this at a chunk
	     * boundary. Clean up partial chunk if necessary at end of loop.
	     */
	    if (counter > 0 && code == 0 && AFS_CHUNKOFFSET(fileBase) == 0) {
		code = afs_DoPartialWrite(avc, &treq);
		if (code)
		    break;
	    }
	    /* write case, we ask segmap_release to call putpage.  Really, we
	     * don't have to do this on every page mapin, but for now we're
	     * lazy, and don't modify the rest of AFS to scan for modified
	     * pages on a close or other "synchronize with file server"
	     * operation.  This makes things a little cleaner, but probably
	     * hurts performance. */
	    sflags = SM_WRITE;
	}
	if (tsize <= 0) {
	    code = 0;
	    break;		/* nothing to transfer, we're done */
	}
	if (arw == UIO_WRITE)
	    avc->states |= CDirty;	/* may have been cleared by DoPartialWrite */

	/* Before dropping lock, hold the chunk (create it if necessary).  This
	 * serves two purposes:  (1) Ensure Cache Truncate Daemon doesn't try
	 * to purge the chunk's pages while we have them locked.  This would
	 * cause deadlock because we might be waiting for the CTD to free up
	 * a chunk.  (2)  If we're writing past the original EOF, and we're
	 * at the base of the chunk, then make sure it exists online
	 * before we do the uiomove, since the segmap_release will
	 * write out to the chunk, causing it to get fetched if it hasn't
	 * been created yet.  The code that would otherwise notice that
	 * we're fetching a chunk past EOF won't work, since we've
	 * already adjusted the file size above.
	 */
	ObtainWriteLock(&avc->vlock, 551);
	while (avc->vstates & VPageCleaning) {
	    ReleaseWriteLock(&avc->vlock);
	    ReleaseWriteLock(&avc->lock);
	    afs_osi_Sleep(&avc->vstates);
	    ObtainWriteLock(&avc->lock, 334);
	    ObtainWriteLock(&avc->vlock, 552);
	}
	ReleaseWriteLock(&avc->vlock);
	{
	    afs_size_t toff, tlen;
	    dcp = afs_GetDCache(avc, fileBase, &treq, &toff, &tlen, 2);
	    if (!dcp) {
		code = ENOENT;
		break;
	    }
	}
	ReleaseWriteLock(&avc->lock);	/* uiomove may page fault */
	AFS_GUNLOCK();
#if	defined(AFS_SUN56_ENV)
	data = segmap_getmap(segkmap, AFSTOV(avc), (u_offset_t) pageBase);
	raddr = (caddr_t) (((uintptr_t) data + pageOffset) & PAGEMASK);
#else
	data = segmap_getmap(segkmap, AFSTOV(avc), pageBase);
	raddr = (caddr_t) (((u_int) data + pageOffset) & PAGEMASK);
#endif
	rsize =
	    (((u_int) data + pageOffset + tsize + PAGEOFFSET) & PAGEMASK) -
	    (u_int) raddr;
	if (code == 0) {
	    /* if we're doing a write, and we're starting at the rounded
	     * down page base, and we're writing enough data to cover all
	     * created pages, then we must be writing all of the pages
	     * in this MAXBSIZE window that we're creating.
	     */
	    created = 0;
	    if (arw == UIO_WRITE && ((long)raddr == (long)data + pageOffset)
		&& tsize >= rsize) {
		/* probably the dcache backing this guy is around, but if
		 * not, we can't do this optimization, since we're creating
		 * writable pages, which must be backed by a chunk.
		 */
		AFS_GLOCK();
		dcp_newpage = afs_FindDCache(avc, pageBase);
		if (dcp_newpage
		    && hsame(avc->m.DataVersion, dcp_newpage->f.versionNo)) {
		    ObtainWriteLock(&avc->lock, 251);
		    ObtainWriteLock(&avc->vlock, 576);
		    ObtainReadLock(&dcp_newpage->lock);
		    if ((avc->activeV == 0)
			&& hsame(avc->m.DataVersion, dcp_newpage->f.versionNo)
			&& !(dcp_newpage->dflags & (DFFetching))) {
			AFS_GUNLOCK();
			segmap_pagecreate(segkmap, raddr, rsize, 1);
			AFS_GLOCK();
			ObtainWriteLock(&afs_xdcache, 252);
			/* Mark the pages as created and dirty */
			afs_indexFlags[dcp_newpage->index]
			    |= (IFAnyPages | IFDirtyPages);
			ReleaseWriteLock(&afs_xdcache);
			created = 1;
		    }
		    ReleaseReadLock(&dcp_newpage->lock);
		    afs_PutDCache(dcp_newpage);
		    ReleaseWriteLock(&avc->vlock);
		    ReleaseWriteLock(&avc->lock);
		} else if (dcp_newpage)
		    afs_PutDCache(dcp_newpage);
		AFS_GUNLOCK();
	    }
	    if (!created)
		code =
		    afs_fc2errno(segmap_fault
				 (kas.a_hat, segkmap, raddr, rsize,
				  F_SOFTLOCK, mode));
	}
	if (code == 0) {
	    AFS_UIOMOVE(data + pageOffset, tsize, arw, auio, code);
	    segmap_fault(kas.a_hat, segkmap, raddr, rsize, F_SOFTUNLOCK,
			 mode);
	}
	if (code == 0) {
	    code = segmap_release(segkmap, data, sflags);
	} else {
	    (void)segmap_release(segkmap, data, 0);
	}
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 253);
	counter++;
	if (dcp)
	    afs_PutDCache(dcp);
	if (code)
	    break;
    }
    if (didFakeOpen) {
	afs_FakeClose(avc, acred);
    }
    if (arw == UIO_WRITE && (avc->states & CDirty)) {
	code2 = afs_DoPartialWrite(avc, &treq);
	if (!code)
	    code = code2;
    }

    if (!code && avc->vc_error) {
	code = avc->vc_error;
    }
    ReleaseWriteLock(&avc->lock);
    afs_BozonUnlock(&avc->pvnLock, avc);
    if (!code) {
#ifdef	AFS_SUN53_ENV
	if ((ioflag & FSYNC) && (arw == UIO_WRITE)
	    && !AFS_NFSXLATORREQ(acred))
	    code = afs_fsync(avc, 0, acred);
#else
	if ((ioflag & IO_SYNC) && (arw == UIO_WRITE)
	    && !AFS_NFSXLATORREQ(acred))
	    code = afs_fsync(avc, acred);
#endif
    }
#ifdef	AFS_SUN52_ENV
    /* 
     * If things worked, add in as remaining in request any bytes
     * we didn't write due to file size ulimit.
     */
    if (code == 0 && extraResid > 0)
	auio->uio_resid += extraResid;
#endif
    return afs_CheckCode(code, &treq, 46);
}

afs_map(vp, off, as, addr, len, prot, maxprot, flags, cred)
     struct vnode *vp;
     struct as *as;
     offset_t off;
     caddr_t *addr;
     u_int len;
     u_char prot, maxprot;
     u_int flags;
     struct AFS_UCRED *cred;
{
    struct segvn_crargs crargs;
    register afs_int32 code;
    struct vrequest treq;
    register struct vcache *avc = VTOAFS(vp);

    AFS_STATCNT(afs_map);


    /* check for reasonableness on segment bounds; apparently len can be < 0 */
    if (off < 0 || off + len < 0) {
	return (EINVAL);
    }
#ifndef AFS_64BIT_CLIENT
    if (AfsLargeFileSize(off, len)) {	/* file is larger than 2 GB */
	code = EFBIG;
	goto out;
    }
#endif

    if (vp->v_flag & VNOMAP)	/* File isn't allowed to be mapped */
	return (ENOSYS);

    if (vp->v_filocks)		/* if locked, disallow mapping */
	return (EAGAIN);

    AFS_GLOCK();
    if (code = afs_InitReq(&treq, cred))
	goto out;

    if (vp->v_type != VREG) {
	code = ENODEV;
	goto out;
    }

    code = afs_VerifyVCache(avc, &treq);
    if (code) {
	goto out;
    }
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc, cred);	/* ensure old pages are gone */
    avc->states |= CMAPPED;	/* flag cleared at afs_inactive */
    afs_BozonUnlock(&avc->pvnLock, avc);

    AFS_GUNLOCK();
    as_rangelock(as);
    if ((flags & MAP_FIXED) == 0) {
#if	defined(AFS_SUN57_ENV)
	map_addr(addr, len, off, 1, flags);
#elif	defined(AFS_SUN56_ENV)
	map_addr(addr, len, off, 1);
#else
	map_addr(addr, len, (off_t) off, 1);
#endif
	if (*addr == NULL) {
	    as_rangeunlock(as);
	    code = ENOMEM;
	    goto out1;
	}
    } else
	(void)as_unmap(as, *addr, len);	/* unmap old address space use */
    /* setup the create parameter block for the call */
    crargs.vp = AFSTOV(avc);
    crargs.offset = (u_int) off;
    crargs.cred = cred;
    crargs.type = flags & MAP_TYPE;
    crargs.prot = prot;
    crargs.maxprot = maxprot;
    crargs.amp = (struct anon_map *)0;
    crargs.flags = flags & ~MAP_TYPE;

    code = as_map(as, *addr, len, segvn_create, (char *)&crargs);
    as_rangeunlock(as);
  out1:
    AFS_GLOCK();
    code = afs_CheckCode(code, &treq, 47);
    AFS_GUNLOCK();
    return code;
  out:
    code = afs_CheckCode(code, &treq, 48);
    AFS_GUNLOCK();
    return code;
}


/*
 * For Now We use standard local kernel params for AFS system values. Change this
 * at some point.
 */
afs_pathconf(vp, cmd, outdatap, credp)
     register struct AFS_UCRED *credp;
     struct vnode *vp;
     int cmd;
     u_long *outdatap;
{
    AFS_STATCNT(afs_cntl);
    switch (cmd) {
    case _PC_LINK_MAX:
	*outdatap = MAXLINK;
	break;
    case _PC_NAME_MAX:
	*outdatap = MAXNAMLEN;
	break;
    case _PC_PATH_MAX:
	*outdatap = MAXPATHLEN;
	break;
    case _PC_CHOWN_RESTRICTED:
	*outdatap = 1;
	break;
    case _PC_NO_TRUNC:
	*outdatap = 1;
	break;
    default:
	return EINVAL;
    }
    return 0;
}

afs_ioctl(vnp, com, arg, flag, credp, rvalp)
     struct vnode *vnp;
     int com, arg, flag;
     cred_t *credp;
     int *rvalp;
{
    return (ENOTTY);
}

void
afs_rwlock(vnp, wlock)
     struct vnode *vnp;
     int wlock;
{
    rw_enter(&(VTOAFS(vnp))->rwlock, (wlock ? RW_WRITER : RW_READER));
}


void
afs_rwunlock(vnp, wlock)
     struct vnode *vnp;
     int wlock;
{
    rw_exit(&(VTOAFS(vnp))->rwlock);
}


/* NOT SUPPORTED */
afs_seek(vnp, ooff, noffp)
     struct vnode *vnp;
     offset_t ooff;
     offset_t *noffp;
{
    register int code = 0;

#ifndef AFS_64BIT_CLIENT
# define __MAXOFF_T MAXOFF_T
#else
# define __MAXOFF_T MAXOFFSET_T
#endif

    if ((*noffp < 0 || *noffp > __MAXOFF_T))
	code = EINVAL;
    return code;
}

int
afs_frlock(vnp, cmd, ap, flag, off,
#ifdef AFS_SUN59_ENV
	   flkcb,
#endif
	   credp)
     struct vnode *vnp;
     int cmd;
#if	defined(AFS_SUN56_ENV)
     struct flock64 *ap;
#else
     struct flock *ap;
#endif
     int flag;
     offset_t off;
#ifdef AFS_SUN59_ENV
     struct flk_callback *flkcb;
#endif
     struct AFS_UCRED *credp;
{
    register afs_int32 code = 0;
    /*
     * Implement based on afs_lockctl
     */
    AFS_GLOCK();
#ifdef AFS_SUN59_ENV
    if (flkcb)
	afs_warn("Don't know how to deal with flk_callback's!\n");
#endif
    if ((cmd == F_GETLK) || (cmd == F_O_GETLK) || (cmd == F_SETLK)
	|| (cmd == F_SETLKW)) {
#ifdef	AFS_SUN53_ENV
	ap->l_pid = ttoproc(curthread)->p_pid;
	ap->l_sysid = 0;
#else
	ap->l_pid = ttoproc(curthread)->p_epid;
	ap->l_sysid = ttoproc(curthread)->p_sysid;
#endif

	AFS_GUNLOCK();
#ifdef	AFS_SUN56_ENV
	code = convoff(vnp, ap, 0, off);
#else
	code = convoff(vnp, ap, 0, (off_t) off);
#endif
	if (code)
	    return code;
	AFS_GLOCK();
    }

    code = afs_lockctl(VTOAFS(vnp), ap, cmd, credp);
    AFS_GUNLOCK();
    return code;
}


int
afs_space(vnp, cmd, ap, flag, off, credp)
     struct vnode *vnp;
     int cmd;
#if	defined(AFS_SUN56_ENV)
     struct flock64 *ap;
#else
     struct flock *ap;
#endif
     int flag;
     offset_t off;
     struct AFS_UCRED *credp;
{
    register afs_int32 code = EINVAL;
    struct vattr vattr;

    if ((cmd == F_FREESP)
#ifdef	AFS_SUN56_ENV
	&& ((code = convoff(vnp, ap, 0, off)) == 0)) {
#else
	&& ((code = convoff(vnp, ap, 0, (off_t) off)) == 0)) {
#endif
	AFS_GLOCK();
	if (!ap->l_len) {
	    vattr.va_mask = AT_SIZE;
	    vattr.va_size = ap->l_start;
	    code = afs_setattr(VTOAFS(vnp), &vattr, 0, credp);
	}
	AFS_GUNLOCK();
    }
    return (code);
}

int
afs_dump(vp, addr, i1, i2)
     struct vnode *vp;
     caddr_t addr;
     int i1, i2;
{
    AFS_STATCNT(afs_dump);
    afs_warn("AFS_DUMP. MUST IMPLEMENT THIS!!!\n");
    return EINVAL;
}


/* Nothing fancy here; just compare if vnodes are identical ones */
afs_cmp(vp1, vp2)
     struct vnode *vp1, *vp2;
{
    AFS_STATCNT(afs_cmp);
    return (vp1 == vp2);
}


int
afs_realvp(struct vnode *vp, struct vnode **vpp)
{
    AFS_STATCNT(afs_realvp);
    return EINVAL;
}


int
afs_pageio(vp, pp, ui1, ui2, i1, credp)
     struct vnode *vp;
     struct page *pp;
     u_int ui1, ui2;
     int i1;
     struct cred *credp;
{
    afs_warn("afs_pageio: Not implemented\n");
    return EINVAL;
}

int
afs_dumpctl(vp, i
#ifdef AFS_SUN59_ENV
	    , blkp
#endif
    )
     struct vnode *vp;
     int i;
#ifdef AFS_SUN59_ENV
     int *blkp;
#endif
{
    afs_warn("afs_dumpctl: Not implemented\n");
    return EINVAL;
}

#ifdef	AFS_SUN54_ENV
extern void
afs_dispose(vp, p, fl, dn, cr)
     struct vnode *vp;
     struct page *p;
     int fl, dn;
     struct cred *cr;
{
    fs_dispose(vp, p, fl, dn, cr);
}

int
afs_setsecattr(vp, vsecattr, flag, creds)
     struct vnode *vp;
     vsecattr_t *vsecattr;
     int flag;
     struct cred *creds;
{
    return ENOSYS;
}

int
afs_getsecattr(vp, vsecattr, flag, creds)
     struct vnode *vp;
     vsecattr_t *vsecattr;
     int flag;
     struct cred *creds;
{
    return fs_fab_acl(vp, vsecattr, flag, creds);
}
#endif

#ifdef	AFS_GLOBAL_SUNLOCK
extern int gafs_open(), gafs_close(), afs_ioctl(), gafs_access();
extern int gafs_getattr(), gafs_setattr(), gafs_lookup(), gafs_create();
extern int gafs_remove(), gafs_link(), gafs_rename(), gafs_mkdir();
extern int gafs_rmdir(), gafs_readdir(), gafs_fsync(), gafs_symlink();
extern int gafs_fid(), gafs_readlink(), fs_setfl(), afs_pathconf();
extern int afs_lockctl();
extern void gafs_inactive();

#ifdef AFS_SUN510_ENV
struct fs_operation_def afs_vnodeops_template[] = {
    { VOPNAME_OPEN,		gafs_open },
    { VOPNAME_CLOSE,		gafs_close },
    { VOPNAME_READ,		afs_vmread },
    { VOPNAME_WRITE,		afs_vmwrite },
    { VOPNAME_IOCTL,		afs_ioctl },
    { VOPNAME_SETFL,		fs_setfl },
    { VOPNAME_GETATTR,		gafs_getattr },
    { VOPNAME_SETATTR,		gafs_setattr },
    { VOPNAME_ACCESS,		gafs_access },
    { VOPNAME_LOOKUP,		gafs_lookup },
    { VOPNAME_CREATE,		gafs_create },
    { VOPNAME_REMOVE,		gafs_remove },
    { VOPNAME_LINK,		gafs_link },
    { VOPNAME_RENAME,		gafs_rename },
    { VOPNAME_MKDIR,		gafs_mkdir },
    { VOPNAME_RMDIR,		gafs_rmdir },
    { VOPNAME_READDIR,		gafs_readdir },
    { VOPNAME_SYMLINK,		gafs_symlink },   
    { VOPNAME_READLINK,		gafs_readlink },
    { VOPNAME_FSYNC,		gafs_fsync },
    { VOPNAME_INACTIVE,		gafs_inactive },
    { VOPNAME_FID,		gafs_fid },
    { VOPNAME_RWLOCK,		afs_rwlock },
    { VOPNAME_RWUNLOCK,		afs_rwunlock },
    { VOPNAME_SEEK,		afs_seek },
    { VOPNAME_CMP,		afs_cmp },
    { VOPNAME_FRLOCK,		afs_frlock },
    { VOPNAME_SPACE,		afs_space },
    { VOPNAME_REALVP,		afs_realvp },
    { VOPNAME_GETPAGE,		afs_getpage },
    { VOPNAME_PUTPAGE,		afs_putpage },
    { VOPNAME_MAP,		afs_map },
    { VOPNAME_ADDMAP,		afs_addmap },
    { VOPNAME_DELMAP,		afs_delmap },
    { VOPNAME_POLL,		fs_poll },
    { VOPNAME_DUMP,		afs_dump },
    { VOPNAME_PATHCONF,		afs_pathconf },
    { VOPNAME_PAGEIO,		afs_pageio },
    { VOPNAME_DUMPCTL,		afs_dumpctl },   
    { VOPNAME_DISPOSE,		afs_dispose },
    { VOPNAME_GETSECATTR,	afs_getsecattr },
    { VOPNAME_SETSECATTR,	afs_setsecattr },
    { VOPNAME_SHRLOCK,		fs_shrlock },
    NULL,
};
struct vnodeops *afs_ops;
#else
struct vnodeops Afs_vnodeops = {
    gafs_open,
    gafs_close,
    afs_vmread,
    afs_vmwrite,
    afs_ioctl,
    fs_setfl,
    gafs_getattr,
    gafs_setattr,
    gafs_access,
    gafs_lookup,
    gafs_create,
    gafs_remove,
    gafs_link,
    gafs_rename,
    gafs_mkdir,
    gafs_rmdir,
    gafs_readdir,
    gafs_symlink,
    gafs_readlink,
    gafs_fsync,
    gafs_inactive,
    gafs_fid,
    afs_rwlock,
    afs_rwunlock,
    afs_seek,
    afs_cmp,
    afs_frlock,
    afs_space,
    afs_realvp,
    afs_getpage,
    afs_putpage,
    afs_map,
    afs_addmap,
    afs_delmap,
    fs_poll,
    afs_dump,
    afs_pathconf,
    afs_pageio,
    afs_dumpctl,
#ifdef	AFS_SUN54_ENV
    afs_dispose,
    afs_setsecattr,
    afs_getsecattr,
#endif
#if	defined(AFS_SUN56_ENV)
    fs_shrlock,
#endif
};
struct vnodeops *afs_ops = &Afs_vnodeops;
#endif



gafs_open(avcp, aflags, acred)
     register struct vcache **avcp;
     afs_int32 aflags;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_open(avcp, aflags, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_close(avc, aflags, count, offset, acred)
     offset_t offset;
     int count;
     register struct vcache *avc;
     afs_int32 aflags;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_close(avc, aflags, count, offset, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_getattr(avc, attrs, flags, acred)
     int flags;
     register struct vcache *avc;
     register struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_getattr(avc, attrs, flags, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_setattr(avc, attrs, flags, acred)
     int flags;
     register struct vcache *avc;
     register struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_setattr(avc, attrs, flags, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_access(avc, amode, flags, acred)
     int flags;
     register struct vcache *avc;
     register afs_int32 amode;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_access(avc, amode, flags, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_lookup(adp, aname, avcp, pnp, flags, rdir, acred)
     struct pathname *pnp;
     int flags;
     struct vnode *rdir;
     register struct vcache *adp, **avcp;
     char *aname;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_lookup(adp, aname, avcp, pnp, flags, rdir, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_create(adp, aname, attrs, aexcl, amode, avcp, acred)
     register struct vcache *adp;
     char *aname;
     struct vattr *attrs;
     enum vcexcl aexcl;
     int amode;
     struct vcache **avcp;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_create(adp, aname, attrs, aexcl, amode, avcp, acred);
    AFS_GUNLOCK();
    return (code);
}

gafs_remove(adp, aname, acred)
     register struct vcache *adp;
     char *aname;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_remove(adp, aname, acred);
    AFS_GUNLOCK();
    return (code);
}

gafs_link(adp, avc, aname, acred)
     register struct vcache *avc;
     register struct vcache *adp;
     char *aname;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_link(adp, avc, aname, acred);
    AFS_GUNLOCK();
    return (code);
}

gafs_rename(aodp, aname1, andp, aname2, acred)
     register struct vcache *aodp, *andp;
     char *aname1, *aname2;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_rename(aodp, aname1, andp, aname2, acred);
#ifdef AFS_SUN510_ENV
    if (code == 0) {
	struct vcache *avcp = NULL;
	
	(void) afs_lookup(andp, aname2, &avcp, NULL, 0, NULL, acred);
	if (avcp) {
	    struct vnode *vp = AFSTOV(avcp), *pvp = AFSTOV(andp);
	    
	    mutex_enter(&vp->v_lock);
	    kmem_free(vp->v_path, strlen(vp->v_path) + 1);
	    vp->v_path = NULL;
	    mutex_exit(&vp->v_lock);
	    VN_SETPATH(afs_globalVp, pvp, vp, aname2, strlen(aname2));
	    
	    AFS_RELE(avcp);
	}
    }
#endif
    AFS_GUNLOCK();
    return (code);
}

gafs_mkdir(adp, aname, attrs, avcp, acred)
     register struct vcache *adp;
     register struct vcache **avcp;
     char *aname;
     struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_mkdir(adp, aname, attrs, avcp, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_rmdir(adp, aname, cdirp, acred)
     struct vnode *cdirp;
     register struct vcache *adp;
     char *aname;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_rmdir(adp, aname, cdirp, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_readdir(avc, auio, acred, eofp)
     int *eofp;
     register struct vcache *avc;
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_readdir(avc, auio, acred, eofp);
    AFS_GUNLOCK();
    return (code);
}

gafs_symlink(adp, aname, attrs, atargetName, acred)
     register struct vcache *adp;
     register char *atargetName;
     char *aname;
     struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_symlink(adp, aname, attrs, atargetName, acred);
    AFS_GUNLOCK();
    return (code);
}


gafs_readlink(avc, auio, acred)
     register struct vcache *avc;
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
    code = afs_readlink(avc, auio, acred);
    AFS_GUNLOCK();
    return (code);
}

#ifdef	AFS_SUN53_ENV
gafs_fsync(avc, flag, acred)
     int flag;
#else
gafs_fsync(avc, acred)
#endif
     register struct vcache *avc;
     struct AFS_UCRED *acred;
{
    register int code;

    AFS_GLOCK();
#ifdef	AFS_SUN53_ENV
    code = afs_fsync(avc, flag, acred);
#else
    code = afs_fsync(avc, acred);
#endif
    AFS_GUNLOCK();
    return (code);
}

void
afs_inactive(struct vcache *avc, struct AFS_UCRED *acred)
{
    struct vnode *vp = AFSTOV(avc);
    if (afs_shuttingdown)
	return;

    /*
     * In Solaris and HPUX s800 and HP-UX10.0 they actually call us with
     * v_count 1 on last reference!
     */
    mutex_enter(&vp->v_lock);
    if (avc->vrefCount <= 0)
	osi_Panic("afs_inactive : v_count <=0\n");

    /*
     * If more than 1 don't unmap the vnode but do decrement the ref count
     */
    vp->v_count--;
    if (vp->v_count > 0) {
	mutex_exit(&vp->v_lock);
	return;
    }
    mutex_exit(&vp->v_lock);

    /*
     * Solaris calls VOP_OPEN on exec, but doesn't call VOP_CLOSE when
     * the executable exits.  So we clean up the open count here.
     *
     * Only do this for mvstat 0 vnodes: when using fakestat, we can't
     * lose the open count for volume roots (mvstat 2), even though they
     * will get VOP_INACTIVE'd when released by afs_PutFakeStat().
     */
    if (avc->opens > 0 && avc->mvstat == 0 && !(avc->states & CCore))
	avc->opens = avc->execsOrWriters = 0;

    afs_InactiveVCache(avc, acred);
}

void
gafs_inactive(avc, acred)
     register struct vcache *avc;
     struct AFS_UCRED *acred;
{
    AFS_GLOCK();
    afs_inactive(avc, acred);
    AFS_GUNLOCK();
}


gafs_fid(avc, fidpp)
     struct vcache *avc;
     struct fid **fidpp;
{
    register int code;

    AFS_GLOCK();
    code = afs_fid(avc, fidpp);
    AFS_GUNLOCK();
    return (code);
}

#endif /* AFS_GLOBAL_SUNLOCK */
