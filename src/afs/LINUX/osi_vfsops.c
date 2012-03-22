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


#define __NO_VERSION__		/* don't define kernel_version in module.h */
#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"

#include "osi_compat.h"

struct vcache *afs_globalVp = 0;
struct vfs *afs_globalVFS = 0;
struct vfsmount *afs_cacheMnt;
int afs_was_mounted = 0;	/* Used to force reload if mount/unmount/mount */

extern struct super_operations afs_sops;
#if !defined(AFS_NONFSTRANS)
extern struct export_operations afs_export_ops;
#endif
extern afs_rwlock_t afs_xvcache;
extern struct afs_q VLRU;

extern struct dentry_operations afs_dentry_operations;

/* Forward declarations */
static void iattr2vattr(struct vattr *vattrp, struct iattr *iattrp);
static int afs_root(struct super_block *afsp);
int afs_fill_super(struct super_block *sb, void *data, int silent);


/*
 * afs_mount (2.6.37+) and afs_get_sb (2.6.36-) are the entry
 * points from the vfs when mounting afs.  The super block
 * structure is setup in the afs_fill_super callback function.
 */

#if defined(STRUCT_FILE_SYSTEM_TYPE_HAS_MOUNT)
static struct dentry *
afs_mount(struct file_system_type *fs_type, int flags,
           const char *dev_name, void *data) {
    return mount_nodev(fs_type, flags, data, afs_fill_super);
}
#elif defined(GET_SB_HAS_STRUCT_VFSMOUNT)
static int
afs_get_sb(struct file_system_type *fs_type, int flags,
	   const char *dev_name, void *data, struct vfsmount *mnt) {
    return get_sb_nodev(fs_type, flags, data, afs_fill_super, mnt);
}
#else
static struct super_block *
afs_get_sb(struct file_system_type *fs_type, int flags,
	   const char *dev_name, void *data) {
    return get_sb_nodev(fs_type, flags, data, afs_fill_super);
}
#endif

struct file_system_type afs_fs_type = {
    .owner = THIS_MODULE,
    .name = "afs",
#if defined(STRUCT_FILE_SYSTEM_TYPE_HAS_MOUNT)
    .mount = afs_mount,
#else
    .get_sb = afs_get_sb,
#endif
    .kill_sb = kill_anon_super,
    .fs_flags = FS_BINARY_MOUNTDATA,
};

struct backing_dev_info *afs_backing_dev_info;

int
afs_fill_super(struct super_block *sb, void *data, int silent)
{
    int code = 0;

    AFS_GLOCK();
    if (afs_was_mounted) {
	printf
	    ("You must reload the AFS kernel extensions before remounting AFS.\n");
	AFS_GUNLOCK();
	return -EINVAL;
    }
    afs_was_mounted = 1;

    /* Set basics of super_block */
   __module_get(THIS_MODULE);

    afs_globalVFS = sb;
    sb->s_flags |= MS_NOATIME;
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = AFS_VFSMAGIC;
    sb->s_op = &afs_sops;	/* Super block (vfs) ops */

#if defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
    sb->s_d_op = &afs_dentry_operations;
#endif

    /* used for inodes backing_dev_info field, also */
    afs_backing_dev_info = osi_Alloc(sizeof(struct backing_dev_info));
    memset(afs_backing_dev_info, 0, sizeof(struct backing_dev_info));
#if defined(HAVE_LINUX_BDI_INIT)
    bdi_init(afs_backing_dev_info);
#endif
#if defined(STRUCT_BACKING_DEV_INFO_HAS_NAME)
    afs_backing_dev_info->name = "openafs";
#endif
    afs_backing_dev_info->ra_pages = 32;
#if defined (STRUCT_SUPER_BLOCK_HAS_S_BDI)
    sb->s_bdi = afs_backing_dev_info;
    /* The name specified here will appear in the flushing thread name - flush-afs */
    bdi_register(afs_backing_dev_info, NULL, "afs");
#endif
#if !defined(AFS_NONFSTRANS)
    sb->s_export_op = &afs_export_ops;
#endif
#if defined(MAX_NON_LFS)
#ifdef AFS_64BIT_CLIENT
#if !defined(MAX_LFS_FILESIZE)
#if BITS_PER_LONG==32
#define MAX_LFS_FILESIZE (((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1) 
#elif BITS_PER_LONG==64
#define MAX_LFS_FILESIZE 0x7fffffffffffffff
#endif
#endif
    sb->s_maxbytes = MAX_LFS_FILESIZE;
#else
    sb->s_maxbytes = MAX_NON_LFS;
#endif
#endif
    code = afs_root(sb);
    if (code) {
	afs_globalVFS = NULL;
	afs_FlushAllVCaches();
        module_put(THIS_MODULE);
    }

    AFS_GUNLOCK();
    return code ? -EINVAL : 0;
}


/* afs_root - stat the root of the file system. AFS global held on entry. */
static int
afs_root(struct super_block *afsp)
{
    afs_int32 code = 0;
    struct vrequest treq;
    struct vcache *tvp = 0;

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
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
		struct inode *ip = AFSTOV(tvp);
		struct vattr vattr;

		afs_getattr(tvp, &vattr, credp);
		afs_fill_inode(ip, &vattr);

		/* setup super_block and mount point inode. */
		afs_globalVp = tvp;
#if defined(HAVE_LINUX_D_MAKE_ROOT)
		afsp->s_root = d_make_root(ip);
#else
		afsp->s_root = d_alloc_root(ip);
#endif
#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
		afsp->s_root->d_op = &afs_dentry_operations;
#endif
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
    cred_t *credp = crref();
    struct inode *ip = dp->d_inode;
    int code;

    VATTR_NULL(&vattr);
    iattr2vattr(&vattr, iattrp);	/* Convert for AFS vnodeops call. */

    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ip), &vattr, credp);
    if (!code) {
	afs_getattr(VTOAFS(ip), &vattr, credp);
	vattr2inode(ip, &vattr);
    }
    AFS_GUNLOCK();
    crfree(credp);
    return -code;
}


#if defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
static afs_kmem_cache_t *afs_inode_cachep;

static struct inode *
afs_alloc_inode(struct super_block *sb)
{
    struct vcache *vcp;

    vcp = (struct vcache *) kmem_cache_alloc(afs_inode_cachep, KALLOC_TYPE);
    if (!vcp)
	return NULL;

    return AFSTOV(vcp);
}

static void
afs_destroy_inode(struct inode *inode)
{
    kmem_cache_free(afs_inode_cachep, inode);
}

void
init_once(void * foo)
{
    struct vcache *vcp = (struct vcache *) foo;

    inode_init_once(AFSTOV(vcp));
}

int
afs_init_inodecache(void)
{
#if defined(KMEM_CACHE_TAKES_DTOR)
    afs_inode_cachep = kmem_cache_create("afs_inode_cache",
		sizeof(struct vcache), 0,
		SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT, init_once_func, NULL);
#else
    afs_inode_cachep = kmem_cache_create("afs_inode_cache",
		sizeof(struct vcache), 0,
		SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT, init_once_func);
#endif
    if (afs_inode_cachep == NULL)
	return -ENOMEM;
    return 0;
}

void
afs_destroy_inodecache(void)
{
    if (afs_inode_cachep)
	(void) kmem_cache_destroy(afs_inode_cachep);
}
#else
int
afs_init_inodecache(void)
{
    return 0;
}

void
afs_destroy_inodecache(void)
{
    return;
}
#endif

#if defined(STRUCT_SUPER_OPERATIONS_HAS_EVICT_INODE)
static void
afs_evict_inode(struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);

    if (vcp->vlruq.prev || vcp->vlruq.next)
	osi_Panic("inode freed while on LRU");
    if (vcp->hnext)
	osi_Panic("inode freed while still hashed");

    truncate_inode_pages(&ip->i_data, 0);
    end_writeback(ip);

#if !defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
    afs_osi_Free(ip->u.generic_ip, sizeof(struct vcache));
#endif
}
#else
static void
afs_clear_inode(struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);

    if (vcp->vlruq.prev || vcp->vlruq.next)
	osi_Panic("inode freed while on LRU");
    if (vcp->hnext)
	osi_Panic("inode freed while still hashed");

#if !defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
    afs_osi_Free(ip->u.generic_ip, sizeof(struct vcache));
#endif
}
#endif

/* afs_put_super
 * Called from unmount to release super_block. */
static void
afs_put_super(struct super_block *sbp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    afs_globalVFS = 0;
    afs_globalVp = 0;

    afs_shutdown();
    mntput(afs_cacheMnt);

    osi_linux_verify_alloced_memory();
#if defined(HAVE_LINUX_BDI_INIT)
    bdi_destroy(afs_backing_dev_info);
#endif
    osi_Free(afs_backing_dev_info, sizeof(struct backing_dev_info));
    AFS_GUNLOCK();

    sbp->s_dev = 0;
    module_put(THIS_MODULE);
}


/* afs_statfs
 * statp is in user space, so we need to cobble together a statfs, then
 * copy it.
 */
int
#if defined(STATFS_TAKES_DENTRY)
afs_statfs(struct dentry *dentry, struct kstatfs *statp)
#else
afs_statfs(struct super_block *sbp, struct kstatfs *statp)
#endif
{
    memset(statp, 0, sizeof(*statp));

    AFS_STATCNT(afs_statfs);

    /* hardcode in case that which is giveth is taken away */
    statp->f_type = 0x5346414F;
#if defined(STATFS_TAKES_DENTRY)
    statp->f_bsize = dentry->d_sb->s_blocksize;
#else
    statp->f_bsize = sbp->s_blocksize;
#endif
    statp->f_blocks = statp->f_bfree = statp->f_bavail = statp->f_files =
	statp->f_ffree = 9000000;
    statp->f_fsid.val[0] = AFS_VFSMAGIC;
    statp->f_fsid.val[1] = AFS_VFSFSID;
    statp->f_namelen = 256;

    return 0;
}

struct super_operations afs_sops = {
#if defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
  .alloc_inode =	afs_alloc_inode,
  .destroy_inode =	afs_destroy_inode,
#endif
#if defined(STRUCT_SUPER_OPERATIONS_HAS_EVICT_INODE)
  .evict_inode =	afs_evict_inode,
#else
  .clear_inode =	afs_clear_inode,
#endif
  .put_super =		afs_put_super,
  .statfs =		afs_statfs,
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
	vattrp->va_atime.tv_sec = iattrp->ia_atime.tv_sec;
	vattrp->va_atime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime.tv_sec;
	vattrp->va_mtime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime.tv_sec;
	vattrp->va_ctime.tv_usec = 0;
    }
}

/* vattr2inode
 * Rewrite the inode cache from the attr. Assumes all vattr fields are valid.
 */
void
vattr2inode(struct inode *ip, struct vattr *vp)
{
    ip->i_ino = vp->va_nodeid;
#ifdef HAVE_LINUX_SET_NLINK
    set_nlink(ip, vp->va_nlink);
#else
    ip->i_nlink = vp->va_nlink;
#endif
    ip->i_blocks = vp->va_blocks;
#ifdef STRUCT_INODE_HAS_I_BLKBITS
    ip->i_blkbits = AFS_BLKBITS;
#endif
#ifdef STRUCT_INODE_HAS_I_BLKSIZE
    ip->i_blksize = vp->va_blocksize;
#endif
    ip->i_rdev = vp->va_rdev;
    ip->i_mode = vp->va_mode;
    ip->i_uid = vp->va_uid;
    ip->i_gid = vp->va_gid;
    i_size_write(ip, vp->va_size);
    ip->i_atime.tv_sec = vp->va_atime.tv_sec;
    ip->i_atime.tv_nsec = 0;
    ip->i_mtime.tv_sec = vp->va_mtime.tv_sec;
    /* Set the mtime nanoseconds to the sysname generation number.
     * This convinces NFS clients that all directories have changed
     * any time the sysname list changes.
     */
    ip->i_mtime.tv_nsec = afs_sysnamegen;
    ip->i_ctime.tv_sec = vp->va_ctime.tv_sec;
    ip->i_ctime.tv_nsec = 0;
}
