/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "../afs/param.h"
#include "../h/systm.h"
#include "../h/types.h"
#include "../h/errno.h"
#include "../h/stat.h"
#include "../h/user.h"
#include "../h/uio.h"
#include "../h/vattr.h"
#include "../h/file.h"
#include "../h/vfs.h"
#include "../h/chownx.h"
#include "../h/systm.h"
#include "../h/access.h"
#include "../rpc/types.h"
#include "../afs/osi_vfs.h"
#include "../netinet/in.h"
#include "../h/mbuf.h"
#include "../h/vmuser.h"
#include "../h/shm.h"
#include "../rpc/types.h"
#include "../rpc/xdr.h"

#include "../afs/stds.h"
#include "../afs/afs_osi.h"
#define RFTP_INTERNALS 1
#include "../afs/volerrors.h"
#include "../afsint/afsint.h"
#include "../afsint/vldbint.h"
#include "../afs/lock.h"
#include "../afs/exporter.h"
#include "../afs/afs.h"
#include "../afs/afs_chunkops.h"
#include "../afs/afs_stats.h"
#include "../afs/nfsclient.h"
#include "../afs/icl.h"
#include "../afs/prs_fs.h"
#include "../h/flock.h"


/*
 * declare all the functions so they can be used to init the table
 */
/* creation/naming/deletion */
int afs_gn_link();
int afs_gn_mkdir();
int afs_gn_mknod();
int afs_gn_remove();
int afs_gn_rename();
int afs_gn_rmdir();
/* lookup, file handle stuff */
int afs_gn_lookup();
int afs_gn_fid();
/* access to files */
int afs_gn_open();
int afs_gn_create();
int afs_gn_hold();
int afs_gn_rele();
int afs_gn_close();
int afs_gn_map();
int afs_gn_unmap();
/* manipulate attributes of files */
int afs_gn_access();
int afs_gn_getattr();
int afs_gn_setattr();
/* data update operations */
int afs_gn_fclear();
int afs_gn_fsync();
int afs_gn_ftrunc();
int afs_gn_rdwr();
int afs_gn_lockctl();
/* extensions */
int afs_gn_ioctl();
int afs_gn_readlink();
int afs_gn_select();
int afs_gn_symlink();
int afs_gn_readdir();
/* buffer ops */
int afs_gn_strategy();
/* security things */
int afs_gn_revoke();
int afs_gn_getacl();
int afs_gn_setacl();
int afs_gn_getpcl();
int afs_gn_setpcl();
int afs_gn_enosys();


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
	afs_gn_fid,
	/* access to files */
	afs_gn_open,
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
	afs_gn_strategy,
	/* security things */
	afs_gn_revoke,
	afs_gn_getacl,
	afs_gn_setacl,
	afs_gn_getpcl,
	afs_gn_setpcl,
	afs_gn_enosys,	/* vn_seek */
	afs_gn_enosys,	/* vn_spare0 */
	afs_gn_enosys,	/* vn_spare1 */
	afs_gn_enosys,	/* vn_spare2 */
	afs_gn_enosys,	/* vn_spare3 */
	afs_gn_enosys,	/* vn_spare4 */
	afs_gn_enosys,	/* vn_spare5 */
	afs_gn_enosys,	/* vn_spare6 */
	afs_gn_enosys,	/* vn_spare7 */
	afs_gn_enosys,	/* vn_spare8 */
	afs_gn_enosys,	/* vn_spare9 */
	afs_gn_enosys,	/* vn_spareA */
	afs_gn_enosys,	/* vn_spareB */
	afs_gn_enosys,	/* vn_spareC */
	afs_gn_enosys,	/* vn_spareD */
	afs_gn_enosys,	/* vn_spareE */
	afs_gn_enosys	/* vn_spareF */
};
struct vnodeops *afs_ops = &afs_gn_vnodeops;


int
afs_gn_link(vp, dp, name, cred)
struct	vnode	*vp;
struct	vnode	*dp;
char		*name;
struct ucred	*cred;
{
    int		error;

    AFS_STATCNT(afs_gn_link);
    error = afs_link(vp, dp, name, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GNLINK, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_mkdir(dp, name, mode, cred)
struct	vnode	*dp;
char		*name;
int		mode;
struct ucred	*cred;
{
    struct	vattr	va;
    struct	vnode	*vp;
    int		error;

    AFS_STATCNT(afs_gn_mkdir);
    VATTR_NULL(&va);
    va.va_type = VDIR;
    va.va_mode = (mode & 07777) & ~get_umask();
    error = afs_mkdir(dp, name, &va, &vp, cred);
    if (! error) {
	AFS_RELE(vp);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GMKDIR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_mknod(dp, name, mode, dev, cred)
struct	vnode	*dp;
char		*name;
int		mode;
dev_t		dev;
struct ucred	*cred;
{
    struct	vattr	va;
    struct	vnode	*vp;
    int		error;

    AFS_STATCNT(afs_gn_mknod);
    VATTR_NULL(&va);
    va.va_type = IFTOVT(mode);
    va.va_mode = (mode & 07777) & ~get_umask();

/**** I'm not sure if suser() should stay here since it makes no sense in AFS; however the documentation says that one "should be super-user unless making a FIFO file. Others systems such as SUN do this checking in the early stages of mknod (before the abstraction), so it's equivalently the same! *****/
    if (va.va_type != VFIFO && !suser(&error))
	return(EPERM);
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
	    error = afs_create(dp, name, &va, NONEXCL, mode, &vp, cred);
    }
    if (! error) {
	AFS_RELE(vp);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GMKNOD, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_remove(vp, dp, name, cred)
struct	vnode	*vp;	    /* Ignored in AFS */
struct	vnode	*dp;
char		*name;
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_remove);
   error = afs_remove(dp, name, cred);
   afs_Trace3(afs_iclSetp, CM_TRACE_GREMOVE, ICL_TYPE_POINTER, (afs_int32)dp,
	      ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
   return(error);
}


int
afs_gn_rename(vp, dp, name, tp, tdp, tname, cred)
struct	vnode	*dp;
char		*name;
struct	vnode	*vp;	    /* Ignored in AFS */
struct	vnode	*tp;	    /* Ignored in AFS */
struct	vnode	*tdp;
char	       	*tname;
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_rename);
    error = afs_rename(dp, name, tdp, tname, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GRENAME, ICL_TYPE_POINTER, (afs_int32)dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_STRING, tname, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_rmdir(vp, dp, name, cred)
struct	vnode	*vp;	    /* Ignored in AFS */
struct	vnode	*dp;
char		*name;
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_rmdir);
    error = afs_rmdir(dp, name, cred);
    if (error) {
	if (error == 66 /* 4.3's ENOTEMPTY */)
	    error = EEXIST; 	/* AIX returns EEXIST where 4.3 used ENOTEMPTY */
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_GRMDIR, ICL_TYPE_POINTER, (afs_int32)dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_lookup(dp, vpp, name, flags, vattrp, cred)
struct	vattr	*vattrp;
struct	vnode	*dp;
struct	vnode	**vpp;
char		*name;
afs_uint32         flags;  /* includes FOLLOW... */
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_lookup);
    error = afs_lookup(dp, name, vpp, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GLOOKUP, ICL_TYPE_POINTER, (afs_int32)dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, error);
   if (vattrp != NULL && error == 0)
       afs_gn_getattr(*vpp, vattrp, cred);
    return(error);
}


int
afs_gn_fid(vp, fidp, cred)
struct	vnode	*vp;
struct fid	*fidp;
struct ucred	*cred;
{
    int	    error;

    AFS_STATCNT(afs_gn_fid);
    error =  afs_fid(vp, fidp);
    afs_Trace3(afs_iclSetp, CM_TRACE_GFID, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, (afs_int32)fidp, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_open(vp, flags, ext, vinfop, cred)
struct	vnode	*vp;
int		flags;
int		ext;		/* Ignored in AFS */
struct ucred	**vinfop;	/* return ptr for fp->f_vinfo, used as fp->f_cred */
struct ucred	*cred;
{
    int		error;
    struct vattr	va;
    struct vcache *tvp = (struct vcache *)vp;
    afs_int32 modes;

    AFS_STATCNT(afs_gn_open);
    modes = 0;
    if ((flags & FREAD)) modes |= R_ACC;
    if ((flags & FEXEC)) modes |= X_ACC;
    if ((flags & FWRITE) || (flags & FTRUNC)) modes |= W_ACC;

    while ((flags & FNSHARE) && tvp->opens) {
	if (!(flags & FDELAY)) {
	   error = ETXTBSY;
	   goto abort;
	}
	afs_osi_Sleep(&tvp->opens);
    }

    error = afs_access(vp, modes, cred);
    if (error) {
       goto abort;
    }

    error = afs_open(&vp, flags, cred);
    if (!error) {
	if (flags & FTRUNC) {
	    VATTR_NULL(&va);
	    va.va_size = 0;
	    error = afs_setattr(vp, &va, cred);
	}

	if (flags & FNSHARE)
	    tvp->states |= CNSHARE;

	if (! error) {
	    *vinfop = cred; /* fp->f_vinfo is like fp->f_cred in suns */
	}
	else {
	    /* an error occurred; we've told CM that the file
             * is open, so close it now so that open and
             * writer counts are correct.  Ignore error code,
             * as it is likely to fail (the setattr just did).
             */
	    afs_close(vp, flags, cred);
	}
    }

abort:
    afs_Trace3(afs_iclSetp, CM_TRACE_GOPEN, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_create(dp, vpp, flags, name, mode, vinfop, cred)
struct	vnode	*dp;
struct	vnode	**vpp;
int		flags;
char		*name;
int		mode;
struct ucred	**vinfop; /* return ptr for fp->f_vinfo, used as fp->f_cred */
struct ucred	*cred;
{
    struct	vattr	va;
    enum	vcexcl	exclusive;
    int		error, modes=0;

    AFS_STATCNT(afs_gn_create);
    if ((flags & (O_EXCL|O_CREAT)) == (O_EXCL|O_CREAT))
	exclusive = EXCL;
    else	
	exclusive = NONEXCL;
    VATTR_NULL(&va);
    va.va_type = VREG;
    va.va_mode = (mode & 07777) & ~get_umask();
    if ((flags & FREAD)) modes |= R_ACC;
    if ((flags & FEXEC)) modes |= X_ACC;
    if ((flags & FWRITE) || (flags & FTRUNC)) modes |= W_ACC;
    error = afs_create(dp, name, &va, exclusive, modes, vpp, cred);
    if (error) {
	return error;
    }
    /* 'cr_luid' is a flag (when it comes thru the NFS server it's set to
     * RMTUSER_REQ) that determines if we should call afs_open(). We shouldn't
     * call it when this NFS traffic since the close will never happen thus
     * we'd never flush the files out to the server! Gross but the simplest
     * solution we came out with */
    if (cred->cr_luid != RMTUSER_REQ) {
	while ((flags & FNSHARE) && ((struct vcache *)*vpp)->opens) {
	    if (!(flags & FDELAY))
		return ETXTBSY;
	    afs_osi_Sleep(&((struct vcache *)*vpp)->opens);
	}
	/* Since in the standard copen() for bsd vnode kernels they do an
	 * vop_open after the vop_create, we must do the open here since there
	 * are stuff in afs_open that we need. For example advance the
	 * execsOrWriters flag (else we'll be treated as the sun's "core"
	 * case). */
	*vinfop	= cred; /* save user creds in fp->f_vinfo */
	error = afs_open(vpp, flags, cred);
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GCREATE, ICL_TYPE_POINTER, (afs_int32)dp,
	       ICL_TYPE_STRING, name, ICL_TYPE_LONG, mode, ICL_TYPE_LONG, error);
    return error;
}


int
afs_gn_hold(vp)
struct	vnode	*vp;
{
    AFS_STATCNT(afs_gn_hold);
    ++(vp->v_count);
    return(0);
}

int vmPageHog = 0;

int
afs_gn_rele(vp)
struct	vnode	*vp;
{
   struct vcache *vcp = (struct vcache *)vp;
   int		error = 0;

    AFS_STATCNT(afs_gn_rele);
    if (vp->v_count == 0)
	osi_Panic("afs_rele: zero v_count");
    if (--(vp->v_count) == 0) {
	if (vcp->states & CPageHog) {
	    vmPageHog --;
	    vcp->states &= ~CPageHog;
	}
	error = afs_inactive(vp, 0);
    }
    return(error);
}


int
afs_gn_close(vp, flags, vinfo, cred)
struct	vnode	*vp;
int		flags;
caddr_t		vinfo;		/* Ignored in AFS */
struct ucred	*cred;
{
    int		error;
    struct vcache *tvp = (struct vcache *)vp;

    AFS_STATCNT(afs_gn_close);

    if (flags & FNSHARE) {
	tvp->states &= ~CNSHARE;
	afs_osi_Wakeup(&tvp->opens);
    }

    error = afs_close(vp, flags, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GCLOSE, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_map(vp, addr, len, off, flag, cred)
struct	vnode	*vp;
caddr_t		addr;
u_int 		len, off, flag;
struct ucred	*cred;
{
    struct vcache *vcp = (struct vcache *)vp;
    struct vrequest treq;
    afs_int32 error;
    AFS_STATCNT(afs_gn_map);
#ifdef	notdef
    if (error = afs_InitReq(&treq, cred)) return error;
    error = afs_VerifyVCache(vcp, &treq);
    if (error)
	return afs_CheckCode(error, &treq, 49);
#endif
    osi_FlushPages(vcp, cred);		/* XXX ensure old pages are gone XXX */
    ObtainWriteLock(&vcp->lock, 401);
    vcp->states |= CMAPPED;		/* flag cleared at afs_inactive */
    /*
     * We map the segment into our address space using the handle returned by vm_create.
     */
    if (!vcp->vmh) {
	/* Consider  V_INTRSEG too for interrupts */
	if (error = vms_create(&vcp->segid, V_CLIENT, vcp->v.v_gnode, vcp->m.Length, 0, 0)) {
	    ReleaseWriteLock(&vcp->lock);
	    return(EOPNOTSUPP);
	}
	vcp->vmh = SRVAL(vcp->segid, 0, 0);
    }
    vcp->v.v_gnode->gn_seg = vcp->segid; 	/* XXX Important XXX */
    if (flag & SHM_RDONLY) {
	vp->v_gnode->gn_mrdcnt++;
    }
    else {
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
	    vcp->credp = (struct ucred *)0;
	    crfree(crp);
	}
	vcp->credp = cred;
    }
    ReleaseWriteLock(&vcp->lock);
    VN_HOLD(vp);
    afs_Trace4(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, addr, ICL_TYPE_LONG, len, ICL_TYPE_LONG, off);
    return(0);
}


int
afs_gn_unmap(vp, flag, cred)
struct	vnode	*vp;
int 		flag;
struct ucred	*cred;
{
    struct vcache *vcp = (struct vcache *)vp;
    AFS_STATCNT(afs_gn_unmap);
    ObtainWriteLock(&vcp->lock, 402);
    if (flag & SHM_RDONLY) {
	vp->v_gnode->gn_mrdcnt--;
	if (vp->v_gnode->gn_mrdcnt <=0) vp->v_gnode->gn_mrdcnt = 0;
    }
    else {
	vp->v_gnode->gn_mwrcnt--;
	if (vp->v_gnode->gn_mwrcnt <=0) vp->v_gnode->gn_mwrcnt = 0;
    }
    ReleaseWriteLock(&vcp->lock);

    AFS_RELE(vp);
    return 0;
}


int
afs_gn_access(vp, mode, who, cred)
struct	vnode		*vp;
int			mode;
int			who;
struct ucred		*cred;
{
    int	error;
    struct vattr vattr;

    AFS_STATCNT(afs_gn_access);
    if (mode & ~0x7)
	return(EINVAL);

    error = afs_access(vp, mode, cred);
    if (!error) {
	/* Additional testing */
	if (who == ACC_OTHERS || who == ACC_ANY) {
	    error = afs_getattr(vp, &vattr, cred);
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
	    error = afs_getattr(vp, &vattr, cred);
	    if (!error) {
		if ((!((vattr.va_mode >> 6) & mode)) || (!((vattr.va_mode >> 3) & mode)) ||
			(!(vattr.va_mode & mode)))
		    error = EACCES;
		else
		    error = 0;
	    }
	}
  
    }
out:
    afs_Trace3(afs_iclSetp, CM_TRACE_GACCESS, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, mode, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_getattr(vp, vattrp, cred)
struct	vnode	*vp;
struct	vattr	*vattrp;
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_getattr);
    error = afs_getattr(vp, vattrp, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GGETATTR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_setattr(vp, op, arg1, arg2, arg3, cred)
struct	vnode	*vp;
int		op;
int		arg1;
int		arg2;
int		arg3;
struct ucred	*cred;
{
    struct	vattr	va;
    int		error = 0;

   AFS_STATCNT(afs_gn_setattr);
    VATTR_NULL(&va);
    switch(op) {
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
		return(error);
#endif
	    if (arg1 & T_SETTIME) {
		va.va_atime.tv_sec = time;
		va.va_mtime.tv_sec = time;
	    } else {
		va.va_atime = *(struct timestruc_t *) arg2;
		va.va_mtime = *(struct timestruc_t *) arg3;
	    }
	    break;
	default:
	    return(EINVAL);
    }

    error = afs_setattr(vp, &va, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GSETATTR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, error);
    return(error);
}


char zero_buffer[PAGESIZE];
int
afs_gn_fclear(vp, flags, offset, length, vinfo, cred) 
struct	vnode	*vp;
int		flags;
offset_t	offset;
offset_t	length;
caddr_t		vinfo;
struct ucred	*cred;
{
    int i, len, error = 0;
    struct iovec iov;
    struct uio uio;
    static int fclear_init =0; 
    register struct vcache *avc = (struct vcache *)vp;

   AFS_STATCNT(afs_gn_fclear);
    if (!fclear_init) {
	bzero(zero_buffer, PAGESIZE);
	fclear_init = 1;
    }
    /*
     * Don't clear past ulimit
     */
    if (offset + length > get_ulimit())
	return(EFBIG);

    /* Flush all pages first */
    if (avc->segid) {
	AFS_GUNLOCK();
	vm_flushp(avc->segid, 0, MAXFSIZE/PAGESIZE - 1);
	vms_iowait(avc->vmh);	
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
	if (error = afs_rdwr(vp, &uio, UIO_WRITE, 0, cred))
	    break;
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_GFCLEAR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, offset, ICL_TYPE_LONG, length, ICL_TYPE_LONG, error);
    return (error);
}


int
afs_gn_fsync(vp, flags, vinfo, cred)
struct	vnode	*vp;
int		flags;	    /* Not used by AFS */
caddr_t		vinfo;	    /* Not used by AFS */
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_fsync);
    error = afs_fsync(vp, cred);
    afs_Trace3(afs_iclSetp, CM_TRACE_GFSYNC, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_ftrunc(vp, flags, length, vinfo, cred)
struct	vnode	*vp;
int		flags;	    /* Ignored in AFS */
offset_t	length;
caddr_t		vinfo;	    /* Ignored in AFS */
struct ucred	*cred;
{
    struct vattr	va;
    int		error;

    AFS_STATCNT(afs_gn_ftrunc);
    VATTR_NULL(&va);
    va.va_size = length;
    error = afs_setattr(vp, &va, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GFTRUNC, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, length, ICL_TYPE_LONG, error);
    return(error);
}

/* Min size of a file which is dumping core before we declare it a page hog. */
#define MIN_PAGE_HOG_SIZE 8388608

int afs_gn_rdwr(vp, op, flags, ubuf, ext, vinfo, vattrp, cred)
struct	vnode	*vp;
enum	uio_rw	op;
int		flags;
struct	uio	*ubuf;
int		ext;	    /* Ignored in AFS */
caddr_t		vinfo;	    /* Ignored in AFS */
struct	vattr	*vattrp;
struct ucred	*cred;
{
    register struct vcache *vcp = (struct vcache *)vp;   
    struct vrequest treq;
    int error=0;
    int free_cred = 0;

    AFS_STATCNT(afs_gn_rdwr);

    if (vcp->vc_error) {
	if (op == UIO_WRITE)
	    return vcp->vc_error;
	else
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
	crhold(cred); /* Bump refcount for reference in vcache */

	if (vcp->credp) {
	    struct ucred *crp;
	    UpgradeSToWLock(&vcp->lock, 508);
	    crp = vcp->credp;
	    vcp->credp = (struct ucred *)0;
	    ConvertWToSLock(&vcp->lock);
	    crfree(crp);
	}
	vcp->credp = cred;	
    }
    ReleaseSharedLock(&vcp->lock);

    /*
     * XXX Is the following really required?? XXX
     */
    if (error = afs_InitReq(&treq, cred)) return error;
    if (error = afs_VerifyVCache(vcp, &treq))
	return afs_CheckCode(error, &treq, 50);
    osi_FlushPages(vcp, cred);		/* Flush old pages */

    if (AFS_NFSXLATORREQ(cred)) {
	if (flags & FSYNC)
	    flags &= ~FSYNC;
	if (op == UIO_READ) {
	    if (!afs_AccessOK(vcp, PRSFS_READ, &treq,
			      CHECK_MODE_BITS|CMB_ALLOW_EXEC_AS_READ)) {
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
	ObtainWriteLock(&vcp->lock,240);
	vcp->states |= CDirty;		/* Set the dirty bit */
	afs_FakeOpen(vcp);
	ReleaseWriteLock(&vcp->lock);
    }

    error = afs_vm_rdwr(vp, ubuf, op, flags, cred);

    if (op == UIO_WRITE) {
	ObtainWriteLock(&vcp->lock,241);
	afs_FakeClose(vcp, cred);	/* XXXX For nfs trans and cores XXXX */
	ReleaseWriteLock(&vcp->lock);
    }
    if (vattrp != NULL && error == 0)
	afs_gn_getattr(vp, vattrp, cred);

    afs_Trace4(afs_iclSetp, CM_TRACE_GRDWR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, flags, ICL_TYPE_LONG, op, ICL_TYPE_LONG, error);

    if (free_cred)
      crfree(cred);
    return(error);
}


#define AFS_MAX_VM_CHUNKS 10
afs_vm_rdwr(vp, uiop, rw, ioflag, credp)
    register struct vnode *vp;
    struct uio *uiop;
    enum uio_rw rw;
    int ioflag;
    struct ucred *credp; 
{
    register afs_int32 code = 0;
    register int i;
    afs_int32 blockSize, fileSize;
    afs_int32 xfrSize, xfrOffset;
    register struct vcache *vcp = (struct vcache *)vp;
    struct dcache *tdc;
    register afs_int32 start_offset;
    int save_resid = uiop->afsio_resid;
    int old_offset, first_page;
    int offset, count, len;
    int counter = 0;
    struct vrequest treq;

    if (code = afs_InitReq(&treq, credp)) return code;

    /* special case easy transfer; apparently a lot are done */
    if ((xfrSize=uiop->afsio_resid) == 0) return 0;

    ObtainReadLock(&vcp->lock);
    fileSize = vcp->m.Length;
    if (rw == UIO_WRITE && (ioflag & IO_APPEND)) { /* handle IO_APPEND mode */
	uiop->afsio_offset = fileSize;
    }
    /* compute xfrOffset now, and do some checks */
    xfrOffset = uiop->afsio_offset;
    if (xfrOffset < 0 || xfrOffset + xfrSize < 0) {
	code = EINVAL;
	goto fail;
    }

    /* check for "file too big" error, which should really be done above us */
    if (rw == UIO_WRITE && xfrSize + fileSize > get_ulimit()) {
	code = EFBIG;
	goto fail;
    }

    if (!vcp->vmh) {
	/* Consider  V_INTRSEG too for interrupts */
	if (code = vms_create(&vcp->segid, V_CLIENT, vcp->v.v_gnode,
			      vcp->m.Length, 0, 0)) {
	    goto fail;
	}
	vcp->vmh = SRVAL(vcp->segid, 0, 0);	
    }
    if (rw == UIO_READ) {
	/* don't read past EOF */
	if (xfrSize+xfrOffset > fileSize)
	    xfrSize = fileSize - xfrOffset;
	if (xfrSize <= 0) goto fail;	    
	ReleaseReadLock(&vcp->lock);
	AFS_GUNLOCK();
	code = vm_move(vcp->segid, xfrOffset, xfrSize, rw, uiop);
	AFS_GLOCK();
	/*
	 * If at a chunk boundary and staying within chunk,
	 * start prefetch of next chunk.
	 */
	if (counter == 0 || AFS_CHUNKOFFSET(xfrOffset) == 0
	    && xfrSize <= AFS_CHUNKSIZE(xfrOffset)) {
	    ObtainWriteLock(&vcp->lock,407);
	    tdc = afs_FindDCache(vcp, xfrOffset);
            if (tdc) {
	        if (!(tdc->flags & DFNextStarted))
	            afs_PrefetchChunk(vcp, tdc, credp, &treq);
	        afs_PutDCache(tdc);
            }
	    ReleaseWriteLock(&vcp->lock);
	}
	return code;
    }

    /* UIO_WRITE */
    ReleaseReadLock(&vcp->lock);
    ObtainWriteLock(&vcp->lock,400);
    vcp->m.Date = osi_Time();	/* Set file date (for ranlib) */
    /* extend file */
    /* un-protect last page. */
    vm_protectp(vcp->vmh, vcp->m.Length/PAGESIZE, 1, FILEKEY);
    if (xfrSize + xfrOffset > fileSize) {
	vcp->m.Length = xfrSize+xfrOffset;
    }	    
    if ((!(vcp->states & CPageHog)) && (xfrSize >= MIN_PAGE_HOG_SIZE)) {
	vmPageHog ++;
	vcp->states |= CPageHog;
    }
    ReleaseWriteLock(&vcp->lock);
    
    /* If the write will fit into a single chunk we'll write all of it
     * at once. Otherwise, we'll write one chunk at a time, flushing
     * some of it to disk.
     */
    start_offset = uiop->afsio_offset;
    count = 0;

    /* Only create a page to avoid excess VM access if we're writing a
     * small file which is either new or completely overwrites the 
     * existing file.
     */
    if ((xfrOffset == 0) && (xfrSize < PAGESIZE) && (xfrSize >= fileSize) &&
	(vcp->v.v_gnode->gn_mwrcnt == 0) &&
	(vcp->v.v_gnode->gn_mrdcnt == 0)) {
	    (void) vm_makep(vcp->segid, 0);
    }
	
    while (xfrSize > 0) {
	offset = AFS_CHUNKBASE(xfrOffset);
	len = xfrSize;
	
	if (AFS_CHUNKSIZE(xfrOffset) <= len)
	    len = AFS_CHUNKSIZE(xfrOffset) - (xfrOffset - offset);
	
	if (len == xfrSize) {
	    /* All data goes to this one chunk. */
	    AFS_GUNLOCK();
	    old_offset = uiop->afsio_offset;
	    code = vm_move(vcp->segid, xfrOffset, xfrSize, rw, uiop);
	    AFS_GLOCK();
	    xfrOffset += len;
	    xfrSize = 0;
	}
	else {
	    /* Write just one chunk's worth of data. */
	    struct uio tuio;
	    struct iovec tvec[16]; /* Should have access to #define */

	    /* Purge dirty chunks of file if there are too many dirty chunks.
	     * Inside the write loop, we only do this at a chunk boundary.
	     * Clean up partial chunk if necessary at end of loop.
	     */
	    if (counter > 0 && code == 0 && xfrOffset == offset) {
		ObtainWriteLock(&vcp->lock,403);
	        code = afs_DoPartialWrite(vcp, &treq);
		vcp->states |= CDirty;
		ReleaseWriteLock(&vcp->lock);
	    }
	    counter++;
	    
	    afsio_copy(uiop, &tuio, tvec);
	    afsio_trim(&tuio, len);
	    tuio.afsio_offset = xfrOffset;
	    
	    AFS_GUNLOCK();
	    old_offset = uiop->afsio_offset;
	    code = vm_move(vcp->segid, xfrOffset, len, rw, &tuio);
	    AFS_GLOCK();
	    len -= tuio.afsio_resid;
	    afsio_skip(uiop, len);
	    xfrSize -= len;
	    xfrOffset += len;
	}
	
	first_page = old_offset >> PGSHIFT;
	AFS_GUNLOCK();
	code = vm_writep(vcp->segid, first_page,
		  1 + ((old_offset + (len - 1)) >> PGSHIFT) - first_page);
	if (++count > AFS_MAX_VM_CHUNKS) {
	    count = 0;
	    vms_iowait(vcp->segid);
	}
	AFS_GLOCK();
	
    }

    if (count) {
	AFS_GUNLOCK();
	vms_iowait(vcp->segid);
	AFS_GLOCK();
    }

    ObtainWriteLock(&vcp->lock,242);
    if (code == 0 && (vcp->states & CDirty)) {
	code = afs_DoPartialWrite(vcp, &treq);
    }
    vm_protectp(vcp->vmh, vcp->m.Length/PAGESIZE, 1, RDONLY);
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

  fail:
    ReleaseReadLock(&vcp->lock);
    return code;
}



static int lock_normalize(vp, lckdat, offset, cred)
    struct vnode *vp;
    struct eflock *lckdat;
    offset_t offset;
    struct ucred *cred;
{
    struct vattr vattr;
    int code;

    switch(lckdat->l_whence) {
	case 0:
	    return 0;
	case 1:
	    lckdat->l_start += (off_t) offset;
	    break;
	case 2:
	    code = afs_getattr(vp, &vattr, cred);
	    if (code != 0) return code;
	    lckdat->l_start += (off_t) vattr.va_size;
	    break;
	default: return EINVAL;
    }
    lckdat->l_whence = 0;
    return 0;
}



afs_gn_lockctl(vp, offset, lckdat, cmd, ignored_fcn, ignored_id, cred)
void (*ignored_fcn)();
void *ignored_id;
struct	vnode	*vp;
offset_t	offset;
struct	eflock	*lckdat;
struct ucred	*cred;
{
   int		error, ncmd=0;
   struct flock	flkd;
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
   flkd.l_type   = lckdat->l_type;
   flkd.l_whence = lckdat->l_whence;
   flkd.l_start  = lckdat->l_start;
   flkd.l_len    = lckdat->l_len;
   flkd.l_pid    = lckdat->l_pid;
   flkd.l_sysid  = lckdat->l_sysid;
   
   if (flkd.l_start != lckdat->l_start || flkd.l_len != lckdat->l_len)
       return EINVAL;
    if (error = lock_normalize(vp, &flkd, offset, cred))
      return(error);
    error = afs_lockctl(vp, &flkd, ncmd, cred);
   lckdat->l_type   = flkd.l_type;
   lckdat->l_whence = flkd.l_whence;
   lckdat->l_start  = flkd.l_start;
   lckdat->l_len    = flkd.l_len;
   lckdat->l_pid    = flkd.l_pid;
   lckdat->l_sysid  = flkd.l_sysid;
    afs_Trace3(afs_iclSetp, CM_TRACE_GLOCKCTL, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, ncmd, ICL_TYPE_LONG, error);
    return(error);
}


/* NOTE: In the nfs glue routine (nfs_gn2sun.c) the order was wrong (vp, flags, cmd, arg, ext); was that another typo? */
int afs_gn_ioctl(vp, cmd, arg, flags, channel, ext)
struct	vnode	*vp;
int		cmd;
int		arg;
int		flags;		/* Ignored in AFS */
int		channel;	/* Ignored in AFS */
int		ext;		/* Ignored in AFS */
{
    int	error;
    AFS_STATCNT(afs_gn_ioctl);
    /* This seems to be a perfect fit for our ioctl redirection (afs_xioctl hack); thus the ioctl(2) entry in sysent.c is unaffected in the aix/afs port. */ 
    error = afs_ioctl(vp, cmd, arg);
    afs_Trace3(afs_iclSetp, CM_TRACE_GIOCTL, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, cmd, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_readlink(vp, uiop, cred) 
struct	vnode	*vp;
struct	uio	*uiop;
struct	ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_readlink);
    error = afs_readlink(vp, uiop, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GREADLINK, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_select(vp, which, vinfo, mpx)
struct	vnode	*vp;
int		which;
caddr_t		*vinfo;
caddr_t		*mpx;
{
   AFS_STATCNT(afs_gn_select);
    /* NO SUPPORT for this in afs YET! */
    return(EOPNOTSUPP);
}


int
afs_gn_symlink(vp, link, target, cred)
struct	vnode	*vp;
char		*target;
char	   	*link;
struct ucred	*cred;
{
    struct vattr	va;
   int		error;

    AFS_STATCNT(afs_gn_symlink);
    VATTR_NULL(&va);
    va.va_mode = 0777;
    error = afs_symlink(vp, link, &va, target, cred);
    afs_Trace4(afs_iclSetp, CM_TRACE_GSYMLINK, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_STRING, link, ICL_TYPE_STRING, target, ICL_TYPE_LONG, error);
    return(error);
}


int
afs_gn_readdir(vp, uiop, cred)
struct	vnode	*vp;
struct	uio	*uiop;
struct ucred	*cred;
{
   int		error;

   AFS_STATCNT(afs_gn_readdir);
    error = afs_readdir(vp, uiop, cred);
    afs_Trace2(afs_iclSetp, CM_TRACE_GREADDIR, ICL_TYPE_POINTER, (afs_int32)vp,
	       ICL_TYPE_LONG, error);
    return(error);
}


extern Simple_lock afs_asyncbuf_lock;
/*
 * Buffers are ranked by age.  A buffer's age is the value of afs_biotime
 * when the buffer is processed by naix_vmstrategy.  afs_biotime is
 * incremented for each buffer.  A buffer's age is kept in its av_back field.
 * The age ranking is used by the daemons, which favor older buffers.
 */
afs_int32 afs_biotime = 0;

extern struct buf *afs_asyncbuf;
extern int afs_asyncbuf_cv;
/* This function is called with a list of buffers, threaded through
 * the av_forw field.  Our goal is to copy the list of buffers into the
 * afs_asyncbuf list, sorting buffers into sublists linked by the b_work field.
 * Within buffers within the same work group, the guy with the lowest address
 * has to be located at the head of the queue; his b_bcount field will also
 * be increased to cover all of the buffers in the b_work queue.
 */
#define	AIX_VM_BLKSIZE	8192
afs_gn_strategy(abp, cred)
struct ucred *cred;
register struct buf *abp; 
{
    register struct buf **lbp, *tbp;
    int *lwbp;		/* last guy in work chain */
    struct buf *nbp, *qbp, *qnbp, *firstComparable;
    int doMerge;
    int oldPriority;

#define EFS_COMPARABLE(x,y)	((x)->b_vp == (y)->b_vp \
				 && (x)->b_xmemd.subspace_id == (y)->b_xmemd.subspace_id \
				 && (x)->b_flags == (y)->b_flags \
    				 && !((x)->b_flags & B_PFPROT) \
				 && !((y)->b_flags & B_PFPROT))

    oldPriority = disable_lock(INTMAX, &afs_asyncbuf_lock);
    for(tbp = abp; tbp; tbp=nbp) {
	nbp = tbp->av_forw;	/* remember for later */
	tbp->b_work = 0;
	tbp->av_back = (struct buf *) afs_biotime++;

	/* first insert the buffer into the afs_async queue.  Insert buffer
	 * sorted within its disk position within a set of comparable buffers.
	 * Ensure that all comparable buffers are grouped contiguously.
	 * Later on, we'll merge adjacent buffers into a single request.
	 */
	firstComparable = (struct buf *) 0;
	lbp = &afs_asyncbuf;
	for(qbp = *lbp; qbp; lbp = &qbp->av_forw, qbp = *lbp) {
	    if (EFS_COMPARABLE(tbp, qbp)) {
		if (!firstComparable) firstComparable = qbp;
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
	if (firstComparable == (struct buf *) 0) {
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
	
	doMerge = 1;			/* start the loop */
	while(doMerge) {		/* loop until an iteration doesn't
					 * make any more changes */
	    doMerge = 0;
	    for (qbp = firstComparable; ; qbp = qnbp) {
		qnbp = qbp->av_forw;
		if (!qnbp) break;	/* we're done */
		if (!EFS_COMPARABLE(qbp, qnbp)) break;

		/* try to merge qbp and qnbp */

		/* first check if both not adjacent go on to next region */
		if ((dbtob(qbp->b_blkno) + qbp->b_bcount) != dbtob(qnbp->b_blkno)) 
		    continue;

		/* note if both in the same block, the first byte of leftmost guy
		 * and last byte of rightmost guy are in the same block.
		 */
		if ((dbtob(qbp->b_blkno) & ~(AIX_VM_BLKSIZE-1)) ==
		    ((dbtob(qnbp->b_blkno)+qnbp->b_bcount-1) & ~(AIX_VM_BLKSIZE-1))) {
		    doMerge = 1;	/* both in same block */
		}
		else if ((qbp->b_bcount & (AIX_VM_BLKSIZE-1)) == 0
			 && (qnbp->b_bcount & (AIX_VM_BLKSIZE-1)) == 0) {
		    doMerge = 1;	/* both integral #s of blocks */
		}
		if (doMerge) {
		    register struct buf *xbp;

		    /* merge both of these blocks together */
		    /* first set age to the older of the two */
		    if ((int) qnbp->av_back - (int) qbp->av_back < 0)
			qbp->av_back = qnbp->av_back;
		    lwbp = &qbp->b_work;
		    /* find end of qbp's work queue */
		    for(xbp = (struct buf *)(*lwbp); xbp;
			lwbp = &xbp->b_work, xbp = (struct buf *) (*lwbp));
		    /*
		     * now setting *lwbp will change the last ptr in the qbp's
		     * work chain
		     */
		    qbp->av_forw = qnbp->av_forw; /* splice out qnbp */
		    qbp->b_bcount += qnbp->b_bcount; /* fix count */
		    *lwbp = (int) qnbp; /* append qnbp to end */
		    /*
		     * note that qnbp is bogus, but it doesn't matter because
		     * we're going to restart the for loop now.
		     */
		    break; /* out of the for loop */
		}
	    }
	}
    }	/* for loop for all interrupt data */
    /* at this point, all I/O has been queued.  Wakeup the daemon */
    e_wakeup_one((int*) &afs_asyncbuf_cv);
    unlock_enable(oldPriority, &afs_asyncbuf_lock);
    return 0;
}


afs_inactive(avc, acred)
    register struct vcache *avc;
    struct AFS_UCRED *acred;
{
    afs_InactiveVCache(avc, acred);
}

int
afs_gn_revoke(vp)
struct	vnode	*vp;
{
    AFS_STATCNT(afs_gn_revoke);
    /* NO SUPPORT for this in afs YET! */
    return(EOPNOTSUPP);
}

int afs_gn_getacl(vp, uiop, cred) 
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
{ 
    return ENOSYS;
};


int afs_gn_setacl(vp, uiop, cred) 
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
{ 
    return ENOSYS;
};


int afs_gn_getpcl(vp, uiop, cred) 
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
{ 
    return ENOSYS;
};


int afs_gn_setpcl(vp, uiop, cred) 
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
{ 
    return ENOSYS;
};
int afs_gn_enosys()
{
    return ENOSYS;
}
 
extern struct vfsops	Afs_vfsops;
extern struct vnodeops	afs_gn_vnodeops;
extern int		Afs_init();

#define	AFS_CALLOUT_TBL_SIZE	256

/*
 * the following additional layer of gorp is due to the fact that the
 * filesystem layer no longer obtains the kernel lock for me.  I was relying
 * on this behavior to avoid having to think about locking.
 */

static
vfs_mount(struct vfs *a, struct ucred *b) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_mount)(a, b);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_unmount(struct vfs *a, int b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_unmount)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_root(struct vfs *a, struct vnode **b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_root)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_statfs(struct vfs *a, struct statfs *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_statfs)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_sync(struct gfs *a) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_sync)(a);
	if (!glockOwner)
	    AFS_GUNLOCK();
	return ret;
}

static
vfs_vget(struct vfs *a, struct vnode **b, struct fileid *c
	 , struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_vget)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_cntl(struct vfs *a, int b, caddr_t c, size_t d, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_cntl)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vfs_quotactl(struct vfs *a, int b, uid_t c, caddr_t d
	     , struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*Afs_vfsops.vfs_quotactl)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}


struct vfsops locked_Afs_vfsops = {
	vfs_mount,
	vfs_unmount,
	vfs_root,
	vfs_statfs,
	vfs_sync,
	vfs_vget,
	vfs_cntl,
	vfs_quotactl,
};

static
vn_link(struct vnode *a, struct vnode *b, char *c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_link)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_mkdir(struct vnode *a, char *b, int c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_mkdir)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_mknod(struct vnode *a, caddr_t b, int c, dev_t d, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_mknod)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_remove(struct vnode *a, struct vnode *b, char *c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_remove)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_rename(struct vnode *a, struct vnode *b, caddr_t c
	  , struct vnode *d, struct vnode *e, caddr_t f, struct ucred *g) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_rename)(a, b, c, d, e, f, g);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_rmdir(struct vnode *a, struct vnode *b, char *c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_rmdir)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_lookup(struct vnode *a, struct vnode **b, char *c, int d,
	  struct vattr *v, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_lookup)(a, b, c, d, v, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_fid(struct vnode *a, struct fileid *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_fid)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_open(struct vnode *a, int b, int c, caddr_t *d, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_open)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_create(struct vnode *a, struct vnode **b, int c, caddr_t d
	  , int e, caddr_t *f, struct ucred *g) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_create)(a, b, c, d, e, f, g);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_hold(struct vnode *a) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_hold)(a);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_rele(struct vnode *a) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_rele)(a);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_close(struct vnode *a, int b, caddr_t c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_close)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_map(struct vnode *a, caddr_t b, uint c, uint d, uint e, struct ucred *f) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_map)(a, b, c, d, e, f);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_unmap(struct vnode *a, int b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_unmap)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_access(struct vnode *a, int b, int c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_access)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_getattr(struct vnode *a, struct vattr *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_getattr)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_setattr(struct vnode *a, int b, int c, int d, int e, struct ucred *f) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_setattr)(a, b, c, d, e, f);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_fclear(struct vnode *a, int b, offset_t c, offset_t d
	  , caddr_t e, struct ucred *f) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_fclear)(a, b, c, d, e, f);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_fsync(struct vnode *a, int b, int c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_fsync)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_ftrunc(struct vnode *a, int b, offset_t c, caddr_t d, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_ftrunc)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_rdwr(struct vnode *a, enum uio_rw b, int c, struct uio *d
	, int e, caddr_t f, struct vattr *v, struct ucred *g) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_rdwr)(a, b, c, d, e, f, v, g);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_lockctl(struct vnode *a, offset_t b, struct eflock *c, int d
	   , int (*e)(), ulong *f, struct ucred *g) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_lockctl)(a, b, c, d, e, f, g);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_ioctl(struct vnode *a, int b, caddr_t c, size_t d, int e, struct ucred *f) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_ioctl)(a, b, c, d, e, f);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_readlink(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_readlink)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_select(struct vnode *a, int b, ushort c, ushort *d, void (*e)()
	  , caddr_t f, struct ucred *g) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_select)(a, b, c, d, e, f, g);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_symlink(struct vnode *a, char *b, char *c, struct ucred *d) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_symlink)(a, b, c, d);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_readdir(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_readdir)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_revoke(struct vnode *a, int b, int c, struct vattr *d, struct ucred *e) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_revoke)(a, b, c, d, e);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_getacl(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_getacl)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_setacl(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_setacl)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_getpcl(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_getpcl)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

static
vn_setpcl(struct vnode *a, struct uio *b, struct ucred *c) {
	register glockOwner, ret;

	glockOwner = ISAFS_GLOCK();
	if (!glockOwner)
	    AFS_GLOCK();
	ret = (*afs_gn_vnodeops.vn_setpcl)(a, b, c);
	if (!glockOwner)
	    AFS_GUNLOCK();

	return ret;
}

extern int afs_gn_strategy();

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
	afs_gn_strategy,	/* no locking!!! (discovered the hard way) */
	vn_revoke,
	vn_getacl,
	vn_setacl,
	vn_getpcl,
	vn_setpcl,
};

struct gfs afs_gfs = {
	&locked_Afs_vfsops,
	&locked_afs_gn_vnodeops,
	AFS_MOUNT_AFS,
	"afs",
	Afs_init,
	GFS_VERSION4 | GFS_REMOTE,
	NULL
};

