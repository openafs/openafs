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
#define ViceLogThenPanic(level, str) \
    do { ViceLog(level, str); osi_Panic str; } while(0);

extern int OpenLog(const char *filename);
extern int ReOpenLog(const char *fileName);
extern void SetupLogSignals(void);
extern void CloseLog(void);
extern void SetupLogSoftSignals(void);

#ifdef AFS_NT40_ENV
#ifndef _MFC_VER
#include <winsock2.h>
#endif /* _MFC_VER */

/* Initialize the windows sockets before calling networking routines. */
     extern int afs_winsockInit(void);
     extern void afs_winsockCleanup(void);

/* Unbuffer output when Un*x would do line buffering. */
#define setlinebuf(S) setvbuf(S, NULL, _IONBF, 0)

#endif /* AFS_NT40_ENV */

#ifndef HAVE_POSIX_REGEX
extern char *re_comp(const char *sp);
extern int re_exec(const char *p1);
#endif

     typedef char b32_string_t[8];
/* b64_string_t is 8 bytes, in stds.h */
     typedef char lb64_string_t[12];

#include <afs/ktime.h>
#include "afsutil_prototypes.h"

#endif /* _AFSUTIL_H_ */
