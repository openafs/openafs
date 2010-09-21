/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Kill command based on process management library (procmgmt.h).
 * Intended for use on Windows NT, where it will only signal processes
 * linked with the process management library (except SIGKILL).
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "procmgmt.h"


/* Define NULL-terminated array of signal name/number pairs */
typedef struct {
    char *name;
    int number;
} signal_map_t;

static signal_map_t signalMap[] = {
    {"HUP", SIGHUP},
    {"INT", SIGINT},
    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},
    {"ABRT", SIGABRT},
    {"FPE", SIGFPE},
    {"KILL", SIGKILL},
    {"SEGV", SIGSEGV},
    {"TERM", SIGTERM},
    {"USR1", SIGUSR1},
    {"USR2", SIGUSR2},
    {"CLD", SIGCLD},
    {"CHLD", SIGCHLD},
    {"TSTP", SIGTSTP},
    {NULL, 0}
};


static void
PrintSignalList(void)
{
    int i;

    for (i = 0; signalMap[i].name; i++) {
	printf("%s ", signalMap[i].name);
    }
    printf("\n");
}

static int
SignalArgToNumber(const char *sigargp)
{
    long signo;
    char *endp;
    int i;

    /* check for numeric signal arg */
    errno = 0;
    signo = strtol(sigargp, &endp, 0);

    if (errno == 0 && *endp == '\0') {
	return (int)signo;
    }

    /* check for named signal arg */
    for (i = 0; signalMap[i].name; i++) {
	if (!strcasecmp(signalMap[i].name, sigargp)) {
	    return signalMap[i].number;
	}
    }

    return -1;			/* invalid signal argument */
}


static void
PrintUsage(const char *whoami)
{
    printf("usage: %s [-signal] [pid] ...\n", whoami);
    printf("       %s -l\n", whoami);
}



int
main(int argc, char *argv[])
{
    int status = 0;
    char *whoami;

    /* set whoami to last component of argv[0] */
    if ((whoami = strrchr(argv[0], '/')) != NULL) {
	whoami++;
    } else if ((whoami = strrchr(argv[0], '\\')) != NULL) {
	whoami++;
    } else {
	whoami = argv[0];
    }

    /* parse command arguments */
    if (argc <= 1) {
	/* no arguments; print usage statement */
	PrintUsage(whoami);

    } else if (!strcmp(argv[1], "-l")) {
	/* request to print a list of valid signals */
	PrintSignalList();

    } else {
	/* request to send a signal */
	int i;
	int signo = SIGTERM;	/* SIGTERM is default if no signal specified */

	for (i = 1; i < argc; i++) {
	    char *curargp = argv[i];

	    if (i == 1 && *curargp == '-') {
		/* signal argument specified; translate to a number */
		curargp++;	/* advance past signal flag indicator ('-') */

		if ((signo = SignalArgToNumber(curargp)) == -1) {
		    printf("%s: unknown signal %s.\n", whoami, curargp);
		    PrintUsage(whoami);
		    status = 1;
		    break;
		}
	    } else {
		/* pid specified; send signal */
		long pid;
		char *endp;

		errno = 0;
		pid = strtol(curargp, &endp, 0);

		if (errno != 0 || *endp != '\0' || pid <= 0) {
		    printf("%s: invalid pid value %s.\n", whoami, curargp);
		    status = 1;
		} else {
		    if (kill((pid_t) pid, signo)) {
			if (errno == EINVAL) {
			    printf("%s: invalid signal number %d.\n", whoami,
				   signo);
			    status = 1;
			    break;

			} else if (errno == ESRCH) {
			    printf("%s: no such process %d.\n", whoami,
				   (int)pid);
			    status = 1;

			} else if (errno == EPERM) {
			    printf("%s: no privilege to signal %d.\n", whoami,
				   (int)pid);
			    status = 1;

			} else {
			    printf("%s: failed to signal %d (errno = %d).\n",
				   whoami, (int)pid, (int)errno);
			    status = 1;
			}
		    }
		}
	    }
	}
    }

    return status;
}
