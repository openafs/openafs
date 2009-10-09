/* Kernel compatibility routines
 *
 * This file contains definitions to provide compatibility between different
 * versions of the Linux kernel. It is an ifdef maze, but the idea is that
 * by concentrating the horror here, the rest of the tree may remaing a
 * little cleaner...
 */

#ifndef AFS_LINUX_OSI_COMPAT_H
#define AFS_LINUX_OSI_COMPAT_H

#ifndef DO_SYNC_READ
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
#else
static inline void afs_linux_clear_nfsfs_renamed(void) { return; }
static inline void afs_linux_set_nfsfs_renamed(void) { return; }
#endif

#ifndef HAVE_KERNEL_HLIST_UNHASHED
static void
hlist_unhashed(const struct hlist_node *h) {
    return (!h->pprev == NULL);
}
#endif

#if defined(WRITEPAGE_ACTIVATE)
#define AOP_WRITEPAGE_ACTIVATE WRITEPAGE_ACTIVATE
#endif

#if defined(HAVE_WRITE_BEGIN) && !defined(HAVE_GRAB_CACHE_PAGE_WRITE_BEGIN)
static inline struct page *
grab_cache_page_write_begin(struct address_space *mapping, pgoff_t index,
			    unsigned int flags) {
    return __grab_cache_page(mapping, index);
}
#endif

extern void init_once(void *);
#if defined(HAVE_KMEM_CACHE_T)
static inline int
init_once_kmem_cache_t(void * foo, kmem_cache_t * cachep, unsigned long flags) {
    init_once(foo);
    return 0;
}
#define init_once_func init_once_kmem_cache_t
#elif defined(KMEM_CACHE_INIT)
static inline int
init_once_kmem_cache_init(struct kmem_cache * cachep, void * foo) {
    init_once(foo);
    return 0;
}
#define init_once_func init_once_kmem_cache_init
#elif !defined(KMEM_CACHE_CTOR_TAKES_VOID)
static inline int
init_once_compat(void * foo, struct kmem_cache * cachep, unsigned long flags) {
    init_once(foo);
    return 0;
}
#define init_once_func init_once_compat
#else
#define init_once_func init_once
#endif

#ifndef SLAB_RECLAIM_ACCOUNT
#define SLAB_RECLAIM_ACCOUNT 0
#endif

#if defined(HAVE_KMEM_CACHE_T)
#define afs_kmem_cache_t kmem_cache_t
#else
#define afs_kmem_cache_t struct kmem_cache
#endif

static inline afs_kmem_cache_t *
afs_kmem_cache_create(const char *cache_name, size_t size, size_t size2,
			unsigned long flags, void (*ctor)(void *)) {
    return kmem_cache_create(cache_name, size, size2, flags, ctor
#if defined(KMEM_CACHE_TAKES_DTOR)
	, NULL
#endif
	);
}

#if defined(SLAB_KERNEL)
#define GFP_KERNEL SLAB_KERNEL
#endif

#endif
