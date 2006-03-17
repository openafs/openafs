/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_VOL_DAEMON_COM_H
#define _AFS_VOL_DAEMON_COM_H

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

/* general command codes */
#define SYNC_COM_CHANNEL_CLOSE 0


/* SYNC protocol response codes
 *
 * response codes 0-65535 are reserved for 
 * global SYNC package response codes
 */
#define SYNC_RES_CODE_USER_BASE 65536
#define SYNC_RES_CODE_DECL(code) (SYNC_RES_CODE_USER_BASE+(code))

/* general response codes */
#define SYNC_OK                0   /* sync call returned ok */
#define SYNC_DENIED            1   /* sync request denied by server */
#define SYNC_COM_ERROR         2   /* sync protocol communicaions error */
#define SYNC_BAD_COMMAND       3   /* sync command code not implemented by server */
#define SYNC_FAILED            4   /* sync server-side procedure failed */


/* SYNC protocol reason codes
 *
 * reason codes 0-65535 are reserved for
 * global SYNC package reason codes
 */
#define SYNC_REASON_CODE_USER_BASE 65536
#define SYNC_REASON_CODE_DECL(code) (SYNC_REASON_CODE_USER_BASE+(code))

/* general reason codes */
#define SYNC_REASON_NONE                 0
#define SYNC_REASON_MALFORMED_PACKET     1


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


/* client-side state object */
typedef struct SYNC_client_state {
    int fd;
    afs_uint16 port;
    afs_uint32 proto_version;
    int retry_limit;            /* max number of times for SYNC_ask to retry */
    afs_int32 hard_timeout;     /* upper limit on time to keep trying */
    byte fatal_error;           /* fatal error on this client conn */
} SYNC_client_state;

/* wire types */
typedef struct SYNC_command_hdr {
    afs_uint32 proto_version;   /* sync protocol version */
    afs_int32 programType;      /* type of program issuing the request */
    afs_int32 command;          /* request type */
    afs_int32 reason;           /* reason for request */
    afs_uint32 command_len;     /* entire length of command */
    afs_uint32 flags;
} SYNC_command_hdr;

typedef struct SYNC_response_hdr {
    afs_uint32 proto_version;    /* sync protocol version */
    afs_uint32 response_len;    /* entire length of response */
    afs_int32 response;         /* response code */
    afs_int32 reason;           /* reason for response */
    afs_uint32 flags;
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


/* client-side prototypes */
extern afs_int32 SYNC_ask(SYNC_client_state *, SYNC_command * com, SYNC_response * res);
extern int SYNC_connect(SYNC_client_state *);             /* setup the channel */
extern int SYNC_disconnect(SYNC_client_state *);          /* just close the socket */
extern afs_int32 SYNC_closeChannel(SYNC_client_state *);  /* do a graceful channel close */
extern int SYNC_reconnect(SYNC_client_state *);           /* do a reconnect after a protocol error, or from a forked child */

/* server-side prototypes */
extern int SYNC_getCom(int fd, SYNC_command * com);
extern int SYNC_putRes(int fd, SYNC_response * res);
extern int SYNC_verifyProtocolString(char * buf, size_t len);

#endif /* _AFS_VOL_DAEMON_COM_H */
