/*
 * Copyright 2006-2008, Sine Nomine Associates and others.
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
#include <string.h>


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

#ifdef USE_UNIX_SOCKETS
#include <afs/afsutil.h>
#include <sys/un.h>
#endif

/*@printflike@*/ extern void Log(const char *format, ...);

#ifdef osi_Assert
#undef osi_Assert
#endif
#define osi_Assert(e) (void)(e)

int (*V_BreakVolumeCallbacks) ();

#define MAXHANDLERS	4	/* Up to 4 clients; must be at least 2, so that
				 * move = dump+restore can run on single server */

#define MAX_BIND_TRIES	5	/* Number of times to retry socket bind */

static int SYNC_ask_internal(SYNC_client_state * state, SYNC_command * com, SYNC_response * res);

/* daemon com SYNC general interfaces */

/**
 * fill in sockaddr structure.
 *
 * @param[in]  endpoint pointer to sync endpoint object
 * @param[out] addr     pointer to sockaddr structure
 *
 * @post sockaddr structure populated using information from
 *       endpoint structure.
 */
void
SYNC_getAddr(SYNC_endpoint_t * endpoint, SYNC_sockaddr_t * addr)
{
#ifdef USE_UNIX_SOCKETS
    char tbuffer[AFSDIR_PATH_MAX];
#endif /* USE_UNIX_SOCKETS */

    memset(addr, 0, sizeof(*addr));

#ifdef USE_UNIX_SOCKETS
    strcompose(tbuffer, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH, "/",
               endpoint->un, NULL);
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, tbuffer, (sizeof(struct sockaddr_un) - sizeof(short)));
#else  /* !USE_UNIX_SOCKETS */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_addr.s_addr = htonl(0x7f000001);
    addr->sin_family = AF_INET;	/* was localhost->h_addrtype */
    addr->sin_port = htons(endpoint->in);	/* XXXX htons not _really_ neccessary */
#endif /* !USE_UNIX_SOCKETS */
}

/**
 * get a socket descriptor of the appropriate domain.
 *
 * @param[in]  endpoint pointer to sync endpoint object
 *
 * @return socket descriptor
 *
 * @post socket of domain specified in endpoint structure is created and
 *       returned to caller.
 */
int
SYNC_getSock(SYNC_endpoint_t * endpoint)
{
    int sd;
    assert((sd = socket(endpoint->domain, SOCK_STREAM, 0)) >= 0);
    return sd;
}

/* daemon com SYNC client interface */

/**
 * open a client connection to a sync server
 *
 * @param[in] state  pointer to sync client handle
 *
 * @return operation status
 *    @retval 1 success
 *
 * @note at present, this routine aborts rather than returning an error code
 */
int
SYNC_connect(SYNC_client_state * state)
{
    SYNC_sockaddr_t addr;
    /* I can't believe the following is needed for localhost connections!! */
    static time_t backoff[] =
	{ 3, 3, 3, 5, 5, 5, 7, 15, 16, 24, 32, 40, 48, 0 };
    time_t *timeout = &backoff[0];

    if (state->fd >= 0) {
	return 1;
    }

    SYNC_getAddr(&state->endpoint, &addr);

    for (;;) {
	state->fd = SYNC_getSock(&state->endpoint);
	if (connect(state->fd, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
	    return 1;
	if (!*timeout)
	    break;
	if (!(*timeout & 1))
	    Log("SYNC_connect: temporary failure on circuit '%s' (will retry)\n",
		state->proto_name);
	SYNC_disconnect(state);
	sleep(*timeout++);
    }
    perror("SYNC_connect failed (giving up!)");
    return 0;
}

/**
 * forcibly disconnect a sync client handle.
 *
 * @param[in] state  pointer to sync client handle
 *
 * @retval operation status
 *    @retval 0 success
 */
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

/**
 * gracefully disconnect a sync client handle.
 *
 * @param[in] state  pointer to sync client handle
 *
 * @return operation status
 *    @retval SYNC_OK success
 */
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
    com.hdr.flags |= SYNC_FLAG_CHANNEL_SHUTDOWN;

    /* in case the other end dropped, don't do any retries */
    state->retry_limit = 0;
    state->hard_timeout = 0;

    SYNC_ask(state, &com, &res);
    SYNC_disconnect(state);

    return SYNC_OK;
}

/**
 * forcibly break a client connection, and then create a new connection.
 *
 * @param[in] state  pointer to sync client handle
 *
 * @post old connection dropped; new connection established
 *
 * @return @see SYNC_connect()
 */
int
SYNC_reconnect(SYNC_client_state * state)
{
    SYNC_disconnect(state);
    return SYNC_connect(state);
}

/**
 * send a command to a sync server and wait for a response.
 *
 * @param[in]  state  pointer to sync client handle
 * @param[in]  com    command object
 * @param[out] res    response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_COM_ERROR communications error
 *    @retval SYNC_BAD_COMMAND server did not recognize command code
 *
 * @note this routine merely handles error processing; SYNC_ask_internal()
 *       handles the low-level details of communicating with the SYNC server.
 *
 * @see SYNC_ask_internal
 */
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
	    Log("SYNC_ask: protocol mismatch on circuit '%s'; make sure "
		"fileserver, volserver, salvageserver and salvager are same "
		"version\n", state->proto_name);
	    break;
	} else if ((code == SYNC_COM_ERROR) && (tries < state->retry_limit)) {
	    Log("SYNC_ask: protocol communications failure on circuit '%s'; "
		"attempting reconnect to server\n", state->proto_name);
	    SYNC_reconnect(state);
	    /* try again */
	} else {
	    /* 
	     * unknown (probably protocol-specific) response code, pass it up to 
	     * the caller, and let them deal with it 
	     */
	    break;
	}
    }

    if (code == SYNC_COM_ERROR) {
	Log("SYNC_ask: fatal protocol error on circuit '%s'; disabling sync "
	    "protocol until next server restart\n", 
	    state->proto_name);
	state->fatal_error = 1;
    }

    return code;
}

/**
 * send a command to a sync server and wait for a response.
 *
 * @param[in]  state  pointer to sync client handle
 * @param[in]  com    command object
 * @param[out] res    response object
 *
 * @return operation status
 *    @retval SYNC_OK success
 *    @retval SYNC_COM_ERROR communications error
 *
 * @internal
 */
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
	Log("SYNC_ask:  invalid sync file descriptor on circuit '%s'\n",
	    state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    if (com->hdr.command_len > SYNC_PROTO_MAX_LEN) {
	Log("SYNC_ask:  internal SYNC buffer too small on circuit '%s'; "
	    "please file a bug\n", state->proto_name);
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
	Log("SYNC_ask:  write failed on circuit '%s'\n", state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    if (com->hdr.command == SYNC_COM_CHANNEL_CLOSE) {
	/* short circuit close channel requests */
	res->hdr.response = SYNC_OK;
	goto done;
    }

    n = recv(state->fd, buf, SYNC_PROTO_MAX_LEN, 0);
    if (n == 0 || (n < 0 && WSAEINTR != WSAGetLastError())) {
	Log("SYNC_ask:  No response on circuit '%s'\n", state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#else /* !AFS_NT40_ENV */
    n = write(state->fd, buf, com->hdr.command_len);
    if (com->hdr.command_len != n) {
	Log("SYNC_ask: write failed on circuit '%s'\n", state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }

    if (com->hdr.command == SYNC_COM_CHANNEL_CLOSE) {
	/* short circuit close channel requests */
	res->hdr.response = SYNC_OK;
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
	Log("SYNC_ask: No response on circuit '%s'\n", state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#endif /* !AFS_NT40_ENV */

    res->recv_len = n;

    if (n < sizeof(res->hdr)) {
	Log("SYNC_ask:  response too short on circuit '%s'\n", 
	    state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(&res->hdr, buf, sizeof(res->hdr));
#endif

    if ((n - sizeof(res->hdr)) > res->payload.len) {
	Log("SYNC_ask:  response too long on circuit '%s'\n", 
	    state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
#ifdef AFS_NT40_ENV
    memcpy(res->payload.buf, buf + sizeof(res->hdr), n - sizeof(res->hdr));
#endif

    if (res->hdr.response_len != n) {
	Log("SYNC_ask:  length field in response inconsistent "
	    "on circuit '%s'\n", state->proto_name);
	res->hdr.response = SYNC_COM_ERROR;
	goto done;
    }
    if (res->hdr.response == SYNC_DENIED) {
	Log("SYNC_ask: negative response on circuit '%s'\n", state->proto_name);
    }

  done:
    return res->hdr.response;
}


/* 
 * daemon com SYNC server-side interfaces 
 */

/**
 * receive a command structure off a sync socket.
 *
 * @param[in] fd    socket descriptor
 * @param[out] com  sync command object to be populated
 *
 * @return operation status
 *    @retval SYNC_OK command received
 *    @retval SYNC_COM_ERROR there was a socket communications error
 */
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

/**
 * write a response structure to a sync socket.
 *
 * @param[in] fd
 * @param[in] res
 *
 * @return operation status
 *    @retval SYNC_OK
 *    @retval SYNC_COM_ERROR
 */
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

/**
 * clean up old sockets.
 *
 * @param[in]  state  server state object
 *
 * @post unix domain sockets are cleaned up
 */
void
SYNC_cleanupSock(SYNC_server_state_t * state)
{
#ifdef USE_UNIX_SOCKETS
    remove(state->addr.sun_path);
#endif
}

/**
 * bind socket and set it to listen state.
 *
 * @param[in] state  server state object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval nonzero failure
 *
 * @post socket bound and set to listen state
 */
int
SYNC_bindSock(SYNC_server_state_t * state)
{
    int code;
    int on = 1;
    int numTries;

    /* Reuseaddr needed because system inexplicably leaves crud lying around */
    code =
	setsockopt(state->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
		   sizeof(on));
    if (code)
	Log("SYNC_bindSock: setsockopt failed with (%d)\n", errno);

    for (numTries = 0; numTries < state->bind_retry_limit; numTries++) {
	code = bind(state->fd, 
		    (struct sockaddr *)&state->addr, 
		    sizeof(state->addr));
	if (code == 0)
	    break;
	Log("SYNC_bindSock: bind failed with (%d), will sleep and retry\n",
	    errno);
	sleep(5);
    }
    listen(state->fd, state->listen_depth);

    return code;
}
