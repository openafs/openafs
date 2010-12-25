/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFSUTIL_H_
#define _AFSUTIL_H_

#include <time.h>
/* Include afs installation dir retrieval routines */
#include <afs/dirpath.h>

/* These macros are return values from extractAddr. They do not represent
 * any valid IP address and so can indicate a failure.
 */
#define	AFS_IPINVALID 		0xffffffff	/* invalid IP address */
#define AFS_IPINVALIDIGNORE	0xfffffffe	/* no input given to extractAddr */

/* logging defines
 */
#ifndef AFS_NT40_ENV
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* for inet_ntoa() */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern int LogLevel;
extern int mrafsStyleLogs;
#ifndef AFS_NT40_ENV
extern int serverLogSyslog;
extern int serverLogSyslogFacility;
extern char *serverLogSyslogTag;
#endif
extern void vFSLog(const char *format, va_list args)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 0);

extern void SetLogThreadNumProgram(int (*func) (void) );

extern void FSLog(const char *format, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

#define ViceLog(level, str)  do { if ((level) <= LogLevel) (FSLog str); } while (0)
#define vViceLog(level, str) do { if ((level) <= LogLevel) (vFSLog str); } while (0)

extern int OpenLog(const char *filename);
extern int ReOpenLog(const char *fileName);
extern void SetupLogSignals(void);

extern int
afs_vsnprintf( /*@out@ */ char *p, size_t avail, const char *fmt,
	      va_list ap)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 0)
    /*@requires maxSet(p) >= (avail-1)@ */
    /*@modifies p@ */ ;

extern /*@printflike@ */ int
afs_snprintf( /*@out@ */ char *p, size_t avail, const char *fmt, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 4)
    /*@requires maxSet(p) >= (avail-1)@ */
    /*@modifies p@ */ ;

extern int
afs_vasnprintf (char **ret, size_t max_sz, const char *format, va_list args)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 0);

extern int
afs_vasprintf (char **ret, const char *format, va_list args)
    AFS_ATTRIBUTE_FORMAT(__printf__, 2, 0);

extern int
afs_asprintf (char **ret, const char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 2, 3);

extern int
afs_asnprintf (char **ret, size_t max_sz, const char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 4);

/* special version of ctime that clobbers a *different static variable, so
 * that ViceLog can call ctime and not cause buffer confusion.
 */
extern char *vctime(const time_t * atime);

/* Need a thead safe ctime for pthread builds. Use std ctime for LWP */
#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
#if defined(AFS_SUN5_ENV) && !defined(_POSIX_PTHREAD_SEMANTICS) && (_POSIX_C_SOURCE - 0 < 199506L)
#define afs_ctime(C, B, L) ctime_r(C, B, L)
#else
/* Cast is for platforms which do not prototype ctime_r */
#define afs_ctime(C, B, L) (char*)ctime_r(C, B)
#endif /* AFS_SUN5_ENV */
#else /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */
static_inline char *
afs_ctime(const time_t *C, char *B, size_t S) {
#if !defined(AFS_NT40_ENV) || (_MSC_VER < 1400)
    strncpy(B, ctime(C), (S-1));
    B[S-1] = '\0';
#else
    ctime_s(B, S, C);
#endif
    return B;
}
#endif /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */


/* abort the current process. */
#ifdef AFS_NT40_ENV
#define afs_abort() afs_NTAbort()
#else
#define afs_abort() abort()
#endif


#ifdef AFS_NT40_ENV
#ifndef _MFC_VER
#include <winsock2.h>
#endif /* _MFC_VER */

/* Initialize the windows sockets before calling networking routines. */
     extern int afs_winsockInit(void);
     extern void afs_winsockCleanup(void);

     struct timezone {
	 int tz_minuteswest;	/* of Greenwich */
	 int tz_dsttime;	/* type of dst correction to apply */
     };
#define gettimeofday afs_gettimeofday
     int afs_gettimeofday(struct timeval *tv, struct timezone *tz);

/* Unbuffer output when Un*x would do line buffering. */
#define setlinebuf(S) setvbuf(S, NULL, _IONBF, 0)

/* Abort on error, possibly trapping to debugger or dumping a trace. */
     void afs_NTAbort(void);
#endif /* AFS_NT40_ENV */

#ifndef HAVE_POSIX_REGEX
extern char *re_comp(const char *sp);
extern int re_exec(const char *p1);
#endif

     typedef char b32_string_t[8];
/* b64_string_t is 8 bytes, in stds.h */
     typedef char lb64_string_t[12];

#ifndef HAVE_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#include <afs/ktime.h>
#include "afsutil_prototypes.h"

#endif /* _AFSUTIL_H_ */
