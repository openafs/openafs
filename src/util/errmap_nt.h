/* Copyright (C) 1998 Transarc Corporation.  All rights reserved.
 *
 */

#ifndef TRANSARC_ERRMAP_NT_H
#define	TRANSARC_ERRMAP_NT_H

/* Declare NT to Unix-ish error translation function */
extern int nterr_nt2unix(long ntErr, int defaultErr);

/* Include native error code definitions */
#include <errno.h>

/* Define additional local codes beyond NT errno range. */

#define AFS_NT_ERRNO_BASE  100

/* Overloaded codes. */
#define EWOULDBLOCK        EAGAIN

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

#endif	/* TRANSARC_ERRMAP_NT_H  */
