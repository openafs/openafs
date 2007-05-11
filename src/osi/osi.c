/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
#include <osi/osi_mem_local.h>
#include <osi/osi_trace.h>
#include <osi/osi_object_init.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_syscall.h>
#include <osi/osi_time.h>

osi_config_t osi_config;

struct osi_state osi_state = 
    {
	OSI_STATE_UNINITIALIZED
    };

osi_init_func_t * osi_init_vec[] =
    {
	&osi_mem_PkgInit,
	&osi_common_options_PkgInit,
	&osi_object_init_PkgInit,
	&osi_mem_object_cache_PkgInit,
	&osi_syscall_PkgInit,
	&osi_cpu_PkgInit,
	&osi_cache_PkgInit,
	&osi_time_approx_PkgInit,
	&osi_thread_PkgInit,
	&osi_mem_local_PkgInit,
	&osi_Trace_PkgInit,
	osi_NULL
     };
osi_fini_func_t * osi_fini_vec[] =
    {
	&osi_Trace_PkgShutdown,
	&osi_mem_local_PkgShutdown,
	&osi_thread_PkgShutdown,
	&osi_time_approx_PkgShutdown,
	&osi_cache_PkgShutdown,
	&osi_cpu_PkgShutdown,
	&osi_syscall_PkgShutdown,
	&osi_mem_object_cache_PkgShutdown,
	&osi_object_init_PkgShutdown,
	&osi_common_options_PkgShutdown,
	&osi_mem_PkgShutdown,
	osi_NULL
    };

/*
 * osi library initialization/shutdown routines
 */
osi_result
osi_PkgInit(osi_ProgramType_t type, osi_options_t * opts)
{
    osi_result res;
    osi_init_func_t * fp;
    osi_uint32 i;

    osi_config.programType = type;
    osi_state.state = OSI_STATE_INITIALIZING;

    if (opts) {
	res = osi_options_Copy(&osi_config.options, opts);
    } else {
	res = osi_options_Init(type, &osi_config.options);
    }
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    /* set epoch */
    res = osi_time_get32(&osi_config.epoch);
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    /*
     * now do the primary initialization loop
     */
    for (i = 0, fp = osi_init_vec[i]; 
	 fp != osi_NULL; 
	 i++, fp = osi_init_vec[i]) {
	res = (*fp)();
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
    }

    /* 
     * now bring up the osi section of the instrumentation namespace
     */
#if defined(OSI_TRACE_ENABLED)
    res = osi_TracePointTableInit();
#endif

 error:
    if (OSI_RESULT_OK_LIKELY(res)) {
	osi_state.state = OSI_STATE_ONLINE;
    } else {
	osi_state.state = OSI_STATE_ERROR;
    }

    return res;
}

osi_result
osi_PkgShutdown(void)
{
    osi_result res;
    osi_fini_func_t * fp;
    osi_uint32 i;

    osi_state.state = OSI_STATE_SHUTTING_DOWN;

    /*
     * do the primary finalization loop
     */
    for (i = 0, fp = osi_fini_vec[i]; 
	 fp != osi_NULL; 
	 i++, fp = osi_fini_vec[i]) {
	res = (*fp)();
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    break;
	}
    }

    if (OSI_RESULT_OK_LIKELY(res)) {
	osi_state.state = OSI_STATE_SHUTDOWN;
    } else {
	osi_state.state = OSI_STATE_ERROR;
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


OSI_INIT_FUNC_DECL(osi_null_init_func)
{
    return OSI_OK;
}

OSI_FINI_FUNC_DECL(osi_null_fini_func)
{
    return OSI_OK;
}
