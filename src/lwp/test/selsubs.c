/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* selsubs.c - common code for client and server. */
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <sys/stat.h>

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/lwp/test/selsubs.c,v 1.8.2.1 2006/03/20 13:41:32 jaltman Exp $");


#include "lwp.h"
#include "seltest.h"

#ifdef NEEDS_ALLOCFDSET
/* Include these if testing against 32 bit fd_set IOMGR. */
fd_set *
IOMGR_AllocFDSet(void)
{
    fd_set *tmp = (fd_set *) malloc(sizeof(fd_set));
    memset((char *)tmp, 0, sizeof(fd_set));
    return tmp;
}

void
IOMGR_FreeFDSet(fd_set * fds)
{
    free((char *)fds);
}
#endif

/* The TCP spec calls for writing at least one byte of OOB data which is
 * read by the receiver using recv with the MSG_OOB flag set.
 */
void
sendOOB(int fd)
{
    char c = (char)1;

    Log("Sending OOB.\n");
    if (send(fd, &c, 1, MSG_OOB) < 0) {
	Die(1, "sendOOB");
    }
}

void
recvOOB(int fd)
{
    char c;

    Log("Received OOB\n");
    if (recv(fd, &c, 1, MSG_OOB) < 0) {
	Die(1, "recvOOB");
    }
    Log("Handled OOB\n");
}

void
assertNullFDSet(int fd, fd_set * fds)
{
    int i;
    int n = sizeof(*fds) / sizeof(int);
    int *j = (int *)fds;

    if (fd >= 0)
	FD_CLR(fd, fds);

    for (i = 0; i < n; i++)
	assert(j[i] == 0);
}

/* OpenFDs
 *
 * Open file descriptors until file descriptor n or higher is returned.
 */
#include <sys/stat.h>
void
OpenFDs(n)
     int n;
{
    int i;
    struct stat sbuf;
    int fd, lfd;

    lfd = -1;
    for (i = 0; i < n; i++) {
	if (fstat(i, &sbuf) == 0)
	    continue;
	if ((fd = open("/dev/null", 0, 0)) < 0) {
	    if (lfd >= 0) {
		close(lfd);
		return;
	    }
	} else {
	    if (fd >= n) {
		close(fd);
		return;
	    } else {
		lfd = fd;
	    }
	}
    }
}

/* If flag is set, abort. */
void
Die(int flag, char *msg)
{
    char tmp[1024];
    extern char *program;

    (void)sprintf(tmp, "%s: %s: ", program ? program : "", msg);
    perror(tmp);
    fflush(stderr);
    if (flag)
	abort();
    else
	exit(1);
}



void
Log(char *fmt, ...)
{
    va_list args;
    struct timeval now;
    struct timezone tz;
    struct tm *ltime;
    time_t tt;
    int code;
    PROCESS pid;
    extern char *program;

    code = gettimeofday(&now, &tz);
    assert(code == 0);

    tt = now.tv_sec;
    ltime = localtime(&tt);

    LWP_CurrentProcess(&pid);
    fprintf(stderr, "%s 0x%x %02d:%02d:%02d.%d: ", program ? program : "",
	    pid, ltime->tm_hour, ltime->tm_min, ltime->tm_sec, now.tv_usec);

    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fflush(stdout);
    va_end(args);
}
