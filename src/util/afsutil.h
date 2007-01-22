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
#include <afs/param.h>

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

extern int LogLevel;
extern int mrafsStyleLogs;
#ifndef AFS_NT40_ENV
extern int serverLogSyslog;
extern int serverLogSyslogFacility;
extern char *serverLogSyslogTag;
#endif
extern void vFSLog(const char *format, va_list args);
extern void SetLogThreadNumProgram(int (*func) () );

/*@printflike@*/ extern void FSLog(const char *format, ...);
#define ViceLog(level, str)  if ((level) <= LogLevel) (FSLog str)
#define vViceLog(level, str) if ((level) <= LogLevel) (vFSLog str)

extern int OpenLog(const char *filename);
extern int ReOpenLog(const char *fileName);
extern void SetupLogSignals(void);

extern int
afs_vsnprintf( /*@out@ */ char *p, size_t avail, const char *fmt,
	      va_list ap)
    /*@requires maxSet(p) >= (avail-1)@ */
    /*@modifies p@ */ ;

     extern /*@printflike@ */ int
       afs_snprintf( /*@out@ */ char *p, size_t avail,
		    const char *fmt, ...)
    /*@requires maxSet(p) >= (avail-1)@ */
    /*@modifies p@ */ ;


/* special version of ctime that clobbers a *different static variable, so
 * that ViceLog can call ctime and not cause buffer confusion.
 */
     extern char *vctime(const time_t * atime);

/* Need a thead safe ctime for pthread builds. Use std ctime for LWP */
#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
#ifdef OSI_SUN5_ENV
#define afs_ctime(C, B, L) ctime_r(C, B, L)
#else /* !OSI_SUN5_ENV */
/* Cast is for platforms which do not prototype ctime_r */
#define afs_ctime(C, B, L) (char*)ctime_r(C, B)
#endif /* OSI_SUN5_ENV */
#else /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */
#define afs_ctime(C, B, S) \
	((void)strncpy(B, ctime(C), (S-1)), (B)[S-1] = '\0', (B))
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

/* regular expression parser for NT */
     extern char *re_comp(char *sp);
     extern int rc_exec(char *p);

/* Abort on error, possibly trapping to debugger or dumping a trace. */
     void afs_NTAbort(void);
#endif /* NT40 */

     typedef char b32_string_t[8];
/* b64_string_t is 8 bytes, in stds.h */
     typedef char lb64_string_t[12];

#ifndef UKERNEL
#include "afs/ktime.h"
#endif
#include "afsutil_prototypes.h"

#endif /* _AFSUTIL_H_ */
