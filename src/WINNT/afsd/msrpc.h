/*
 * Copyright (c) 2009 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <Rpc.h>
#include <RpcdceP.h>
#include <RpcNdr.h>
#include "cm_nls.h"

#define I_RpcGetBuffer MSRPC_I_GetBuffer

#define I_RpcFreeBuffer MSRPC_I_FreeBuffer

#define NdrServerInitializeNew MSRPC_NdrServerInitializeNew

/*! \brief Maximum size of an accepted message

   This value is arbitrary.  The chosen value is larger than the
   maximum fragment size used by the SMB RPC transport and is only
   meant to mitigate the damage that could be done from a malformed
   packet.
 */
#define MAX_RPC_MSG_SIZE 102400

/*! \brief Default message size */
#define DEF_RPC_MSG_SIZE 2048

/*! \brief Status of a msrpc_call object */
typedef enum msrpc_call_status {
    MSRPC_CALL_NONE = 0,
    MSRPC_CALL_MSGRECEIVED,
    MSRPC_CALL_DISPATCHED,
    MSRPC_CALL_MSGSENDING,
    MSRPC_CALL_MSGSENT,
    MSRPC_CALL_SKIP,
} MSRPC_CALL_STATUS;

typedef enum PDU_type {
    PDU_TYPE_REQUEST = 0,
    PDU_TYPE_PING = 1,
    PDU_TYPE_RESPONSE = 2,
    PDU_TYPE_FAULT = 3,
    PDU_TYPE_WORKING = 4,
    PDU_TYPE_NOCALL = 5,
    PDU_TYPE_REJECT = 6,
    PDU_TYPE_ACK = 7,
    PDU_TYPE_CL_CANCEL = 8,
    PDU_TYPE_FACK = 9,
    PDU_TYPE_CANCEL_ACK = 10,
    PDU_TYPE_BIND = 11,
    PDU_TYPE_BIND_ACK = 12,
    PDU_TYPE_BIND_NAK = 13,
    PDU_TYPE_ALTER_CONTEXT = 14,
    PDU_TYPE_ALTER_CONTEXT_RESP = 15,
    PDU_TYPE_SHUTDOWN = 16,
    PDU_TYPE_CO_CANCEL = 18,
    PDU_TYPE_ORPHANED = 19
} PDU_TYPE;

typedef enum msrpc_bind_reject_reason {
    BIND_REJ_REASON_NOT_SPECIFIED = 0,
    BIND_REJ_TEMPORARY_CONGESTION = 1,
    BIND_REJ_LOCAL_LIMIT_EXCEEDED = 2,
    BIND_REJ_CALLED_PADDR_UNKNOWN = 3, /* not used */
    BIND_REJ_PROTOCOL_VERSION_NOT_SUPPORTED = 4,
    BIND_REJ_DEFAULT_CONTEXT_NOT_SUPPORTED = 5, /* not used */
    BIND_REJ_USER_DATA_NOT_READABLE = 6, /* not used */
    BIND_REJ_NO_PSAP_AVAILABLE = 7 /* not used */
} BIND_REJECT_REASON;

typedef unsigned __int8	    byte;
typedef unsigned __int8	    u_int8;
typedef unsigned __int16    u_int16;
typedef unsigned __int32    u_int32;
typedef unsigned __int64    u_int64;

/*! \brief Common RPC envelope header */
typedef
struct E_CommonHeader {
    u_int8  rpc_vers;		/*!< 00:01 RPC version == 5 */
    u_int8  rpc_vers_minor;	/*!< 01:01 minor version */
    u_int8  PTYPE;		/*!< 02:01 packet type */
    u_int8  pfc_flags;		/*!< 03:01 flags (see PFC_... ) */

#define PFC_FIRST_FRAG           0x01 /* First fragment */
#define PFC_LAST_FRAG            0x02 /* Last fragment */
#define PFC_PENDING_CANCEL       0x04 /* Cancel was pending at sender */
#define PFC_RESERVED_1           0x08
#define PFC_CONC_MPX             0x10 /* supports concurrent multiplexing
				       * of a single connection. */
#define PFC_DID_NOT_EXECUTE      0x20 /* only meaningful on `fault' packet;
				       * if true, guaranteed call did not
				       * execute. */
#define PFC_MAYBE                0x40 /* `maybe' call semantics requested */
#define PFC_OBJECT_UUID          0x80 /* if true, a non-nil object UUID
				       * was specified in the handle, and
				       * is present in the optional object
				       * field. If false, the object field
				       * is omitted. */

    byte    packed_drep[4]; /*!< 04:04 NDR data representation format label */
    u_int16 frag_length;    /*!< 08:02 total length of fragment */
    u_int16 auth_length;    /*!< 10:02 length of auth_value */
    u_int32 call_id;	    /*!< 12:04 call identifier */
} E_CommonHeader;

typedef   u_int16   p_context_id_t; /* local context identifier */

/*! \brief RPC message buffer
 */
typedef struct msrpc_buffer {
    BYTE * buf_data;
    unsigned int buf_length;
    unsigned int buf_alloc;
    unsigned int buf_pos;

    PDU_TYPE pdu_type;
} msrpc_buffer;

typedef struct cm_user cm_user_t;

/*! \brief RPC call

   A single RPC call.  This might even be a single fragment of a
   message.
 */
typedef struct msrpc_call {
    struct msrpc_call * next;
    struct msrpc_call * prev;

    MSRPC_CALL_STATUS status;

    u_int32 call_id;	       /*!< Call ID */
    p_context_id_t context_id;	/*!< presentation context ID (last seen) */

    E_CommonHeader * in_header;	/*!< Pointer to the header of the
				  input packet.  Only valid during a
				  request dispatch. */

    msrpc_buffer in;		/*!< Input buffer */

    msrpc_buffer out;		/*!< Output buffer */

    RPC_MESSAGE msg;

    cm_user_t *cm_userp;	/*!< Held reference to the cm_user_t
                                   associated with this call. */
} msrpc_call;

/*! \brief RPC connection structure

   Represents a single RPC connection.  Can support multiple queued
   RPCs.
*/
typedef struct _msrpc_conn {
    unsigned int max_xmit_frag;	/*!< Maximum size of fragment
                                   client->server */
    unsigned int max_recv_frag;	/*!< Maximum size of fragment
                                   server->client */
    u_int32 assoc_group_id;	/*!< Association group ID for the
                                   connection. */

    unsigned int rpc_vers;	/*!< RPC Protocol version. Always 5 */
    unsigned int rpc_vers_minor; /*!< Minor version number.  0 or 1 */

    char * secondary_address;	/*!< Secondary address for the
                                   connection. */

    RPC_SERVER_INTERFACE * interface;
				/*!< Selected server interface.  Only
                                   valid after a successful bind. */

    msrpc_call * head;		/*!< Queue of msrpc_call objects for
                                   the connection. */
    msrpc_call * tail;
} msrpc_conn;


void
MSRPC_Init(void);

void
MSRPC_Shutdown(void);

int
MSRPC_InitConn(msrpc_conn * conn, const char * secondary_address);

int
MSRPC_FreeConn(msrpc_conn * conn);

int
MSRPC_WriteMessage(msrpc_conn * conn, BYTE * buffer, unsigned int len,
		   cm_user_t * userp);

int
MSRPC_PrepareRead(msrpc_conn * conn);

int
MSRPC_ReadMessageLength(msrpc_conn * conn, unsigned int max_len);

int
MSRPC_ReadMessage(msrpc_conn * conn, BYTE *buffer, unsigned int len);

RPC_STATUS
RPC_ENTRY
I_RpcGetBuffer (IN OUT RPC_MESSAGE __RPC_FAR * Message);

RPC_STATUS
RPC_ENTRY
I_RpcFreeBuffer (IN OUT RPC_MESSAGE __RPC_FAR * Message);

unsigned char  *
RPC_ENTRY
NdrServerInitializeNew(
    PRPC_MESSAGE            pRpcMsg,
    PMIDL_STUB_MESSAGE      pStubMsg,
    PMIDL_STUB_DESC         pStubDescriptor
    );

extern cm_user_t *
MSRPC_GetCmUser(void);

extern void RPC_SRVSVC_Init(void);

extern void RPC_SRVSVC_Shutdown(void);

extern int
MSRPC_IsWellKnownService(const clientchar_t * lastNamep);
