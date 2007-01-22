/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_RECORD_FSA_H
#define _OSI_TRACE_CONSUMER_RECORD_FSA_H 1

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record queueing mechanism
 * record finite state automata
 */

#include <trace/consumer/record_types.h>

typedef osi_result osi_trace_consumer_record_fsa_dequeue_func_t(osi_TracePoint_record_queue_t *);
typedef osi_result osi_trace_consumer_record_fsa_enqueue_func_t(osi_TracePoint_record_queue_t *);
typedef osi_result osi_trace_consumer_record_fsa_free_func_t(osi_TracePoint_record_queue_t *);

typedef struct {
    osi_trace_consumer_record_fsa_dequeue_func_t * dequeue_fp;
    osi_trace_consumer_record_fsa_enqueue_func_t * enqueue_fp;
    osi_trace_consumer_record_fsa_free_func_t * free_fp;
} osi_trace_consumer_record_fsa_registration_t;

osi_extern osi_result
osi_trace_consumer_record_fsa_transition(osi_TracePoint_record_queue_t *);

osi_extern osi_result
osi_trace_consumer_record_fsa_register(osi_trace_consumer_record_fsa_state_type_t,
				       osi_trace_consumer_record_fsa_dequeue_func_t *,
				       osi_trace_consumer_record_fsa_enqueue_func_t *,
				       osi_trace_consumer_record_fsa_free_func_t *);

osi_extern osi_result
osi_trace_consumer_record_fsa_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_record_fsa_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_RECORD_FSA_H */
