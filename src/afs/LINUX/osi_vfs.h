/* Copyright 1998 Transarc Corporation - All Rights Reserved.
 *
 * osi_vfs.h
 *
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

	unsigned long		i_ino;
	unsigned int		i_count;
	kdev_t			i_dev;
	umode_t			i_mode;
	nlink_t			i_nlink;
	uid_t			i_uid;
	gid_t			i_gid;
	kdev_t			i_rdev;
	off_t			i_size;
	time_t			i_atime;
	time_t			i_mtime;
	time_t			i_ctime;
	unsigned long		i_blksize;
	unsigned long		i_blocks;
	unsigned long		i_version;
	unsigned long		i_nrpages;
	struct semaphore	i_sem;
	struct semaphore	i_atomic_write;
	struct inode_operations	*i_op;
	struct super_block	*i_sb;
	struct wait_queue	*i_wait;
	struct file_lock	*i_flock;
	struct vm_area_struct	*i_mmap;
	struct page		*i_pages;
	struct dquot		*i_dquot[MAXQUOTAS];

	unsigned long		i_state;

	unsigned int		i_flags;
	unsigned char		i_pipe;
	unsigned char		i_sock;

	int			i_writecount;
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
