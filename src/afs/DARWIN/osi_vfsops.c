/*
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#ifndef AFS_DARWIN80_ENV
#include <sys/syscall.h>
#endif
#include <sys/sysctl.h>
#include "../afs/sysctl.h"

#ifndef M_UFSMNT
#define M_UFSMNT M_TEMP /* DARWIN80 MALLOC doesn't look at the type anyway */
#endif

struct vcache *afs_globalVp = 0;
struct mount *afs_globalVFS = 0;
int afs_vfs_typenum;

int
afs_quotactl()
{
    return EOPNOTSUPP;
}

int
afs_fhtovp(mp, fhp, vpp)
     struct mount *mp;
     struct fid *fhp;
     struct vnode **vpp;
{

    return (EINVAL);
}

int
afs_vptofh(vp, fhp)
     struct vnode *vp;
     struct fid *fhp;
{

    return (EINVAL);
}

#ifdef AFS_DARWIN80_ENV
#define CTX_TYPE vfs_context_t
#define CTX_PROC_CONVERT(C) vfs_context_proc((C))
#define STATFS_TYPE struct vfsstatfs
#else
#define CTX_TYPE struct proc *
#define CTX_PROC_CONVERT(C) (C)
#define STATFS_TYPE struct statfs
#define vfs_statfs(VFS) &(VFS)->mnt_stat
#endif
#define PROC_DECL(out,in) struct proc *out = CTX_PROC_CONVERT(in)

int
afs_start(mp, flags, p)
     struct mount *mp;
     int flags;
     CTX_TYPE p;
{
    return (0);			/* nothing to do. ? */
}

int
afs_statfs(struct mount *mp, STATFS_TYPE *abp, CTX_TYPE ctx);
#ifdef AFS_DARWIN80_ENV
int
afs_mount(mp, devvp, data, ctx)
     register struct mount *mp;
     vnode_t *devvp;
     user_addr_t data;
     vfs_context_t ctx;
#else
int
afs_mount(mp, path, data, ndp, p)
     register struct mount *mp;
     char *path;
     caddr_t data;
     struct nameidata *ndp;
     CTX_TYPE ctx;
#endif
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    size_t size;
    int error;
#ifdef AFS_DARWIN80_ENV
    struct vfsioattr ioattr;
    /* vfs_statfs advertised as RO, but isn't */
    /* new api will be needed to initialize this information (nfs needs to
       set mntfromname too) */
#endif
    STATFS_TYPE *mnt_stat = vfs_statfs(mp); 

    if (vfs_isupdate(mp))
	return EINVAL;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (data == 0 && afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = mp;
#ifdef AFS_DARWIN80_ENV
    vfs_ioattr(mp, &ioattr);
    ioattr.io_devblocksize = 8192;
    vfs_setioattr(mp, &ioattr);
    /* f_iosize is handled in VFS_GETATTR */
#else
    mp->vfs_bsize = 8192;
    mp->mnt_stat.f_iosize = 8192;
#endif
    vfs_getnewfsid(mp);

#ifndef AFS_DARWIN80_ENV
    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
#endif
    memset(mnt_stat->f_mntfromname, 0, MNAMELEN);

    if (data == 0) {
	strcpy(mnt_stat->f_mntfromname, "AFS");
	/* null terminated string "AFS" will fit, just leave it be. */
	vfs_setfsprivate(mp, NULL);
    } else {
	struct VenusFid *rootFid = NULL;
	struct volume *tvp;
	char volName[MNAMELEN];

	(void)copyinstr(data, volName, MNAMELEN - 1, &size);
	memset(volName + size, 0, MNAMELEN - size);

	if (volName[0] == 0) {
	    strcpy(mnt_stat->f_mntfromname, "AFS");
	    vfs_setfsprivate(mp, &afs_rootFid);
	} else {
	    struct cell *localcell = afs_GetPrimaryCell(READ_LOCK);
	    if (localcell == NULL) {
		AFS_GUNLOCK();
		return ENODEV;
	    }

	    /* Set the volume identifier to "AFS:volume.name" */
	    snprintf(mnt_stat->f_mntfromname, MNAMELEN - 1, "AFS:%s",
		     volName);
	    tvp =
		afs_GetVolumeByName(volName, localcell->cellNum, 1,
				    (struct vrequest *)0, READ_LOCK);

	    if (tvp) {
		int volid = (tvp->roVol ? tvp->roVol : tvp->volume);
		MALLOC(rootFid, struct VenusFid *, sizeof(*rootFid), M_UFSMNT,
		       M_WAITOK);
		rootFid->Cell = localcell->cellNum;
		rootFid->Fid.Volume = volid;
		rootFid->Fid.Vnode = 1;
		rootFid->Fid.Unique = 1;
	    } else {
		AFS_GUNLOCK();
		return ENODEV;
	    }

	    vfs_setfsprivate(mp, &rootFid);
	}
    }
#ifdef AFS_DARWIN80_ENV
    afs_vfs_typenum=vfs_typenum(mp);
    vfs_setauthopaque(mp);
    vfs_setauthopaqueaccess(mp);
#else
    strcpy(mp->mnt_stat.f_fstypename, "afs");
#endif
    AFS_GUNLOCK();
    (void)afs_statfs(mp, mnt_stat, ctx);
    return 0;
}

int
afs_unmount(mp, flags, ctx)
     struct mount *mp;
     int flags;
     CTX_TYPE ctx;
{
    void *mdata = vfs_fsprivate(mp);
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    if (mdata != (qaddr_t) - 1) {
	if (mdata != NULL) {
	    vfs_setfsprivate(mp, (qaddr_t) - 1);
	    FREE(mdata, M_UFSMNT);
	} else {
	    if (flags & MNT_FORCE) {
                if (afs_globalVp) {
#ifdef AFS_DARWIN80_ENV
                    afs_PutVCache(afs_globalVp);
#else
                    AFS_GUNLOCK();
                    vrele(AFSTOV(afs_globalVp));
                    AFS_GLOCK();
#endif
                }
		afs_globalVp = NULL;
		AFS_GUNLOCK();
	        vflush(mp, NULLVP, FORCECLOSE/*0*/);
		AFS_GLOCK();
		afs_globalVFS = 0;
		afs_shutdown();
	    } else {
		AFS_GUNLOCK();
		return EBUSY;
	    }
	}
	vfs_clearflags(mp, MNT_LOCAL);
    }

    AFS_GUNLOCK();

    return 0;
}

#ifdef AFS_DARWIN80_ENV
int
afs_root(struct mount *mp, struct vnode **vpp, vfs_context_t ctx)
#else
int
afs_root(struct mount *mp, struct vnode **vpp)
#endif
{
    void *mdata = vfs_fsprivate(mp);
    int error;
    struct vrequest treq;
    register struct vcache *tvp = 0;
#ifdef AFS_DARWIN80_ENV
    struct ucred *cr = vfs_context_ucred(ctx);
    int needref=0;
#else
    struct proc *p = current_proc();
    struct ucred _cr;
    struct ucred *cr =&_cr;

    pcred_readlock(p);
    cr = *p->p_cred->pc_ucred;
    pcred_unlock(p);
#endif
    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    if (mdata == NULL && afs_globalVp
	&& (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
	error = 0;
#ifdef AFS_DARWIN80_ENV
        needref=1;
#endif
    } else if (mdata == (qaddr_t) - 1) {
	error = ENOENT;
    } else {
	struct VenusFid *rootFid = (mdata == NULL)
	    ? &afs_rootFid : (struct VenusFid *)mdata;

	if (!(error = afs_InitReq(&treq, cr)) && !(error = afs_CheckInit())) {
	    tvp = afs_GetVCache(rootFid, &treq, NULL, NULL);
#ifdef AFS_DARWIN80_ENV
            if (tvp) {
	        AFS_GUNLOCK();
                error = afs_darwin_finalizevnode(tvp, NULL, NULL, 1);
	        AFS_GLOCK();
                if (error)
                   tvp = NULL;
                else 
                   /* re-acquire the usecount that finalizevnode disposed of */
                   vnode_ref(AFSTOV(tvp));
            }
#endif
	    /* we really want this to stay around */
	    if (tvp) {
		if (mdata == NULL) {
		    if (afs_globalVp) {
			afs_PutVCache(afs_globalVp);
			afs_globalVp = NULL;
		    }
		    afs_globalVp = tvp;
#ifdef AFS_DARWIN80_ENV
                    needref=1;
#endif
                }
	    } else
		error = ENOENT;
	}
    }
    if (tvp) {
#ifndef AFS_DARWIN80_ENV /* DARWIN80 caller does not need a usecount reference */
	osi_vnhold(tvp, 0);
	AFS_GUNLOCK();
	vn_lock(AFSTOV(tvp), LK_EXCLUSIVE | LK_RETRY, p);
	AFS_GLOCK();
#endif
#ifdef AFS_DARWIN80_ENV
        if (needref) /* this iocount is for the caller. the initial iocount
                        is for the eventual afs_PutVCache. for mdata != null,
                        there will not be a PutVCache, so the caller gets the
                        initial (from GetVCache or finalizevnode) iocount*/
           vnode_get(AFSTOV(tvp));
#endif
	if (mdata == NULL) {
	    afs_globalVFS = mp;
	}
	*vpp = AFSTOV(tvp);
#ifndef AFS_DARWIN80_ENV 
	AFSTOV(tvp)->v_flag |= VROOT;
	AFSTOV(tvp)->v_vfsp = mp;
#endif
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *vpp,
	       ICL_TYPE_INT32, error);
    AFS_GUNLOCK();
    return error;
}

#ifndef AFS_DARWIN80_ENV /* vget vfsop never had this prototype AFAIK */
int
afs_vget(mp, lfl, vp)
     struct mount *mp;
     struct vnode *vp;
     int lfl;
{
    int error;
    //printf("vget called. help!\n");
    if (vp->v_usecount < 0) {
	vprint("bad usecount", vp);
	panic("afs_vget");
    }
    error = vget(vp, lfl, current_proc());
    if (!error)
	insmntque(vp, mp);	/* take off free list */
    return error;
}

int afs_vfs_vget(struct mount *mp, void *ino, struct vnode **vpp)
{
   return ENOENT; /* cannot implement */
}

#endif

int
afs_statfs(struct mount *mp, STATFS_TYPE *abp, CTX_TYPE ctx)
{
    STATFS_TYPE *sysstat = vfs_statfs(mp);
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

#if 0
    abp->f_type = MOUNT_AFS;
#endif
#ifdef AFS_DARWIN80_ENV
    abp->f_bsize = abp->f_iosize = vfs_devblocksize(mp);
#else
    abp->f_bsize = mp->vfs_bsize;
    abp->f_iosize = mp->vfs_bsize;
#endif

    /* Fake a high number below to satisfy programs that use the statfs call
     * to make sure that there's enough space in the device partition before
     * storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 2000000;

    if (abp != sysstat) {
        abp->f_fsid.val[0] = sysstat->f_fsid.val[0];
        abp->f_fsid.val[1] = sysstat->f_fsid.val[1];
#ifndef AFS_DARWIN80_ENV
	abp->f_type = vfs_typenum(mp);
#endif
	memcpy((caddr_t) & abp->f_mntonname[0],
	       (caddr_t) sysstat->f_mntonname, MNAMELEN);
	memcpy((caddr_t) & abp->f_mntfromname[0],
	       (caddr_t) sysstat->f_mntfromname, MNAMELEN);
    }

    AFS_GUNLOCK();
    return 0;
}

#ifdef AFS_DARWIN80_ENV
int
afs_vfs_getattr(struct mount *mp, struct vfs_attr *outattrs,
                vfs_context_t context)
{
    VFSATTR_RETURN(outattrs, f_bsize, vfs_devblocksize(mp));
    VFSATTR_RETURN(outattrs, f_iosize, vfs_devblocksize(mp));
    VFSATTR_RETURN(outattrs, f_blocks, 2000000);
    VFSATTR_RETURN(outattrs, f_bfree, 2000000);
    VFSATTR_RETURN(outattrs, f_bavail, 2000000);
    VFSATTR_RETURN(outattrs, f_files, 2000000);
    VFSATTR_RETURN(outattrs, f_ffree, 2000000);
    if ( VFSATTR_IS_ACTIVE(outattrs, f_capabilities) )
    {
         vol_capabilities_attr_t *vcapattrptr;
         vcapattrptr = &outattrs->f_capabilities;
         vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] =
                   VOL_CAP_FMT_SYMBOLICLINKS |
                   VOL_CAP_FMT_HARDLINKS |
                   VOL_CAP_FMT_ZERO_RUNS |
                   VOL_CAP_FMT_CASE_SENSITIVE |
                   VOL_CAP_FMT_CASE_PRESERVING |
                   VOL_CAP_FMT_FAST_STATFS;
         vcapattrptr->capabilities[VOL_CAPABILITIES_INTERFACES] = 
                   VOL_CAP_INT_ADVLOCK | 
                   VOL_CAP_INT_FLOCK;
         vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
         vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

         /* Capabilities we know about: */
         vcapattrptr->valid[VOL_CAPABILITIES_FORMAT] =
                 VOL_CAP_FMT_PERSISTENTOBJECTIDS |
                 VOL_CAP_FMT_SYMBOLICLINKS |
                 VOL_CAP_FMT_HARDLINKS |
                 VOL_CAP_FMT_JOURNAL |
                 VOL_CAP_FMT_JOURNAL_ACTIVE |
                 VOL_CAP_FMT_NO_ROOT_TIMES |
                 VOL_CAP_FMT_SPARSE_FILES |
                 VOL_CAP_FMT_ZERO_RUNS |
                 VOL_CAP_FMT_CASE_SENSITIVE |
                 VOL_CAP_FMT_CASE_PRESERVING |
                 VOL_CAP_FMT_FAST_STATFS;
         vcapattrptr->valid[VOL_CAPABILITIES_INTERFACES] =
                 VOL_CAP_INT_SEARCHFS |
                 VOL_CAP_INT_ATTRLIST |
                 VOL_CAP_INT_NFSEXPORT |
                 VOL_CAP_INT_READDIRATTR |
                 VOL_CAP_INT_EXCHANGEDATA |
                 VOL_CAP_INT_COPYFILE |
                 VOL_CAP_INT_ALLOCATE |
                 VOL_CAP_INT_VOL_RENAME |
                 VOL_CAP_INT_ADVLOCK |
                 VOL_CAP_INT_FLOCK;
         vcapattrptr->valid[VOL_CAPABILITIES_RESERVED1] = 0;
         vcapattrptr->valid[VOL_CAPABILITIES_RESERVED2] = 0;
             
         VFSATTR_SET_SUPPORTED(outattrs, f_capabilities);
    }
    return 0;
}
#endif

#ifdef AFS_DARWIN80_ENV
int
afs_sync(mp, waitfor, ctx)
     struct mount *mp;
     int waitfor;
     CTX_TYPE ctx;
#else
int
afs_sync(mp, waitfor, cred, p)
     struct mount *mp;
     int waitfor;
     struct ucred *cred;
     struct proc *p;
#endif
{
    return 0;
}

u_int32_t afs_darwin_realmodes = 0;

#ifdef AFS_DARWIN80_ENV
int afs_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp, 
	       user_addr_t newp, size_t newlen, vfs_context_t context)
#else
int afs_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, 
	       void *newp, size_t newlen, struct proc *p)
#endif
{
    int error;

    switch (name[0]) {
    case AFS_SC_ALL:
        /* nothing defined */
        break;
    case AFS_SC_DARWIN:
        if (namelen < 3)
	    return ENOENT;
	switch (name[1]) {
	case AFS_SC_DARWIN_ALL:
	    switch (name[2]) {
	    case AFS_SC_DARWIN_ALL_REALMODES:
#ifdef AFS_DARWIN80_ENV
               newlen;
               /* XXX complicated */
#else
	        return sysctl_int(oldp, oldlenp, newp, newlen,
				  &afs_darwin_realmodes);
#endif
	    }
	    break;
	    /* darwin version specific sysctl's goes here */
	}
	break;
    }
    return EOPNOTSUPP;
}

typedef (*PFI) ();
extern int vfs_opv_numops;	/* The total number of defined vnode operations */
extern struct vnodeopv_desc afs_vnodeop_opv_desc;
int
afs_init(struct vfsconf *vfc)
{
#ifndef AFS_DARWIN80_ENV /* vfs_fsadd does all this junk */
    int j;
    int (**opv_desc_vector) ();
    struct vnodeopv_entry_desc *opve_descp;



    MALLOC(afs_vnodeop_p, PFI *, vfs_opv_numops * sizeof(PFI), M_TEMP,
	   M_WAITOK);

    memset(afs_vnodeop_p, 0, vfs_opv_numops * sizeof(PFI));

    opv_desc_vector = afs_vnodeop_p;
    for (j = 0; afs_vnodeop_opv_desc.opv_desc_ops[j].opve_op; j++) {
	opve_descp = &(afs_vnodeop_opv_desc.opv_desc_ops[j]);

	/*
	 * Sanity check:  is this operation listed
	 * in the list of operations?  We check this
	 * by seeing if its offest is zero.  Since
	 * the default routine should always be listed
	 * first, it should be the only one with a zero
	 * offset.  Any other operation with a zero
	 * offset is probably not listed in
	 * vfs_op_descs, and so is probably an error.
	 *
	 * A panic here means the layer programmer
	 * has committed the all-too common bug
	 * of adding a new operation to the layer's
	 * list of vnode operations but
	 * not adding the operation to the system-wide
	 * list of supported operations.
	 */
	if (opve_descp->opve_op->vdesc_offset == 0
	    && opve_descp->opve_op->vdesc_offset != VOFFSET(vop_default)) {
	    printf("afs_init: operation %s not listed in %s.\n",
		   opve_descp->opve_op->vdesc_name, "vfs_op_descs");
	    panic("load_afs: bad operation");
	}
	/*
	 * Fill in this entry.
	 */
	opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
	    opve_descp->opve_impl;
    }

    /*
     * Finally, go back and replace unfilled routines
     * with their default.  (Sigh, an O(n^3) algorithm.  I
     * could make it better, but that'd be work, and n is small.)
     */

    /*
     * Force every operations vector to have a default routine.
     */
    opv_desc_vector = afs_vnodeop_p;
    if (opv_desc_vector[VOFFSET(vop_default)] == NULL) {
	panic("afs_init: operation vector without default routine.");
    }
    for (j = 0; j < vfs_opv_numops; j++)
	if (opv_desc_vector[j] == NULL)
	    opv_desc_vector[j] = opv_desc_vector[VOFFSET(vop_default)];
#endif
    return 0;
}

struct vfsops afs_vfsops = {
   afs_mount,
   afs_start,
   afs_unmount,
   afs_root,
#ifdef AFS_DARWIN80_ENV
   0,
   afs_vfs_getattr,
#else
   afs_quotactl,
   afs_statfs,
#endif
   afs_sync,
#ifdef AFS_DARWIN80_ENV
   0,0,0,
#else
   afs_vfs_vget,
   afs_fhtovp,
   afs_vptofh,
#endif
   afs_init,
   afs_sysctl, 
#ifdef AFS_DARWIN80_ENV
   0 /*setattr */,
   {0}
#endif
};
