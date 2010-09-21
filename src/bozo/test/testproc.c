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

static int ignore = 0;
static int sleepTime = 10;

sigproc()
{
    printf("testproc received signal\n");
    if (ignore)
	return 0;
    exit(0);
}

main(argc, argv)
     int argc;
     char **argv;
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
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-ignore") == 0) {
	    ignore = 1;
	} else if (strcmp(argv[i], "-sleep") == 0) {
	    sleepTime = atoi(argv[i + 1]);
	    i++;
	} else {
	    printf("unrecognized option '%s', try one of\n", argv[i]);
	    printf("-ignore	    ignore SIGTERM signal\n");
	    printf("-sleep <n>	    sleep N seconds before exiting\n");
	    exit(1);
	}
    }
    sleep(sleepTime);
    exit(0);
}
