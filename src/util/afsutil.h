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
#define	AFS_IPINVALID 		0xffffffff /* invalid IP address */
#define AFS_IPINVALIDIGNORE	0xfffffffe /* no input given to extractAddr */

/* logging defines
 */
#include <stdio.h>
#include <stdarg.h>
extern int LogLevel;
#ifndef AFS_NT40_ENV
extern int serverLogSyslog;
extern int serverLogSyslogFacility;
#endif
extern void FSLog(const char *format, ...);
#define ViceLog(level, str)  if ((level) <= LogLevel) (FSLog str)

extern int OpenLog(const char *filename);
extern int ReOpenLog(const char *fileName);
extern void SetupLogSignals(void);


/* special version of ctime that clobbers a *different static variable, so
 * that ViceLog can call ctime and not cause buffer confusion.
 */
extern char *vctime(const time_t *atime);

/* Need a thead safe ctime for pthread builds. Use std ctime for LWP */
#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
#ifdef AFS_SUN5_ENV
#define afs_ctime(C, B, L) ctime_r(C, B, L)
#else
/* Cast is for platforms which do not prototype ctime_r */
#define afs_ctime(C, B, L) (char*)ctime_r(C, B)
#endif /* AFS_SUN5_ENV */
#else /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */
#define afs_ctime(C, B, S) \
	((void)strncpy(B, ctime(C), (S-1)), (B)[S-1] = '\0', (B))
#endif  /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */


/* Convert a 4 byte integer to a text string. */
extern char*	afs_inet_ntoa(afs_int32 addr);


/* copy strings, converting case along the way. */
extern char *lcstring(char *d, char *s, int n);
extern char *ucstring(char *d, char *s, int n);
extern char *strcompose(char *buf, size_t len, ...);

/* abort the current process. */
#ifdef AFS_NT40_ENV
#define afs_abort() afs_NTAbort()
#else
#define afs_abort() abort()
#endif


#ifdef AFS_NT40_ENV
#include <winsock2.h>

/* Initialize the windows sockets before calling networking routines. */
extern int afs_winsockInit(void);

struct timezone {
    int  tz_minuteswest;     /* of Greenwich */
    int  tz_dsttime;    /* type of dst correction to apply */
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
#endif

/* get temp dir path */
char *gettmpdir(void);

/* Base 32 conversions used for NT since directory names are
 * case-insensitive.
 */
typedef char b32_string_t[8];
char *int_to_base32(b32_string_t s, int a);
int base32_to_int(char *s);

#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
/* base 64 converters for namei interface. Flip bits to differences are
 * early in name.
 */
typedef char lb64_string_t[12];
char *int64_to_flipbase64(b64_string_t s, afs_int64 a);
int64_t flipbase64_to_int64(char *s);
#define int32_to_flipbase64(S, A) int64_to_flipbase64(S, (afs_int64)(A))
#endif

/* This message preserves our ability to license AFS to the U.S. Government
 * more than once.
 */

#define AFS_GOVERNMENT_MESSAGE \
"===================== U.S. Government Restricted Rights ======================\n\
If you are licensing the Software on behalf of the U.S. Government\n\
(\"Government\"), the following provisions apply to you.  If the Software is\n\
supplied to the Department of Defense (\"DoD\"), it is classified as \"Commercial\n\
Computer Software\" under paragraph 252.227-7014 of the DoD Supplement to the\n\
Federal Acquisition Regulations (\"DFARS\") (or any successor regulations)\n\
and the Government is acquiring only the license rights granted herein (the\n\
license rights customarily provided to non-Government users).  If the Software\n\
is supplied to any unit or agency of the Government other than DoD, it is\n\
classified as \"Restricted Computer Software\" and the Government's rights in\n\
the Software are defined in paragraph 52.227-19 of the Federal Acquisition\n\
Regulations (\"FAR\") (or any successor regulations) or, in the case of NASA,\n\
in paragraph 18.52.227-86 of the NASA Supplement in the FAR (or any successor\n\
regulations).\n"

#endif /* _AFSUTIL_H_ */
