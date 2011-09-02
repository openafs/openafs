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

/* Include C Runtime Library error code definitions */
#include <errno.h>

/*
 * Define additional POSIX codes not specified by
 * Visual Studio.  Different versions include different
 * error code definitions.  If an error is not declared
 * by the C Runtime Library, define it using the equivalent
 * Winsock error.  If there is no Winsock equivalent,
 * define an AFS private value in the Winsock space.
 */

/* Overloaded codes. */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK        WSAEWOULDBLOCK
#endif

#ifndef EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#endif
#ifndef EALREADY
#define EALREADY                WSAEALREADY
#endif
#ifndef ENOTSOCK
#define ENOTSOCK                WSAENOTSOCK
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ           WSAEDESTADDRREQ
#endif
#ifndef EMSGSIZE
#define EMSGSIZE                WSAEMSGSIZE
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE              WSAEPROTOTYPE
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT             WSAENOPROTOOPT
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP              WSAEOPNOTSUPP
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#endif
#ifndef EADDRINUSE
#define EADDRINUSE              WSAEADDRINUSE
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#endif
#ifndef ENETDOWN
#define ENETDOWN                WSAENETDOWN
#endif
#ifndef ENETUNREACH
#define ENETUNREACH             WSAENETUNREACH
#endif
#ifndef ENETRESET
#define ENETRESET               WSAENETRESET
#endif
#ifndef ECONNABORTED
#define ECONNABORTED            WSAECONNABORTED
#endif
#ifndef ECONNRESET
#define ECONNRESET              WSAECONNRESET
#endif
#ifndef ENOBUFS
#define ENOBUFS                 WSAENOBUFS
#endif
#ifndef EISCONN
#define EISCONN                 WSAEISCONN
#endif
#ifndef ENOTCONN
#define ENOTCONN                WSAENOTCONN
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN               WSAESHUTDOWN
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS            WSAETOOMANYREFS
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT               WSAETIMEDOUT
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED            WSAECONNREFUSED
#endif
#ifndef ELOOP
#define ELOOP                   WSAELOOP
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG            WSAENAMETOOLONG
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN               WSAEHOSTDOWN
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH            WSAEHOSTUNREACH
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY               WSAENOTEMPTY
#endif
#ifndef EPROCLIM
#define EPROCLIM                WSAEPROCLIM
#endif
#ifndef EUSERS
#define EUSERS                  WSAEUSERS
#endif
#ifndef EDQUOT
#define EDQUOT                  WSAEDQUOT
#endif
#ifndef ESTALE
#define ESTALE                  WSAESTALE
#endif
#ifndef EREMOTE
#define EREMOTE                 WSAEREMOTE
#endif

/*
 * New codes
 * Highest known value is WSA_QOS_RESERVED_PETYPE (WSABASEERR + 1031)
 */
#define AFS_NT_ERRNO_BASE  WSABASEERR + 1100

#ifndef EOVERFLOW
#define EOVERFLOW          (AFS_NT_ERRNO_BASE + 0)
#endif
#ifndef ENOMSG
#define ENOMSG             (AFS_NT_ERRNO_BASE + 1)
#endif
#ifndef ETIME
#define ETIME              (AFS_NT_ERRNO_BASE + 2)
#endif
#ifndef ENOTBLK
#define ENOTBLK		   (AFS_NT_ERRNO_BASE + 3)
#endif

#endif /* OPENAFS_ERRMAP_NT_H  */

