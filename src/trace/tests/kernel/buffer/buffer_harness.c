/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <osi/osi_proc.h>
#include <osi/osi_counter.h>
#include <osi/osi_thread.h>
#include <trace/gen_rgy.h>
#include <trace/cursor.h>
#include <trace/generator/activation.h>
#include <trace/generator/buffer.h>
#include <trace/KERNEL/cursor.h>

/*
 * osi tracing framework
 * trace kernel simulator
 * trace buffer tester
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

osi_static void * read_thread(void *);
osi_static void * send_thread(void *);

osi_static void
usage(const char * progname)
{
    (osi_Msg "usage: %s <trace buffer size n, s.t. n = 2^N>\n",
     (progname) ? progname : "buffer_harness");
}

int
main(int argc, char ** argv)
{
    int i, code, count, ret = 0;
    osi_options_t opts;
    osi_options_val_t val;

    osi_Assert(OSI_RESULT_OK(osi_options_Init(osi_ProgramType_TraceKernel, 
					      &opts)));

    if (argc < 2) {
	usage(argv[0]);
	ret = -1;
	goto error;
    }

    val.type = OSI_OPTION_VAL_TYPE_UINT32;
    val.val.v_uint32 = atoi(argv[1]);

    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_BUFFER_SIZE,
					     &val)));

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceKernel, &opts)));

    osi_Assert(OSI_RESULT_OK(osi_trace_activation_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_trace_buffer_PkgInit()));

    harness();

 error:
    return ret;
}

#define TEST_FIRES 5000000
#define TEST_PROBE 1

#define TEST_SEND_THREADS 1

osi_static osi_counter_t fire_counter;
osi_static osi_counter_t read_counter;
osi_static osi_counter_t miss_counter;

osi_mutex_t reader_lock;
osi_condvar_t reader_cv;
int reader_online = 0;

osi_static void
harness(void)
{
    osi_result res;
    osi_uint32 i;
    osi_thread_p tid;

    res = osi_TracePoint_Activate(TEST_PROBE);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: failed to activate probe %d\n", __osi_func__, TEST_PROBE);
	goto error;
    }

    osi_counter_init(&fire_counter, 0);
    osi_counter_init(&read_counter, 0);
    osi_counter_init(&miss_counter, 0);

    osi_mutex_Init(&reader_lock,
		   osi_trace_impl_mutex_opts());
    osi_condvar_Init(&reader_cv,
		     osi_trace_impl_condvar_opts());

    osi_mutex_Lock(&reader_lock);
    osi_thread_createU(&tid, &read_thread, osi_NULL,
		       osi_trace_impl_thread_opts());
    do {
	osi_condvar_Wait(&reader_cv, &reader_lock);
    } while (!reader_online);
    osi_mutex_Unlock(&reader_lock);

    for (i = 0; i < TEST_SEND_THREADS; i++) {
	osi_thread_createU(&tid, &send_thread, osi_NULL,
			   osi_trace_impl_thread_opts());
    }

    for (i = 0 ; i < 5; i++) {
	osi_counter_val_t sent, read, dropped;
	osi_counter_value(&fire_counter, &sent);
	osi_counter_value(&read_counter, &read);
	osi_counter_value(&miss_counter, &dropped);
	(osi_Msg "%s: sent=%llu, read=%llu, dropped=%llu\n", 
	 __osi_func__, sent, read, dropped);
	sleep(1);
    }

 error:
    (void)0;
}

osi_static void *
send_thread(void * arg)
{
    int i;

    for (i = 0; i < TEST_FIRES; i++) {
	osi_Trace_OSI_Event(TEST_PROBE, osi_Trace_Args1(i));
	osi_counter_inc(&fire_counter);
    }

    return NULL;
}

osi_static void *
read_thread(void * arg)
{
    osi_trace_cursor_t cursor;
    int code;
    long rval;
    osi_TracePoint_record record;

    osi_mutex_Lock(&reader_lock);
    reader_online = 1;
    osi_condvar_Broadcast(&reader_cv);
    osi_mutex_Unlock(&reader_lock);

    cursor.last_idx = 0;
    cursor.context_id = osi_proc_current_id();
    cursor.records_lost = 0;

    while (1) {
	code = osi_trace_cursor_sys_read((long)&cursor,
					 (long)&record,
					 0,
					 &rval);
	if (code) {
	    (osi_Msg "sys_read returned error code %d\n", code);
	    goto error;
	} else if (!rval) {
	    (osi_Msg "sys_read return val was zero!???\n");
	    goto error;
	}
	osi_counter_inc(&read_counter);
	osi_counter_add(&miss_counter, cursor.records_lost);
	/*
	(osi_Msg "%s: payload %u\n", __osi_func__, record.payload[0]);
	*/
    }

 error:
    return NULL;
}
