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
#define BITMAP_LATER		1
#define FAST_RESTART		1

#include <afs/afs_sysnames.h>

/* Global lock in AFS part of client. */
#define AFS_GLOBAL_SUNLOCK 1
#define AFS_GCPAGS		1       /* if nonzero, garbage collect PAGs */

/* File system entry (used if vmount.h doesn't define MNT_AFS */
#define AFS_MOUNT_AFS	4
#define AFS_SYSCALL    31

/* Machine / Operating system information */
#define SYS_NAME	"rs_aix51"
#define SYS_NAME_ID	SYS_NAME_ID_rs_aix51
#define AFSBIG_ENDIAN	1
#define RIOS		1		/* POWERseries 6000. (sj/pc)	*/
#define AFS_VM_RDWR_ENV 1	        /* read/write implemented via VM */
#define AFS_USE_GETTIMEOFDAY 1  /* use gettimeofday to implement rx clock */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#ifndef _POWER
#define _POWER		1		/* _POWERseries!		*/
#endif
#ifndef COMPAT_43
#define COMPAT_43
#endif

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
#define	osi_GTime(x)	time		/* something for the silly time(0)?? */
#define	AFS_KALLOC	kmem_alloc
#define	AFS_KFREE	kmem_free
#define	VATTR_NULL(V)	memset((void*)V, -1, sizeof(*(V)))
#define va_nodeid	va_serialno
#endif /* !_KERNEL	*/
#define	AFS_DIRENT
#endif /* AFS_PARAM_H */
