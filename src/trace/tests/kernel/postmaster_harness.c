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
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <osi/osi_proc.h>
#include <osi/osi_counter.h>
#include <trace/gen_rgy.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/common/options.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/postmaster.h>
#include <trace/mail/common.h>

/*
 * osi tracing framework
 * trace kernel simulator
 * postmaster tester
 */

#include <stdio.h>
#include <errno.h>

/* override certain libosi symbols as necessary */

#ifdef osi_syscall_suser_check
#undef osi_syscall_suser_check
#endif

void
osi_syscall_suser_check(int * retcode)
{
    *retcode = 0;
}

#ifdef osi_trace_gen_id
#undef osi_trace_gen_id
#endif

osi_result
osi_trace_gen_id(osi_trace_gen_id_t * gen_id)
{
    *gen_id = OSI_TRACE_GEN_RGY_KERNEL_ID;
    return OSI_OK;
}


osi_static void harness(void);

osi_counter_t send_count;
osi_counter_t recv_count;

osi_static void * check_thread(void *);
osi_static void * send_thread(void *);


int
main(int argc, char ** argv)
{
    int i, code, count, ret = 0;
    osi_options_t opts;

    osi_options_Init(osi_ProgramType_TraceKernel, &opts);
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceKernel, &opts)));

    osi_Assert(OSI_RESULT_OK(osi_trace_gen_rgy_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_trace_mail_allocator_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_trace_mail_postmaster_PkgInit()));

    harness();

    return ret;
}

#define TEST_GENS 10

osi_static osi_trace_gen_id_t consumer_gen_id;

osi_static osi_mutex_t checker_lock;
osi_static osi_condvar_t checker_cv;
osi_static int checker_online = 0;

osi_static osi_trace_gen_id_t gen_vec[TEST_GENS];

struct check_thread_args {
    int index;
};

struct send_thread_args {
    int index;
};

osi_static struct check_thread_args check_thread_args[TEST_GENS];
osi_static struct send_thread_args send_thread_args[TEST_GENS];

osi_static void
harness(void)
{
    int code, i;
    osi_trace_generator_address_t addr;
    osi_thread_p tid;

    osi_counter_init(&send_count, 0);
    osi_counter_init(&recv_count, 0);

    addr.programType = osi_ProgramType_TraceCollector;
    addr.pid = osi_proc_current_id();

    code = osi_trace_gen_rgy_sys_register(&addr, &consumer_gen_id);
    if (code) {
	(osi_Msg "%s: sys_register of trace collector failed with code %d", __osi_func__, code);
	goto error;
    }

    osi_mutex_Init(&checker_lock, &osi_trace_common_options.mutex_opts);
    osi_condvar_Init(&checker_cv, &osi_trace_common_options.condvar_opts);
    osi_mutex_Lock(&checker_lock);
    osi_thread_createU(&tid, &check_thread, osi_NULL, &osi_trace_common_options.thread_opts);
    while (!checker_online) {
	(osi_Msg "%s: waiting for checker thread to come online...\n", __osi_func__);
	osi_condvar_Wait(&checker_cv, &checker_lock);
    }
    osi_mutex_Unlock(&checker_lock);

    addr.programType = osi_ProgramType_Bosserver;

    for (i = 1; i <= TEST_GENS; i++) {
	addr.pid = osi_proc_current_id() + i;
	code = osi_trace_gen_rgy_sys_register(&addr, &gen_vec[i-1]);
	(osi_Msg "%s: sys_register returned res=%d gen_id=%d\n", __osi_func__, code, gen_vec[i-1]);
    }

    for (i = 1; i <= TEST_GENS; i++) {
	send_thread_args[i-1].index = i-1;
	osi_thread_createU(&tid, &send_thread, &send_thread_args[i-1], 
			   &osi_trace_common_options.thread_opts);
    }

    for (i = 0; i < 5; i++) {
	osi_counter_val_t sent, received;
	osi_counter_value(&send_count, &sent);
	osi_counter_value(&recv_count, &received);
	(osi_Msg "%s: sent=%llu, received=%llu\n", __osi_func__, sent, received);
	sleep(1);
    }

    for (i = 1; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_unregister(gen_vec[i-1]);
	(osi_Msg "%s: sys_unregister returned res=%d\n", __osi_func__, code);
    }

 error:
    (void)0;
}

osi_static void *
check_thread(void * arg)
{
    osi_result res;
    int code;
    osi_trace_mail_message_t * msg;

    osi_mutex_Lock(&checker_lock);
    checker_online = 1;
    osi_condvar_Broadcast(&checker_cv);
    osi_mutex_Unlock(&checker_lock);

    while (1) {
	res = osi_trace_mail_msg_alloc(OSI_TRACE_MAIL_BODY_LEN_MAX, &msg);
	if (OSI_RESULT_FAIL(res)) {
	    (osi_Msg "%s: msg_alloc failed with code %d\n", __osi_func__, res);
	    goto error;
	}

	code = osi_trace_mail_sys_check(1,
					msg,
					0);
	if (code) {
	    (osi_Msg "%s: mail_sys_check returned error code %d\n", __osi_func__, code);
	    goto error;
	} else {
	    osi_counter_inc(&recv_count);
	}
	(void)osi_trace_mail_msg_put(msg);
    }

 error:
    (osi_Msg "%s: mail thread shutting down on error\n", __osi_func__);
    return NULL;
}

#define TEST_MSGS 1000

osi_static void *
send_thread(void * arg)
{
    osi_result res;
    int code, i;
    osi_trace_mail_message_t * msg;

    for (i = 0 ; i < TEST_MSGS ; i++) {
	res = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_ping_t), &msg);
	if (OSI_RESULT_FAIL(res)) {
	    (osi_Msg "%s: msg_alloc failed with code %d\n", __osi_func__, res);
	    goto error;
	}

	msg->envelope.env_dst = consumer_gen_id;
	msg->envelope.env_src = OSI_TRACE_GEN_RGY_KERNEL_ID;
	msg->envelope.env_proto_type = OSI_TRACE_MAIL_MSG_PING;
	msg->envelope.env_proto_version = 1;
	msg->envelope.env_len = sizeof(msg->envelope);
	msg->envelope.env_flags = 0;
	msg->envelope.env_xid = i + 1;
	msg->envelope.env_ref_xid = 0;

	code = osi_trace_mail_sys_send(msg);
	if (code) {
	    (osi_Msg "%s: sys_send failed with error code %d\n", __osi_func__, code);
	} else {
	    osi_counter_inc(&send_count);
	}

	(void) osi_trace_mail_msg_put(msg);
    }

 error:
    (osi_Msg "%s: send thread done\n", __osi_func__);
    return NULL;
}
