/*
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/afs/DARWIN/osi_vfsops.c,v 1.11 2003/10/24 06:26:01 shadow Exp $");

#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include "../afs/sysctl.h"

struct vcache *afs_globalVp = 0;
struct mount *afs_globalVFS = 0;

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

int
afs_start(mp, flags, p)
     struct mount *mp;
     int flags;
     struct proc *p;
{
    return (0);			/* nothing to do. ? */
}

int
afs_mount(mp, path, data, ndp, p)
     register struct mount *mp;
     char *path;
     caddr_t data;
     struct nameidata *ndp;
     struct proc *p;
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    size_t size;
    int error;

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (data == NULL && afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = mp;
    mp->vfs_bsize = 8192;
    vfs_getnewfsid(mp);
    mp->mnt_stat.f_iosize = 8192;

    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
    memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);

    if (data == NULL) {
	strcpy(mp->mnt_stat.f_mntfromname, "AFS");
	/* null terminated string "AFS" will fit, just leave it be. */
	mp->mnt_data = (qaddr_t) NULL;
    } else {
	struct VenusFid *rootFid = NULL;
	struct volume *tvp;
	char volName[MNAMELEN];

	(void)copyinstr((char *)data, volName, MNAMELEN - 1, &size);
	memset(volName + size, 0, MNAMELEN - size);

	if (volName[0] == 0) {
	    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
	    mp->mnt_data = (qaddr_t) & afs_rootFid;
	} else {
	    struct cell *localcell = afs_GetPrimaryCell(READ_LOCK);
	    if (localcell == NULL) {
		AFS_GUNLOCK();
		return ENODEV;
	    }

	    /* Set the volume identifier to "AFS:volume.name" */
	    snprintf(mp->mnt_stat.f_mntfromname, MNAMELEN - 1, "AFS:%s",
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

	    mp->mnt_data = (qaddr_t) rootFid;
	}
    }
    strcpy(mp->mnt_stat.f_fstypename, "afs");
    AFS_GUNLOCK();
    (void)afs_statfs(mp, &mp->mnt_stat, p);
    return 0;
}

int
afs_unmount(mp, flags, p)
     struct mount *mp;
     int flags;
     struct proc *p;
{

    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    if (mp->mnt_data != (qaddr_t) - 1) {
	if (mp->mnt_data != NULL) {
	    FREE(mp->mnt_data, M_UFSMNT);
	    mp->mnt_data = (qaddr_t) - 1;
	} else {
	    if (flags & MNT_FORCE) {
		afs_globalVFS = 0;
		afs_shutdown();
	    } else {
		AFS_GUNLOCK();
		return EBUSY;
	    }
	}
	mp->mnt_flag &= ~MNT_LOCAL;
    }

    AFS_GUNLOCK();

    return 0;
}

int
afs_root(struct mount *mp, struct vnode **vpp)
{
    int error;
    struct vrequest treq;
    register struct vcache *tvp = 0;
    struct proc *p = current_proc();
    struct ucred cr;

    pcred_readlock(p);
    cr = *p->p_cred->pc_ucred;
    pcred_unlock(p);
    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    if (mp->mnt_data == NULL && afs_globalVp
	&& (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
	error = 0;
    } else if (mp->mnt_data == (qaddr_t) - 1) {
	error = ENOENT;
    } else {
	struct VenusFid *rootFid = (mp->mnt_data == NULL)
	    ? &afs_rootFid : (struct VenusFid *)mp->mnt_data;

	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(error = afs_InitReq(&treq, &cr)) && !(error = afs_CheckInit())) {
	    tvp = afs_GetVCache(rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp) {
		if (mp->mnt_data == NULL)
		    afs_globalVp = tvp;
	    } else
		error = ENOENT;
	}
    }
    if (tvp) {
	osi_vnhold(tvp, 0);
	AFS_GUNLOCK();
	vn_lock(AFSTOV(tvp), LK_EXCLUSIVE | LK_RETRY, p);
	AFS_GLOCK();
	if (mp->mnt_data == NULL) {
	    afs_globalVFS = mp;
	}
	*vpp = AFSTOV(tvp);
	AFSTOV(tvp)->v_flag |= VROOT;
	AFSTOV(tvp)->v_vfsp = mp;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *vpp,
	       ICL_TYPE_INT32, error);
    AFS_GUNLOCK();
    return error;
}

int
afs_vget(mp, lfl, vp)
     struct mount *mp;
     struct vnode *vp;
     int lfl;
{
    int error;
    printf("vget called. help!\n");
    if (vp->v_usecount < 0) {
	vprint("bad usecount", vp);
	panic("afs_vget");
    }
    error = vget(vp, lfl, current_proc());
    if (!error)
	insmntque(vp, mp);	/* take off free list */
    return error;
}

int
afs_statfs(struct mount *mp, struct statfs *abp, struct proc *p)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

#if 0
    abp->f_type = MOUNT_AFS;
#endif
    abp->f_bsize = mp->vfs_bsize;
    abp->f_iosize = mp->vfs_bsize;

    /* Fake a high number below to satisfy programs that use the statfs call
     * to make sure that there's enough space in the device partition before
     * storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 2000000;

    abp->f_fsid.val[0] = mp->mnt_stat.f_fsid.val[0];
    abp->f_fsid.val[1] = mp->mnt_stat.f_fsid.val[1];
    if (abp != &mp->mnt_stat) {
	abp->f_type = mp->mnt_vfc->vfc_typenum;
	memcpy((caddr_t) & abp->f_mntonname[0],
	       (caddr_t) mp->mnt_stat.f_mntonname, MNAMELEN);
	memcpy((caddr_t) & abp->f_mntfromname[0],
	       (caddr_t) mp->mnt_stat.f_mntfromname, MNAMELEN);
    }

    AFS_GUNLOCK();
    return 0;
}

int
afs_sync(mp, waitfor, cred, p)
     struct mount *mp;
     int waitfor;
     struct ucred *cred;
     struct prioc *p;
{
    return 0;
}

u_int32_t afs_darwin_realmodes = 0;

int afs_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, 
	       void *newp, size_t newlen, struct proc *p)
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
	        return sysctl_int(oldp, oldlenp, newp, newlen,
				  &afs_darwin_realmodes);
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
}

struct vfsops afs_vfsops = {
    afs_mount,
    afs_start,
    afs_unmount,
    afs_root,
    afs_quotactl,
    afs_statfs,
    afs_sync,
    afs_vget,
    afs_fhtovp,
    afs_vptofh,
    afs_init,
    afs_sysctl
};
