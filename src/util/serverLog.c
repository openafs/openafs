/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  serverLog.c     - Server logging                                      */
/*                                                                        */
/*  Information Technology Center                                         */
/*  Date: 05/21/97                                                        */
/*                                                                        */
/*  Function    - These routiens implement logging from the servers       */
/*                                                                        */
/* ********************************************************************** */

#include <afs/param.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <time.h>
#else
#ifdef AFS_AIX_ENV
#include <time.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#endif
#include <afs/procmgmt.h>  /* signal(), kill(), wait(), etc. */
#include <fcntl.h>
#include <afs/stds.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV)
#include <string.h>
#else
#include <strings.h>
#endif
#include "afsutil.h"
#include "fileutil.h"
#if defined(AFS_PTHREAD_ENV)
#include <assert.h>
#include <pthread.h>
static pthread_mutex_t serverLogMutex;
#define LOCK_SERVERLOG() assert(pthread_mutex_lock(&serverLogMutex)==0)
#define UNLOCK_SERVERLOG() assert(pthread_mutex_unlock(&serverLogMutex)==0)

#ifdef AFS_NT40_ENV
#define NULLDEV "NUL"
#else
#define NULLDEV "/dev/null"
#endif

#else /* AFS_PTHREAD_ENV */
#define LOCK_SERVERLOG() 
#define UNLOCK_SERVERLOG() 
#endif  /* AFS_PTHREAD_ENV */

#ifdef AFS_NT40_ENV
#define F_OK 0
#endif

static int serverLogFD = -1;

#include <stdarg.h>
int LogLevel;

/* VARARGS1 */
void FSLog (const char *format, ...)
{
    va_list args;

    time_t currenttime;
    char *timeStamp;
    char tbuffer[1024];
    char *info;
    int len;

    currenttime = time(0);
    timeStamp = afs_ctime(&currenttime, tbuffer, sizeof(tbuffer));
    timeStamp[24] = ' ';  /* ts[24] is the newline, 25 is the null */
    info = &timeStamp[25];

    va_start(args, format);
    (void) vsprintf(info, format, args);
    va_end(args);

    len = strlen(tbuffer);
    LOCK_SERVERLOG();
    if (serverLogFD > 0)
	write(serverLogFD, tbuffer, len);
    UNLOCK_SERVERLOG();

#if !defined(AFS_PTHREAD_ENV)
    fflush(stdout);
    fflush(stderr);     /* in case they're sharing the same FD */
#endif
}

static void DebugOn(int loglevel)
{
    if (loglevel == 0) {
        ViceLog(0, ("Reset Debug levels to 0\n"));
    } else {
        ViceLog(0, ("Set Debug On level = %d\n",loglevel));
    }
} /*DebugOn*/



void SetDebug_Signal(int signo)
{
    extern int IOMGR_SoftSig();

    if (LogLevel > 0) {
        LogLevel *= 5;
    }
    else {
        LogLevel = 1;
    }
#if defined(AFS_PTHREAD_ENV)
    DebugOn(LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, LogLevel);
#endif /* AFS_PTHREAD_ENV */

    signal(signo, SetDebug_Signal);   /* on some platforms, this signal */
				      /* handler needs to be set again */
} /*SetDebug_Signal*/

void ResetDebug_Signal(int signo)
{
    LogLevel = 0;

#if defined(AFS_PTHREAD_ENV)
    DebugOn(LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, LogLevel);
#endif /* AFS_PTHREAD_ENV */

    signal(signo, ResetDebug_Signal);   /* on some platforms, this signal */
					/* handler needs to be set again */
} /*ResetDebug_Signal*/


void SetupLogSignals(void)
{
    signal(SIGHUP, ResetDebug_Signal);
    /* Note that we cannot use SIGUSR1 -- Linux stole it for pthreads! */
    signal(SIGTSTP, SetDebug_Signal);
}

int OpenLog(const char *fileName) 
{
    /*
     * This function should allow various libraries that inconsistently
     * use stdout/stderr to all go to the same place
     */
    int tempfd;
    char oldName[MAXPATHLEN];

    strcpy(oldName, fileName);
    strcat(oldName, ".old");

    /* don't check error */
    renamefile(fileName, oldName);
    tempfd = open(fileName, O_WRONLY|O_TRUNC|O_CREAT, 0666);
    if(tempfd < 0)
    {
	printf("Unable to open log file %s\n", fileName);
	return -1;
    }

#if defined(AFS_PTHREAD_ENV)
    /* redirect stdout and stderr so random printf's don't write to data */
    assert(freopen(NULLDEV, "w", stdout) != NULL);
    assert(freopen(NULLDEV, "w", stderr) != NULL);

    assert(pthread_mutex_init(&serverLogMutex, NULL) == 0);

    serverLogFD = tempfd;
#else
    close(tempfd); /* just checking.... */
    freopen(fileName, "w", stdout);
    freopen(fileName, "w", stderr);
    serverLogFD = fileno(stdout);
#endif /* AFS_PTHREAD_ENV */

    return 0;
} /*OpenLog*/

int ReOpenLog(const char *fileName) 
{
#if !defined(AFS_PTHREAD_ENV)
    int tempfd;
#endif

    if (access(fileName, F_OK)==0)
	return 0; /* exists, no need to reopen. */

#if defined(AFS_PTHREAD_ENV)
    LOCK_SERVERLOG();
    if (serverLogFD > 0)
	close(serverLogFD);
    serverLogFD = open(fileName,O_WRONLY|O_APPEND|O_CREAT, 0666);
    UNLOCK_SERVERLOG();
    return serverLogFD < 0 ? -1 : 0;
#else

    tempfd = open(fileName,O_WRONLY|O_APPEND|O_CREAT, 0666);
    if(tempfd < 0)
    {
	printf("Unable to open log file %s\n", fileName);
	return -1;
    }
    close(tempfd);

    freopen(fileName, "a", stdout);
    freopen(fileName, "a", stderr);
    serverLogFD = fileno(stdout);
  

    return 0;
#endif /* AFS_PTHREAD_ENV */
}
