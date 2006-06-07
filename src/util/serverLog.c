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

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

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
#include <syslog.h>
#endif
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <fcntl.h>
#include <afs/stds.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <sys/stat.h>
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
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_NT40_ENV
#define F_OK 0
#define O_NONBLOCK 0
#endif

static int
dummyThreadNum(void)
{
    return -1;
}
static int (*threadNumProgram) () = dummyThreadNum;

static int serverLogFD = -1;

#ifndef AFS_NT40_ENV
int serverLogSyslog = 0;
int serverLogSyslogFacility = LOG_DAEMON;
char *serverLogSyslogTag = 0;
#endif

#include <stdarg.h>
int LogLevel;
int mrafsStyleLogs = 0;
static int threadIdLogs = 0;
int printLocks = 0;
static char ourName[MAXPATHLEN];

void
SetLogThreadNumProgram(int (*func) () )
{
    threadNumProgram = func;
}

void
WriteLogBuffer(char *buf, afs_uint32 len)
{
    LOCK_SERVERLOG();
    if (serverLogFD > 0)
	(void)write(serverLogFD, buf, len);
    UNLOCK_SERVERLOG();
}

int
LogThreadNum(void) 
{
  return (*threadNumProgram) ();
}

void
vFSLog(const char *format, va_list args)
{
    time_t currenttime;
    char *timeStamp;
    char tbuffer[1024];
    char *info;
    size_t len;
    int num;

    currenttime = time(0);
    timeStamp = afs_ctime(&currenttime, tbuffer, sizeof(tbuffer));
    timeStamp[24] = ' ';	/* ts[24] is the newline, 25 is the null */
    info = &timeStamp[25];

    if (mrafsStyleLogs || threadIdLogs) {
	num = (*threadNumProgram) ();
        if (num > -1) {
	(void)afs_snprintf(info, (sizeof tbuffer) - strlen(tbuffer), "[%d] ",
			   num);
	info += strlen(info);
    }
    }

    (void)afs_vsnprintf(info, (sizeof tbuffer) - strlen(tbuffer), format,
			args);

    len = strlen(tbuffer);
    LOCK_SERVERLOG();
#ifndef AFS_NT40_ENV
    if (serverLogSyslog) {
	syslog(LOG_INFO, "%s", info);
    } else
#endif
    if (serverLogFD > 0)
	(void)write(serverLogFD, tbuffer, len);
    UNLOCK_SERVERLOG();

#if !defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    if (!serverLogSyslog) {
	fflush(stdout);
	fflush(stderr);		/* in case they're sharing the same FD */
    }
#endif
}				/*vFSLog */

/* VARARGS1 */
/*@printflike@*/
void
FSLog(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vFSLog(format, args);
    va_end(args);
}				/*FSLog */

static int
DebugOn(int loglevel)
{
    if (loglevel == 0) {
	ViceLog(0, ("Reset Debug levels to 0\n"));
    } else {
	ViceLog(0, ("Set Debug On level = %d\n", loglevel));
    }
    return 0;
}				/*DebugOn */



void
SetDebug_Signal(int signo)
{
/*    extern int IOMGR_SoftSig();*/

    if (LogLevel > 0) {
	LogLevel *= 5;

#if defined(AFS_PTHREAD_ENV)
        if (LogLevel > 1 && threadNumProgram != NULL && 
            threadIdLogs == 0) {
            threadIdLogs = 1;
        }
#endif
    } else {
	LogLevel = 1;

#if defined(AFS_PTHREAD_ENV)
        if (threadIdLogs == 1)
            threadIdLogs = 0;
#endif
    }
    printLocks = 2;
#if defined(AFS_PTHREAD_ENV)
    DebugOn(LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, LogLevel);
#endif /* AFS_PTHREAD_ENV */

    (void)signal(signo, SetDebug_Signal);	/* on some platforms, this
						 * signal handler needs to
						 * be set again */
}				/*SetDebug_Signal */

void
ResetDebug_Signal(int signo)
{
    LogLevel = 0;

    if (printLocks > 0)
	--printLocks;
#if defined(AFS_PTHREAD_ENV)
    DebugOn(LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, LogLevel);
#endif /* AFS_PTHREAD_ENV */

    (void)signal(signo, ResetDebug_Signal);	/* on some platforms,
						 * this signal handler
						 * needs to be set
						 * again */
#if defined(AFS_PTHREAD_ENV)
    if (threadIdLogs == 1)
        threadIdLogs = 0;
#endif
    if (mrafsStyleLogs)
	OpenLog((char *)&ourName);
}				/*ResetDebug_Signal */


void
SetupLogSignals(void)
{
    (void)signal(SIGHUP, ResetDebug_Signal);
    /* Note that we cannot use SIGUSR1 -- Linux stole it for pthreads! */
    (void)signal(SIGTSTP, SetDebug_Signal);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif
}

int
OpenLog(const char *fileName)
{
    /*
     * This function should allow various libraries that inconsistently
     * use stdout/stderr to all go to the same place
     */
    int tempfd, isfifo = 0;
    char oldName[MAXPATHLEN];
    struct timeval Start;
    struct tm *TimeFields;
    char FileName[MAXPATHLEN];

#ifndef AFS_NT40_ENV
    struct stat statbuf;

    if (serverLogSyslog) {
	openlog(serverLogSyslogTag, LOG_PID, serverLogSyslogFacility);
	return (0);
    }

    /* Support named pipes as logs by not rotating them */
    if ((lstat(fileName, &statbuf) == 0)  && (S_ISFIFO(statbuf.st_mode))) {
	isfifo = 1;
    }
#endif

    if (mrafsStyleLogs) {
        time_t t;
	TM_GetTimeOfDay(&Start, 0);
        t = Start.tv_sec;	
	TimeFields = localtime(&t);
	if (fileName) {
	    if (strncmp(fileName, (char *)&ourName, strlen(fileName)))
		strcpy((char *)&ourName, (char *)fileName);
	}
	afs_snprintf(FileName, MAXPATHLEN, "%s.%d%02d%02d%02d%02d%02d",
		     ourName, TimeFields->tm_year + 1900,
		     TimeFields->tm_mon + 1, TimeFields->tm_mday,
		     TimeFields->tm_hour, TimeFields->tm_min,
		     TimeFields->tm_sec);
	if (!isfifo)
	    renamefile(fileName, FileName);	/* don't check error code */
	tempfd = open(fileName, O_WRONLY | O_TRUNC | O_CREAT | (isfifo?O_NONBLOCK:0), 0666);
    } else {
	strcpy(oldName, fileName);
	strcat(oldName, ".old");

	/* don't check error */
	if (!isfifo)
	    renamefile(fileName, oldName);
	tempfd = open(fileName, O_WRONLY | O_TRUNC | O_CREAT | (isfifo?O_NONBLOCK:0), 0666);
    }

    if (tempfd < 0) {
	printf("Unable to open log file %s\n", fileName);
	return -1;
    }
    /* redirect stdout and stderr so random printf's don't write to data */
    (void)freopen(fileName, "a", stdout);
    (void)freopen(fileName, "a", stderr);
#ifdef HAVE_SETVBUF
#ifdef SETVBUF_REVERSED
    setvbuf(stderr, _IONBF, NULL, 0);
#else
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
#else
    setbuf(stderr, NULL);
#endif

#if defined(AFS_PTHREAD_ENV)
    assert(pthread_mutex_init(&serverLogMutex, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */

    serverLogFD = tempfd;

    return 0;
}				/*OpenLog */

int
ReOpenLog(const char *fileName)
{
    int isfifo = 0;
#if !defined(AFS_PTHREAD_ENV)
    int tempfd;
#endif
#if !defined(AFS_NT40_ENV)
    struct stat statbuf;
#endif

    if (access(fileName, F_OK) == 0)
	return 0;		/* exists, no need to reopen. */

#if !defined(AFS_NT40_ENV)
    if (serverLogSyslog) {
	return 0;
    }

    /* Support named pipes as logs by not rotating them */
    if ((lstat(fileName, &statbuf) == 0)  && (S_ISFIFO(statbuf.st_mode))) {
	isfifo = 1;
    }
#endif

    LOCK_SERVERLOG();
    if (serverLogFD > 0)
	close(serverLogFD);
    serverLogFD = open(fileName, O_WRONLY | O_APPEND | O_CREAT | (isfifo?O_NONBLOCK:0), 0666);
    if (serverLogFD > 0) {
	(void)freopen(fileName, "a", stdout);
	(void)freopen(fileName, "a", stderr);
#ifdef HAVE_SETVBUF
#ifdef SETVBUF_REVERSED
	setvbuf(stderr, _IONBF, NULL, 0);
#else
	setvbuf(stderr, NULL, _IONBF, 0);
#endif
#else
	setbuf(stderr, NULL);
#endif

    }
    UNLOCK_SERVERLOG();
    return serverLogFD < 0 ? -1 : 0;
}
