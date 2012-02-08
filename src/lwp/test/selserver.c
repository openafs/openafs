/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* selserver.c tests the IOMGR_Select interface. Specifically it verifies
 * that the read/write/exception lists work correctly with file descriptors
 * larger than 31. Generally, the calls to IOMGR_Select pass in all the
 * read, write and exception fd_sets. When handling is complete, the file
 * descriptor is cleared from the set. Then if fd_set is checked to make sure
 * there are no other bits set. The fd_sets which should have had nothing set
 * are also checked.
 * 
 * The client can send one of the following:
 * -end - shoots the selserver.
 * -delay n - The selserver thread handling this request should sleep for
 *            n seconds before accepting data. Used in conjuction with -write
 *            so that the write STREAM can fill. This tests the IOMGR_Select
 *            write fd_sets.
 * -write n - writes n bytes to the selserver. If -delay is not set, a default
 *            of 5 seconds is used.
 * -soob - Send an out-of-band message to selserver. Tests the exception
 *            fd_set.
 */

/* Typical test scanario:
 * selclient -soob : used to test exception fd's on selserver.
 * selclient -delay 20 -write 150000 : used to test read/write fd's.
 * Adjust delay to 40 seconds if running loopback. Times and sizes derived
 * on IRIX 6.2 and 6.4.
 */

#include <afsconfig.h>
#include <afs/param.h>

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


#include "lwp.h"
#include "seltest.h"

extern int IOMGR_Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);


void sendTest(int, int, int);
void handleRequest(char *);

/* Server Pool */
#define MAX_THREADS 5
int nThreads = 0;

typedef struct {
#define CH_FREE 0
#define CH_INUSE 1
    int ch_state;
    int ch_fd;
    struct sockaddr_in ch_addr;
    fd_set ch_read, ch_write, ch_except;
    PROCESS ch_pid;
} clientHandle_t;

void handleWrite(clientHandle_t *, selcmd_t *);

clientHandle_t clientHandles[MAX_THREADS];

clientHandle_t *
getClientHandle()
{
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
	if (clientHandles[i].ch_state == CH_FREE) {
	    clientHandles[i].ch_state = CH_INUSE;
	    nThreads++;
	    return &clientHandles[i];
	}
    }
    Die(1, "No free client handles!\n");

    return (clientHandle_t *) NULL;	/* quiet compiler. */
}

int nSigIO = 0;
void
sigIO()
{
    nSigIO++;
}


Usage()
{
    printf("Usage: selserver [-fd n] [-syssel] port\n");
    printf("\t-fd n\tUse file descriptor n for socket.\n");
    exit(1);
}

char *program;

main(int ac, char **av)
{
    int i;
    int on = 1;
    short port = -1;		/* host order. */
    int setFD = 0;
    struct sockaddr_in saddr;
    int acceptFD;
    clientHandle_t *clientHandle;
    int code;
    int addr_len;
    PROCESS pid;
    fd_set *rfds, *wfds, *efds;
    int sockFD;

    program = av[0];

    if (ac < 2)
	Usage();

/*    lwp_debug = 1; */

    signal(SIGIO, sigIO);


    for (i = 1; i < ac; i++) {
	if (!strcmp("-fd", av[i])) {
	    if (++i >= ac) {
		printf("Missing number for -fd option.\n");
		Usage();
	    }
	    setFD = atoi(av[i]);
	    if (setFD <= 2) {
		printf("%d: file descriptor must be at least 3.\n", setFD);
		Usage();
	    }
	} else {
	    if (port == -1) {
		port = atoi(av[i]);
		if (port <= 0) {
		    printf("%s: port must be at least 1\n", av[i]);
		    Usage();
		}
	    } else {
		printf("%s: Unknown argument.\n", av[i]);
	    }
	}
    }

    if (port == -1) {
	printf("Missing port.\n");
	Usage();
    }

    if (!setFD) {
	setFD = 31;
	printf("Using default socket of %d.\n", setFD);
    }

    OpenFDs(setFD);

    IOMGR_Initialize();

    /* Setup server processes */
    for (i = 0; i < MAX_THREADS; i++) {
	if (LWP_CreateProcess
	    (handleRequest, 32768, LWP_NORMAL_PRIORITY,
	     (char *)&clientHandles[i], "HandleRequestThread", &pid) < 0) {
	    printf("%s: Failed to start all LWP's\n", program);
	    exit(1);
	}
	clientHandles[i].ch_pid = pid;
    }


    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0) {
	perror("socket");
	exit(1);
    }
    Log("Using socket at file descriptor %d.\n", sockFD);

    if (setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
	< 0) {
	perror("setsockopt: ");
	exit(1);
    }

    memset((void *)&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_port = ntohs(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockFD, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
	perror("bind: ");
	exit(1);
    }


    rfds = IOMGR_AllocFDSet();
    wfds = IOMGR_AllocFDSet();
    efds = IOMGR_AllocFDSet();
    if (!rfds || !wfds || !efds) {
	printf("main: Could not alloc fd_set's.\n");
	exit(1);
    }

    listen(sockFD, 100);


    while (1) {
	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_ZERO(efds);
	FD_SET(sockFD, rfds);
	FD_SET(sockFD, efds);

	Log("Main - going to select.\n");
	code =
	    IOMGR_Select(sockFD + 1, rfds, wfds, efds, (struct timeval *)0);
	switch (code) {
	case 0:		/* Timer, can't happen here. */
	case -1:
	case -2:
	    Log("Oops! select returns %d!\n", code);
	    abort();
	default:
	    if (FD_ISSET(sockFD, efds)) {
		recvOOB(sockFD);
		assertNullFDSet(sockFD, rfds);
		assertNullFDSet(-1, wfds);
		assertNullFDSet(sockFD, efds);
	    }
	    if (FD_ISSET(sockFD, rfds)) {
		while (nThreads > MAX_THREADS) {
		    IOMGR_Sleep(1);
		}

		clientHandle = getClientHandle();

		addr_len = sizeof(clientHandle->ch_addr);
		clientHandle->ch_fd = accept(sockFD, (struct sockaddr *)
					     &clientHandle->ch_addr,
					     &addr_len);
		if (clientHandle->ch_fd < 0) {
		    perror("accept: ");
		    exit(1);
		}

		Log("Main - signalling LWP 0x%x\n", &clientHandle->ch_state);
		LWP_NoYieldSignal(&clientHandle->ch_state);
		assertNullFDSet(sockFD, rfds);
		assertNullFDSet(-1, wfds);
		assertNullFDSet(-1, efds);
		break;
	    }
	    Die(1, "(main) No data to read.\n");
	}
    }
}

void
handleRequest(char *arg)
{
    clientHandle_t *ch = (clientHandle_t *) arg;
    selcmd_t sc;
    struct stat sbuf;
    int code = 0;
    int c_errno = 0;

    while (1) {
	Log("(handleRequest) going to sleep on 0x%x\n", &ch->ch_state);
	LWP_WaitProcess(&ch->ch_state);
	assert(ch->ch_state == CH_INUSE);

	FD_ZERO(&(ch->ch_read));
	FD_ZERO(&(ch->ch_write));
	FD_ZERO(&(ch->ch_except));
	FD_SET(ch->ch_fd, &(ch->ch_read));
	FD_SET(ch->ch_fd, &(ch->ch_except));
	code =
	    IOMGR_Select(ch->ch_fd + 1, &(ch->ch_read), &(ch->ch_write),
			 &(ch->ch_except), (struct timeval *)NULL);
	if (FD_ISSET(ch->ch_fd, &(ch->ch_except))) {
	    Log("Received expception. Read fd_set shows %d\n",
		FD_ISSET(ch->ch_fd, &(ch->ch_read)));
	    assertNullFDSet(ch->ch_fd, &(ch->ch_read));
	    assertNullFDSet(-1, &(ch->ch_write));
	    assertNullFDSet(ch->ch_fd, &(ch->ch_except));
	    goto done;
	}
	assert(code > 0);

	if (read(ch->ch_fd, (char *)&sc, sizeof(sc)) != sizeof(sc)) {
	    Die(1, "(handleRequest) read command");
	}

	Log("(handleRequest)cmd=%d\n", sc.sc_cmd);
	fflush(stdout);

	switch (sc.sc_cmd) {
	case SC_PROBE:
	    Log("Probed from client at %s\n",
		inet_ntoa(ch->ch_addr.sin_addr));
	    break;
#ifdef notdef
	case SC_OOB:
	    nThreads--;
	    ch->ch_fd = 0;
	    ch->ch_state = CH_FREE;
	    return;
#endif
	case SC_WRITE:
	    handleWrite(ch, &sc);
	    break;
	case SC_END:
	    Log("Ending ungracefully in server.\n");
	    exit(1);
	default:
	    Log("%d: bad command to handleRequest.\n", sc.sc_cmd);
	    break;
	}

      done:
	/* We're done now, so we can be re-used. */
	nThreads--;
	close(ch->ch_fd);
	ch->ch_fd = 0;
	ch->ch_state = CH_FREE;
    }
}

int write_I = 0;
void
handleWrite(clientHandle_t * ch, selcmd_t * sc)
{
    int i;
    fd_set *rfds, *wfds, *efds;
    char *buf;
    int code;
    int scode;
    char c;
    int nbytes;

    rfds = IOMGR_AllocFDSet();
    wfds = IOMGR_AllocFDSet();
    efds = IOMGR_AllocFDSet();
    assert(rfds && wfds && efds);

    if (sc->sc_delay > 0) {
	IOMGR_Sleep(sc->sc_delay);
    }

    Log("(handleWrite 0x%x) waking after %d second sleep.\n", ch->ch_pid,
	sc->sc_delay);

    if (sc->sc_flags & SC_WAIT_OOB)
	sendOOB(ch->ch_fd);

    buf = (char *)malloc(sc->sc_info);
    assert(buf);
    i = 0;

    while (1) {
	FD_ZERO(rfds);
	FD_SET(ch->ch_fd, rfds);
	FD_ZERO(efds);
	FD_SET(ch->ch_fd, efds);
	FD_ZERO(wfds);
	scode =
	    IOMGR_Select(ch->ch_fd + 1, rfds, wfds, efds,
			 (struct timeval *)0);
	assert(scode > 0);

	if (FD_ISSET(ch->ch_fd, rfds)) {

	    assert(i < sc->sc_info);

	    code = read(ch->ch_fd, &buf[i], 1);
	    i++;
	    write_I++;
	    if (code != 1) {
		Log("code =%d\n", code);
		assert(code == 1);
	    }

	    /* Test for valid fds */
	    assertNullFDSet(ch->ch_fd, rfds);
	    assertNullFDSet(-1, wfds);
	    assertNullFDSet(-1, efds);
	    if (c == END_DATA || i >= sc->sc_info) {
		break;
	    }
	}
    }
    Log("Read %d bytes of data.\n", sc->sc_info);
    nbytes = write(ch->ch_fd, buf, sc->sc_info);
    assert(nbytes == sc->sc_info);
    Log("Wrote data back to client.\n");
    IOMGR_FreeFDSet(rfds);
    IOMGR_FreeFDSet(wfds);
    IOMGR_FreeFDSet(efds);
}
