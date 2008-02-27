/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PARAM_COMMON_H
#define AFS_PARAM_COMMON_H

#ifndef UKERNEL

/* This section for kernel libafs compiles only */
#define AFS_LINUX20_ENV		1
#define AFS_LINUX22_ENV		1
#define AFS_LINUX24_ENV		1
#define AFS_LINUX26_ENV		1

#define AFS_MOUNT_AFS		"afs"	/* The name of the filesystem type */
#define AFS_64BIT_IOPS_ENV	1
#define AFS_NAMEI_ENV		1	/* User space interface to file system */
#define AFS_64BIT_ENV		1
#define AFS_64BIT_CLIENT	1
#define AFS_NONFSTRANS		1
#define AFS_USERSPACE_IP_ADDR	1
#define RXK_LISTENER_ENV	1
#define AFS_GCPAGS		1	/* Set to Userdisabled, allow sysctl to override */
#define AFS_HAVE_FFS		1	/* Use system's ffs */
#define AFS_HAVE_STATVFS	0	/* System doesn't support statvfs */
#define AFS_VM_RDWR_ENV		1	/* read/write implemented via VM */
#define AFS_USE_GETTIMEOFDAY	1	/* use gettimeofday to implement rx clock */

#if defined(__KERNEL__) && !defined(KDUMP_KERNEL)
#define AFS_GLOBAL_SUNLOCK
#endif /* __KERNEL__	&& !DUMP_KERNEL */

#ifdef KERNEL
#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif
#endif /* KERNEL */

#ifndef KERNEL
#define __USE_LARGEFILE64 1
#if !defined off64_t
#define off64_t __off64_t
#endif
#endif

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */
#define AFS_USR_LINUX20_ENV	1
#define AFS_USR_LINUX22_ENV	1
#define AFS_USR_LINUX24_ENV	1
#define AFS_USR_LINUX26_ENV	1

#define AFS_ENV			1
#define AFS_NONFSTRANS 		1
#define AFS_MOUNT_AFS 		"afs"	/* The name of the filesystem type. */
#define AFS_64BIT_IOPS_ENV	1
#define AFS_NAMEI_ENV		1	/* User space interface to file system */
#define AFS_USERSPACE_IP_ADDR 	1
#define RXK_LISTENER_ENV 	1
#define AFS_GCPAGS		0	/* if nonzero, garbage collect PAGs */
#define AFS_HAVE_FFS		1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't support statvfs */
#define AFS_VM_RDWR_ENV		1	/* read/write implemented via VM */

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

#define AFS_DIRENT
#ifndef CMSERVERPREF
#define CMSERVERPREF
#endif

#endif /* !defined(UKERNEL) */

#ifndef AFS_ARM_LINUX24_ENV
#ifdef __GLIBC__
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ > 3)
#define USE_UCONTEXT
#endif
#endif
#endif

#include <afs/afs_sysnames.h>

#endif /* AFS_PARAM_COMMON_H */
