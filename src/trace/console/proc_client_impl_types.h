/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_PROC_CLIENT_IMPL_TYPES_H
#define _OSI_TRACE_CONSOLE_PROC_CLIENT_IMPL_TYPES_H 1

/*
 * osi tracing framework
 * trace console
 * instrumented process rpc client
 * implementation-private type definitions
 */

#include <osi/osi_list.h>

typedef struct {
    osi_list_element proc_list;
    osi_Trace_rpc_addr4 agent;
    osi_Trace_rpc_proc_info proc_info;
} osi_trace_console_proc_list_node_t;

#endif /* _OSI_TRACE_CONSOLE_PROC_CLIENT_IMPL_TYPES_H */
