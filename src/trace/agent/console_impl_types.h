/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_CONSOLE_IMPL_TYPES_H
#define _OSI_TRACE_AGENT_CONSOLE_IMPL_TYPES_H 1

#include <osi/osi_list.h>
#include <osi/osi_event.h>
#include <trace/gen_rgy.h>
#include <trace/agent/trap.h>
#include <rx/rx.h>

/*
 * osi tracing framework
 * trace agent library
 * console registry
 * implementation-private type definitions
 */


typedef struct {
    /*
     * BEGIN sync block
     * the following fields are synchronized by osi_trace_console_registry.lock
     */
    osi_list_element_volatile console_list;
    osi_list_head_volatile probe_ena_list;
    osi_trace_console_handle_t osi_volatile console_handle;
    osi_trace_console_addr_t * osi_volatile console_addr_vec;
    osi_uint32 osi_volatile flags;
    /*
     * END sync block
     */
} osi_trace_console_t;

typedef enum {
    OSI_TRACE_CONSOLE_RGY_PROBE_STATE_INVALID,
    OSI_TRACE_CONSOLE_RGY_PROBE_STATE_ENABLING,
    OSI_TRACE_CONSOLE_RGY_PROBE_STATE_ENABLED,
    OSI_TRACE_CONSOLE_RGY_PROBE_STATE_DISABLING,
} osi_trace_console_probe_ena_state_t;

typedef struct {
    osi_list_element_volatile ena_list;
    osi_trace_console_probe_ena_state_t osi_volatile state;
    osi_trace_gen_id_t osi_volatile gen_id;

    osi_event_subscription_t gen_sub;

    osi_uint8 cache_idx;
    char osi_volatile filter[1]; /* length determined by cache index */
} osi_trace_console_probe_ena_cache_t;

/* console control flags */
#define OSI_TRACE_CONSOLE_FLAG_TRAP_ENA       0x1

#endif /* _OSI_TRACE_AGENT_CONSOLE_IMPL_TYPES_H */
