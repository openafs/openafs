/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * VFS operations for Linux
 *
 * super_block operations should return negated errno to Linux.
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#define __NO_VERSION__		/* don't define kernel_version in module.h */
#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"
#if !defined(AFS_LINUX26_ENV)
#include "h/locks.h"
#endif
#if defined(AFS_LINUX24_ENV)
#include "h/smp_lock.h"
#endif


struct vcache *afs_globalVp = 0;
struct vfs *afs_globalVFS = 0;
#if defined(AFS_LINUX24_ENV)
struct vfsmount *afs_cacheMnt;
#endif
int afs_was_mounted = 0;	/* Used to force reload if mount/unmount/mount */

extern struct super_operations afs_sops;
extern afs_rwlock_t afs_xvcache;
extern struct afs_q VLRU;

extern struct dentry_operations afs_dentry_operations;

/* Forward declarations */
static void iattr2vattr(struct vattr *vattrp, struct iattr *iattrp);
static void update_inode_cache(struct inode *ip, struct vattr *vp);
static int afs_root(struct super_block *afsp);
struct super_block *afs_read_super(struct super_block *sb, void *data, int silent);
int afs_fill_super(struct super_block *sb, void *data, int silent);
static struct super_block *afs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);

/* afs_file_system
 * VFS entry for Linux - installed in init_module
 * Linux mounts file systems by:
 * 1) register_filesystem(&afs_file_system) - done in init_module
 * 2) Mount call comes to us via do_mount -> read_super -> afs_read_super.
 *    We are expected to setup the super_block. See afs_read_super.
 */
#if defined(AFS_LINUX26_ENV)
struct backing_dev_info afs_backing_dev_info = {
	.ra_pages	= (VM_MAX_READAHEAD * 1024) / PAGE_CACHE_SIZE,
	.state		= 0,
};

struct file_system_type afs_fs_type = {
    .owner = THIS_MODULE,
    .name = "afs",
    .get_sb = afs_get_sb,
    .kill_sb = kill_anon_super,
    .fs_flags = FS_BINARY_MOUNTDATA,
};
#elif defined(AFS_LINUX24_ENV)
DECLARE_FSTYPE(afs_fs_type, "afs", afs_read_super, 0);
#else
struct file_system_type afs_fs_type = {
    "afs",			/* name - used by mount operation. */
    0,				/* requires_dev - no for network filesystems. mount() will 
				 * pass us an "unnamed" device. */
    afs_read_super,		/* wrapper to afs_mount */
    NULL			/* pointer to next file_system_type once registered. */
};
#endif

/* afs_read_super
 * read the "super block" for AFS - roughly eguivalent to struct vfs.
 * dev, covered, s_rd_only, s_dirt, and s_type will be set by read_super.
 */
#if defined(AFS_LINUX26_ENV)
static struct super_block *
afs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    return get_sb_nodev(fs_type, flags, data, afs_fill_super);
}

int
afs_fill_super(struct super_block *sb, void *data, int silent)
#else
struct super_block *
afs_read_super(struct super_block *sb, void *data, int silent)
#endif
{
    int code = 0;

    AFS_GLOCK();
    if (afs_was_mounted) {
	printf
	    ("You must reload the AFS kernel extensions before remounting AFS.\n");
	AFS_GUNLOCK();
#if defined(AFS_LINUX26_ENV)
	return -EINVAL;
#else
	return NULL;
#endif
    }
    afs_was_mounted = 1;

    /* Set basics of super_block */
#if !defined(AFS_LINUX24_ENV)
    lock_super(sb);
#endif
#if defined(AFS_LINUX26_ENV)
   __module_get(THIS_MODULE);
#else
    MOD_INC_USE_COUNT;
#endif

    afs_globalVFS = sb;
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = AFS_VFSMAGIC;
    sb->s_op = &afs_sops;	/* Super block (vfs) ops */
#if defined(MAX_NON_LFS)
    sb->s_maxbytes = MAX_NON_LFS;
#endif
    code = afs_root(sb);
    if (code) {
	afs_globalVFS = NULL;
#if defined(AFS_LINUX26_ENV)
        module_put(THIS_MODULE);
#else
        MOD_DEC_USE_COUNT;
#endif
    }

#if !defined(AFS_LINUX24_ENV)
    unlock_super(sb);
#endif

    AFS_GUNLOCK();
#if defined(AFS_LINUX26_ENV)
    return code ? -EINVAL : 0;
#else
    return code ? NULL : sb;
#endif
}


/* afs_root - stat the root of the file system. AFS global held on entry. */
static int
afs_root(struct super_block *afsp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp = 0;

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	cred_t *credp = crref();

	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, credp)) && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    if (tvp) {
		extern struct inode_operations afs_dir_iops;
#if defined(AFS_LINUX24_ENV)
		extern struct file_operations afs_dir_fops;
#endif

		/* "/afs" is a directory, reset inode ops accordingly. */
		AFSTOV(tvp)->v_op = &afs_dir_iops;
#if defined(AFS_LINUX24_ENV)
		AFSTOV(tvp)->v_fop = &afs_dir_fops;
#endif

		/* setup super_block and mount point inode. */
		afs_globalVp = tvp;
#if defined(AFS_LINUX24_ENV)
		afsp->s_root = d_alloc_root(AFSTOI(tvp));
#else
		afsp->s_root = d_alloc_root(AFSTOI(tvp), NULL);
#endif
		afsp->s_root->d_op = &afs_dentry_operations;
	    } else
		code = ENOENT;
	}
	crfree(credp);
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, afs_globalVp,
	       ICL_TYPE_INT32, code);
    return code;
}

/* super_operations */

/* afs_notify_change
 * Linux version of setattr call. What to change is in the iattr struct.
 * We need to set bits in both the Linux inode as well as the vcache.
 */
int
afs_notify_change(struct dentry *dp, struct iattr *iattrp)
{
    struct vattr vattr;
    int code;
    cred_t *credp = crref();
    struct inode *ip = dp->d_inode;

    AFS_GLOCK();
#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    VATTR_NULL(&vattr);
    iattr2vattr(&vattr, iattrp);	/* Convert for AFS vnodeops call. */
    update_inode_cache(ip, &vattr);
    code = afs_setattr(ITOAFS(ip), &vattr, credp);
    afs_CopyOutAttrs(ITOAFS(ip), &vattr);
    /* Note that the inode may still not have all the correct info. But at
     * least we've got the newest version of what was supposed to be set.
     */

#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
    AFS_GUNLOCK();
    crfree(credp);
    return -code;
}


/* This list is simply used to initialize the i_list member of the
 * linux inode. This stops linux inode syncing code from choking on our
 * inodes.
 */
static LIST_HEAD(dummy_inode_list);


/* This is included for documentation only. */
/* afs_write_inode
 * Used to flush in core inode to disk. We don't need to do this. Top level
 * write_inode() routine will clear i_dirt. If this routine is in the table,
 * it's expected to do the cleaning and clear i_dirt.
 * 
 * 9/24/99: This is what we thought until we discovered msync() does end up calling
 * this function to sync a single inode to disk. msync() only flushes selective
 * pages to disk. So it needs an inode syncing function to update metadata when it
 * has synced some pages of a file to disk.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#ifdef WRITE_INODE_NOT_VOID
static int
#else
static void
#endif
afs_write_inode(struct inode *ip, int unused)
#else
static void
afs_write_inode(struct inode *ip)
#endif
{
    list_del(&ip->i_list);
    /* and put it back on our dummy list. */
    put_inode_on_dummy_list(ip);

    /* for now we don't actually update the metadata during msync. This
     * is just to keep linux happy.  */
#ifdef WRITE_INODE_NOT_VOID
    return 0;
#endif
}

#if defined(AFS_LINUX26_ENV)
static void
afs_drop_inode(struct inode *ip)
{
	generic_delete_inode(ip);
	AFS_GUNLOCK();		/* locked by afs_delete_inode() */
}
#endif

static void
afs_destroy_inode(struct inode *ip)
{
    ip->i_state = 0;
}


/* afs_put_inode
 * called from iput when count goes to zero. Linux version of inactive.
 * For Linux 2.2, this funcionality has moved to the delete inode super op.
 * If we use the common inode pool, we'll need to set i_nlink to 0 here.
 * That will trigger the call to delete routine.
 */

static void
afs_delete_inode(struct inode *ip)
{
#if defined(AFS_LINUX26_ENV)
    AFS_GLOCK();		/* after spin_unlock(inode_lock) */
    put_inode_on_dummy_list(ip);
    osi_clear_inode(ip);
#else
    AFS_GLOCK();
    osi_clear_inode(ip);
    AFS_GUNLOCK();
#endif
}


/* afs_put_super
 * Called from unmount to release super_block. */
static void
afs_put_super(struct super_block *sbp)
{
    int code = 0;

    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

#if !defined(AFS_LINUX26_ENV)
    if (!suser()) {
	AFS_GUNLOCK();
	return;
    }
#endif

    afs_globalVFS = 0;
    afs_globalVp = 0;
    afs_shutdown();
#if defined(AFS_LINUX24_ENV)
    mntput(afs_cacheMnt);
#endif

    osi_linux_verify_alloced_memory();
    AFS_GUNLOCK();

    if (!code) {
	sbp->s_dev = 0;
#if defined(AFS_LINUX26_ENV)
	module_put(THIS_MODULE);
#else
	MOD_DEC_USE_COUNT;
#endif
    }
}


/* afs_statfs
 * statp is in user space, so we need to cobble together a statfs, then
 * copy it.
 */
#if defined(AFS_LINUX26_ENV)
int
afs_statfs(struct super_block *sbp, struct kstatfs *statp)
#elif defined(AFS_LINUX24_ENV)
int
afs_statfs(struct super_block *sbp, struct statfs *statp)
#else
int
afs_statfs(struct super_block *sbp, struct statfs *__statp, int size)
#endif
{
#if !defined(AFS_LINUX24_ENV)
    struct statfs stat, *statp;

    if (size < sizeof(struct statfs))
	return;

    memset(&stat, 0, size);
    statp = &stat;
#else
    memset(statp, 0, sizeof(*statp));
#endif

    AFS_STATCNT(afs_statfs);

    statp->f_type = 0;		/* Can we get a real type sometime? */
    statp->f_bsize = sbp->s_blocksize;
    statp->f_blocks = statp->f_bfree = statp->f_bavail = statp->f_files =
	statp->f_ffree = 9000000;
    statp->f_fsid.val[0] = AFS_VFSMAGIC;
    statp->f_fsid.val[1] = AFS_VFSFSID;
    statp->f_namelen = 256;

#if !defined(AFS_LINUX24_ENV)
    memcpy_tofs(__statp, &stat, size);
#endif
    return 0;
}

void
afs_umount_begin(struct super_block *sbp)
{
    afs_shuttingdown = 1;
}

struct super_operations afs_sops = {
#if defined(AFS_LINUX26_ENV)
  .drop_inode =		afs_drop_inode,
  .destroy_inode =	afs_destroy_inode,
#endif
  .delete_inode =	afs_delete_inode,
  .write_inode =	afs_write_inode,
  .put_super =		afs_put_super,
  .statfs =		afs_statfs,
  .umount_begin =	afs_umount_begin
#if !defined(AFS_LINUX24_ENV)
  .notify_change =	afs_notify_change,
#endif
};

/************** Support routines ************************/

/* vattr_setattr
 * Set iattr data into vattr. Assume vattr cleared before call.
 */
static void
iattr2vattr(struct vattr *vattrp, struct iattr *iattrp)
{
    vattrp->va_mask = iattrp->ia_valid;
    if (iattrp->ia_valid & ATTR_MODE)
	vattrp->va_mode = iattrp->ia_mode;
    if (iattrp->ia_valid & ATTR_UID)
	vattrp->va_uid = iattrp->ia_uid;
    if (iattrp->ia_valid & ATTR_GID)
	vattrp->va_gid = iattrp->ia_gid;
    if (iattrp->ia_valid & ATTR_SIZE)
	vattrp->va_size = iattrp->ia_size;
    if (iattrp->ia_valid & ATTR_ATIME) {
#if defined(AFS_LINUX26_ENV)
	vattrp->va_atime.tv_sec = iattrp->ia_atime.tv_sec;
#else
	vattrp->va_atime.tv_sec = iattrp->ia_atime;
#endif
	vattrp->va_atime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
#if defined(AFS_LINUX26_ENV)
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime.tv_sec;
#else
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime;
#endif
	vattrp->va_mtime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
#if defined(AFS_LINUX26_ENV)
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime.tv_sec;
#else
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime;
#endif
	vattrp->va_ctime.tv_usec = 0;
    }
}

/* update_inode_cache
 * Update inode with info from vattr struct. Use va_mask to determine what
 * to update.
 */
static void
update_inode_cache(struct inode *ip, struct vattr *vp)
{
    if (vp->va_mask & ATTR_MODE)
	ip->i_mode = vp->va_mode;
    if (vp->va_mask & ATTR_UID)
	ip->i_uid = vp->va_uid;
    if (vp->va_mask & ATTR_GID)
	ip->i_gid = vp->va_gid;
    if (vp->va_mask & ATTR_SIZE)
	ip->i_size = vp->va_size;
    if (vp->va_mask & ATTR_ATIME)
#if defined(AFS_LINUX26_ENV)
	ip->i_atime.tv_sec = vp->va_atime.tv_sec;
#else
	ip->i_atime = vp->va_atime.tv_sec;
#endif
    if (vp->va_mask & ATTR_MTIME)
#if defined(AFS_LINUX26_ENV)
	ip->i_mtime.tv_sec = vp->va_mtime.tv_sec;
#else
	ip->i_mtime = vp->va_mtime.tv_sec;
#endif
    if (vp->va_mask & ATTR_CTIME)
#if defined(AFS_LINUX26_ENV)
	ip->i_ctime.tv_sec = vp->va_ctime.tv_sec;
#else
	ip->i_ctime = vp->va_ctime.tv_sec;
#endif
}

/* vattr2inode
 * Rewrite the inode cache from the attr. Assumes all vattr fields are valid.
 */
void
vattr2inode(struct inode *ip, struct vattr *vp)
{
    ip->i_ino = vp->va_nodeid;
    ip->i_nlink = vp->va_nlink;
    ip->i_blocks = vp->va_blocks;
    ip->i_blksize = vp->va_blocksize;
    ip->i_rdev = vp->va_rdev;
    ip->i_mode = vp->va_mode;
    ip->i_uid = vp->va_uid;
    ip->i_gid = vp->va_gid;
    ip->i_size = vp->va_size;
#if defined(AFS_LINUX26_ENV)
    ip->i_atime.tv_sec = vp->va_atime.tv_sec;
    ip->i_mtime.tv_sec = vp->va_mtime.tv_sec;
    ip->i_ctime.tv_sec = vp->va_ctime.tv_sec;
#else
    ip->i_atime = vp->va_atime.tv_sec;
    ip->i_mtime = vp->va_mtime.tv_sec;
    ip->i_ctime = vp->va_ctime.tv_sec;
#endif
}

/* Put this afs inode on our own dummy list. Linux expects to see inodes
 * nicely strung up in lists. Linux inode syncing code chokes on our inodes if
 * they're not on any lists.
 */
void
put_inode_on_dummy_list(struct inode *ip)
{
    /* Initialize list. See explanation above. */
    list_add(&ip->i_list, &dummy_inode_list);
}

/* And yet another routine to update the inode cache - called from ProcessFS */
void
vcache2inode(struct vcache *avc)
{
    struct vattr vattr;

    VATTR_NULL(&vattr);
    afs_CopyOutAttrs(avc, &vattr);	/* calls vattr2inode */
}

/* Yet another one for fakestat'ed mountpoints */
void
vcache2fakeinode(struct vcache *rootvp, struct vcache *mpvp)
{
    struct vattr vattr;

    VATTR_NULL(&vattr);
    afs_CopyOutAttrs(rootvp, &vattr);
    vattr2inode(AFSTOI(mpvp), &vattr);
}
