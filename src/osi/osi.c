/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <osi/osi_impl.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_trace.h>
#include <osi/osi_object_init.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_syscall.h>
#include <osi/osi_time.h>

osi_config_t osi_config;

/*
 * osi library initialization/shutdown routines
 */
osi_result
osi_PkgInit(osi_ProgramType_t type, osi_options_t * opts)
{
    osi_result res = OSI_OK;

    osi_config.programType = type;

    if (opts) {
	res = osi_options_Copy(&osi_config.options, opts);
    } else {
	res = osi_options_Init(type, &osi_config.options);
    }

    if (OSI_RESULT_FAIL(res) ||
	OSI_RESULT_FAIL(osi_time_get(&osi_config.epoch)) ||
	OSI_RESULT_FAIL(osi_common_options_PkgInit()) ||
	OSI_RESULT_FAIL(osi_object_init_PkgInit()) ||
	OSI_RESULT_FAIL(osi_mem_object_cache_PkgInit()) ||
	OSI_RESULT_FAIL(osi_syscall_PkgInit()) ||
	OSI_RESULT_FAIL(osi_cpu_PkgInit()) ||
	OSI_RESULT_FAIL(osi_cache_PkgInit()) ||
	OSI_RESULT_FAIL(osi_time_approx_PkgInit()) ||
	OSI_RESULT_FAIL(osi_thread_PkgInit()) ||
	OSI_RESULT_FAIL(osi_mem_PkgInit()) ||
	OSI_RESULT_FAIL(osi_Trace_PkgInit())) {
	res = OSI_FAIL;
	goto error;
    }

#if defined(OSI_TRACE_ENABLED)
    res = osi_TracePointTableInit();
#endif

 error:
    return res;
}

osi_result
osi_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    if (OSI_RESULT_FAIL(osi_Trace_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_mem_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_thread_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_time_approx_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_cache_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_cpu_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_syscall_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_mem_object_cache_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_object_init_PkgShutdown()) ||
	OSI_RESULT_FAIL(osi_common_options_PkgShutdown())) {
	res = OSI_FAIL;
    }

    return res;
}

osi_result osi_PkgInitChild(osi_ProgramType_t type)
{
    osi_config.programType = type;
    /* XXX eventually need to do a lot more here */
    return OSI_OK;
}




osi_result osi_nullfunc()
{
    return OSI_OK;
}

