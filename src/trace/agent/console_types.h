/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_CONSOLE_TYPES_H
#define _OSI_TRACE_AGENT_CONSOLE_TYPES_H 1

#include <osi/osi_list.h>
#include <trace/gen_rgy.h>
#include <trace/agent/trap.h>
#include <rx/rx.h>

/*
 * osi tracing framework
 * trace agent library
 * console registry
 */

#define OSI_TRACE_CONSOLE_MH_MAX 32

typedef afsUUID osi_trace_console_handle_t;

typedef enum {
    OSI_TRACE_CONSOLE_TRANSPORT_RX_UDP,
    OSI_TRACE_CONSOLE_TRANSPORT_RX_TCP,
    OSI_TRACE_CONSOLE_TRANSPORT_SNMP_V1,
    OSI_TRACE_CONSOLE_TRANSPORT_MAX_ID
} osi_trace_console_transport_t;

typedef struct {
    osi_uint32 osi_volatile addr;
    osi_uint16 osi_volatile port;
    osi_uint16 osi_volatile flags;
} osi_trace_console_addr4_t;

typedef union {
    osi_trace_console_addr4_t addr4;
} osi_trace_console_addr_u_t;

typedef struct {
    osi_uint32 cache_idx;
    osi_trace_console_transport_t osi_volatile transport;
    osi_trace_console_addr_u_t addr;
    struct rx_connection * osi_volatile conn;
} osi_trace_console_addr_t;

typedef struct {
    osi_list_element_volatile console_list;
    osi_trace_console_handle_t osi_volatile console_handle;
    osi_trace_console_addr_t * osi_volatile console_addr_vec;
    osi_uint32 osi_volatile flags;
} osi_trace_console_t;

/* console control flags */
#define OSI_TRACE_CONSOLE_FLAG_TRAP_ENA       0x1

#endif /* _OSI_TRACE_AGENT_CONSOLE_TYPES_H */
