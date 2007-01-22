/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <osi/osi_mem.h>
#include <trace/gen_rgy.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>
#include <trace/console/proc_client.h>

/*
 * osi tracing framework
 * trace console library
 * agent instrumented process query rpc interface 
 * low-level client routines
 */

osi_extern struct rx_securityClass * osi_trace_console_rpc_C_sc_vec[1];
osi_extern struct rx_securityClass * osi_trace_console_rpc_S_sc_vec[1];
osi_extern struct rx_service * osi_trace_console_rpc_service;

osi_extern osi_Trace_rpc_console_handle osi_trace_console_uuid;

osi_result
osi_trace_console_proc_agent(osi_Trace_rpc_addr4 * addr,
			     osi_Trace_rpc_proc_handle * proc_id)
{
    osi_result res;
    afs_int32 code;
    struct rx_connection * conn;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    code = Agent_ProcSelf(conn,
			  proc_id);
    res = (osi_result) code;

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

osi_result
osi_trace_console_proc_info(osi_Trace_rpc_addr4 * addr,
			    osi_Trace_rpc_proc_handle proc_id,
			    osi_Trace_rpc_proc_info * info)
{
    osi_result res;
    afs_int32 code;
    struct rx_connection * conn;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    code = Agent_ProcInfo(conn,
			  proc_id,
			  info);
    res = (osi_result) code;

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

osi_result
osi_trace_console_proc_foreach(osi_Trace_rpc_addr4 * addr,
			       osi_trace_console_proc_iterator_t * fp,
			       void * sdata)
{
    osi_result res = OSI_OK;
    afs_int32 code;
    afs_uint32 nentries;
    osi_Trace_rpc_proc_handle idx;
    osi_Trace_rpc_proc_info_vec info_vec;
    osi_Trace_rpc_proc_info * info;
    struct rx_connection * conn;
    osi_uint32 i;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    for (idx = 0; OSI_RESULT_OK(res); ) {
	nentries = OSI_TRACE_RPC_PROC_VEC_MAX;
	code = Agent_ProcList(conn,
			      idx,
			      &nentries,
			      &info_vec);
	res = (osi_result) code;
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    break;
	}

	if (!nentries) {
	    break;
	}

	info = (osi_Trace_rpc_proc_info *) 
	    info_vec.osi_Trace_rpc_proc_info_vec_val;
	for (i = 0; i < info_vec.osi_Trace_rpc_proc_info_vec_len; i++) {
	    res = (*fp)(addr, &info[i], sdata);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }
	}	    
    }

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

osi_result
osi_trace_console_proc_client_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_console_proc_client_PkgShutdown(void)
{
    return OSI_OK;
}

