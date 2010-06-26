/* Kernel compatibility routines
 *
 * This file contains definitions to provide compatibility between different
 * versions of the Linux kernel. It is an ifdef maze, but the idea is that
 * by concentrating the horror here, the rest of the tree may remaing a
 * little cleaner...
 */

#ifndef AFS_LINUX_OSI_COMPAT_H
#define AFS_LINUX_OSI_COMPAT_H

#if defined(HAVE_LINUX_FREEZER_H)
# include <linux/freezer.h>
#endif

#ifndef HAVE_LINUX_DO_SYNC_READ
static inline int
do_sync_read(struct file *fp, char *buf, size_t count, loff_t *offp) {
    return generic_file_read(fp, buf, count, offp);
}

static inline int
do_sync_write(struct file *fp, char *buf, size_t count, loff_t *offp) {
    return generic_file_write(fp, buf, count, offp);
}

#endif /* DO_SYNC_READ */

static inline int
afs_posix_lock_file(struct file *fp, struct file_lock *flp) {
#ifdef POSIX_LOCK_FILE_WAIT_ARG
    return posix_lock_file(fp, flp, NULL);
#else
    flp->fl_flags &=~ FL_SLEEP;
    return posix_lock_file(fp, flp);
#endif
}

static inline void
afs_posix_test_lock(struct file *fp, struct file_lock *flp) {
#if defined(POSIX_TEST_LOCK_CONFLICT_ARG)
    struct file_lock conflict;
    if (posix_test_lock(fp, flp, &conflict)) {
	locks_copy_lock(flp, &conflict);
	flp->fl_type = F_UNLCK;
    }
#elif defined(POSIX_TEST_LOCK_RETURNS_CONFLICT)
    struct file_lock *conflict;
    if (conflict = posix_test_lock(fp, flp)) {
	locks_copy_lock(flp, conflict);
	flp->fl_type = F_UNLCK;
    }
#else
    posix_test_lock(fp, flp);
#endif
}

#ifdef DCACHE_NFSFS_RENAMED
static inline void
afs_linux_clear_nfsfs_renamed(struct dentry *dp) {
    spin_lock(&dp->d_lock);
    dp->d_flags &= ~DCACHE_NFSFS_RENAMED;
    spin_unlock(&dp->d_lock);
}

static inline void
afs_linux_set_nfsfs_renamed(struct dentry *dp) {
    spin_lock(&dp->d_lock);
    dp->d_flags |= DCACHE_NFSFS_RENAMED;
    spin_unlock(&dp->d_lock);
}

static inline int
afs_linux_nfsfs_renamed(struct dentry *dp) {
    return dp->d_flags & DCACHE_NFSFS_RENAMED;
}

#else
static inline void afs_linux_clear_nfsfs_renamed(void) { return; }
static inline void afs_linux_set_nfsfs_renamed(void) { return; }
#endif

#ifndef HAVE_LINUX_HLIST_UNHASHED
static void
hlist_unhashed(const struct hlist_node *h) {
    return (!h->pprev == NULL);
}
#endif

#if defined(WRITEPAGE_ACTIVATE)
#define AOP_WRITEPAGE_ACTIVATE WRITEPAGE_ACTIVATE
#endif

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN) && !defined(HAVE_LINUX_GRAB_CACHE_PAGE_WRITE_BEGIN)
static inline struct page *
grab_cache_page_write_begin(struct address_space *mapping, pgoff_t index,
			    unsigned int flags) {
    return __grab_cache_page(mapping, index);
}
#endif

#if defined(HAVE_KMEM_CACHE_T)
#define afs_kmem_cache_t kmem_cache_t
#else
#define afs_kmem_cache_t struct kmem_cache
#endif

extern void init_once(void *);
#if defined(HAVE_KMEM_CACHE_T)
static inline void
init_once_func(void * foo, kmem_cache_t * cachep, unsigned long flags) {
#if defined(SLAB_CTOR_VERIFY)
    if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
        SLAB_CTOR_CONSTRUCTOR)
#endif
    init_once(foo);
}
#elif defined(KMEM_CACHE_INIT)
static inline void
init_once_func(struct kmem_cache * cachep, void * foo) {
    init_once(foo);
}
#elif !defined(KMEM_CACHE_CTOR_TAKES_VOID)
static inline void
init_once_func(void * foo, struct kmem_cache * cachep, unsigned long flags) {
#if defined(SLAB_CTOR_VERIFY)
    if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
        SLAB_CTOR_CONSTRUCTOR)
#endif
    init_once(foo);
}
#else
static inline void
init_once_func(void * foo) {
    init_once(foo);
}
#endif

#ifndef SLAB_RECLAIM_ACCOUNT
#define SLAB_RECLAIM_ACCOUNT 0
#endif

#if defined(SLAB_KERNEL)
#define KALLOC_TYPE SLAB_KERNEL
#else
#define KALLOC_TYPE GFP_KERNEL
#endif

static inline struct key *
afs_linux_key_alloc(struct key_type *type, const char *desc, uid_t uid,
		    gid_t gid, key_perm_t perm, unsigned long flags)
{
#if defined(KEY_ALLOC_NEEDS_STRUCT_TASK)
    return key_alloc(type, desc, uid, gid, current, perm, flags);
#elif defined(KEY_ALLOC_NEEDS_CRED)
    return key_alloc(type, desc, uid, gid, current_cred(), perm, flags);
#else
    return key_alloc(type, desc, uid, gid, perm, flags);
#endif
}

#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
static inline struct key*
afs_linux_search_keyring(afs_ucred_t *cred, struct key_type *type)
{
    key_ref_t key_ref;

    if (cred->tgcred->session_keyring) {
	key_ref = keyring_search(
		      make_key_ref(cred->tgcred->session_keyring, 1),
		      type, "_pag");
	if (IS_ERR(key_ref))
	    return ERR_CAST(key_ref);

	return key_ref_to_ptr(key_ref);
    }

    return ERR_PTR(-ENOKEY);
}

static inline int
afs_linux_cred_is_current(afs_ucred_t *cred)
{
    return (cred == current_cred());
}

#else
static inline struct key*
afs_linux_search_keyring(afs_ucred_t *cred, struct key_type *type)
{
    return request_key(type, "_pag", NULL);
}

static inline int
afs_linux_cred_is_current(afs_ucred_t *cred)
{
    return 1;
}
#endif

#ifdef LINUX_KEYRING_SUPPORT
# ifndef KEY_ALLOC_NOT_IN_QUOTA
#  define KEY_ALLOC_NOT_IN_QUOTA 1
# endif
# ifndef KEY_ALLOC_IN_QUOTA
#  define KEY_ALLOC_IN_QUOTA 0
# endif
#endif
#endif

#ifndef HAVE_LINUX_PAGE_OFFSET
static inline loff_t
page_offset(struct page *pp)
{
    return (((loff_t) pp->index) << PAGE_CACHE_SHIFT);
}
#endif

#ifndef HAVE_LINUX_ZERO_USER_SEGMENTS
static inline void
zero_user_segments(struct page *pp, unsigned int from1, unsigned int to1,
		   unsigned int from2, unsigned int to2)
{
    void *base = kmap_atomic(pp, KM_USER0);

    if (to1 > from1)
	memset(base + from1, 0, to1 - from1);

    if (to2 > from2)
	memset(base + from2, 0, to2 - from2);

    flush_dcache_page(pp);
    kunmap_atomic(base, KM_USER0);
}
#endif

#ifndef HAVE_LINUX_KERNEL_SETSOCKOPT
/* Available from 2.6.19 */

static inline int
kernel_setsockopt(struct socket *sockp, int level, int name, char *val,
		  unsigned int len) {
    mm_segment_t old_fs = get_fs();
    int ret;

    set_fs(get_ds());
    ret = sockp->ops->setsockopt(sockp, level, name, val, len);
    set_fs(old_fs);

    return ret;
}

static inline int
kernel_getsockopt(struct socket *sockp, int level, int name, char *val,
		  int *len) {
    mm_segment_t old_fs = get_fs();
    int ret;

    set_fs(get_ds());
    ret = sockp->ops->setsockopt(sockp, level, name, val, len);
    set_fs(old_fs);

    return ret;
}
#endif

#ifdef HAVE_TRY_TO_FREEZE
static inline void
afs_try_to_freeze(void) {
# ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
    try_to_freeze(PF_FREEZE);
# else
    try_to_freeze();
# endif
}
#else
static inline void
afs_try_to_freeze(void) {
# ifdef CONFIG_PM
    if (current->flags & PF_FREEZE) {
	refrigerator(PF_FREEZE);
# endif
}
#endif

#if !defined(HAVE_LINUX_PAGECHECKED)
# if defined(HAVE_LINUX_PAGEFSMISC)
#  include <linux/page-flags.h>

#  define PageChecked(p)            PageFsMisc((p))
#  define SetPageChecked(p)         SetPageFsMisc((p))
#  define ClearPageChecked(p)       ClearPageFsMisc((p))

# endif
#endif

#if !defined(NEW_EXPORT_OPS)
extern struct export_operations export_op_default;
#endif

static inline struct dentry *
afs_get_dentry_from_fh(struct super_block *afs_cacheSBp, afs_dcache_id_t *ainode,
		int cache_fh_len, int cache_fh_type,
		int (*afs_fh_acceptable)(void *, struct dentry *)) {
#if defined(NEW_EXPORT_OPS)
    return afs_cacheSBp->s_export_op->fh_to_dentry(afs_cacheSBp, &ainode->ufs.fh,
		cache_fh_len, cache_fh_type);
#else
    if (afs_cacheSBp->s_export_op && afs_cacheSBp->s_export_op->decode_fh)
	return afs_cacheSBp->s_export_op->decode_fh(afs_cacheSBp, ainode->ufs.raw,
			cache_fh_len, cache_fh_type, afs_fh_acceptable, NULL);
    else
	return export_op_default.decode_fh(afs_cacheSBp, ainode->ufs.raw,
			cache_fh_len, cache_fh_type, afs_fh_acceptable, NULL);
#endif
}

static inline int
afs_get_fh_from_dentry(struct dentry *dp, afs_ufs_dcache_id_t *ainode, int *max_lenp) {
    if (dp->d_sb->s_export_op->encode_fh)
        return dp->d_sb->s_export_op->encode_fh(dp, &ainode->raw[0], max_lenp, 0);
#if defined(NEW_EXPORT_OPS)
    /* If fs doesn't provide an encode_fh method, assume the default INO32 type */
    *max_lenp = sizeof(struct fid)/4;
    ainode->fh.i32.ino = dp->d_inode->i_ino;
    ainode->fh.i32.gen = dp->d_inode->i_generation;
    return FILEID_INO32_GEN;
#else
    /* or call the default encoding function for the old API */
    return export_op_default.encode_fh(dp, &ainode->raw[0], max_lenp, 0);
#endif
}

static inline void
afs_init_sb_export_ops(struct super_block *sb) {
#if !defined(NEW_EXPORT_OPS)
    /*
     * decode_fh will call this function.  If not defined for this FS, make
     * sure it points to the default
     */
    if (!sb->s_export_op->find_exported_dentry)
	sb->s_export_op->find_exported_dentry = find_exported_dentry;
#endif
}
