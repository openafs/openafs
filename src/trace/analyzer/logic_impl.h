/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_LOGIC_IMPL_H
#define _OSI_TRACE_ANALYZER_LOGIC_IMPL_H 1

/*
 * osi tracing framework
 * data analysis library
 * logic component
 */

#include <trace/analyzer/logic_types.h>

typedef struct osi_trace_anly_logic_data {
    osi_volatile osi_trace_anly_logic_op_t logic_op;
} osi_trace_anly_logic_data_t;

#endif /* _OSI_TRACE_ANALYZER_LOGIC_IMPL_H */
