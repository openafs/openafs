/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
#include <osi/osi_thread.h>
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
 * gen_rgy tester
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


#define CHECKING(x) (osi_Msg x " test: ")
#define PASS        (osi_Msg "pass\n")
#define FAIL        \
    osi_Macro_Begin \
        (osi_Msg "fail\n"); \
        goto fail; \
    osi_Macro_End
#define FAIL_CODE(x) \
    osi_Macro_Begin \
        (osi_Msg "error code %d\n", x); \
        goto fail; \
    osi_Macro_End
#define FAIL_MSG(x) \
    osi_Macro_Begin \
        (osi_Msg x "\n"); \
        goto fail; \
    osi_Macro_End

void harness(void);


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

    srand(time(NULL));

    harness();

    return ret;
}

#define TEST_GENS (OSI_TRACE_GEN_RGY_MAX_ID_KERNEL - 1)
#define TEST_GENS_P2 10
#define OVERFLOW_GENS 10000
#define WORKER_THREADS 16
#define THREAD_PASSES 2
#define THREAD_ITERATIONS 1000
osi_static osi_trace_gen_id_t gen_vec[TEST_GENS];

osi_static osi_uint32 osi_volatile threads_done = 0;
osi_static osi_uint32 osi_volatile thread_done_pass = 0;
osi_static osi_mutex_t thread_done_lock;
osi_static osi_condvar_t thread_done_cv;

void *
worker_thread(void *);

void
harness(void)
{
    int code, i;
    osi_trace_generator_address_t addr;
    osi_thread_p tid;
    osi_proc_id_t pid;
    osi_trace_gen_id_t my_gen_id, gen_id, tmp_gen_id;

    pid = osi_proc_current_id();

    CHECKING("sys_get invalid gen");
    code = osi_trace_gen_rgy_sys_get(5);
    if (!code) {
	FAIL;
    }
    PASS;

    CHECKING("sys_put invalid gen");
    code = osi_trace_gen_rgy_sys_put(5);
    if (!code) {
	FAIL;
    }
    PASS;

    CHECKING("sys_lookup_I2A invalid gen");
    code = osi_trace_gen_rgy_sys_lookup_I2A(1, &addr);
    if (!code) {
	FAIL;
    }
    PASS;

    CHECKING("sys_lookup_A2I invalid pid");
    addr.pid = pid;
    code = osi_trace_gen_rgy_sys_lookup_A2I(&addr, &gen_id);
    if (!code) {
	FAIL;
    }
    PASS;

    CHECKING("sys_register my pid");
    addr.programType = osi_ProgramType_TraceCollector;
    addr.pid = pid;
    code = osi_trace_gen_rgy_sys_register(&addr, &my_gen_id);
    if (code) {
	FAIL_CODE(code);
    }

    CHECKING("sys_register");
    addr.programType = osi_ProgramType_Bosserver;
    for (i = 1; i <= TEST_GENS; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_register(&addr, &gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("overflow gen table");
    addr.programType = osi_ProgramType_Utility;
    for (i = 0; i < OVERFLOW_GENS ; i++) {
	addr.pid = pid + TEST_GENS + i;
	code = osi_trace_gen_rgy_sys_register(&addr, &tmp_gen_id);
	if (!code) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("sys_get valid gen");
    for (i = 1; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_get(gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("sys_lookup_I2A valid gen");
    for (i = 1; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_lookup_I2A(gen_vec[i-1],
						&addr);
	if (code) {
	    FAIL_CODE(code);
	} else if ((addr.programType != osi_ProgramType_Bosserver) ||
		   (addr.pid != (pid + i))) {
	    FAIL_MSG("bad addr");
	}
    }
    PASS;

    CHECKING("sys_lookup_A2I valid gen");
    for (i = 1; i <= TEST_GENS; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_lookup_A2I(&addr, 
						&gen_id);
	if (code) {
	    FAIL_CODE(code);
	} else if (gen_id != gen_vec[i-1]) {
	    FAIL_MSG("bad gen_id");
	}
    }
    PASS;

    CHECKING("sys_get_by_addr valid gen");
    for (i = 1; i <= TEST_GENS; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_get_by_addr(&addr, 
						 &gen_id);
	if (code) {
	    FAIL_CODE(code);
	} else if (gen_id != gen_vec[i-1]) {
	    FAIL_MSG("bad gen_id");
	}
    }
    PASS;

    CHECKING("sys_put valid gen");
    for (i = 1; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_put(gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("sys_put before unregister");
    for (i = 1; i <= 5; i++) {
	code = osi_trace_gen_rgy_sys_put(gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("sys_unregister");
    for (i = 1; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_unregister(gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("sys_lookup_I2A unregistered, but referenced, gen");
    for (i = 6; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_lookup_I2A(gen_vec[i-1],
						&addr);
	if (code) {
	    FAIL_CODE(code);
	} else if ((addr.programType != osi_ProgramType_Bosserver) ||
		   (addr.pid != (pid + i))) {
	    FAIL_MSG("bad addr");
	}
    }
    PASS;

    CHECKING("sys_lookup_A2I unregistered, but referenced, gen");
    for (i = 6; i <= TEST_GENS; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_lookup_A2I(&addr, 
						&gen_id);
	if (code) {
	    FAIL_CODE(code);
	} else if (gen_id != gen_vec[i-1]) {
	    FAIL_MSG("bad gen_id");
	}
    }
    PASS;

    CHECKING("sys_lookup_I2A unregistered, unreferenced gen");
    for (i = 1; i <= 5; i++) {
	code = osi_trace_gen_rgy_sys_lookup_I2A(gen_vec[i-1],
						&addr);
	if (!code) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("sys_lookup_A2I unregistered, unreferenced gen");
    for (i = 1; i <= 5; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_lookup_A2I(&addr, 
						&gen_id);
	if (!code) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("sys_put after unregister");
    for (i = 6; i <= TEST_GENS; i++) {
	code = osi_trace_gen_rgy_sys_put(gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    CHECKING("sys_register 2nd pass");
    addr.programType = osi_ProgramType_Bosserver;
    for (i = 1; i <= TEST_GENS_P2; i++) {
	addr.pid = pid + i;
	code = osi_trace_gen_rgy_sys_register(&addr, &gen_vec[i-1]);
	if (code) {
	    FAIL_CODE(code);
	}
    }
    PASS;

    threads_done = 0;
    osi_mutex_Init(&thread_done_lock, &osi_trace_common_options.mutex_opts);
    osi_condvar_Init(&thread_done_cv, &osi_trace_common_options.condvar_opts);

    {
	osi_thread_p tid;
	osi_thread_options_t opts;
	osi_result res;
	
	osi_thread_options_Copy(&opts, &osi_trace_common_options.thread_opts);
	osi_thread_options_Set(&opts, OSI_THREAD_OPTION_DETACHED, 1);
	
	for (i = 0; i < WORKER_THREADS; i++) {
	    res = osi_thread_createU(&tid, &worker_thread, osi_NULL, &opts);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		FAIL_CODE(res);
	    }
	}
    }

    osi_mutex_Lock(&thread_done_lock);
    while (thread_done_pass != THREAD_PASSES) {
	osi_condvar_Wait(&thread_done_cv, &thread_done_lock);
    }
    osi_mutex_Unlock(&thread_done_lock);

    osi_mutex_Destroy(&thread_done_lock);
    osi_condvar_Destroy(&thread_done_cv);

 fail:
    (void)0;
}

#define THREAD_FAIL(x) \
    osi_Macro_Begin \
        (osi_Msg "[%d] %s failed\n", osi_thread_current_id(), x); \
        goto fail; \
    osi_Macro_End
#define THREAD_FAIL_CODE(x, code) \
    osi_Macro_Begin \
        (osi_Msg "[%d] %s failed with code %d\n", osi_thread_current_id(), x, code); \
        goto fail; \
    osi_Macro_End

void *
worker_thread(void * args)
{
    int code, i, j;
    osi_trace_generator_address_t addr;
    osi_trace_gen_id_t my_gen_id, tmp_id;

    addr.programType = osi_ProgramType_TraceCollector;
    addr.pid = osi_thread_current_id();
    for (i = 0; i < THREAD_ITERATIONS; i++) {
	code = osi_trace_gen_rgy_sys_register(&addr, &my_gen_id);
	if (code) {
	    THREAD_FAIL_CODE("sys_register", code);
	}

	for (j = 0; j < TEST_GENS_P2; j++) {
	    code = osi_trace_gen_rgy_sys_get(gen_vec[j]);
	    if (code) {
		THREAD_FAIL_CODE("sys_get", code);
	    }
	}

	for (j = 0; j < TEST_GENS_P2; j++) {
	    code = osi_trace_gen_rgy_sys_get(gen_vec[j]);
	    if (code) {
		THREAD_FAIL_CODE("sys_get", code);
	    }
	}

	for (j = 0; j < TEST_GENS_P2; j++) {
	    code = osi_trace_gen_rgy_sys_put(gen_vec[j]);
	    if (code) {
		THREAD_FAIL_CODE("sys_put", code);
	    }
	}

	code = osi_trace_gen_rgy_sys_unregister(my_gen_id);
	if (code) {
	    THREAD_FAIL_CODE("sys_register", code);
	}

	for (j = 0; j < TEST_GENS_P2; j++) {
	    code = osi_trace_gen_rgy_sys_put(gen_vec[j]);
	    if (code) {
		THREAD_FAIL_CODE("sys_put", code);
	    }
	}
    }

    osi_mutex_Lock(&thread_done_lock);
    threads_done++;
    if (threads_done == WORKER_THREADS) {
	thread_done_pass++;
	threads_done = 0;
	osi_condvar_Broadcast(&thread_done_cv);
    } else {
	int pass_save = thread_done_pass;
	while (pass_save == thread_done_pass) {
	    osi_condvar_Wait(&thread_done_cv, &thread_done_lock);
	}
    }
    osi_mutex_Unlock(&thread_done_lock);

    /* for this pass, we are simply try to produce race conditions.
     * return codes are never checked because it is not possible
     * to know from this level of abstraction which ones should succeed
     */
    addr.programType = osi_ProgramType_TraceCollector;
    addr.pid = osi_thread_current_id();
    for (i = 0 ; i < THREAD_ITERATIONS; i++) {
	osi_trace_gen_id_t holds[4];
	for (j = 0; j < 4; j++) {
	    holds[j] = (rand() >> 8) & OSI_TRACE_GEN_RGY_MAX_ID_KERNEL;
	}
	(void)osi_trace_gen_rgy_sys_get(holds[0]);
	(void)osi_trace_gen_rgy_sys_get(holds[1]);
	(void)osi_trace_gen_rgy_sys_register(&addr, &my_gen_id);
	(void)osi_trace_gen_rgy_sys_put(holds[1]);
	(void)osi_trace_gen_rgy_sys_get(holds[2]);
	(void)osi_trace_gen_rgy_sys_get(holds[3]);
	(void)osi_trace_gen_rgy_sys_put(holds[2]);
	(void)osi_trace_gen_rgy_sys_unregister(my_gen_id);
	(void)osi_trace_gen_rgy_sys_put(holds[3]);
	(void)osi_trace_gen_rgy_sys_put(holds[0]);
    }

    osi_mutex_Lock(&thread_done_lock);
    threads_done++;
    if (threads_done == WORKER_THREADS) {
	thread_done_pass++;
	threads_done = 0;
	osi_condvar_Broadcast(&thread_done_cv);
    }
    osi_mutex_Unlock(&thread_done_lock);

 fail:
    return osi_NULL;
}
