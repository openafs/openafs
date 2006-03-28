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
    ("$Header: /cvs/openafs/src/afs/AIX/osi_vnodeops.c,v 1.15.2.3 2006/01/26 15:45:51 shadow Exp $");

#include "h/systm.h"
#include "h/types.h"
#include "h/errno.h"
#include "h/stat.h"
#include "h/user.h"
#include "h/uio.h"
#include "h/vattr.h"
#include "h/file.h"
#include "h/vfs.h"
#include "h/chownx.h"
#include "h/systm.h"
#include "h/access.h"
#ifdef AFS_AIX51_ENV
#include "h/acl.h"
#endif
#include "rpc/types.h"
#include "osi_vfs.h"
#include "netinet/in.h"
#include "h/mbuf.h"
#include "h/vmuser.h"
#include "h/shm.h"
#include "rpc/types.h"
#include "rpc/xdr.h"

#include "afs/stds.h"
#include "afs/afs_osi.h"
#define RFTP_INTERNALS 1
#include "afs/volerrors.h"
#include "afsint.h"
#include "vldbint.h"
#include "afs/lock.h"
#include "afs/exporter.h"
#include "afs/afs.h"
#include "afs/afs_chunkops.h"
#include "afs/afs_stats.h"
#include "afs/nfsclient.h"
#include "afs/icl.h"
#include "afs/prs_fs.h"
#include "h/flock.h"
#include "afsincludes.h"


int
afs_gn_link(struct vnode *vp, 
	    struct vnode *dp, 
	    char *name, 
	    struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_link);
    error = afs_link(vp, dp, name, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GNLINK, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_mkdir(struct vnode *dp, 
	     char *name, 
	     int32long64_t Mode, 
	     struct ucred *cred)
{
    struct vattr va;
    struct vnode *vp;
    int error;
    int mode = Mode;

    AFS_STATCNT(afs_gn_mkdir);
    VATTR_NULL(&va);
    va.va_type = VDIR;
    va.va_mode = (mode & 07777) & ~get_umask();
    error = afs_mkdir(dp, name, &va, &vp, cred);
    if (!error) {
	AFS_RELE(vp);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GMKDIR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG,
	       error);
    return (error);
}


int
afs_gn_mknod(struct vnode *dp, 
	     char *name, 
	     int32long64_t Mode, 
	     dev_t dev, 
	     struct ucred *cred)
{
    struct vattr va;
    struct vnode *vp;
    int error;
    int mode = Mode;

    AFS_STATCNT(afs_gn_mknod);
    VATTR_NULL(&va);
    va.va_type = IFTOVT(mode);
    va.va_mode = (mode & 07777) & ~get_umask();

/**** I'm not sure if suser() should stay here since it makes no sense in AFS; however the documentation says that one "should be super-user unless making a FIFO file. Others systems such as SUN do this checking in the early stages of mknod (before the abstraction), so it's equivalently the same! *****/
    if (va.va_type != VFIFO && !suser((char *)&error))
	return (EPERM);
    switch (va.va_type) {
    case VDIR:
	error = afs_mkdir(dp, name, &va, &vp, cred);
	break;
    case VNON:
	error = EINVAL;
	break;
    case VBAD:
    case VCHR:
    case VBLK:
	va.va_rdev = dev;
    default:
	error = afs_create(VTOAFS(dp), name, &va, NONEXCL, mode, (struct vcache **)&vp, cred);
    }
    if (!error) {
	AFS_RELE(vp);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GMKNOD, ICL_TYPE_POINTER, (afs_int32) vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG,
	       error);
    return (error);
}


int
afs_gn_remove(struct vnode *vp,		/* Ignored in AFS */
              struct vnode * dp, 
	      char *name, 
	      struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_remove);
    error = afs_remove(dp, name, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GREMOVE, ICL_TYPE_POINTER, dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_rename(struct vnode *vp, 		/* Ignored in AFS */
	      struct vnode *dp, 
	      char *name, 
	      struct vnode *tp, 		/* Ignored in AFS */
	      struct vnode *tdp, 
	      char *tname, 
	      struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_rename);
    error = afs_rename(dp, name, tdp, tname, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GRENAME, ICL_TYPE_POINTER, dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_STRING, tname, ICL_TYPE_LONG,
	       error);
    return (error);
}


int
afs_gn_rmdir(struct vnode *vp, 		/* Ignored in AFS */
	     struct vnode *dp, 
	     char *name, 
	     struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_rmdir);
    error = afs_rmdir(dp, name, cred);
    if (error) {
	if (error == 66 /* 4.3's ENOTEMPTY */ )
	    error = EEXIST;	/* AIX returns EEXIST where 4.3 used ENOTEMPTY */
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_GRMDIR, ICL_TYPE_POINTER, dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_lookup(struct vnode *dp, 
	      struct vnode **vpp, 
	      char *name, 
	      int32long64_t Flags, 	/* includes FOLLOW... */
	      struct vattr *vattrp, 
	      struct ucred *cred)
{
    int error;
    int flags = Flags;

    AFS_STATCNT(afs_gn_lookup);
    error = afs_lookup(dp, name, vpp, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GLOOKUP, ICL_TYPE_POINTER, dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    if (vattrp != NULL && error == 0)
	afs_gn_getattr(*vpp, vattrp, cred);
    return (error);
}


int
afs_gn_fid(struct vnode *vp, 
	struct fid *fidp, 
	struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_fid);
    error = afs_fid(vp, fidp);
    afs_Trace3(afs_iclSetp, CM_TRACE_GFID, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, (afs_int32) fidp, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_open(struct vnode *vp, 
	    int32long64_t Flags, 
	    ext_t ext, 
	    struct ucred **vinfop, 
	    struct ucred *cred)
{
    int error;
    struct vattr va;
    struct vcache *tvp = VTOAFS(vp);
    afs_int32 modes;
    int flags = Flags;

    AFS_STATCNT(afs_gn_open);
    modes = 0;
    if ((flags & FREAD))
	modes |= R_ACC;
    if ((flags & FEXEC))
	modes |= X_ACC;
    if ((flags & FWRITE) || (flags & FTRUNC))
	modes |= W_ACC;

    while ((flags & FNSHARE) && tvp->opens) {
	if (!(flags & FDELAY)) {
	    error = ETXTBSY;
	    goto abort;
	}
	afs_osi_Sleep(&tvp->opens);
    }

    error = afs_access(VTOAFS(vp), modes, cred);
    if (error) {
	goto abort;
    }

    error = afs_open((struct vcache **) &vp, flags, cred);
    if (!error) {
	if (flags & FTRUNC) {
	    VATTR_NULL(&va);
	    va.va_size = 0;
	    error = afs_setattr(VTOAFS(vp), &va, cred);
	}

	if (flags & FNSHARE)
	    tvp->states |= CNSHARE;

	if (!error) {
	    *vinfop = cred;	/* fp->f_vinfo is like fp->f_cred in suns */
	} else {
	    /* an error occurred; we've told CM that the file
	     * is open, so close it now so that open and
	     * writer counts are correct.  Ignore error code,
	     * as it is likely to fail (the setattr just did).
	     */
	    afs_close(vp, flags, cred);
	}
    }

  abort:
    afs_Trace3(afs_iclSetp, CM_TRACE_GOPEN, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_create(struct vnode *dp, 
	      struct vnode **vpp, 
	      int32long64_t Flags, 
	      char *name, 
	      int32long64_t Mode, 
	      struct ucred **vinfop, /* return ptr for fp->f_vinfo, used as fp->f_cred */
	      struct ucred *cred)

{
    struct vattr va;
    enum vcexcl exclusive;
    int error, modes = 0;
    int flags = Flags;
    int mode = Mode;

    AFS_STATCNT(afs_gn_create);
    if ((flags & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT))
	exclusive = EXCL;
    else
	exclusive = NONEXCL;
    VATTR_NULL(&va);
    va.va_type = VREG;
    va.va_mode = (mode & 07777) & ~get_umask();
    if ((flags & FREAD))
	modes |= R_ACC;
    if ((flags & FEXEC))
	modes |= X_ACC;
    if ((flags & FWRITE) || (flags & FTRUNC))
	modes |= W_ACC;
    error = afs_create(VTOAFS(dp), name, &va, exclusive, modes, (struct vcache **)vpp, cred);
    if (error) {
	return error;
    }
    /* 'cr_luid' is a flag (when it comes thru the NFS server it's set to
     * RMTUSER_REQ) that determines if we should call afs_open(). We shouldn't
     * call it when this NFS traffic since the close will never happen thus
     * we'd never flush the files out to the server! Gross but the simplest
     * solution we came out with */
    if (cred->cr_luid != RMTUSER_REQ) {
	while ((flags & FNSHARE) && VTOAFS(*vpp)->opens) {
	    if (!(flags & FDELAY))
		return ETXTBSY;
	    afs_osi_Sleep(&VTOAFS(*vpp)->opens);
	}
	/* Since in the standard copen() for bsd vnode kernels they do an
	 * vop_open after the vop_create, we must do the open here since there
	 * are stuff in afs_open that we need. For example advance the
	 * execsOrWriters flag (else we'll be treated as the sun's "core"
	 * case). */
	*vinfop = cred;		/* save user creds in fp->f_vinfo */
	error = afs_open((struct vcache **)vpp, flags, cred);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GCREATE, ICL_TYPE_POINTER, dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG,
	       error);
    return error;
}


int
afs_gn_hold(struct vnode *vp)
{
    AFS_STATCNT(afs_gn_hold);
    ++(vp->v_count);
    return (0);
}

int vmPageHog = 0;

int
afs_gn_rele(struct vnode *vp)
{
    struct vcache *vcp = VTOAFS(vp);
    int error = 0;

    AFS_STATCNT(afs_gn_rele);
    if (vp->v_count == 0)
	osi_Panic("afs_rele: zero v_count");
    if (--(vp->v_count) == 0) {
	if (vcp->states & CPageHog) {
	    vmPageHog--;
	    vcp->states &= ~CPageHog;
	}
	error = afs_inactive(vp, 0);
    }
    return (error);
}


int
afs_gn_close(struct vnode *vp, 
	     int32long64_t Flags, 
	     caddr_t vinfo, 		/* Ignored in AFS */
	     struct ucred *cred)
{
    int error;
    struct vcache *tvp = VTOAFS(vp);
    int flags = Flags;

    AFS_STATCNT(afs_gn_close);

    if (flags & FNSHARE) {
	tvp->states &= ~CNSHARE;
	afs_osi_Wakeup(&tvp->opens);
    }

    error = afs_close(vp, flags, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GCLOSE, ICL_TYPE_POINTER, (afs_int32) vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_map(struct vnode *vp, 
	   caddr_t addr, 
	   uint32long64_t Len, 
	   uint32long64_t Off, 
	   uint32long64_t Flag, 
	   struct ucred *cred)
{
    struct vcache *vcp = VTOAFS(vp);
    struct vrequest treq;
    afs_int32 error;
    afs_int32 len = Len;
    afs_int32 off = Off;
    afs_int32 flag = Flag;

    AFS_STATCNT(afs_gn_map);
#ifdef	notdef
    if (error = afs_InitReq(&treq, cred))
	return error;
    error = afs_VerifyVCache(vcp, &treq);
    if (error)
	return afs_CheckCode(error, &treq, 49);
#endif
    osi_FlushPages(vcp, cred);	/* XXX ensure old pages are gone XXX */
    ObtainWriteLock(&vcp->lock, 401);
    vcp->states |= CMAPPED;	/* flag cleared at afs_inactive */
    /*
     * We map the segment into our address space using the handle returned by vm_create.
     */
    if (!vcp->segid) {
	afs_uint32 tlen = vcp->m.Length;
#ifdef AFS_64BIT_CLIENT
	if (vcp->m.Length > afs_vmMappingEnd)
	    tlen = afs_vmMappingEnd;
#endif
	/* Consider  V_INTRSEG too for interrupts */
	if (error =
	    vms_create(&vcp->segid, V_CLIENT, (dev_t) vcp->v.v_gnode, tlen, 0, 0)) {
	    ReleaseWriteLock(&vcp->lock);
	    return (EOPNOTSUPP);
	}
#ifdef AFS_64BIT_KERNEL
	vcp->vmh = vm_handle(vcp->segid, (int32long64_t) 0);
#else
	vcp->vmh = SRVAL(vcp->segid, 0, 0);
#endif
    }
    vcp->v.v_gnode->gn_seg = vcp->segid;	/* XXX Important XXX */
    if (flag & SHM_RDONLY) {
	vp->v_gnode->gn_mrdcnt++;
    } else {
	vp->v_gnode->gn_mwrcnt++;
    }
    /*
     * We keep the caller's credentials since an async daemon will handle the 
     * request at some point. We assume that the same credentials will be used.
     */
    if (!vcp->credp || (vcp->credp != cred)) {
	crhold(cred);
	if (vcp->credp) {
	    struct ucred *crp = vcp->credp;
	    vcp->credp = NULL;
	    crfree(crp);
	}
	vcp->credp = cred;
    }
    ReleaseWriteLock(&vcp->lock);
    VN_HOLD(vp);
    afs_Trace4(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, addr, ICL_TYPE_LONG, len, ICL_TYPE_LONG, off);
    return (0);
}


int
afs_gn_unmap(struct vnode *vp, 
	     int32long64_t flag, 
	     struct ucred *cred)
{
    struct vcache *vcp = VTOAFS(vp);
    AFS_STATCNT(afs_gn_unmap);
    ObtainWriteLock(&vcp->lock, 402);
    if (flag & SHM_RDONLY) {
	vp->v_gnode->gn_mrdcnt--;
	if (vp->v_gnode->gn_mrdcnt <= 0)
	    vp->v_gnode->gn_mrdcnt = 0;
    } else {
	vp->v_gnode->gn_mwrcnt--;
	if (vp->v_gnode->gn_mwrcnt <= 0)
	    vp->v_gnode->gn_mwrcnt = 0;
    }
    ReleaseWriteLock(&vcp->lock);

    AFS_RELE(vp);
    return 0;
}


int
afs_gn_access(struct vnode *vp, 
	      int32long64_t Mode, 
	      int32long64_t Who, 
	      struct ucred *cred)
{
    int error;
    struct vattr vattr;
    int mode = Mode;
    int who = Who;

    AFS_STATCNT(afs_gn_access);
    if (mode & ~0x7) {
	error = EINVAL;
	goto out;
    }

    error = afs_access(VTOAFS(vp), mode, cred);
    if (!error) {
	/* Additional testing */
	if (who == ACC_OTHERS || who == ACC_ANY) {
	    error = afs_getattr(VTOAFS(vp), &vattr, cred);
	    if (!error) {
		if (who == ACC_ANY) {
		    if (((vattr.va_mode >> 6) & mode) == mode) {
			error = 0;
			goto out;
		    }
		}
		if (((vattr.va_mode >> 3) & mode) == mode)
		    error = 0;
		else
		    error = EACCES;
	    }
	} else if (who == ACC_ALL) {
	    error = afs_getattr(VTOAFS(vp), &vattr, cred);
	    if (!error) {
		if ((!((vattr.va_mode >> 6) & mode))
		    || (!((vattr.va_mode >> 3) & mode))
		    || (!(vattr.va_mode & mode)))
		    error = EACCES;
		else
		    error = 0;
	    }
	}

    }
  out:
    afs_Trace3(afs_iclSetp, CM_TRACE_GACCESS, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, mode, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_getattr(struct vnode *vp, 
	       struct vattr *vattrp, 
	       struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_getattr);
    error = afs_getattr(VTOAFS(vp), vattrp, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GGETATTR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_setattr(struct vnode *vp, 
	       int32long64_t op, 
	       int32long64_t arg1, 
	       int32long64_t arg2, 
	       int32long64_t arg3, 
	       struct ucred *cred)
{
    struct vattr va;
    int error = 0;

    AFS_STATCNT(afs_gn_setattr);
    VATTR_NULL(&va);
    switch (op) {
	/* change mode */
    case V_MODE:
	va.va_mode = arg1;
	break;
    case V_OWN:
	if ((arg1 & T_OWNER_AS_IS) == 0)
	    va.va_uid = arg2;
	if ((arg1 & T_GROUP_AS_IS) == 0)
	    va.va_gid = arg3;
	break;
    case V_UTIME:
#ifdef	notdef
	error = afs_access(vp, VWRITE, cred);
	if (error)
	    goto out;
#endif
	if (arg1 & T_SETTIME) {
	    va.va_atime.tv_sec = time;
	    va.va_mtime.tv_sec = time;
	} else {
	    va.va_atime = *(struct timestruc_t *)arg2;
	    va.va_mtime = *(struct timestruc_t *)arg3;
	}
	break;
    default:
	error = EINVAL;
	goto out;
    }

    error = afs_setattr(VTOAFS(vp), &va, cred);
  out:
    afs_Trace2(afs_iclSetp, CM_TRACE_GSETATTR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, error);
    return (error);
}


char zero_buffer[PAGESIZE];
int
afs_gn_fclear(struct vnode *vp, 
	      int32long64_t flags, 
	      offset_t offset, 
	      offset_t length, 
	      caddr_t vinfo, 
	      struct ucred *cred)
{
    int i, len, error = 0;
    struct iovec iov;
    struct uio uio;
    static int fclear_init = 0;
    struct vcache *avc = VTOAFS(vp);

    AFS_STATCNT(afs_gn_fclear);
    if (!fclear_init) {
	memset(zero_buffer, 0, PAGESIZE);
	fclear_init = 1;
    }
    /*
     * Don't clear past ulimit
     */
    if (offset + length > get_ulimit())
	return (EFBIG);

    /* Flush all pages first */
    if (avc->segid) {
	AFS_GUNLOCK();
	vm_flushp(avc->segid, 0, MAXFSIZE / PAGESIZE - 1);
	vms_iowait(avc->segid);
	AFS_GLOCK();
    }
    uio.afsio_offset = offset;
    for (i = offset; i < offset + length; i = uio.afsio_offset) {
	len = offset + length - i;
	iov.iov_len = (len > PAGESIZE) ? PAGESIZE : len;
	iov.iov_base = zero_buffer;
	uio.afsio_iov = &iov;
	uio.afsio_iovcnt = 1;
	uio.afsio_seg = AFS_UIOSYS;
	uio.afsio_resid = iov.iov_len;
	if (error = afs_rdwr(VTOAFS(vp), &uio, UIO_WRITE, 0, cred))
	    break;
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GFCLEAR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, offset, ICL_TYPE_LONG, length, ICL_TYPE_LONG,
	       error);
    return (error);
}


int
afs_gn_fsync(struct vnode *vp, 
	     int32long64_t flags, 	/* Not used by AFS */
	     int32long64_t vinfo, 	/* Not used by AFS */
	     struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_fsync);
    error = afs_fsync(vp, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GFSYNC, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_ftrunc(struct vnode *vp, 
	      int32long64_t flags, 
	      offset_t length, 
	      caddr_t vinfo, 
	      struct ucred *cred)
{
    struct vattr va;
    int error;

    AFS_STATCNT(afs_gn_ftrunc);
    VATTR_NULL(&va);
    va.va_size = length;
    error = afs_setattr(VTOAFS(vp), &va, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GFTRUNC, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(length), ICL_TYPE_LONG, error);
    return (error);
}

/* Min size of a file which is dumping core before we declare it a page hog. */
#define MIN_PAGE_HOG_SIZE 8388608

int
afs_gn_rdwr(struct vnode *vp, 
	    enum uio_rw op, 
	    int32long64_t Flags, 
	    struct uio *ubuf, 
	    ext_t ext, 			/* Ignored in AFS */
	    caddr_t vinfo, 		/* Ignored in AFS */
	    struct vattr *vattrp, 
	    struct ucred *cred)
{
    struct vcache *vcp = VTOAFS(vp);
    struct vrequest treq;
    int error = 0;
    int free_cred = 0;
    int flags = Flags;

    AFS_STATCNT(afs_gn_rdwr);

    if (vcp->vc_error) {
	if (op == UIO_WRITE) {
	    afs_Trace2(afs_iclSetp, CM_TRACE_GRDWR1, ICL_TYPE_POINTER, vp,
		       ICL_TYPE_LONG, vcp->vc_error);
	    return vcp->vc_error;
	} else
	    return EIO;
    }

    ObtainSharedLock(&vcp->lock, 507);
    /*
     * We keep the caller's credentials since an async daemon will handle the 
     * request at some point. We assume that the same credentials will be used.
     * If this is being called from an NFS server thread, then dupe the
     * cred and only use that copy in calls and for the stach. 
     */
    if (!vcp->credp || (vcp->credp != cred)) {
#ifdef AFS_AIX_IAUTH_ENV
	if (AFS_NFSXLATORREQ(cred)) {
	    /* Must be able to use cred later, so dupe it so that nfs server
	     * doesn't overwrite it's contents.
	     */
	    cred = crdup(cred);
	    free_cred = 1;
	}
#endif
	crhold(cred);		/* Bump refcount for reference in vcache */

	if (vcp->credp) {
	    struct ucred *crp;
	    UpgradeSToWLock(&vcp->lock, 508);
	    crp = vcp->credp;
	    vcp->credp = NULL;
	    ConvertWToSLock(&vcp->lock);
	    crfree(crp);
	}
	vcp->credp = cred;
    }
    ReleaseSharedLock(&vcp->lock);

    /*
     * XXX Is the following really required?? XXX
     */
    if (error = afs_InitReq(&treq, cred))
	return error;
    if (error = afs_VerifyVCache(vcp, &treq))
	return afs_CheckCode(error, &treq, 50);
    osi_FlushPages(vcp, cred);	/* Flush old pages */

    if (AFS_NFSXLATORREQ(cred)) {
	if (flags & FSYNC)
	    flags &= ~FSYNC;
	if (op == UIO_READ) {
	    if (!afs_AccessOK
		(vcp, PRSFS_READ, &treq,
		 CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
		if (free_cred)
		    crfree(cred);
		return EACCES;
	    }
	}
    }

    /*
     * We have to bump the open/exwriters field here courtesy of the nfs xlator
     * because there're no open/close nfs rpcs to call our afs_open/close.
     * We do a similar thing on the afs_read/write interface.
     */
    if (op == UIO_WRITE) {
#ifdef AFS_64BIT_CLIENT
	if (ubuf->afsio_offset < afs_vmMappingEnd) {
#endif /* AFS_64BIT_CLIENT */
	    ObtainWriteLock(&vcp->lock, 240);
	    vcp->states |= CDirty;	/* Set the dirty bit */
	    afs_FakeOpen(vcp);
	    ReleaseWriteLock(&vcp->lock);
#ifdef AFS_64BIT_CLIENT
	}
#endif /* AFS_64BIT_CLIENT */
    }

    error = afs_vm_rdwr(vp, ubuf, op, flags, cred);

    if (op == UIO_WRITE) {
#ifdef AFS_64BIT_CLIENT
	if (ubuf->afsio_offset < afs_vmMappingEnd) {
#endif /* AFS_64BIT_CLIENT */
	    ObtainWriteLock(&vcp->lock, 241);
	    afs_FakeClose(vcp, cred);	/* XXXX For nfs trans and cores XXXX */
	    ReleaseWriteLock(&vcp->lock);
#ifdef AFS_64BIT_CLIENT
	}
#endif /* AFS_64BIT_CLIENT */
    }
    if (vattrp != NULL && error == 0)
	afs_gn_getattr(vp, vattrp, cred);

    afs_Trace4(afs_iclSetp, CM_TRACE_GRDWR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, op, ICL_TYPE_LONG, error);

    if (free_cred)
	crfree(cred);
    return (error);
}

#define AFS_MAX_VM_CHUNKS 10
static int
afs_vm_rdwr(struct vnode *vp, 
	    struct uio *uiop, 
	    enum uio_rw rw, 
	    int ioflag, 
	    struct ucred *credp)
{
    afs_int32 code = 0;
    int i;
    afs_int32 blockSize;
    afs_size_t fileSize, xfrOffset, offset, old_offset, xfrSize;
    vmsize_t txfrSize;
#ifdef AFS_64BIT_CLIENT
    afs_size_t finalOffset;
    off_t toffset;
    int mixed = 0;
    afs_size_t add2resid = 0;
#endif /* AFS_64BIT_CLIENT */
    struct vcache *vcp = VTOAFS(vp);
    struct dcache *tdc;
    afs_size_t start_offset;
    afs_int32 save_resid = uiop->afsio_resid;
    int first_page, last_page, pages;
    int count, len;
    int counter = 0;
    struct vrequest treq;

    if (code = afs_InitReq(&treq, credp))
	return code;

    /* special case easy transfer; apparently a lot are done */
    if ((xfrSize = uiop->afsio_resid) == 0)
	return 0;

    ObtainReadLock(&vcp->lock);
    fileSize = vcp->m.Length;
    if (rw == UIO_WRITE && (ioflag & IO_APPEND)) {	/* handle IO_APPEND mode */
	uiop->afsio_offset = fileSize;
    }
    /* compute xfrOffset now, and do some checks */
    xfrOffset = uiop->afsio_offset;
    if (xfrOffset < 0 || xfrOffset + xfrSize < 0) {
	code = EINVAL;
	ReleaseReadLock(&vcp->lock);
	goto fail;
    }
#ifndef AFS_64BIT_CLIENT
    /* check for "file too big" error, which should really be done above us */
    if (rw == UIO_WRITE && xfrSize + fileSize > get_ulimit()) {
	code = EFBIG;
	ReleaseReadLock(&vcp->lock);
	goto fail;
    }
#endif /* AFS_64BIT_CLIENT */

#ifdef AFS_64BIT_CLIENT
    if (xfrOffset + xfrSize > afs_vmMappingEnd) {
        if (rw == UIO_READ) {
            /* don't read past EOF */
            if (xfrSize+xfrOffset > fileSize) {
		add2resid = xfrSize + xfrOffset - fileSize;
		xfrSize = fileSize - xfrOffset;
		if (xfrSize <= 0) {
		    ReleaseReadLock(&vcp->lock);
		    goto fail;
		}
                txfrSize = xfrSize;
                afsio_trim(uiop, txfrSize);
            }
        }
	if (xfrOffset < afs_vmMappingEnd) {
	    /* special case of a buffer crossing the VM mapping line */
	    struct uio tuio;
	    struct iovec tvec[16];	/* Should have access to #define */
	    afs_int32 tsize;

	    mixed = 1;
	    finalOffset = xfrOffset + xfrSize;
	    tsize = (afs_size_t) (xfrOffset + xfrSize - afs_vmMappingEnd);
	    txfrSize = xfrSize;
	    afsio_copy(uiop, &tuio, tvec);
	    afsio_skip(&tuio, txfrSize - tsize);
	    afsio_trim(&tuio, tsize);
	    tuio.afsio_offset = afs_vmMappingEnd;
	    ReleaseReadLock(&vcp->lock);
	    ObtainWriteLock(&vcp->lock, 243);
	    afs_FakeClose(vcp, credp);	/* XXXX For nfs trans and cores XXXX */
	    ReleaseWriteLock(&vcp->lock);
	    code = afs_direct_rdwr(vp, &tuio, rw, ioflag, credp);
	    ObtainWriteLock(&vcp->lock, 244);
	    afs_FakeOpen(vcp);	/* XXXX For nfs trans and cores XXXX */
	    ReleaseWriteLock(&vcp->lock);
	    if (code)
		goto fail;
	    ObtainReadLock(&vcp->lock);
	    xfrSize = afs_vmMappingEnd - xfrOffset;
	    txfrSize = xfrSize;
	    afsio_trim(uiop, txfrSize);
	} else {
	    ReleaseReadLock(&vcp->lock);
	    code = afs_direct_rdwr(vp, uiop, rw, ioflag, credp);
	    uiop->uio_resid += add2resid;
	    return code;
	}
    }
#endif /* AFS_64BIT_CLIENT */

    if (!vcp->segid) {
	afs_uint32 tlen = vcp->m.Length;
#ifdef AFS_64BIT_CLIENT
	if (vcp->m.Length > afs_vmMappingEnd)
	    tlen = afs_vmMappingEnd;
#endif
	/* Consider  V_INTRSEG too for interrupts */
	if (code =
	    vms_create(&vcp->segid, V_CLIENT, (dev_t) vcp->v.v_gnode, tlen, 0, 0)) {
	    ReleaseReadLock(&vcp->lock);
	    goto fail;
	}
#ifdef AFS_64BIT_KERNEL
	vcp->vmh = vm_handle(vcp->segid, (int32long64_t) 0);
#else
	vcp->vmh = SRVAL(vcp->segid, 0, 0);
#endif
    }
    vcp->v.v_gnode->gn_seg = vcp->segid;
    if (rw == UIO_READ) {
	ReleaseReadLock(&vcp->lock);
	/* don't read past EOF */
	if (xfrSize + xfrOffset > fileSize)
	    xfrSize = fileSize - xfrOffset;
	if (xfrSize <= 0)
	    goto fail;
#ifdef AFS_64BIT_CLIENT
	toffset = xfrOffset;
	uiop->afsio_offset = xfrOffset;
	afs_Trace3(afs_iclSetp, CM_TRACE_VMWRITE, ICL_TYPE_POINTER, vcp,
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(xfrOffset),
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(xfrSize));
	AFS_GUNLOCK();
	txfrSize = xfrSize;
	code = vm_move(vcp->segid, toffset, txfrSize, rw, uiop);
#else /* AFS_64BIT_CLIENT */
	AFS_GUNLOCK();
	code = vm_move(vcp->segid, xfrOffset, xfrSize, rw, uiop);
#endif /* AFS_64BIT_CLIENT */
	AFS_GLOCK();
	/*
	 * If at a chunk boundary and staying within chunk,
	 * start prefetch of next chunk.
	 */
	if (counter == 0 || AFS_CHUNKOFFSET(xfrOffset) == 0
	    && xfrSize <= AFS_CHUNKSIZE(xfrOffset)) {
	    ObtainWriteLock(&vcp->lock, 407);
	    tdc = afs_FindDCache(vcp, xfrOffset);
	    if (tdc) {
		if (!(tdc->mflags & DFNextStarted))
		    afs_PrefetchChunk(vcp, tdc, credp, &treq);
		afs_PutDCache(tdc);
	    }
	    ReleaseWriteLock(&vcp->lock);
	}
#ifdef AFS_64BIT_CLIENT
	if (mixed) {
	    uiop->afsio_offset = finalOffset;
	}
	uiop->uio_resid += add2resid;
#endif /* AFS_64BIT_CLIENT */
	return code;
    }

    /* UIO_WRITE */
    start_offset = uiop->afsio_offset;
    afs_Trace3(afs_iclSetp, CM_TRACE_VMWRITE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(start_offset),
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(xfrSize));
    ReleaseReadLock(&vcp->lock);
    ObtainWriteLock(&vcp->lock, 400);
    vcp->m.Date = osi_Time();	/* Set file date (for ranlib) */
    /* extend file */
    /* un-protect last page. */
    last_page = vcp->m.Length / PAGESIZE;
#ifdef AFS_64BIT_CLIENT
    if (vcp->m.Length > afs_vmMappingEnd)
	last_page = afs_vmMappingEnd / PAGESIZE;
#endif
    vm_protectp(vcp->segid, last_page, 1, FILEKEY);
    if (xfrSize + xfrOffset > fileSize) {
	vcp->m.Length = xfrSize + xfrOffset;
    }
    if ((!(vcp->states & CPageHog)) && (xfrSize >= MIN_PAGE_HOG_SIZE)) {
	vmPageHog++;
	vcp->states |= CPageHog;
    }
    ReleaseWriteLock(&vcp->lock);

    /* If the write will fit into a single chunk we'll write all of it
     * at once. Otherwise, we'll write one chunk at a time, flushing
     * some of it to disk.
     */
    count = 0;

    /* Only create a page to avoid excess VM access if we're writing a
     * small file which is either new or completely overwrites the 
     * existing file.
     */
    if ((xfrOffset == 0) && (xfrSize < PAGESIZE) && (xfrSize >= fileSize)
	&& (vcp->v.v_gnode->gn_mwrcnt == 0)
	&& (vcp->v.v_gnode->gn_mrdcnt == 0)) {
	(void)vm_makep(vcp->segid, 0);
    }

    while (xfrSize > 0) {
	offset = AFS_CHUNKBASE(xfrOffset);
	len = xfrSize;

	if (AFS_CHUNKSIZE(xfrOffset) <= len)
	    len =
		(afs_size_t) AFS_CHUNKSIZE(xfrOffset) - (xfrOffset - offset);

	if (len == xfrSize) {
	    /* All data goes to this one chunk. */
	    AFS_GUNLOCK();
	    old_offset = uiop->afsio_offset;
#ifdef AFS_64BIT_CLIENT
	    uiop->afsio_offset = xfrOffset;
	    toffset = xfrOffset;
	    txfrSize = xfrSize;
	    code = vm_move(vcp->segid, toffset, txfrSize, rw, uiop);
#else /* AFS_64BIT_CLIENT */
	    code = vm_move(vcp->segid, xfrOffset, xfrSize, rw, uiop);
#endif /* AFS_64BIT_CLIENT */
	    AFS_GLOCK();
	    if (code) {
		goto fail;
	    }
	    xfrOffset += len;
	    xfrSize = 0;
	} else {
	    /* Write just one chunk's worth of data. */
	    struct uio tuio;
	    struct iovec tvec[16];	/* Should have access to #define */

	    /* Purge dirty chunks of file if there are too many dirty chunks.
	     * Inside the write loop, we only do this at a chunk boundary.
	     * Clean up partial chunk if necessary at end of loop.
	     */
	    if (counter > 0 && code == 0 && xfrOffset == offset) {
		ObtainWriteLock(&vcp->lock, 403);
		if (xfrOffset > vcp->m.Length)
		    vcp->m.Length = xfrOffset;
		code = afs_DoPartialWrite(vcp, &treq);
		vcp->states |= CDirty;
		ReleaseWriteLock(&vcp->lock);
		if (code) {
		    goto fail;
		}
	    }
	    counter++;

	    afsio_copy(uiop, &tuio, tvec);
	    afsio_trim(&tuio, len);
	    tuio.afsio_offset = xfrOffset;

	    AFS_GUNLOCK();
	    old_offset = uiop->afsio_offset;
#ifdef AFS_64BIT_CLIENT
	    toffset = xfrOffset;
	    code = vm_move(vcp->segid, toffset, len, rw, &tuio);
#else /* AFS_64BIT_CLIENT */
	    code = vm_move(vcp->segid, xfrOffset, len, rw, &tuio);
#endif /* AFS_64BIT_CLIENT */
	    AFS_GLOCK();
	    len -= tuio.afsio_resid;
	    if (code || (len <= 0)) {
		code = code ? code : EINVAL;
		goto fail;
	    }
	    afsio_skip(uiop, len);
	    xfrSize -= len;
	    xfrOffset += len;
	}

	first_page = (afs_size_t) old_offset >> PGSHIFT;
	pages =
	    1 + (((afs_size_t) old_offset + (len - 1)) >> PGSHIFT) -
	    first_page;
	afs_Trace3(afs_iclSetp, CM_TRACE_VMWRITE2, ICL_TYPE_POINTER, vcp,
		   ICL_TYPE_INT32, first_page, ICL_TYPE_INT32, pages);
	AFS_GUNLOCK();
	code = vm_writep(vcp->segid, first_page, pages);
	if (code) {
	    AFS_GLOCK();
	    goto fail;
	}
	if (++count > AFS_MAX_VM_CHUNKS) {
	    count = 0;
	    code = vms_iowait(vcp->segid);
	    if (code) {
		/* cache device failure? */
		AFS_GLOCK();
		goto fail;
	    }
	}
	AFS_GLOCK();

    }

    if (count) {
	AFS_GUNLOCK();
	code = vms_iowait(vcp->segid);
	AFS_GLOCK();
	if (code) {
	    /* cache device failure? */
	    goto fail;
	}
    }

    ObtainWriteLock(&vcp->lock, 242);
    if (code == 0 && (vcp->states & CDirty)) {
	code = afs_DoPartialWrite(vcp, &treq);
    }
    vm_protectp(vcp->segid, last_page, 1, RDONLY);
    ReleaseWriteLock(&vcp->lock);

    /* If requested, fsync the file after every write */
    if (ioflag & FSYNC)
	afs_fsync(vp, credp);

    ObtainReadLock(&vcp->lock);
    if (vcp->vc_error) {
	/* Pretend we didn't write anything. We need to get the error back to
	 * the user. If we don't it's possible for a quota error for this
	 * write to succeed and the file to be closed without the user ever
	 * having seen the store error. And AIX syscall clears the error if
	 * anything was written.
	 */
	code = vcp->vc_error;
	if (code == EDQUOT || code == ENOSPC)
	    uiop->afsio_resid = save_resid;
    }
#ifdef AFS_64BIT_CLIENT
    if (mixed) {
	uiop->afsio_offset = finalOffset;
    }
#endif /* AFS_64BIT_CLIENT */
    ReleaseReadLock(&vcp->lock);

  fail:
    afs_Trace2(afs_iclSetp, CM_TRACE_VMWRITE3, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_INT32, code);
    return code;
}


static int
afs_direct_rdwr(struct vnode *vp, 
	        struct uio *uiop, 
		enum uio_rw rw, 
		int ioflag, 
		struct ucred *credp)
{
    afs_int32 code = 0;
    afs_size_t fileSize, xfrOffset, offset, old_offset, xfrSize;
    struct vcache *vcp = VTOAFS(vp);
    afs_int32 save_resid = uiop->afsio_resid;
    struct vrequest treq;

    if (code = afs_InitReq(&treq, credp))
	return code;

    /* special case easy transfer; apparently a lot are done */
    if ((xfrSize = uiop->afsio_resid) == 0)
	return 0;

    ObtainReadLock(&vcp->lock);
    fileSize = vcp->m.Length;
    if (rw == UIO_WRITE && (ioflag & IO_APPEND)) {	/* handle IO_APPEND mode */
	uiop->afsio_offset = fileSize;
    }
    /* compute xfrOffset now, and do some checks */
    xfrOffset = uiop->afsio_offset;
    if (xfrOffset < 0 || xfrOffset + xfrSize < 0) {
	code = EINVAL;
	ReleaseReadLock(&vcp->lock);
	goto fail;
    }

    /* check for "file too big" error, which should really be done above us */
#ifdef notdef
    if (rw == UIO_WRITE && xfrSize + fileSize > get_ulimit()) {
	code = EFBIG;
	ReleaseReadLock(&vcp->lock);
	goto fail;
    }
#endif
    ReleaseReadLock(&vcp->lock);
    if (rw == UIO_WRITE) {
	ObtainWriteLock(&vcp->lock, 400);
	vcp->m.Date = osi_Time();	/* Set file date (for ranlib) */
	/* extend file */
	if (xfrSize + xfrOffset > fileSize)
	    vcp->m.Length = xfrSize + xfrOffset;
	ReleaseWriteLock(&vcp->lock);
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_DIRECTRDWR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(uiop->afsio_offset),
	       ICL_TYPE_LONG, uiop->afsio_resid);
    code = afs_rdwr(VTOAFS(vp), uiop, rw, ioflag, credp);
    if (code != 0) {
	uiop->afsio_resid = save_resid;
    } else {
	uiop->afsio_offset = xfrOffset + xfrSize;
	if (uiop->afsio_resid > 0) {
	    /* should zero here the remaining buffer */
	    uiop->afsio_resid = 0;
	}
	/* Purge dirty chunks of file if there are too many dirty chunks.
	 * Inside the write loop, we only do this at a chunk boundary.
	 * Clean up partial chunk if necessary at end of loop.
	 */
	if (AFS_CHUNKBASE(uiop->afsio_offset) != AFS_CHUNKBASE(xfrOffset)) {
	    ObtainWriteLock(&vcp->lock, 402);
	    code = afs_DoPartialWrite(vcp, &treq);
	    vcp->states |= CDirty;
	    ReleaseWriteLock(&vcp->lock);
	}
    }

  fail:
    return code;
}


static int
lock_normalize(struct vnode *vp, 
	       struct flock *lckdat, 
	       offset_t offset, 
	       struct ucred *cred)
{
    struct vattr vattr;
    int code;

    switch (lckdat->l_whence) {
    case 0:
	return 0;
    case 1:
	lckdat->l_start += (off_t) offset;
	break;
    case 2:
	code = afs_getattr(VTOAFS(vp), &vattr, cred);
	if (code != 0)
	    return code;
	lckdat->l_start += (off_t) vattr.va_size;
	break;
    default:
	return EINVAL;
    }
    lckdat->l_whence = 0;
    return 0;
}



int
afs_gn_lockctl(struct vnode *vp, 
	       offset_t offset, 
	       struct eflock *lckdat, 
	       int32long64_t cmd, 
	       int (*ignored_fcn) (),
#ifdef AFS_AIX52_ENV /* Changed in AIX 5.2 and up */
	       ulong * ignored_id, 
#else /* AFS_AIX52_ENV */
	       ulong32int64_t * ignored_id,
#endif /* AFS_AIX52_ENV */
	       struct ucred *cred)
{
    int error, ncmd = 0;
    struct flock flkd;
    struct vattr *attrs;

    AFS_STATCNT(afs_gn_lockctl);
    /* Convert from AIX's cmd to standard lockctl lock types... */
    if (cmd == 0)
	ncmd = F_GETLK;
    else if (cmd & SETFLCK) {
	ncmd = F_SETLK;
	if (cmd & SLPFLCK)
	    ncmd = F_SETLKW;
    }
    flkd.l_type = lckdat->l_type;
    flkd.l_whence = lckdat->l_whence;
    flkd.l_start = lckdat->l_start;
    flkd.l_len = lckdat->l_len;
    flkd.l_pid = lckdat->l_pid;
    flkd.l_sysid = lckdat->l_sysid;

    if (flkd.l_start != lckdat->l_start || flkd.l_len != lckdat->l_len)
	return EINVAL;
    if (error = lock_normalize(vp, &flkd, offset, cred))
	return (error);
    error = afs_lockctl(vp, &flkd, ncmd, cred);
    lckdat->l_type = flkd.l_type;
    lckdat->l_whence = flkd.l_whence;
    lckdat->l_start = flkd.l_start;
    lckdat->l_len = flkd.l_len;
    lckdat->l_pid = flkd.l_pid;
    lckdat->l_sysid = flkd.l_sysid;
    afs_Trace3(afs_iclSetp, CM_TRACE_GLOCKCTL, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, ncmd, ICL_TYPE_LONG, error);
    return (error);
}


/* NOTE: In the nfs glue routine (nfs_gn2sun.c) the order was wrong (vp, flags, cmd, arg, ext); was that another typo? */
int
afs_gn_ioctl(struct vnode *vp, 
	     int32long64_t Cmd, 
	     caddr_t arg, 
	     size_t flags, 		/* Ignored in AFS */
	     ext_t ext, 		/* Ignored in AFS */
	     struct ucred *crp)		/* Ignored in AFS */
{
    int error;
    int cmd = Cmd;

    AFS_STATCNT(afs_gn_ioctl);
    /* This seems to be a perfect fit for our ioctl redirection (afs_xioctl hack); thus the ioctl(2) entry in sysent.c is unaffected in the aix/afs port. */
    error = afs_ioctl(vp, cmd, arg);
    afs_Trace3(afs_iclSetp, CM_TRACE_GIOCTL, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, cmd, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_readlink(struct vnode *vp, 
	        struct uio *uiop, 
		struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_readlink);
    error = afs_readlink(vp, uiop, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GREADLINK, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_select(struct vnode *vp, 
	      int32long64_t correl,
	      ushort e,
	      ushort *re,
	      void (* notify)(),
	      caddr_t vinfo,
	      struct ucred *crp)
{
    AFS_STATCNT(afs_gn_select);
    /* NO SUPPORT for this in afs YET! */
    return (EOPNOTSUPP);
}


int
afs_gn_symlink(struct vnode *vp, 
	       char *link, 
	       char *target, 
	       struct ucred *cred)
{
    struct vattr va;
    int error;

    AFS_STATCNT(afs_gn_symlink);
    VATTR_NULL(&va);
    va.va_mode = 0777;
    error = afs_symlink(vp, link, &va, target, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GSYMLINK, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_STRING, link, ICL_TYPE_STRING, target, ICL_TYPE_LONG,
	       error);
    return (error);
}


int
afs_gn_readdir(struct vnode *vp, 
	       struct uio *uiop, 
	       struct ucred *cred)
{
    int error;

    AFS_STATCNT(afs_gn_readdir);
    error = afs_readdir(vp, uiop, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GREADDIR, ICL_TYPE_POINTER, vp,
	       ICL_TYPE_LONG, error);
    return (error);
}


extern Simple_lock afs_asyncbuf_lock;
extern struct buf *afs_asyncbuf;
extern int afs_asyncbuf_cv;

/*
 * Buffers are ranked by age.  A buffer's age is the value of afs_biotime
 * when the buffer is processed by afs_gn_strategy.  afs_biotime is
 * incremented for each buffer.  A buffer's age is kept in its av_back field.
 * The age ranking is used by the daemons, which favor older buffers.
 */
afs_int32 afs_biotime = 0;

/* This function is called with a list of buffers, threaded through
 * the av_forw field.  Our goal is to copy the list of buffers into the
 * afs_asyncbuf list, sorting buffers into sublists linked by the b_work field.
 * Within buffers within the same work group, the guy with the lowest address
 * has to be located at the head of the queue; his b_bcount field will also
 * be increased to cover all of the buffers in the b_work queue.
 */
#define	AIX_VM_BLKSIZE	8192
/* Note: This function seems to be called as ddstrategy entry point, ie
 * has one argument. However, it also needs to be present as
 * vn_strategy entry point which has three arguments, but it seems to never
 * be called in that capacity (it would fail horribly due to the argument
 * mismatch). I'm confused, but it obviously has to be this way, maybe
 * some IBM people can shed som light on this 
 */
int
afs_gn_strategy(struct buf *abp)
{
    struct buf **lbp, *tbp;
    struct buf **lwbp;
    struct buf *nbp, *qbp, *qnbp, *firstComparable;
    int doMerge;
    int oldPriority;

#define EFS_COMPARABLE(x,y)	((x)->b_vp == (y)->b_vp \
				 && (x)->b_xmemd.subspace_id == (y)->b_xmemd.subspace_id \
				 && (x)->b_flags == (y)->b_flags \
    				 && !((x)->b_flags & B_PFPROT) \
				 && !((y)->b_flags & B_PFPROT))

    oldPriority = disable_lock(INTMAX, &afs_asyncbuf_lock);
    for (tbp = abp; tbp; tbp = nbp) {
	nbp = tbp->av_forw;	/* remember for later */
	tbp->b_work = 0;
	tbp->av_back = (struct buf *)afs_biotime++;

	/* first insert the buffer into the afs_async queue.  Insert buffer
	 * sorted within its disk position within a set of comparable buffers.
	 * Ensure that all comparable buffers are grouped contiguously.
	 * Later on, we'll merge adjacent buffers into a single request.
	 */
	firstComparable = NULL;
	lbp = &afs_asyncbuf;
	for (qbp = *lbp; qbp; lbp = &qbp->av_forw, qbp = *lbp) {
	    if (EFS_COMPARABLE(tbp, qbp)) {
		if (!firstComparable)
		    firstComparable = qbp;
		/* this buffer is comparable, so see if the next buffer
		 * is farther in the file; if it is insert before next buffer.
		 */
		if (tbp->b_blkno < qbp->b_blkno) {
		    break;
		}
	    } else {
		/* If we're at the end of a block of comparable buffers, we
		 * insert the buffer here to keep all comparable buffers
		 * contiguous.
		 */
		if (firstComparable)
		    break;
	    }
	}
	/* do the insert before qbp now */
	tbp->av_forw = *lbp;
	*lbp = tbp;
	if (firstComparable == NULL) {
	    /* next we're going to do all sorts of buffer merging tricks, but
	     * here we know we're the only COMPARABLE block in the
	     * afs_asyncbuf list, so we just skip that and continue with
	     * the next input buffer.
	     */
	    continue;
	}

	/* we may have actually added the "new" firstComparable */
	if (tbp->av_forw == firstComparable)
	    firstComparable = tbp;
	/*
	 * when we get here, firstComparable points to the first dude in the
	 * same vnode and subspace that we (tbp) are in.  We go through the
	 * area of this list with COMPARABLE buffers (a contiguous region) and
	 * repeated merge buffers that are contiguous and in the same block or
	 * buffers that are contiguous and are both integral numbers of blocks.
	 * Note that our end goal is to have as big blocks as we can, but we
	 * must minimize the transfers that are not integral #s of blocks on
	 * block boundaries, since Episode will do those smaller and/or
	 * unaligned I/Os synchronously.
	 *
	 * A useful example to consider has the async queue with this in it:
	 * [8K block, 2 pages] [4K block, 1 page] [4K hole] [8K block, 2 pages]
	 * If we get a request that fills the 4K hole, we want to merge this
	 * whole mess into a 24K, 6 page transfer.  If we don't, however, we
	 * don't want to do any merging since adding the 4K transfer to the 8K
	 * transfer makes the 8K transfer synchronous.
	 *
	 * Note that if there are any blocks whose size is a multiple of
	 * the file system block size, then we know that such blocks are also
	 * on block boundaries.
	 */

	doMerge = 1;		/* start the loop */
	while (doMerge) {	/* loop until an iteration doesn't
				 * make any more changes */
	    doMerge = 0;
	    for (qbp = firstComparable;; qbp = qnbp) {
		qnbp = qbp->av_forw;
		if (!qnbp)
		    break;	/* we're done */
		if (!EFS_COMPARABLE(qbp, qnbp))
		    break;

		/* try to merge qbp and qnbp */

		/* first check if both not adjacent go on to next region */
		if ((dbtob(qbp->b_blkno) + qbp->b_bcount) !=
		    dbtob(qnbp->b_blkno))
		    continue;

		/* note if both in the same block, the first byte of leftmost guy
		 * and last byte of rightmost guy are in the same block.
		 */
		if ((dbtob(qbp->b_blkno) & ~(AIX_VM_BLKSIZE - 1)) ==
		    ((dbtob(qnbp->b_blkno) + qnbp->b_bcount -
		      1) & ~(AIX_VM_BLKSIZE - 1))) {
		    doMerge = 1;	/* both in same block */
		} else if ((qbp->b_bcount & (AIX_VM_BLKSIZE - 1)) == 0
			   && (qnbp->b_bcount & (AIX_VM_BLKSIZE - 1)) == 0) {
		    doMerge = 1;	/* both integral #s of blocks */
		}
		if (doMerge) {
		    struct buf *xbp;

		    /* merge both of these blocks together */
		    /* first set age to the older of the two */
		    if ((int32long64_t) qnbp->av_back - 
			    (int32long64_t) qbp->av_back < 0) {
			qbp->av_back = qnbp->av_back;
		    }
		    lwbp = (struct buf **) &qbp->b_work;
		    /* find end of qbp's work queue */
		    for (xbp = *lwbp; xbp;
			 lwbp = (struct buf **) &xbp->b_work, xbp = *lwbp);
		    /*
		     * now setting *lwbp will change the last ptr in the qbp's
		     * work chain
		     */
		    qbp->av_forw = qnbp->av_forw;	/* splice out qnbp */
		    qbp->b_bcount += qnbp->b_bcount;	/* fix count */
		    *lwbp = qnbp;	/* append qnbp to end */
		    /*
		     * note that qnbp is bogus, but it doesn't matter because
		     * we're going to restart the for loop now.
		     */
		    break;	/* out of the for loop */
		}
	    }
	}
    }				/* for loop for all interrupt data */
    /* at this point, all I/O has been queued.  Wakeup the daemon */
    e_wakeup_one((int *)&afs_asyncbuf_cv);
    unlock_enable(oldPriority, &afs_asyncbuf_lock);
    return 0;
}


int
afs_inactive(struct vcache *avc, 
	     struct AFS_UCRED *acred)
{
    afs_InactiveVCache(avc, acred);
}

int
afs_gn_revoke(struct vnode *vp,
              int32long64_t cmd,
	      int32long64_t flag,
	      struct vattr *vinfop,
	      struct ucred *crp)
{
    AFS_STATCNT(afs_gn_revoke);
    /* NO SUPPORT for this in afs YET! */
    return (EOPNOTSUPP);
}

int
afs_gn_getacl(struct vnode *vp, 
	      struct uio *uiop, 
	      struct ucred *cred)
{
    return ENOSYS;
};


int
afs_gn_setacl(struct vnode *vp, 
	      struct uio *uiop, 
	      struct ucred *cred)
{
    return ENOSYS;
};


int
afs_gn_getpcl(struct vnode *vp, 
	      struct uio *uiop, 
	      struct ucred *cred)
{
    return ENOSYS;
};


int
afs_gn_setpcl(struct vnode *vp, 
	      struct uio *uiop, 
	      struct ucred *cred)
{
    return ENOSYS;
};


int
afs_gn_seek(struct vnode* vp, offset_t * offp, struct ucred * crp)
{
/*
 * File systems which do not wish to do offset validation can simply
 * return 0.  File systems which do not provide the vn_seek entry point
 * will have a maximum offset of OFF_MAX (2 gigabytes minus 1) enforced
 * by the logical file system.
 */
    return 0;
}


int
afs_gn_enosys()
{
    return ENOSYS;
}

/*
 * declare a struct vnodeops and initialize it with ptrs to all functions
 */
struct vnodeops afs_gn_vnodeops = {
    /* creation/naming/deletion */
    afs_gn_link,
    afs_gn_mkdir,
    afs_gn_mknod,
    afs_gn_remove,
    afs_gn_rename,
    afs_gn_rmdir,
    /* lookup, file handle stuff */
    afs_gn_lookup,
    (int(*)(struct vnode*,struct fileid*,struct ucred*))
	afs_gn_fid,
    /* access to files */
    (int(*)(struct vnode *, int32long64_t, ext_t, caddr_t *,struct ucred *))
	afs_gn_open,
    (int(*)(struct vnode *, struct vnode **, int32long64_t,caddr_t, int32long64_t, caddr_t *, struct ucred *))
	afs_gn_create,
    afs_gn_hold,
    afs_gn_rele,
    afs_gn_close,
    afs_gn_map,
    afs_gn_unmap,
    /* manipulate attributes of files */
    afs_gn_access,
    afs_gn_getattr,
    afs_gn_setattr,
    /* data update operations */
    afs_gn_fclear,
    afs_gn_fsync,
    afs_gn_ftrunc,
    afs_gn_rdwr,
    afs_gn_lockctl,
    /* extensions */
    afs_gn_ioctl,
    afs_gn_readlink,
    afs_gn_select,
    afs_gn_symlink,
    afs_gn_readdir,
    /* buffer ops */
    (int(*)(struct vnode*,struct buf*,struct ucred*))
	afs_gn_strategy,
    /* security things */
    afs_gn_revoke,
    afs_gn_getacl,
    afs_gn_setacl,
    afs_gn_getpcl,
    afs_gn_setpcl,
    afs_gn_seek,
    (int(*)(struct vnode *, int32long64_t, int32long64_t, offset_t, offset_t, struct ucred *))
	afs_gn_enosys,		/* vn_fsync_range */
    (int(*)(struct vnode *, struct vnode **, int32long64_t, char *, struct vattr *, int32long64_t, caddr_t *, struct ucred *))
	afs_gn_enosys,		/* vn_create_attr */
    (int(*)(struct vnode *, int32long64_t, void *, size_t, struct ucred *))
	afs_gn_enosys,		/* vn_finfo */
    (int(*)(struct vnode *, caddr_t, offset_t, offset_t, uint32long64_t, uint32long64_t, struct ucred *))
	afs_gn_enosys,		/* vn_map_lloff */
    (int(*)(struct vnode*,struct uio*,int*,struct ucred*))
	afs_gn_enosys,		/* vn_readdir_eofp */
    (int(*)(struct vnode *, enum uio_rw, int32long64_t, struct uio *, ext_t , caddr_t, struct vattr *, struct vattr *, struct ucred *))
	afs_gn_enosys,		/* vn_rdwr_attr */
    (int(*)(struct vnode*,int,void*,struct ucred*))
	afs_gn_enosys,		/* vn_memcntl */
#ifdef AFS_AIX53_ENV /* Present in AIX 5.3 and up */
    (int(*)(struct vnode*,const char*,struct uio*,struct ucred*))
	afs_gn_enosys,		/* vn_getea */
    (int(*)(struct vnode*,const char*,struct uio*,int,struct ucred*))
	afs_gn_enosys,		/* vn_setea */
    (int(*)(struct vnode *, struct uio *, struct ucred *))
	afs_gn_enosys,		/* vn_listea */
    (int(*)(struct vnode *, const char *, struct ucred *))
	afs_gn_enosys,		/* vn_removeea */
    (int(*)(struct vnode *, const char *, struct vattr *, struct ucred *))
	afs_gn_enosys,		/* vn_statea */
    (int(*)(struct vnode *, uint64_t, acl_type_t *, struct uio *, size_t *, mode_t *, struct ucred *))
	afs_gn_enosys,		/* vn_getxacl */
    (int(*)(struct vnode *, uint64_t, acl_type_t, struct uio *, mode_t,  struct ucred *))
	afs_gn_enosys,		/* vn_setxacl */
#else /* AFS_AIX53_ENV */
    afs_gn_enosys,		/* vn_spare7 */
    afs_gn_enosys,		/* vn_spare8 */
    afs_gn_enosys,		/* vn_spare9 */
    afs_gn_enosys,		/* vn_spareA */
    afs_gn_enosys,		/* vn_spareB */
    afs_gn_enosys,		/* vn_spareC */
    afs_gn_enosys,		/* vn_spareD */
#endif /* AFS_AIX53_ENV */
    afs_gn_enosys,		/* vn_spareE */
    afs_gn_enosys		/* vn_spareF */
#ifdef AFS_AIX51_ENV
    ,(int(*)(struct gnode*,long long,char*,unsigned long*, unsigned long*,unsigned int*))
	afs_gn_enosys,		/* pagerBackRange */
    (int64_t(*)(struct gnode*))
	afs_gn_enosys,		/* pagerGetFileSize */
    (void(*)(struct gnode *, vpn_t, vpn_t *, vpn_t *, vpn_t *, boolean_t))
	afs_gn_enosys,		/* pagerReadAhead */
    (void(*)(struct gnode *, int64_t, int64_t, uint))
	afs_gn_enosys,		/* pagerReadWriteBehind */
    (void(*)(struct gnode*,long long,unsigned long,unsigned long,unsigned int))
	afs_gn_enosys		/* pagerEndCopy */
#endif
};
struct vnodeops *afs_ops = &afs_gn_vnodeops;



extern struct vfsops Afs_vfsops;
extern int Afs_init();

#define	AFS_CALLOUT_TBL_SIZE	256

/*
 * the following additional layer of gorp is due to the fact that the
 * filesystem layer no longer obtains the kernel lock for me.  I was relying
 * on this behavior to avoid having to think about locking.
 */

static
vfs_mount(struct vfs *a, struct ucred *b)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_mount) (a, b);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_unmount(struct vfs *a, int b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_unmount) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_root(struct vfs *a, struct vnode **b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_root) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_statfs(struct vfs *a, struct statfs *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_statfs) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_sync(struct gfs *a)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_sync) (a);
    if (!glockOwner)
	AFS_GUNLOCK();
    return ret;
}

static
vfs_vget(struct vfs *a, struct vnode **b, struct fileid *c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_vget) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_cntl(struct vfs *a, int b, caddr_t c, size_t d, struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_cntl) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vfs_quotactl(struct vfs *a, int b, uid_t c, caddr_t d, struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_quotactl) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

#ifdef AFS_AIX51_ENV
static
vfs_syncvfs(struct gfs *a, struct vfs *b, int c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*Afs_vfsops.vfs_syncvfs) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}
#endif


struct vfsops locked_Afs_vfsops = {
    vfs_mount,
    vfs_unmount,
    vfs_root,
    vfs_statfs,
    vfs_sync,
    vfs_vget,
    vfs_cntl,
    vfs_quotactl,
#ifdef AFS_AIX51_ENV
    vfs_syncvfs
#endif
};

static
vn_link(struct vnode *a, struct vnode *b, char *c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_link) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_mkdir(struct vnode *a, char *b, int32long64_t c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_mkdir) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_mknod(struct vnode *a, caddr_t b, int32long64_t c, dev_t d,
	 struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_mknod) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_remove(struct vnode *a, struct vnode *b, char *c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_remove) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_rename(struct vnode *a, struct vnode *b, caddr_t c, struct vnode *d,
	  struct vnode *e, caddr_t f, struct ucred *g)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_rename) (a, b, c, d, e, f, g);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_rmdir(struct vnode *a, struct vnode *b, char *c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_rmdir) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_lookup(struct vnode *a, struct vnode **b, char *c, int32long64_t d,
	  struct vattr *v, struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_lookup) (a, b, c, d, v, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_fid(struct vnode *a, struct fileid *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_fid) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_open(struct vnode *a, 
	int32long64_t b, 
	ext_t c, 
	caddr_t * d, 
	struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_open) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_create(struct vnode *a, struct vnode **b, int32long64_t c, caddr_t d,
	  int32long64_t e, caddr_t * f, struct ucred *g)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_create) (a, b, c, d, e, f, g);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_hold(struct vnode *a)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_hold) (a);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_rele(struct vnode *a)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_rele) (a);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_close(struct vnode *a, int32long64_t b, caddr_t c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_close) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_map(struct vnode *a, caddr_t b, uint32long64_t c, uint32long64_t d,
       uint32long64_t e, struct ucred *f)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_map) (a, b, c, d, e, f);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_unmap(struct vnode *a, int32long64_t b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_unmap) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_access(struct vnode *a, int32long64_t b, int32long64_t c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_access) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_getattr(struct vnode *a, struct vattr *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_getattr) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_setattr(struct vnode *a, int32long64_t b, int32long64_t c, int32long64_t d,
	   int32long64_t e, struct ucred *f)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_setattr) (a, b, c, d, e, f);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_fclear(struct vnode *a, int32long64_t b, offset_t c, offset_t d
	  , caddr_t e, struct ucred *f)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_fclear) (a, b, c, d, e, f);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_fsync(struct vnode *a, int32long64_t b, int32long64_t c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_fsync) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_ftrunc(struct vnode *a, int32long64_t b, offset_t c, caddr_t d,
	  struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_ftrunc) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_rdwr(struct vnode *a, enum uio_rw b, int32long64_t c, struct uio *d,
	ext_t e, caddr_t f, struct vattr *v, struct ucred *g)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_rdwr) (a, b, c, d, e, f, v, g);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_lockctl(struct vnode *a,
	   offset_t b,
	   struct eflock *c,
	   int32long64_t d,
	   int (*e) (),
#ifdef AFS_AIX52_ENV /* Changed in AIX 5.2 and up */
	       ulong * f, 
#else /* AFS_AIX52_ENV */
	       ulong32int64_t * f,
#endif /* AFS_AIX52_ENV */
	   struct ucred *g)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_lockctl) (a, b, c, d, e, f, g);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_ioctl(struct vnode *a, int32long64_t b, caddr_t c, size_t d, ext_t e,
	 struct ucred *f)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_ioctl) (a, b, c, d, e, f);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_readlink(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_readlink) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_select(struct vnode *a, int32long64_t b, ushort c, ushort * d,
	  void (*e) (), caddr_t f, struct ucred *g)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_select) (a, b, c, d, e, f, g);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_symlink(struct vnode *a, char *b, char *c, struct ucred *d)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_symlink) (a, b, c, d);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_readdir(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_readdir) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_revoke(struct vnode *a, int32long64_t b, int32long64_t c, struct vattr *d,
	  struct ucred *e)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_revoke) (a, b, c, d, e);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_getacl(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_getacl) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_setacl(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_setacl) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_getpcl(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_getpcl) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}

static
vn_setpcl(struct vnode *a, struct uio *b, struct ucred *c)
{
    int glockOwner, ret;

    glockOwner = ISAFS_GLOCK();
    if (!glockOwner)
	AFS_GLOCK();
    ret = (*afs_gn_vnodeops.vn_setpcl) (a, b, c);
    if (!glockOwner)
	AFS_GUNLOCK();

    return ret;
}


struct vnodeops locked_afs_gn_vnodeops = {
    vn_link,
    vn_mkdir,
    vn_mknod,
    vn_remove,
    vn_rename,
    vn_rmdir,
    vn_lookup,
    vn_fid,
    vn_open,
    vn_create,
    vn_hold,
    vn_rele,
    vn_close,
    vn_map,
    vn_unmap,
    vn_access,
    vn_getattr,
    vn_setattr,
    vn_fclear,
    vn_fsync,
    vn_ftrunc,
    vn_rdwr,
    vn_lockctl,
    vn_ioctl,
    vn_readlink,
    vn_select,
    vn_symlink,
    vn_readdir,
    (int(*)(struct vnode*,struct buf*,struct ucred*))
	afs_gn_strategy,	/* no locking!!! (discovered the hard way) */
    vn_revoke,
    vn_getacl,
    vn_setacl,
    vn_getpcl,
    vn_setpcl,
    afs_gn_seek,
    (int(*)(struct vnode *, int32long64_t, int32long64_t, offset_t, offset_t, struct ucred *))
	afs_gn_enosys,		/* vn_fsync_range */
    (int(*)(struct vnode *, struct vnode **, int32long64_t, char *, struct vattr *, int32long64_t, caddr_t *, struct ucred *))
	afs_gn_enosys,		/* vn_create_attr */
    (int(*)(struct vnode *, int32long64_t, void *, size_t, struct ucred *))
	afs_gn_enosys,		/* vn_finfo */
    (int(*)(struct vnode *, caddr_t, offset_t, offset_t, uint32long64_t, uint32long64_t, struct ucred *))
	afs_gn_enosys,		/* vn_map_lloff */
    (int(*)(struct vnode*,struct uio*,int*,struct ucred*))
	afs_gn_enosys,		/* vn_readdir_eofp */
    (int(*)(struct vnode *, enum uio_rw, int32long64_t, struct uio *, ext_t , caddr_t, struct vattr *, struct vattr *, struct ucred *))
	afs_gn_enosys,		/* vn_rdwr_attr */
    (int(*)(struct vnode*,int,void*,struct ucred*))
	afs_gn_enosys,		/* vn_memcntl */
#ifdef AFS_AIX53_ENV /* Present in AIX 5.3 and up */
    (int(*)(struct vnode*,const char*,struct uio*,struct ucred*))
	afs_gn_enosys,		/* vn_getea */
    (int(*)(struct vnode*,const char*,struct uio*,int,struct ucred*))
	afs_gn_enosys,		/* vn_setea */
    (int(*)(struct vnode *, struct uio *, struct ucred *))
	afs_gn_enosys,		/* vn_listea */
    (int(*)(struct vnode *, const char *, struct ucred *))
	afs_gn_enosys,		/* vn_removeea */
    (int(*)(struct vnode *, const char *, struct vattr *, struct ucred *))
	afs_gn_enosys,		/* vn_statea */
    (int(*)(struct vnode *, uint64_t, acl_type_t *, struct uio *, size_t *, mode_t *, struct ucred *))
	afs_gn_enosys,		/* vn_getxacl */
    (int(*)(struct vnode *, uint64_t, acl_type_t, struct uio *, mode_t,  struct ucred *))
	afs_gn_enosys,		/* vn_setxacl */
#else /* AFS_AIX53_ENV */
    afs_gn_enosys,		/* vn_spare7 */
    afs_gn_enosys,		/* vn_spare8 */
    afs_gn_enosys,		/* vn_spare9 */
    afs_gn_enosys,		/* vn_spareA */
    afs_gn_enosys,		/* vn_spareB */
    afs_gn_enosys,		/* vn_spareC */
    afs_gn_enosys,		/* vn_spareD */
#endif /* AFS_AIX53_ENV */
    afs_gn_enosys,		/* vn_spareE */
    afs_gn_enosys		/* vn_spareF */
#ifdef AFS_AIX51_ENV
    ,(int(*)(struct gnode*,long long,char*,unsigned long*, unsigned long*,unsigned int*))
	afs_gn_enosys,		/* pagerBackRange */
    (int64_t(*)(struct gnode*))
	afs_gn_enosys,		/* pagerGetFileSize */
    (void(*)(struct gnode *, vpn_t, vpn_t *, vpn_t *, vpn_t *, boolean_t))
	afs_gn_enosys,		/* pagerReadAhead */
    (void(*)(struct gnode *, int64_t, int64_t, uint))
	afs_gn_enosys,		/* pagerReadWriteBehind */
    (void(*)(struct gnode*,long long,unsigned long,unsigned long,unsigned int))
	afs_gn_enosys		/* pagerEndCopy */
#endif
};

struct gfs afs_gfs = {
    &locked_Afs_vfsops,
    &locked_afs_gn_vnodeops,
    AFS_MOUNT_AFS,
    "afs",
    Afs_init,
    GFS_VERSION4 | GFS_VERSION42 | GFS_REMOTE,
    NULL
};
