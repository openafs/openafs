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

/* Overloaded codes. */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK        WSAEWOULDBLOCK
#endif

#define ETIMEDOUT          WSAETIMEDOUT
#define EDQUOT		   WSAEDQUOT
#define ELOOP              WSAELOOP
#define EOPNOTSUPP         WSAEOPNOTSUPP
#define ENOTSOCK           WSAENOTSOCK
#define ECONNREFUSED       WSAECONNREFUSED
#define ESTALE		   WSAESTALE


/* New codes */
#define AFS_NT_ERRNO_BASE  WSABASEERR + 1000

#define EOVERFLOW          (AFS_NT_ERRNO_BASE + 0)
#define ENOMSG             (AFS_NT_ERRNO_BASE + 1)
#define ETIME              (AFS_NT_ERRNO_BASE + 2)
#define ENOTBLK		   (AFS_NT_ERRNO_BASE + 3)

#endif /* OPENAFS_ERRMAP_NT_H  */
