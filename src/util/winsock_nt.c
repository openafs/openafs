/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* winsock.c contains winsock related routines. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/util/winsock_nt.c,v 1.5.2.2 2006/08/30 01:41:41 jaltman Exp $");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <sys/timeb.h>
#include <afs/afsutil.h>

/* afs_wsInit - LWP version
 * Initialize the winsock 2 environment.
 * 
 * Returns 0 on success, -1 on error.
 */
int
afs_winsockInit(void)
{
	int code;
	WSADATA data;
	WORD sockVersion;

	sockVersion = 2;
	code = WSAStartup(sockVersion, &data);
	if (code)
	    return -1;

	if (data.wVersion != 2)
	    return -1;
    return 0;
}

void
afs_winsockCleanup(void)
{
    WSACleanup();
}

int
afs_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct _timeb myTime;

    _ftime(&myTime);
    tv->tv_sec = myTime.time;
    tv->tv_usec = myTime.millitm * 1000;
    if (tz) {
	tz->tz_minuteswest = myTime.timezone;
	tz->tz_dsttime = myTime.dstflag;
    }
    return 0;
}
#endif
