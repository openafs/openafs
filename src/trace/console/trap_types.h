/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_TRAP_TYPES_H
#define _OSI_TRACE_CONSOLE_TRAP_TYPES_H 1

/*
 * osi tracing framework
 * trace console
 * types for trap support
 */

#include <trace/rpc/console_api.h>
#include <trace/agent/trap.h>
#include <osi/osi_list.h>

typedef struct {
    osi_list_element_volatile trap_list;
    osi_trace_trap_encoding_t trap_encoding;
    osi_uint32 trap_version;
    osi_Trace_rpc_trap_data trap_payload;
} osi_trace_console_trap_t;

typedef osi_result osi_trace_console_trap_handler_t(osi_trace_console_trap_t *,
						    void * sdata);

#endif /* _OSI_TRACE_CONSOLE_TRAP_TYPES_H */
