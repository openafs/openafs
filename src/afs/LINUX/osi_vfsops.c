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
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afs_stats.h"
#include "../h/locks.h"
#if defined(AFS_LINUX24_ENV)
#include "../h/smp_lock.h"
#endif

#define __NO_VERSION__ /* don't define kernel_verion in module.h */
#include <linux/module.h>


struct vcache *afs_globalVp = 0;
struct vfs *afs_globalVFS = 0;
int afs_was_mounted = 0; /* Used to force reload if mount/unmount/mount */

extern struct super_operations afs_sops;
extern afs_rwlock_t afs_xvcache;
extern struct afs_q VLRU;

extern struct dentry_operations afs_dentry_operations;

/* Forward declarations */
static void iattr2vattr(struct vattr *vattrp, struct iattr *iattrp);
static void update_inode_cache(struct inode *ip, struct vattr *vp);
static int afs_root(struct super_block *afsp);
struct super_block *afs_read_super(struct super_block *sb, void *data,
				   int silent);
void put_inode_on_dummy_list(struct inode *ip);

/* afs_file_system
 * VFS entry for Linux - installed in init_module
 * Linux mounts file systems by:
 * 1) register_filesystem(&afs_file_system) - done in init_module
 * 2) Mount call comes to us via do_mount -> read_super -> afs_read_super.
 *    We are expected to setup the super_block. See afs_read_super.
 */
#if defined(AFS_LINUX24_ENV)
DECLARE_FSTYPE(afs_file_system, "afs", afs_read_super, 0);
#else
struct file_system_type afs_file_system = {
    "afs",	/* name - used by mount operation. */
    0,		/* requires_dev - no for network filesystems. mount() will 
		 * pass us an "unnamed" device. */
    afs_read_super, /* wrapper to afs_mount */
    NULL	/* pointer to next file_system_type once registered. */
};
#endif

/* afs_read_super
 * read the "super block" for AFS - roughly eguivalent to struct vfs.
 * dev, covered, s_rd_only, s_dirt, and s_type will be set by read_super.
 */
struct super_block *afs_read_super(struct super_block *sb, void *data,
				   int silent)
{
    int code = 0;

    AFS_GLOCK();
    if (afs_was_mounted) {
	printf("You must reload the AFS kernel extensions before remounting AFS.\n");
	return NULL;
    }
    afs_was_mounted = 1;

    /* Set basics of super_block */
#if !defined(AFS_LINUX24_ENV)
    lock_super(sb);
#endif
    MOD_INC_USE_COUNT;

    afs_globalVFS = sb;
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = AFS_VFSMAGIC;
    sb->s_op = &afs_sops;	/* Super block (vfs) ops */
    code = afs_root(sb);
    if (code)
	MOD_DEC_USE_COUNT;

#if !defined(AFS_LINUX24_ENV)
    unlock_super(sb);
#endif

    AFS_GUNLOCK();
    return code ? NULL : sb;
}


/* afs_root - stat the root of the file system. AFS global held on entry. */
static int afs_root(struct super_block *afsp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp=0;

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	cred_t *credp = crref();
	afs_globalVp = 0;
	if (!(code = afs_InitReq(&treq, credp)) &&
	    !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, (afs_int32 *)0,
				(struct vcache*)0, WRITE_LOCK);
	    if (tvp) {
		extern struct inode_operations afs_dir_iops;
#if defined(AFS_LINUX24_ENV)
		extern struct file_operations afs_dir_fops;
#endif
		
		/* "/afs" is a directory, reset inode ops accordingly. */
		tvp->v.v_op = &afs_dir_iops;
#if defined(AFS_LINUX24_ENV)
		tvp->v.v_fop = &afs_dir_fops;
#endif

		/* setup super_block and mount point inode. */
		afs_globalVp = tvp;
#if defined(AFS_LINUX24_ENV)
		afsp->s_root = d_alloc_root((struct inode*)tvp);
#else
		afsp->s_root = d_alloc_root((struct inode*)tvp, NULL);
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

/* afs_read_inode
 * called via iget to read in the inode. The passed in inode has i_ino, i_dev
 * and i_sb setup on input. Linux file systems use this to get super block
 * inode information, so we don't really care what happens here.
 * For Linux 2.2, we'll be called if we participate in the inode pool.
 */
void afs_read_inode(struct inode *ip)
{
    /* I don't think we ever get called with this. So print if we do. */
    printf("afs_read_inode: Called for inode %d\n", ip->i_ino);
}


/* afs_notify_change
 * Linux version of setattr call. What to change is in the iattr struct.
 * We need to set bits in both the Linux inode as well as the vcache.
 */
int afs_notify_change(struct dentry *dp, struct iattr* iattrp)
{
    struct vattr vattr;
    int code;
    cred_t *credp = crref();
    struct inode *ip = dp->d_inode;

    AFS_GLOCK();
    VATTR_NULL(&vattr);
    iattr2vattr(&vattr, iattrp); /* Convert for AFS vnodeops call. */
    update_inode_cache(ip, &vattr);
    code = afs_setattr((struct vcache*)ip, &vattr, credp);
    afs_CopyOutAttrs((struct vcache*)ip, &vattr);
    /* Note that the inode may still not have all the correct info. But at
     * least we've got the newest version of what was supposed to be set.
     */

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
void afs_write_inode(struct inode *ip, int unused) 
#else
void afs_write_inode(struct inode *ip) 
#endif
{
    /* and put it back on our dummy list. */
    list_del(&ip->i_list);
    list_add(&ip->i_list, &dummy_inode_list);

    /* for now we don't actually update the metadata during msync. This
     * is just to keep linux happy.  */
}


/* afs_put_inode
 * called from iput when count goes to zero. Linux version of inactive.
 * For Linux 2.2, this funcionality has moved to the delete inode super op.
 * If we use the common inode pool, we'll need to set i_nlink to 0 here.
 * That will trigger the call to delete routine.
 */

void afs_delete_inode(struct inode *ip)
{
    struct vcache *vc = (struct vcache*)ip;

    AFS_GLOCK();
    ObtainWriteLock(&vc->lock, 504);
    osi_clear_inode(ip);
    ReleaseWriteLock(&vc->lock);
    AFS_GUNLOCK();
}


/* afs_put_super
 * Called from unmount to release super_block. */
void afs_put_super(struct super_block *sbp)
{
    extern int afs_afs_cold_shutdown;
    int code = 0;
    int fv_slept;

    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    if (!suser()) {
	AFS_GUNLOCK();
	return;
    }

    afs_globalVFS = 0;
    afs_globalVp = 0;
    afs_shutdown();

    osi_linux_verify_alloced_memory();
 done:
    AFS_GUNLOCK();

    if (!code) {
	sbp->s_dev = 0;
	MOD_DEC_USE_COUNT;
    }
}

#ifdef NOTUSED
/* afs_write_super
 * Not required since we don't write out a super block. */
void afs_write_super(struct super_block *sbp)
{
}

/* afs_remount_fs
 * Used to remount filesystems with different flags. Not relevant for AFS.
 */
int afs_remount_fs(struct super_block *sbp, int *, char *)
{
    return -EINVAL;
}
#endif

/* afs_statfs
 * statp is in user space, so we need to cobble together a statfs, then
 * copy it.
 */
#if defined(AFS_LINUX24_ENV)
int afs_statfs(struct super_block *sbp, struct statfs *statp)
#else
int afs_statfs(struct super_block *sbp, struct statfs *statp, int size)
#endif
{
    struct statfs stat;

    AFS_STATCNT(afs_statfs);

#if !defined(AFS_LINUX24_ENV)
    if (size < sizeof(struct statfs))
	return;
	
    memset(&stat, 0, size);
#endif
    stat.f_type = 0; /* Can we get a real type sometime? */
    stat.f_bsize = sbp->s_blocksize;
    stat.f_blocks =  stat.f_bfree =  stat.f_bavail =  stat.f_files =
	stat.f_ffree = 9000000;
    stat.f_fsid.val[0] = AFS_VFSMAGIC;
    stat.f_fsid.val[1] = AFS_VFSFSID;
    stat.f_namelen = 256;

#if defined(AFS_LINUX24_ENV)
    *statp = stat;
#else
    memcpy_tofs(statp, &stat, size);
#endif
    return 0;
}


#if defined(AFS_LINUX24_ENV)
struct super_operations afs_sops = {
    read_inode:        afs_read_inode,
    write_inode:       afs_write_inode,
    delete_inode:      afs_delete_inode,
    put_super:         afs_put_super,
    statfs:            afs_statfs,
};
#else
struct super_operations afs_sops = {
    afs_read_inode,
    afs_write_inode,		/* afs_write_inode - see doc above. */
    NULL,		/* afs_put_inode */
    afs_delete_inode,
    afs_notify_change,
    afs_put_super,
    NULL,		/* afs_write_super - see doc above */
    afs_statfs,
    NULL,		/* afs_remount_fs - see doc above */
    NULL,		/* afs_clear_inode */
    NULL,		/* afs_umount_begin */
};
#endif

/************** Support routines ************************/

/* vattr_setattr
 * Set iattr data into vattr. Assume vattr cleared before call.
 */
static void iattr2vattr(struct vattr *vattrp, struct iattr *iattrp)
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
	vattrp->va_atime.tv_sec = iattrp->ia_atime;
	vattrp->va_atime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime;
	vattrp->va_mtime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime;
	vattrp->va_ctime.tv_usec = 0;
    }
}

/* update_inode_cache
 * Update inode with info from vattr struct. Use va_mask to determine what
 * to update.
 */
static void update_inode_cache(struct inode *ip, struct vattr *vp)
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
	ip->i_atime = vp->va_atime.tv_sec;
    if (vp->va_mask & ATTR_MTIME)
	ip->i_mtime = vp->va_mtime.tv_sec;
    if (vp->va_mask & ATTR_CTIME)
	ip->i_ctime = vp->va_ctime.tv_sec;
}

/* vattr2inode
 * Rewrite the inode cache from the attr. Assumes all vattr fields are valid.
 */
void vattr2inode(struct inode *ip, struct vattr *vp)
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
    ip->i_atime = vp->va_atime.tv_sec;
    ip->i_mtime = vp->va_mtime.tv_sec;
    ip->i_ctime = vp->va_ctime.tv_sec;

    /* we should put our inodes on a dummy inode list to keep linux happy.*/
    if (!ip->i_list.prev && !ip->i_list.next) { 
	/* this might be bad as we are reaching under the covers of the 
	 * list structure but we want to avoid putting the inode 
	 * on the list more than once.	 */
	put_inode_on_dummy_list(ip);
    }
}

/* Put this afs inode on our own dummy list. Linux expects to see inodes
 * nicely strung up in lists. Linux inode syncing code chokes on our inodes if
 * they're not on any lists.
 */
void put_inode_on_dummy_list(struct inode *ip)
{
    /* Initialize list. See explanation above. */
    list_add(&ip->i_list, &dummy_inode_list);
}

/* And yet another routine to update the inode cache - called from ProcessFS */
void vcache2inode(struct vcache *avc)
{
    struct vattr vattr;

    VATTR_NULL(&vattr);
    afs_CopyOutAttrs(avc, &vattr); /* calls vattr2inode */
}
