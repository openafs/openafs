#ifndef	AFS_PARAM_COMMON_H
#define	AFS_PARAM_COMMON_H

#define AFSLITTLE_ENDIAN	1
#define AFS_HAVE_FFS		1	/* Use system's ffs. */

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#ifndef IGNORE_STDS_H
#include <sys/param.h>
#endif

#define AFS_XBSD_ENV		1		/* {Free,Open,Net}BSD */
#define AFS_X86_XBSD_ENV	1

#define AFS_NAMEI_ENV		1	/* User space interface to file system */
#define AFS_64BIT_CLIENT	1
#define AFS_64BIT_IOPS_ENV	1	/* Needed for NAMEI */
#define AFS_FBSD_ENV		1
#define AFS_X86_FBSD_ENV	1
#define AFS_X86_ENV		1
#undef AFS_NONFSTRANS
#define AFS_NONFSTRANS		1
#define FTRUNC O_TRUNC

#define IUPD 0x0010
#define IACC 0x0020
#define ICHG 0x0040
#define IMOD 0x0080

#define IN_LOCK(ip) lockmgr(&ip->i_lock, LK_EXCLUSIVE, \
			    NULL, curproc)
#define IN_UNLOCK(ip) lockmgr(&ip->i_lock, LK_RELEASE, \
			      NULL, curproc)

#include <afs/afs_sysnames.h>

#define AFS_VFS_ENV		1
#define AFS_VFSINCL_ENV		1
#define AFS_GREEDY43_ENV	1
#define AFS_ENV			1

#define AFS_SYSCALL		339

#ifndef MOUNT_UFS
#define MOUNT_UFS "ufs"
#endif

#define RXK_LISTENER_ENV	1
#define AFS_GCPAGS		0	/* if nonzero, garbage collect PAGs */
#define AFS_USE_GETTIMEOFDAY	1	/* use gettimeofday to implement rx clock */

/* Extra kernel definitions (from kdefs file) */
#ifdef _KERNEL
#define AFS_GLOBAL_SUNLOCK	1
#define	AFS_VFS34		1	/* What is VFS34??? */
#define	afsio_iov		uio_iov
#define	afsio_iovcnt		uio_iovcnt
#define	afsio_offset		uio_offset
#define	afsio_seg		uio_segflg
#define	afsio_resid		uio_resid
#define	AFS_UIOSYS		UIO_SYSSPACE
#define	AFS_UIOUSER		UIO_USERSPACE
#define	AFS_CLBYTES		CLBYTES
#define AFS_KALLOC(x)		osi_fbsd_alloc((x), 1)
#define AFS_KFREE(x,y)		osi_fbsd_free((x))
#define	v_count			v_usecount
#define v_vfsp			v_mount
#define vfs_bsize		mnt_stat.f_bsize
#define vfs_fsid		mnt_stat.f_fsid
#define va_nodeid		va_fileid
#define vfs_vnodecovered 	mnt_vnodecovered
#define direct			dirent
#define vnode_t			struct vnode

#ifndef MUTEX_DEFAULT
#define MUTEX_DEFAULT 0
#endif /* MUTEX_DEFAULT */

#ifndef SSYS
#define SSYS 0x00002
#endif /* SSYS */

#define p_rcred p_ucred

# if !defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
enum vcexcl { NONEXCL, EXCL };
# endif /* ! ASSEMBLER & ! __LANGUAGE_ASSEMBLY__ */
#endif /* _KERNEL */

/*
 * Consolidate version checks into configure-test-like definitions
 */

/* r270870 moved if_data into ifnet to avoid namespace-stealing macros. */
#if __FreeBSD_version >= 1100030
#define FBSD_IF_METRIC_IN_STRUCT_IFNET
#endif

/* r271438 changed the ifa_ifwithnet KPI signature. */
#if __FreeBSD_version >= 1100032
#define FBSD_IFA_IFWITHNET_THREE_ARGS
#endif

/* r273707 added a flags argument to syscall_register/syscall_helper_register */
#if __FreeBSD_version >= 1100041
#define FBSD_SYSCALL_REGISTER_TAKES_FLAGS
#endif

/* r285819 eliminated b_saveaddr from struct buf */
#if __FreeBSD_version >= 1100078
#define FBSD_STRUCT_BUF_NO_SAVEADDR
#endif

/* r292373 changed the KPI for VOP_GETPAGES */
#if __FreeBSD_version >= 1100092
#define FBSD_VOP_GETPAGES_BUSIED
#endif

/* r333813 changed network interfaces and addrs to be traversed via
 * CK_STAILQ_FOREACH instead of TAILQ_FOREACH */
#if __FreeBSD_version >= 1200064
# define AFS_FBSD_NET_FOREACH CK_STAILQ_FOREACH
#else
# define AFS_FBSD_NET_FOREACH TAILQ_FOREACH
#endif

/* r342872 changed NET_EPOCH_ENTER() et al to use explicit epoch trackers. */
#if __FreeBSD_version >= 1300008
# define FBSD_NET_ET_EXPLICIT
#endif

/* r343030 removed getpbuf() et al, use UMA alloc instead */
#if __FreeBSD_version >= 1300008
# define FBSD_UMA_GETPBUF
#endif

/* r353868 removed if_addr_rlock et al, use the net epoch instead */
#if __FreeBSD_version >= 1300054
# define AFS_IF_ADDR_RLOCK(ifn) RX_NET_EPOCH_ENTER()
# define AFS_IF_ADDR_RUNLOCK(ifn) RX_NET_EPOCH_EXIT()
#else
# define AFS_IF_ADDR_RLOCK(ifn) if_addr_rlock(ifn)
# define AFS_IF_ADDR_RUNLOCK(ifn) if_addr_runlock(ifn)
#endif

/* r355537 removed VI_DOOMED, use VN_IS_DOOMED instead */
#if __FreeBSD_version >= 1300064
# define AFS_IS_DOOMED(vp) VN_IS_DOOMED(vp)
#else
# define AFS_IS_DOOMED(vp) (((vp)->v_iflag & VI_DOOMED) != 0)
#endif

/* r356337 dropped the 'flags' arg from VOP_UNLOCK */
#if __FreeBSD_version >= 1300074
# define AFS_VOP_UNLOCK(vp) VOP_UNLOCK(vp)
#else
# define AFS_VOP_UNLOCK(vp) VOP_UNLOCK(vp, 0)
#endif

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define UKERNEL			1	/* user space kernel */
#define AFS_ENV			1
#define AFS_VFSINCL_ENV		1
#define AFS_USR_FBSD_ENV	1
#undef  AFS_NONFSTRANS
#define AFS_NONFSTRANS		1

#define AFS_SYSCALL		339
#define AFS_NAMEI_ENV		1	/* User space interface to file system */
#define AFS_64BIT_IOPS_ENV	1	/* Needed for NAMEI */
#define AFS_USERSPACE_IP_ADDR	1
#define RXK_LISTENER_ENV	1
#define AFS_GCPAGS		0	/* if nonzero, garbage collect PAGs */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#include <afs/afs_sysnames.h>

#define	afsio_iov		uio_iov
#define	afsio_iovcnt		uio_iovcnt
#define	afsio_offset		uio_offset
#define	afsio_seg		uio_segflg
#define	afsio_fmode		uio_fmode
#define	afsio_resid		uio_resid
#define	AFS_UIOSYS		1
#define	AFS_UIOUSER		UIO_USERSPACE
#define	AFS_CLBYTES		MCLBYTES
#define	AFS_MINCHANGE		2
#define	VATTR_NULL		usr_vattr_null

#define AFS_DIRENT
#ifndef CMSERVERPREF
#define CMSERVERPREF
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>

#endif /* !defined(UKERNEL) */

#endif /* AFS_COMMON_PARAM_H */
