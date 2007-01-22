#ifndef	AFS_PARAM_COMMON_H
#define	AFS_PARAM_COMMON_H


#ifndef UKERNEL
/* This section for kernel libafs compiles only */


#ifndef ASSEMBLER
#include <sys/param.h>
#endif

#define AFS_XBSD_ENV 1		/* {Free,Open,Net}BSD */

#define AFS_NAMEI_ENV     1	/* User space interface to file system */
#define AFS_64BIT_IOPS_ENV 1	/* Needed for NAMEI */
#define AFS_NBSD_ENV 1
#define AFS_NBSD15_ENV 1
#define AFS_NONFSTRANS 1
#define AFS_KERBEROS_ENV 1
#define FTRUNC O_TRUNC

#define IUPD 0x0010
#define IACC 0x0020
#define ICHG 0x0040
#define IMOD 0x0080

#define IN_LOCK(ip)     lockmgr(&ip->i_lock, LK_EXCLUSIVE, \
                                NULL, curproc)
#define IN_UNLOCK(ip)   lockmgr(&ip->i_lock, LK_RELEASE, \
                                NULL, curproc)

#include <afs/afs_sysnames.h>

#define AFS_VM_RDWR_ENV	1
#define AFS_VFS_ENV	1
#define AFS_VFSINCL_ENV 1
#define AFS_GREEDY43_ENV	1
#define AFS_ENV  	1

#define AFS_MOUNT_AFS	"afs"
#define AFS_SYSCALL 210


#ifndef MOUNT_UFS
#define MOUNT_UFS "ufs"
#endif

#ifndef	MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_AFS
#endif

#define AFS_HAVE_FFS            1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't supports statvfs */

#define AFS_GCPAGS	        0	/* if nonzero, garbage collect PAGs */
#define AFS_USE_GETTIMEOFDAY    1	/* use gettimeofday to implement rx clock */

/* Extra kernel definitions (from kdefs file) */
#ifdef _KERNEL
#define AFS_GLOBAL_SUNLOCK        1
#define	AFS_VFS34	1	/* What is VFS34??? */
#define	AFS_SHORTGID	1	/* are group id's short? */
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	UIO_SYSSPACE
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	CLBYTES
#define	osi_GetTime(x)	microtime(x)
#define	AFS_KALLOC(x)	kalloc(x)
#define	AFS_KFREE(x,y)	kfree(x,y)
#define	v_count		v_usecount
#define v_vfsp		v_mount
#define vfs_bsize	mnt_stat.f_bsize
#define vfs_fsid	mnt_stat.f_fsid
#define va_nodeid	va_fileid
#define vfs_vnodecovered mnt_vnodecovered
#define direct		dirent
#define vnode_t		struct vnode

#ifndef MUTEX_DEFAULT
#define MUTEX_DEFAULT   0
#endif /* MUTEX_DEFAULT */

#ifndef SSYS
#define SSYS            0x00002
#endif /* SSYS */

#define p_rcred         p_ucred

#define	VN_RELE(vp)	vrele(((struct vnode *)(vp)))
#define	VN_HOLD(vp)	VREF(((struct vnode *)(vp)))

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
enum vcexcl { NONEXCL, EXCL };

#ifdef KERNEL
#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif
#endif /* KERNEL */

#endif /* ! ASSEMBLER & ! __LANGUAGE_ASSEMBLY__ */
#endif /* _KERNEL */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */


#endif /* !defined(UKERNEL) */

/* global defines */

#define OSI_NBSD_ENV        1
#define OSI_NBSD15_ENV      1

#endif /* AFS_PARAM_COMMON_H */
