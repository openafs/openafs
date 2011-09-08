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

#include <roken.h>

#ifdef AFS_NT40_ENV
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

/* exported from libafsauthent.dll */
int
afs_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    return rk_gettimeofday(tv, tz);
}
#endif
