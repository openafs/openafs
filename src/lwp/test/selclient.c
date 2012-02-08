/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* selclient.c tests the IOMGR_Select interface. Specifically it verifies
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
#include <stdio.h>
#include <stdarg.h>
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
#include <time.h>


#include "lwp.h"
#include "seltest.h"

/* Put this in lwp.h? */
extern int IOMGR_Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);


void sendTest(int, int, int, int);
void sendEnd(int);

int nSigIO = 0;
void
sigIO()
{
    nSigIO++;
}

char *program;


Usage()
{
    printf
	("Usage: selclient [-fd n] [-oob] [-soob] [-delay n] [-end] [-write n] host port\n");
    printf("\t-fd n\tUse file descriptor n for socket.\n");
    printf("\t-oob\tRequest a MSG_OOB from server.\n");
    printf("\t-soob\tSend a MSG_OOB to server.\n");
    printf("\t-delay n\tAsk server to delay n seconds for my writes.\n");
    printf("\t-end\tAsk server to terminate.\n");
    printf("\t-write\tWrite n bytes to server, verify reply.\n");
    exit(1);
}

main(int ac, char **av)
{
    int i;
    int on = 1;
    char *hostname = 0;
    struct hostent *hostent;
    int host;			/* net order. */
    short port = -1;		/* host order. */
    int setFD = 0;
    int sockFD;
    struct sockaddr_in saddr;
    int reqOOB = 0;
    int putOOB = 0;
    int delay = 5;
    int doEnd = 0;
    int doWrite = 0;
    int writeSize = 0;

    program = av[0];

    signal(SIGIO, sigIO);


    for (i = 1; i < ac; i++) {
	if (!strcmp("-fd", av[i])) {
	    if (++i >= ac) {
		printf("Missing number for -fd option.\n");
		Usage();
	    }
	    setFD = atoi(av[i]);
	    if (setFD <= 2) {
		printf("%s: %d: file descriptor must be at least 3.\n",
		       program, setFD);
		Usage();
	    }
	} else if (!strcmp("-end", av[i])) {
	    doEnd = 1;
	} else if (!strcmp("-delay", av[i])) {
	    if (++i >= ac) {
		printf("%s: Missing time for -delay option.\n", program);
		Usage();
	    }
	    delay = atoi(av[i]);
	    if (delay < 0) {
		printf("%s: %s: delay must be at least 0 seconds.\n", program,
		       av[i]);
		Usage();
	    }
	} else if (!strcmp("-write", av[i])) {
	    doWrite = 1;
	    if (++i >= ac) {
		printf("%s: Missing size for -write option.\n", program);
		Usage();
	    }
	    writeSize = atoi(av[i]);
	    if (writeSize < 1) {
		printf("%s: %s: Write size must be at least 1 byte.\n",
		       program, av[i]);
		Usage();
	    }
	} else if (!strcmp("-oob", av[i])) {
	    reqOOB = 1;
	} else if (!strcmp("-soob", av[i])) {
	    putOOB = 1;
	} else {
	    if (!hostname) {
		hostname = av[i];
	    } else if (port == -1) {
		port = atoi(av[i]);
		if (port <= 0) {
		    printf("%s: %s: port must be at least 1\n", program,
			   av[i]);
		    Usage();
		}
	    } else {
		printf("%s: %s: Unknown argument.\n", program, av[i]);
	    }
	}
    }

    if (!hostname) {
	printf("%s: Missing hostname and port.\n", program);
	Usage();
    }
    if (port == -1) {
	printf("%s: Missing port.\n", program);
	Usage();
    }

    if (writeSize == 0 && doEnd == 0 && putOOB == 0) {
	printf("%s: Missing action.\n", program);
	Usage();
    }

    if (!setFD) {
	setFD = 31;
	printf("%s: Using default socket of %d.\n", program, setFD);
    }

    if (!(hostent = gethostbyname(hostname))) {
	printf("%s: Failed to find host entry for %s.\n", program, hostname);
	exit(1);
    }

    memcpy((void *)&host, (const void *)hostent->h_addr, sizeof(host));

    OpenFDs(setFD);

    /* Connect to server. */
    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0) {
	Die(0, "socket");
    }
    printf("%s: Using socket at file descriptor %d.\n", program, sockFD);

    memset((void *)&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = host;	/* already in network byte order. */
    saddr.sin_port = htons(port);

    if (connect(sockFD, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
	assert(0);
    }


    if (doEnd) {
	sendEnd(sockFD);
    } else if (putOOB) {
	Log("Will send OOB in 2 seconds.\n");
	sleep(2);
	sendOOB(sockFD);
	Log("Sent OOB, sleeping for 5 seconds.\n");
	sleep(5);
	Log("Sent OOB, exiting.\n");
    } else {
	IOMGR_Initialize();
	sendTest(sockFD, delay, reqOOB, writeSize);
    }
    close(sockFD);
}

/* sendTest
 */
int writeIndex;
void
sendTest(int sockFD, int delay, int reqOOB, int size)
{
    char *buf, *bufTest;
    fd_set *rfds, *wfds, *efds;
    int i, j;
    int nbytes, code;
    selcmd_t selCmd;
    time_t stime, etime;

    buf = (char *)malloc(size);
    assert(buf);
    bufTest = (char *)malloc(size);
    assert(bufTest);

    for (j = i = 0; i < size; i++, j++) {
	if (j == END_DATA)
	    j++;
	if (j > 255)
	    j = 0;
	buf[i] = (char)j;
    }

    selCmd.sc_cmd = SC_WRITE;
    selCmd.sc_info = size;
    selCmd.sc_delay = delay;
    selCmd.sc_flags = SC_WAIT_ONLY;

    nbytes = write(sockFD, (char *)&selCmd, sizeof(selCmd));
    assert(nbytes == sizeof(selCmd));

    Log("Starting to write %d bytes.\n", size);
    if (!delay) {
	nbytes = write(sockFD, buf, size);
	assert(nbytes == size);
    } else {
	rfds = IOMGR_AllocFDSet();
	wfds = IOMGR_AllocFDSet();
	efds = IOMGR_AllocFDSet();
	if (!rfds || !wfds || !efds) {
	    printf("%s: Could not allocate all fd_sets.\n", program);
	    exit(1);
	}

	for (writeIndex = i = 0; i < size; writeIndex++, i++) {
	    FD_ZERO(rfds);
	    FD_ZERO(wfds);
	    FD_ZERO(efds);
	    FD_SET(sockFD, wfds);
	    FD_SET(sockFD, efds);
	    (void)time(&stime);
	    code =
		IOMGR_Select(sockFD + 1, rfds, wfds, efds,
			     (struct timeval *)NULL);
	    assert(code > 0);

	    if (FD_ISSET(sockFD, wfds)) {
		(void)time(&etime);
		if (etime - stime > 1) {
		    Log("Waited %d seconds to write at offset %d.\n",
			etime - stime, i);
		}
		stime = etime;
		nbytes = write(sockFD, &buf[i], 1);
		(void)time(&etime);
		if (etime - stime > 1) {
		    Log("Waited %d seconds IN write.\n", etime - stime);
		}
		assert(nbytes == 1);
		FD_CLR(sockFD, wfds);
	    }
	    assertNullFDSet(0, rfds);
	    assertNullFDSet(0, wfds);
	    assertNullFDSet(0, efds);
	}
    }

    Log("Wrote %d bytes.\n", size);
    i = 0;
    while (i < size) {
	nbytes = read(sockFD, &bufTest[i], size);
	i += nbytes;
    }
    Log("Read %d bytes.\n", size);

    assert(memcmp(buf, bufTest, size) == 0);
    Log("Compared %d bytes.\n", size);
}

void
sendEnd(int fd)
{
    selcmd_t sc;

    sc.sc_cmd = SC_END;

    if (write(fd, (char *)&sc, sizeof(sc)) != sizeof(sc)) {
	Die(1, "(sendEnd) write failed: ");
    }
}
