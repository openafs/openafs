/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CURSOR_H
#define _OSI_TRACE_CURSOR_H 1


/*
 * tracepoint cursor interface
 */


/*
 * ring buffer cursor structure
 */
typedef struct osi_trace_cursor {
    osi_uint64 last_idx;
    osi_int32 context_id;
    osi_int32 records_lost;
} osi_trace_cursor_t;


/*
 * vectorized retrieve handle
 */
typedef struct osi_TracePoint_rvec {
    osi_TracePoint_record ** rvec;
    osi_uint32 nrec;
} osi_TracePoint_rvec_t;

#if !OSI_DATAMODEL_IS(OSI_ILP32_ENV)
typedef struct osi_TracePoint_rvec32 {
    osi_uint32 rvec;
    osi_uint32 nrec;
} osi_TracePoint_rvec32_t;
#endif /* !OSI_ILP32_ENV */

/*
 * max vector len
 */
#define OSI_TRACE_SYSCALL_OP_READV_MAX 128



#define osi_trace_cursor_init(cptr) \
    osi_mem_zero(cptr,sizeof(osi_trace_cursor_t))
#define osi_trace_cursor_destroy(cptr)



#endif /* _OSI_TRACE_CURSOR_H */
