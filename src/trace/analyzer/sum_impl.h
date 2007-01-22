/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_SUM_IMPL_H
#define _OSI_TRACE_ANALYZER_SUM_IMPL_H 1

/*
 * osi tracing framework
 * data analysis library
 * sum component
 */

#include <osi/osi_counter.h>

typedef struct osi_trace_anly_sum_data {
    osi_counter_t sum;
} osi_trace_anly_sum_data_t;

#endif /* _OSI_TRACE_ANALYZER_SUM_IMPL_H */
