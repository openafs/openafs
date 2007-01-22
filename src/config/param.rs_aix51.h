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

#define	AFS_AIX_ENV		1
#define	AFS_AIX32_ENV		1
#define	AFS_AIX41_ENV		1
#define AFS_AIX42_ENV		1
#define AFS_AIX51_ENV		1

#define AFS_64BIT_ENV		1
#define AFS_64BIT_CLIENT	1
#define AFS_NAMEI_ENV		1
#ifdef AFS_NAMEI_ENV
#define AFS_64BIT_IOPS_ENV	1
#endif

#define AFS_HAVE_FLOCK_SYSID    1

#include <afs/afs_sysnames.h>

/* Global lock in AFS part of client. */
#define AFS_GLOBAL_SUNLOCK 1
#define AFS_GCPAGS		1	/* if nonzero, garbage collect PAGs */

/* File system entry (used if vmount.h doesn't define MNT_AFS */
#define AFS_MOUNT_AFS	4
#define AFS_SYSCALL    31

/* Machine / Operating system information */
#define SYS_NAME	"rs_aix51"
#define SYS_NAME_ID	SYS_NAME_ID_rs_aix51
#define AFSBIG_ENDIAN	1
#define RIOS		1	/* POWERseries 6000. (sj/pc)    */
#define AFS_VM_RDWR_ENV 1	/* read/write implemented via VM */
#define AFS_USE_GETTIMEOFDAY 1	/* use gettimeofday to implement rx clock */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#ifndef _POWER
#define _POWER		1	/* _POWERseries!                */
#endif
#ifndef COMPAT_43
#define COMPAT_43
#endif

#define KERNEL_HAVE_UERROR 1
#define KERNEL_HAVE_PIN 1

/* Extra kernel definitions (from kdefs file) */
#ifdef _KERNEL
#define	AFS_SHORTGID	1
#define	AFS_UIOFMODE	1
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	UIO_SYSSPACE
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	CLBYTES
#define	AFS_MINCHANGE	2
#define osi_GetTime(x)          do {curtime(x); (x)->tv_usec = (x)->tv_usec/1000;} while (0)
#define	osi_GTime(x)	time	/* something for the silly time(0)?? */
#define	AFS_KALLOC	kmem_alloc
#define	AFS_KFREE	kmem_free
#define	VATTR_NULL(V)	memset((void*)V, -1, sizeof(*(V)))
#define va_nodeid	va_serialno
#endif /* !_KERNEL      */
#define	AFS_DIRENT
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

#define AFS_64BIT_ENV           1
#define AFS_64BIT_CLIENT        1
#define AFS_NAMEI_ENV           1
#ifdef AFS_NAMEI_ENV
#define AFS_64BIT_IOPS_ENV	1
#endif
#define BITMAP_LATER            1
#define FAST_RESTART            1

#define AFS_VFS_ENV	1
/* Used only in vfsck code; is it needed any more???? */
#define RXK_LISTENER_ENV	1
#define AFS_USERSPACE_IP_ADDR	1
#define AFS_GCPAGS		0	/* if nonzero, garbage collect PAGs */

#ifdef KERNEL

#define UKERNEL			1	/* user space kernel */
#define AFS_ENV			1
#define AFS_USR_AIX_ENV		1
#define AFS_USR_AIX41_ENV	1
#define AFS_USR_AIX42_ENV	1
#define AFS_USR_AIX51_ENV		1

#else /* KERNEL */

#define	AFS_AIX_ENV		1
#define	AFS_AIX32_ENV		1
#define	AFS_AIX41_ENV		1
#define AFS_AIX42_ENV		1
#define AFS_AIX51_ENV		1

#define AFS_HAVE_FLOCK_SYSID    1

#endif /* KERNEL */

#include <afs/afs_sysnames.h>

													       /*#define AFS_GLOBAL_SUNLOCK    1 *//* For global locking */

#define	AFS_3DISPARES		1	/* Utilize the 3 available disk inode 'spares' */
#define	AFS_SYSCALL		105

/* File system entry (used if mount.h doesn't define MOUNT_AFS */
#define AFS_MOUNT_AFS	 4

/* Machine / Operating system information */
#define sys_rs_aix51	1
#define SYS_NAME	"rs_aix51"
#define SYS_NAME_ID	SYS_NAME_ID_rs_aix51
#define AFSBIG_ENDIAN	1
#define AFS_HAVE_FFS            1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't support statvfs */

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

#endif /* AFS_PARAM_H */

#endif /* !defined(UKERNEL) */


#ifndef AFS_PARAM_COMMON_H
#define AFS_PARAM_COMMON_H      1

/* global defines */

#define	OSI_AIX_ENV		1
#define	OSI_AIX32_ENV		1
#define	OSI_AIX41_ENV		1
#define OSI_AIX42_ENV		1
#define OSI_AIX51_ENV		1

#endif /* AFS_PARAM_COMMON_H */
