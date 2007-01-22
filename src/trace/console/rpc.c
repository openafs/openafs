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
#include <trace/rpc/console_api.h>
#include <trace/console/trap_impl.h>

/*
 * osi tracing framework
 * trace agent library
 * general rpc stubs
 */

afs_int32
STrace_Console_GetVersion(struct rx_call * tcall, 
			  afs_uint32 * ver, 
			  afs_uint32 * build_date)
{
    *ver = 1;
    *build_date = 0;
    return OSI_RESULT_RPC_CODE(OSI_OK);
}

afs_int32
STrace_Console_TimeStamp(struct rx_call * tcall,
			 osi_Trace_rpc_agent_handle * agent_handle,
			 afs_uint32 time_sync)
{
    return RXGEN_OPCODE;
}

afs_int32
STrace_Console_Trap(struct rx_call * tcall,
		    osi_Trace_rpc_agent_handle * agent_handle,
		    afs_uint32 trap_record_format,
		    afs_uint32 trap_record_version,
		    afs_uint32 reserved0,
		    afs_uint32 reserved1,
		    osi_Trace_rpc_trap_data * trap_payload)
{
    osi_result res;
    osi_trace_console_trap_t trap;

    trap.trap_encoding = (osi_trace_trap_encoding_t) trap_record_format;
    trap.trap_version = trap_record_version;
    trap.trap_payload = *trap_payload;

    res = osi_trace_console_trap_handle(&trap);

    return OSI_RESULT_RPC_CODE(res);
}

afs_int32
STrace_Console_BulkTrap(struct rx_call * tcall,
			osi_Trace_rpc_agent_handle * agent_handle,
			afs_uint32 trap_record_format,
			afs_uint32 trap_record_version,
			afs_uint32 reserved0,
			afs_uint32 reserved1,
			osi_Trace_rpc_bulk_trap trap_payloads)
{
    osi_result res, code = OSI_OK;
    osi_uint32 rec;
    osi_trace_console_trap_t trap;

    trap.trap_encoding = (osi_trace_trap_encoding_t) trap_record_format;
    trap.trap_version = trap_record_version;
    for (rec = 0; 
	 rec < trap_payloads.osi_Trace_rpc_bulk_trap_len;
	 rec++) {
	trap.trap_payload = trap_payloads.osi_Trace_rpc_bulk_trap_val[rec];

	res = osi_trace_console_trap_handle(&trap);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    return OSI_RESULT_RPC_CODE(code);
}
