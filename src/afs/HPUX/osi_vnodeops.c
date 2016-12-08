/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This is a placeholder for routines unique to the port of AFS to hp-ux*/

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */

#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/pathname.h>

extern struct vfsops Afs_vfsops;
extern int afs_hp_strategy();
extern int afs_bmap(), afs_badop(), afs_noop(), afs_lockf();
extern int afs_pagein();
extern int afs_pageout();
extern int afs_ioctl();
extern int afs_prealloc();
extern int afs_mapdbd();
extern int afs_mmap();
extern int afs_cachelimit();
extern int afs_vm_checkpage();
extern int afs_vm_fscontiguous();
extern int afs_vm_stopio();
extern int afs_read_ahead();
extern int afs_unmap();
extern int afs_release();
extern int afs_swapfs_len();
extern int afs_readdir2();
extern int afs_readdir();
extern int afs_readdir3();
extern int afs_pathconf();
extern int afs_close();

#define vtoblksz(vp)	((vp)->v_vfsp->vfs_bsize)

#if defined(AFS_HPUX110_ENV)
/* We no longer need to lock on the VM Empire,
 * or at least that is what is claimed. 
 * so we will noopt the vmemp_ routines
 * This needs to be looked at closer.
 */
#define vmemp_lockx()
#undef  vmemp_returnx
#define vmemp_returnx(a) return(a)
#define vmemp_unlockx()
#endif

#if !defined(AFS_HPUX110_ENV)
/*
 * Copy an mbuf to the contiguous area pointed to by cp.
 * Skip <off> bytes and copy <len> bytes.
 * Returns the number of bytes not transferred.
 * The mbuf is NOT changed.
 */
int
m_cpytoc(m, off, len, cp)
     struct mbuf *m;
     int off, len;
     caddr_t cp;
{
    int ml;

    if (m == NULL || off < 0 || len < 0 || cp == NULL)
	osi_Panic("m_cpytoc");
    while (off && m)
	if (m->m_len <= off) {
	    off -= m->m_len;
	    m = m->m_next;
	    continue;
	} else
	    break;
    if (m == NULL)
	return (len);

    ml = MIN(len, m->m_len - off);
    memcpy(cp, mtod(m, caddr_t) + off, (u_int) ml);
    cp += ml;
    len -= ml;
    m = m->m_next;

    while (len && m) {
	ml = m->m_len;
	memcpy(cp, mtod(m, caddr_t), (u_int) ml);
	cp += ml;
	len -= ml;
	m = m->m_next;
    }

    return (len);
}
#endif

/* 
 *  Note that the standard Sun vnode interface doesn't haven't an vop_lockf(), so this code is
 * totally new.  This came about because HP-UX has lockf() implemented as
 * a system call while Sun has it implemented as a library (apparently).
 * To handle this, we have to translate the lockf() request into an
 * fcntl() looking request, and then translate the results back if necessary.
 * we call afs_lockctl() directly .
 */
afs_lockf(vp, flag, len, cred, fp, LB, UB)
     struct vnode *vp;
     int flag;
     afs_ucred_t *cred;
     struct file *fp;
     k_off_t len, LB, UB;
{
    /*for now, just pretend it works */
    struct k_flock flock;
    int cmd, code;

    /*
     * Create a flock structure and translate the lockf request
     * into an appropriate looking fcntl() type request for afs_lockctl()
     */
    flock.l_whence = 0;
    flock.l_len = len;
    flock.l_start = fp->f_offset;
    /* convert negative lengths to positive */
    if (flock.l_len < 0) {
	flock.l_start += flock.l_len;
	flock.l_len = -(flock.l_len);
    }
    /*
     * Adjust values to look like fcntl() requests.
     * All locks are write locks, only F_LOCK requests
     * are blocking.  F_TEST has to be translated into
     * a get lock and then back again.
     */
    flock.l_type = F_WRLCK;
    cmd = F_SETLK;
    switch (flag) {
    case F_ULOCK:
	flock.l_type = F_UNLCK;
	break;
    case F_LOCK:
	cmd = F_SETLKW;
	break;
    case F_TEST:
	cmd = F_GETLK;
	break;
    }
    u.u_error = mp_afs_lockctl(vp, &flock, cmd, fp->f_cred);
    if (u.u_error) {
	return (u.u_error);	/* some other error code */
    }
    /*
     * if request is F_TEST, and GETLK changed
     * the lock type to ULOCK, then return 0, else
     * set errno to EACCESS and return.
     */
    if (flag == F_TEST && flock.l_type != F_UNLCK) {
	u.u_error = EACCES;
	return (u.u_error);
    }
    return (0);
}


#if defined(AFS_HPUX1122_ENV)
#include "machine/vm/vmparam.h"
#else
#include "../machine/vmparam.h"	/* For KERNELSPACE */
#endif
#include "h/debug.h"
#include "h/types.h"
#if !defined(AFS_HPUX1123_ENV)
	/* 11.23 is using 64 bit in many cases */
#define kern_daddr_t daddr_t
#endif
#include "h/param.h"
#include "h/vmmac.h"
#include "h/time.h"
#include "ufs/inode.h"
#include "ufs/fs.h"
#include "h/dbd.h"
#if defined(AFS_HPUX1123_ENV)
dbd_t       *finddbd();
#endif /* AFS_HPUX1123_ENV */
#include "h/vfd.h"
#include "h/region.h"
#include "h/pregion.h"
#include "h/vmmeter.h"
#include "h/user.h"
#include "h/sysinfo.h"
#include "h/pfdat.h"
#if !defined(AFS_HPUX1123_ENV)
#include "h/tuneable.h"
#endif
#include "h/buf.h"
#include "netinet/in.h"

/* a freelist of one */
struct buf *afs_bread_freebp = 0;

/*
 *  Only rfs_read calls this, and it only looks at bp->b_un.b_addr.
 *  Thus we can use fake bufs (ie not from the real buffer pool).
 */
afs_bread(vp, lbn, bpp)
     struct vnode *vp;
     kern_daddr_t lbn;
     struct buf **bpp;
{
    int offset, fsbsize, error;
    struct buf *bp;
    struct iovec iov;
    struct uio uio;

    memset(&uio, 0, sizeof(uio));
    memset(&iov, 0, sizeof(iov));

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
    uio.uio_fpflags = 0;
    *bpp = 0;

    error = afs_read(VTOAFS(vp), &uio, p_cred(u.u_procp), 0);
    if (error) {
	afs_bread_freebp = bp;
	return error;
    }
    if (*bpp) {
	afs_bread_freebp = bp;
    } else {
	*(struct buf **)&bp->b_vp = bp;	/* mark as fake */
	*bpp = bp;
    }
    return 0;
}

afs_brelse(vp, bp)
     struct vnode *vp;
     struct buf *bp;
{
    AFS_STATCNT(afs_brelse);

    if ((struct buf *)bp->b_vp != bp) {	/* not fake */
	ufs_brelse(bp->b_vp, bp);
    } else if (afs_bread_freebp) {
	AFS_KFREE(bp->b_un.b_addr, vp->v_vfsp->vfs_bsize);
	AFS_KFREE(bp, sizeof(*bp));
    } else {
	afs_bread_freebp = bp;
    }
}


afs_bmap(avc, abn, anvp, anbn)
     struct vcache *avc;
     kern_daddr_t abn, *anbn;
     struct vcache **anvp;
{
    AFS_STATCNT(afs_bmap);
    if (anvp)
	*anvp = avc;
    if (anbn)
	*anbn = abn * (8192 / DEV_BSIZE);	/* in 512 byte units */
    return 0;
}

afs_inactive(avc, acred)
     struct vcache *avc;
     afs_ucred_t *acred;
{
    struct vnode *vp = AFSTOV(avc);
    ulong_t context;
    lock_t *sv_lock;
    if (afs_shuttingdown != AFS_RUNNING)
	return;

    /*
     * In Solaris and HPUX s800 and HP-UX10.0 they actually call us with
     * v_count 1 on last reference!
     */
    MP_H_SPINLOCK_USAV(vn_h_sl_pool, vp, &sv_lock, &context);
    if (avc->vrefCount < 1)
	osi_Panic("afs_inactive : v_count < 1\n");

    /*
     * If more than 1 don't unmap the vnode but do decrement the ref count
     */
    vp->v_count--;
    if (vp->v_count > 0) {
	MP_SPINUNLOCK_USAV(sv_lock, context);
	return 0;
    }
    MP_SPINUNLOCK_USAV(sv_lock, context);
    afs_InactiveVCache(avc, acred);
    return 0;
}


int
mp_afs_open(struct vnode **avcp, int aflags, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_open(avcp, aflags, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_close(struct vnode *avcp, int aflags, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_close(avcp, aflags, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_rdwr(struct vnode *avcp, struct uio *uio, enum uio_rw arw,
	    int aio, afs_ucred_t *acred)
{
    int code;
    long save_resid;

    AFS_GLOCK();
    save_resid = uio->uio_resid;
    code = afs_rdwr(avcp, uio, arw, aio, acred);
    if (arw == UIO_WRITE && code == ENOSPC) {
	/* HP clears code if any data written. */
	uio->uio_resid = save_resid;
    }
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_getattr(struct vnode *avcp, struct vattr *attrs,
	       afs_ucred_t *acred, enum vsync unused1)
{
    int code;

    AFS_GLOCK();
    code = afs_getattr(avcp, attrs, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_setattr(struct vnode *avcp, struct vattr *attrs,
	       afs_ucred_t *acred, int unused1)
{
    int code;

    AFS_GLOCK();
    code = afs_setattr(avcp, attrs, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_access(struct vnode *avcp, int mode, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_access(avcp, mode, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_lookup(struct vnode *adp, char *aname,
	      struct vnode **avcp, afs_ucred_t *acred,
	      struct vnode *unused1)
{
    int code;

    AFS_GLOCK();
    code = afs_lookup(adp, aname, avcp, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_create(struct vnode *adp, char *aname, struct vattr *attrs,
	      enum vcexcl aexcl, int amode, struct vnode **avcp,
	      afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_create(adp, aname, attrs, aexcl, amode, avcp, acred);
    AFS_GUNLOCK();
    return (code);
}


int
mp_afs_remove(struct vnode *adp, char *aname,
	      afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_remove(adp, aname, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_link(struct vnode *avc, struct vnode *adp,
	    char *aname, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_link(avc, adp, aname, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_rename(struct vnode *aodp, char *aname1,
	      struct vnode *andp, char *aname2,
	      afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_rename(aodp, aname1, andp, aname2, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_mkdir(struct vnode *adp, char *aname, struct vattr *attrs,
	     struct vnode **avcp, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_mkdir(adp, aname, attrs, avcp, acred);
    AFS_GUNLOCK();
    return (code);
}


int
mp_afs_rmdir(struct vnode *adp, char *aname, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_rmdir(adp, aname, acred);
    AFS_GUNLOCK();
    return (code);
}


int
mp_afs_readdir(struct vnode *avc, struct uio *auio,
	       afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_readdir(avc, auio, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_symlink(struct vnode *adp, char *aname, struct vattr *attrs,
	       char *atargetName, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_symlink(adp, aname, attrs, atargetName, NULL, acred);
    AFS_GUNLOCK();
    return (code);
}


int
mp_afs_readlink(struct vnode *avc, struct uio *auio,
		afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_readlink(avc, auio, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_fsync(struct vnode *avc, afs_ucred_t *acred, int unused1)
{
    int code;

    AFS_GLOCK();
    code = afs_fsync(avc, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_bread(struct vnode *avc, kern_daddr_t lbn, struct buf **bpp,
	     struct vattr *unused1, struct ucred *unused2)
{
    int code;

    AFS_GLOCK();
    code = afs_bread(avc, lbn, bpp);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_brelse(struct vnode *avc, struct buf *bp)
{
    int code;

    AFS_GLOCK();
    code = afs_brelse(avc, bp);
    AFS_GUNLOCK();
    return (code);
}


int
mp_afs_inactive(struct vnode *avc, afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_inactive(avc, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_lockctl(struct vnode *avc, struct flock *af, int cmd,
	       afs_ucred_t *acred, struct file *unused1, off_t unused2,
	       off_t unused3)
{
    int code;

    AFS_GLOCK();
    code = afs_lockctl(avc, af, cmd, acred);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_fid(struct vnode *avc, struct fid **fidpp)
{
    int code;

    AFS_GLOCK();
    code = afs_fid(avc, fidpp);
    AFS_GUNLOCK();
    return (code);
}

int
mp_afs_readdir2(struct vnode *avc, struct uio *auio,
		afs_ucred_t *acred)
{
    int code;

    AFS_GLOCK();
    code = afs_readdir2(avc, auio, acred);
    AFS_GUNLOCK();
    return (code);
}


struct vnodeops Afs_vnodeops = {
    mp_afs_open,
    mp_afs_close,
    mp_afs_rdwr,
    afs_ioctl,
    afs_noop,
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
    afs_readdir,
    mp_afs_symlink,
    mp_afs_readlink,
    mp_afs_fsync,
    mp_afs_inactive,
    afs_bmap,
    afs_hp_strategy,
#if	!defined(AFS_NONFSTRANS)
    /* on HPUX102 the nfs translator calls afs_bread but does
     * not call afs_brelse. Hence we see a memory leak. If the
     * VOP_BREAD() call fails, then nfs does VOP_RDWR() to get
     * the same data : this is the path we follow now. */
    afs_noop,
    afs_noop,
#else
    mp_afs_bread,
    mp_afs_brelse,
#endif
    afs_badop,			/* pathsend */
    afs_noop,			/* setacl */
    afs_noop,			/* getacl */
    afs_pathconf,
    afs_pathconf,
    mp_afs_lockctl,
    afs_lockf,			/* lockf */
    mp_afs_fid,
    afs_noop,			/*fsctl */
    afs_badop,
    afs_pagein,
    afs_pageout,
    NULL,
    NULL,
    afs_prealloc,
    afs_mapdbd,
    afs_mmap,
    afs_cachelimit,
    afs_vm_checkpage,
    afs_vm_fscontiguous,
    afs_vm_stopio,
    afs_read_ahead,
    afs_release,
    afs_unmap,
    afs_swapfs_len,
    mp_afs_readdir2,
    afs_readdir3,
};

struct vnodeops *afs_ops = &Afs_vnodeops;

/* vnode file operations, and our own */
extern int vno_rw();
extern int vno_ioctl();
extern int vno_select();
extern int afs_closex();
extern int vno_close();
struct fileops afs_fileops = {
    vno_rw,
    vno_ioctl,
    vno_select,
    afs_close,
};

#define vtoblksz(vp)	((vp)->v_vfsp->vfs_bsize)

/*
 ********************************************************************
 ****
 ****                   afspgin_setup_io_ranges ()
 ****    similar to:    nfspgin_setup_io_ranges ()
 ********************************************************************
 */
pgcnt_t
afspgin_setup_io_ranges(vfspage_t * vm_info, pgcnt_t bpages, k_off_t isize,
			pgcnt_t startindex)
{
    pgcnt_t file_offset = VM_FILE_OFFSET(vm_info);
    pgcnt_t minpage;		/* first page to bring in */
    pgcnt_t maxpage;		/* one past last page to bring in */
    pgcnt_t maxpagein;
    pgcnt_t multio_maxpage;
    kern_daddr_t start_blk;
    dbd_t *dbd;
    expnd_flags_t up_reason, down_reason;
    int count = 1;
    int indx = 0;
    int max_num_io;
    int dbdtype;
    preg_t *prp;

    VM_GET_IO_INFO(vm_info, maxpagein, max_num_io);

    /*
     * We do not go past the end of the current pregion nor past the end
     * of the current file.
     */

    maxpage = startindex + (bpages - (startindex + file_offset) % bpages);
    maxpage = vm_reset_maxpage(vm_info, maxpage);
    maxpage = MIN(maxpage, (pgcnt_t) btorp(isize) - file_offset);
    maxpage = MIN(maxpage, startindex + maxpagein);
    multio_maxpage = maxpage = vm_maxpage(vm_info, maxpage);

    if (!maxpage)
	return (0);

    VASSERT(maxpage >= startindex);

    /*
     * Expanding the fault will create calls to FINDENTRY() for new
     * pages, which will obsolete "dbd", so copy what it points to
     * and clear it to prevent using stale data.
     */

    prp = VM_PRP(vm_info);
    dbdtype = DBD_TYPE(vm_info);
    start_blk = DBD_DATA(vm_info);
    vm_info->dbd = NULL;
    vm_info->vfd = NULL;
    VASSERT(dbdtype != DBD_NONE);

    if (max_num_io == 1) {
	/*
	 * We need to set up one I/O: First we attempt to expand the
	 * I/O forward. Then we expand the I/O backwards.
	 */
	count =
	    expand_faultin_up(vm_info, dbdtype, (int)bpages, maxpage, count,
			      startindex, start_blk, &up_reason);
	maxpage = startindex + count;
	VASSERT(maxpage <= startindex + maxpagein);
	minpage = startindex - (startindex + file_offset) % bpages;
	minpage = MAX(minpage, maxpage - maxpagein);
	VASSERT(startindex >= VM_BASE_OFFSET(vm_info));
	minpage = vm_minpage(vm_info, minpage);
	VASSERT(minpage <= startindex);
	count =
	    expand_faultin_down(vm_info, dbdtype, (int)bpages, minpage, count,
				&startindex, &start_blk, &down_reason);
	VM_SET_IO_STARTINDX(vm_info, 0, startindex);
	VM_SET_IO_STARTBLK(vm_info, 0, start_blk);
	VM_SET_IO_COUNT(vm_info, 0, count);
	VM_SET_NUM_IO(vm_info, 1);
    }

    if (max_num_io > 1) {
	/*
	 * We need to set up multiple I/O information; beginning
	 * with the startindex, we will expand upwards. The expansion
	 * could stop for one of 2 reasons; we take the appropriate
	 * action in each of these cases:
	 *      o VM reasons: abort setting up the multiple I/O
	 *        information and return to our caller indicating
	 *        that "retry" is required.
	 *      o pagelimit: set up the next I/O info [we may have
	 *        reached multio_maxpage at this point].
	 * Note that expansion involves no more than a block at a time;
	 * hence it could never stop due to "discontiguous block"
	 * reason.
	 */
	startindex = minpage = vm_minpage(vm_info, 0);
	for (indx = 0; (indx < max_num_io) && (startindex < multio_maxpage);
	     indx++, startindex += count) {
	    dbd = FINDDBD(prp->p_reg, startindex);
	    start_blk = dbd->dbd_data;
	    maxpage =
		startindex + (bpages - (startindex + file_offset) % bpages);
	    maxpage = min(maxpage, multio_maxpage);
	    count =
		expand_faultin_up(vm_info, dbdtype, bpages, maxpage,
				  1 /* count */ ,
				  startindex, start_blk, &up_reason);
	    VM_SET_IO_STARTINDX(vm_info, indx, startindex);
	    VM_SET_IO_STARTBLK(vm_info, indx, start_blk);
	    VM_SET_IO_COUNT(vm_info, indx, count);
	    if (up_reason & VM_REASONS)
		break;
	    VASSERT(!(up_reason & NONCONTIGUOUS_BLOCK));
	    VASSERT(up_reason & PAGELIMIT);
	}
	if (startindex < multio_maxpage) {
	    VM_MULT_IO_FAILURE(vm_info);
	    VM_REINIT_FAULT_DBDVFD(vm_info);
	    return (0);		/* retry */
	}
	count = maxpagein;
	VM_SET_NUM_IO(vm_info, indx);
    }

    /*
     * Tell VM where the I/O intends to start.  This may be different
     * from the faulting point.
     */

    VM_SET_STARTINDX(vm_info, VM_GET_IO_STARTINDX(vm_info, 0));

    return (count);

}

/*
 ********************************************************************
 ****
 ****                   afspgin_blkflsh ()
 ****   similar to:     nfspgin_blkflsh ()
 ********************************************************************
 */
retval_t
afspgin_blkflsh(vfspage_t * vm_info, struct vnode * devvp, pgcnt_t * num_4k)
{
    int flush_reslt = 0;
    pgcnt_t count = *num_4k;
    pgcnt_t page_count;
    int indx = 0;
    int num_io = VM_GET_NUM_IO(vm_info);

    /*
     * On this blkflush() we don't want to purge the buffer cache and we do
     * want to wait, so the flags are '0'.
     */

    for (indx = 0; indx < num_io; indx++) {
	flush_reslt =
	    blkflush(devvp, (kern_daddr_t) VM_GET_IO_STARTBLK(vm_info, indx),
		     ptob(VM_GET_IO_COUNT(vm_info, indx)), 0,
		     VM_REGION(vm_info));
	if (flush_reslt) {
	    vm_lock(vm_info);
	    if (vm_page_now_valid(vm_info, &page_count)) {
		vm_release_memory(vm_info);
		vm_release_structs(vm_info);
		*num_4k = page_count;
		return (VM_PAGE_PRESENT);
	    }
	    return (VM_RETRY);
	}
    }
    return (VM_DONE);
}

/*
 ********************************************************************
 ****
 ****                   afspgin_io ()
 ****    similar to:    nfspgin_io ()
 ********************************************************************
 */
int
afspgin_io(vfspage_t * vm_info, struct vnode *devvp, pgcnt_t bpages,
	   pgcnt_t maxpagein, pgcnt_t count)
{
    int i;
    int error = 0;
    caddr_t vaddr = VM_ADDR(vm_info);
    caddr_t virt_addr = VM_MAPPED_ADDR(vm_info);
    pagein_info_t *io = VM_PAGEIN_INFO(vm_info);
    preg_t *prp = VM_PRP(vm_info);
    int wrt = VM_WRT(vm_info);
    space_t space = VM_SPACE(vm_info);
    int num_io = VM_GET_NUM_IO(vm_info);

#ifdef notdef			/* Not used in AFS */
    /*
     * With VM_READ_AHEAD_ALLOWED() macro, check if read-ahead should
     * be used in this case.
     *
     * Unlike UFS, NFS does not start the faulting page I/O
     * asynchronously. Why?  Asynchronous requests are handled by the
     * biod's.  It doesn't make sense to queue up the faulting request
     * behind other asynchrnous requests.  This is not true for UFS
     * where the asynchrnous request is immediately handled.
     */

    if ((VM_READ_AHEAD_ALLOWED(vm_info)) && (nfs_read_ahead_on)
	&& (NFS_DO_READ_AHEAD) && (should_do_read_ahead(prp, vaddr))) {

	pgcnt_t max_rhead_io;
	caddr_t rhead_vaddr;
	pgcnt_t total_rheads_allowed;

	/*
	 * Determine the maximum amount of read-ahead I/O.
	 */
	total_rheads_allowed = maxpagein - count;

	/*
	 * If the count is less than a block, raise it to one.
	 */
	if (total_rheads_allowed < bpages)
	    total_rheads_allowed = bpages;

	max_rhead_io = total_rheads_allowed;
	rhead_vaddr = VM_MAPPED_ADDR(vm_info) + (count * NBPG);
	error =
	    nfs_read_ahead(vm_info->vp, prp, wrt, space, rhead_vaddr,
			   &max_rhead_io);

	/*
	 * Set the next fault location.  If read_ahead launches any
	 * I/O it will adjust it accordingly.
	 */
	vm_info->prp->p_nextfault = vm_info->startindex + count;

	/*
	 * Now perform the faulting I/O synchronously.
	 */
	vm_unlock(vm_info);

	error =
	    syncpageio((swblk_t) VM_GET_IO_STARTBLK(vm_info, 0),
		       VM_MAPPED_SPACE(vm_info), VM_MAPPED_ADDR(vm_info),
		       (int)ptob(count), B_READ, devvp,
		       B_vfs_pagein | B_pagebf, VM_REGION(vm_info));
    } else
#endif
    {
	virt_addr = VM_MAPPED_ADDR(vm_info);
	vm_unlock(vm_info);
	for (i = 0; i < num_io; i++) {
	    /*
	     * REVISIT -- investigate doing asyncpageio().
	     */
	    error |= (io[i].error =
		      syncpageio((swblk_t) VM_GET_IO_STARTBLK(vm_info, i),
				 VM_MAPPED_SPACE(vm_info), virt_addr,
				 (int)ptob(VM_GET_IO_COUNT(vm_info, i)),
				 B_READ, devvp, B_vfs_pagein | B_pagebf,
				 VM_REGION(vm_info)));
	    virt_addr += ptob(VM_GET_IO_COUNT(vm_info, i));
	}
	/*
	 * Set the next fault location.  If read_ahead launches any
	 * I/O it will adjust it accordingly.
	 */
	vm_info->prp->p_nextfault = vm_info->startindex + count;
    }

    return (error);
}

/*
 ********************************************************************
 ****
 ****                   afspgin_update_dbd ()
 ****    similar to:    nfspgin_update_dbd ()
 ********************************************************************
 */
void
afspgin_update_dbd(vfspage_t * vm_info, int bsize)
{
    k_off_t off;
    pgcnt_t count = bsize / NBPG;
    k_off_t rem;
    pgcnt_t m;
    pgcnt_t pgindx;
    kern_daddr_t blkno;
    int num_io = VM_GET_NUM_IO(vm_info);
    int i;

    for (i = 0; i < num_io; i++) {

	pgindx = VM_GET_IO_STARTINDX(vm_info, i);
	off = vnodindx(VM_REGION(vm_info), pgindx);
	rem = off % bsize;
	blkno = VM_GET_IO_STARTBLK(vm_info, i);

	VASSERT(bsize % NBPG == 0);
	VASSERT(rem % NBPG == 0);

	pgindx -= (pgcnt_t) btop(rem);
	blkno -= (kern_daddr_t) btodb(rem);

	/*
	 * This region could start in mid-block.  If so, pgindx
	 * could be less than 0, so we adjust pgindx and blkno back
	 * up so that pgindx is 0.
	 */

	if (pgindx < 0) {
	    pgcnt_t prem;
	    prem = 0 - pgindx;
	    pgindx = 0;
	    count -= prem;
	    blkno += btodb(ptob(prem));
	}

	for (m = 0; m < count && pgindx < VM_REGION_SIZE(vm_info);
	     m++, pgindx++, blkno += btodb(NBPG)) {
	    /*
	     * Note:  since this only changes one block, it
	     * assumes only one block was faulted in.  Currently
	     * this is always true for remote files, and we only
	     * get here for remote files, so everything is ok.
	     */
	    vm_mark_dbd(vm_info, pgindx, blkno);
	}
    }
}

int
afs_pagein(vp, prp, wrt, space, vaddr, ret_startindex)
     struct vnode *vp;
     preg_t *prp;
     int wrt;
     space_t space;
     caddr_t vaddr;
     pgcnt_t *ret_startindex;
{
    pgcnt_t startindex;
    pgcnt_t pgindx = *ret_startindex;
    pgcnt_t maxpagein;
    struct vnode *devvp;
    pgcnt_t count;
    kern_daddr_t start_blk = 0;
    int bsize;
    int error;
    k_off_t isize;
    int shared;			/* writable memory mapped file */
    retval_t retval = 0;
    pgcnt_t ok_dbd_limit = 0;	/* last dbd that we can trust */
    pgcnt_t bpages;		/* number of pages per block */
    pgcnt_t page_count;
    vfspage_t *vm_info = NULL;
    int done;

    struct vattr va;

    caddr_t nvaddr;
    space_t nspace;
    int change_to_fstore = 0;	/* need to change dbds to DBD_FSTORE */
    int flush_start_blk = 0;
    int flush_end_blk = 0;

    int i, j;

    AFS_STATCNT(afs_pagein);
    vmemp_lockx();		/* lock down VM empire */

    /* Initialize the VM info structure */
    done =
	vm_pagein_init(&vm_info, prp, pgindx, space, vaddr, wrt, 0,
		       LGPG_ENABLE);

    /* Check to see if we slept and the page was falted in. */
    if (done) {
	vm_release_structs(vm_info);
	vmemp_returnx(1);
    }

    vp = VM_GET_PAGEIN_VNODE(vm_info);
    VASSERT(vp != NULL);
    shared = VM_SHARED_OBJECT(vm_info);
    VASSERT(DBD_TYPE(vm_info) != DBD_NONE);

    /*
     * Get the devvp and block size for this vnode type
     */
    devvp = vp;
    bsize = vp->v_vfsp->vfs_bsize;
    if (bsize <= 0 || (bsize & (DEV_BSIZE - 1)))
	osi_Panic("afs_pagein: bsize is zero or not a multiple of DEV_BSIZE");

    bpages = (pgcnt_t) btop(bsize);
    VASSERT(bpages > 0);
    VM_SET_FS_MAX_PAGES(vm_info, bpages);

    /* this trace cannot be here because the afs_global lock might not be
     * held at this point. We hold the vm global lock throughout
     * this procedure ( and not the AFS global lock )
     * afs_Trace4(afs_iclSetp, CM_TRACE_HPPAGEIN, ICL_TYPE_POINTER, (afs_int32) vp,
     * ICL_TYPE_LONG, DBD_TYPE(vm_info), ICL_TYPE_LONG, bpages, 
     * ICL_TYPE_LONG, shared);
     */
    /* Come here if we have to release the region lock before
     * locking pages.  This can happen in memreserve() and
     * blkflush().
     */
  retry:
    /*
     * For remote files like ours, we want to check to see if the file has shrunk.
     * If so, we should invalidate any pages past the end.  In the name
     * of efficiency, we only do this if the page we want to fault is
     * past the end of the file.
     */
    {
	if (VOP_GETATTR(vp, &va, kt_cred(u.u_kthreadp), VIFSYNC) != 0) {
	    VM_ZOMBIE_OBJECT(vm_info);
	    vm_release_memory(vm_info);
	    vm_release_structs(vm_info);
	    vmemp_returnx(0);
	}
	isize = va.va_size;
	if (vnodindx(VM_REGION(vm_info), pgindx) >= isize) {
	    /*
	     * The file has shrunk and someone is trying to access a
	     * page past the end of the object.  Shrink the object back
	     * to its currrent size, send a SIGBUS to the faulting
	     * process and return.
	     *
	     * We must release the region lock before calling mtrunc(),
	     * since mtrunc() locks all the regions that are using this
	     * file.
	     */
	    vm_release_memory(vm_info);
	    vm_truncate_region(vm_info, isize);
	    vm_release_structs(vm_info);
	    vmemp_returnx(-SIGBUS);
	}
    }

    maxpagein = vm_pick_maxpagein(vm_info);
    if (vm_wait_for_memory(vm_info, maxpagein, 1)) {
	/* Check to see if we should continue faulting.  */
	if (vm_page_now_valid(vm_info, &page_count)) {
	    vm_release_memory(vm_info);
	    vm_release_structs(vm_info);
	    vmemp_returnx(page_count);
	}
    }
    if (count = vm_no_io_required(vm_info)) {
	/* Release any excess memory.  */
	vm_release_memory(vm_info);
	vm_release_structs(vm_info);
	vmemp_returnx(count);
    }
#ifdef OSDEBUG
    /*
     * We should never have DBD_HOLE pages in a non-MMF region.
     */
    if (!shared)
	VASSERT(dbd->dbd_type != DBD_HOLE);
#endif
    VASSERT(DBD_TYPE(vm_info) != DBD_NONE);

    startindex = *ret_startindex;

    /*
     * If the page we want is in memory already, take it
     */
    if (VM_MEMORY_RESERVED(vm_info) < maxpagein) {
	/* pick up the rest of memory now.  */
	if (vm_wait_for_memory(vm_info, maxpagein, 0)) {
	    if (vm_page_now_valid(vm_info, &page_count)) {
		vm_release_memory(vm_info);
		vm_release_structs(vm_info);
		vmemp_returnx(page_count);
	    }
	    goto retry;
	}
    }

    if (!
	(count =
	 afspgin_setup_io_ranges(vm_info, bpages, isize, startindex))) {
	goto retry;
    }

    startindex = VM_GET_STARTINDX(vm_info);

    VASSERT(maxpagein >= count);

    /*
     * Release the memory we won't need.
     */
    if (count < maxpagein) {
	vm_release_excess_memory(vm_info,
				 (VM_MEMORY_RESERVED(vm_info) - count));
    }

    retval = afspgin_blkflsh(vm_info, devvp, &count);

    if (retval == VM_RETRY) {
	goto retry;
    }

    if (retval == VM_PAGE_PRESENT)
	return (count);

#if 0
    /*
     * The definition of krusage_cntr_t is in h/kmetric.h, which
     * is not shipped.  Since it's just statistics, we punt and do
     * not update it.  If it's a problem we'll need to get HP to export
     * an interface that we can use to increment the counter.
     */

    /* It's a real fault, not a reclaim */
    {
	krusage_cntr_t *temp;
	temp = kt_cntrp(u.u_kthreadp);
	temp->krc_majflt++;
    }
#endif

    /*
     * Tell VM where the I/O intends to start.  This may be different
     * from the faulting point.
     */

    /*
     * vm_prepare_io will fill the region with pages and release the
     * region lock.
     */
    vm_prepare_io(vm_info, &count);

    /*
     * Count may have been adjusted, check to make sure it's non-zero.
     */
    if (count == 0) {
	if (vm_retry(vm_info)) {
	    goto retry;
	}

	/*
	 * Release resources and retry the fault.  Release any excess
	 * memory.
	 */

	vm_release_memory(vm_info);
	vm_release_structs(vm_info);
	vmemp_returnx(0);
    }

    error = afspgin_io(vm_info, devvp, bpages, maxpagein, count);

    if ((VM_IS_ZOMBIE(vm_info)) || (error)) {
	retval = -SIGBUS;
	VM_ZOMBIE_OBJECT(vm_info);
	goto backout;
    }
    /*
     * For a writable memory mapped file that is remote we must
     * detect potential holes in the file and force allocation of
     * disk space on the remote system.  Unfortunately, there is
     * no easy way to do this, so this gets a little ugly.
     */
    if (shared && wrt) {
	/*
	 * See if The user wants to write to this page.  Write some
	 * minimal amount of data back to the remote file to
	 * force allocation of file space.  We only need to
	 * write a small amount, since holes are always at
	 * least one filesystem block in size.
	 */
	error = vm_alloc_hole(vm_info);

	/*
	 * If some sort of I/O error occurred we generate a
	 * SIGBUS for the process that caused the write,
	 * undo our page locks, etc and return.
	 */
	if ((VM_IS_ZOMBIE(vm_info)) || (error)) {
	    VM_ZOMBIE_OBJECT(vm_info);
	    retval = -SIGBUS;
	    goto backout;
	}

	/*
	 * Change these dbds to DBD_FSTORE.  We cannot do it here,
	 * since the region must be locked, and it is not locked
	 * at the moment.  We cannot lock the region yet, as we
	 * first have to release the page locks.
	 */
	change_to_fstore = 1;
    }

    vm_finish_io(vm_info, count);

    /*
     * Acquire the lock before we play around with changing the vfd's.
     */
    vm_lock(vm_info);

    if (change_to_fstore)
	afspgin_update_dbd(vm_info, bsize);

#if defined(AFS_HPUX110_ENV)
    getppdp()->cnt.v_exfod += count;
#else
    mpproc_info[getprocindex()].cnt.v_exfod += count;
#endif
    vmemp_unlockx();		/* free up VM empire */
    *ret_startindex = startindex;

    /*
     * In case we have any excess memory...
     */
    if (VM_MEMORY_RESERVED(vm_info))
	vm_release_memory(vm_info);
    vm_release_structs(vm_info);

    return count;

  backout:

    vm_finish_io_failed(vm_info, count);

    vm_lock(vm_info);

    vm_undo_validation(vm_info, count);

    /*
     * In case we have any excess memory...
     */
    if (VM_MEMORY_RESERVED(vm_info))
	vm_release_memory(vm_info);
    vm_release_structs(vm_info);

    vmemp_unlockx();		/* free up VM empire */
    return retval;
}

int
afs_pageout(vp, prp, start, end, flags)
     struct vnode *vp;		/* not used */
     preg_t *prp;
     pgcnt_t start;
     pgcnt_t end;
     int flags;
{
    struct vnode *filevp;
    struct vnode *devvp;
    pgcnt_t i;
    int steal;
    int vhand;
    int hard;
    int *piocnt;		/* wakeup counter used if PAGEOUT_WAIT */
    struct ucred *old_cred;
    vfspage_t vm_info;
    fsdata_t args;

    int inode_changed = 0;
    int file_is_remote;
    struct inode *ip;

    AFS_STATCNT(afs_pageout);

    steal = (flags & PAGEOUT_FREE);
    vhand = (flags & PAGEOUT_VHAND);
    hard = (flags & PAGEOUT_HARD);

    vmemp_lockx();

    /*  Initialize the VM info structure.  */
    vm_pageout_init(&vm_info, prp, start, end, 0, 0, 0, flags);

    /*
     * If the region is marked "don't swap", then don't steal any pages
     * from it.  We can, however, write dirty pages out to disk (only if
     * PAGEOUT_FREE is not set).
     */
    if (vm_no_pageout(&vm_info)) {
	vmemp_unlockx();
	return (0);
    }

    /*
     * If caller wants to wait until the I/O is complete.
     */
    vm_setup_wait_for_io(&vm_info);

    filevp = VM_GET_PAGEOUT_VNODE(&vm_info);	/* always page out to back store */
    VASSERT(filevp != NULL);

    memset((caddr_t) & args, 0, sizeof(fsdata_t));
    args.remote_down = 0;	/* assume remote file servers are up */
    args.remote = 1;		/* we are remote */
    args.bsize = 0;		/* filled up later by afs_vm_checkpage() */

    if (filevp->v_fstype == VUFS) {
	ip = VTOI(filevp);
	devvp = ip->i_devvp;
	file_is_remote = 0;
    } else {
	file_is_remote = 1;
	devvp = filevp;

	/*
	 * If we are vhand(), and this is an NFS file, we need to
	 * see if the NFS server is "down".  If so, we decide
	 * if we will try to talk to it again, or defer pageouts
	 * of dirty NFS pages until a future time.
	 */
#ifdef	notdef
	if (vhand && filevp->v_fstype == VNFS && vtomi(filevp)->mi_down
	    && vtomi(filevp)->mi_hard) {
	    extern afs_int32 vhand_nfs_retry;
	    /*
	     * If there is still time left on our timer, we will
	     * not talk to this server right now.
	     */
	    if (vhand_nfs_retry > 0)
		args.remote_down = 1;
	}
#endif
    }

    /*
     * Initialize args.  We set bsize to 0 to tell vfs_vfdcheck() that
     * it must get the file size and other attributes if it comes across
     * a dirty page.
     */
    vm_info.fs_data = (caddr_t) & args;

    /* this trace cannot be here because the afs_global lock might not be
     * held at this point. We hold the vm global lock throughout
     * this procedure ( and not the AFS global lock )
     * afs_Trace4(afs_iclSetp, CM_TRACE_HPPAGEOUT, ICL_TYPE_POINTER, (afs_int32) filevp,
     * ICL_TYPE_LONG, start, ICL_TYPE_LONG, end, ICL_TYPE_LONG, flags);
     */

    i = start;

    while (i <= end) {
	struct buf *bp;
	k_off_t start;
	pgcnt_t npages;
	k_off_t nbytes;
	int error;

	extern int pageiodone();
	space_t nspace;
	caddr_t nvaddr;

	/*
	 * Ask the VM system to find the next run of pages.
	 */
	vm_find_next_range(&vm_info, i, end);

	/*
	 * It's possible that the remote file shrunk in size.  Check the flags
	 * to see if the request was beyond the end of the file.  If it was,
	 * truncate the region to the file size and continue.  We could be on a
	 * run so after trunction continue, there may be some I/O to write
	 * out.
	 */
	if (VM_FS_FLAGS(&vm_info) & PAGEOUT_TRUNCATE) {
	    pgcnt_t pglen = (pgcnt_t) btorp(args.isize);

	    /*
	     * This page is past the end of the file.  Unlock this page
	     * (region_trunc will throw it away) and then call
	     * region_trunc() to invalidate all pages past the new end of
	     * the file.
	     */
	    region_trunc(VM_REGION(&vm_info), pglen, pglen + 1);

	    /*
	     * remove the truncation flag.
	     */
	    VM_UNSETFS_FLAGS(&vm_info, PAGEOUT_TRUNCATE);
	}

	if (VM_NO_PAGEOUT_RUN(&vm_info))
	    break;

	/*
	 * We have a run of dirty pages [args.start...args.end].
	 */
	VASSERT(filevp->v_fstype != VCDFS);
	VASSERT((filevp->v_vfsp->vfs_flag & VFS_RDONLY) == 0);
	VASSERT(VM_GET_NUM_IO(&vm_info) == 1);

	/*
	 * We will be doing an I/O on the region, let the VM system know.
	 */
	(void)vm_up_physio_count(&vm_info);

	/*
	 * Okay, get set to perform the I/O.
	 */
	inode_changed = 1;
	npages =
	    (VM_END_PAGEOUT_INDX(&vm_info) + 1) -
	    VM_START_PAGEOUT_INDX(&vm_info);

	/*
	 * Allocate and initialize an I/O buffer.
	 */
	bp = bswalloc();
	vm_init_bp(&vm_info, bp);	/* Let the VM system initialize */

	/* Identify this buffer for KI */
	bp->b_bptype = B_vfs_pageout | B_pagebf;

	if (steal)
	    bp->b_flags = B_CALL | B_BUSY | B_PAGEOUT;	/* steal pages */
	else
	    bp->b_flags = B_CALL | B_BUSY;	/* keep pages */

	/*
	 * If we are vhand paging over NFS, we will wait for the I/O
	 * to complete.  
	 */
	if (vhand && filevp->v_fstype == VNFS) {
	    bp->b_flags &= ~B_CALL;
	} else {
	    bp->b_iodone = (int (*)())pageiodone;
	}

	/*
	 * Make sure we do not write past the end of the file.
	 */
	nbytes = ptob(npages);
	start = vnodindx(VM_REGION(&vm_info), vm_info.start);
	if (start + nbytes > args.isize) {
#ifdef OSDEBUG
	    /*
	     * The amount we are off better not be bigger than a
	     * filesystem block.
	     */
	    if (start + nbytes - args.isize >= args.bsize) {
		osi_Panic("afs_pageout: remainder too large");
	    }
#endif
	    /*
	     * Reset the size of the I/O as necessary.  For remote
	     * files, we set the size to the exact number of bytes to
	     * the end of the file.  For local files, we round this up
	     * to the nearest DEV_BSIZE chunk since disk I/O must always
	     * be in multiples of DEV_BSIZE.  In this case, we do not
	     * bother to zero out the data past the "real" end of the
	     * file, this is done when the data is read (either through
	     * mmap() or by normal file system access).
	     */
	    if (file_is_remote)
		nbytes = args.isize - start;
	    else
		nbytes = roundup(args.isize - start, DEV_BSIZE);
	}

	/*
	 * Now get ready to perform the I/O
	 */
	if (!vm_protect_pageout(&vm_info, npages)) {
	    VASSERT(vhand);
	    vm_undo_invalidation(&vm_info, vm_info.start, vm_info.end);
	    vm_finish_io_failed(&vm_info, npages);
	    bswfree(bp);
	    break;
	}
	/*
	 * If this is an NFS write by vhand(), we will not be calling
	 * pageiodone().  asyncpageio() increments parolemem for us
	 * if bp->b_iodone is pageiodone, so we must do it manually
	 * if pageiodone() will not be called automatically.
	 */
	if (!(bp->b_flags & B_CALL) && steal) {
	    ulong_t context;

	    SPINLOCK_USAV(pfdat_lock, context);
	    parolemem += btorp(nbytes);
	    SPINUNLOCK_USAV(pfdat_lock, context);
	}
	blkflush(devvp, VM_START_PAGEOUT_BLK(&vm_info), (long)nbytes,
		 (BX_NOBUFWAIT | BX_PURGE), VM_REGION(&vm_info));

	/*
	 * If vhand is the one paging things out, and this is an NFS
	 * file, we need to temporarily become a different user so
	 * that we are not trying to page over NFS as root.  We use
	 * the user credentials associated with the writable file
	 * pointer that is in the psuedo-vas for this MMF.
	 *
	 * NOTE: we are currently using "va_rss" to store the ucred
	 *       value in the vas (this should be fixed in 10.0).
	 */
	old_cred = kt_cred(u.u_kthreadp);
	if (vhand) {
#if defined(AFS_HPUX1123_ENV)
		/*
		 * DEE - 1123 does not have the vas.h, and it looks
		 * we should never be called with a NFS type file anyway.
		 * so where did this come from? Was it copied from NFS?
		 * I assume it was, so we will add an assert for now
		 * and see if the code runs at all.
		 */
		VASSERT(filevp->v_fstype != VNFS);
#else
	    set_kt_cred(u.u_kthreadp, filevp->v_vas->va_cred);

	    /*
	     * If root was the one who opened the mmf for write,
	     * va_cred will be NULL.  So reset kt_cred(u.u_kthreadp) to what it
	     * was.  We will page out as root, but that is the
	     * correct thing to do in this case anyway.
	     */
	    if (kt_cred(u.u_kthreadp) == NULL)
		set_kt_cred(u.u_kthreadp, old_cred);
#endif
	}

	/*
	 * Really do the I/O.
	 */
	error =
	    asyncpageio(bp, VM_START_PAGEOUT_BLK(&vm_info),
			VM_MAPPED_SPACE(&vm_info), VM_MAPPED_ADDR(&vm_info),
			(int)nbytes, B_WRITE, devvp);

	VASSERT(error == 0);

#ifdef	notdef
	/*
	 * If we are vhand paging over NFS we want to wait for the
	 * I/O to complete and take the appropriate actions if an
	 * error is encountered.
	 */
	if (vhand) {
	    if (waitforpageio(bp) && nfs_mi_harddown(filevp)) {
		/*
		 * The server is down, ignore this failure, and
		 * try again later. (rfscall() has set our retry
		 * timer).
		 */
		fsdata.remote_down = 1;
		pageiocleanup(bp, 0);

		/*
		 * vm_vfdcheck() has cleared the valid bit on the
		 * vfds for these pages.  We must go back and set the
		 * valid bit, as the pages are really not gone.
		 *
		 * NOTE: we can do this because we still hold (and have
		 * not released) the region lock.
		 */
		if (steal)
		    vm_undo_invalidation(&vm_info, vm_info.start,
					 vm_info.end);
	    } else {
		/*
		 * The I/O succeeded, or we had an error that we do
		 * not want to defer until later.  Call pageidone()
		 * to handle things.
		 */
		pageiodone(bp);
	    }
	}
#endif

	/*
	 * And restore our credentials to what they were.
	 */
	set_kt_cred(u.u_kthreadp, old_cred);

	/*
	 * If we reserved memory in vfs_vfdcheck(), (only for NFS) we
	 * can now unreserve it.
	 */
	if (vm_info.vm_flags & PAGEOUT_RESERVED) {
	    vm_info.vm_flags &= ~PAGEOUT_RESERVED;
	    vm_release_malloc_memory();
	}

	/*
	 * Update statistics
	 */
	if (steal) {
	    if (flags & PF_DEACT) {
#if defined(AFS_HPUX110_ENV)
		getppdp()->cnt.v_pswpout += npages;
#else
		mpproc_info[getprocindex()].cnt.v_pswpout += npages;
#endif
/*		sar_bswapout += ptod(npages);*/
	    } else if (vhand) {
#if defined(AFS_HPUX110_ENV)
		getppdp()->cnt.v_pgout++;
		getppdp()->cnt.v_pgpgout += npages;
#else
		mpproc_info[getprocindex()].cnt.v_pgout++;
		mpproc_info[getprocindex()].cnt.v_pgpgout += npages;
#endif
	    }
	}

	/*
	 * If time and patience have delivered enough
	 * pages, then quit now while we are ahead.
	 */
	if (VM_STOP_PAGING(&vm_info))
	    break;

	i = VM_END_PAGEOUT_INDX(&vm_info) - VM_BASE_OFFSET(&vm_info) + 1;
    }

    vm_finish_pageout(&vm_info);	/* update vhand's stealscan */

    vmemp_unlockx();

    /*
     * If we wanted to wait for the I/O to complete, sleep on piocnt.
     * We must decrement it by one first, and then make sure that it
     * is non-zero before going to sleep.
     */
    vm_wait_for_io(&vm_info);

    if (inode_changed && !file_is_remote) {
	imark(ip, IUPD | ICHG);
	iupdat(ip, 0, 0);
    }
    return 0;
}

int
afs_mapdbd(filevp, offset, bn, flags, hole, startidx, endidx)
     struct vnode *filevp;
     off_t offset;
     kern_daddr_t *bn;		/* Block number. */
     int flags;			/* B_READ or B_WRITE */
     int *hole;			/* To be used for read-ahead. */
     pgcnt_t *startidx;		/* To be used for read-ahead. */
     pgcnt_t *endidx;		/* To be used for read-ahead. */
{
    kern_daddr_t lbn, local_bn;
    int on;
    int err;
    long bsize = vtoblksz(filevp) & ~(DEV_BSIZE - 1);

    if (startidx)
	*startidx = (pgcnt_t) (offset / NBPG);
    if (endidx)
	*endidx = (pgcnt_t) (offset / NBPG);
    if (hole)
	*hole = 0;		/* Can't have holes. */
    if (bsize <= 0)
	osi_Panic("afs_mapdbd: zero size");

    lbn = (kern_daddr_t) (offset / bsize);
    on = offset % bsize;

    err = VOP_BMAP(filevp, lbn, NULL, &local_bn, flags);
    VASSERT(err == 0);

    /*
     * We can never get a bn less than zero on remote files.
     */
    VASSERT(local_bn >= 0);

    local_bn = local_bn + btodb(on);
    *bn = local_bn;

    return (0);
}

/*
 * Return values:
 *      1: The blocks are contiguous.
 *      0: The blocks are not contiguous.
 */
int
afs_vm_fscontiguous(vp, args, cur_data)
     struct vnode *vp;
     vfspage_t *args;
     u_int cur_data;
{
    if (cur_data == (VM_END_PAGEOUT_BLK(args) + btodb(NBPG))) {
	return (1);
    } else {
	return (0);
    }
}

/*
 * Return values:
 *      1: Stop, this page is the last in the block.
 *      0: Continue on
 * Terminate requests at filesystem block boundaries
 */
afs_vm_stopio(vp, args)
     struct vnode *vp;
     vfspage_t *args;
{
    fsdata_t *fsdata = (fsdata_t *) args->fs_data;

#if defined(AFS_HPUX1123_ENV)
	uint64_t tmpdb;
	tmpdb = VM_END_PAGEOUT_BLK(args);

	if ((dbtob(tmpdb) + NBPG) % (fsdata->bsize) == 0)
#else
    if ((dbtob(VM_END_PAGEOUT_BLK(args)) + NBPG) % (fsdata->bsize) == 0) 
#endif /* AFS_HPUX1123_ENV */
	{
	return (1);
    } else {
	return (0);
    }
}

/*
 *      afs_vm_checkpage is called by the VM while collecting a run of
 *      pages on a pageout.  afs_vm_checkpage() is called for each page
 *      VM wants to write to disk.
 */
afs_vm_checkpage(vp, args, pgindx, cur_data)
     struct vnode *vp;
     vfspage_t *args;
     pgcnt_t pgindx;
     int cur_data;
{
    fsdata_t *fsdata = (fsdata_t *) args->fs_data;

    if (fsdata->remote_down) {	/* never happens for AFS */
	/*
	 * The remote system is down.
	 */
	VASSERT(args->run == 0);
	return 1;
    }
    /*
     * A dirty page.  If we have not yet determined the file size and
     * other attributes that we need to write out pages (the block
     * size and ok_dbd_limit), get that information now.
     */
    if (fsdata->bsize == 0) {
	k_off_t isize;
	long bsize;
	struct vattr va;
	struct vnode *filevp;
	/*
	 * Get the various attributes about the file.  Store them
	 * in args for the next time around.
	 */
	filevp = args->vp;

	bsize = vtoblksz(filevp);
	args->maxpgs = (pgcnt_t) btop(bsize);

	if (VOP_GETATTR(filevp, &va, kt_cred(u.u_kthreadp), VIFSYNC) != 0) {
	    /*
	     * The VOP_GETATTR() failed.  
	     * we are vhand, and this is a hard mount, we will
	     * skip dirty pages for a while and try again later.
	     */
	    if (args->vm_flags & PAGEOUT_VHAND) {
		VASSERT(args->run == 0);
		return 1;
	    }
	    /*
	     * This is a "soft" mount, or some other error was
	     * returned from the server.  Mark this region
	     * as a zombie, and free this dirty page.
	     */
	    VM_ZOMBIE_OBJECT(args);

	    /*
	     * The caller will see r_zomb and remove the page
	     * appropriately.
	     */
	    return (1);
	}
	isize = va.va_size;
	fsdata->isize = isize;
	fsdata->bsize = bsize;
	fsdata->remote = 1;
    }
    /*
     * See if the file has shrunk (this could have happened
     * asynchronously because of NFS or DUX).  If so, invalidate
     * all of the pages past the end of the file. This is only
     * needed for remote files, as local files are truncated
     * synchronously.
     */

    if (vnodindx(VM_REGION(args), pgindx) > fsdata->isize) {
	/*
	 * This page is past the end of the file.  Unlock this page
	 * (region_trunc will throw it away) and then call region_trunc()
	 * to invalidate all pages past the new end of the file.
	 */
	VM_SETFS_FLAGS(args, PAGEOUT_TRUNCATE);
	return (1);
    }
#ifdef notdef
    if ((args->vm_flags & PAGEOUT_VHAND)
	&& (!(args->vm_flags & PAGEOUT_RESERVED))
	&& (!(VM_IS_ZOMBIE(args)))) {
	VASSERT(args->run == 0);
	if (vm_reserve_malloc_memory(NFS_PAGEOUT_MEM)) {
	    /*
	     * Got enough memory to pageout.  Mark the fact that we did
	     * a sysprocmemreserve(), so that we can sysprocmemunreserve() it
	     * later (in remote_pageout()).
	     */
	    args->vm_flags |= PAGEOUT_RESERVED;
	} else {
	    /*
	     * We do not have enough memory to do this pageout.  By
	     * definition, we do not yet have a run, so we just unlock
	     * this page and tell foreach_valid() to continue scanning.
	     * If we come across another dirty page, we will try to
	     * reserve memory again.  That is okay, in fact some memory
	     * may have freed up (as earlier pageouts complete under
	     * interrupt).
	     */
	    return 1;
	}
    }
#endif
    return (0);
}

afs_swapfs_len(bp)
     struct buf *bp;
{
    long fs_bsize;
    long max_size;
    long bnrem;

    fs_bsize = vtoblksz(bp->b_vp);
    /*
     * Check to see if we are starting mid block.  If so, then
     * we must return the remainder of the block or less depending
     * on the length.
     */
    bnrem = bp->b_offset % fs_bsize;
    if (bnrem) {
	max_size = fs_bsize - bnrem;
    } else {
	max_size = fs_bsize;
    }

    if (bp->b_bcount > max_size) {
	return (max_size);
    } else {
	return (bp->b_bcount);
    }
}

afs_mmap(vp, off, size_bytes, access)
     struct vnode *vp;
     u_int off;
#if defined(AFS_HPUX1111_ENV)
     u_long size_bytes;
#else
     u_int size_bytes;
#endif
     int access;
{
    long bsize = vtoblksz(vp);

    if (bsize % NBPG != 0) {
	return (EINVAL);
    }

    return (0);
}

afs_cachelimit(vp, len, location)
     struct vnode *vp;
     k_off_t len;
     int *location;
{
    /*
     * Disk addresses are logical, not physical, so fragments are
     * transparent.
     */
    *location = btorp(len) + 1;
}

afs_release(vp)
     struct vnode *vp;
{
    return (0);
}

int
afs_unmap(vp, off, size_bytes, access)
     struct vnode *vp;
     u_int off;
#if defined(AFS_HPUX1111_ENV)
     u_long size_bytes;
#else
     u_int size_bytes;
#endif
     int access;
{
    return 0;
}

int
afs_read_ahead(vp, prp, wrt, space, vaddr, rhead_cnt)
     struct vnode *vp;
     preg_t *prp;
     int wrt;
     space_t space;
     caddr_t vaddr;
     pgcnt_t *rhead_cnt;
{
    printf("afs_read_ahead returning 0 \n");
    return 0;
}

int
afs_prealloc(vp, size, ignore_minfree, reserved)
     struct vnode *vp;
      /* DEE on 11.22 following is off_t */
     size_t size;
     int ignore_minfree;
     int reserved;
{
    printf("afs_prealloc returning ENOSPC\n");
    return ENOSPC;
}

int
afs_ioctl(vp, com, data, flag, cred)
     struct vnode *vp;
     int com;
     caddr_t data;
     int flag;
     struct ucred *cred;
{
    int error;
    struct afs_ioctl afsioctl, *ai;

    AFS_STATCNT(afs_ioctl);

    /* The call must be a VICEIOCTL call */
    if (((com >> 8) & 0xff) == 'V') {
#ifdef notdef
	/* AFS_COPYIN returns error 14. Copy data in instead */
	AFS_COPYIN(data, (caddr_t) & afsioctl, sizeof(afsioctl), error);
	if (error)
	    return (error);
#endif
	ai = (struct afs_ioctl *)data;
	afsioctl.in = ai->in;
	afsioctl.out = ai->out;
	afsioctl.in_size = ai->in_size;
	afsioctl.out_size = ai->out_size;
	error = HandleIoctl(VTOAFS(vp), com, &afsioctl);
	return (error);
    }
    return (ENOTTY);
}

#if defined(AFS_HPUX1111_ENV)
/* looks like even if appl is 32 bit, we need to round to 8 bytes */
/* This had no effect, it must not be being used */

#define roundtoint(x)   (((x) + (sizeof(long) - 1)) & ~(sizeof(long) - 1))
#define reclen(dp)      roundtoint(((dp)->d_namlen + 1 + (sizeof(u_long)) +\
                                sizeof(u_int) + 2 * sizeof(u_short)))
#else

#define roundtoint(x)   (((x) + (sizeof(int) - 1)) & ~(sizeof(int) - 1))
#define reclen(dp)      roundtoint(((dp)->d_namlen + 1 + (sizeof(u_long)) +\
                                2 * sizeof(u_short)))
#endif

int
afs_readdir(vp, uiop, cred)
     struct vnode *vp;
     struct uio *uiop;
     struct ucred *cred;
{
    struct uio auio;
    struct iovec aiov;
    caddr_t ibuf, obuf, ibufend, obufend;
    struct __dirent32 *idp;
    struct dirent *odp;
    int count, outcount;
    dir_off_t offset;
    uint64_t tmp_offset;

    memset(&auio, 0, sizeof(auio));
    memset(&aiov, 0, sizeof(aiov));

    count = uiop->uio_resid;
    /* Allocate temporary space for format conversion */
    ibuf = kmem_alloc(2 * count);	/* overkill - fix later */
    obuf = kmem_alloc(count + sizeof(struct dirent));
    aiov.iov_base = ibuf;
    aiov.iov_len = count;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    offset = auio.uio_offset = uiop->uio_offset;
    auio.uio_seg = UIOSEG_KERNEL;
    auio.uio_resid = count;
    auio.uio_fpflags = 0;

    u.u_error = mp_afs_readdir2(vp, &auio, cred);
    if (u.u_error)
	goto out;

    /* Convert entries from __dirent32 to dirent format */

    for (idp = (struct __dirent32 *)ibuf, odp =
	 (struct dirent *)obuf, ibufend =
	 ibuf + (count - auio.uio_resid), obufend = obuf + count;
	 (caddr_t) idp < ibufend;
	 idp = (struct __dirent32 *)((caddr_t) idp + idp->__d_reclen), odp =
	 (struct dirent *)((caddr_t) odp + odp->d_reclen)) {
	odp->d_ino = idp->__d_ino;
	odp->d_namlen = idp->__d_namlen;
	(void)strcpy(odp->d_name, idp->__d_name);
	odp->d_reclen = reclen(odp);
	if ((caddr_t) odp + odp->d_reclen > obufend)
	    break;
	/* record offset *after* we're sure to use this entry */
	memcpy((char *)&tmp_offset, (char *)&idp->__d_off, sizeof tmp_offset);
	offset = tmp_offset;
    }

    outcount = (caddr_t) odp - obuf;
    AFS_UIOMOVE(obuf, outcount, UIO_READ, uiop, u.u_error);
    if (u.u_error)
	goto out;
    uiop->uio_offset = offset;
  out:
    kmem_free(ibuf, count);
    kmem_free(obuf, count + sizeof(struct dirent));
    return u.u_error;
}


#define roundtolong(x)   (((x) + (sizeof(long) - 1)) & ~(sizeof(long) - 1))
#define reclen_dirent64(dp)      roundtolong(((dp)->__d_namlen + 1 + (2*sizeof(u_long)) +\
                                2 * sizeof(u_short)))

int
afs_readdir3(vp, uiop, cred)
     struct vnode *vp;
     struct uio *uiop;
     struct ucred *cred;
{
    struct uio auio;
    struct iovec aiov;
    caddr_t ibuf, obuf, ibufend, obufend;
    struct __dirent32 *idp;
    struct __dirent64 *odp;
    int count, outcount;
    dir_off_t offset;

    memset(&auio, 0, sizeof(auio));
    memset(&aiov, 0, sizeof(aiov));

    count = uiop->uio_resid;
    /* Allocate temporary space for format conversion */
    ibuf = kmem_alloc(2 * count);	/* overkill - fix later */
    obuf = kmem_alloc(count + sizeof(struct __dirent64));
    aiov.iov_base = ibuf;
    aiov.iov_len = count;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    offset = auio.uio_offset = uiop->uio_offset;
    auio.uio_seg = UIOSEG_KERNEL;
    auio.uio_resid = count;
    auio.uio_fpflags = 0;

    u.u_error = mp_afs_readdir2(vp, &auio, cred);
    if (u.u_error)
	goto out;

    /* Convert entries from __dirent32 to __dirent64 format */

    for (idp = (struct __dirent32 *)ibuf, odp =
	 (struct __dirent64 *)obuf, ibufend =
	 ibuf + (count - auio.uio_resid), obufend = obuf + count;
	 (caddr_t) idp < ibufend;
	 idp = (struct __dirent32 *)((caddr_t) idp + idp->__d_reclen), odp =
	 (struct __dirent64 *)((caddr_t) odp + odp->__d_reclen)) {
	memcpy((char *)&odp->__d_off, (char *)&idp->__d_off,
	       sizeof odp->__d_off);
	odp->__d_ino = idp->__d_ino;
	odp->__d_namlen = idp->__d_namlen;
	(void)strcpy(odp->__d_name, idp->__d_name);
	odp->__d_reclen = reclen_dirent64(odp);
	if ((caddr_t) odp + odp->__d_reclen > obufend)
	    break;
	/* record offset *after* we're sure to use this entry */
	offset = odp->__d_off;
    }

    outcount = (caddr_t) odp - obuf;
    AFS_UIOMOVE(obuf, outcount, UIO_READ, uiop, u.u_error);
    if (u.u_error)
	goto out;
    uiop->uio_offset = offset;
  out:
    kmem_free(ibuf, count);
    kmem_free(obuf, count + sizeof(struct __dirent64));
    return u.u_error;
}

#define AFS_SV_SEMA_HASH 1
#define AFS_SV_SEMA_HASH_DEBUG 0

#if AFS_SV_SEMA_HASH
/* This portion of the code was originally used to implement 
 * thread specific storage for the semaphore save area. However,
 * there were some spare fields in the proc structure, this is
 * now being used for the saving semapores.  Hence, this portion of
 * the code is no longer used.
 */

/* This portion of the code implements thread specific information.
 * The thread id is passed in as the key. The semaphore saved area 
 * is hashed on this key.
 */

/* why is this hash table required ?
 * The AFS code is written in such a way that a GLOCK() is done in 
 * one function and the GUNLOCK() is done in another function further
 * down the call chain. The GLOCK() call has to save the current
 * semaphore status before acquiring afs_global_sema. The GUNLOCK
 * has to release afs_global_sema and reacquire the sempahore status
 * that existed before the corresponding GLOCK. If GLOCK() and
 * GUNLOCK() were called in the same function, the GLOCK call could 
 * have stored the saved sempahore status in a local variable and the
 * corresponding GUNLOCK() call could have restored the original
 * status from this local variable. But this is not the case with 
 * AFS code. Hence, we have to implement a thread specific semaphore
 * save area. This is implemented as a hash table. The key is the 
 * thread id.
 */

/* In order for multithreaded processes to work, the sv_sema structures
 * must be saved on a per-thread basis, not a per-process basis.  There
 * is no per-thread storage available to hijack in the OS per-thread
 * data structures (e.g. struct user) so we revive this code.
 * I removed the upper limit on the memory consumption since we don't
 * know how many threads there will be.  Now the code first checks the
 * freeList.  If that fails it then tries garbage collecting.  If that
 * doesn't free up anything then it allocs what it needs.
 */

#define ELEMENT		sv_sema_t
#define KEY		tid_t
#define Hash(xx)	(  (xx) % sizeOfHashTable )
#define hashLockInit(xx) initsema(&xx,1, FILESYS_SEMA_PRI, FILESYS_SEMA_ORDER)
#define hashLock(xx)	MP_PSEMA(&xx)
#define hashUnlock(xx) 	MP_VSEMA(&xx)

typedef struct elem {
    struct elem *next;
    ELEMENT element;
    KEY key;
    int refCnt;
} Element;

typedef struct bucket {
    sema_t lock;
    Element *element;
} Bucket;

static int sizeOfHashTable;
static Bucket *hashTable;

static int currentSize = 0;
static Element *freeList;	/* free list */

#pragma align 64
static sema_t afsHashLock = { 0 };	/* global lock for hash table */

static void afsHashGarbageCollect();

/*
** The global lock protects the global data structures, 
** e.g. freeList and currentSize.
** The bucket lock protects the link list hanging off that bucket.
** The lock hierarchy : one can obtain the bucket lock while holding 
** the global lock, but not vice versa.
*/


void
afsHash(int nbuckets)
{				/* allocate the hash table */
    int i;

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHash: enter\n");
#endif

    sizeOfHashTable = nbuckets;
    currentSize = nbuckets * sizeof(Bucket);

    if (hashTable)
	osi_Panic("afs: SEMA Hashtable already created\n");

    hashTable = (Bucket *) AFS_KALLOC(sizeOfHashTable * sizeof(Bucket));
    if (!hashTable)
	osi_Panic("afs: cannot create SEMA Hashtable\n");

    /* initialize the hash table and associated locks */
    memset(hashTable, 0, sizeOfHashTable * sizeof(Bucket));
    for (i = 0; i < sizeOfHashTable; i++)
	hashLockInit(hashTable[i].lock);
    hashLockInit(afsHashLock);

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHash: exit\n");
#endif
}

ELEMENT *
afsHashInsertFind(KEY key)
{
    int index;
    Element *ptr;

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHashInsertFind: %d\n", key);
#endif
    if (!hashTable)
	osi_Panic("afs: afsHashInsertFind: no hashTable\n");

    index = Hash(key);		/* get bucket number */
    hashLock(hashTable[index].lock);	/* lock this bucket */
    ptr = hashTable[index].element;

    /* if it is already there */
    while (ptr) {
	if (ptr->key == key) {
	    ptr->refCnt++;	/* hold it */
	    hashUnlock(hashTable[index].lock);
#if AFS_SV_SEMA_HASH_DEBUG
	    printf("afsHashInsertFind: %d FOUND\n", key);
#endif
	    return &(ptr->element);
	} else {
	    ptr = ptr->next;
	}
    }

    hashUnlock(hashTable[index].lock);

    /*  if something exists in the freeList, take it from there */
    ptr = NULL;
    hashLock(afsHashLock);

    if (freeList) {
	ptr = freeList;		/* reuse entry */
	freeList = freeList->next;
    } else {
	afsHashGarbageCollect();	/* afsHashLock locked */
	if (freeList) {
	    ptr = freeList;	/* reuse entry */
	    freeList = freeList->next;
	} else {
	    ptr = (Element *) AFS_KALLOC(sizeof(Element));
	}
    }

    currentSize += sizeof(Element);	/* update memory used */
    hashUnlock(afsHashLock);

    if (!ptr)
	osi_Panic("afs: SEMA Hashtable cannot create new entry\n");
    /* create new entry */
    ptr->key = key;
    memset(&ptr->element, 0, sizeof(ptr->element));
    ptr->refCnt = 1;		/* this guy */

    /* insert new entry in bucket */
    hashLock(hashTable[index].lock);	/* lock this bucket */
    ptr->next = hashTable[index].element;
    hashTable[index].element = ptr;
    hashUnlock(hashTable[index].lock);

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHashInsertFind: %d MADE\n", key);
#endif

    return &(ptr->element);
}

ELEMENT *
afsHashFind(KEY key)
{
    int index;
    Element *ptr;

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHashFind: %d\n", key);
#endif
    if (!hashTable)
	osi_Panic("afs: afsHashFind: no hashTable\n");

    index = Hash(key);		/* get bucket number */
    hashLock(hashTable[index].lock);	/* lock this bucket */
    ptr = hashTable[index].element;

    /* it should be in the hash table */
    while (ptr) {
	if (ptr->key == key) {
	    if (ptr->refCnt <= 0)
		osi_Panic("afs: SEMA HashTable entry already released\n");
	    hashUnlock(hashTable[index].lock);
#if AFS_SV_SEMA_HASH_DEBUG
	    printf("afsHashFind: %d FOUND\n", key);
#endif
	    return &(ptr->element);
	} else {
	    ptr = ptr->next;
	}
    }

    hashUnlock(hashTable[index].lock);
    /* it better be in the hash table */
    osi_Panic("afs: SEMA HashTable wants non-existent entry \n");
    return 0;
}

void
afsHashRelease(KEY key)
{
    int index;
    Element *ptr;

#if AFS_SV_SEMA_HASH_DEBUG
    printf("afsHashRelease: %d\n", key);
#endif
    if (!hashTable)
	osi_Panic("afs: afsHashRelease: no hashTable\n");

    index = Hash(key);		/* get bucket number */
    hashLock(hashTable[index].lock);	/* lock this bucket */
    ptr = hashTable[index].element;

    /* it should be in the hash table */
    while (ptr) {
	if (ptr->key == key) {
	    if (ptr->refCnt <= 0)
		osi_Panic("afs: SEMA HashTable entry already released\n");
	    ptr->refCnt--;	/* release this guy */
	    hashUnlock(hashTable[index].lock);
#if AFS_SV_SEMA_HASH_DEBUG
	    printf("afsHashRelease: %d FOUND\n", key);
#endif
	    return;
	} else {
	    ptr = ptr->next;
	}
    }

    hashUnlock(hashTable[index].lock);
    /* it better be in the hash table */
    osi_Panic("afs: SEMA HashTable deleting non-existent entry \n");
}

/* this should be called with afsHashLock WRITE locked */
static void
afsHashGarbageCollect()
{
    int index;
    Element *ptr;
    int foundFlag = 0;

    if (!hashTable)
	osi_Panic("afs: afsHashGarbageCollect: no hashTable\n");

    for (index = 0; index < sizeOfHashTable; index++) {
	hashLock(hashTable[index].lock);
	ptr = hashTable[index].element;	/* pick up bucket */

	while (ptr && !ptr->refCnt) {
	    /* insert this element into free list */
	    Element *temp;
	    temp = ptr->next;
	    ptr->next = freeList;
	    freeList = ptr;

	    foundFlag = 1;	/* found at least one */
	    currentSize -= sizeof(Element);
	    ptr = temp;
	}
	hashTable[index].element = ptr;

	/* scan thru the remaining list */
	if (ptr) {
	    while (ptr->next) {
		if (ptr->next->refCnt == 0) {
		    /* collect this element */
		    Element *temp;
		    temp = ptr->next;
		    ptr->next = ptr->next->next;
		    temp->next = freeList;
		    freeList = temp;
		    foundFlag = 1;
		    currentSize -= sizeof(Element);
		} else {
		    ptr = ptr->next;
		}
	    }
	}
	hashUnlock(hashTable[index].lock);
    }
#if 0
    if (!foundFlag)
	osi_Panic("afs: SEMA HashTable full\n");
#endif
}

#endif /* AFS_SV_SEMA_HASH */


afs_hp_strategy(bp)
     struct buf *bp;
{
    afs_int32 code;
    struct uio tuio;
    struct iovec tiovec[1];
    extern caddr_t hdl_kmap_bp();
    struct kthread *t = u.u_kthreadp;

    memset(&tuio, 0, sizeof(tuio));
    memset(&tiovec, 0, sizeof(tiovec));

    AFS_STATCNT(afs_hp_strategy);
    /*
     * hdl_kmap_bp() saves "b_bcount" and restores it in hdl_remap_bp() after
     * the I/O.  We must save and restore the count because pageiodone()
     * uses b_bcount to determine how many pages to unlock.
     *
     * Remap the entire range.
     */
    hdl_kmap_bp(bp);

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_HPSTRAT, ICL_TYPE_POINTER, bp->b_vp,
	       ICL_TYPE_LONG, (int)bp->b_blkno * DEV_BSIZE, ICL_TYPE_LONG,
	       bp->b_bcount, ICL_TYPE_LONG, 0);

    /* Set up the uio structure */
    tuio.afsio_iov = tiovec;
    tuio.afsio_iovcnt = 1;
    tuio.afsio_offset = DEV_BSIZE * bp->b_blkno;
    tuio.afsio_seg = AFS_UIOSYS;
    tuio.afsio_resid = bp->b_bcount;
    tuio.uio_fpflags = 0;
    tiovec[0].iov_base = bp->b_un.b_addr;
    tiovec[0].iov_len = bp->b_bcount;

    /* Do the I/O */
    if ((bp->b_flags & B_READ) == B_READ) {
	/* read b_bcount bytes into kernel address b_un.b_addr
	 * starting at byte DEV_BSIZE * b_blkno. Bzero anything
	 * we can't read, and finally call iodone(bp).  File is
	 * in bp->b_vp. Credentials are from u area??
	 */
	code = afs_rdwr(VTOAFS(bp->b_vp), &tuio, UIO_READ, 0, kt_cred(t));
	if (code == 0)
	    if (tuio.afsio_resid > 0) {
		privlbzero(bvtospace(bp, bp->b_un.b_addr),
			   bp->b_un.b_addr + bp->b_bcount - tuio.afsio_resid,
			   (size_t) tuio.afsio_resid);

	    }
    } else
	code = afs_rdwr(VTOAFS(bp->b_vp), &tuio, UIO_WRITE, 0, kt_cred(t));

    /* Remap back to the user's space */
    hdl_remap_bp(bp);

    AFS_GUNLOCK();

    iodone(bp);
    return code;
}

afs_pathconf(vp, name, resultp, cred)
     struct vnode *vp;
     int name;
     int *resultp;
     struct ucred *cred;	/* unused */
{
    switch (name) {
    case _PC_LINK_MAX:		/* Maximum number of links to a file */
	*resultp = 255;		/* an unsigned short on the fileserver */
	break;			/* a unsigned char in the client.... */

    case _PC_NAME_MAX:		/* Max length of file name */
	*resultp = 255;
	break;

    case _PC_PATH_MAX:		/* Maximum length of Path Name */
	*resultp = 1024;
	break;

    case _PC_PIPE_BUF:		/* Max atomic write to pipe.  See fifo_vnops */
    case _PC_CHOWN_RESTRICTED:	/* Anybody can chown? */
    case _PC_NO_TRUNC:		/* No file name truncation on overflow? */
	u.u_error = EOPNOTSUPP;
	return (EOPNOTSUPP);
	break;

    case _PC_MAX_CANON:	/* TTY buffer size for canonical input */
	/* need more work here for pty, ite buffer size, if differ */
	if (vp->v_type != VCHR) {
	    u.u_error = EINVAL;
	    return (EINVAL);
	}
	*resultp = CANBSIZ;	/*for tty */
	break;

    case _PC_MAX_INPUT:
	/* need more work here for pty, ite buffer size, if differ */
	if (vp->v_type != VCHR) {	/* TTY buffer size */
	    u.u_error = EINVAL;
	    return (EINVAL);
	}
	*resultp = TTYHOG;	/*for tty */
	break;

    case _PC_VDISABLE:
	/* Terminal special characters can be disabled? */
	if (vp->v_type != VCHR) {
	    u.u_error = EINVAL;
	    return (EINVAL);
	}
	*resultp = 1;
	break;

    case _PC_SYNC_IO:
	if ((vp->v_type != VREG) && (vp->v_type != VBLK)) {
	    *resultp = -1;
	    return EINVAL;
	}
	*resultp = 1;		/* Synchronized IO supported for this file */
	break;

    case _PC_FILESIZEBITS:
	if (vp->v_type != VDIR)
	    return (EINVAL);
	*resultp = MAX_SMALL_FILE_BITS;
	break;

    default:
	return (EINVAL);
    }

    return (0);
}
