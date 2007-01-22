/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_MAIL_H
#define _OSI_TRACE_MAIL_H 1


/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 */

#include <osi/osi_list.h>
#include <trace/gen_rgy.h>
#include <osi/osi_refcnt.h>
#include <trace/mail/msg.h>

/*
 * mail result codes
 */
#define OSI_TRACE_MAIL_RESULT_NO_MAIL  OSI_RESULT_SUCCESS_CODE(OSI_TRACE_ERROR_MAILBOX_EMPTY)


/* constants */
#define OSI_TRACE_MAIL_BODY_LEN_MAX 4096

typedef osi_uint64 osi_trace_mail_xid_t;

/* routing envelope on all mail messages */
typedef struct osi_trace_mail_envelope {
    osi_uint8 env_proto_version;      /* protocol version */
    osi_uint8 env_len;                /* length of envelope */
    osi_trace_gen_id_t env_src;       /* source address */
    osi_trace_gen_id_t env_dst;       /* destination address */
    osi_uint16 env_proto_type;        /* protocol message type */
    osi_uint16 env_flags;             /* envelope flags */
    osi_trace_mail_xid_t env_xid;     /* transaction id or zero */
    osi_trace_mail_xid_t env_ref_xid; /* reference transaction id or zero */
} osi_trace_mail_envelope_t;

/*
 * envelope flags
 */
#define OSI_TRACE_MAIL_ENV_FLAG_RPC_REQ    0x1
#define OSI_TRACE_MAIL_ENV_FLAG_RPC_RES    0x2

/*
 * message-specific data which is local to a 
 * given process/kernel context
 */
typedef struct osi_trace_mail_message_local_data {
    osi_refcnt_t refcnt;
} osi_trace_mail_message_local_data_t;

/*
 * actual message object
 */
typedef struct osi_trace_mail_message {
    osi_trace_mail_envelope_t envelope;    /* this MUST be the first element in the structure */
    void * body;
    osi_uint32 body_len;
    osi_trace_mail_message_local_data_t * ldata;     /* pointer to special
						      * message data which we
						      * don't need to copy around
						      * between contexts */
} osi_trace_mail_message_t;

/*
 * mail queueing object
 */
typedef struct {
    osi_list_element_volatile msg_list;
    osi_trace_mail_message_t * osi_volatile msg;
} osi_trace_mail_queue_t;

/*
 * vectorized message object for mail syscalls
 */
#define OSI_TRACE_MAIL_MVEC_MAX 16
typedef struct osi_trace_mail_mvec {
    osi_trace_mail_message_t * mvec[OSI_TRACE_MAIL_MVEC_MAX];
    osi_uint32 nrec;
} osi_trace_mail_mvec_t;


/* 32-bit syscall interface support */
typedef struct osi_trace_mail_message32 {
    osi_trace_mail_envelope_t envelope;
    osi_uint32 body;
    osi_uint32 body_len;
} osi_trace_mail_message32_t;

typedef struct osi_trace_mail_mvec32 {
    osi_uint32 mvec[OSI_TRACE_MAIL_MVEC_MAX];
    osi_uint32 nrec;
} osi_trace_mail_mvec32_t;


/* package initialization */
osi_extern osi_result osi_trace_mail_PkgInit(void);
osi_extern osi_result osi_trace_mail_PkgShutdown(void);


/* 
 * msg buffer allocator interfaces
 */

osi_extern osi_result osi_trace_mail_msg_alloc(osi_uint32 body_len,
					       osi_trace_mail_message_t ** msg);
osi_extern osi_result osi_trace_mail_msg_get(osi_trace_mail_message_t * msg);
osi_extern osi_result osi_trace_mail_msg_put(osi_trace_mail_message_t * msg);
osi_extern osi_result osi_trace_mail_mq_alloc(osi_trace_mail_queue_t ** mq_out);
osi_extern osi_result osi_trace_mail_mq_free(osi_trace_mail_queue_t * mq);

/* 
 * msg header manipulation interfaces
 */

osi_extern osi_result osi_trace_mail_prepare_send(osi_trace_mail_message_t * msg,
						  osi_trace_gen_id_t dst,
						  osi_trace_mail_xid_t msg_xid,
						  osi_trace_mail_xid_t msg_ref_xid,
						  osi_trace_mail_msg_t proto_type);
osi_extern osi_result osi_trace_mail_prepare_response(osi_trace_mail_message_t * msg,
						      osi_trace_mail_message_t * rmsg,
						      osi_trace_mail_xid_t msg_xid,
						      osi_trace_mail_msg_t proto_type);

/*
 * transaction id allocator interfaces
 */

osi_extern osi_result osi_trace_mail_xid_alloc(osi_trace_mail_xid_t * xid);
osi_extern osi_result osi_trace_mail_xid_retire(osi_trace_mail_xid_t xid);

#endif /* _OSI_TRACE_MAIL_H */
