/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux interpretations of vnode and vfs structs.
 *
 * The Linux "inode" has been abstracted to the fs independent part to avoid
 * wasting 100+bytes per vnode.
 */

#ifndef OSI_VFS_H_
#define OSI_VFS_H_

/* The vnode should match the current implementation of the fs independent
 * part of the Linux inode.
 */
/* The first cut is to continue to use a separate vnode pool. */
typedef struct vnode {
	struct list_head	i_hash;
	struct list_head	i_list;
	struct list_head	i_dentry;
#if defined(AFS_LINUX24_ENV)
        struct list_head        i_dirty_buffers;
#endif
#if defined(STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS)
        struct list_head        i_dirty_data_buffers;
#endif
	unsigned long		i_ino;
	unsigned int		i_count;
	kdev_t			i_dev;
	umode_t			i_mode;
	nlink_t			i_nlink;
	uid_t			i_uid;
	gid_t			i_gid;
	kdev_t			i_rdev;
#if defined(AFS_LINUX24_ENV) || defined(pgoff2loff) 
        loff_t                  i_size;
#else
	off_t			i_size;
#endif
	time_t			i_atime;
	time_t			i_mtime;
	time_t			i_ctime;
	unsigned long		i_blksize;
	unsigned long		i_blocks;
	unsigned long		i_version;
#if !defined(AFS_LINUX24_ENV)
	unsigned long		i_nrpages;
#endif
#ifdef STRUCT_INODE_HAS_I_BYTES
        unsigned short          i_bytes;
#endif
	struct semaphore	i_sem;
#ifdef STRUCT_INODE_HAS_I_TRUNCATE_SEM
        struct rw_semaphore     i_truncate_sem;
#endif
#if defined(AFS_LINUX24_ENV)
        struct semaphore        i_zombie;
#else
	struct semaphore	i_atomic_write;
#endif
	struct inode_operations	*i_op;
#if defined(AFS_LINUX24_ENV)
        struct file_operations  *i_fop;
#endif
	struct super_block	*i_sb;
#if defined(AFS_LINUX24_ENV)
        wait_queue_head_t       i_wait;
#else
	struct wait_queue	*i_wait;
#endif
	struct file_lock	*i_flock;
#if defined(AFS_LINUX24_ENV)
        struct address_space    *i_mapping;
        struct address_space    i_data;
#else
	struct vm_area_struct	*i_mmap;
#if defined(STRUCT_INODE_HAS_I_MMAP_SHARED)
        struct vm_area_struct   *i_mmap_shared;
#endif
	struct page		*i_pages;
#endif
#if defined(STRUCT_INODE_HAS_I_MAPPING_OVERLOAD)
        int                     i_mapping_overload;
#endif
	struct dquot		*i_dquot[MAXQUOTAS];
#if defined(AFS_LINUX24_ENV)
        struct pipe_inode_info  *i_pipe;
        struct block_device     *i_bdev;
#if defined(STRUCT_INODE_HAS_I_CDEV)
        struct char_device      *i_cdev;
#endif
        unsigned long           i_dnotify_mask;
        struct dnotify_struct   *i_dnotify;
#endif

	unsigned long		i_state;

	unsigned int		i_flags;
#if !defined(AFS_LINUX24_ENV)
	unsigned char		i_pipe;
#endif
	unsigned char		i_sock;

#if defined(AFS_LINUX24_ENV)
        atomic_t                i_writecount;
#else
	int			i_writecount;
#endif
	unsigned int		i_attr_flags;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,10)
	__u32                   i_generation;
#endif
#ifdef notdef
	union {
		struct pipe_inode_info		pipe_i;
		struct minix_inode_info		minix_i;
		struct ext2_inode_info		ext2_i;
		struct hpfs_inode_info		hpfs_i;
		struct ntfs_inode_info          ntfs_i;
		struct msdos_inode_info		msdos_i;
		struct umsdos_inode_info	umsdos_i;
		struct iso_inode_info		isofs_i;
		struct nfs_inode_info		nfs_i;
		struct sysv_inode_info		sysv_i;
		struct affs_inode_info		affs_i;
		struct ufs_inode_info		ufs_i;
		struct romfs_inode_info		romfs_i;
		struct coda_inode_info		coda_i;
		struct smb_inode_info		smbfs_i;
		struct hfs_inode_info		hfs_i;
		struct adfs_inode_info		adfs_i;
		struct qnx4_inode_info		qnx4_i;	   
		struct socket			socket_i;
		void				*generic_ip;
	} u;
#endif
} vnode_t;

/* Map vnode fields to inode fields. */
#define i_number	i_ino
#define v_count		i_count
#define v_op		i_op
#if defined(AFS_LINUX24_ENV)
#define v_fop           i_fop
#endif
#define v_type		i_mode
#define v_vfsp		i_sb
#define vfs_vnodecovered s_covered

/* v_type bits map to mode bits: */
#define VNON 0
#define VREG S_IFREG
#define VDIR S_IFDIR
#define VBLK S_IFBLK
#define VCHR S_IFCHR
#define VLNK S_IFLNK
#define VSOCK S_IFSOCK

/* vcexcl - used only by afs_create */
enum vcexcl { EXCL, NONEXCL } ;

/* afs_open and afs_close needs to distinguish these cases */
#define FWRITE	O_WRONLY|O_RDWR|O_APPEND
#define FTRUNC	O_TRUNC


#define IO_APPEND O_APPEND
#define FSYNC O_SYNC

#define VTOI(V)  ((struct inode*)V)
#define VN_HOLD(V) ((vnode_t*)V)->i_count++;
#define VN_RELE(V) osi_iput((struct inode *)V);
#define VFS_STATFS(V, S) ((V)->s_op->statfs)((V), (S), sizeof(*(S)))



/* Various mode bits */
#define VWRITE	S_IWUSR
#define VREAD	S_IRUSR
#define VEXEC	S_IXUSR
#define VSUID	S_ISUID
#define VSGID	S_ISGID


#define vfs super_block

typedef struct vattr {
    int		va_type;	/* One of v_types above. */
    size_t	va_size;
    unsigned long va_blocks;
    unsigned long va_blocksize;
    int		va_mask;	/* AT_xxx operation to perform. */
    umode_t	va_mode;	/* mode bits. */
    uid_t	va_uid;
    gid_t	va_gid;
    int		va_fsid;	/* Not used? */
    dev_t	va_rdev;
    ino_t	va_nodeid;	/* Inode number */
    nlink_t	va_nlink;	/* link count for file. */
    struct timeval va_atime;
    struct timeval va_mtime;
    struct timeval va_ctime;
} vattr_t;

#define VATTR_NULL(A) memset(A, 0, sizeof(struct vattr))


/* va_masks - these should match their respective ATTR_xxx #defines in fs.h.
 * afs_notify_change has to use the attr bits in both the Linux and AFS
 * meanings. The glue layer code uses the ATTR_xxx style names.
 */
#define AT_SIZE		ATTR_SIZE
#define AT_MODE		ATTR_MODE
#define AT_UID		ATTR_UID
#define AT_GID		ATTR_GID
#define AT_MTIME	ATTR_MTIME


#define vnodeops inode_operations

#endif /* OSI_VFS_H_ */
