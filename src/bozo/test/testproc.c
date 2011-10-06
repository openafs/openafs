/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ignore = 0;
static int sleepTime = 10;
static int run = 1;
static char *file = NULL;

void
trim(char *s)
{
    char *p = strchr(s, '\n');
    if (p) {
	*p = '\0';
    }
}

void
readfile(void)
{
    FILE *fp;
    int *bogus = NULL;
    char buffer[256];
    if (file) {
	if ((fp = fopen(file, "r")) == NULL) {
	    fprintf(stderr, "Unable to open file %s\n", file);
	    exit(-1);
	}
	fgets(buffer, sizeof(buffer), fp);
	trim(buffer);
	if (strncmp(buffer, "sleep ", 6) == 0) {
	    int t = atoi(buffer + 6);
	    if (t) {
		sleepTime = t;
	    }
	}
	if (strcmp(buffer, "run") == 0) {
	    run = 1;
	}
	if (strcmp(buffer, "return") == 0) {
	    run = 0;
	    sleepTime = 0;
	}
	if (strcmp(buffer, "exit") == 0) {
	    exit(1);
	}
	if (strcmp(buffer, "crash") == 0) {
	    *bogus = 1;		/* intentional */
	    exit(2);		/* should not reach */
	}
	fclose(fp);
    }
}

void
sigproc(int signo)
{
    printf("testproc received signal\n");
    if (ignore)
	return;
    exit(0);
}

void
sigreload(int signo)
{
    readfile();
    return;
}


int
main(int argc, char **argv)
{
    int i;

#ifdef	AFS_AIX31_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    signal(SIGTERM, sigproc);
    signal(SIGQUIT, sigproc);
    signal(SIGHUP, sigreload);

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-ignore") == 0) {
	    ignore = 1;
	} else if (strcmp(argv[i], "-sleep") == 0) {
	    sleepTime = atoi(argv[i + 1]);
	    i++;
	} else if (strcmp(argv[i], "-file") == 0) {
	    file = argv[i + 1];
	    i++;
	} else {
	    printf("unrecognized option '%s', try one of\n", argv[i]);
	    printf("-ignore	    ignore SIGTERM signal\n");
	    printf("-sleep <n>	    sleep N seconds before exiting\n");
	    printf("-file <file>    read file for next action\n");
	    return 1;
	}
    }

    while (run) {
	if (file) {
	    readfile();
	} else {
	    run = 0;
	}
	if (sleepTime) {
	    sleep(sleepTime);
	}
    }

    return 0;
}
