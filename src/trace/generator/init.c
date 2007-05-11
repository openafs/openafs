/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <trace/common/init.h>
#include <trace/generator/generator.h>
#include <trace/generator/init.h>
#include <trace/generator/module.h>
#include <trace/generator/module_mail.h>
#include <trace/generator/probe.h>

/*
 * osi tracing framework
 */

OSI_INIT_FUNC_DECL(osi_trace_generator_PkgInit)
{
    osi_result res;

    res = osi_trace_common_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_generator_PkgInit(): common initialization failed\n");
	goto error;
    }

    res = osi_trace_buffer_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_generator_PkgInit(): buffer initialization failed\n");
	goto error;
    }

    res = osi_trace_module_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_generator_PkgInit(): module initialization failed\n");
	goto error;
    }

    res = osi_trace_probe_rgy_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_generator_PkgInit(): probe rgy initialization failed\n");
	goto error;
    }

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_generator_PkgShutdown)
{
    osi_result res;

    res = osi_trace_probe_rgy_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_module_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_buffer_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_common_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

 error:
    return res;
}

OSI_INIT_FUNC_DECL(osi_Trace_PkgInit)
{
    return osi_trace_generator_PkgInit();
}
OSI_FINI_FUNC_DECL(osi_Trace_PkgShutdown)
{
    return osi_trace_generator_PkgShutdown();
}
