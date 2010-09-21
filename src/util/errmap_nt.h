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

#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#ifdef ELOOP
#undef ELOOP
#endif
#define ELOOP                   WSAELOOP
#ifdef ENAMETOOLONG
#undef ENAMETOOLONG
#endif
#define ENAMETOOLONG            WSAENAMETOOLONG
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#ifdef ENOTEMPTY
#undef ENOTEMPTY
#endif
#define ENOTEMPTY               WSAENOTEMPTY
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE

/*
 * New codes
 * Highest known value is WSA_QOS_RESERVED_PETYPE (WSABASEERR + 1031)
 */
#define AFS_NT_ERRNO_BASE  WSABASEERR + 1100

#define EOVERFLOW          (AFS_NT_ERRNO_BASE + 0)
#define ENOMSG             (AFS_NT_ERRNO_BASE + 1)
#define ETIME              (AFS_NT_ERRNO_BASE + 2)
#define ENOTBLK		   (AFS_NT_ERRNO_BASE + 3)

#endif /* OPENAFS_ERRMAP_NT_H  */

