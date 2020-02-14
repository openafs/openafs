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

#if defined(LINUX_KEYRING_SUPPORT)
# include <linux/rwsem.h>
# include <linux/key.h>
# if defined(HAVE_LINUX_KEY_TYPE_H)
#  include <linux/key-type.h>
# endif
# ifndef KEY_ALLOC_IN_QUOTA
/* Before these flags were added in Linux commit v2.6.18-rc1~816,
 * key_alloc just took a boolean not_in_quota */
#  define KEY_ALLOC_IN_QUOTA 0
#  define KEY_ALLOC_NOT_IN_QUOTA 1
# endif
#endif

#if defined(STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT) && !defined(DCACHE_NEED_AUTOMOUNT)
# define DCACHE_NEED_AUTOMOUNT DMANAGED_AUTOMOUNT
#endif

#ifdef HAVE_LINUX_STRUCT_VFS_PATH
typedef struct vfs_path afs_linux_path_t;
#else
typedef struct path afs_linux_path_t;
#endif

#if defined(STRUCT_DENTRY_HAS_D_U_D_ALIAS)
# define d_alias d_u.d_alias
#endif

#if defined(STRUCT_FILE_HAS_F_PATH)
# if !defined(f_dentry)
#  define f_dentry f_path.dentry
# endif
#endif

#ifndef HAVE_LINUX_FILE_DENTRY
#define file_dentry(file) ((file)->f_dentry)
#endif

#if defined(HAVE_LINUX_LOCKS_LOCK_FILE_WAIT)
# define flock_lock_file_wait locks_lock_file_wait
#endif

#if !defined(HAVE_LINUX_DO_SYNC_READ) && !defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
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
    conflict = posix_test_lock(fp, flp);
    if (conflict) {
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

#ifdef LINUX_KEYRING_SUPPORT
static inline struct key *
afs_linux_key_alloc(struct key_type *type, const char *desc, afs_kuid_t uid,
		    afs_kgid_t gid, key_perm_t perm, unsigned long flags)
{
# if defined(KEY_ALLOC_BYPASS_RESTRICTION)
    return key_alloc(type, desc, uid, gid, current_cred(), perm, flags, NULL);
# elif defined(KEY_ALLOC_NEEDS_STRUCT_TASK)
    return key_alloc(type, desc, uid, gid, current, perm, flags);
# elif defined(KEY_ALLOC_NEEDS_CRED)
    return key_alloc(type, desc, uid, gid, current_cred(), perm, flags);
# else
    return key_alloc(type, desc, uid, gid, perm, flags);
# endif
}

# if defined(STRUCT_TASK_STRUCT_HAS_CRED)
static inline struct key *
afs_session_keyring(afs_ucred_t *cred)
{
#  if defined(STRUCT_CRED_HAS_SESSION_KEYRING)
    return cred->session_keyring;
#  else
    return cred->tgcred->session_keyring;
#  endif
}

static inline struct key*
afs_linux_search_keyring(afs_ucred_t *cred, struct key_type *type)
{
    key_ref_t key_ref;

    if (afs_session_keyring(cred)) {
#  if defined(KEYRING_SEARCH_TAKES_RECURSE)
	key_ref = keyring_search(
		      make_key_ref(afs_session_keyring(cred), 1),
		      type, "_pag", 1);
#  else
	key_ref = keyring_search(
		      make_key_ref(afs_session_keyring(cred), 1),
		      type, "_pag");
#  endif
	if (IS_ERR(key_ref))
	    return ERR_CAST(key_ref);

	return key_ref_to_ptr(key_ref);
    }

    return ERR_PTR(-ENOKEY);
}
# else
static inline struct key*
afs_linux_search_keyring(afs_ucred_t *cred, struct key_type *type)
{
    return request_key(type, "_pag", NULL);
}
# endif /* STRUCT_TASK_STRUCT_HAS_CRED */

static_inline struct key *
afs_set_session_keyring(struct key *keyring)
{
    struct key *old;
#if defined(STRUCT_CRED_HAS_SESSION_KEYRING)
    struct cred *new_creds;
    old = current_session_keyring();
    new_creds = prepare_creds();
    rcu_assign_pointer(new_creds->session_keyring, keyring);
    commit_creds(new_creds);
#else
    spin_lock_irq(&current->sighand->siglock);
    old = task_session_keyring(current);
    smp_wmb();
    task_session_keyring(current) = keyring;
    spin_unlock_irq(&current->sighand->siglock);
#endif
    return old;
}
#endif /* LINUX_KEYRING_SUPPORT */

#ifdef STRUCT_TASK_STRUCT_HAS_CRED
static inline int
afs_linux_cred_is_current(afs_ucred_t *cred)
{
    return (cred == current_cred());
}
#else
static inline int
afs_linux_cred_is_current(afs_ucred_t *cred)
{
    return 1;
}
#endif

#ifndef HAVE_LINUX_PAGE_OFFSET
static inline loff_t
page_offset(struct page *pp)
{
    return (((loff_t) pp->index) << PAGE_SHIFT);
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

static inline void
zero_user_segment(struct page *pp, unsigned int from1, unsigned int to1)
{
    zero_user_segments(pp, from1, to1, 0, 0);
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
    ret = sockp->ops->getsockopt(sockp, level, name, val, len);
    set_fs(old_fs);

    return ret;
}
#endif

#ifdef HAVE_TRY_TO_FREEZE
static inline int
afs_try_to_freeze(void) {
# ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
    return try_to_freeze(PF_FREEZE);
# else
    return try_to_freeze();
# endif
}
#else
static inline int
afs_try_to_freeze(void) {
# ifdef CONFIG_PM
    if (current->flags & PF_FREEZE) {
	refrigerator(PF_FREEZE);
	return 1;
    }
# endif
    return 0;
}
#endif

/* The commit which changed refrigerator so that it takes no arguments
 * also added freezing(), so if LINUX_REFRIGERATOR_TAKES_PF_FREEZE is
 * true, the kernel doesn't have a freezing() function.
 */
#ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
static inline int
freezing(struct task_struct *p)
{
# ifdef CONFIG_PM
    return p->flags & PF_FREEZE;
# else
    return 0;
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
#if defined(EXPORT_OP_ENCODE_FH_TAKES_INODES)
        return dp->d_sb->s_export_op->encode_fh(dp->d_inode, &ainode->raw[0], max_lenp, NULL);
#else
        return dp->d_sb->s_export_op->encode_fh(dp, &ainode->raw[0], max_lenp, 0);
#endif
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
    if (!sb->s_export_op->find_exported_dentry) {
	/* Some kernels (at least 2.6.9) do not prototype find_exported_dentry,
	 * even though it is exported, so prototype it ourselves. Newer
	 * kernels do prototype it, but as long as our protoype matches the
	 * real one (the signature never changed before NEW_EXPORT_OPS came
	 * into play), there should be no problems. */
	extern struct dentry * find_exported_dentry(struct super_block *sb, void *obj, void *parent,
	                                            int (*acceptable)(void *context, struct dentry *de),
	                                            void *context);
	sb->s_export_op->find_exported_dentry = find_exported_dentry;
    }
#endif
}

static inline void
afs_linux_lock_inode(struct inode *ip) {
#if defined(HAVE_LINUX_INODE_LOCK)
    inode_lock(ip);
#elif defined(STRUCT_INODE_HAS_I_MUTEX)
    mutex_lock(&ip->i_mutex);
#else
    down(&ip->i_sem);
#endif
}

static inline void
afs_linux_unlock_inode(struct inode *ip) {
#if defined(HAVE_LINUX_INODE_LOCK)
    inode_unlock(ip);
#elif defined(STRUCT_INODE_HAS_I_MUTEX)
    mutex_unlock(&ip->i_mutex);
#else
    up(&ip->i_sem);
#endif
}

/* Use these to lock or unlock an inode for processing
 * its dentry aliases en masse.
 */
#if defined(HAVE_DCACHE_LOCK)
#define afs_d_alias_lock(ip)	    spin_lock(&dcache_lock)
#define afs_d_alias_unlock(ip)	    spin_unlock(&dcache_lock)
#else
#define afs_d_alias_lock(ip)	    spin_lock(&(ip)->i_lock)
#define afs_d_alias_unlock(ip)	    spin_unlock(&(ip)->i_lock)
#endif


/* Use this instead of dget for dentry operations
 * that occur under a higher lock (e.g. alias processing).
 * Requires that the higher lock (e.g. dcache_lock or
 * inode->i_lock) is already held.
 */
static inline void
afs_linux_dget(struct dentry *dp) {
#if defined(HAVE_DCACHE_LOCK)
    dget_locked(dp);
#else
    dget(dp);
#endif
}


static inline int
afs_inode_setattr(struct osi_file *afile, struct iattr *newattrs) {

    int code = 0;
    struct inode *inode = OSIFILE_INODE(afile);
#if !defined(HAVE_LINUX_INODE_SETATTR)
    code = inode->i_op->setattr(afile->filp->f_dentry, newattrs);
#elif defined(INODE_SETATTR_NOT_VOID)
    if (inode->i_op && inode->i_op->setattr)
	code = inode->i_op->setattr(afile->filp->f_dentry, newattrs);
    else
	code = inode_setattr(inode, newattrs);
#else
    inode_setattr(inode, newattrs);
#endif
    return code;
}

#if defined(HAVE_LINUX_PATH_LOOKUP)
static inline int
afs_kern_path(char *aname, int flags, struct nameidata *nd) {
    return path_lookup(aname, flags, nd);
}
#else
static inline int
afs_kern_path(char *aname, int flags, afs_linux_path_t *path) {
    return kern_path(aname, flags, path);
}
#endif

static inline void
#if defined(HAVE_LINUX_PATH_LOOKUP)
afs_get_dentry_ref(struct nameidata *nd, struct vfsmount **mnt, struct dentry **dpp) {
#else
afs_get_dentry_ref(afs_linux_path_t *path, struct vfsmount **mnt, struct dentry **dpp) {
#endif
#if defined(HAVE_LINUX_PATH_LOOKUP)
# if defined(STRUCT_NAMEIDATA_HAS_PATH)
    *dpp = dget(nd->path.dentry);
    if (mnt)
	*mnt = mntget(nd->path.mnt);
    path_put(&nd->path);
# else
    *dpp = dget(nd->dentry);
    if (mnt)
	*mnt = mntget(nd->mnt);
    path_release(nd);
# endif
#else
    *dpp = dget(path->dentry);
    if (mnt)
	*mnt = mntget(path->mnt);
    path_put(path);
#endif
}

/* wait_event_freezable appeared with 2.6.24 */

/* These implement the original AFS wait behaviour, with respect to the
 * refrigerator, rather than the behaviour of the current wait_event_freezable
 * implementation.
 */

#ifndef wait_event_freezable
# define wait_event_freezable(waitqueue, condition)				\
({										\
    int _ret;									\
    do {									\
	_ret = wait_event_interruptible(waitqueue, 				\
					(condition) || freezing(current));	\
	if (_ret && !freezing(current))					\
	    break;								\
	else if (!(condition))							\
	    _ret = -EINTR;							\
    } while (afs_try_to_freeze());						\
    _ret;									\
})

# define wait_event_freezable_timeout(waitqueue, condition, timeout)		\
({										\
     int _ret;									\
     do {									\
	_ret = wait_event_interruptible_timeout(waitqueue,			\
						(condition || 			\
						 freezing(current)),		\
						timeout);			\
     } while (afs_try_to_freeze());						\
     _ret;									\
})
#endif

#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
static inline struct file *
afs_dentry_open(struct dentry *dp, struct vfsmount *mnt, int flags, const struct cred *creds) {
#if defined(DENTRY_OPEN_TAKES_PATH)
    afs_linux_path_t path;
    struct file *filp;
    path.mnt = mnt;
    path.dentry = dp;
    /* note that dentry_open will path_get for us */
    filp = dentry_open(&path, flags, creds);
    return filp;
#else
    return dentry_open(dget(dp), mntget(mnt), flags, creds);
#endif
}
#endif

static inline int
afs_truncate(struct inode *inode, int len)
{
    int code;
#if defined(STRUCT_INODE_OPERATIONS_HAS_TRUNCATE)
    code = vmtruncate(inode, len);
#else
    code = inode_newsize_ok(inode, len);
    if (!code)
        truncate_setsize(inode, len);
#endif
    return code;
}

static inline struct proc_dir_entry *
#if defined(HAVE_LINUX_STRUCT_PROC_OPS)
afs_proc_create(char *name, umode_t mode, struct proc_dir_entry *parent, struct proc_ops *ops) {
#else
afs_proc_create(char *name, umode_t mode, struct proc_dir_entry *parent, struct file_operations *ops) {
#endif
#if defined(HAVE_LINUX_PROC_CREATE)
    return proc_create(name, mode, parent, ops);
#else
    struct proc_dir_entry *entry;
    entry = create_proc_entry(name, mode, parent);
    if (entry)
	entry->proc_fops = ops;
    return entry;
#endif
}

static inline int
afs_dentry_count(struct dentry *dp)
{
#if defined(HAVE_LINUX_D_COUNT)
    return d_count(dp);
#elif defined(D_COUNT_INT)
    return dp->d_count;
#else
    return atomic_read(&dp->d_count);
#endif
}

static inline void
afs_maybe_shrink_dcache(struct dentry *dp)
{
#if defined(HAVE_LINUX_D_COUNT) || defined(D_COUNT_INT)
    spin_lock(&dp->d_lock);
    if (afs_dentry_count(dp) > 1) {
	spin_unlock(&dp->d_lock);
	shrink_dcache_parent(dp);
    } else
	spin_unlock(&dp->d_lock);
#else
    if (afs_dentry_count(dp) > 1)
	shrink_dcache_parent(dp);
#endif
}

static inline int
afs_d_invalidate(struct dentry *dp)
{
#if defined(D_INVALIDATE_IS_VOID)
    d_invalidate(dp);
    return 0;
#else
    return d_invalidate(dp);
#endif
}

#if defined(HAVE_LINUX___VFS_WRITE)
# define AFS_FILE_NEEDS_SET_FS 1
#elif defined(HAVE_LINUX_KERNEL_WRITE)
/* #undef AFS_FILE_NEEDS_SET_FS */
#else
# define AFS_FILE_NEEDS_SET_FS 1
#endif

static inline int
afs_file_read(struct file *filp, char __user *buf, size_t len, loff_t *pos)
{
#if defined(HAVE_LINUX___VFS_WRITE)
    return __vfs_read(filp, buf, len, pos);
#elif defined(HAVE_LINUX_KERNEL_WRITE)
# if defined(KERNEL_READ_OFFSET_IS_LAST)
    return kernel_read(filp, buf, len, pos);
# else
    return kernel_read(filp, *pos, buf, len);
# endif
#else
    return filp->f_op->read(filp, buf, len, pos);
#endif
}

static inline int
afs_file_write(struct file *filp, char __user *buf, size_t len, loff_t *pos)
{
#if defined(HAVE_LINUX___VFS_WRITE)
    return __vfs_write(filp, buf, len, pos);
#elif defined(HAVE_LINUX_KERNEL_WRITE)
# if defined(KERNEL_READ_OFFSET_IS_LAST)
    return kernel_write(filp, buf, len, pos);
# else
    return kernel_write(filp, buf, len, *pos);
# endif
#else
    return filp->f_op->write(filp, buf, len, pos);
#endif
}

#endif /* AFS_LINUX_OSI_COMPAT_H */
