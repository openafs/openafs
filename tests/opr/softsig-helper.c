/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* A test helper for the softsig code. This just sits and reports what signals
 * it has received.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <pthread.h>
#include <sys/mman.h>
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#include <afs/opr.h>
#include <opr/softsig.h>

#define SIGTABLEENTRY(name)	{ #name, SIG##name }

static struct sigtable {
	char *name;
	int signo;
} sigtable[] = {
	SIGTABLEENTRY(INT),
	SIGTABLEENTRY(HUP),
	SIGTABLEENTRY(QUIT),
	SIGTABLEENTRY(ALRM),
	SIGTABLEENTRY(TERM),
	SIGTABLEENTRY(TSTP),
	SIGTABLEENTRY(USR1),
	SIGTABLEENTRY(USR2)
	};

static char *signame(int signo) {
    int i;

    for (i = 0; i < sizeof(sigtable) / sizeof(sigtable[0]); ++i) {
	if (sigtable[i].signo == signo) {
	    return sigtable[i].name;
	}
    }

    return "UNK";
}

static void
handler(int signal) {
    printf("Received %s\n", signame(signal));

    fflush(stdout);
}

static void *
mythread(void *arg) {
    while (1) {
	sleep(60);
    }
    return NULL;
}

static void
disable_core(void)
{
#ifdef HAVE_GETRLIMIT
    struct rlimit rlp;

    memset(&rlp, 0, sizeof(rlp));

    /*
     * Set our core limit to 0 for SIGSEGV/SIGBUS, so we don't litter core
     * files around when running the test.
     */
    if (setrlimit(RLIMIT_CORE, &rlp) < 0) {
	perror("setrlimit(RLIMIT_CORE)");
    }
#endif
}

static void *
crashingThread(void *arg) {
    disable_core();
    *(char *)1 = 'a';  /* raises SIGSEGV */
    return NULL; /* Ha! */
}

static void *
thrownUnderTheBusThread(void *arg) {
    char *path = arg;
    int fd = open(path, O_CREAT | O_APPEND, 0660);
    char *m = mmap(NULL, 10, PROT_WRITE, MAP_PRIVATE, fd, 0);
    disable_core();
    *(m + 11) = 'a';  /* raises SIGBUS */
    return NULL;
}


int
main(int argvc, char **argv)
{
    char *path;
    int i;
    int threads = 10;

    opr_softsig_Init();

    opr_Verify(opr_softsig_Register(SIGINT, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGHUP, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGQUIT, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGALRM, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGTERM, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGTSTP, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGUSR1, handler) == 0);
    opr_Verify(opr_softsig_Register(SIGUSR2, handler) == 0);

    for (i=0; i<threads; i++) {
	pthread_t id;
	opr_Verify(pthread_create(&id, NULL, mythread, NULL) == 0);
    }

    if (argvc>1 && strcmp(argv[1], "-crash") == 0) {
	pthread_t id;
	opr_Verify(pthread_create(&id, NULL, crashingThread, NULL) == 0);
    } else if (argvc>1 && strcmp(argv[1], "-buserror") == 0) {
	if (argvc > 2)
	    path = argv[2];
	else
	    opr_abort();
	pthread_t id;
	opr_Verify(pthread_create(&id, NULL, thrownUnderTheBusThread,
				  path) == 0);
    } else {
	printf("Ready\n");
	fflush(stdout);
    }

    mythread(NULL);

    return 0;
}
