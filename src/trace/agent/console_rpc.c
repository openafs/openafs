/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/consumer/gen_rgy.h>
#include <trace/consumer/config.h>
#include <trace/cursor.h>
#include <trace/syscall.h>
#include <trace/agent/console.h>
#include <trace/agent/console_impl.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>

/*
 * osi tracing framework
 * trace agent library
 * console registration RPC stubs
 */

osi_static osi_result bulkaddrs4_to_addrvec(osi_Trace_rpc_bulkaddrs4 *,
					    osi_trace_console_addr_t **);
osi_static osi_result addr4_to_addr(osi_Trace_rpc_addr4 *,
				    osi_trace_console_addr_t *);


/*
 * register a console and its multihome address vector
 *
 * [IN] tcall            -- rx_call state
 * [IN] console_handle   -- console uuid
 * [IN] console_options  -- options vector
 * [IN] naddrs           -- number of mh addrs
 * [IN] spare1           -- reserved for future use
 * [IN] console_addrs    -- mh address vector
 */
afs_int32
SAgent_ConsoleRegisterIP4(struct rx_call * tcall,
			  osi_Trace_rpc_console_handle * console_handle,
			  osi_Trace_rpc_console_options * console_options,
			  afs_uint32 naddrs,
			  afs_uint32 spare1,
			  osi_Trace_rpc_bulkaddrs4 * console_addrs)
{
    osi_result code = OSI_OK;
    osi_trace_console_addr_t * addr_vec = osi_NULL;
    size_t addr_vec_len;

    addr_vec_len = (size_t) console_addrs->osi_Trace_rpc_bulkaddrs4_len;
    code = bulkaddrs4_to_addrvec(console_addrs, &addr_vec);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_console_rgy_register(console_handle, 
					  addr_vec, addr_vec_len);

    osi_trace_console_rgy_console_addr_free(addr_vec);

 error:
    return OSI_RESULT_RPC_CODE(code);
}

/*
 * unregister a console from this agent
 *
 * [IN] tcall           -- rx_call state
 * [IN] console_handle  -- uuid of console
 */
afs_int32
SAgent_ConsoleUnregister(struct rx_call * tcall,
			 osi_Trace_rpc_console_handle * console_handle)
{
    osi_result code;

    code = osi_trace_console_rgy_unregister(console_handle);

    return OSI_RESULT_RPC_CODE(code);
}

/*
 * re-enable sending traps to a console
 *
 * [IN] tcall           -- rx_call state
 * [IN] console_handle  -- uuid of console
 */
afs_int32
SAgent_ConsoleEnable(struct rx_call * tcall,
		     osi_Trace_rpc_console_handle * console_handle)
{
    osi_result code;

    code = osi_trace_console_rgy_enable(console_handle);

    return OSI_RESULT_RPC_CODE(code);
}

/*
 * temporarily disable sending traps to a console
 *
 * [IN] tcall           -- rx_call state
 * [IN] console_handle  -- uuid of console
 */
afs_int32
SAgent_ConsoleDisable(struct rx_call * tcall,
		      osi_Trace_rpc_console_handle * console_handle)
{
    osi_result code;

    code = osi_trace_console_rgy_disable(console_handle);

    return OSI_RESULT_RPC_CODE(code);
}

/*
 * keep-alive packet to keep agent from 
 * automatically deallocating a dead console
 *
 * [IN] tcall           -- rx_call state
 * [IN] console_handle  -- uuid of console
 */
afs_int32
SAgent_ConsoleKeepAlive(struct rx_call * tcall,
			osi_Trace_rpc_console_handle * console_handle)
{
    return RXGEN_OPCODE;
}

/*
 * convert a wire representation ipv4 address vector to the 
 * internal address vector representation
 *
 * [IN] wirevec     -- pointer to wire representation addr vector
 * [INOUT] vec_out  -- address in which to store pointer to newly allocated vec
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on invalid wirevec
 */
osi_static osi_result
bulkaddrs4_to_addrvec(osi_Trace_rpc_bulkaddrs4 * wirevec,
		      osi_trace_console_addr_t ** vec_out)
{
    osi_result res;
    osi_trace_console_addr_t * vec = osi_NULL;
    osi_uint32 i;

    res = osi_trace_console_rgy_console_addr_alloc(&vec,
						   wirevec->osi_Trace_rpc_bulkaddrs4_len);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    for (i = 0; i < wirevec->osi_Trace_rpc_bulkaddrs4_len; i++) {
	res = addr4_to_addr(&wirevec->osi_Trace_rpc_bulkaddrs4_val[i],
			    &vec[i]);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
    }

 done:
    return res;

 error:
    if (vec) {
	osi_trace_console_rgy_console_addr_free(vec);
    }
    goto done;
}

/*
 * convert a wire format ipv4 address element to the internal representation
 *
 * [IN] in   -- wire format address
 * [IN] out  -- internal format address
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid transport id
 */
osi_static osi_result addr4_to_addr(osi_Trace_rpc_addr4 * in,
				    osi_trace_console_addr_t * out)
{
    osi_result res = OSI_OK;

    switch(in->transport) {
    case OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_UDP:
	out->transport = OSI_TRACE_CONSOLE_TRANSPORT_RX_UDP;
	break;
    case OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_TCP:
	out->transport = OSI_TRACE_CONSOLE_TRANSPORT_RX_TCP;
	break;
    case OSI_TRACE_RPC_CONSOLE_TRANSPORT_SNMP_V1:
	out->transport = OSI_TRACE_CONSOLE_TRANSPORT_SNMP_V1;
	break;
    default:
	res = OSI_FAIL;
	goto error;
    }

    out->addr.addr4.addr = in->addr;
    out->addr.addr4.port = in->port;
    out->addr.addr4.flags = 0;

 error:
    return res;
}
