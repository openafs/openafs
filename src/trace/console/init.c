/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * trace console library
 * initialization/shutdown
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/common/options.h>
#include <trace/console/init.h>
#include <trace/console/trap.h>
#include <trace/console/trap_queue.h>
#include <trace/rpc/console_api.h>
#include <rx/rx.h>
#include <afs/afsutil.h>
#include <trace/console/plugin.h>
#include <trace/console/probe_client.h>
#include <trace/console/proc_client.h>
#include <trace/console/console_registration.h>
#include <trace/console/agent_client.h>

osi_extern int Trace_Console_ExecuteRequest(register struct rx_call *);

struct rx_securityClass * osi_trace_console_rpc_C_sc_vec[1];
struct rx_securityClass * osi_trace_console_rpc_S_sc_vec[1];
struct rx_service * osi_trace_console_rpc_service;

osi_Trace_rpc_console_handle osi_trace_console_uuid;

osi_static osi_result
osi_trace_console_server_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_trace_console_rpc_S_sc_vec[0] = rxnull_NewServerSecurityObject();
    if (osi_compiler_expect_false(osi_trace_console_rpc_S_sc_vec[0] == osi_NULL)) {
	(osi_Msg "%s: rxnull_NewServerSecurityObject failed\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    osi_trace_console_rpc_service =
	rx_NewService(0, OSI_TRACE_RPC_SERVICE_CONSOLE, "trace console", 
		      osi_trace_console_rpc_S_sc_vec, 1, &Trace_Console_ExecuteRequest);
    if (osi_compiler_expect_false(osi_trace_console_rpc_service == osi_NULL)) {
	(osi_Msg "%s: rx_NewService failed\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    rx_SetMinProcs(osi_trace_console_rpc_service, 3);
    rx_SetMaxProcs(osi_trace_console_rpc_service, 12);
    rx_StartServer(0);

 error:
    return res;
}

osi_static osi_result
osi_trace_console_server_PkgShutdown(void)
{
    return OSI_OK;
}

osi_static osi_result
osi_trace_console_uuid_init(void)
{
    osi_result res = OSI_OK;
    afs_int32 code;

    code = afs_uuid_create(&osi_trace_console_uuid);
    if (code) {
	(osi_Msg "%s: WARNING: uuid generation failed\n", __osi_func__);
	res = OSI_FAIL;
    }

    return res;
}

osi_result
osi_trace_console_PkgInit(void)
{
    osi_result res;
    osi_options_val_t opt;

    res = osi_config_options_Get(OSI_OPTION_TRACED_PORT,
				 &opt);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get TRACED_PORT option value\n");
	goto error;
    }

    /* XXX rxbind */
    if (rx_Init(opt.val.v_uint16) < 0) {
	(osi_Msg "%s: rx_Init failed\n");
	res = OSI_FAIL;
    }

    osi_trace_console_rpc_C_sc_vec[0] = rxnull_NewClientSecurityObject();
    if (osi_compiler_expect_false(osi_trace_console_rpc_C_sc_vec[0] == osi_NULL)) {
	(osi_Msg "%s: rxnull_NewClientSecurityObject failed\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    res = osi_trace_console_uuid_init();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_registration_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_agent_client_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_proc_client_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_probe_client_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_trap_handler_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_trap_queue_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_server_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* this MUST be last */
    res = osi_trace_console_plugin_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_console_PkgShutdown(void)
{
    osi_result res, code = OSI_OK;

    /* this MUST be first */
    code = osi_trace_console_plugin_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_console_server_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_console_trap_handler_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_console_trap_queue_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    code = osi_trace_console_probe_client_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    code = osi_trace_console_proc_client_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    code = osi_trace_console_agent_client_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    code = osi_trace_console_registration_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 error:
    return code;
}
