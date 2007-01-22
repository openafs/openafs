/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_RECORD_QUEUE_TYPES_H
#define _OSI_TRACE_CONSUMER_RECORD_QUEUE_TYPES_H 1

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record cache
 * private implementation
 */

#include <osi/osi_list.h>
#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>

typedef struct {
    osi_list_head_volatile record_list;
    osi_uint32 osi_volatile record_list_len;
    osi_condvar_t cv;
    osi_mutex_t lock;
} osi_trace_consumer_record_queue_t;

#endif /* _OSI_TRACE_CONSUMER_RECORD_QUEUE_TYPES_H */
