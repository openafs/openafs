/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_LOGIC_TYPES_H
#define _OSI_TRACE_ANALYZER_LOGIC_TYPES_H 1

/*
 * osi tracing framework
 * data analysis library
 * logic component
 */

typedef enum {
    OSI_TRACE_ANLY_LOGIC_OP_NOT,
    OSI_TRACE_ANLY_LOGIC_OP_AND,
    OSI_TRACE_ANLY_LOGIC_OP_OR,
    OSI_TRACE_ANLY_LOGIC_OP_XOR,
    OSI_TRACE_ANLY_LOGIC_OP_NAND,
    OSI_TRACE_ANLY_LOGIC_OP_NOR,
    OSI_TRACE_ANLY_LOGIC_OP_XNOR,
    OSI_TRACE_ANLY_LOGIC_OP_MAX_ID
} osi_trace_anly_logic_op_t;

#endif /* _OSI_TRACE_ANALYZER_LOGIC_TYPES_H */
