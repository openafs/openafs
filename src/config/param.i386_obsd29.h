/*
 * Jim Rees, University of Michigan CITI
 */

#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

#ifndef IGNORE_STDS_H
#include <sys/param.h>
#include <afs/afs_sysnames.h>
#endif

#define AFS_XBSD_ENV 1             /* {Free,Open,Net}BSD */
#define AFS_X86_XBSD_ENV 1

#define AFS_NAMEI_ENV     1   /* User space interface to file system */
#define AFS_64BIT_IOPS_ENV 1  /* Needed for NAMEI */
#define	AFS_OBSD_ENV	1
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

#define AFS_VM_RDWR_ENV	0
#define AFS_VFS_ENV	1
#define AFS_VFSINCL_ENV 1
#define AFS_GREEDY43_ENV	1
#define AFS_ENV  	1

#define AFS_SYSCALL	210
#define AFS_MOUNT_AFS	"afs"

#if 0
#ifndef MOUNT_UFS
#define MOUNT_UFS "ufs"
#endif

#ifndef	MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_AFS
#endif
#endif /* 0 */
#define SYS_NAME	"i386_obsd29"
#define SYS_NAME_ID	2002

#define AFS_HAVE_FFS            1       /* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't supports statvfs */

#define AFS_GCPAGS	        0       /* if nonzero, garbage collect PAGs */
#define AFS_USE_GETTIMEOFDAY    1       /* use gettimeofday to implement rx clock */

#define AFSLITTLE_ENDIAN 1

/* Extra kernel definitions (from kdefs file) */
#ifdef _KERNEL
#define AFS_GLOBAL_SUNLOCK        1
#define	AFS_VFS34	1	/* What is VFS34??? */
#define	AFS_SHORTGID	0	/* are group id's short? */
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	UIO_SYSSPACE
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	MB_CLBYTES
#define	osi_GetTime(x)	microtime(x)
#define	AFS_KALLOC(x)	kalloc(x)
#define	AFS_KFREE(x,y)	kfree(x,y)
#define	v_count		v_usecount
#define v_vfsp		v_mount
#define vfs_bsize	mnt_stat.f_bsize
#define vfs_fsid	mnt_stat.f_fsid
#define va_nodeid	va_fileid
#define vfs_vnodecovered mnt_vnodecovered
#define vnode_t		struct vnode
#define setgroups	sys_setgroups

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
enum vcexcl {NONEXCL, EXCL};

#ifdef KERNEL
#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif
#endif /* KERNEL */

#endif	/* ! ASSEMBLER & ! __LANGUAGE_ASSEMBLY__ */
#endif /* _KERNEL */

#endif	/* AFS_PARAM_H */
