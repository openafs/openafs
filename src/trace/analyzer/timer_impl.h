/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_TIMER_IMPL_H
#define _OSI_TRACE_ANALYZER_TIMER_IMPL_H 1

/*
 * osi tracing framework
 * data analysis library
 * timer component
 */

#include <osi/osi_time.h>

typedef enum {
    OSI_TRACE_ANLY_TIMER_FAN_IN_TIMEOUT = 0,
    OSI_TRACE_ANLY_TIMER_FAN_IN_RESET = 1,
    OSI_TRACE_ANLY_TIMER_FAN_IN_FIRE = 2,
} osi_trace_anly_timer_fan_in_t;

typedef struct osi_trace_anly_timer_data {
    osi_time32_t timestamp;
    osi_bool_t active;
    osi_trace_anly_var_t * var; /* back pointer to our parent var object */
} osi_trace_anly_timer_data_t;

#endif /* _OSI_TRACE_ANALYZER_TIMER_IMPL_H */
