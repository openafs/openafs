/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/common/init.h>
#include <trace/consumer/config.h>
#include <trace/consumer/i2n.h>
#include <trace/consumer/module.h>
#include <trace/consumer/record_queue.h>
#include <trace/consumer/record_fsa.h>

/*
 * osi tracing framework
 * trace consumer
 * initialization/shutdown
 */

osi_result
osi_trace_consumer_PkgInit(void)
{
    osi_result res;

    res = osi_trace_common_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace common initialization failed\n", __osi_func__);
	goto error;
    }

    res = osi_trace_consumer_kernel_config_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace kernel config initialization failed\n", __osi_func__);
	goto error;
    }

    res = osi_trace_consumer_record_fsa_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace record finite state automata initialization failed\n", __osi_func__);
	goto error;
    }

    res = osi_trace_consumer_record_queue_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace record queue initialization failed\n", __osi_func__);
	goto error;
    }

    res = osi_trace_consumer_i2n_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace i2n initialization failed\n", __osi_func__);
	goto error;
    }

    res = osi_trace_gen_rgy_msg_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: trace gen_rgy mail handler initialization failed\n", __osi_func__);
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_PkgShutdown(void)
{
    osi_result res;

    res = osi_trace_gen_rgy_msg_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_consumer_i2n_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_consumer_record_queue_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_consumer_record_fsa_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_consumer_kernel_config_PkgShutdown();
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


osi_result
osi_Trace_PkgInit(void)
{
    return osi_trace_consumer_PkgInit();
}

osi_result
osi_Trace_PkgShutdown(void)
{
    return osi_trace_consumer_PkgShutdown();
}

/*
 * stubs for subsystems which must be initialized
 * via common init, but don't exist in the consumer
 */
osi_result
osi_trace_activation_PkgInit(void)
{
    return OSI_OK;
}
osi_result
osi_trace_activation_PkgShutdown(void)
{
    return OSI_OK;
}
