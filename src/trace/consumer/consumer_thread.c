/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_thread.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/consumer/activation.h>
#include <trace/consumer/cursor.h>
#include <trace/consumer/consumer.h>

/*
 * osi tracing framework
 * trace consumer thread
 */


typedef struct {
    osi_cpu_id_t cpuid;
    void * arg;
} osi_trace_consumer_thread_arg_t;


osi_static void *
osi_trace_consumer_thread(void * arg_in)
{
    osi_trace_consumer_thread_arg_t * arg;

    arg = (osi_trace_consumer_thread_arg_t *) arg_in;

#if defined(OSI_IMPLEMENTS_THREAD_BIND)
    osi_thread_bind_current(arg->cpuid);
#endif

    osi_trace_consumer_loop();

    return NULL;
}

osi_static osi_result
osi_trace_consumer_cpu_init(osi_cpu_id_t cpuid, void * sdata)
{
    osi_result res = OSI_OK;
    osi_thread_p t;
    osi_trace_consumer_thread_arg_t * arg;
    osi_thread_options_t opt;

    osi_thread_options_Init(&opt);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_DETACHED, 1);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_TRACE_ALLOWED, 0);

    arg = (osi_trace_consumer_thread_arg_t *)
	osi_mem_alloc(sizeof(osi_trace_consumer_thread_arg_t));
    if (osi_compiler_expect_false(arg == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    arg->arg = sdata;
    arg->cpuid = cpuid;

    res = osi_thread_createU(&t,
			     &osi_trace_consumer_thread,
			     arg,
			     &opt);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_mem_free(arg, sizeof(osi_trace_consumer_thread_arg_t));
    }

 error:
    osi_thread_options_Destroy(&opt);
    return res;
}


osi_result
osi_trace_consumer_start(void)
{
    osi_result res = OSI_OK;

    res = osi_cpu_list_iterate(&osi_trace_consumer_cpu_init, osi_NULL);

    return res;
}
