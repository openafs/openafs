/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * common support
 * initialization/shutdown
 */

#include <trace/common/trace_impl.h>
#include <trace/analyzer/init.h>
#include <trace/analyzer/var.h>
#include <trace/analyzer/counter.h>
#include <trace/analyzer/sum.h>
#include <trace/analyzer/logic.h>

osi_result
osi_trace_anly_PkgInit(void)
{
    osi_result res;

    res = osi_trace_anly_var_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_anly_PkgInit(): var initialization failed\n");
	goto error;
    }

    res = osi_trace_anly_counter_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_anly_PkgInit(): counter initialization failed\n");
	goto error;
    }

    res = osi_trace_anly_sum_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_anly_PkgInit(): sum initialization failed\n");
	goto error;
    }

    res = osi_trace_anly_logic_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_anly_PkgInit(): logic initialization failed\n");
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_anly_PkgShutdown(void)
{
    osi_result res, code = OSI_OK;

    res = osi_trace_anly_logic_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	code = res;
    }

    res = osi_trace_anly_sum_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	code = res;
    }

    res = osi_trace_anly_counter_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	code = res;
    }

    if (OSI_RESULT_OK(code)) {
	code = osi_trace_anly_var_PkgShutdown();
    }

 error:
    return code;
}
