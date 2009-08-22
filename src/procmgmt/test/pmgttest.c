/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Test of the process management library */


#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <crtdbg.h>
#else
#include <math.h>
#include <time.h>
#include <sys/time.h>
extern char **environ;
#endif

#include <afs/procmgmt.h>


/* define constants */

#define TEST_CHILD_MAX  20	/* max number of active child procs in test */

#if defined(AFS_NT40_ENV)
/* Test for reset of signal() handler (i.e., unreliable signal() behavior).
 * Note: some Unix systems implement a reliable form of signal(), some
 *       do not; NT does not.  Only test the NT behavior because we trust
 *       the Unix implementation.
 */
#define TEST_SIGNAL_RESET
#endif


/* define globals */

static volatile int lastSignalCaught;	/* last signo caught by sig handler */
static volatile int chldSignalCaught;	/* SIGCHLD caught by sig handler */

static pid_t childPid[TEST_CHILD_MAX];	/* pids of active child procs */

static char spawntestDataBuffer[] = "Four score and seven years ago...";


/* define arguments for child processes
 *
 * format:
 *     argv[0] - exe name
 *     argv[1] - CHILD_ARG1
 *     argv[2] - test name
 *     argv[3...n] - arguments for specified test (argv[2])
 */
#define CHILD_ARG_BAD   255	/* child got bad args for test (exit status) */
#define CHILD_EXEC_FAILED   254	/* child failed exec() (Unix only) */

#define CHILD_ARG1  "cHiLd"	/* indicates that proc is a child */

#define SPAWNTEST_ARG2  "spawntest"	/* spawn test child */
#define SPAWNTEST_ARG_MAX 4	/* must match SPAWNTEST_ARG[] count */
static char *SPAWNTEST_ARG[] = { "fred and wilma",
    "barney and betty",
    "",				/* test empty string arg */
    "flintstone and rubble"
};

#define SPAWNTEST_ENV_NAME      "PMGT_SPAWNTEST"
#define SPAWNTEST_ENV_VALUE     "bambam"
#define SPAWNTEST_ENV_SETSTR    SPAWNTEST_ENV_NAME "=" SPAWNTEST_ENV_VALUE

#define SPAWNBUFTEST_ARG2  "spawnbuftest"	/* spawn with buffer test child */

#define WAITTEST_ARG2  "waittest"	/* wait test child */

#define WNOHANGTEST_ARG2  "wnohangtest"	/* wait w/ WNOHANG test child */

#define SIGNALTEST_ARG2  "signaltest"	/* signal test child */

#define ABORTTEST_ARG2  "aborttest"	/* abort test child */


/* define utility functions */

/*
 * TimedSleep() -- put thread to sleep for specified number of seconds.
 */
#define FOREVER 0xFFFFFFFF

static void
TimedSleep(unsigned sec)
{
#ifdef AFS_NT40_ENV
    if (sec == FOREVER) {
	Sleep(INFINITE);
    } else {
	Sleep(sec * 1000);
    }
#else
    if (sec == FOREVER) {
	while (1) {
	    select(0, 0, 0, 0, 0);
	}
    } else {
	time_t timeStart = time(NULL);
	struct timeval sleeptime;

	sleeptime.tv_sec = sec;
	sleeptime.tv_usec = 0;

	while (1) {
	    if (select(0, 0, 0, 0, &sleeptime) == 0) {
		/* timeout */
		break;
	    } else {
		/* returned for reason other than timeout */
		double cumSec = difftime(time(NULL), timeStart);
		double remSec = (double)sec - cumSec;

		if (remSec <= 0.0) {
		    break;
		}
		sleeptime.tv_sec = ceil(remSec);
	    }
	}
    }
#endif
}


/*
 * Bailout() -- cleanup and exit test; parent only.
 */
static void
Bailout(void)
{
    int i;

    /* kill any child processes */
    for (i = 0; i < TEST_CHILD_MAX; i++) {
	if (childPid[i] > (pid_t) 0) {
	    (void)kill(childPid[i], SIGKILL);
	}
    }

    printf("\nAbandoning test due to error\n");
    exit(1);
}


/*
 * ChildTableLookup() -- lookup child in childPid[] table and return index.
 *
 * Find specified child, or any child if pid is (pid_t)-1.
 */
static int
ChildTableLookup(pid_t pid)
{
    int i;

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	if ((childPid[i] > (pid_t) 0)
	    && (pid == (pid_t) - 1 || childPid[i] == pid)) {
	    break;
	}
    }

    if (i >= TEST_CHILD_MAX) {
	/* not found */
	i = -1;
    }
    return i;
}


/*
 * ChildTableClear() -- clear childPid[] table.
 */
static void
ChildTableClear(void)
{
    int i;

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	childPid[i] = (pid_t) - 1;
    }
}

/*
 * Signal catching routine.
 */
static void
SignalCatcher(int signo)
{
    lastSignalCaught = signo;

    if (signo == SIGCHLD) {
	chldSignalCaught = 1;
    }
}


/*
 * Basic API test -- single threaded, no child processes.
 */
static void
BasicAPITest(void)
{
    sigset_t sigSet;
    void (*sigDisp) (int);
    struct sigaction newAction, oldAction;

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    printf("\n\nBASIC API TEST: SINGLE THREADED, NO CHILDREN\n\n");

    /* -------------------------------------------------------------- */

    printf("Testing signal set (sigset_t) manipulation functions\n");

    sigemptyset(&sigSet);

    if (sigismember(&sigSet, SIGHUP) || sigismember(&sigSet, SIGINT)
	|| sigismember(&sigSet, SIGQUIT) || sigismember(&sigSet, SIGILL)
	|| sigismember(&sigSet, SIGABRT) || sigismember(&sigSet, SIGFPE)
	|| sigismember(&sigSet, SIGKILL) || sigismember(&sigSet, SIGSEGV)
	|| sigismember(&sigSet, SIGTERM) || sigismember(&sigSet, SIGUSR1)
	|| sigismember(&sigSet, SIGUSR2) || sigismember(&sigSet, SIGCHLD)
	|| sigismember(&sigSet, SIGTSTP)) {
	printf("sigemptyset() did not clear all defined signals\n");
	Bailout();
    }

    sigfillset(&sigSet);

    if (!sigismember(&sigSet, SIGHUP) || !sigismember(&sigSet, SIGINT)
	|| !sigismember(&sigSet, SIGQUIT) || !sigismember(&sigSet, SIGILL)
	|| !sigismember(&sigSet, SIGABRT) || !sigismember(&sigSet, SIGFPE)
	|| !sigismember(&sigSet, SIGKILL) || !sigismember(&sigSet, SIGSEGV)
	|| !sigismember(&sigSet, SIGTERM) || !sigismember(&sigSet, SIGUSR1)
	|| !sigismember(&sigSet, SIGUSR2) || !sigismember(&sigSet, SIGCHLD)
	|| !sigismember(&sigSet, SIGTSTP)) {
	printf("sigfillset() did not set all defined signals\n");
	Bailout();
    }

    sigaddset(&sigSet, SIGUSR1);
    sigaddset(&sigSet, SIGUSR2);
    sigaddset(&sigSet, SIGINT);

    if (!sigismember(&sigSet, SIGINT) || !sigismember(&sigSet, SIGUSR1)
	|| !sigismember(&sigSet, SIGUSR2)) {
	printf("sigaddset() did not add defined signal to set\n");
	Bailout();
    }

    sigdelset(&sigSet, SIGUSR1);
    sigdelset(&sigSet, SIGUSR2);
    sigdelset(&sigSet, SIGINT);

    if (sigismember(&sigSet, SIGINT) || sigismember(&sigSet, SIGUSR1)
	|| sigismember(&sigSet, SIGUSR2)) {
	printf("sigdelset() did not delete defined signal from set\n");
	Bailout();
    }


    /* -------------------------------------------------------------- */

    printf("Testing signal handler installation (sigaction(), signal())\n");

    newAction.sa_handler = SignalCatcher;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    errno = 0;
    if (!sigaction(SIGKILL, &newAction, NULL)) {
	printf("sigaction() allowed a handler to be set for SIGKILL\n");
	Bailout();
    } else if (errno != EINVAL) {
	printf("sigaction(SIGKILL,...) did not set errno to EINVAL\n");
	Bailout();
    }

    errno = 0;
    if (!sigaction(NSIG, &newAction, NULL)) {
	printf("sigaction() allowed a handler for an invalid signo\n");
	Bailout();
    } else if (errno != EINVAL) {
	printf("sigaction(NSIG,...) did not set errno to EINVAL\n");
	Bailout();
    }

    errno = 0;
    if (signal(SIGKILL, SignalCatcher) != SIG_ERR) {
	printf("signal() allowed a handler to be set for SIGKILL\n");
	Bailout();
    } else if (errno != EINVAL) {
	printf("signal(SIGKILL,...) did not set errno to EINVAL\n");
	Bailout();
    }

    errno = 0;
    if (signal(NSIG, SignalCatcher) != SIG_ERR) {
	printf("signal() allowed a handler to be set for an invalid signo\n");
	Bailout();
    } else if (errno != EINVAL) {
	printf("signal(NSIG,...) did not set errno to EINVAL\n");
	Bailout();
    }

    if (sigaction(SIGTERM, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    if (sigaction(SIGTERM, NULL, &oldAction)) {
	printf("sigaction() failed to retrieve old signal handler\n");
	Bailout();
    } else if (oldAction.sa_handler != newAction.sa_handler
	       || oldAction.sa_flags != newAction.sa_flags) {
	printf("sigaction() returned incorrect old signal handler values\n");
	Bailout();
    }

    if ((sigDisp = signal(SIGTERM, SIG_DFL)) == SIG_ERR) {
	printf("signal() failed to install valid signal handler\n");
	Bailout();
    } else if (sigDisp != newAction.sa_handler) {
	printf("signal() returned incorrect old signal handler\n");
	Bailout();
    }

    if ((sigDisp = signal(SIGTERM, SIG_DFL)) == SIG_ERR) {
	printf("signal() failed to install valid signal handler (2)\n");
	Bailout();
    } else if (sigDisp != SIG_DFL) {
	printf("signal() returned incorrect old signal handler (2)\n");
	Bailout();
    }

    if (sigaction(SIGTERM, NULL, &oldAction)) {
	printf("sigaction() failed to retrieve old signal handler (2)\n");
	Bailout();
    } else if (oldAction.sa_handler != SIG_DFL) {
	printf("sigaction() returned incorrect old signal handler (2)\n");
	Bailout();
    }


    /* -------------------------------------------------------------- */

    printf
	("Testing signal catching (sigaction(), signal(), kill(), raise())\n");

    newAction.sa_handler = SIG_DFL;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    if (raise(SIGCHLD)) {
	printf("raise() failed to send SIGCHLD (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    /* if made it here means SIGCHLD was (correctly) ignored */

    newAction.sa_handler = SignalCatcher;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGTERM, &newAction, NULL)
	|| sigaction(SIGUSR1, &newAction, NULL)
	|| sigaction(SIGUSR2, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler (2)\n");
	Bailout();
    }

    lastSignalCaught = NSIG;

    if (raise(SIGTERM)) {
	printf("raise() failed to send SIGTERM (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGTERM) {
	printf("raise() failed to deliver SIGTERM\n");
	Bailout();
    }

    if (raise(SIGUSR1)) {
	printf("raise() failed to send SIGUSR1 (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGUSR1) {
	printf("raise() failed to deliver SIGUSR1\n");
	Bailout();
    }

    if (raise(SIGUSR2)) {
	printf("raise() failed to send SIGUSR2 (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGUSR2) {
	printf("raise() failed to deliver SIGUSR2\n");
	Bailout();
    }

    if (sigaction(SIGTERM, NULL, &oldAction)) {
	printf("sigaction() failed to retrieve old SIGTERM handler\n");
	Bailout();
    } else if (oldAction.sa_handler != newAction.sa_handler
	       || oldAction.sa_flags != newAction.sa_flags) {
	printf("sigaction() returned incorrect old SIGTERM handler values\n");
	Bailout();
    }

    if (sigaction(SIGUSR1, NULL, &oldAction)) {
	printf("sigaction() failed to retrieve old SIGUSR1 handler\n");
	Bailout();
    } else if (oldAction.sa_handler != newAction.sa_handler
	       || oldAction.sa_flags != newAction.sa_flags) {
	printf("sigaction() returned incorrect old SIGUSR1 handler values\n");
	Bailout();
    }

    if (sigaction(SIGUSR2, NULL, &oldAction)) {
	printf("sigaction() failed to retrieve old SIGUSR2 handler\n");
	Bailout();
    } else if (oldAction.sa_handler != newAction.sa_handler
	       || oldAction.sa_flags != newAction.sa_flags) {
	printf("sigaction() returned incorrect old SIGUSR2 handler values\n");
	Bailout();
    }

    if (signal(SIGTERM, SignalCatcher) == SIG_ERR
	|| signal(SIGUSR1, SignalCatcher) == SIG_ERR
	|| signal(SIGUSR2, SignalCatcher) == SIG_ERR) {
	printf("signal() failed to install valid signal handler (3)\n");
	Bailout();
    }

    lastSignalCaught = NSIG;

    if (kill(getpid(), SIGTERM)) {
	printf("kill() failed to send SIGTERM (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGTERM) {
	printf("kill() failed to deliver SIGTERM\n");
	Bailout();
    }

    if (kill(getpid(), SIGUSR1)) {
	printf("kill() failed to send SIGUSR1 (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGUSR1) {
	printf("kill() failed to deliver SIGUSR1\n");
	Bailout();
    }

    if (kill(getpid(), SIGUSR2)) {
	printf("kill() failed to send SIGUSR2 (errno = %d)\n", errno);
	Bailout();
    }

    TimedSleep(1);		/* wait for signal delivery */

    if (lastSignalCaught != SIGUSR2) {
	printf("kill() failed to deliver SIGUSR2\n");
	Bailout();
    }

    if ((sigDisp = signal(SIGTERM, SIG_DFL)) == SIG_ERR) {
	printf("signal() failed to retrieve old SIGTERM handler\n");
	Bailout();
    } else if (sigDisp != SIG_DFL) {
#ifdef TEST_SIGNAL_RESET
	printf("signal() returned incorrect old SIGTERM handler\n");
	Bailout();
#endif
    }

    if ((sigDisp = signal(SIGUSR1, SIG_DFL)) == SIG_ERR) {
	printf("signal() failed to retrieve old SIGUSR1 handler\n");
	Bailout();
    } else if (sigDisp != SIG_DFL) {
#ifdef TEST_SIGNAL_RESET
	printf("signal() returned incorrect old SIGUSR1 handler\n");
	Bailout();
#endif
    }

    if ((sigDisp = signal(SIGUSR2, SIG_DFL)) == SIG_ERR) {
	printf("signal() failed to retrieve old SIGUSR2 handler\n");
	Bailout();
    } else if (sigDisp != SIG_DFL) {
#ifdef TEST_SIGNAL_RESET
	printf("signal() returned incorrect old SIGUSR2 handler\n");
	Bailout();
#endif
    }

    /*
     * NOTE: not testing effects of sa_mask in sigaction(); the NT process
     *       management library currently serializes signals (to get Unix
     *       signal handling semantics on a uniprocessor) so sa_mask is
     *       effectively ignored (since all signals, except SIGKILL, are
     *       blocked while a signal handler is running).
     */
    printf("\tNOTICE: effectiveness of sigaction()'s sa_mask not tested;\n");
    printf("\tsee comments in test source\n");



    /* -------------------------------------------------------------- */

    printf("Testing childless waiting (wait(), waitpid())\n");

    errno = 0;
    if (waitpid((pid_t) 17, NULL, 0) != -1) {
	printf("waitpid(17,...) with no child did not return -1\n");
	Bailout();
    } else if (errno != ECHILD) {
	printf("waitpid(17,...) with no child did not set errno to ECHILD\n");
	Bailout();
    }

    errno = 0;
    if (waitpid((pid_t) - 1, NULL, 0) != -1) {
	printf("waitpid(-1,...) with no child did not return -1\n");
	Bailout();
    } else if (errno != ECHILD) {
	printf("waitpid(-1,...) with no child did not set errno to ECHILD\n");
	Bailout();
    }

    errno = 0;
    if (wait(NULL) != -1) {
	printf("wait() with no child did not return -1\n");
	Bailout();
    } else if (errno != ECHILD) {
	printf("wait() with no child did not set errno to ECHILD\n");
	Bailout();
    }
}



/*
 * Process management test -- single threaded.
 */
static void
SingleThreadMgmtTest(char *exeName)
{
    struct sigaction newAction;
    char *childArgv[SPAWNTEST_ARG_MAX + 10];
    char childArgStr[50];
    pid_t waitPid;
    int waitStatus, waitIdx;
    int signo;
    int i, j;

    printf("\n\nPROCESS MANAGEMENT TEST:  SINGLE THREADED\n\n");

    /* -------------------------------------------------------------- */

    printf
	("Testing child spawning (spawnprocve(), wait(), WIFEXITED(), WEXITSTATUS())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    /* Set SIGCHLD handler to SIG_DFL. NOTE: on some Unix systems, setting
     * SIGCHLD handler to SIG_IGN changes the semantics of wait()/waitpid().
     */
    newAction.sa_handler = SIG_DFL;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    if (putenv(SPAWNTEST_ENV_SETSTR)) {
	printf("putenv() failed\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = SPAWNTEST_ARG2;

    for (i = 0; i <= SPAWNTEST_ARG_MAX; i++) {
	char countBuf[10];
	sprintf(countBuf, "%d", i);

	childArgv[3] = countBuf;

	for (j = 0; j < i; j++) {
	    childArgv[4 + j] = SPAWNTEST_ARG[j];
	}
	childArgv[4 + j] = NULL;

	if (i % 2 == 0) {
	    childPid[1] =
		spawnprocve(exeName, childArgv, environ, CHILD_EXEC_FAILED);
	} else {
	    childPid[1] = spawnprocv(exeName, childArgv, CHILD_EXEC_FAILED);
	}

	if (childPid[1] == (pid_t) - 1) {
	    printf("spawnprocve(%s,...) failed to start child (errno = %d)\n",
		   exeName, errno);
	    Bailout();
	}

	do {
	    waitPid = wait(&waitStatus);
	} while (waitPid == (pid_t) - 1 && errno == EINTR);

	if (waitPid != childPid[1]) {
	    if (waitPid == (pid_t) - 1) {
		printf("wait() failed getting child status (errno = %d)\n",
		       errno);
	    } else {
		printf
		    ("wait() returned wrong pid (expected = %d, got = %d)\n",
		     (int)childPid[1], (int)waitPid);
	    }
	    Bailout();
	}

	childPid[1] = (pid_t) - 1;	/* clear child pid for Bailout() */

	if (!WIFEXITED(waitStatus)) {
	    printf("WIFEXITED() returned FALSE, expected TRUE\n");
	    Bailout();
	}

	if (WEXITSTATUS(waitStatus) != i) {
	    if (WEXITSTATUS(waitStatus) == CHILD_ARG_BAD) {
		printf("WEXITSTATUS() indicates child got bad args\n");
	    } else if (WEXITSTATUS(waitStatus) == CHILD_EXEC_FAILED) {
		printf("WEXITSTATUS() indicates child exec() failed\n");
	    } else {
		printf("WEXITSTATUS() returned %d, expected %d\n",
		       (int)WEXITSTATUS(waitStatus), i);
	    }
	    Bailout();
	}
    }


    /* -------------------------------------------------------------- */

#if defined(AFS_NT40_ENV)

    printf
	("Testing child spawning with data buffer (spawnprocveb(), wait(), WIFEXITED(), WEXITSTATUS())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    /* Set SIGCHLD handler to SIG_DFL. NOTE: on some Unix systems, setting
     * SIGCHLD handler to SIG_IGN changes the semantics of wait()/waitpid().
     */
    newAction.sa_handler = SIG_DFL;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    if (putenv(SPAWNTEST_ENV_SETSTR)) {
	printf("putenv() failed\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = SPAWNBUFTEST_ARG2;

    for (i = 0; i <= SPAWNTEST_ARG_MAX; i++) {
	char countBuf[10];
	sprintf(countBuf, "%d", i);

	childArgv[3] = countBuf;

	for (j = 0; j < i; j++) {
	    childArgv[4 + j] = SPAWNTEST_ARG[j];
	}
	childArgv[4 + j] = NULL;

	childPid[1] =
	    spawnprocveb(exeName, childArgv, environ, spawntestDataBuffer,
			 sizeof(spawntestDataBuffer));

	if (childPid[1] == (pid_t) - 1) {
	    printf
		("spawnprocveb(%s,...) failed to start child (errno = %d)\n",
		 exeName, errno);
	    Bailout();
	}

	do {
	    waitPid = wait(&waitStatus);
	} while (waitPid == (pid_t) - 1 && errno == EINTR);

	if (waitPid != childPid[1]) {
	    if (waitPid == (pid_t) - 1) {
		printf("wait() failed getting child status (errno = %d)\n",
		       errno);
	    } else {
		printf
		    ("wait() returned wrong pid (expected = %d, got = %d)\n",
		     (int)childPid[1], (int)waitPid);
	    }
	    Bailout();
	}

	childPid[1] = (pid_t) - 1;	/* clear child pid for Bailout() */

	if (!WIFEXITED(waitStatus)) {
	    printf("WIFEXITED() returned FALSE, expected TRUE\n");
	    Bailout();
	}

	if (WEXITSTATUS(waitStatus) != i) {
	    if (WEXITSTATUS(waitStatus) == CHILD_ARG_BAD) {
		printf("WEXITSTATUS() indicates child got bad args\n");
	    } else {
		printf("WEXITSTATUS() returned %d, expected %d\n",
		       (int)WEXITSTATUS(waitStatus), i);
	    }
	    Bailout();
	}
    }

#endif /* AFS_NT40_ENV */


    /* -------------------------------------------------------------- */

    printf
	("Testing child waiting (spawnprocve(), wait(), waitpid(), sigaction(), WIFEXITED(), WEXITSTATUS())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    lastSignalCaught = NSIG;
    chldSignalCaught = 0;

    newAction.sa_handler = SignalCatcher;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = WAITTEST_ARG2;
    childArgv[3] = childArgStr;
    childArgv[4] = NULL;

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	sprintf(childArgStr, "%d", i);

	childPid[i] =
	    spawnprocve(exeName, childArgv, environ, CHILD_EXEC_FAILED);

	if (childPid[i] == (pid_t) - 1) {
	    printf("spawnprocve(%s,...) failed to start child (errno = %d)\n",
		   exeName, errno);
	    Bailout();
	}
    }

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	if (i % 2 == 0) {
	    /* wait() */
	    do {
		waitPid = wait(&waitStatus);
	    } while (waitPid == (pid_t) - 1 && errno == EINTR);

	    if (waitPid == (pid_t) - 1) {
		printf("wait() failed getting child status (errno = %d)\n",
		       errno);
		Bailout();
	    }

	    waitIdx = ChildTableLookup(waitPid);

	    if (waitIdx < 0) {
		printf("wait() returned unknown pid (%d)\n", (int)waitPid);
		Bailout();
	    }
	} else {
	    /* waitpid() */
	    waitIdx = ChildTableLookup((pid_t) - 1);

	    if (waitIdx < 0) {
		printf("Child table unexpectedly empty\n");
		Bailout();
	    }

	    do {
		waitPid = waitpid(childPid[waitIdx], &waitStatus, 0);
	    } while (waitPid == (pid_t) - 1 && errno == EINTR);

	    if (waitPid != childPid[waitIdx]) {
		if (waitPid == (pid_t) - 1) {
		    printf("waitpid() failed getting status (errno = %d)\n",
			   errno);
		} else {
		    printf("waitpid() returned wrong pid "
			   "(expected = %d, got = %d)\n",
			   (int)childPid[waitIdx], (int)waitPid);
		}
		Bailout();
	    }
	}

	childPid[waitIdx] = (pid_t) - 1;	/* clear child pid for Bailout() */

	if (!WIFEXITED(waitStatus)) {
	    printf("WIFEXITED() returned FALSE, expected TRUE\n");
	    Bailout();
	}

	if (WEXITSTATUS(waitStatus) != waitIdx) {
	    if (WEXITSTATUS(waitStatus) == CHILD_ARG_BAD) {
		printf("WEXITSTATUS() indicates child got bad args\n");
	    } else if (WEXITSTATUS(waitStatus) == CHILD_EXEC_FAILED) {
		printf("WEXITSTATUS() indicates child exec() failed\n");
	    } else {
		printf("WEXITSTATUS() returned %d, expected %d\n",
		       (int)WEXITSTATUS(waitStatus), waitIdx);
	    }
	    Bailout();
	}
    }

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    if (!chldSignalCaught) {
	printf("SIGCHLD never caught (last signo = %d)\n", lastSignalCaught);
	Bailout();
    }


    /* -------------------------------------------------------------- */

    printf
	("Testing child waiting with WNOHANG (spawnprocve(), waitpid(), kill())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    /* Set SIGCHLD handler to SIG_DFL. NOTE: on some Unix systems, setting
     * SIGCHLD handler to SIG_IGN changes the semantics of wait()/waitpid().
     */
    newAction.sa_handler = SIG_DFL;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = WNOHANGTEST_ARG2;
    childArgv[3] = NULL;

    childPid[1] = spawnprocve(exeName, childArgv, environ, CHILD_EXEC_FAILED);

    if (childPid[1] == (pid_t) - 1) {
	printf("spawnprocve(%s,...) failed to start child (errno = %d)\n",
	       exeName, errno);
	Bailout();
    }

    for (i = 0; i < 20; i++) {
	do {
	    waitPid = waitpid(childPid[1], &waitStatus, WNOHANG);
	} while (waitPid == (pid_t) - 1 && errno == EINTR);

	if (waitPid != (pid_t) 0) {
	    if (waitPid == (pid_t) - 1) {
		printf("waitpid() failed getting child status (errno = %d)\n",
		       errno);

	    } else {
		printf("waitpid() returned unexpected value (%d)\n", waitPid);
	    }
	    Bailout();
	}
    }

    TimedSleep(2);		/* wait for child to init signal mechanism (NT only) */

    if (kill(childPid[1], SIGKILL)) {
	printf("kill() failed to send SIGKILL (errno = %d)\n", errno);
	Bailout();
    }

    do {
	waitPid = waitpid(childPid[1], &waitStatus, 0);
    } while (waitPid == (pid_t) - 1 && errno == EINTR);

    if (waitPid != childPid[1]) {
	if (waitPid == (pid_t) - 1) {
	    printf("waitpid() failed getting child status (errno = %d)\n",
		   errno);
	} else {
	    printf("waitpid() returned wrong pid (expected = %d, got = %d)\n",
		   (int)childPid[1], (int)waitPid);
	}
	Bailout();
    }

    childPid[1] = (pid_t) - 1;	/* clear child pid for Bailout() */


    /* -------------------------------------------------------------- */

    printf
	("Testing child signaling (spawnprocve(), kill(), waitpid(), sigaction(), WIFSIGNALED(), WTERMSIG())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    lastSignalCaught = NSIG;
    chldSignalCaught = 0;

    newAction.sa_handler = SignalCatcher;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = SIGNALTEST_ARG2;
    childArgv[3] = NULL;

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	childPid[i] =
	    spawnprocve(exeName, childArgv, environ, CHILD_EXEC_FAILED);

	if (childPid[i] == (pid_t) - 1) {
	    printf("spawnprocve(%s,...) failed to start child (errno = %d)\n",
		   exeName, errno);
	    Bailout();
	}
    }

    TimedSleep(4);		/* wait for children to init signal mechanism (NT only) */

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	if (i % 3 == 0) {
	    signo = SIGTERM;
	} else if (i % 3 == 1) {
	    signo = SIGKILL;
	} else {
	    signo = SIGUSR1;
	}

	if (kill(childPid[i], signo)) {
	    printf("kill() failed to send signal (errno = %d)\n", errno);
	    Bailout();
	}
    }

    for (i = 0; i < TEST_CHILD_MAX; i++) {
	if (i % 3 == 0) {
	    signo = SIGTERM;
	} else if (i % 3 == 1) {
	    signo = SIGKILL;
	} else {
	    signo = SIGUSR1;
	}

	do {
	    waitPid = waitpid(childPid[i], &waitStatus, 0);
	} while (waitPid == (pid_t) - 1 && errno == EINTR);

	if (waitPid != childPid[i]) {
	    if (waitPid == (pid_t) - 1) {
		printf("waitpid() failed getting child status (errno = %d)\n",
		       errno);
	    } else {
		printf("waitpid() returned wrong pid "
		       "(expected = %d, got = %d)\n", (int)childPid[i],
		       (int)waitPid);
	    }
	    Bailout();
	}

	childPid[i] = (pid_t) - 1;	/* clear child pid for Bailout() */

	if (!WIFSIGNALED(waitStatus)) {
	    printf("WIFSIGNALED() returned FALSE, expected TRUE\n");
	    Bailout();
	}

	if (WTERMSIG(waitStatus) != signo) {
	    printf("WTERMSIG() returned %d, expected %d\n",
		   (int)WTERMSIG(waitStatus), signo);
	    Bailout();
	}
    }

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    if (!chldSignalCaught) {
	printf("SIGCHLD never caught (last signo = %d)\n", lastSignalCaught);
	Bailout();
    }

    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }


    /* -------------------------------------------------------------- */

    printf
	("Testing child aborting (spawnprocve(), waitpid(), WIFSIGNALED(), WTERMSIG())\n");

    /* clear child pid vector for Bailout() */
    ChildTableClear();

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    lastSignalCaught = NSIG;
    chldSignalCaught = 0;

    newAction.sa_handler = SignalCatcher;
    sigfillset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &newAction, NULL)) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }

    childArgv[0] = exeName;
    childArgv[1] = CHILD_ARG1;
    childArgv[2] = ABORTTEST_ARG2;
    childArgv[3] = NULL;

    childPid[1] = spawnprocve(exeName, childArgv, environ, CHILD_EXEC_FAILED);

    if (childPid[1] == (pid_t) - 1) {
	printf("spawnprocve(%s,...) failed to start child (errno = %d)\n",
	       exeName, errno);
	Bailout();
    }

    TimedSleep(2);		/* wait for child to init signal mechanism (NT only) */

    do {
	waitPid = waitpid(childPid[1], &waitStatus, 0);
    } while (waitPid == (pid_t) - 1 && errno == EINTR);

    if (waitPid != childPid[1]) {
	if (waitPid == (pid_t) - 1) {
	    printf("waitpid() failed getting child status (errno = %d)\n",
		   errno);
	} else {
	    printf("waitpid() returned wrong pid (expected = %d, got = %d)\n",
		   (int)childPid[1], (int)waitPid);
	}
	Bailout();
    }

    childPid[1] = (pid_t) - 1;	/* clear child pid for Bailout() */

    if (!WIFSIGNALED(waitStatus)) {
	printf("WIFSIGNALED() returned FALSE, expected TRUE\n");
	Bailout();
    }

    if (WTERMSIG(waitStatus) != SIGABRT) {
	printf("WTERMSIG() returned %d, expected SIGABRT (%d)\n",
	       (int)WTERMSIG(waitStatus), (int)SIGABRT);
	Bailout();
    }

    TimedSleep(3);		/* wait for outstanding SIGCHLD's to get delivered */

    if (!chldSignalCaught) {
	printf("SIGCHLD never caught (last signo = %d)\n", lastSignalCaught);
	Bailout();
    }

    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
	printf("sigaction() failed to install valid signal handler\n");
	Bailout();
    }
}



/*
 * Parent process behavior.
 */
static void
BehaveLikeAParent(int argc, char *argv[])
{
    BasicAPITest();
    SingleThreadMgmtTest(argv[0]);

    printf("\nAll tests completed successfully.\n");
}


/*
 * Child process behavior.
 */
static void
BehaveLikeAChild(int argc, char *argv[])
{
    int argvCount;
    char *testName;

#ifdef AFS_NT40_ENV
    /* Turn off debug message box used by abort()/assert() in debug
     * version of MSVC C run-time library.
     */
    (void)_CrtSetReportMode(_CRT_WARN, 0);
    (void)_CrtSetReportMode(_CRT_ERROR, 0);
    (void)_CrtSetReportMode(_CRT_ASSERT, 0);
#endif

    /* verify that argc and argv are in sync */
    for (argvCount = 0; argv[argvCount]; argvCount++);

    if (argc != argvCount) {
	exit(CHILD_ARG_BAD);
    }

    /* verify test argument format */

    if (argc < 3) {
	/* all tests require at least argv[2] (test name) */
	exit(CHILD_ARG_BAD);
    }

    /* perform as required for particular test */

    testName = argv[2];

    if (!strcmp(testName, SPAWNTEST_ARG2)) {
	/* SPAWN TEST child */
	int i;
	char *envstr;

	if (((envstr = getenv(SPAWNTEST_ENV_NAME)) == NULL)
	    || (strcmp(envstr, SPAWNTEST_ENV_VALUE))) {
	    exit(CHILD_ARG_BAD);
	}

	if (atoi(argv[3]) != (argc - 4)) {
	    exit(CHILD_ARG_BAD);
	}

	for (i = 0; i < argc - 4; i++) {
	    if (strcmp(argv[4 + i], SPAWNTEST_ARG[i])) {
		exit(CHILD_ARG_BAD);
	    }
	}

#if defined(AFS_NT40_ENV)
	if (spawnDatap != NULL || spawnDataLen != 0) {
	    exit(CHILD_ARG_BAD);
	}
#endif
	exit(i);


#if defined(AFS_NT40_ENV)
    } else if (!strcmp(testName, SPAWNBUFTEST_ARG2)) {
	/* SPAWNBUF TEST child */
	int i;
	char *envstr;

	if (((envstr = getenv(SPAWNTEST_ENV_NAME)) == NULL)
	    || (strcmp(envstr, SPAWNTEST_ENV_VALUE))) {
	    exit(CHILD_ARG_BAD);
	}

	if (atoi(argv[3]) != (argc - 4)) {
	    exit(CHILD_ARG_BAD);
	}

	for (i = 0; i < argc - 4; i++) {
	    if (strcmp(argv[4 + i], SPAWNTEST_ARG[i])) {
		exit(CHILD_ARG_BAD);
	    }
	}

	if (spawnDataLen != sizeof(spawntestDataBuffer)
	    || strcmp(spawnDatap, spawntestDataBuffer)) {
	    exit(CHILD_ARG_BAD);
	}

	exit(i);
#endif /* AFS_NT40_ENV */


    } else if (!strcmp(testName, WAITTEST_ARG2)) {
	/* WAIT TEST child */
	int rc;

	if (argc != 4) {
	    exit(CHILD_ARG_BAD);
	}

	rc = strtol(argv[3], NULL, 10);

	exit(rc);

    } else if (!strcmp(testName, WNOHANGTEST_ARG2)) {
	/* WNOHANG TEST child */
	TimedSleep(FOREVER);
	printf("\tchild unexpectedly returned from TimedSleep(FOREVER)\n");
	exit(1);		/* should never execute */

    } else if (!strcmp(testName, SIGNALTEST_ARG2)) {
	/* SIGNAL TEST child */
	TimedSleep(FOREVER);
	printf("\tchild unexpectedly returned from TimedSleep(FOREVER)\n");
	exit(1);		/* should never execute */

    } else if (!strcmp(testName, ABORTTEST_ARG2)) {
	/* ABORT TEST child */

#ifdef AFS_NT40_ENV
	printf("\tNOTICE:\n");
	printf("\t\tChild is calling abort().\n");
	printf("\t\tAbnormal termination message will appear.\n");
	printf("\t\tA software exception dialog will appear; press 'OK'.\n");
	printf("\t\tAutomatic debugger invocation MUST be disabled.\n");
	fflush(stdout);
#endif
	abort();
	printf("\tchild unexpectedly returned from abort()\n");
	exit(1);		/* should never execute */

    } else {
	/* UNKNOWN TEST */
	exit(CHILD_ARG_BAD);
    }
}


/*
 * Main function.
 */
int
main(int argc, char *argv[])
{
    if (argc == 1) {
	/* PARENT process */
	BehaveLikeAParent(argc, argv);
    } else if ((argc > 1) && !strcmp(argv[1], CHILD_ARG1)) {
	/* CHILD process */
	BehaveLikeAChild(argc, argv);
    } else {
	printf("\nUsage: %s\n", argv[0]);
    }

    return 0;
}
