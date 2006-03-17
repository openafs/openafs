/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * localhost interprocess communication for servers
 *
 * currently handled by a localhost socket
 * (yes, this needs to be replaced someday)
 */

#ifndef _WIN32
#define FD_SETSIZE 65536
#endif

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <time.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <errno.h>
#include <assert.h>
#include <signal.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "daemon_com.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include <rx/rx_queue.h>

/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)

int (*V_BreakVolumeCallbacks) ();

#define MAXHANDLERS	4	/* Up to 4 clients; must be at least 2, so that
				 * move = dump+restore can run on single server */

#define MAX_BIND_TRIES	5	/* Number of times to retry socket bind */

static int getport(SYNC_client_state * state, struct sockaddr_in *addr);
static int SYNC_ask_internal(SYNC_client_state * state, SYNC_command * com, SYNC_response * res);

/* daemon com SYNC client interface */

int
SYNC_connect(SYNC_client_state * state)
{
    struct sockaddr_in addr;
    /* I can't believe the following is needed for localhost connections!! */
    static time_t backoff[] =
	{ 3, 3, 3, 5, 5, 5, 7, 15, 16, 24, 32, 40, 48, 0 };
    time_t *timeout = &backoff[0];

    if (state->fd >= 0) {
	return 1;
    }

    for (;;) {
	state->fd = getport(state, &addr);
	if (connect(state->fd, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
	    return 1;
	if (!*timeout)
	    break;
	if (!(*timeout & 1))
	    Log("SYNC_connect temporary failure (will retry)\n");
	SYNC_disconnect(state);
	sleep(*timeout++);
    }
    perror("SYNC_connect failed (giving up!)");
    return 0;
}

int
SYNC_disconnect(SYNC_client_state * state)
{
#ifdef AFS_NT40_ENV
    closesocket(state->fd);
#else
    close(state->fd);
#endif
    state->fd = -1;
    return 0;
}

afs_int32
SYNC_closeChannel(SYNC_client_state * state)
{
    afs_int32 code;
    SYNC_command com;
    SYNC_response res;
    SYNC_PROTO_BUF_DECL(ores);

    if (state->fd == -1)
	return SYNC_OK;

    memset(&com, 0, sizeof(com));
    memset(&res, 0, sizeof(res));

    res.payload.len = SYNC_PROTO_MAX_LEN;
    res.payload.buf = ores;

    com.hdr.command = SYNC_COM_CHANNEL_CLOSE;
    com.hdr.command_len = sizeof(SYNC_command_hdr);

    /* in case the other end dropped, don't do any retries */
    state->retry_limit = 0;
    state->hard_timeout = 0;

    code = SYNC_ask(state, &com, &res);

    if (code == SYNC_OK) {
	if (res.hdr.response != SYNC_OK) {
	    Log("SYNC_closeChannel:  channel shutdown request denied; closing socket anyway\n");
	} else if (!(res.hdr.flags & SYNC_FLAG_CHANNEL_SHUTDOWN)) {
	    Log("SYNC_closeChannel:  channel shutdown request mishandled by server\n");
	}
    } else {
	Log("SYNC_closeChannel: channel communications problem");
    }

    SYNC_disconnect(state);

    return code;
}

int
SYNC_reconnect(SYNC_client_state * state)
{
    SYNC_disconnect(state);
    return SYNC_connect(state);
}

/* private function to fill in the sockaddr struct for us */
static int
getport(SYNC_client_state * state, struct sockaddr_in *addr)
{
    int sd;

    memset(addr, 0, sizeof(*addr));
    assert((sd = socket(AF_INET, SOCK_STREAM, 0)) >= 0);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_addr.s_addr = htonl(0x7f000001);
    addr->sin_family = AF_INET;	/* was localhost->h_addrtype */
    addr->sin_port = htons(state->port);	/* XXXX htons not _really_ neccessary */

    return sd;
}

afs_int32
SYNC_ask(SYNC_client_state * state, SYNC_command * com, SYNC_response * res)
{
    int tries;
    afs_uint32 now, timeout, code=SYNC_OK;

    if (state->fatal_error) {
	return SYNC_COM_ERROR;
    }

    if (state->fd == -1) {
	SYNC_connect(state);
    }

    if (state->fd == -1) {
	state->fatal_error = 1;
	return SYNC_COM_ERROR;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    com->hdr.flags |= SYNC_FLAG_DAFS_EXTENSIONS;
#endif

    now = FT_ApproxTime();
    timeout = now + state->hard_timeout;
    for (tries = 0; 
	 (tries <= state->retry_limit) && (now <= timeout);
	 tries++, now = FT_ApproxTime()) {
	code = SYNC_ask_internal(state, com, res);
	if (code == SYNC_OK) {
	    break;
	} else if (code == SYNC_BAD_COMMAND) {
	    Log("SYNC_ask: protocol mismatch; make sure fileserver, volserver, salvageserver and salvager are same version\n");
	    break;
	} else if (code == SYNC_COM_ERROR) {
	    Log("SYNC_ask: protocol communications failure; attempting reconnect to server\n");
	    SYNC_reconnect(state);
	    /* try again */
	} else {
	    /* unknown (probably protocol-specific) response code, pass it up to the caller, and let them deal with it */
	    break;
	}
    }

    if (code == SYNC_COM_ERROR) {
	Log("SYNC_ask: fatal protocol error; disabling sync protocol to server running on port %d until next server restart\n", 
	    state->port);
	state->fatal_error = 1;
    }

    return code;
}

static afs_int32
SYNC_ask_internal(SYNC_client_state * state, SYNC_command * com, SYNC_response * res)
{
    int n;
    SYNC_PROTO_BUF_DECL(buf);
#ifndef AFS_NT40_ENV
    int iovcnt;
    struct iovec iov[2];
#endif

    if (state->fd == -1) {
	Log("SYNC_ask:  invalid sync file descriptor\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    if (com->hdr.command_len > SYNC_PROTO_MAX_LEN) {
	Log("SYNC_ask:  internal SYNC buffer too small; please file a bug\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    com->hdr.proto_version = state->proto_version;

    memcpy(buf, &com->hdr, sizeof(com->hdr));
    if (com->payload.len) {
	memcpy(buf + sizeof(com->hdr), com->payload.buf, 
	       com->hdr.command_len - sizeof(com->hdr));
    }

#ifdef AFS_NT40_ENV
    n = send(state->fd, buf, com->hdr.command_len, 0);
    if (n != com->hdr.command_len) {
	Log("SYNC_ask:  write failed\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    n = recv(state->fd, buf, SYNC_PROTO_MAX_LEN, 0);
    if (n == 0 || (n < 0 && WSAEINTR != WSAGetLastError())) {
	Log("SYNC_ask:  No response\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#else /* !AFS_NT40_ENV */
    n = write(state->fd, buf, com->hdr.command_len);
    if (com->hdr.command_len != n) {
	Log("SYNC_ask: write failed\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    /* receive the response */
    iov[0].iov_base = (char *)&res->hdr;
    iov[0].iov_len = sizeof(res->hdr);
    if (res->payload.len) {
	iov[1].iov_base = (char *)res->payload.buf;
	iov[1].iov_len = res->payload.len;
	iovcnt = 2;
    } else {
	iovcnt = 1;
    }
    n = readv(state->fd, iov, iovcnt);
    if (n == 0 || (n < 0 && errno != EINTR)) {
	Log("SYNC_ask: No response\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#endif /* !AFS_NT40_ENV */

    res->recv_len = n;

    if (n < sizeof(res->hdr)) {
	Log("SYNC_ask:  response too short\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(&res->hdr, buf, sizeof(res->hdr));
#endif

    if ((n - sizeof(res->hdr)) > res->payload.len) {
	Log("SYNC_ask:  response too long\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(res->payload.buf, buf + sizeof(res->hdr), n - sizeof(res->hdr));
#endif

    if (res->hdr.response_len != n) {
	Log("SYNC_ask:  length field in response inconsistent\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
    if (res->hdr.response == SYNC_DENIED) {
	Log("SYNC_ask: negative response\n");
    }

  done:
    return res->hdr.response;
}


/* 
 * daemon com SYNC server-side interfaces 
 */

/* get a command */
afs_int32
SYNC_getCom(int fd, SYNC_command * com)
{
    int n;
    afs_int32 code = SYNC_OK;
#ifdef AFS_NT40_ENV
    SYNC_PROTO_BUF_DECL(buf);
#else
    struct iovec iov[2];
    int iovcnt;
#endif

#ifdef AFS_NT40_ENV
    n = recv(fd, buf, SYNC_PROTO_MAX_LEN, 0);

    if (n == 0 || (n < 0 && WSAEINTR != WSAGetLastError())) {
	Log("SYNC_getCom:  error receiving command\n");
	code = SYNC_COM_ERROR;
	goto done;
    }
#else /* !AFS_NT40_ENV */
    iov[0].iov_base = (char *)&com->hdr;
    iov[0].iov_len = sizeof(com->hdr);
    if (com->payload.len) {
	iov[1].iov_base = (char *)com->payload.buf;
	iov[1].iov_len = com->payload.len;
	iovcnt = 2;
    } else {
	iovcnt = 1;
    }

    n = readv(fd, iov, iovcnt);
    if (n == 0 || (n < 0 && errno != EINTR)) {
	Log("SYNC_getCom:  error receiving command\n");
	code = SYNC_COM_ERROR;
	goto done;
    }
#endif /* !AFS_NT40_ENV */

    com->recv_len = n;

    if (n < sizeof(com->hdr)) {
	Log("SYNC_getCom:  command too short\n");
	code = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(&com->hdr, buf, sizeof(com->hdr));
#endif

    if ((n - sizeof(com->hdr)) > com->payload.len) {
	Log("SYNC_getCom:  command too long\n");
	code = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(com->payload.buf, buf + sizeof(com->hdr), n - sizeof(com->hdr));
#endif

 done:
    return code;
}

/* put a response */
afs_int32
SYNC_putRes(int fd, SYNC_response * res)
{
    int n;
    afs_int32 code = SYNC_OK;
    SYNC_PROTO_BUF_DECL(buf);

    if (res->hdr.response_len > (sizeof(res->hdr) + res->payload.len)) {
	Log("SYNC_putRes:  response_len field in response header inconsistent\n");
	code = SYNC_COM_ERROR;
	goto done;
    }

    if (res->hdr.response_len > SYNC_PROTO_MAX_LEN) {
	Log("SYNC_putRes:  internal SYNC buffer too small; please file a bug\n");
	code = SYNC_COM_ERROR;
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    res->hdr.flags |= SYNC_FLAG_DAFS_EXTENSIONS;
#endif

    memcpy(buf, &res->hdr, sizeof(res->hdr));
    if (res->payload.len) {
	memcpy(buf + sizeof(res->hdr), res->payload.buf, 
	       res->hdr.response_len - sizeof(res->hdr));
    }

#ifdef AFS_NT40_ENV
    n = send(fd, buf, res->hdr.response_len, 0);
#else /* !AFS_NT40_ENV */
    n = write(fd, buf, res->hdr.response_len);
#endif /* !AFS_NT40_ENV */

    if (res->hdr.response_len != n) {
	Log("SYNC_putRes: write failed\n");
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

 done:
    return code;
}

/* return 0 for legal (null-terminated) string,
 * 1 for illegal (unterminated) string */
int
SYNC_verifyProtocolString(char * buf, size_t len)
{
    int ret = 0;
    size_t s_len;

    s_len = afs_strnlen(buf, len);

    return (s_len == len) ? 1 : 0;
}
