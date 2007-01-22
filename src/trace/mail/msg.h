/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_MAIL_MSG_H
#define _OSI_TRACE_MAIL_MSG_H 1


/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * common message payload definitions
 *
 */

#include <trace/directory.h>

/* this gets stored in the envelope header env_proto_type field */
typedef enum {
    OSI_TRACE_MAIL_MSG_NULL,                         /* null message ; no content */
    OSI_TRACE_MAIL_MSG_RET_CODE,                     /* return code only message */
    OSI_TRACE_MAIL_MSG_GEN_UP,                       /* generator up notification */
    OSI_TRACE_MAIL_MSG_GEN_DOWN,                     /* generator down notification */
    OSI_TRACE_MAIL_MSG_PROBE_REGISTER,               /* probe registration notification */
    OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_REQ,           /* probe activate request */
    OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_RES,           /* probe activate response */
    OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_REQ,         /* probe deactivate request */
    OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_RES,         /* probe deactivate response */
    OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_ID,            /* probe activate by id request */
    OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_ID,          /* probe deactivate by id request */
    OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_REQ,            /* directory id to name resolve request */
    OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_RES,            /* directory id to name resolve response */
    OSI_TRACE_MAIL_MSG_DIRECTORY_N2I_REQ,            /* directory name to id resolve request */
    OSI_TRACE_MAIL_MSG_DIRECTORY_N2I_RES,            /* directory name to id resolve response */
    OSI_TRACE_MAIL_MSG_PING,                         /* ping packet */
    OSI_TRACE_MAIL_MSG_PONG,                         /* pong packet */
    OSI_TRACE_MAIL_MSG_MODULE_INFO_REQ,              /* trace module count request */
    OSI_TRACE_MAIL_MSG_MODULE_INFO_RES,              /* trace module count response */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_REQ,            /* trace module info request */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_RES,            /* trace module info response */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_REQ,    /* trace module info request */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_RES,    /* trace module info response */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_REQ,  /* trace module info request */
    OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_RES,  /* trace module info response */
    OSI_TRACE_MAIL_MSG_BOZO_STARTUP,                 /* bosserver starting */
    OSI_TRACE_MAIL_MSG_BOZO_SHUTDOWN,                /* bosserver shutting down */
    OSI_TRACE_MAIL_MSG_BOZO_PROC_START,              /* bosserver starting process */
    OSI_TRACE_MAIL_MSG_BOZO_PROC_STOP,               /* bosserver stopping process */
    OSI_TRACE_MAIL_MSG_BOZO_PROC_CRASH,              /* bosserver detected process crash */
    OSI_TRACE_MAIL_MSG_ID_MAX
} osi_trace_mail_msg_t;

/* return code message */
typedef struct {
    osi_int32 code;
    osi_uint32 spare;
} osi_trace_mail_msg_result_code_t;

/* generator up notification message */
typedef struct {
    osi_uint32 gen_id;
    osi_uint16 gen_type;
    osi_uint8 gen_state;
    osi_uint8 flags;
    osi_uint32 gen_pid;
    osi_uint32 spares[2];
} osi_trace_mail_msg_gen_up_t;

/* generator down notification message */
typedef struct {
    osi_uint32 gen_id;
    osi_uint16 gen_type;
    osi_uint8 gen_state;
    osi_uint8 flags;
    osi_uint32 gen_pid;
    osi_uint32 spares[2];
} osi_trace_mail_msg_gen_down_t;

/* probe registration notification message */
typedef struct {
    osi_uint32 probe_id;
    char probe_name[OSI_TRACE_MAX_PROBE_NAME_LEN];
} osi_trace_mail_msg_probe_register_t;

/* probe activate control message */
typedef struct {
    osi_uint32 spare;
    char probe_filter[OSI_TRACE_MAX_PROBE_NAME_LEN];
} osi_trace_mail_msg_probe_activate_request_t;
typedef struct {
    osi_int32 code;
    osi_uint32 nhits;
    osi_uint32 spares[2];
} osi_trace_mail_msg_probe_activate_response_t;

/* probe deactivate control message */
typedef struct {
    osi_uint32 spare;
    char probe_filter[OSI_TRACE_MAX_PROBE_NAME_LEN];
} osi_trace_mail_msg_probe_deactivate_request_t;
typedef struct {
    osi_int32 code;
    osi_uint32 nhits;
    osi_uint32 spares[2];
} osi_trace_mail_msg_probe_deactivate_response_t;

/* probe activate by id control message */
typedef struct {
    osi_uint32 probe_id;
    osi_uint32 spare;
} osi_trace_mail_msg_probe_activate_id_request_t;

/* probe deactivate by id control message */
typedef struct {
    osi_uint32 probe_id;
    osi_uint32 spare;
} osi_trace_mail_msg_probe_deactivate_id_request_t;

/* N2I directory resolve message */
typedef struct {
    osi_uint32 spare;
    char probe_name[OSI_TRACE_MAX_PROBE_NAME_LEN];
} osi_trace_mail_msg_probe_n2i_request_t;
typedef struct {
    osi_uint32 probe_id;
    osi_uint32 spare;
} osi_trace_mail_msg_probe_n2i_response_t;

/* I2N directory resolve message */
typedef struct {
    osi_uint32 probe_id;
    osi_uint32 spare;
} osi_trace_mail_msg_probe_i2n_request_t;
typedef struct {
    osi_int32 code;
    osi_uint32 probe_id;
    osi_uint8 probe_stability;
    osi_uint8 arg_count;
    osi_uint16 arg_override;
    osi_uint32 probe_ttl;
    osi_uint8 arg_stability[12];
    osi_uint32 arg_ttl[12];
    char probe_name[OSI_TRACE_MAX_PROBE_NAME_LEN];
} osi_trace_mail_msg_probe_i2n_response_t;

/* ping/pong messages */
typedef struct {
    osi_uint32 timestamp;
    osi_uint32 spare;
} osi_trace_mail_msg_ping_t;
typedef struct {
    osi_uint32 timestamp;
    osi_uint32 spare;
} osi_trace_mail_msg_pong_t;

/* trace module summary info message */
typedef struct {
    osi_uint32 spares[2];
} osi_trace_mail_msg_module_info_req_t;
typedef struct {
    osi_uint32 code;
    osi_uint32 programType;
    osi_int32  epoch;
    osi_uint32 probe_count;
    osi_uint32 module_count;
    osi_uint32 module_version_cksum;
    osi_uint8  module_version_cksum_type;
    osi_uint8  spare1;
    osi_uint16 spare2;
    osi_uint32 spares[5];
} osi_trace_mail_msg_module_info_res_t;

/* trace module lookup by id message */
typedef struct {
    osi_uint32 module_id;
    osi_uint32 spare;
} osi_trace_mail_msg_module_lookup_req_t;
typedef struct {
    osi_uint32 code;
    osi_uint32 module_id;
    osi_uint32 module_version;
    osi_uint32 spare;
    char module_name[OSI_TRACE_MODULE_NAME_LEN_MAX];
    char module_prefix[OSI_TRACE_MODULE_PREFIX_LEN_MAX];
} osi_trace_mail_msg_module_lookup_res_t;

/* trace module lookup by name message */
typedef struct {
    osi_uint32 spare;
    char module_name[OSI_TRACE_MODULE_NAME_LEN_MAX];
} osi_trace_mail_msg_module_lookup_by_name_req_t;
typedef struct {
    osi_uint32 code;
    osi_uint32 module_id;
    osi_uint32 module_version;
    osi_uint32 spare;
    char module_prefix[OSI_TRACE_MODULE_PREFIX_LEN_MAX];
} osi_trace_mail_msg_module_lookup_by_name_res_t;

/* trace module lookup by prefix message */
typedef struct {
    osi_uint32 spare;
    char module_prefix[OSI_TRACE_MODULE_PREFIX_LEN_MAX];
} osi_trace_mail_msg_module_lookup_by_prefix_req_t;
typedef struct {
    osi_uint32 code;
    osi_uint32 module_id;
    osi_uint32 module_version;
    osi_uint32 spare;
    char module_name[OSI_TRACE_MODULE_NAME_LEN_MAX];
} osi_trace_mail_msg_module_lookup_by_prefix_res_t;

/* bosserver notification messages */
typedef struct {
    osi_uint32 pid;
    osi_uint32 spare;
} osi_trace_mail_msg_bozo_startup_t;
typedef struct {
    osi_uint32 pid;
    osi_uint32 spare;
} osi_trace_mail_msg_bozo_shutdown_t;
typedef struct {
    osi_uint32 pid;
    osi_uint32 spare;
} osi_trace_mail_msg_bozo_proc_start_t;
typedef struct {
    osi_uint32 pid;
    osi_uint32 spare;
} osi_trace_mail_msg_bozo_proc_stop_t;
typedef struct {
    osi_uint32 pid;
    osi_uint32 signal;
    osi_uint32 reason;
    osi_uint32 spare;
} osi_trace_mail_msg_bozo_proc_crash_t;

#endif /* _OSI_TRACE_MAIL_MSG_H */
