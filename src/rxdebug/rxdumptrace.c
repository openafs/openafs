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

extern char *rxi_tracename;
extern int rxi_logfd;

struct rx_trace {
    afs_uint32 cid;
    unsigned short call;
    unsigned short qlen;
    afs_uint32 now;
    afs_uint32 waittime;
    afs_uint32 servicetime;
    afs_uint32 event;
};

#include <errno.h>
#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#endif

int
main(int argc, char **argv)
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

#endif
