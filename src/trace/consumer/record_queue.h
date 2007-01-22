/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_RECORD_QUEUE_H
#define _OSI_TRACE_CONSUMER_RECORD_QUEUE_H 1

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record cache
 */

osi_extern osi_result 
osi_trace_consumer_record_queue_enqueue_new(osi_TracePoint_record *);

osi_extern osi_result osi_trace_consumer_record_queue_PkgInit(void);
osi_extern osi_result osi_trace_consumer_record_queue_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_RECORD_QUEUE_H */
