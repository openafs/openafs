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

#define UKERNEL
#define AFS_ENV			1
/* define AFS_USR_XXX_ENV XXX */

#include <afs/afs_sysnames.h>

#define RXK_LISTENER_ENV        1
#define AFS_USERSPACE_IP_ADDR   1
#define AFS_GCPAGS		0       /* if nonzero, garbage collect PAGs */

#define	AFS_3DISPARES		1	/* Utilize the 3 available disk inode 'spares' */
#define	AFS_SYSCALL		31

/* File system entry (used if mount.h doesn't define MOUNT_AFS */
#define AFS_MOUNT_AFS	 1

/* Machine / Operating system information */
#define sys_sun4_413	1
#define SYS_NAME	"sun4_413"
#define SYS_NAME_ID	SYS_NAME_ID_sun4_411
#define AFSBIG_ENDIAN	1
#define AFS_HAVE_FFS    1       /* Use system's ffs. */
#define AFS_HAVE_STATVFS 0

/* Extra kernel definitions (from kdefs file) */
#ifdef KERNEL
/* sun definitions here */
#define	AFS_UIOFMODE		1	/* Only in afs/afs_vnodeops.c (afs_ustrategy) */
#define	afsio_iov		uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg		uio_segflg
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
