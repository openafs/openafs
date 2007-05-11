/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_RECORD_QUEUE_IMPL_H
#define _OSI_TRACE_CONSUMER_RECORD_QUEUE_IMPL_H 1

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record cache
 * private implementation
 */

#include <trace/consumer/record_types.h>
#include <trace/consumer/record_queue_types.h>

osi_extern osi_trace_consumer_record_queue_t * osi_trace_consumer_incoming_record_queue;


osi_extern osi_result
osi_trace_consumer_record_queue_create(osi_trace_consumer_record_queue_t **);
osi_extern osi_result
osi_trace_consumer_record_queue_destroy(osi_trace_consumer_record_queue_t *);

osi_extern osi_result
osi_trace_consumer_record_queue_free_record(osi_TracePoint_record_queue_t *);

osi_extern osi_result
osi_trace_consumer_record_queue_enqueue(osi_trace_consumer_record_queue_t *,
					osi_TracePoint_record_queue_t *);

osi_extern osi_result
osi_trace_consumer_record_queue_dequeue(osi_trace_consumer_record_queue_t *,
					osi_TracePoint_record_queue_t **);

osi_extern osi_result
osi_trace_consumer_record_queue_dequeue_nosleep(osi_trace_consumer_record_queue_t *,
						osi_TracePoint_record_queue_t **);

osi_extern osi_result
osi_trace_consumer_record_queue_waiter_cancel(osi_trace_consumer_record_queue_t *);

/* record queue iteration support */
typedef osi_result osi_trace_consumer_record_queue_worker_t(osi_trace_consumer_record_queue_t *,
							    osi_TracePoint_record_queue_t *,
							    void * sdata);

osi_extern osi_result
osi_trace_consumer_record_queue_foreach(osi_trace_consumer_record_queue_t *,
					osi_trace_consumer_record_queue_worker_t *,
					void * sdata);

osi_extern osi_result
osi_trace_consumer_record_queue_bulk_trans(osi_trace_consumer_record_queue_t *,
					   osi_trace_consumer_record_queue_worker_t *,
					   void * sdata);

#endif /* _OSI_TRACE_CONSUMER_RECORD_QUEUE_IMPL_H */
