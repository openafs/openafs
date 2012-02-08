/*
 * Copyright 2006-2008, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_VOL_DAEMON_COM_H
#define _AFS_VOL_DAEMON_COM_H 1

/*
 * SYNC protocol constants
 */

/* SYNC protocol command codes
 *
 * command codes 0-65535 are reserved for
 * global SYNC package command codes
 */
#define SYNC_COM_CODE_USER_BASE 65536
#define SYNC_COM_CODE_DECL(code) (SYNC_COM_CODE_USER_BASE+(code))

/**
 * general command codes.
 */
enum SYNCOpCode {
    SYNC_COM_CHANNEL_CLOSE    = 0,      /**< request sync channel shutdown */
    SYNC_OP_CODE_END
};


/* SYNC protocol response codes
 *
 * response codes 0-65535 are reserved for
 * global SYNC package response codes
 */
#define SYNC_RES_CODE_USER_BASE 65536
#define SYNC_RES_CODE_DECL(code) (SYNC_RES_CODE_USER_BASE+(code))

/**
 * general response codes.
 */
enum SYNCReasonCode {
    SYNC_OK                   = 0,  /**< sync call returned ok */
    SYNC_DENIED               = 1,  /**< sync request denied by server */
    SYNC_COM_ERROR            = 2,  /**< sync protocol communicaions error */
    SYNC_BAD_COMMAND          = 3,  /**< sync command code not implemented by server */
    SYNC_FAILED               = 4,  /**< sync server-side procedure failed */
    SYNC_RESPONSE_CODE_END
};

/* SYNC protocol reason codes
 *
 * reason codes 0-65535 are reserved for
 * global SYNC package reason codes
 */
#define SYNC_REASON_CODE_USER_BASE 65536
#define SYNC_REASON_CODE_DECL(code) (SYNC_REASON_CODE_USER_BASE+(code))

/* general reason codes */
#define SYNC_REASON_NONE                 0
#define SYNC_REASON_MALFORMED_PACKET     1   /**< command packet was malformed */
#define SYNC_REASON_NOMEM                2   /**< sync server out of memory */
#define SYNC_REASON_ENCODING_ERROR       3
#define SYNC_REASON_PAYLOAD_TOO_BIG      4   /**< payload too big for response packet buffer */

/* SYNC protocol flags
 *
 * flag bits 0-7 are reserved for
 * global SYNC package flags
 */
#define SYNC_FLAG_CODE_USER_BASE 8
#define SYNC_FLAG_CODE_DECL(code) (1 << (SYNC_FLAG_CODE_USER_BASE+(code)))

/* general flag codes */
#define SYNC_FLAG_CHANNEL_SHUTDOWN   0x1
#define SYNC_FLAG_DAFS_EXTENSIONS    0x2   /* signal that other end of socket is compiled
					    * with demand attach extensions */

/* SYNC protocol response buffers */
#define SYNC_PROTO_MAX_LEN     768  /* maximum size of sync protocol message */

/* use a large type to get proper buffer alignment so we can safely cast the pointer */
#define SYNC_PROTO_BUF_DECL(buf) \
    afs_int64 _##buf##_l[SYNC_PROTO_MAX_LEN/sizeof(afs_int64)]; \
    char * buf = (char *)(_##buf##_l)

#ifdef AFS_LINUX26_ENV
/* Some Linux kernels have a bug where we are not woken up immediately from a
 * select() when data is available. Work around this by having a low select()
 * timeout, so we don't hang in those situations. */
# define SYNC_SELECT_TIMEOUT 10
#else
# define SYNC_SELECT_TIMEOUT 86400
#endif

#ifdef USE_UNIX_SOCKETS
#include <afs/afsutil.h>
#include <sys/un.h>
#define SYNC_SOCK_DOMAIN AF_UNIX
typedef struct sockaddr_un SYNC_sockaddr_t;
#else  /* USE_UNIX_SOCKETS */
#define SYNC_SOCK_DOMAIN AF_INET
typedef struct sockaddr_in SYNC_sockaddr_t;
#endif /* USE_UNIX_SOCKETS */

/**
 * sync server endpoint address.
 */
typedef struct SYNC_endpoint {
    int domain;     /**< socket domain */
    afs_uint16 in;  /**< localhost ipv4 tcp port number */
    char * un;      /**< unix domain socket filename (not a full path) */
} SYNC_endpoint_t;

#define SYNC_ENDPOINT_DECL(in_port, un_path) \
    { SYNC_SOCK_DOMAIN, in_port, un_path }


/**
 * SYNC server state structure.
 */
typedef struct SYNC_server_state {
    osi_socket fd;              /**< listening socket descriptor */
    SYNC_endpoint_t endpoint;   /**< server endpoint address */
    afs_uint32 proto_version;   /**< our protocol version */
    int bind_retry_limit;       /**< upper limit on times to retry socket bind() */
    int listen_depth;           /**< socket listen queue depth */
    char * proto_name;          /**< sync protocol associated with this conn */
    SYNC_sockaddr_t addr;       /**< server listen socket sockaddr */
    afs_uint32 pkt_seq;         /**< packet xmit sequence counter */
    afs_uint32 res_seq;         /**< response xmit sequence counter */
} SYNC_server_state_t;

/**
 * SYNC client state structure.
 */
typedef struct SYNC_client_state {
    osi_socket fd;              /**< client socket descriptor */
    SYNC_endpoint_t endpoint;   /**< address of sync server */
    afs_uint32 proto_version;   /**< our protocol version */
    int retry_limit;            /**< max number of times for SYNC_ask to retry */
    afs_int32 hard_timeout;     /**< upper limit on time to keep trying */
    char * proto_name;          /**< sync protocol associated with this conn */
    afs_uint32 pkt_seq;         /**< packet xmit sequence counter */
    afs_uint32 com_seq;         /**< command xmit sequence counter */
} SYNC_client_state;

/* wire types */
/**
 * on-wire command packet header.
 */
typedef struct SYNC_command_hdr {
    afs_uint32 proto_version;   /**< sync protocol version */
    afs_uint32 pkt_seq;         /**< packet sequence number */
    afs_uint32 com_seq;         /**< command sequence number */
    afs_int32 programType;      /**< type of program issuing the request */
    afs_int32 pid;              /**< pid of requestor */
    afs_int32 tid;              /**< thread id of requestor */
    afs_int32 command;          /**< request type */
    afs_int32 reason;           /**< reason for request */
    afs_uint32 command_len;     /**< entire length of command */
    afs_uint32 flags;           /**< miscellanous control flags */
} SYNC_command_hdr;

/**
 * on-wire response packet header.
 */
typedef struct SYNC_response_hdr {
    afs_uint32 proto_version;   /**< sync protocol version */
    afs_uint32 pkt_seq;         /**< packet sequence number */
    afs_uint32 com_seq;         /**< in response to com_seq... */
    afs_uint32 res_seq;         /**< response sequence number */
    afs_uint32 response_len;    /**< entire length of response */
    afs_int32 response;         /**< response code */
    afs_int32 reason;           /**< reason for response */
    afs_uint32 flags;           /**< miscellanous control flags */
} SYNC_response_hdr;


/* user-visible types */
typedef struct SYNC_command {
    SYNC_command_hdr hdr;
    struct {
	afs_uint32 len;
	void * buf;
    } payload;
    afs_int32 recv_len;
} SYNC_command;

typedef struct SYNC_response {
    SYNC_response_hdr hdr;
    struct {
	afs_uint32 len;
	void * buf;
    } payload;
    afs_int32 recv_len;
} SYNC_response;

/* general prototypes */
extern osi_socket SYNC_getSock(SYNC_endpoint_t * endpoint);
extern void SYNC_getAddr(SYNC_endpoint_t * endpoint, SYNC_sockaddr_t * addr);

/* client-side prototypes */
extern afs_int32 SYNC_ask(SYNC_client_state *, SYNC_command * com, SYNC_response * res);
extern int SYNC_connect(SYNC_client_state *);             /* setup the channel */
extern int SYNC_disconnect(SYNC_client_state *);          /* just close the socket */
extern afs_int32 SYNC_closeChannel(SYNC_client_state *);  /* do a graceful channel close */
extern int SYNC_reconnect(SYNC_client_state *);           /* do a reconnect after a protocol error, or from a forked child */

/* server-side prototypes */
extern int SYNC_getCom(SYNC_server_state_t *, osi_socket fd, SYNC_command * com);
extern int SYNC_putRes(SYNC_server_state_t *, osi_socket fd, SYNC_response * res);
extern int SYNC_verifyProtocolString(char * buf, size_t len);
extern void SYNC_cleanupSock(SYNC_server_state_t * state);
extern int SYNC_bindSock(SYNC_server_state_t * state);

#endif /* _AFS_VOL_DAEMON_COM_H */
