/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * SGI specific vfs related defines
 */
#ifndef _SGI_VFS_H_
#define _SGI_VFS_H_

/*
 * UFS -> XFS translations
 */
/* Note: If SGI ever has more than one behavior per vnode, we'll have
 * to search for the one we want.
 */
#define XFS_VTOI(V) ((xfs_inode_t*)((V)->v_fbhv->bd_pdata))
#define vfstom(V) (bhvtom((V)->vfs_fbhv))

struct xfs_inode;
typedef struct xfs_inode xfs_inode_t;
#define xfs_ino_t uint64_t
#define XFS_ILOCK_SHARED 0x08
struct xfs_trans;
struct xfs_mount;
extern int xfs_iget(struct mount *, struct xfs_trans *, xfs_ino_t, uint,
		    xfs_inode_t **, daddr_t);

#ifdef AFS_SGI64_ENV
#define XFS_ITOV(ip) BHV_TO_VNODE((struct bhv_desc *)(((char*)(ip)) + 6*sizeof(void*)))
#else
#define XFS_ITOV(ip) (*((vnode_t**)((((char*)(ip)) + 6*sizeof(void*)))))
#endif

/* When we have XFS only clients, then these macros will be defined in
 * terms of the XFS inode only.
 */
extern struct vnodeops *afs_xfs_vnodeopsp;

/* These need to be functions to wrap the vnode op calls. */
extern ino_t VnodeToIno(vnode_t * vp);
extern dev_t VnodeToDev(vnode_t * vp);
extern off_t VnodeToSize(vnode_t * vp);

/* Page routines are vnode ops in Irix 6.5. These macros paper over the
 * differences.
 */
#define	PTOSSVP(vp, off, len)  VOP_TOSS_PAGES((vp), (off), (len), 0)
#define PFLUSHINVALVP(vp, off, len) VOP_FLUSHINVAL_PAGES((vp), (off), (len), 0)
#define PFLUSHVP(vp, len, flags, code) \
		VOP_FLUSH_PAGES((vp), 0, (len), (flags), 0, code)
#define PINVALFREE(vp, off)    VOP_INVALFREE_PAGES((vp), (off))


/*
 * VFS translations
 */
#ifdef AFS_SGI_VNODE_GLUE
/* The Octane does not have the mrlock_t in the bvh_head_t. So we should
 * not lock it.
 */
extern int afs_is_numa_arch;
#define BHV_IPREVENT(BHP) afs_is_numa_arch ? BHV_INSERT_PREVENT(BHP) : (void)0
#define BHV_IALLOW(BHP)   afs_is_numa_arch ? BHV_INSERT_ALLOW(BHP) : (void)0

/* similar definition in /usr/include/sys/vfs.h (sgi_64) */
#define VFS_STATFS(vfsp,sp,vp)\
	(*((vfsops_t *)(vfsp)->vfs_fops)->vfs_statvfs)((vfsp)->vfs_fbhv,sp,vp)
#define	AFS_VOP_ATTR_SET(vp, name, value, valuelen, flags, cred, rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_attr_set)((vp)->v_fbhv,name,value,valuelen,flags,cred); \
	BHV_IALLOW(&(vp)->v_bh);	\
}

#define	AFS_VOP_ATTR_GET(vp, name, value, valuelenp, flags, cred, rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_attr_get)((vp)->v_fbhv,name,value,valuelenp,flags,cred); \
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_SETATTR(vp, vap, f, cr, rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_setattr)((vp)->v_fbhv, vap, f, cr);	\
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_GETATTR(vp, vap, f, cr, rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_getattr)((vp)->v_fbhv, vap, f, cr);	\
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_REMOVE(dvp,p,cr,rv) \
{	\
	BHV_IPREVENT(&(dvp)->v_bh);	\
	rv = (*((vnodeops_t *)(dvp)->v_fops)->vop_remove)((dvp)->v_fbhv,p,cr);	\
	BHV_IALLOW(&(dvp)->v_bh);	\
}
#define	AFS_VOP_LOOKUP(vp,cp,vpp,pnp,f,rdir,cr,rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_lookup)((vp)->v_fbhv,cp,vpp,pnp,f,rdir,cr);	\
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_RMDIR(dp,p,cdir,cr,rv) \
{	\
	BHV_IPREVENT(&(dp)->v_bh);	\
	rv = (*((vnodeops_t *)(dp)->v_fops)->vop_rmdir)((dp)->v_fbhv,p,cdir,cr);	\
	BHV_IALLOW(&(dp)->v_bh);	\
}
#define	AFS_VOP_WRITE(vp,uiop,iof,cr,rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_write)((vp)->v_fbhv,uiop,iof,cr,&(curprocp->p_flid));\
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_READ(vp,uiop,iof,cr,rv) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	rv = (*((vnodeops_t *)(vp)->v_fops)->vop_read)((vp)->v_fbhv,uiop,iof,cr,&(curprocp->p_flid));\
	BHV_IALLOW(&(vp)->v_bh);	\
}
#define	AFS_VOP_RWLOCK(vp,i) \
{	\
	BHV_IPREVENT(&(vp)->v_bh);	\
	(void)(*((vnodeops_t *)(vp)->v_fops)->vop_rwlock)((vp)->v_fbhv, i); \
	/* "allow" is done by rwunlock */	\
}
#define	AFS_VOP_RWUNLOCK(vp,i) \
{	/* "prevent" was done by rwlock */    \
	(void)(*((vnodeops_t *)(vp)->v_fops)->vop_rwunlock)((vp)->v_fbhv, i);\
	BHV_IALLOW(&(vp)->v_bh);	\
}

/* VOP_RECLAIM remains as is, since it doesn't do the PREVENT/ALLOW */

#define AFS_VN_OPEN(path, seg, mode, perm, dvp, why) \
	vn_open((path), (seg), (mode), (perm), (dvp), (why), 0)
#else /* AFS_SGI_VNODE_GLUE */
#define VFS_STATFS	VFS_STATVFS
#define AFS_VOP_ATTR_SET VOP_ATTR_SET
#define AFS_VOP_ATTR_GET VOP_ATTR_GET
#define AFS_VOP_SETATTR  VOP_SETATTR
#define AFS_VOP_GETATTR  VOP_GETATTR
#define AFS_VOP_REMOVE   VOP_REMOVE
#define AFS_VOP_LOOKUP   VOP_LOOKUP
#define AFS_VOP_RMDIR    VOP_RMDIR
#define AFS_VOP_READ(vp, uiop, iof, cr, code) \
	VOP_READ((vp), (uiop), (iof), (cr), &curuthread->ut_flid, code)
#define AFS_VOP_WRITE(vp, uiop, iof, cr, code) \
	VOP_WRITE((vp), (uiop), (iof), (cr), &curuthread->ut_flid, code)

#define AFS_VN_OPEN(path, seg, mode, perm, dvp, why) \
	vn_open((path), (seg), (mode), (perm), (dvp), (why), 0, NULL)

#define AFS_VOP_RWLOCK(vp, flag)	VOP_RWLOCK((vp), (flag))
#define AFS_VOP_RWUNLOCK(vp, flag)	VOP_RWUNLOCK((vp), (flag))

#endif /* AFS_SGI_VNODE_GLUE */
#define devtovfs	vfs_devsearch
#define va_blocksize	va_blksize
#define va_blocks	va_nblocks
#define MAXNAMLEN	256

/* These macros hide the shape change inthe vnode for sgi_64 w/o NUMA.
 * They call routines so that if statements do not contain complex
 * expressions.
 */
#ifdef AFS_SGI_VNODE_GLUE
extern int afs_is_numa_arch;
typedef struct bhv_head1 {
    struct bhv_desc *bh_first;	/* first behavior in chain */
#ifdef notdef
    /* This is not present in the non NUMA machines. */
    mrlock_t bh_mrlock;		/* lock for ops-in-progress synch. */
#endif
} bhv_head1_t;

typedef struct vnode1 {
    struct vnlist v_list;	/* freelist linkage */
    uint v_flag;		/* vnode flags (see below) */
    cnt_t v_count;		/* reference count */
    u_short v_namecap;		/* name cache capability */
    enum vtype v_type;		/* vnode type */
    dev_t v_rdev;		/* device (VCHR, VBLK) */
    struct vfs *v_vfsmountedhere;	/* ptr to vfs mounted here */
    struct vfs *v_vfsp;		/* ptr to containing VFS */
    struct stdata *v_stream;	/* associated stream */
    struct filock *v_filocks;	/* ptr to filock list */
    mutex_t *v_filocksem;	/* ptr to mutex for list */
    vnumber_t v_number;		/* in-core vnode number */
    short v_listid;		/* free list id */
    cnt_t v_intpcount;		/* interp. refcount for imon */
    bhv_head1_t v_bh;		/* behavior head */

    /*
     * Used only by global cache.
     */
    struct vnode *v_hashp;	/* hash list for lookup */
    struct vnode *v_hashn;	/* hash list for lookup */

    /*
     * Values manipulated only by VM and
     * the page/buffer caches.
     */
    struct pregion *v_mreg;	/* mapped file region pointer */
    int v_dbuf;			/* delwri buffer count */
    pgno_t v_pgcnt;		/* pages hashed to vnode */
    struct pfdat *v_dpages;	/* delwri pages */
    struct buf *v_buf;		/* vnode buffer tree head */
    unsigned int v_bufgen;	/* buf list generation number */
    mutex_t v_buf_lock;		/* mutex for buffer tree */

    vnode_pcache_t v_pc;	/* Page cache structure.
				 * per vnode. Refer to
				 * vnode_pcache.h
				 * for details.
				 */
#ifdef VNODE_TRACING
    struct ktrace *v_trace;	/* trace header structure    */
#endif
#ifdef CKPT
    ckpt_handle_t v_ckpt;	/* ckpt lookup info */
#endif
} vnode1_t;

extern struct pfdat *vnode_get_dpages(vnode_t *);
extern int vnode_get_dbuf(vnode_t *);
extern pgno_t vnode_get_pgcnt(vnode_t *);
extern struct pregion *vnode_get_mreg(vnode_t *);
#define VN_GET_DPAGES(V)	(afs_is_numa_arch ? \
				 ((V)->v_dpages) : \
				 (((vnode1_t*)(V))->v_dpages))
#define VN_SET_DPAGES(V, D)	(afs_is_numa_arch ? \
				  ((V)->v_dpages = (D)) : \
				 (((vnode1_t*)(V))->v_dpages = (D)))
#define VN_GET_DBUF(V)		(afs_is_numa_arch ? \
				 ((V)->v_dbuf) : (((vnode1_t*)(V))->v_dbuf))
#define VN_GET_PGCNT(V)		(afs_is_numa_arch ? \
				 ((V)->v_pgcnt) : (((vnode1_t*)(V))->v_pgcnt))
#define VN_GET_MREG(V)		(afs_is_numa_arch ? \
				 ((V)->v_mreg) : (((vnode1_t*)(V))->v_mreg))
#define AFS_VN_MAPPED(V)	(VN_GET_MREG(V) != NULL)
#define AFS_VN_DIRTY(V)		(VN_GET_DBUF(V) || VN_GET_DPAGES(V))
#define AFS_VN_INIT_BUF_LOCK(V)	\
	(afs_is_numa_arch ? \
	 init_mutex(&((V)->v_buf_lock), MUTEX_DEFAULT, "vn_buf_lock", \
		    (long)(V) ) : \
	 init_mutex(&(((vnode1_t*)(V))->v_buf_lock), MUTEX_DEFAULT, \
		    "vn_buf_lock", \
		    (long)(V) ))
#define AFS_VN_DESTROY_BUF_LOCK(V) \
	(afs_is_numa_arch ? \
	 mutex_destroy(&((V)->v_buf_lock)) : \
	 mutex_destroy(&(((vnode1_t*)(V))->v_buf_lock)))

#else
#define VN_GET_DPAGES(V) 	((V)->v_dpages)
#define VN_SET_DPAGES(V, D)	(V)->v_dpages = (D)
#define VN_GET_DBUF(V)		((V)->v_dbuf)
#define VN_GET_PGCNT(V)		((V)->v_pgcnt)
#define VN_GET_MREG(V)		((V)->v_mreg)
#define AFS_VN_MAPPED		VN_MAPPED
#define AFS_VN_DIRTY		VN_DIRTY
#define AFS_VN_INIT_BUF_LOCK(V) \
	init_mutex(&((V)->v_buf_lock), MUTEX_DEFAULT, "vn_buf_lock", (long)(V))
#define AFS_VN_DESTROY_BUF_LOCK(V)	\
	 mutex_destroy(&((V)->v_buf_lock))
#endif /* AFS_SGI_VNODE_GLUE */

/*
 * Misc
 */
#define ucred cred
#define uprintf printf
#define d_fileno d_ino

#define CLBYTES	NBPC
/*
 * Flock(3) call. (from sys/file.h)
 */
#define LOCK_SH         1	/* shared lock */
#define LOCK_EX         2	/* exclusive lock */
#define LOCK_NB         4	/* don't block when locking */
#define LOCK_UN         8	/* unlock */

#endif


#if defined(AFS_SGI64_ENV) && defined(CKPT)
/* This is a fid for checkpoint restart. Note that the length will be
 * greater than 10 and so afs_vget can distinguish this fid.
 */
typedef struct {
    u_short af_len;
    u_short af_cell;
    u_int af_volid;
    u_int af_vno;
    u_int af_uniq;
} afs_fid2_t;
#endif /* _SGI_VFS_H_ */
