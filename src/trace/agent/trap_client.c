/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <trace/gen_rgy.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/console_api.h>
#include <trace/agent/trap_client.h>

/*
 * osi tracing framework
 * trace console library
 * agent probe control rpc interface 
 * low-level client routines
 */

osi_extern struct rx_securityClass * osi_trace_agent_rpc_C_sc_vec[1];
osi_extern struct rx_securityClass * osi_trace_agent_rpc_S_sc_vec[1];
osi_extern struct rx_service * osi_trace_agent_rpc_service;

osi_extern osi_Trace_rpc_agent_handle osi_trace_agent_uuid;


/*
 * send a previously encoded trace data trap to
 * a vector of consoles
 *
 * [IN] conns             -- vector of rx_connections
 * [OUT] results          -- per-conn result vector
 * [IN] nconns            -- number of conns in vec
 * [IN] encoding          -- opaque pointer to encoded payload
 * [IN] encoding_size     -- size of encoded payload
 * [IN] encoding_type     -- type of encoding used
 * [IN] encoding_version  -- encoding type versioning
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if one or more of the calls failed
 */
osi_result
osi_trace_agent_trap_send_raw(struct rx_connection ** conns,
			      osi_uint32 * results,
			      osi_uint32 nconns,
			      void * encoding,
			      osi_size_t encoding_size,
			      osi_trace_trap_encoding_t encoding_type,
			      osi_uint32 encoding_version)
{
    osi_result res = OSI_OK;
    osi_Trace_rpc_trap_data payload;

    if (nconns) {
	payload.osi_Trace_rpc_trap_data_len = encoding_size;
	payload.osi_Trace_rpc_trap_data_val = (char *) encoding;
	osi_mem_zero(results, nconns * sizeof(osi_uint32));

	multi_Rx(conns, nconns) {
	    multi_Trace_Console_Trap(&osi_trace_agent_uuid,
				     encoding_type, 
				     encoding_version,
				     0,
				     0,
				     &payload);
	    if (multi_error) {
		results[multi_i] = multi_error;
		res = OSI_FAIL;
	    }
	}
	multi_End;
    }

    return res;
}

/*
 * send a previously encoded vector of trace data traps to
 * a vector of consoles
 *
 * [IN] conns             -- vector of rx_connections
 * [OUT] results          -- per-conn result vector
 * [IN] nconns            -- number of conns in vec
 * [IN] encoding          -- opaque pointer to encoded payload
 * [IN] encoding_size     -- size of encoded payload
 * [IN] encoding_type     -- type of encoding used
 * [IN] encoding_version  -- encoding type versioning
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if one or more of the calls failed
 */
osi_result
osi_trace_agent_trap_send_vec_raw(struct rx_connection ** conns,
				  osi_uint32 * results,
				  osi_uint32 nconns,
				  void ** encoding,
				  osi_size_t * encoding_size,
				  osi_trace_trap_encoding_t encoding_type,
				  osi_uint32 encoding_version,
				  osi_uint32 nencodings)
{
    osi_result res = OSI_OK;
    osi_Trace_rpc_bulk_trap payload;
    osi_Trace_rpc_trap_data * vec;
    osi_uint32 i;

    if (nconns) {
	payload.osi_Trace_rpc_bulk_trap_len = nencodings;
	payload.osi_Trace_rpc_bulk_trap_val = vec = 
	    osi_mem_alloc(nencodings * sizeof(osi_Trace_rpc_trap_data));
	if (osi_compiler_expect_false(vec == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}

	for (i = 0; i < nencodings; i++) {
	    vec[i].osi_Trace_rpc_trap_data_len = encoding_size[i];
	    vec[i].osi_Trace_rpc_trap_data_val = (char *) encoding[i];
	}

	osi_mem_zero(results, nconns * sizeof(osi_uint32));

	multi_Rx(conns, nconns) {
	    multi_Trace_Console_BulkTrap(&osi_trace_agent_uuid,
					 encoding_type, 
					 encoding_version,
					 0,
					 0,
					 payload);
	    if (multi_error) {
		results[multi_i] = multi_error;
		res = OSI_FAIL;
	    }
	}
	multi_End;
    }

 error:
    return res;
}


osi_result
osi_trace_agent_trap_client_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_agent_trap_client_PkgShutdown(void)
{
    return OSI_OK;
}

