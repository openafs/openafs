#ifndef UKERNEL
/* This section for kernel libafs compiles only */

/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

#define	AFS_OSF_ENV	1
#define	AFS_OSF20_ENV	1
#define	AFS_OSF30_ENV	1
#define	AFS_OSF32_ENV	1
#define	AFS_OSF32C_ENV	1
#define	AFS_DUX40_ENV	1
#define	AFS_DUX50_ENV	1
#define AFS_DUX51_ENV   1
#define	__alpha		1
#define	AFS_ALPHA_ENV	1
#define	AFS_DECOSF_ENV	1
#define AFS_BOZONLOCK_ENV       1
#define	AFS_64BIT_ENV	1
#define AFS_64BIT_CLIENT	1

#include <afs/afs_sysnames.h>

#define AFS_VM_RDWR_ENV	1
#define AFS_VFS_ENV	1
#define AFS_GREEDY43_ENV	1
#define AFS_ENV  	1
#define AFS_MINPHYS_ENV	1
#define CMUSTD_ENV	1

#define AFS_SYSCALL	232
#define AFS_MOUNT_AFS	13

#ifndef	MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_AFS
#endif
#define SYS_NAME	"alpha_dux51"
#define SYS_NAME_ID	SYS_NAME_ID_alpha_dux51

#define AFS_HAVE_FFS            1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#define AFS_GCPAGS		1	/* if nonzero, garbage collect PAGs */

#ifdef AFS_NAMEI_ENV
#define AFS_64BIT_IOPS_ENV     1	/* needed for NAMEI... */
#else
#define AFS_3DISPARES   1	/* Utilize the 3 available disk inode spares */
#endif

#define AFS_USE_GETTIMEOFDAY 1	/* use gettimeofday to implement rx clock */

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
#include <machine/endian.h>
#if	BYTE_ORDER == BIG_ENDIAN
#define	AFSBIG_ENDIAN		1
#else
#if	BYTE_ORDER == LITTLE_ENDIAN
#define	AFSLITTLE_ENDIAN	1
#else
#error	machine/endian.h must define BYTE_ORDER!
#endif
#endif
#endif /* ! ASSEMBLER && ! __LANGUAGE_ASSEMBLY__ */

#define NEARINODE_HINT  1	/* hint to ufs module to scatter inodes on disk */
#define nearInodeHash(volid, hval) {                          \
                unsigned char*  ts = (unsigned char*)&(volid);\
                for ((hval)=0; ts<(unsigned char*)&(volid)+sizeof(volid);ts++){\
                    (hval) *= 173;                      \
                    (hval) += *ts;                      \
                }                                       \
                }

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
#define vfs_bsize	m_stat.f_bsize
#define vfs_fsid	m_stat.f_fsid
#define va_nodeid	va_fileid
#define vfs_vnodecovered m_vnodecovered
#define direct		dirent
#define vnode_t		struct vnode

#define	VN_RELE(vp)	vrele(((struct vnode *)(vp)))
#define	VN_HOLD(vp)	VREF(((struct vnode *)(vp)))

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
enum vcexcl { NONEXCL, EXCL };

#include <net/net_globals.h>

#endif /* ! ASSEMBLER & ! __LANGUAGE_ASSEMBLY__ */

#define memset(A, B, S) bzero(A, S)
#define memcpy(B, A, S) bcopy(A, B, S)
#define memcmp(A, B, S) bcmp(A, B, S)
#endif /* _KERNEL */

#endif /* AFS_PARAM_H */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

#define AFS_VFS_ENV	1
/* Used only in vfsck code; is it needed any more???? */
#define RXK_LISTENER_ENV	1
#define AFS_USERSPACE_IP_ADDR	1
#define AFS_GCPAGS		0	/* if nonzero, garbage collect PAGs */

#define UKERNEL			1	/* user space kernel */
#define AFS_GREEDY43_ENV	1	/* Used only in rx/rx_user.c */
#define AFS_64BIT_ENV		1
#define AFS_ENV			1
#define AFS_USR_OSF_ENV		1
#define AFS_USR_DUX40_ENV	1

#include <afs/afs_sysnames.h>

													       /*#define AFS_GLOBAL_SUNLOCK    1 *//* For global locking */

#define	AFS_3DISPARES		1	/* Utilize the 3 available disk inode 'spares' */
#define	AFS_SYSCALL		232

/* File system entry (used if mount.h doesn't define MOUNT_AFS */
#define AFS_MOUNT_AFS	 1

/* Machine / Operating system information */
#define sys_alpha_dux40	1
#define SYS_NAME	"alpha_dux51"
#define SYS_NAME_ID	SYS_NAME_ID_alpha_dux51
#define AFS_HAVE_FFS            1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't support statvfs */
#define AFSLITTLE_ENDIAN        1

/* Extra kernel definitions (from kdefs file) */
#ifdef KERNEL
#define	AFS_UIOFMODE		1	/* Only in afs/afs_vnodeops.c (afs_ustrategy) */
#define	AFS_SYSVLOCK		1	/* sys v locking supported */
/*#define	AFS_USEBUFFERS	1*/
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	1
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	MCLBYTES
#define	AFS_MINCHANGE	2
#define	VATTR_NULL	usr_vattr_null
#endif /* KERNEL */
#define	AFS_DIRENT
#ifndef CMSERVERPREF
#define CMSERVERPREF
#endif
#define	ROOTINO		UFSROOTINO

#endif /* AFS_PARAM_H */

#endif /* !defined(UKERNEL) */


#ifndef AFS_PARAM_COMMON_H
#define AFS_PARAM_COMMON_H

/* global defines */

#define	OSI_OSF_ENV	1
#define	OSI_OSF20_ENV	1
#define	OSI_OSF30_ENV	1
#define	OSI_OSF32_ENV	1
#define	OSI_OSF32C_ENV	1
#define	OSI_DUX40_ENV	1
#define	OSI_DUX50_ENV	1
#define OSI_DUX51_ENV   1

#endif /* AFS_PARAM_COMMON_H */
