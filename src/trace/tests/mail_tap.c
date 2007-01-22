/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/syscall.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

osi_static void
dump_message(osi_trace_mail_message_t * msg);
osi_static char *
get_proto_type(osi_trace_mail_message_t * msg);

osi_static void
osi_bringup(void)
{
    osi_options_t opts;
    osi_options_val_t val;
    osi_ProgramType_t ptype = osi_ProgramType_TestSuite;

    osi_Assert(OSI_RESULT_OK(osi_options_Copy(&opts, &osi_options_default_daemon)));

    val.type = OSI_OPTION_VAL_TYPE_BOOL;
    val.val.v_bool = OSI_FALSE;

    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_START_MAIL_THREAD,
					     &val)));

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(ptype, &opts)));

    osi_options_Destroy(&opts);
}			     
    
int
main(int argc, char ** argv)
{
    int i, code, tap_id, rval, ret = 0;
    osi_result res;
    osi_trace_gen_id_t gen_id;
    osi_trace_mail_message_t * msg;

    osi_bringup();

    tap_id = atoi(argv[1]);
    res = osi_trace_gen_id(&gen_id);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: failed to get a gen id\n", argv[0]);
	ret = 1;
	goto error;
    }
    (osi_Msg "%s: our gen id is %d\n", argv[0], gen_id);

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_TAP,
			    tap_id,
			    gen_id,
			    0, 
			    &rval);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: failed to tap gen id %d\n", argv[0], tap_id);
	ret = 1;
	goto error;
    }

    while (1) {
	res = osi_trace_mail_msg_alloc(OSI_TRACE_MAIL_BODY_LEN_MAX, &msg);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    (osi_Msg "%s: failed to allocate a message object\n", argv[0]);
	    ret = 1;
	    goto error;
	}

	res = osi_trace_mail_check(gen_id, msg, 0);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    (osi_Msg "%s: msg check failed with errno %d\n", argv[0], errno);
	    ret = 1;
	    goto error;
	}

	dump_message(msg);
	osi_trace_mail_msg_put(msg);
    }

 error:
    return ret;
}

osi_static char *
get_proto_type(osi_trace_mail_message_t * msg)
{
    osi_static char buf[128];

    switch(msg->envelope.env_proto_type) {
    case OSI_TRACE_MAIL_MSG_NULL:
	return "NULL";
    case OSI_TRACE_MAIL_MSG_RET_CODE:
	return "RET_CODE";
    case OSI_TRACE_MAIL_MSG_GEN_UP:
	return "GEN_UP";
    case OSI_TRACE_MAIL_MSG_GEN_DOWN:
	return "GEN_DOWN";
    case OSI_TRACE_MAIL_MSG_PROBE_REGISTER:
	return "PROBE_REGISTER";
    case OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_REQ:
	return "PROBE_ACTIVATE_REQ";
    case OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_RES:
	return "PROBE_ACTIVATE_RES";
    case OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_REQ:
	return "PROBE_DEACTIVATE_REQ";
    case OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_RES:
	return "PROBE_DEACTIVATE_RES";
    case OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_ID:
	return "PROBE_ACTIVATE_ID";
    case OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_ID:
	return "PROBE_DEACTIVATE_ID";
    case OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_REQ:
	return "DIRECTORY_I2N_REQ";
    case OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_RES:
	return "DIRECTORY_I2N_RES";
    case OSI_TRACE_MAIL_MSG_DIRECTORY_N2I_REQ:
	return "DIRECTORY_I2N_REQ";
    case OSI_TRACE_MAIL_MSG_DIRECTORY_N2I_RES:
	return "DIRECTORY_I2N_RES";
    case OSI_TRACE_MAIL_MSG_PING:
	return "PING";
    case OSI_TRACE_MAIL_MSG_PONG:
	return "PONG"; 
    case OSI_TRACE_MAIL_MSG_MODULE_INFO_REQ:
	return "MODULE_INFO_REQ";
    case OSI_TRACE_MAIL_MSG_MODULE_INFO_RES:
	return "MODULE_INFO_RES";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_REQ:
	return "MODULE_LOOKUP_REQ";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_RES:
	return "MODULE_LOOKUP_RES";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_REQ:
	return "MODULE_LOOKUP_BY_NAME_REQ";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_RES:
	return "MODULE_LOOKUP_BY_NAME_RES";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_REQ:
	return "MODULE_LOOKUP_BY_PREFIX_REQ";
    case OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_RES:
	return "MODULE_LOOKUP_BY_PREFIX_RES";
    case OSI_TRACE_MAIL_MSG_BOZO_STARTUP:
	return "BOZO_STARTUP";
    case OSI_TRACE_MAIL_MSG_BOZO_SHUTDOWN:
	return "BOZO_SHUTDOWN";
    case OSI_TRACE_MAIL_MSG_BOZO_PROC_START:
	return "BOZO_PROC_START";
    case OSI_TRACE_MAIL_MSG_BOZO_PROC_STOP:
	return "BOZO_PROC_STOP";
    case OSI_TRACE_MAIL_MSG_BOZO_PROC_CRASH:
	return "BOZO_PROC_CRASH";
    default:
	sprintf(buf, "UNKNOWN ID %u", msg->envelope.env_proto_type);
	return buf;
   }
}

osi_static void
dump_message(osi_trace_mail_message_t * msg)
{
    char * type;

    (osi_Msg "*** Version: %u  Hdr Len: %u\n",
     msg->envelope.env_proto_version,
     msg->envelope.env_len);
    (osi_Msg "*** From: %u  To: %u  XId: %lx  RXId: %lx\n",
     msg->envelope.env_src,
     msg->envelope.env_dst,
     msg->envelope.env_xid,
     msg->envelope.env_ref_xid);

    type = get_proto_type(msg);

    (osi_Msg "*** Type: %s\n", type);
}
