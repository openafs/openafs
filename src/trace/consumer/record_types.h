/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_RECORD_TYPES_H
#define _OSI_TRACE_CONSUMER_RECORD_TYPES_H 1

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record queueing mechanism
 * record finite state automata
 */

#include <trace/directory.h>

/*
 * the following enumeration defines the valid FSA states
 * for a tracepoint record
 */
typedef enum {
    OSI_TRACE_CONSUMER_RECORD_STATE_INCOMING,
    OSI_TRACE_CONSUMER_RECORD_STATE_I2N_RESOLVE,
    OSI_TRACE_CONSUMER_RECORD_STATE_PROBE_CACHE,
    OSI_TRACE_CONSUMER_RECORD_STATE_FILTER,
    OSI_TRACE_CONSUMER_RECORD_STATE_ENCODE,
    OSI_TRACE_CONSUMER_RECORD_STATE_SEND,
    OSI_TRACE_CONSUMER_RECORD_STATE_DISCARD,
    OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID
} osi_trace_consumer_record_fsa_state_type_t;

typedef struct {
    void * info[OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID];
} osi_trace_consumer_record_fsa_state_info_t;

typedef struct {
    osi_trace_consumer_record_fsa_state_type_t state;
    osi_trace_consumer_record_fsa_state_type_t next_state;
    osi_trace_consumer_record_fsa_state_info_t info;
} osi_trace_consumer_record_fsa_state_t;

typedef struct {
    osi_list_element_volatile record_list;
    osi_trace_consumer_record_fsa_state_t osi_volatile state;
    /* XXX record versioning */
    osi_TracePoint_record osi_volatile record;
} osi_TracePoint_record_queue_t;


#endif /* _OSI_TRACE_CONSUMER_RECORD_TYPES_H */
