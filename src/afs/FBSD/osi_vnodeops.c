/*
 * A large chunk of this file appears to be copied directly from
 * sys/nfsclient/nfs_bio.c, which has the following license:
 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */
/*
 * Pursuant to a statement of U.C. Berkeley dated 1999-07-22, this license
 * is amended to drop clause (3) above.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#if __FreeBSD_version >= 1000030
#include <sys/rwlock.h>
#endif
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <sys/vmmeter.h>
extern int afs_pbuf_freecnt;

#define GETNAME()       \
    struct componentname *cnp = ap->a_cnp; \
    char *name; \
    name = malloc(cnp->cn_namelen+1, M_TEMP, M_WAITOK); \
    memcpy(name, cnp->cn_nameptr, cnp->cn_namelen); \
    name[cnp->cn_namelen] = '\0'

#define DROPNAME() free(name, M_TEMP)

#ifdef LINK_MAX
# define AFS_LINK_MAX LINK_MAX
#else
# define AFS_LINK_MAX (32767)
#endif

/*
 * Here we define compatibility functions/macros for interfaces that
 * have changed between different FreeBSD versions.
 */
static __inline void ma_vm_page_lock_queues(void) {};
static __inline void ma_vm_page_unlock_queues(void) {};
static __inline void ma_vm_page_lock(vm_page_t m) { vm_page_lock(m); };
static __inline void ma_vm_page_unlock(vm_page_t m) { vm_page_unlock(m); };

#if __FreeBSD_version >= 1000030
#define AFS_VM_OBJECT_WLOCK(o)	VM_OBJECT_WLOCK(o)
#define AFS_VM_OBJECT_WUNLOCK(o)	VM_OBJECT_WUNLOCK(o)
#else
#define AFS_VM_OBJECT_WLOCK(o)	VM_OBJECT_LOCK(o)
#define AFS_VM_OBJECT_WUNLOCK(o)	VM_OBJECT_UNLOCK(o)
#endif

#ifdef VM_CNT_ADD
# define AFS_VM_CNT_ADD(var, x) VM_CNT_ADD(var, x)
# define AFS_VM_CNT_INC(var)    VM_CNT_INC(var)
#else
# define AFS_VM_CNT_ADD(var, x) PCPU_ADD(cnt.var, x)
# define AFS_VM_CNT_INC(var)    PCPU_INC(cnt.var)
#endif

/*
 * Mosty copied from sys/ufs/ufs/ufs_vnops.c:ufs_pathconf().
 * We should know the correct answers to these questions with
 * respect to the AFS protocol (which may differ from the UFS
 * values) but for the moment this will do.
 */
static int
afs_vop_pathconf(struct vop_pathconf_args *ap)
{
	int error;

	error = 0;
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = AFS_LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		break;
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
#ifdef _PC_ACL_EXTENDED
	case _PC_ACL_EXTENDED:
		*ap->a_retval = 0;
		break;
	case _PC_ACL_PATH_MAX:
		*ap->a_retval = 3;
		break;
#endif
#ifdef _PC_MAC_PRESENT
	case _PC_MAC_PRESENT:
		*ap->a_retval = 0;
		break;
#endif
#ifdef _PC_ASYNC_IO
	case _PC_ASYNC_IO:
		/* _PC_ASYNC_IO should have been handled by upper layers. */
		KASSERT(0, ("_PC_ASYNC_IO should not get here"));
		error = EINVAL;
		break;
	case _PC_PRIO_IO:
		*ap->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 0;
		break;
#endif
#ifdef _PC_ALLOC_SIZE_MIN
	case _PC_ALLOC_SIZE_MIN:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_bsize;
		break;
#endif
#ifdef _PC_FILESIZEBITS
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32; /* XXX */
		break;
#endif
#ifdef _PC_REC_INCR_XFER_SIZE
	case _PC_REC_INCR_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_MAX_XFER_SIZE:
		*ap->a_retval = -1; /* means ``unlimited'' */
		break;
	case _PC_REC_MIN_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_XFER_ALIGN:
		*ap->a_retval = PAGE_SIZE;
		break;
#endif
#ifdef _PC_SYMLINK_MAX
	case _PC_SYMLINK_MAX:
		*ap->a_retval = MAXPATHLEN;
		break;
#endif
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
afs_vop_lookup(ap)
     struct vop_lookup_args	/* {
				 * struct vnodeop_desc * a_desc;
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error;
    struct vcache *vcp;
    struct vnode *vp, *dvp;
    int flags = ap->a_cnp->cn_flags;

    dvp = ap->a_dvp;
    if (dvp->v_type != VDIR) {
	return ENOTDIR;
    }

    if ((flags & ISDOTDOT) && (dvp->v_vflag & VV_ROOT))
	return EIO;

    GETNAME();

#if __FreeBSD_version < 1000021
    cnp->cn_flags |= MPSAFE; /* steel */
#endif

    /*
     * Locking rules:
     *
     * - 'dvp' is locked by our caller. We must return it locked, whether we
     * return success or error.
     *
     * - If the lookup succeeds, 'vp' must be locked before we return.
     *
     * - If we lock multiple vnodes, parent vnodes must be locked before
     * children vnodes.
     *
     * As a result, looking up the parent directory (if 'flags' has ISDOTDOT
     * set) is a bit of a special case. In that case, we must unlock 'dvp'
     * before performing the lookup, since the lookup operation may lock the
     * target vnode, and the target vnode is the parent of 'dvp' (so we must
     * lock 'dvp' after locking the target vnode).
     */

    if (flags & ISDOTDOT)
	VOP_UNLOCK(dvp, 0);

    AFS_GLOCK();
    error = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();

    if (error) {
	if (flags & ISDOTDOT)
	    vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
	    && (flags & ISLASTCN) && error == ENOENT)
	    error = EJUSTRETURN;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
	DROPNAME();
	*ap->a_vpp = 0;
	return (error);
    }
    vp = AFSTOV(vcp);		/* always get a node if no error */

    if (flags & ISDOTDOT) {
	/* Must lock 'vp' before 'dvp', since 'vp' is the parent vnode. */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
    } else if (vp == dvp) {
	/* they're the same; afs_lookup() already ref'ed the leaf.
	 * It came in locked, so we don't need to ref OR lock it */
    } else {
	vn_lock(vp, LK_EXCLUSIVE | LK_CANRECURSE | LK_RETRY);
    }
    *ap->a_vpp = vp;

    if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	cnp->cn_flags |= SAVENAME;

    DROPNAME();
    return error;
}

static int
afs_vop_create(ap)
     struct vop_create_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    int error = 0;
    struct vcache *vcp;
    struct vnode *dvp = ap->a_dvp;
    GETNAME();

    AFS_GLOCK();
    error =
	afs_create(VTOAFS(dvp), name, ap->a_vap,
		   ap->a_vap->va_vaflags & VA_EXCLUSIVE ? EXCL : NONEXCL,
		   ap->a_vap->va_mode, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	DROPNAME();
	return (error);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY);
    } else
	*ap->a_vpp = 0;

    DROPNAME();
    return error;
}

static int
afs_vop_mknod(ap)
     struct vop_mknod_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    return (ENODEV);
}

static int
afs_vop_open(ap)
     struct vop_open_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct thread *a_td;
				 * struct file *a_fp;
				 * } */ *ap;
{
    int error;
    struct vcache *vc = VTOAFS(ap->a_vp);

    AFS_GLOCK();
    error = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != ap->a_vp)
	panic("AFS open changed vnode!");
#endif
    AFS_GUNLOCK();
    vnode_create_vobject(ap->a_vp, vc->f.m.Length, ap->a_td);
    osi_FlushPages(vc, ap->a_cred);
    return error;
}

static int
afs_vop_close(ap)
     struct vop_close_args	/* {
				 * struct vnode *a_vp;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct thread *a_td;
				 * } */ *ap;
{
    int code, iflag;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);

    VI_LOCK(vp);
    iflag = vp->v_iflag & VI_DOOMED;
    VI_UNLOCK(vp);
    if (iflag & VI_DOOMED) {
        /* osi_FlushVCache (correctly) calls vgone() on recycled vnodes, we don't
         * have an afs_close to process, in that case */
        if (avc->opens != 0)
            panic("afs_vop_close: doomed vnode %p has vcache %p with non-zero opens %d\n",
                  vp, avc, avc->opens);
        return 0;
    }

    AFS_GLOCK();
    if (ap->a_cred)
	code = afs_close(avc, ap->a_fflag, ap->a_cred);
    else
	code = afs_close(avc, ap->a_fflag, afs_osi_credp);
    osi_FlushPages(avc, ap->a_cred);	/* hold GLOCK, but not basic vnode lock */

    ObtainWriteLock(&avc->lock, 808);
    if (avc->cred != NULL) {
	crfree(avc->cred);
	avc->cred = NULL;
    }
    ReleaseWriteLock(&avc->lock);

    AFS_GUNLOCK();
    return code;
}

static int
afs_vop_access(ap)
     struct vop_access_args	/* {
				 * struct vnode *a_vp;
				 * accmode_t a_accmode;
				 * struct ucred *a_cred;
				 * struct thread *a_td;
				 * } */ *ap;
{
    int code;
    AFS_GLOCK();
    code = afs_access(VTOAFS(ap->a_vp), ap->a_accmode, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

static int
afs_vop_getattr(ap)
     struct vop_getattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;

    AFS_GLOCK();
    code = afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();

    return code;
}

static int
afs_vop_setattr(ap)
     struct vop_setattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;
    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

static int
afs_vop_read(ap)
     struct vop_read_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * 
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold GLOCK, but not basic vnode lock */
    code = afs_read(avc, ap->a_uio, ap->a_cred, 0);
    AFS_GUNLOCK();
    return code;
}

/* struct vop_getpages_args {
 *	struct vnode *a_vp;
 *	vm_page_t *a_m;
 *	int a_count;
 *	int *a_rbehind;
 *	int *a_rahead;
 * };
 */
static int
afs_vop_getpages(struct vop_getpages_args *ap)
{
    int code;
    int i, nextoff, size, toff, npages, count;
    struct uio uio;
    struct iovec iov;
    struct buf *bp;
    vm_offset_t kva;
    vm_object_t object;
    vm_page_t *pages;
    struct vnode *vp;
    struct vcache *avc;

    memset(&uio, 0, sizeof(uio));
    memset(&iov, 0, sizeof(iov));

    vp = ap->a_vp;
    avc = VTOAFS(vp);
    pages = ap->a_m;
#ifdef FBSD_VOP_GETPAGES_BUSIED
    npages = ap->a_count;
    if (ap->a_rbehind)
        *ap->a_rbehind = 0;
    if (ap->a_rahead)
        *ap->a_rahead = 0;
#else
    npages = btoc(ap->a_count);
#endif

    if ((object = vp->v_object) == NULL) {
	printf("afs_getpages: called with non-merged cache vnode??\n");
	return VM_PAGER_ERROR;
    }

    /*
     * If the requested page is partially valid, just return it and
     * allow the pager to zero-out the blanks.  Partially valid pages
     * can only occur at the file EOF.
     */
    {
#ifdef FBSD_VOP_GETPAGES_BUSIED
	AFS_VM_OBJECT_WLOCK(object);
	ma_vm_page_lock_queues();
	if(pages[npages - 1]->valid != 0) {
	    if (--npages == 0) {
		ma_vm_page_unlock_queues();
		AFS_VM_OBJECT_WUNLOCK(object);
		return (VM_PAGER_OK);
	    }
	}
#else
	vm_page_t m = pages[ap->a_reqpage];
	AFS_VM_OBJECT_WLOCK(object);
	ma_vm_page_lock_queues();
	if (m->valid != 0) {
	    /* handled by vm_fault now        */
	    /* vm_page_zero_invalid(m, TRUE); */
	    for (i = 0; i < npages; ++i) {
		if (i != ap->a_reqpage) {
		    ma_vm_page_lock(pages[i]);
		    vm_page_free(pages[i]);
		    ma_vm_page_unlock(pages[i]);
		}
	    }
	    ma_vm_page_unlock_queues();
	    AFS_VM_OBJECT_WUNLOCK(object);
	    return (0);
	}
#endif
	ma_vm_page_unlock_queues();
	AFS_VM_OBJECT_WUNLOCK(object);
    }
    bp = getpbuf(&afs_pbuf_freecnt);

    kva = (vm_offset_t) bp->b_data;
    pmap_qenter(kva, pages, npages);
    AFS_VM_CNT_INC(v_vnodein);
    AFS_VM_CNT_ADD(v_vnodepgsin, npages);

#ifdef FBSD_VOP_GETPAGES_BUSIED
    count = ctob(npages);
#else
    count = ap->a_count;
#endif
    iov.iov_base = (caddr_t) kva;
    iov.iov_len = count;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
    uio.uio_resid = count;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_READ;
    uio.uio_td = curthread;

    AFS_GLOCK();
    osi_FlushPages(avc, osi_curcred());	/* hold GLOCK, but not basic vnode lock */
    code = afs_read(avc, &uio, osi_curcred(), 0);
    AFS_GUNLOCK();
    pmap_qremove(kva, npages);

    relpbuf(bp, &afs_pbuf_freecnt);

    if (code && (uio.uio_resid == count)) {
#ifndef FBSD_VOP_GETPAGES_BUSIED
	AFS_VM_OBJECT_WLOCK(object);
	ma_vm_page_lock_queues();
	for (i = 0; i < npages; ++i) {
	    if (i != ap->a_reqpage)
		vm_page_free(pages[i]);
	}
	ma_vm_page_unlock_queues();
	AFS_VM_OBJECT_WUNLOCK(object);
#endif
	return VM_PAGER_ERROR;
    }

    size = count - uio.uio_resid;
    AFS_VM_OBJECT_WLOCK(object);
    ma_vm_page_lock_queues();
    for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
	vm_page_t m;
	nextoff = toff + PAGE_SIZE;
	m = pages[i];

	/* XXX not in nfsclient? */
	m->flags &= ~PG_ZERO;

	if (nextoff <= size) {
	    /*
	     * Read operation filled an entire page
	     */
	    m->valid = VM_PAGE_BITS_ALL;
	    KASSERT(m->dirty == 0, ("afs_getpages: page %p is dirty", m));
	} else if (size > toff) {
	    /*
	     * Read operation filled a partial page.
	     */
	    m->valid = 0;
	    vm_page_set_validclean(m, 0, size - toff);
	    KASSERT(m->dirty == 0, ("afs_getpages: page %p is dirty", m));
	}

#ifndef FBSD_VOP_GETPAGES_BUSIED
	if (i != ap->a_reqpage) {
#if __FreeBSD_version >= 1000042
	    vm_page_readahead_finish(m);
#else
	    /*
	     * Whether or not to leave the page activated is up in
	     * the air, but we should put the page on a page queue
	     * somewhere (it already is in the object).  Result:
	     * It appears that emperical results show that
	     * deactivating pages is best.
	     */

	    /*
	     * Just in case someone was asking for this page we
	     * now tell them that it is ok to use.
	     */
	    if (!code) {
		if (m->oflags & VPO_WANTED) {
		    ma_vm_page_lock(m);
		    vm_page_activate(m);
		    ma_vm_page_unlock(m);
		}
		else {
		    ma_vm_page_lock(m);
		    vm_page_deactivate(m);
		    ma_vm_page_unlock(m);
		}
		vm_page_wakeup(m);
	    } else {
		ma_vm_page_lock(m);
		vm_page_free(m);
		ma_vm_page_unlock(m);
	    }
#endif	/* __FreeBSD_version 1000042 */
	}
#endif   /* ndef FBSD_VOP_GETPAGES_BUSIED */
    }
    ma_vm_page_unlock_queues();
    AFS_VM_OBJECT_WUNLOCK(object);
    return VM_PAGER_OK;
}

static int
afs_vop_write(ap)
     struct vop_write_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    off_t start, end;
    start = AFS_UIO_OFFSET(ap->a_uio);
    end = start + AFS_UIO_RESID(ap->a_uio);

    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold GLOCK, but not basic vnode lock */
    code =
	afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    AFS_GUNLOCK();

    /* Invalidate any pages in the written area. */
    vn_pages_remove(ap->a_vp, OFF_TO_IDX(start), OFF_TO_IDX(end));

    return code;
}

/*-
 * struct vop_putpages_args {
 *	struct vnode *a_vp;
 *	vm_page_t *a_m;
 *	int a_count;
 *	int a_sync;
 *	int *a_rtvals;
 *	vm_oofset_t a_offset;
 * };
 */
/*
 * All of the pages passed to us in ap->a_m[] are already marked as busy,
 * so there is no additional locking required to set their flags.  -GAW
 */
static int
afs_vop_putpages(struct vop_putpages_args *ap)
{
    int code;
    int i, size, npages, sync;
    struct uio uio;
    struct iovec iov;
    struct buf *bp;
    vm_offset_t kva;
    struct vnode *vp;
    struct vcache *avc;
    struct ucred *cred;

    memset(&uio, 0, sizeof(uio));
    memset(&iov, 0, sizeof(iov));

    vp = ap->a_vp;
    avc = VTOAFS(vp);
    /* Perhaps these two checks should just be KASSERTs instead... */
    if (vp->v_object == NULL) {
	printf("afs_putpages: called with non-merged cache vnode??\n");
	return VM_PAGER_ERROR;	/* XXX I think this is insufficient */
    }
    if (vType(avc) != VREG) {
	printf("afs_putpages: not VREG");
	return VM_PAGER_ERROR;	/* XXX I think this is insufficient */
    }
    npages = btoc(ap->a_count);
    for (i = 0; i < npages; i++)
	ap->a_rtvals[i] = VM_PAGER_AGAIN;
    bp = getpbuf(&afs_pbuf_freecnt);

    kva = (vm_offset_t) bp->b_data;
    pmap_qenter(kva, ap->a_m, npages);
    AFS_VM_CNT_INC(v_vnodeout);
    AFS_VM_CNT_ADD(v_vnodepgsout, ap->a_count);

    iov.iov_base = (caddr_t) kva;
    iov.iov_len = ap->a_count;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = IDX_TO_OFF(ap->a_m[0]->pindex);
    uio.uio_resid = ap->a_count;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_WRITE;
    uio.uio_td = curthread;
    sync = IO_VMIO;
    if (ap->a_sync & VM_PAGER_PUT_SYNC)
	sync |= IO_SYNC;
    /*if (ap->a_sync & VM_PAGER_PUT_INVAL)
     * sync |= IO_INVAL; */

    AFS_GLOCK();

    ObtainReadLock(&avc->lock);
    if (avc->cred != NULL) {
	/*
	 * Use the creds from the process that opened this file for writing; if
	 * any. Otherwise, if we use the current process's creds, we may use
	 * the creds for uid 0 if we are writing back pages from the syncer(4)
	 * process.
	 */
	cred = crhold(avc->cred);
    } else {
	cred = crhold(curthread->td_ucred);
    }
    ReleaseReadLock(&avc->lock);

    code = afs_write(avc, &uio, sync, cred, 0);
    AFS_GUNLOCK();

    pmap_qremove(kva, npages);
    relpbuf(bp, &afs_pbuf_freecnt);

    if (!code) {
	AFS_VM_OBJECT_WLOCK(vp->v_object);
	size = ap->a_count - uio.uio_resid;
	for (i = 0; i < round_page(size) / PAGE_SIZE; i++) {
	    ap->a_rtvals[i] = VM_PAGER_OK;
	    vm_page_undirty(ap->a_m[i]);
	}
	AFS_VM_OBJECT_WUNLOCK(vp->v_object);
    }
    crfree(cred);
    return ap->a_rtvals[0];
}

static int
afs_vop_ioctl(ap)
     struct vop_ioctl_args	/* {
				 * struct vnode *a_vp;
				 * u_long a_command;
				 * void *a_data;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct thread *a_td;
				 * } */ *ap;
{
    struct vcache *tvc = VTOAFS(ap->a_vp);
    int error = 0;

    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    if (((ap->a_command >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	AFS_GLOCK();
	error = HandleIoctl(tvc, ap->a_command, ap->a_data);
	AFS_GUNLOCK();
	return (error);
    } else {
	/* No-op call; just return. */
	return (ENOTTY);
    }
}

static int
afs_vop_fsync(ap)
     struct vop_fsync_args	/* {
				 * struct vnode *a_vp;
				 * int a_waitfor;
				 * struct thread *td;
				 * } */ *ap;
{
    int error;
    struct vnode *vp = ap->a_vp;

    AFS_GLOCK();
    /*vflushbuf(vp, wait); */
    error = afs_fsync(VTOAFS(vp), ap->a_td->td_ucred);
    AFS_GUNLOCK();
    return error;
}

static int
afs_vop_remove(ap)
     struct vop_remove_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    error = afs_remove(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    cache_purge(vp);
    DROPNAME();
    return error;
}

static int
afs_vop_link(ap)
     struct vop_link_args	/* {
				 * struct vnode *a_vp;
				 * struct vnode *a_tdvp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    struct vnode *dvp = ap->a_tdvp;
    struct vnode *vp = ap->a_vp;

    GETNAME();
    if (dvp->v_mount != vp->v_mount) {
	error = EXDEV;
	goto out;
    }
    if (vp->v_type == VDIR) {
	error = EISDIR;
	goto out;
    }
    if ((error = vn_lock(vp, LK_CANRECURSE | LK_EXCLUSIVE)) != 0) {
	goto out;
    }
    AFS_GLOCK();
    error = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    if (dvp != vp)
	VOP_UNLOCK(vp, 0);
  out:
    DROPNAME();
    return error;
}

static int
afs_vop_rename(ap)
     struct vop_rename_args	/* {
				 * struct vnode *a_fdvp;
				 * struct vnode *a_fvp;
				 * struct componentname *a_fcnp;
				 * struct vnode *a_tdvp;
				 * struct vnode *a_tvp;
				 * struct componentname *a_tcnp;
				 * } */ *ap;
{
    int error = 0;
    struct componentname *fcnp = ap->a_fcnp;
    char *fname;
    struct componentname *tcnp = ap->a_tcnp;
    char *tname;
    struct vnode *tvp = ap->a_tvp;
    struct vnode *tdvp = ap->a_tdvp;
    struct vnode *fvp = ap->a_fvp;
    struct vnode *fdvp = ap->a_fdvp;

    /*
     * Check for cross-device rename.
     */
    if ((fvp->v_mount != tdvp->v_mount)
	|| (tvp && (fvp->v_mount != tvp->v_mount))) {
	error = EXDEV;
      abortit:
	if (tdvp == tvp)
	    vrele(tdvp);
	else
	    vput(tdvp);
	if (tvp)
	    vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	return (error);
    }
    /*
     * if fvp == tvp, we're just removing one name of a pair of
     * directory entries for the same element.  convert call into rename.
     ( (pinched from FreeBSD 4.4's ufs_rename())
     
     */
    if (fvp == tvp) {
	if (fvp->v_type == VDIR) {
	    error = EINVAL;
	    goto abortit;
	}

	/* Release destination completely. */
	vput(tdvp);
	vput(tvp);

	/* Delete source. */
	vrele(fdvp);
	vrele(fvp);
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
	    panic("afs_rename: lost from startdir");
	fcnp->cn_nameiop = DELETE;
	VREF(fdvp);
	error = relookup(fdvp, &fvp, fcnp);
	if (error == 0)
	    vrele(fdvp);
	if (fvp == NULL) {
	    return (ENOENT);
	}

	error = VOP_REMOVE(fdvp, fvp, fcnp);
	if (fdvp == fvp)
	    vrele(fdvp);
	else
	    vput(fdvp);
	vput(fvp);
	return (error);
    }
    if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
	goto abortit;

    fname = malloc(fcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(fname, fcnp->cn_nameptr, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    tname = malloc(tcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(tname, tcnp->cn_nameptr, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    error =
	afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, tcnp->cn_cred);
    AFS_GUNLOCK();

    free(fname, M_TEMP);
    free(tname, M_TEMP);
    if (tdvp == tvp)
	vrele(tdvp);
    else
	vput(tdvp);
    if (tvp)
	vput(tvp);
    vrele(fdvp);
    vput(fvp);
    return error;
}

static int
afs_vop_mkdir(ap)
     struct vop_mkdir_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    struct vnode *dvp = ap->a_dvp;
    struct vattr *vap = ap->a_vap;
    int error = 0;
    struct vcache *vcp;

    GETNAME();
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_vop_mkdir: no name");
#endif
    AFS_GLOCK();
    error = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	DROPNAME();
	return (error);
    }
    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY);
    } else
	*ap->a_vpp = 0;
    DROPNAME();
    return error;
}

static int
afs_vop_rmdir(ap)
     struct vop_rmdir_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    error = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    return error;
}

/* struct vop_symlink_args {
 *	struct vnode *a_dvp;
 *	struct vnode **a_vpp;
 *	struct componentname *a_cnp;
 *	struct vattr *a_vap;
 *	char *a_target;
 * };
 */
static int
afs_vop_symlink(struct vop_symlink_args *ap)
{
    struct vnode *dvp;
    struct vnode *newvp;
    struct vcache *vcp;
    int error;

    GETNAME();
    AFS_GLOCK();

    dvp = ap->a_dvp;
    newvp = NULL;

    error =
	afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, NULL,
		    cnp->cn_cred);
    if (error == 0) {
	error = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
	if (error == 0) {
	    newvp = AFSTOV(vcp);
	    vn_lock(newvp, LK_EXCLUSIVE | LK_RETRY);
	}
    }
    AFS_GUNLOCK();
    DROPNAME();
    *(ap->a_vpp) = newvp;
    return error;
}

static int
afs_vop_readdir(ap)
     struct vop_readdir_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * int *a_eofflag;
				 * u_long *a_cookies;
				 * int ncookies;
				 * } */ *ap;
{
    int error;
    off_t off;
/*    printf("readdir %x cookies %x ncookies %d\n", ap->a_vp, ap->a_cookies,
	   ap->a_ncookies); */
    off = ap->a_uio->uio_offset;
    AFS_GLOCK();
    error =
	afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, ap->a_eofflag);
    AFS_GUNLOCK();
    if (!error && ap->a_ncookies != NULL) {
	struct uio *uio = ap->a_uio;
	const struct dirent *dp, *dp_start, *dp_end;
	int ncookies;
	u_long *cookies, *cookiep;

	if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
	    panic("afs_readdir: burned cookies");
	dp = (const struct dirent *)
	    ((const char *)uio->uio_iov->iov_base - (uio->uio_offset - off));

	dp_end = (const struct dirent *)uio->uio_iov->iov_base;
	for (dp_start = dp, ncookies = 0; dp < dp_end;
	     dp = (const struct dirent *)((const char *)dp + dp->d_reclen))
	    ncookies++;

	cookies = malloc(ncookies * sizeof(u_long), M_TEMP,
	       M_WAITOK);
	for (dp = dp_start, cookiep = cookies; dp < dp_end;
	     dp = (const struct dirent *)((const char *)dp + dp->d_reclen)) {
	    off += dp->d_reclen;
	    *cookiep++ = off;
	}
	*ap->a_cookies = cookies;
	*ap->a_ncookies = ncookies;
    }

    return error;
}

static int
afs_vop_readlink(ap)
     struct vop_readlink_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int error;
/*    printf("readlink %x\n", ap->a_vp);*/
    AFS_GLOCK();
    error = afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred);
    AFS_GUNLOCK();
    return error;
}

static int
afs_vop_inactive(ap)
     struct vop_inactive_args	/* {
				 * struct vnode *a_vp;
				 * struct thread *td;
				 * } */ *ap;
{
    struct vnode *vp = ap->a_vp;

    AFS_GLOCK();
    afs_InactiveVCache(VTOAFS(vp), 0);	/* decrs ref counts */
    AFS_GUNLOCK();
    return 0;
}

/*
 * struct vop_reclaim_args {
 *	struct vnode *a_vp;
 * };
 */
static int
afs_vop_reclaim(struct vop_reclaim_args *ap)
{
    int code, slept;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);
    int haveGlock = ISAFS_GLOCK();

    /*
     * In other code paths, we acquire the vnode lock while afs_xvcache is
     * already held (e.g. afs_PutVCache() -> vrele()). Here, we already have
     * the vnode lock, and we need afs_xvcache. So drop the vnode lock in order
     * to hold afs_xvcache.
     */
    VOP_UNLOCK(vp, 0);

    if (!haveGlock)
	AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache, 901);

    /*
     * Note that we deliberately call VOP_LOCK() instead of vn_lock() here.
     * vn_lock() will return an error for VI_DOOMED vnodes, but we know this
     * vnode is already VI_DOOMED. We just want to lock it again, and skip the
     * VI_DOOMED check.
     */
    VOP_LOCK(vp, LK_EXCLUSIVE);

    code = afs_FlushVCache(avc, &slept);

    if (avc->f.states & CVInit) {
	avc->f.states &= ~CVInit;
	afs_osi_Wakeup(&avc->f.states);
    }

    ReleaseWriteLock(&afs_xvcache);
    if (!haveGlock)
	AFS_GUNLOCK();

    if (code) {
	afs_warn("afs_vop_reclaim: afs_FlushVCache failed code %d vnode\n", code);
	VOP_PRINT(vp);
	panic("afs: afs_FlushVCache failed during reclaim");
    }

    vnode_destroy_vobject(vp);
    vp->v_data = 0;

    return 0;
}

static int
afs_vop_strategy(ap)
     struct vop_strategy_args	/* {
				 * struct buf *a_bp;
				 * } */ *ap;
{
    int error;
    AFS_GLOCK();
    error = afs_ustrategy(ap->a_bp, osi_curcred());
    AFS_GUNLOCK();
    return error;
}

static int
afs_vop_print(ap)
     struct vop_print_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(ap->a_vp);
    int s = vc->f.states;

    printf("vc %p vp %p tag %s, fid: %d.%d.%d.%d, opens %d, writers %d", vc, vp, vp->v_tag,
	   (int)vc->f.fid.Cell, (u_int) vc->f.fid.Fid.Volume,
	   (u_int) vc->f.fid.Fid.Vnode, (u_int) vc->f.fid.Fid.Unique, vc->opens,
	   vc->execsOrWriters);
    printf("\n  states%s%s%s%s%s", (s & CStatd) ? " statd" : "",
	   (s & CRO) ? " readonly" : "", (s & CDirty) ? " dirty" : "",
	   (s & CMAPPED) ? " mapped" : "",
	   (s & CVFlushed) ? " flush in progress" : "");
    printf("\n");
    return 0;
}

/*
 * Advisory record locking support (fcntl() POSIX style)
 */
static int
afs_vop_advlock(ap)
     struct vop_advlock_args	/* {
				 * struct vnode *a_vp;
				 * caddr_t  a_id;
				 * int  a_op;
				 * struct flock *a_fl;
				 * int  a_flags;
				 * } */ *ap;
{
    int error, a_op;
    struct ucred cr = *osi_curcred();

    a_op = ap->a_op;
    if (a_op == F_UNLCK) {
	/*
	 * When a_fl->type is F_UNLCK, FreeBSD passes in an a_op of F_UNLCK.
	 * This is (confusingly) different than how you actually release a lock
	 * with fcntl(), which is done with an a_op of F_SETLK and an l_type of
	 * F_UNLCK. Pretend we were given an a_op of F_SETLK in this case,
	 * since this is what afs_lockctl expects.
	 */
	a_op = F_SETLK;
    }

    AFS_GLOCK();
    error =
	afs_lockctl(VTOAFS(ap->a_vp),
		ap->a_fl,
		a_op, &cr,
		(int)(intptr_t)ap->a_id);	/* XXX: no longer unique! */
    AFS_GUNLOCK();
    return error;
}

struct vop_vector afs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		afs_vop_access,
	.vop_advlock =		afs_vop_advlock,
	.vop_close =		afs_vop_close,
	.vop_create =		afs_vop_create,
	.vop_fsync =		afs_vop_fsync,
	.vop_getattr =		afs_vop_getattr,
	.vop_getpages =		afs_vop_getpages,
	.vop_inactive =		afs_vop_inactive,
	.vop_ioctl =		afs_vop_ioctl,
	.vop_link =		afs_vop_link,
	.vop_lookup =		afs_vop_lookup,
	.vop_mkdir =		afs_vop_mkdir,
	.vop_mknod =		afs_vop_mknod,
	.vop_open =		afs_vop_open,
	.vop_pathconf =		afs_vop_pathconf,
	.vop_print =		afs_vop_print,
	.vop_putpages =		afs_vop_putpages,
	.vop_read =		afs_vop_read,
	.vop_readdir =		afs_vop_readdir,
	.vop_readlink =		afs_vop_readlink,
	.vop_reclaim =		afs_vop_reclaim,
	.vop_remove =		afs_vop_remove,
	.vop_rename =		afs_vop_rename,
	.vop_rmdir =		afs_vop_rmdir,
	.vop_setattr =		afs_vop_setattr,
	.vop_strategy =		afs_vop_strategy,
	.vop_symlink =		afs_vop_symlink,
	.vop_write =		afs_vop_write,
};
