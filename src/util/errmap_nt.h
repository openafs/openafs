/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_ERRMAP_NT_H
#define	OPENAFS_ERRMAP_NT_H

/* Declare NT to Unix-ish error translation function */
extern int nterr_nt2unix(long ntErr, int defaultErr);

/* Include native error code definitions */
#include <errno.h>

/* Define additional local codes beyond NT errno range. */

#define AFS_NT_ERRNO_BASE  100

/* Overloaded codes. */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK        EAGAIN
#endif

/* New codes */
#define ELOOP              (AFS_NT_ERRNO_BASE + 1)
#define EOPNOTSUPP         (AFS_NT_ERRNO_BASE + 2)
#define EDQUOT		   (AFS_NT_ERRNO_BASE + 3)
#define ENOTSOCK           (AFS_NT_ERRNO_BASE + 4)
#define ETIMEDOUT          (AFS_NT_ERRNO_BASE + 5)
#define ECONNREFUSED       (AFS_NT_ERRNO_BASE + 6)
#define ESTALE		   (AFS_NT_ERRNO_BASE + 7)
#define ENOTBLK		   (AFS_NT_ERRNO_BASE + 8)
#define EOVERFLOW          (AFS_NT_ERRNO_BASE + 9)
#define ENOMSG             (AFS_NT_ERRNO_BASE + 10)
#define ETIME              (AFS_NT_ERRNO_BASE + 11)

#endif /* OPENAFS_ERRMAP_NT_H  */
