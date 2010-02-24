/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * IRIX inode operations
 *
 * Implements:
 * afsidestroy
 * getinode
 * igetinode
 * xfs_getinode
 * xfs_igetinode
 * icreate
 * afs_syscall_icreate
 * xfs_icreatename64
 * afs_syscall_icreatename64
 * iopenargs64
 * afs_syscall_iopen
 * iopen
 * iopen64
 * xfs_iincdec64
 * iincdec64
 * afs_syscall_idec64
 * afs_syscall_iinc64
 * iincdec
 * iinc
 * idec
 * afs_syscall_iincdec
 * afs_syscall_ilistinode64
 *
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */

#define BAD_IGET	-1000

/*
 * SGI dependent system calls
 */
#ifndef INODESPECIAL
/*
 * `INODESPECIAL' type inodes are ones that describe volumes.
 */
#define INODESPECIAL	0xffffffff	/* ... from ../vol/viceinode.h  */
#endif

void
afsidestroy(struct inode *ip)
{
    if (ip->i_afs) {
	kmem_free(ip->i_afs, sizeof(struct afsparms));
	ip->i_afs = NULL;
    }
}

extern int xfs_fstype;

int
getinode(struct vfs *vfsp, dev_t dev, ino_t inode, struct inode **ipp)
{
    return ENOSYS;
}

int
igetinode(struct vfs *vfsp, dev_t dev, ino_t inode, struct inode **ipp)
{
    return ENOSYS;
}

int XFS_IGET_EPOS;
ino_t XFS_IGET_INO;
dev_t XFS_IGET_DEV;
#define SET_XFS_ERROR(POS, DEV, INO) \
	XFS_IGET_EPOS = (POS), XFS_IGET_DEV = (DEV), XFS_IGET_INO = (INO)

int
xfs_getinode(struct vfs *vfsp, dev_t dev, ino_t inode, struct xfs_inode **ipp)
{
    struct xfs_inode *ip;
    int error;

    if (!vfsp) {
	vfsp = vfs_devsearch(dev, xfs_fstype);
	if (!vfsp) {
	    SET_XFS_ERROR(1, dev, inode);
	    return ENXIO;
	}
    }

    if (error = xfs_iget((((struct mount *)
			   ((vfsp)->vfs_bh.bh_first)->bd_pdata)), (void *)0,
			 (xfs_ino_t) inode, XFS_ILOCK_SHARED, &ip,
			 (daddr_t) 0)) {
	SET_XFS_ERROR(3, vfsp->vfs_dev, inode);
	return error;
    }

    *ipp = ip;
    return 0;
}

/* xfs_igetinode now returns an unlocked inode. This is fine, since we
 * have a refcount on the holding vnode.
 */
int
xfs_igetinode(struct vfs *vfsp, dev_t dev, ino_t inode,
	      struct xfs_inode **ipp)
{
    struct xfs_inode *ip;
    vnode_t *vp;
    vattr_t vattr;
    int error;

    AFS_STATCNT(igetinode);

    *ipp = NULL;
    if (error = xfs_getinode(vfsp, dev, inode, &ip)) {
	return error;
    }

    xfs_iunlock(ip, XFS_ILOCK_SHARED);
    vp = XFS_ITOV(ip);
    vattr.va_mask = AT_STAT;
    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), error);
    if (error) {
	SET_XFS_ERROR(4, vp->v_vfsp->vfs_dev, inode);
	VN_RELE(vp);
	return error;
    }
    if (vattr.va_nlink == 0 || vattr.va_type != VREG) {
	SET_XFS_ERROR(5, vp->v_vfsp->vfs_dev, inode);
	VN_RELE(vp);
	return ENOENT;
    }

    *ipp = ip;
    return 0;
}

/**************************************************************************
 * inode creation routines.
 *
 ***************************************************************************/
struct icreateargs {
    sysarg_t dev;
    sysarg_t near_inode;
    sysarg_t param1;
    sysarg_t param2;
    sysarg_t param3;
    sysarg_t param4;
};

int
icreate(struct icreateargs *uap, rval_t * rvp)
{
    return ENOSYS;
}

int
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, rvp)
     afs_uint32 dev, near_inode, param1, param2, param3, param4;
     rval_t *rvp;
{
    return ENOSYS;
}

/* inode creation routines for icreatename64 entry point. 
 * Create a name in the namespace as well as the inode.
 */

#include <afs/xfsattrs.h>
#include <sys/attributes.h>

extern char *int_to_base64(char *, int);

/* Lock against races creating/removing directory - vos zap RO, vos create RW*/
kmutex_t afs_vol_create_lock;
int afs_vol_create_lock_inited = 0;
#define AFS_LOCK_VOL_CREATE() { \
	if (!afs_vol_create_lock_inited) { \
	    mutex_init(&afs_vol_create_lock, MUTEX_DEFAULT, \
		       "afs_vol_create_lock"); \
	    afs_vol_create_lock_inited = 1; \
	} \
        mutex_enter(&afs_vol_create_lock); \
				 }
#define AFS_UNLOCK_VOL_CREATE() mutex_exit(&afs_vol_create_lock)


/* xfs_icreatename64
 * Create an AFS inode in the XFS name space. If required create the proper
 * containing directory. See sys/xfsattrs.h for the details on the naming
 * conventions and the usage of file and directory attributes.
 *
 * The inode parameters are stored in an XFS attribute called "AFS". In
 * addition gid is set to XFS_VICEMAGIC and uid is set to the low 31 bits
 * of the RW volume id. This is so inode verification in iinc and idec
 * don't need to get the attribute. Note that only the low 31 bits are set.
 * This is because chmod only accepts up to MAX_UID and chmod is used
 * to correct these values in xfs_ListViceInodes.
 */
int
xfs_icreatename64(struct vfs *vfsp, int datap, int datalen,
		  afs_inode_params_t params, ino_t * inop)
{
#define AFS_PNAME_SIZE 16
    char path[64];
    char name[64];
    b64_string_t stmp1, stmp2;
    afs_xfs_attr_t attrs;
    struct vattr vattr;
    int name_version = AFS_XFS_NAME_VERS;
    int code = 0, unused;
    struct vnode *vp;
    struct vnode *dvp;
    int rw_vno;			/* volume ID of parent volume */
    int i;
    int createdDir = 0;
    size_t junk;
    char *s;


    /* Get vnode for directory which will contain new inode. */
    if (datalen >= AFS_PNAME_SIZE)
	return E2BIG;

    AFS_COPYINSTR((char *)datap, path, AFS_PNAME_SIZE - 1, &junk, unused);
    if (*path != '/') {
	return EINVAL;
    }

    rw_vno = (params[1] == INODESPECIAL) ? params[3] : params[0];

    /* directory name */
    strcat(path, "/");
    strcat(path, AFS_INODE_DIR_NAME);
    strcat(path, int_to_base64(stmp1, rw_vno));

    if (params[1] == INODESPECIAL)
	AFS_LOCK_VOL_CREATE();

    code = gop_lookupname(path, AFS_UIOSYS, FOLLOW, &dvp);
    if (code == ENOENT) {
	/* Maybe it's an old directory name format. */
	AFS_COPYINSTR((char *)datap, name, AFS_PNAME_SIZE - 1, &junk, unused);
	strcat(name, "/.");
	strcat(name, int_to_base64(stmp1, rw_vno));
	code = gop_lookupname(name, AFS_UIOSYS, FOLLOW, &dvp);
	if (!code) {
	    /* Use old name format. */
	    strcpy(path, name);
	    name_version = AFS_XFS_NAME_VERS1;
	}
    }

    if (code == ENOENT) {
	afs_xfs_dattr_t dattr;
	/* make directory. */

	code =
	    AFS_VN_OPEN(path, UIO_SYSSPACE, FCREAT | FEXCL, 0700, &dvp,
			CRMKDIR);
	if (code) {
	    if (code == EEXIST) {
		/* someone beat us to it? */
		code = gop_lookupname(path, AFS_UIOSYS, NO_FOLLOW, &dvp);
	    }
	    if (code) {
		AFS_UNLOCK_VOL_CREATE();
		return code;
	    }
	} else
	    createdDir = 1;
	memset(&dattr, 0, sizeof(dattr));
	dattr.atd_version = AFS_XFS_ATD_VERS;
	dattr.atd_volume = rw_vno;
	AFS_VOP_ATTR_SET(dvp, AFS_XFS_DATTR, (char *)&dattr,
			 SIZEOF_XFS_DATTR_T, ATTR_ROOT | ATTR_CREATE,
			 OSI_GET_CURRENT_CRED(), code);
	if (code) {
	    VN_RELE(dvp);
	    if (createdDir)
		(void)vn_remove(path, UIO_SYSSPACE, RMDIRECTORY);
	    AFS_UNLOCK_VOL_CREATE();
	    return code;
	}
    }

    vattr.va_mask = AT_FSID | AT_NODEID;	/* gets a guick return using FSID */
    AFS_VOP_GETATTR(dvp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    if (code) {
	VN_RELE(dvp);
	return code;
    }

    memset(&attrs, 0, sizeof(attrs));
    attrs.at_pino = vattr.va_nodeid;
    VN_RELE(dvp);

    /* Create the desired file. Use up to ten tries to create a unique name. */
    (void)strcpy(name, path);
    (void)strcat(name, "/.");
    (void)strcat(name, int_to_base64(stmp2, params[2]));
    s = &name[strlen(name)];

    attrs.at_tag = 0;		/* Initial guess at a unique tag. */
    for (i = 0; i < 10; i++) {
	*s = '\0';
	strcat(s, ".");
	strcat(s, int_to_base64(stmp1, attrs.at_tag));
	code =
	    AFS_VN_OPEN(name, UIO_SYSSPACE, FCREAT | FEXCL, 0600, &vp,
			CRCREAT);
	if (!code || code != EEXIST)
	    break;

	attrs.at_tag++;
    }
    /* Unlock the creation process since the directory now has a file in it. */
    if (params[1] == INODESPECIAL)
	AFS_UNLOCK_VOL_CREATE();

    if (!code) {
	/* Set attributes. */
	memcpy((char *)attrs.at_param, (char *)params,
	       sizeof(afs_inode_params_t));
	attrs.at_attr_version = AFS_XFS_ATTR_VERS;
	attrs.at_name_version = name_version;
	AFS_VOP_ATTR_SET(vp, AFS_XFS_ATTR, (char *)&attrs, SIZEOF_XFS_ATTR_T,
			 ATTR_ROOT | ATTR_CREATE, OSI_GET_CURRENT_CRED(),
			 code);
	if (!code) {
	    vattr.va_mode = 1;
	    vattr.va_uid = AFS_XFS_VNO_CLIP(params[0]);
	    vattr.va_gid = XFS_VICEMAGIC;
	    vattr.va_mask = AT_MODE | AT_UID | AT_GID;
	    AFS_VOP_SETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
	}
	if (!code) {
	    vattr.va_mask = AT_NODEID;
	    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
	}
	if (!code)
	    *inop = vattr.va_nodeid;
	VN_RELE(vp);
    }

    if (code) {
	/* remove partially created file. */
	(void)vn_remove(name, UIO_SYSSPACE, RMFILE);

	/* and directory if volume special file. */
	if (createdDir) {
	    AFS_LOCK_VOL_CREATE();
	    (void)vn_remove(path, UIO_SYSSPACE, RMDIRECTORY);
	    AFS_UNLOCK_VOL_CREATE();
	}
    }
    return code;
}

/* afs_syscall_icreatename64
 * This is the icreatename64 entry point used by the XFS
 * fileserver suite.
 */
int
afs_syscall_icreatename64(int dev, int datap, int datalen, int paramp,
			  int inop)
{
    struct vfs *vfsp;
    afs_inode_params_t param;
    int code;
    rval_t rval;
    ino_t ino;


    if (!afs_suser(NULL))
	return EPERM;

    vfsp = vfs_devsearch(dev, VFS_FSTYPE_ANY);
    if (vfsp == NULL) {
	return ENXIO;
    }

    AFS_COPYIN((char *)paramp, (char *)param, sizeof(afs_inode_params_t),
	       code);
    if (vfsp->vfs_fstype == xfs_fstype) {
	code = xfs_icreatename64(vfsp, datap, datalen, param, &ino);
	if (code)
	    return code;
	else {
	    AFS_COPYOUT((char *)&ino, (char *)inop, sizeof(ino_t), code);
	    return code;
	}
    }
    return ENXIO;
}

/*
 * iopen system calls -- open an inode for reading/writing
 * Restricted to super user.
 * Any IFREG files.
 */
struct iopenargs {
    sysarg_t dev;
    sysarg_t inode;
    sysarg_t usrmod;
};

struct iopenargs64 {
    sysarg_t dev;
    sysarg_t inode_hi;
    sysarg_t inode_lo;
    sysarg_t usrmod;
};

int
afs_syscall_iopen(int dev, ino_t inode, int usrmod, rval_t * rvp)
{
    struct file *fp;
    int fd;
    int error;
    struct vfs *vfsp;
    struct vnode *vp;

    AFS_STATCNT(afs_syscall_iopen);
    if (!afs_suser(NULL))
	return EPERM;
    vfsp = vfs_devsearch(dev, xfs_fstype);
    if (!vfsp)
	return ENXIO;

    if (vfsp->vfs_fstype == xfs_fstype) {
	struct xfs_inode *xip;
	if (error = xfs_igetinode(vfsp, (dev_t) dev, inode, &xip))
	    return error;
	vp = XFS_ITOV(xip);
	if (error = vfile_alloc((usrmod + 1) & (FMASK), &fp, &fd)) {
	    VN_RELE(vp);
	    return error;
	}
    } else {
	osi_Panic("afs_syscall_iopen: bad fstype = %d\n", vfsp->vfs_fstype);
    }
    vfile_ready(fp, vp);
    rvp->r_val1 = fd;
    return 0;
}

int
iopen(struct iopenargs *uap, rval_t * rvp)
{
    AFS_STATCNT(iopen);
    return (afs_syscall_iopen
	    (uap->dev, (ino_t) uap->inode, uap->usrmod, rvp));
}

int
iopen64(struct iopenargs64 *uap, rval_t * rvp)
{
    AFS_STATCNT(iopen);
    return (afs_syscall_iopen
	    (uap->dev, (ino_t) ((uap->inode_hi << 32) | uap->inode_lo),
	     uap->usrmod, rvp));
}

/*
 * Support for iinc() and idec() system calls--increment or decrement
 * count on inode.
 * Restricted to super user.
 * Only VICEMAGIC type inodes.
 */

/* xfs_iincdec
 *
 * XFS iinc/idec code. Uses 64 bit inode numbers. 
 */
static int
xfs_iincdec64(struct vfs *vfsp, ino_t inode, int inode_p1, int amount)
{
    vnode_t *vp;
    xfs_inode_t *ip;
    int code = 0;
    afs_xfs_attr_t attrs;
    int length = SIZEOF_XFS_ATTR_T;
    afs_xfs_dattr_t dattr;
    struct vattr vattr;
    int nlink;
    int vol;

    code =
	xfs_iget((((struct mount *)((vfsp)->vfs_bh.bh_first)->bd_pdata)),
		 (void *)0, (xfs_ino_t) inode, XFS_ILOCK_SHARED, &ip,
		 (daddr_t) 0);
    if (code)
	return code;

    vp = XFS_ITOV(ip);
    xfs_iunlock(ip, XFS_ILOCK_SHARED);

    vattr.va_mask = AT_GID | AT_UID | AT_MODE;
    AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    if (code)
	code = EPERM;

    if (!code && (vattr.va_gid != XFS_VICEMAGIC))
	code = EPERM;

    if (!code && (AFS_XFS_VNO_CLIP(inode_p1) != vattr.va_uid))
	code = ENXIO;

    if (code) {
	VN_RELE(vp);
	return code;
    }

    nlink = vattr.va_mode & AFS_XFS_MODE_LINK_MASK;
    nlink += amount;
    if (nlink > 07) {
	code = EOVERFLOW;
    }
    if (nlink > 0) {
	vattr.va_mode &= ~AFS_XFS_MODE_LINK_MASK;
	vattr.va_mode |= nlink;
	vattr.va_mask = AT_MODE;
	AFS_VOP_SETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
	VN_RELE(vp);
	return code;
    } else {
	char path[64];
	b64_string_t stmp1, stmp2;
	vnode_t *dvp;
	xfs_inode_t *ip;

	length = SIZEOF_XFS_ATTR_T;
	AFS_VOP_ATTR_GET(vp, AFS_XFS_ATTR, (char *)&attrs, &length, ATTR_ROOT,
			 OSI_GET_CURRENT_CRED(), code);
	VN_RELE(vp);
	if (!code) {
	    if (length != SIZEOF_XFS_ATTR_T
		|| attrs.at_attr_version != AFS_XFS_ATTR_VERS)
		return EINVAL;
	}
	/* Get the vnode for the directory this file is in. */
	if (!attrs.at_pino)
	    return ENOENT;

	code = xfs_getinode(vp->v_vfsp, NULL, attrs.at_pino, &ip);
	if (code)
	    return code;

	dvp = XFS_ITOV(ip);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	/* Verify directory attributes. */
	length = SIZEOF_XFS_DATTR_T;
	AFS_VOP_ATTR_GET(dvp, AFS_XFS_DATTR, (char *)&dattr, &length,
			 ATTR_ROOT, OSI_GET_CURRENT_CRED(), code);
	if (!code) {
	    if (length != SIZEOF_XFS_DATTR_T
		|| dattr.atd_version != AFS_XFS_ATD_VERS)
		code = ENXIO;
	}
	if (code) {
	    VN_RELE(dvp);
	    return code;
	}

	strcpy(path, ".");
	strcat(path, int_to_base64(stmp1, attrs.at_param[2]));
	strcat(path, ".");
	strcat(path, int_to_base64(stmp1, attrs.at_tag));

	AFS_VOP_REMOVE(dvp, path, OSI_GET_CURRENT_CRED(), code);

	if (!code) {
	    int code2;
	    vattr.va_mask = AT_NLINK;
	    AFS_VOP_GETATTR(dvp, &vattr, 0, OSI_GET_CURRENT_CRED(), code2);
	    if (!code2) {
		if (vattr.va_nlink == 2) {
		    vnode_t *ddvp;	/* parent of volume directory. */
		    /* Try to remove the directory if this is a volume
		     * special file. It's ok to fail.
		     */
		    AFS_VOP_LOOKUP(dvp, "..", &ddvp, (struct pathname *)NULL,
				   0, OSI_GET_CURRENT_RDIR(),
				   OSI_GET_CURRENT_CRED(), code2);
		    if (!code2) {
			VN_RELE(dvp);
			dvp = (vnode_t *) 0;
			strcpy(path, ".");
			if (attrs.at_name_version == AFS_XFS_NAME_VERS2)
			    strcpy(path, AFS_INODE_DIR_NAME);
			else
			    strcpy(path, ".");
			int_to_base64(stmp1,
				      (attrs.at_param[1] ==
				       INODESPECIAL) ? attrs.
				      at_param[3] : attrs.at_param[0]);
			strcat(path, stmp1);
			AFS_LOCK_VOL_CREATE();
			AFS_VOP_RMDIR(ddvp, path, OSI_GET_CURRENT_CDIR(),
				      OSI_GET_CURRENT_CRED(), code2);
			AFS_UNLOCK_VOL_CREATE();
			VN_RELE(ddvp);
		    }
		}
	    }
	}
	if (dvp)
	    VN_RELE(dvp);
    }
    return code;
}

int
iincdec64(int dev, int inode_hi, int inode_lo, int inode_p1, int amount)
{
    struct vfs *vfsp;

    if (!afs_suser(NULL))
	return EPERM;
    vfsp = vfs_devsearch(dev, VFS_FSTYPE_ANY);
    if (!vfsp) {
	return ENXIO;
    }

    if (vfsp->vfs_fstype == xfs_fstype) {
	ino_t inode;
	inode = inode_hi;
	inode <<= 32;
	inode |= inode_lo;
	return xfs_iincdec64(vfsp, inode, inode_p1, amount);
    }
    return ENXIO;
}

int
afs_syscall_idec64(int dev, int inode_hi, int inode_lo, int inode_p1)
{
    return iincdec64(dev, inode_hi, inode_lo, inode_p1, -1);
}

int
afs_syscall_iinc64(int dev, int inode_hi, int inode_lo, int inode_p1)
{
    return iincdec64(dev, inode_hi, inode_lo, inode_p1, 1);
}

struct iincargs {
    sysarg_t dev;
    sysarg_t inode;
    sysarg_t inode_p1;
};

int
iinc(struct iincargs *uap, rval_t * rvp)
{
    AFS_STATCNT(iinc);
    return ENOTSUP;
}

int
idec(struct iincargs *uap, rval_t * rvp)
{
    AFS_STATCNT(idec);
    return ENOTSUP;
}

/* afs_syscall_ilistinode64
 * Gathers up all required info for ListViceInodes in one system call.
 */
int
afs_syscall_ilistinode64(int dev, int inode_hi, int inode_lo, int datap,
			 int datalenp)
{
    int code = 0;
    ino_t inode;
    xfs_inode_t *ip;
    vfs_t *vfsp;
    vnode_t *vp;
    struct vattr vattr;
    afs_xfs_attr_t attrs;
    int length;
    i_list_inode_t data;
    int idatalen;

    if (!afs_suser(NULL))
	return EPERM;
    vfsp = vfs_devsearch(dev, xfs_fstype);
    if (!vfsp) {
	return ENXIO;
    }

    AFS_COPYIN((char *)datalenp, &idatalen, sizeof(int), code);
    if (idatalen < sizeof(i_list_inode_t)) {
	idatalen = sizeof(i_list_inode_t);
	AFS_COPYOUT((char *)datalenp, (char *)&idatalen, sizeof(int), code);
	return E2BIG;
    }
    idatalen = sizeof(i_list_inode_t);
    AFS_COPYOUT((char *)datalenp, (char *)&idatalen, sizeof(int), code);

    AFS_COPYIN((char *)datap, (char *)&data, sizeof(i_list_inode_t), code);
    if (data.ili_version != AFS_XFS_ILI_VERSION) {
	data.ili_version = AFS_XFS_ILI_VERSION;
	AFS_COPYOUT((char *)&data, (char *)datap, sizeof(i_list_inode_t),
		    code);
	return EINVAL;
    }


    inode = inode_hi;
    inode <<= 32;
    inode |= inode_lo;
    code =
	xfs_iget((((struct mount *)((vfsp)->vfs_bh.bh_first)->bd_pdata)),
		 (void *)0, (xfs_ino_t) inode, XFS_ILOCK_SHARED, &ip,
		 (daddr_t) 0);
    if (code)
	return code;

    vp = XFS_ITOV(ip);
    xfs_iunlock(ip, XFS_ILOCK_SHARED);

    length = SIZEOF_XFS_ATTR_T;

    AFS_VOP_ATTR_GET(vp, AFS_XFS_ATTR, (char *)&attrs, &length, ATTR_ROOT,
		     OSI_GET_CURRENT_CRED(), code);
    if (code) {
	code = EPERM;
    }

    if (!code) {
	if (attrs.at_attr_version != AFS_XFS_ATTR_VERS)
	    code = EINVAL;
    }

    if (!code) {
	vattr.va_mask = AT_STAT;
	AFS_VOP_GETATTR(vp, &vattr, 0, OSI_GET_CURRENT_CRED(), code);
    }

    if (!code) {
	memset(&data, 0, sizeof(data));
	data.ili_info.inodeNumber = inode;
	data.ili_info.byteCount = vattr.va_size;
	data.ili_info.linkCount = (vattr.va_mode & AFS_XFS_MODE_LINK_MASK);
	memcpy((char *)data.ili_info.param, (char *)attrs.at_param,
	       sizeof(data.ili_info.param));
	data.ili_attr_version = attrs.at_attr_version;
	data.ili_name_version = attrs.at_name_version;
	data.ili_tag = attrs.at_tag;
	data.ili_pino = attrs.at_pino;
	data.ili_vno = vattr.va_uid;
	data.ili_magic = vattr.va_gid;
	AFS_COPYOUT((char *)&data, (char *)datap, sizeof(data), code);
    }
    VN_RELE(vp);
    return code;
}
