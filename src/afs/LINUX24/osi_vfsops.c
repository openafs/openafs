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
#include "h/locks.h"
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
extern int afs_notify_change(struct dentry *dp, struct iattr *iattrp);

/* Forward declarations */
static int afs_root(struct super_block *afsp);
struct super_block *afs_read_super(struct super_block *sb, void *data, int silent);
int afs_fill_super(struct super_block *sb, void *data, int silent);

/* afs_file_system
 * VFS entry for Linux - installed in init_module
 * Linux mounts file systems by:
 * 1) register_filesystem(&afs_file_system) - done in init_module
 * 2) Mount call comes to us via do_mount -> read_super -> afs_read_super.
 *    We are expected to setup the super_block. See afs_read_super.
 */


/* afs_read_super
 * read the "super block" for AFS - roughly eguivalent to struct vfs.
 * dev, covered, s_rd_only, s_dirt, and s_type will be set by read_super.
 */
#if defined(AFS_LINUX24_ENV)
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

struct super_block *
afs_read_super(struct super_block *sb, void *data, int silent)
{
    int code = 0;

    AFS_GLOCK();
    if (afs_was_mounted) {
	printf
	    ("You must reload the AFS kernel extensions before remounting AFS.\n");
	AFS_GUNLOCK();
	return NULL;
    }
    afs_was_mounted = 1;

    /* Set basics of super_block */
#if !defined(AFS_LINUX24_ENV)
    lock_super(sb);
#endif
    MOD_INC_USE_COUNT;

    afs_globalVFS = sb;
    sb->s_flags |= MS_NOATIME;
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = AFS_VFSMAGIC;
    sb->s_op = &afs_sops;	/* Super block (vfs) ops */
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
        MOD_DEC_USE_COUNT;
    }

#if !defined(AFS_LINUX24_ENV)
    unlock_super(sb);
#endif

    AFS_GUNLOCK();
    return code ? NULL : sb;
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
#if defined(AFS_LINUX24_ENV)
		afsp->s_root = d_alloc_root(ip);
#else
		afsp->s_root = d_alloc_root(ip, NULL);
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

#if defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
#if defined(HAVE_KMEM_CACHE_T)
static kmem_cache_t *afs_inode_cachep;
#else
struct kmem_cache *afs_inode_cachep;
#endif

static struct inode *
afs_alloc_inode(struct super_block *sb)
{
    struct vcache *vcp;

#if defined(SLAB_KERNEL)
    vcp = (struct vcache *) kmem_cache_alloc(afs_inode_cachep, SLAB_KERNEL);
#else
    vcp = (struct vcache *) kmem_cache_alloc(afs_inode_cachep, GFP_KERNEL);
#endif
    if (!vcp)
	return NULL;

    return AFSTOV(vcp);
}

static void
afs_destroy_inode(struct inode *inode)
{
    kmem_cache_free(afs_inode_cachep, inode);
}

static void
#if defined(HAVE_KMEM_CACHE_T)
init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
#else
#if defined(KMEM_CACHE_INIT)
init_once(struct kmem_cache * cachep, void * foo)
#else
init_once(void * foo, struct kmem_cache * cachep, unsigned long flags)
#endif
#endif
{
    struct vcache *vcp = (struct vcache *) foo;

#if defined(SLAB_CTOR_VERIFY)
    if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	SLAB_CTOR_CONSTRUCTOR)
#endif
	inode_init_once(AFSTOV(vcp));
}

int
afs_init_inodecache(void)
{
#ifndef SLAB_RECLAIM_ACCOUNT
#define SLAB_RECLAIM_ACCOUNT 0
#endif

#if defined(KMEM_CACHE_TAKES_DTOR)
    afs_inode_cachep = kmem_cache_create("afs_inode_cache",
					 sizeof(struct vcache),
					 0, SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT,
					 init_once, NULL);
#else
    afs_inode_cachep = kmem_cache_create("afs_inode_cache",
					 sizeof(struct vcache),
					 0, SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT,
					 init_once);
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

/* afs_put_super
 * Called from unmount to release super_block. */
static void
afs_put_super(struct super_block *sbp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    if (!suser()) {
	AFS_GUNLOCK();
	return;
    }

    afs_globalVFS = 0;
    afs_globalVp = 0;

    afs_shutdown();
#if defined(AFS_LINUX24_ENV)
    mntput(afs_cacheMnt);
#endif

    osi_linux_verify_alloced_memory();
    AFS_GUNLOCK();

    sbp->s_dev = 0;
    MOD_DEC_USE_COUNT;
}


/* afs_statfs
 * statp is in user space, so we need to cobble together a statfs, then
 * copy it.
 */
#if defined(AFS_LINUX24_ENV)
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

    /* hardcode in case that which is giveth is taken away */
    statp->f_type = 0x5346414F;
#if defined(STATFS_TAKES_DENTRY)
    statp->f_bsize = dentry->d_sb->s_blocksize;
#else
    statp->f_bsize = sbp->s_blocksize;
#endif
    statp->f_blocks = statp->f_bfree = statp->f_bavail = statp->f_files =
	statp->f_ffree = AFS_VFS_FAKEFREE;
    statp->f_fsid.val[0] = AFS_VFSMAGIC;
    statp->f_fsid.val[1] = AFS_VFSFSID;
    statp->f_namelen = 256;

#if !defined(AFS_LINUX24_ENV)
    memcpy_tofs(__statp, &stat, size);
#endif
    return 0;
}

struct super_operations afs_sops = {
#if defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
  .alloc_inode =	afs_alloc_inode,
  .destroy_inode =	afs_destroy_inode,
#endif
  .clear_inode =	afs_clear_inode,
  .put_super =		afs_put_super,
  .statfs =		afs_statfs,
#if !defined(AFS_LINUX24_ENV)
  .notify_change =	afs_notify_change,
#endif
};
