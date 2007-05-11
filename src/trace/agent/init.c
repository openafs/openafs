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
 * trace agent library
 * initialization/shutdown
 */

#include <trace/common/trace_impl.h>
#include <trace/agent/init.h>
#include <trace/agent/console.h>
#include <trace/agent/trap.h>
#include <trace/rpc/agent_api.h>
#include <rx/rx.h>
#include <afs/afsutil.h>

osi_extern int Agent_ExecuteRequest(register struct rx_call *);

osi_uint32 SHostAddrs[ADDRSPERSITE];

struct rx_securityClass * osi_trace_agent_rpc_C_sc_vec[1];
struct rx_securityClass * osi_trace_agent_rpc_S_sc_vec[1];
struct rx_service * osi_trace_agent_rpc_service;

osi_Trace_rpc_agent_handle osi_trace_agent_uuid;

osi_static osi_result
osi_trace_agent_server_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_trace_agent_rpc_S_sc_vec[0] = rxnull_NewServerSecurityObject();
    if (osi_compiler_expect_false(osi_trace_agent_rpc_S_sc_vec[0] == osi_NULL)) {
	(osi_Msg "%s: rxnull_NewServerSecurityObject failed\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    osi_trace_agent_rpc_service =
	rx_NewService(0, OSI_TRACE_RPC_SERVICE_AGENT, "trace agent", 
		      osi_trace_agent_rpc_S_sc_vec, 1, &Agent_ExecuteRequest);
    if (osi_compiler_expect_false(osi_trace_agent_rpc_service == osi_NULL)) {
	(osi_Msg "%s: rx_NewService failed\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    rx_SetMinProcs(osi_trace_agent_rpc_service, 3);
    rx_SetMaxProcs(osi_trace_agent_rpc_service, 12);
    rx_StartServer(0);

 error:
    return res;
}

osi_static osi_result
osi_trace_agent_uuid_init(void)
{
    osi_result res = OSI_OK;
    afs_int32 code;

    code = afs_uuid_create(&osi_trace_agent_uuid);
    if (code) {
	(osi_Msg "%s: WARNING: uuid generation failed\n", __osi_func__);
	res = OSI_FAIL;
    }

    return res;
}

osi_result
osi_trace_agent_PkgInit(void)
{
    osi_result res;
    osi_options_val_t listen, port, rxbind, rxbind_host;

    res = osi_trace_console_rgy_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_agent_PkgInit(): console rgy initialization failed\n");
	goto error;
    }

    res = osi_config_options_Get(OSI_OPTION_RX_PORT,
				 &port);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get RX_PORT option value\n");
	goto error;
    }
    if (port.val.v_uint16 == 0) {
	goto done;
    }

    res = osi_config_options_Get(OSI_OPTION_RX_BIND,
				 &rxbind);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get RX_BIND option value\n");
	goto error;
    }

    if (rxbind.val.v_bool == OSI_TRUE) {
	res = osi_config_options_Get(OSI_OPTION_RX_BIND_HOST,
				     &rxbind_host);
	if (OSI_RESULT_FAIL(res)) {
	    (osi_Msg "failed to get RX_BIND_HOST option value\n");
	    goto error;
	}

	if (rxbind_host.val.v_uint32 == 0) {
	    afs_int32 ccode;
#ifndef AFS_NT40_ENV
	    if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || 
		AFSDIR_SERVER_NETINFO_FILEPATH) {
		char reason[1024];
		ccode = parseNetFiles(SHostAddrs, NULL, NULL,
				      ADDRSPERSITE, reason,
				      AFSDIR_SERVER_NETINFO_FILEPATH,
				      AFSDIR_SERVER_NETRESTRICT_FILEPATH);
	    } else 
#endif	
		{
		    ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
		}
	    if (ccode == 1) {
		rxbind_host.val.v_uint32 = SHostAddrs[0];
	    }
	}

	if (rx_InitHost(rxbind_host.val.v_uint32,
			htons(port.val.v_uint16))) {
	    (osi_Msg "%s: rx_InitHost failed\n", __osi_func__);
	    res = OSI_FAIL;
	    goto error;
	}
    } else if (rx_Init(port.val.v_uint16) < 0) {
	    (osi_Msg "%s: rx_Init failed\n");
	    res = OSI_FAIL;
	    goto error;
    }

    res = osi_trace_agent_uuid_init();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_config_options_Get(OSI_OPTION_TRACED_LISTEN,
				 &listen);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get TRACED_PORT option value\n");
	goto error;
    }

    if (listen.val.v_bool == OSI_TRUE) {
	res = osi_trace_agent_server_PkgInit();
    }

 done:
 error:
    return res;
}

osi_result
osi_trace_agent_PkgShutdown(void)
{
    osi_result res, code = OSI_OK;

    res = osi_trace_console_rgy_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	code = res;
    }

 error:
    return code;
}
