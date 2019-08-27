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
/*  Function    - These routines implement logging from the servers.      */
/*                                                                        */
/* ********************************************************************** */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */

#include <roken.h>		/* Must come after procmgmt.h */
#ifdef AFS_PTHREAD_ENV
 #include <opr/softsig.h>
#endif
#include <afs/opr.h>
#include "afsutil.h"
#include "fileutil.h"
#include <lwp.h>

#if defined(AFS_PTHREAD_ENV)
#include <pthread.h>
static pthread_once_t serverLogOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t serverLogMutex;
#define LOCK_SERVERLOG() opr_Verify(pthread_mutex_lock(&serverLogMutex) == 0)
#define UNLOCK_SERVERLOG() opr_Verify(pthread_mutex_unlock(&serverLogMutex) == 0)

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

/*!
 * Placeholder function to return dummy thread number.
 */
static int
dummyThreadNum(void)
{
    return -1;
}
static int (*threadNumProgram) (void) = dummyThreadNum;

/* After single-threaded startup, accesses to serverlogFD and
 * serverLogSyslog* are protected by LOCK_SERVERLOG(). */
static int serverLogFD = -1;	/*!< The log file descriptor. */
static struct logOptions serverLogOpts;	/*!< logging options */

int LogLevel;			/*!< The current logging level. */
static int threadIdLogs = 0;	/*!< Include the thread id in log messages when true. */
static int resetSignals = 0;	/*!< Reset signal handlers for the next signal when true. */
static char *ourName = NULL;	/*!< The fully qualified log file path, saved for reopens. */

static int OpenLogFile(const char *fileName);
static void RotateLogFile(void);

/*!
 * Determine if the file is a named pipe.
 *
 * This check is performed to support named pipes as logs by not rotating them
 * and opening them with a non-blocking flags.
 *
 * \param[in] fileName  log file name
 *
 * \returns non-zero if log file is a named pipe.
 */
static int
IsFIFO(const char *fileName)
{
    struct stat statbuf;
    return (lstat(fileName, &statbuf) == 0) && (S_ISFIFO(statbuf.st_mode));
}

/*!
 * Return the current logging level.
 */
int
GetLogLevel(void)
{
    return LogLevel;
}

/*!
 * Return the log destination.
 */
enum logDest
GetLogDest(void)
{
    return serverLogOpts.lopt_dest;
}

/*!
 * Get the log filename for file based logging.
 *
 * An empty string is returned if the log destination is not
 * file based.  The caller must make a copy of the string
 * if it is accessed after the CloseLog.
 */
const char *
GetLogFilename(void)
{
    return serverLogOpts.lopt_dest == logDest_file ? (const char*)ourName : "";
}

/*!
 * Set the function to log thread numbers.
 */
void
SetLogThreadNumProgram(int (*func) (void) )
{
    threadNumProgram = func;
}

/*!
 * Write a block of bytes to the log.
 *
 * Write a block of bytes directly to the log without formatting
 * or prepending a timestamp.
 *
 * \param[in] buf  pointer to bytes to write
 * \param[in] len  number of bytes to write
 */
void
WriteLogBuffer(char *buf, afs_uint32 len)
{
    LOCK_SERVERLOG();
    if (serverLogFD >= 0) {
	if (write(serverLogFD, buf, len) < 0) {
	    /* don't care */
        }
    }
    UNLOCK_SERVERLOG();
}

/*!
 * Get the current thread number.
 */
int
LogThreadNum(void)
{
  return (*threadNumProgram) ();
}

/*!
 * Write a message to the log.
 *
 * \param[in] format  printf-style format string
 * \param[in] args    variable list of arguments
 */
void
vFSLog(const char *format, va_list args)
{
    time_t currenttime;
    char tbuffer[1024];
    char *info;
    size_t len;
    struct tm tm;
    int num;

    currenttime = time(NULL);
    len = strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y ",
		   localtime_r(&currenttime, &tm));
    info = &tbuffer[len];

    if (threadIdLogs) {
	num = (*threadNumProgram) ();
        if (num > -1) {
	    snprintf(info, (sizeof tbuffer) - strlen(tbuffer), "[%d] ",
		     num);
	    info += strlen(info);
	}
    }

    vsnprintf(info, (sizeof tbuffer) - strlen(tbuffer), format, args);

    len = strlen(tbuffer);
    LOCK_SERVERLOG();
#ifdef HAVE_SYSLOG
    if (serverLogOpts.dest == logDest_syslog) {
	syslog(LOG_INFO, "%s", info);
    } else
#endif
    if (serverLogFD >= 0) {
	if (write(serverLogFD, tbuffer, len) < 0) {
	    /* don't care */
        }
    }
    UNLOCK_SERVERLOG();

#if !defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    if (serverLogOpts.dest == logDest_file) {
	fflush(stdout);
	fflush(stderr);		/* in case they're sharing the same FD */
    }
#endif
}				/*vFSLog */

/*!
 * Write a message to the log.
 *
 * \param[in] format  printf-style format specification
 * \param[in] ...     arguments for format specification
 */
void
FSLog(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vFSLog(format, args);
    va_end(args);
}				/*FSLog */

/*!
 * Write the command-line invocation to the log.
 *
 * \param[in] argc      argument count from main()
 * \param[in] argv      argument vector from main()
 * \param[in] progname  program name
 * \param[in] version   program version
 * \param[in] logstring log message string
 * \param[in] log       printf-style log function
 */
void
LogCommandLine(int argc, char **argv, const char *progname,
	       const char *version, const char *logstring,
	       void (*log) (const char *format, ...))
{
    int i, l;
    char *commandLine, *cx;

    opr_Assert(argc > 0);

    for (l = i = 0; i < argc; i++)
	l += strlen(argv[i]) + 1;
    if ((commandLine = malloc(l))) {
	for (cx = commandLine, i = 0; i < argc; i++) {
	    strcpy(cx, argv[i]);
	    cx += strlen(cx);
	    *(cx++) = ' ';
	}
	commandLine[l-1] = '\0';
	(*log)("%s %s %s%s(%s)\n", logstring, progname,
		    version, strlen(version)>0?" ":"", commandLine);
	free(commandLine);
    } else {
	/* What, we're out of memory already!? */
	(*log)("%s %s%s%s\n", logstring,
	      progname, strlen(version)>0?" ":"", version);
    }
}

/*!
 * Write the single-DES deprecation warning to the log.
 */
void
LogDesWarning(void)
{
    /* The blank newlines help this stand out a bit more in the log. */
    ViceLog(0, ("\n"));
    ViceLog(0, ("WARNING: You are using single-DES keys in a KeyFile. Using single-DES\n"));
    ViceLog(0, ("WARNING: long-term keys is considered insecure, and it is strongly\n"));
    ViceLog(0, ("WARNING: recommended that you migrate to stronger encryption. See\n"));
    ViceLog(0, ("WARNING: OPENAFS-SA-2013-003 on http://www.openafs.org/security/\n"));
    ViceLog(0, ("WARNING: for details.\n"));
    ViceLog(0, ("\n"));
}

/*!
 * Move the current log file out of the way so a new one can be started.
 *
 * The format of the new name depends on the logging style.  The traditional
 * Transarc style appends ".old" to the log file name.  When MR-AFS style
 * logging is in effect, a time stamp is appended to the log file name instead
 * of ".old".
 *
 * \bug  Unfortunately, no check is made to avoid overwriting
 *       old logs in the traditional Transarc mode.
 *
 * \param fileName  fully qualified log file path
 */
static void
RenameLogFile(const char *fileName)
{
    int code;
    char *nextName = NULL;
    int tries;
    time_t t;
    struct stat buf;
    struct tm *timeFields;

    switch (serverLogOpts.lopt_rotateStyle) {
    case logRotate_none:
	break;
    case logRotate_old:
	code = asprintf(&nextName, "%s.old", fileName);
	if (code < 0) {
	    nextName = NULL;
	}
	break;
    case logRotate_timestamp:
	time(&t);
	for (tries = 0; nextName == NULL && tries < 100; t++, tries++) {
	    timeFields = localtime(&t);
	    code = asprintf(&nextName, "%s.%d%02d%02d%02d%02d%02d",
			    fileName, timeFields->tm_year + 1900,
			    timeFields->tm_mon + 1, timeFields->tm_mday,
			    timeFields->tm_hour, timeFields->tm_min,
			    timeFields->tm_sec);
	    if (code < 0) {
		nextName = NULL;
		break;
	    }
	    if (lstat(nextName, &buf) == 0) {
		/* Avoid clobbering a log. */
		free(nextName);
		nextName = NULL;
	    }
	}
	break;
    default:
	opr_Assert(0);
    }
    if (nextName != NULL) {
	rk_rename(fileName, nextName);	/* Don't check the error code. */
	free(nextName);
    }
}

/*!
 * Write message to the log to indicate the log level.
 *
 * This helper function is called by the signal handlers when the log level is
 * changed, to write a message to the log to indicate the log level has been
 * changed.
 */
static void*
DebugOn(void *param)
{
    int loglevel = (intptr_t)param;
    if (loglevel == 0) {
	ViceLog(0, ("Reset Debug levels to 0\n"));
    } else {
	ViceLog(0, ("Set Debug On level = %d\n", loglevel));
    }
    return 0;
}				/*DebugOn */

/*!
 * Signal handler to increase the logging level.
 *
 * Increase the current logging level to 1 if it in currently 0,
 * otherwise, increase the current logging level by a factor of 5 if it
 * is currently non-zero.
 *
 * Enables thread id logging when the log level is greater than 1.
 */
void
SetDebug_Signal(int signo)
{
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
#if defined(AFS_PTHREAD_ENV)
    DebugOn((void *)(intptr_t)LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, (void *)(intptr_t)LogLevel);
#endif /* AFS_PTHREAD_ENV */

    if (resetSignals) {
	/* When pthreaded softsig handlers are not in use, some platforms
	 * require this signal handler to be set again. */
	(void)signal(signo, SetDebug_Signal);
    }
}				/*SetDebug_Signal */

/*!
 * Signal handler to reset the logging level.
 *
 * Reset the logging level and disable thread id logging.
 *
 * \note This handler has the side-effect of rotating and reopening
 *       MR-AFS style logs.
 */
void
ResetDebug_Signal(int signo)
{
    LogLevel = 0;

#if defined(AFS_PTHREAD_ENV)
    DebugOn((void *)(intptr_t)LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, (void *)(intptr_t)LogLevel);
#endif /* AFS_PTHREAD_ENV */

    if (resetSignals) {
	/* When pthreaded softsig handlers are not in use, some platforms
	 * require this signal handler to be set again. */
	(void)signal(signo, ResetDebug_Signal);
    }
#if defined(AFS_PTHREAD_ENV)
    if (threadIdLogs == 1)
        threadIdLogs = 0;
#endif
    if (serverLogOpts.lopt_rotateOnReset) {
	RotateLogFile();
    }
}				/*ResetDebug_Signal */

/*!
 * Handle requests to reopen the log.
 *
 * This signal handler will reopen the log file. A new, empty log file
 * will be created if the log file does not already exist.
 *
 * External log rotation programs may rotate a server log file by
 * renaming the existing server log file and then immediately sending a
 * signal to the corresponding server process.  Server log messages will
 * continue to be appended to the renamed server log file until the
 * server log is reopened.  After this signal handler completes, server
 * log messages will be written to the new log file.  This allows
 * external log rotation programs to rotate log files without
 * messages being dropped.
 */
void
ReOpenLog_Signal(int signo)
{
    ReOpenLog();
    if (resetSignals) {
	(void)signal(signo, ReOpenLog_Signal);
    }
}

#ifdef AFS_PTHREAD_ENV
/*!
 * Register pthread-safe signal handlers for server log management.
 *
 * \note opr_softsig_Init() must be called before this function.
 */
void
SetupLogSoftSignals(void)
{
    opr_softsig_Register(SIGHUP, ResetDebug_Signal);
    opr_softsig_Register(SIGTSTP, SetDebug_Signal);
    opr_softsig_Register(SIGUSR1, ReOpenLog_Signal);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif
}
#endif /* AFS_PTHREAD_ENV */

/*!
 * Register signal handlers for server log management.
 *
 * \note This function is deprecated and should not be used
 *       in new code. This function should be removed when
 *       all the servers have been converted to pthreads
 *       and lwp has been removed.
 */
void
SetupLogSignals(void)
{
    resetSignals = 1;
    (void)signal(SIGHUP, ResetDebug_Signal);
    (void)signal(SIGTSTP, SetDebug_Signal);
    (void)signal(SIGUSR1, ReOpenLog_Signal);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif
}

#if defined(AFS_PTHREAD_ENV)
static void
InitServerLogMutex(void)
{
    opr_Verify(pthread_mutex_init(&serverLogMutex, NULL) == 0);
}
#endif /* AFS_PTHREAD_ENV */

/*!
 * Redirect stdout and stderr to the log file.
 *
 * \note Call directly after opening the log file.
 *
 * \param[in] fileName  log file name
 */
static void
RedirectStdStreams(const char *fileName)
{
    if (freopen(fileName, "a", stdout) == NULL) {
	/* don't care */
    }
    if (freopen(fileName, "a", stderr) != NULL) {
#ifdef HAVE_SETVBUF
	setvbuf(stderr, NULL, _IONBF, 0);
#else
	setbuf(stderr, NULL);
#endif
    }
}

/*!
 * Open the log file.
 *
 * Open the log file using the options given in OpenLog().
 *
 * \returns 0 on success
 */
static int
OpenLogFile(const char *fileName)
{
    /*
     * This function should allow various libraries that inconsistently
     * use stdout/stderr to all go to the same place
     */
    int tempfd;
    int flags = O_WRONLY | O_CREAT | O_APPEND;

    opr_Assert(serverLogOpts.dest == logDest_file);

    opr_Assert(fileName != NULL);

    if (IsFIFO(fileName)) {
	/* Support named pipes as logs by not rotating them. */
	flags |= O_NONBLOCK;
    } else if (serverLogOpts.lopt_rotateOnOpen) {
	/* Old style logging always started a new log file. */
	flags |= O_TRUNC;
	RenameLogFile(fileName);
    }

    tempfd = open(fileName, flags, 0666);
    if (tempfd < 0) {
	printf("Unable to open log file %s\n", fileName);
	return -1;
    }
    RedirectStdStreams(fileName);

    /* Save our name for reopening. */
    if (ourName != fileName) {
	/* Make a copy if needed */
	free(ourName);
	ourName = strdup(fileName);
	opr_Assert(ourName != NULL);
    }

    serverLogFD = tempfd;

    return 0;
}

/*!
 * Open the log file descriptor or a connection to the system log.
 *
 * This function should be called once during program initialization and
 * must be called before calling FSLog() or WriteLogBuffer().  The
 * fields of the given argument specify the logging destination and
 * various optional features.
 *
 * The lopt_logLevel value specifies the initial logging level.
 *
 * The lopt_dest enum specifies the logging destination; either
 * file based (logDest_file) or the system log (logDest_syslog).
 *
 * File Based Logging
 * ------------------
 *
 * A file will be opened for log messages when the lopt_dest enum is set
 * to logDest_file.  The file specified by lopt_filename will be opened
 * for appending log messages.  A new file will be created if the log
 * file does not exist.
 *
 * The lopt_rotateOnOpen flag specifies whether an existing log file is
 * to be renamed and a new log file created during the call to OpenLog.
 * The lopt_rotateOnOpen flag has no effect if the file given by
 * lopt_filename is a named pipe (fifo).
 *
 * The lopt_rotateOnReset flag specifies whether the log file is renamed
 * and then reopened when the reset signal (SIGHUP) is caught.
 *
 * The lopt_rotateStyle enum specifies how the new log file is renamed when
 * lopt_rotateOnOpen or lopt_rotateOnReset are set. The lopt_rotateStyle
 * may be the traditional Transarc style (logRotate_old) or the MR-AFS
 * style (logRotate_timestamp).
 *
 * When lopt_rotateStyle is set to logRotate_old, the suffix ".old" is
 * appended to the log file name. The existing ".old" log file is
 * removed.
 *
 * When lopt_rotateStyle is set to logRotate_timestamp, a timestamp
 * string is appended to the log file name and existing files are not
 * removed.
 *
 * \note  Messages written to stdout and stderr are redirected to the log
 *        file when file-based logging is in effect.
 *
 * System Logging
 * --------------
 *
 * A connection to the system log (syslog) will be established for log
 * messages when the lopt_dest enum is set to logDest_syslog.
 *
 * The lopt_facility specifies the system log facility to be used when
 * writing messages to the system log.
 *
 * The lopt_tag string specifies the indentification string to be used
 * when writing messages to the system log.
 *
 * \param opts  logging options. A copy of the logging
 *              options will be made before returning to
 *              the caller.
 *
 * \returns 0 on success
 */
int
OpenLog(struct logOptions *opts)
{
    int code;

#if defined(AFS_PTHREAD_ENV)
    opr_Verify(pthread_once(&serverLogOnce, InitServerLogMutex) == 0);
#endif /* AFS_PTHREAD_ENV */

    LogLevel = serverLogOpts.logLevel = opts->logLevel;
    serverLogOpts.dest = opts->dest;
    switch (serverLogOpts.dest) {
    case logDest_file:
	serverLogOpts.lopt_rotateOnOpen = opts->lopt_rotateOnOpen;
	serverLogOpts.lopt_rotateOnReset = opts->lopt_rotateOnReset;
	serverLogOpts.lopt_rotateStyle = opts->lopt_rotateStyle;
	/* OpenLogFile() sets ourName; don't cache filename here. */
	code = OpenLogFile(opts->lopt_filename);
	break;
#ifdef HAVE_SYSLOG
    case logDest_syslog:
	serverLogOpts.lopt_rotateOnOpen = 0;
	serverLogOpts.lopt_rotateOnReset = 0;
	serverLogOpts.lopt_rotateStyle = logRotate_none;
	openlog(opts->lopt_tag, LOG_PID, opts->lopt_facility);
	code = 0;
	break;
#endif
    default:
	opr_Assert(0);
    }
    return code;
}				/*OpenLog */

/*!
 * Reopen the log file descriptor.
 *
 * Reopen the log file descriptor in order to support rotation
 * of the log files.  Has no effect when logging to the syslog.
 *
 * \returns  0 on success
 */
int
ReOpenLog(void)
{
    int flags = O_WRONLY | O_APPEND | O_CREAT;

#ifdef HAVE_SYSLOG
    if (serverLogOpts.dest == logDest_syslog) {
	return 0;
    }
#endif

    LOCK_SERVERLOG();
    if (ourName == NULL) {
	UNLOCK_SERVERLOG();
	return -1;
    }
    if (IsFIFO(ourName)) {
	flags |= O_NONBLOCK;
    }
    if (serverLogFD >= 0)
	close(serverLogFD);
    serverLogFD = open(ourName, flags, 0666);
    if (serverLogFD >= 0) {
        RedirectStdStreams(ourName);
    }
    UNLOCK_SERVERLOG();
    return serverLogFD < 0 ? -1 : 0;
}

/*!
 * Rotate the log file by renaming then truncating.
 */
static void
RotateLogFile(void)
{
    LOCK_SERVERLOG();
    if (ourName != NULL) {
	if (serverLogFD >= 0) {
	    close(serverLogFD);
	    serverLogFD = -1;
	}
	OpenLogFile(ourName);
    }
    UNLOCK_SERVERLOG();
}

/*!
 * Close the server log file.
 *
 * \note Must be preceeded by OpenLog().
 */
void
CloseLog(void)
{
    LOCK_SERVERLOG();

#ifdef HAVE_SYSLOG
    if (serverLogOpts.dest == logDest_syslog) {
	closelog();
    } else
#endif
    {
	if (serverLogFD >= 0) {
	    close(serverLogFD);
	    serverLogFD = -1;
	}
	free(ourName);
	ourName = NULL;
    }
    UNLOCK_SERVERLOG();
}
