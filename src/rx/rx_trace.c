/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef RXDEBUG
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#else
#include <sys/file.h>
#include <unistd.h>
#endif
#include "rx.h"
#include "rx_globals.h"
#include "rx_trace.h"

#ifdef RXTRACEON
char rxi_tracename[80] = "/tmp/rxcalltrace";
#else
char rxi_tracename[80] =
    "\0Change This pathname (and preceding NUL) to initiate tracing";
#endif
int rxi_logfd = -1;
char rxi_tracebuf[4096];
afs_uint32 rxi_tracepos = 0;

struct rx_trace {
    afs_uint32 cid;
    unsigned short call;
    unsigned short qlen;
    afs_uint32 now;
    afs_uint32 waittime;
    afs_uint32 servicetime;
    afs_uint32 event;
};

void
rxi_flushtrace()
{
    if (rxi_logfd >= 0)
	write(rxi_logfd, rxi_tracebuf, rxi_tracepos);
    rxi_tracepos = 0;
}

void
rxi_calltrace(event, call)
     unsigned int event;
     struct rx_call *call;
{
    struct clock now;
    struct rx_trace rxtinfo;

    if (!rxi_tracename[0])
	return;

    if (rxi_logfd < 0) {
	rxi_logfd = open(rxi_tracename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (rxi_logfd < 0)
	    rxi_tracename[0] = '\0';
    }
    clock_GetTime(&now);

    rxtinfo.event = event;
    rxtinfo.now = now.sec * 1000 + now.usec / 1000;
    rxtinfo.cid = call->conn->cid;
    rxtinfo.call = *(call->callNumber);
    rxtinfo.qlen = rx_nWaiting;
    rxtinfo.servicetime = 0;
    rxtinfo.waittime = 0;

    switch (event) {
    case RX_CALL_END:
	clock_Sub(&now, &(call->traceStart));
	rxtinfo.servicetime = now.sec * 10000 + now.usec / 100;
	if (call->traceWait.sec) {
	    now = call->traceStart;
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	call->traceWait.sec = call->traceWait.usec = call->traceStart.sec =
	    call->traceStart.usec = 0;
	break;

    case RX_CALL_START:
	call->traceStart = now;
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_TRACE_DROP:
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_CALL_ARRIVAL:
	call->traceWait = now;
    default:
	break;
    }

    memcpy(rxi_tracebuf + rxi_tracepos, &rxtinfo, sizeof(struct rx_trace));
    rxi_tracepos += sizeof(struct rx_trace);
    if (rxi_tracepos >= (4096 - sizeof(struct rx_trace)))
	rxi_flushtrace();
}

#ifdef DUMPTRACE
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#endif

int
main(argc, argv)
     char **argv;
{
    struct rx_trace ip;
    int err = 0;

    setlinebuf(stdout);
    argv++;
    argc--;
    while (argc && **argv == '-') {
	if (strcmp(*argv, "-trace") == 0) {
	    strcpy(rxi_tracename, *(++argv));
	    argc--;
	} else {
	    err++;
	    break;
	}
	argv++, argc--;
    }
    if (err || argc != 0) {
	printf("usage: dumptrace [-trace pathname]");
	exit(1);
    }

    rxi_logfd = open(rxi_tracename, O_RDONLY);
    if (rxi_logfd < 0) {
	perror("");
	exit(errno);
    }

    while (read(rxi_logfd, &ip, sizeof(struct rx_trace))) {
	printf("%9u ", ip.now);
	switch (ip.event) {
	case RX_CALL_END:
	    putchar('E');
	    break;
	case RX_CALL_START:
	    putchar('S');
	    break;
	case RX_CALL_ARRIVAL:
	    putchar('A');
	    break;
	case RX_TRACE_DROP:
	    putchar('D');
	    break;
	default:
	    putchar('U');
	    break;
	}
	printf(" %3u %7u %7u      %x.%x\n", ip.qlen, ip.servicetime,
	       ip.waittime, ip.cid, ip.call);
    }
    return 0;
}

#endif /* DUMPTRACE */

#endif
