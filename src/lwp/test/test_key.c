/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * test_key - test waiting for keystrokes.
 * Use this test as:
 * 1) test_key - no arguments should wait until stuff followed by <cr>.
 * 2) test_key -delay n - should wait n seconds and beep then exit. If data
 *    entered before n seconds, should be printed.
 * 3) test_key -delay n -iters j - does the above n times.
 *    
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <time.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <lwp.h>

/* prototypes */
void interTest();
void lineTest();

void
Usage(void)
{
    printf("Usage: test_key [-nobuf] [-delay seconds [-iters n]]\n");
    printf
	("test_key waits for keystrokes. Without options it will wait indefinitely.\n");
    printf
	("To show that LWP is still running there is a rotating '|' which cycles every quarter second.\n");
    printf("-nobuf - don't buffer input. Don't need to wait for <cr>.\n");
    printf("-delay seconds - wait for <seconds> for a keystroke.\n");
    printf("-iters n - repeat the wait n times.\n");
    printf
	("-inter - wait for key and beep every five seconds until pressed(overrides other options).\n");
    printf("-line - wait for a whole line to be typed.");
    exit(1);
}


int waitingForAnswer = 0;

static void
PrintDots()
{
    static struct timeval constSleepTime = { 1, 0 };
    struct timeval sleepTime;

    while (waitingForAnswer) {
	sleepTime = constSleepTime;
	IOMGR_Select(0, 0, 0, 0, &sleepTime);
	if (!waitingForAnswer)
	    break;
	printf(".");
	fflush(stdout);
    }
}

static int
DotWriter(char *junk)
{
    while (1) {
	LWP_WaitProcess(&waitingForAnswer);
	PrintDots();
    }
    return 0;
}

void
main(int ac, char **av)
{
    int delay = 0;
    int iters = 0;
    int inter = 0;
    int line = 0;
    int i;
    PROCESS dotpid;
    int rc;

    for (i = 1; i < ac; i++) {
	if (!strcmp("-delay", av[i])) {
	    if (++i >= ac) {
		printf("Missing delay time for -delay option.\n");
	    }
	    delay = atoi(av[i]);
	    if (delay < 0) {
		printf("Delay must be at least 0 seconds.\n");
		Usage();
	    }
	} else if (!strcmp("-iters", av[i])) {
	    if (++i >= ac) {
		printf("Missing iteration count for -iters option.\n");
	    }
	    iters = atoi(av[i]);
	    if (iters < 0) {
		printf("Number of iterations must be at least 0.\n");
		Usage();
	    }
	} else if (!strcmp("-nobuf", av[i])) {
	    rc = setvbuf(stdin, NULL, _IONBF, 0);
	    if (rc < 0) {
		perror("Setting -nobuf for stdin");
	    }
	} else if (!strcmp("-inter", av[i])) {
	    inter = 1;
	} else if (!strcmp("-line", av[i])) {
	    line = 1;
	} else
	    Usage();
    }

    IOMGR_Initialize();

    LWP_CreateProcess(DotWriter, 32000, LWP_NORMAL_PRIORITY, (char *)0,
		      "DotWriter", &dotpid);

    if (inter) {
	interTest();
	exit(1);
    }
    if (line) {
	lineTest();
	exit(1);
    }
    if (delay == 0) {
	delay = -1;		/* Means wait indefinitely. */
    }
    for (; iters >= 0; iters--) {
	waitingForAnswer = 1;
	LWP_NoYieldSignal(&waitingForAnswer);
	rc = LWP_WaitForKeystroke(delay);
	waitingForAnswer = 0;
	if (rc) {
	    printf("\n'%c'\n", getchar());
	    printf("Flushing remaining input.\n");
	    while (LWP_WaitForKeystroke(0)) {
		printf("'%c'\n", getchar());
	    }
	} else {
	    printf("\nNo data available on this iteration.\n");
	}
    }


}

/* interTest() : wait for key press and beep to remind user every five 
 *    seconds.
*/
void
interTest()
{
    char ch;
    int rc;

    printf("Will print dots until you hit a key!\n");
    /* start the dotwriter thread running */
    waitingForAnswer = 1;
    LWP_NoYieldSignal(&waitingForAnswer);

    do {
	rc = LWP_GetResponseKey(5, &ch);
	if (rc == 0)
	    printf("\a");
    } while (rc == 0);

    waitingForAnswer = 0;	/* turn off dotwriter lwp */

    printf("\nYou typed %c\n", ch);

    return;
}

/* lineTest() : wait until a whole line has been entered before processing.
 */
void
lineTest()
{
    char ch;
    int rc;
    char line[256];

    printf("Will print dots until enter a line\n");
    /* start the dotwriter thread running */
    waitingForAnswer = 1;
    LWP_NoYieldSignal(&waitingForAnswer);

    rc = LWP_GetLine(line, 256);

    waitingForAnswer = 0;

    printf("You entered : %s\n", line);
    if (rc)
	printf("linebuf was too small\n");

    return;
}
