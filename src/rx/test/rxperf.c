/*
 * Copyright (c) 2000 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution
 *    at such time that OpenAFS documentation is written.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <afsconfig.h>

/*
nn * We are using getopt since we want it to be possible to link to
 * transarc libs.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <stdarg.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <winsock2.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#ifdef HAVE_ERRX
#include <err.h>		/* not stricly right, but if we have a errx() there
				 * is hopefully a err.h */
#endif
#include <getopt.h>
#include "rx.h"
#include "rx_null.h"
#include "rx_globals.h"

static const char *__progname;

#ifndef HAVE_WARNX
static void
warnx(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
#endif /* !HAVE_WARNX */

#ifndef HAVE_ERRX
static void
errx(int eval, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    exit(eval);
}
#endif /* !HAVE_ERRX */

#ifndef HAVE_WARN
static void
warn(const char *fmt, ...)
{
    va_list args;
    char *errstr;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf(stderr, fmt, args);

    errstr = strerror(errno);

    fprintf(stderr, ": %s\n", errstr ? errstr : "unknown error");
    va_end(args);
}
#endif /* !HAVE_WARN */

#ifndef HAVE_ERR
static void
err(int eval, const char *fmt, ...)
{
    va_list args;
    char *errstr;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf(stderr, fmt, args);

    errstr = strerror(errno);

    fprintf(stderr, ": %s\n", errstr ? errstr : "unknown error");
    va_end(args);

    exit(eval);
}
#endif /* !HAVE_ERR */

#define DEFAULT_PORT 7009	/* To match tcpdump */
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_BYTES 1000000
#define RXPERF_BUFSIZE 512 * 1024

enum { RX_PERF_VERSION = 3 };
enum { RX_SERVER_ID = 147 };
enum { RX_PERF_UNKNOWN = -1,
       RX_PERF_SEND = 0,
       RX_PERF_RECV = 1,
       RX_PERF_RPC = 3,
       RX_PERF_FILE = 4
};

enum { RXPERF_MAGIC_COOKIE = 0x4711 };

/*
 *
 */

#if RXPERF_DEBUG
#define DBFPRINT(x) do { printf x ; } while(0)
#else
#define DBFPRINT(x)
#endif

static void
sigusr1(int foo)
{
    exit(2);			/* XXX profiler */
}

static void
sigint(int foo)
{
    rx_Finalize();
    exit(2);			/* XXX profiler */
}

/*
 *
 */

static struct timeval timer_start;
static struct timeval timer_stop;
static int timer_check = 0;

static void
start_timer(void)
{
    timer_check++;
    gettimeofday(&timer_start, NULL);
}

/*
 *
 */

static void
end_and_print_timer(char *str)
{
    long long start_l, stop_l;

    timer_check--;
    assert(timer_check == 0);
    gettimeofday(&timer_stop, NULL);
    start_l = timer_start.tv_sec * 1000000 + timer_start.tv_usec;
    stop_l = timer_stop.tv_sec * 1000000 + timer_stop.tv_usec;
    printf("%s:\t%8llu msec\n", str, (stop_l - start_l) / 1000);
}

/*
 *
 */

static u_long
str2addr(const char *s)
{
    struct in_addr server;
    struct hostent *h;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
    if (inet_addr(s) != INADDR_NONE)
	return inet_addr(s);
    h = gethostbyname(s);
    if (h != NULL) {
	memcpy(&server, h->h_addr_list[0], sizeof(server));
	return server.s_addr;
    }
    return 0;
}


/*
 *
 */

static void
get_sec(int serverp, struct rx_securityClass **sec, int *secureindex)
{
    if (serverp) {
	*sec = rxnull_NewServerSecurityObject();
	*secureindex = 1;
    } else {
	*sec = rxnull_NewClientSecurityObject();
	*secureindex = 0;
    }
}

/*
 * process the "RPC" and return the results
 */

char somebuf[RXPERF_BUFSIZE];

afs_int32 rxwrite_size = sizeof(somebuf);
afs_int32 rxread_size = sizeof(somebuf);

static int
readbytes(struct rx_call *call, afs_int32 bytes)
{
    afs_int32 size;

    while (bytes > 0) {
	size = rxread_size;
	if (size > bytes)
	    size = bytes;
	if (rx_Read(call, somebuf, size) != size)
	    return 1;
	bytes -= size;
    }
    return 0;
}

static int
sendbytes(struct rx_call *call, afs_int32 bytes)
{
    afs_int32 size;

    while (bytes > 0) {
	size = rxwrite_size;
	if (size > bytes)
	    size = bytes;
	if (rx_Write(call, somebuf, size) != size)
	    return 1;
	bytes -= size;
    }
    return 0;
}


static afs_int32
rxperf_ExecuteRequest(struct rx_call *call)
{
    afs_int32 version;
    afs_int32 command;
    afs_uint32 bytes;
    afs_uint32 recvb;
    afs_uint32 sendb;
    afs_uint32 data;
    afs_uint32 num;
    afs_uint32 *readwrite;
    afs_uint32 i;
    int readp = TRUE;

    DBFPRINT(("got a request\n"));

    if (rx_Read32(call, &version) != 4) {
	warn("rx_Read failed to read version");
	return -1;
    }

    if (htonl(RX_PERF_VERSION) != version) {
	warnx("client has wrong version");
	return -1;
    }

    if (rx_Read32(call, &command) != 4) {
	warnx("rx_Read failed to read command");
	return -1;
    }
    command = ntohl(command);

    if (rx_Read32(call, &data) != 4) {
	warnx("rx_Read failed to read size");
	return -1;
    }
    rxread_size = ntohl(data);
    if (rxread_size > sizeof(somebuf)) {
	warnx("rxread_size too large %d", rxread_size);
	return -1;
    }

    if (rx_Read32(call, &data) != 4) {
	warnx("rx_Read failed to write size");
	return -1;
    }
    rxwrite_size = ntohl(data);
    if (rxwrite_size > sizeof(somebuf)) {
	warnx("rxwrite_size too large %d", rxwrite_size);
	return -1;
    }

    switch (command) {
    case RX_PERF_SEND:
	DBFPRINT(("got a send request\n"));

	if (rx_Read32(call, &bytes) != 4) {
	    warnx("rx_Read failed to read bytes");
	    return -1;
	}
	bytes = ntohl(bytes);

	DBFPRINT(("reading(%d) ", bytes));
	readbytes(call, bytes);

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write32(call, &data) != 4) {
	    warnx("rx_Write failed when sending back result");
	    return -1;
	}
	DBFPRINT(("done\n"));

	break;
    case RX_PERF_RPC:
	DBFPRINT(("got a rpc request, reading commands\n"));

	if (rx_Read32(call, &recvb) != 4) {
	    warnx("rx_Read failed to read recvbytes");
	    return -1;
	}
	recvb = ntohl(recvb);
	if (rx_Read32(call, &sendb) != 4) {
	    warnx("rx_Read failed to read sendbytes");
	    return -1;
	}
	sendb = ntohl(sendb);

	DBFPRINT(("read(%d) ", recvb));
	if (readbytes(call, recvb)) {
	    warnx("readbytes failed");
	    return -1;
	}
	DBFPRINT(("send(%d) ", sendb));
	if (sendbytes(call, sendb)) {
	    warnx("sendbytes failed");
	    return -1;
	}

	DBFPRINT(("done\n"));

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write32(call, &data) != 4) {
	    warnx("rx_Write failed when sending back magic cookie");
	    return -1;
	}

	break;
    case RX_PERF_FILE:
	if (rx_Read32(call, &data) != 4)
	    errx(1, "failed to read num from client");
	num = ntohl(data);

	readwrite = malloc(num * sizeof(afs_uint32));
	if (readwrite == NULL)
	    err(1, "malloc");

	if (rx_Read(call, (char*)readwrite, num * sizeof(afs_uint32)) !=
	    num * sizeof(afs_uint32))
	    errx(1, "failed to read recvlist from client");

	for (i = 0; i < num; i++) {
	    if (readwrite[i] == 0) {
		DBFPRINT(("readp %d", readwrite[i]));
		readp = !readp;
	    }

	    bytes = ntohl(readwrite[i]) * sizeof(afs_uint32);

	    if (readp) {
		DBFPRINT(("read\n"));
		readbytes(call, bytes);
	    } else {
		sendbytes(call, bytes);
		DBFPRINT(("send\n"));
	    }
	}

	break;
    case RX_PERF_RECV:
	DBFPRINT(("got a recv request\n"));

	if (rx_Read32(call, &bytes) != 4) {
	    warnx("rx_Read failed to read bytes");
	    return -1;
	}
	bytes = ntohl(bytes);

	DBFPRINT(("sending(%d) ", bytes));
	sendbytes(call, bytes);

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write32(call, &data) != 4) {
	    warnx("rx_Write failed when sending back result");
	    return -1;
	}
	DBFPRINT(("done\n"));

	break;
    default:
	warnx("client sent a unsupported command");
	return -1;
    }
    DBFPRINT(("done with command\n"));

    return 0;
}

/*
 *
 */

static void
do_server(short port, int nojumbo, int maxmtu, int udpbufsz)
{
    struct rx_service *service;
    struct rx_securityClass *secureobj;
    int secureindex;
    int ret;

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	printf("Can't initialize winsock.\n");
	exit(1);
    }
    rx_EnableHotThread();
#endif

    ret = rx_Init(htons(port));
    if (ret)
	errx(1, "rx_Init failed");

    if (nojumbo)
      rx_SetNoJumbo();

    if (maxmtu)
      rx_SetMaxMTU(maxmtu);

    rx_SetUdpBufSize(udpbufsz);

    get_sec(1, &secureobj, &secureindex);

    service =
	rx_NewService(0, RX_SERVER_ID, "rxperf", &secureobj, secureindex,
		      rxperf_ExecuteRequest);
    if (service == NULL)
	errx(1, "Cant create server");

    rx_SetMinProcs(service, 2);
    rx_SetMaxProcs(service, 100);
    rx_SetCheckReach(service, 1);

    rx_StartServer(1);
    abort();
}

/*
 *
 */

static void
readfile(const char *filename, afs_uint32 ** readwrite, afs_uint32 * size)
{
    FILE *f;
    afs_uint32 len = 16;
    afs_uint32 num = 0;
    afs_uint32 data;
    char *ptr;
    char buf[RXPERF_BUFSIZE];

    *readwrite = malloc(sizeof(afs_uint32) * len);

    if (*readwrite == NULL)
	err(1, "malloc");

    f = fopen(filename, "r");
    if (f == NULL)
	err(1, "fopen");

    while (fgets(buf, sizeof(buf), f) != NULL) {
	if (num >= len) {
	    len = len * 2;
	    *readwrite = realloc(*readwrite, len * sizeof(afs_uint32));
	    if (*readwrite == NULL)
		err(1, "realloc");
	}

	if (*buf != '\n') {
	    data = htonl(strtol(buf, &ptr, 0));
	    if (ptr && ptr == buf)
		errx(1, "can't resolve number of bytes to transfer");
	} else {
	    data = 0;
	}

	(*readwrite)[num] = data;
	num++;
    }

    *size = num;


    if (fclose(f) == -1)
	err(1, "fclose");
}


/*
 *
 */

static void
do_client(const char *server, short port, char *filename, afs_int32 command,
	  afs_int32 times, afs_int32 bytes, afs_int32 sendtimes, afs_int32 recvtimes,
          int dumpstats, int nojumbo, int maxmtu, int udpbufsz)
{
    struct rx_connection *conn;
    struct rx_call *call;
    afs_uint32 addr;
    struct rx_securityClass *secureobj;
    int secureindex;
    afs_int32 data;
    afs_int32 num;
    int ret;
    int i;
    int readp = FALSE;
    char stamp[1024];
    afs_uint32 size;

    afs_uint32 *readwrite;

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	printf("Can't initialize winsock.\n");
	exit(1);
    }
    rx_EnableHotThread();
#endif

    addr = str2addr(server);

    ret = rx_Init(0);
    if (ret)
	errx(1, "rx_Init failed");

    if (nojumbo)
      rx_SetNoJumbo();

    if (maxmtu)
      rx_SetMaxMTU(maxmtu);

    rx_SetUdpBufSize(udpbufsz);

    get_sec(0, &secureobj, &secureindex);

    conn = rx_NewConnection(addr, htons(port), RX_SERVER_ID, secureobj, secureindex);
    if (conn == NULL)
	errx(1, "failed to contact server");

    sprintf(stamp, "send\t%d times\t%d writes\t%d reads", times, sendtimes,
	    recvtimes);
    start_timer();

    for (i = 0; i < times; i++) {

	DBFPRINT(("starting command "));

	call = rx_NewCall(conn);
	if (call == NULL)
	    errx(1, "rx_NewCall failed");

	data = htonl(RX_PERF_VERSION);
	if (rx_Write32(call, &data) != 4)
	    errx(1, "rx_Write failed to send version (err %d)", rx_Error(call));

	data = htonl(command);
	if (rx_Write32(call, &data) != 4)
	    errx(1, "rx_Write failed to send command (err %d)", rx_Error(call));

	data = htonl(rxread_size);
	if (rx_Write32(call, &data) != 4)
	    errx(1, "rx_Write failed to send read size (err %d)", rx_Error(call));
	data = htonl(rxwrite_size);
	if (rx_Write32(call, &data) != 4)
	    errx(1, "rx_Write failed to send write read (err %d)", rx_Error(call));


	switch (command) {
	case RX_PERF_RECV:
	    DBFPRINT(("command "));

	    data = htonl(bytes);
	    if (rx_Write32(call, &data) != 4)
		errx(1, "rx_Write failed to send size (err %d)", rx_Error(call));

	    DBFPRINT(("sending(%d) ", bytes));
	    if (readbytes(call, bytes))
		errx(1, "sendbytes (err %d)", rx_Error(call));

	    if (rx_Read32(call, &data) != 4)
		errx(1, "failed to read result from server (err %d)", rx_Error(call));

	    if (data != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_SEND:
	    DBFPRINT(("command "));

	    data = htonl(bytes);
	    if (rx_Write32(call, &data) != 4)
		errx(1, "rx_Write failed to send size (err %d)", rx_Error(call));

	    DBFPRINT(("sending(%d) ", bytes));
	    if (sendbytes(call, bytes))
		errx(1, "sendbytes (err %d)", rx_Error(call));

	    if (rx_Read32(call, &data) != 4)
		errx(1, "failed to read result from server (err %d)", rx_Error(call));

	    if (data != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_RPC:
	    DBFPRINT(("commands "));

	    data = htonl(sendtimes);
	    if (rx_Write32(call, &data) != 4)
		errx(1, "rx_Write failed to send command (err %d)", rx_Error(call));

	    data = htonl(recvtimes);
	    if (rx_Write32(call, &data) != 4)
		errx(1, "rx_Write failed to send command (err %d)", rx_Error(call));

	    DBFPRINT(("send(%d) ", sendtimes));
	    if (sendbytes(call, sendtimes))
		errx(1, "sendbytes (err %d)", rx_Error(call));

	    DBFPRINT(("recv(%d) ", recvtimes));
	    if (readbytes(call, recvtimes))
		errx(1, "sendbytes (err %d)", rx_Error(call));

	    if (rx_Read32(call, &bytes) != 4)
		errx(1, "failed to read result from server (err %d)", rx_Error(call));

	    if (bytes != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_FILE:
	    readfile(filename, &readwrite, &num);

	    data = htonl(num);
	    if (rx_Write32(call, &data) != 4)
		errx(1, "rx_Write failed to send size (err %d)", rx_Error(call));

	    if (rx_Write(call, (char *)readwrite, num * sizeof(afs_uint32))
		!= num * sizeof(afs_uint32))
		errx(1, "rx_Write failed to send list (err %d)", rx_Error(call));

	    for (i = 0; i < num; i++) {
		if (readwrite[i] == 0)
		    readp = !readp;

		size = ntohl(readwrite[i]) * sizeof(afs_uint32);

		if (readp) {
		    if (readbytes(call, size))
			errx(1, "sendbytes (err %d)", rx_Error(call));
		    DBFPRINT(("read\n"));
		} else {
		    if (sendbytes(call, size))
			errx(1, "sendbytes (err %d)", rx_Error(call));
		    DBFPRINT(("send\n"));
		}
	    }
	    break;
	default:
	    abort();
	}

	rx_EndCall(call, 0);
    }

    end_and_print_timer(stamp);
    DBFPRINT(("done for good\n"));

    if (dumpstats) {
	rx_PrintStats(stdout);
	rx_PrintPeerStats(stdout, conn->peer);
    }
    rx_Finalize();
}

static void
usage()
{
#define COMMON ""

    fprintf(stderr, "usage: %s client -c send -b <bytes>\n", __progname);
    fprintf(stderr, "usage: %s client -c recv -b <bytes>\n", __progname);
    fprintf(stderr,
	    "usage: %s client -c rpc  -S <sendbytes> -R <recvbytes>\n",
	    __progname);
    fprintf(stderr, "usage: %s client -c file -f filename\n", __progname);
    fprintf(stderr,
	    "%s: usage:	common option to the client "
	    "-w <write-bytes> -r <read-bytes> -T times -p port -s server -D\n",
	    __progname);
    fprintf(stderr, "usage: %s server -p port\n", __progname);
#undef COMMMON
    exit(1);
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_server(int argc, char **argv)
{
    short port = DEFAULT_PORT;
    int nojumbo = 0;
    int maxmtu = 0;
    int udpbufsz = 64 * 1024;
    char *ptr;
    int ch;

    while ((ch = getopt(argc, argv, "r:d:p:w:jm:u:4")) != -1) {
	switch (ch) {
	case 'd':
#ifdef RXDEBUG
	    rx_debugFile = fopen(optarg, "w");
	    if (rx_debugFile == NULL)
		err(1, "fopen %s", optarg);
#else
	    errx(1, "compiled without RXDEBUG");
#endif
	    break;
	case 'r':
	    rxread_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve readsize");
	    if (rxread_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)", rxread_size,
		     sizeof(somebuf));
	    break;
	case 'p':
	    port = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve portname");
	    break;
	case 'w':
	    rxwrite_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve writesize");
	    if (rxwrite_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)", rxwrite_size,
		     sizeof(somebuf));
	    break;
	case 'j':
	  nojumbo=1;
	  break;
	case 'm':
	  maxmtu = strtol(optarg, &ptr, 0);
	  if (ptr && *ptr != '\0')
	    errx(1, "can't resolve rx maxmtu to use");
	    break;
	case 'u':
	    udpbufsz = strtol(optarg, &ptr, 0) * 1024;
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve upd buffer size (Kbytes)");
	    break;
	case '4':
	  RX_IPUDP_SIZE = 28;
	  break;
	default:
	    usage();
	}
    }

    if (optind != argc)
	usage();

    do_server(port, nojumbo, maxmtu, udpbufsz);

    return 0;
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_client(int argc, char **argv)
{
    char *host = DEFAULT_HOST;
    int bytes = DEFAULT_BYTES;
    short port = DEFAULT_PORT;
    char *filename = NULL;
    afs_int32 cmd;
    int sendtimes = 3;
    int recvtimes = 30;
    int times = 100;
    int dumpstats = 0;
    int nojumbo = 0;
    int maxmtu = 0;
    int udpbufsz = 64 * 1024;
    char *ptr;
    int ch;

    cmd = RX_PERF_UNKNOWN;

    while ((ch = getopt(argc, argv, "T:S:R:b:c:d:p:r:s:w:f:Djm:u:4")) != -1) {
	switch (ch) {
	case 'b':
	    bytes = strtol(optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve number of bytes to transfer");
	    break;
	case 'c':
	    if (strcasecmp(optarg, "send") == 0)
		cmd = RX_PERF_SEND;
	    else if (strcasecmp(optarg, "recv") == 0)
		cmd = RX_PERF_RECV;
	    else if (strcasecmp(optarg, "rpc") == 0)
		cmd = RX_PERF_RPC;
	    else if (strcasecmp(optarg, "file") == 0)
		cmd = RX_PERF_FILE;
	    else
		errx(1, "unknown command %s", optarg);
	    break;
	case 'd':
#ifdef RXDEBUG
	    rx_debugFile = fopen(optarg, "w");
	    if (rx_debugFile == NULL)
		err(1, "fopen %s", optarg);
#else
	    errx(1, "compiled without RXDEBUG");
#endif
	    break;
	case 'p':
	    port = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve portname");
	    break;
	case 'r':
	    rxread_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve readsize");
	    if (rxread_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)", rxread_size,
		     sizeof(somebuf));
	    break;
	case 's':
	    host = strdup(optarg);
	    if (host == NULL)
		err(1, "strdup");
	    break;
	case 'w':
	    rxwrite_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx(1, "can't resolve writesize");
	    if (rxwrite_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)", rxwrite_size,
		     sizeof(somebuf));
	    break;
	case 'T':
	    times = strtol(optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve number of times to execute rpc");
	    break;
	case 'S':
	    sendtimes = strtol(optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve number of bytes to send");
	    break;
	case 'R':
	    recvtimes = strtol(optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve number of bytes to receive");
	    break;
	case 'f':
	    filename = optarg;
	    break;
	case 'D':
	    dumpstats = 1;
	    break;
	case 'j':
	  nojumbo=1;
	  break;
	case 'm':
	  maxmtu = strtol(optarg, &ptr, 0);
	  if (ptr && *ptr != '\0')
	    errx(1, "can't resolve rx maxmtu to use");
	    break;
	case 'u':
	    udpbufsz = strtol(optarg, &ptr, 0) * 1024;
	    if (ptr && *ptr != '\0')
		errx(1, "can't resolve upd buffer size (Kbytes)");
	    break;
	case '4':
	  RX_IPUDP_SIZE = 28;
	  break;
	default:
	    usage();
	}
    }

    if (optind != argc)
	usage();

    if (cmd == RX_PERF_UNKNOWN)
	errx(1, "no command given to the client");

    do_client(host, port, filename, cmd, times, bytes, sendtimes,
	      recvtimes, dumpstats, nojumbo, maxmtu, udpbufsz);

    return 0;
}

/*
 * setup world and call cmd
 */

int
main(int argc, char **argv)
{
#ifndef AFS_PTHREAD_ENV
    PROCESS pid;
#endif

    __progname = strrchr(argv[0], '/');
    if (__progname == 0)
	__progname = argv[0];

#ifndef AFS_NT40_ENV
    signal(SIGUSR1, sigusr1);
    signal(SIGINT, sigint);
#endif

#ifndef AFS_PTHREAD_ENV
    LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &pid);
#endif

    memset(somebuf, 0, sizeof(somebuf));

    if (argc >= 2 && strcmp(argv[1], "server") == 0)
	rxperf_server(argc - 1, argv + 1);
    else if (argc >= 2 && strcmp(argv[1], "client") == 0)
	rxperf_client(argc - 1, argv + 1);
    else
	usage();
    return 0;
}
