/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_BUFFER_H
#define _OSI_TRACE_GENERATOR_BUFFER_H 1


/*
 * osi tracing framework
 * circular trace buffer implementation
 */

#include <osi/osi_mem_local.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>

OSI_INIT_FUNC_PROTOTYPE(osi_trace_buffer_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_buffer_PkgShutdown);


struct osi_TraceBuffer_config {
    osi_size_t buffer_len;
    osi_mutex_t lock;
};
osi_extern struct osi_TraceBuffer_config osi_TraceBuffer_config;

#define OSI_TRACE_BUFFER_LEN_DEFAULT  128 /* must be a power of 2 */

/* 
 * trace framework ring buffer
 *
 * for kernel case this buffer is cpu-specific
 * for userspace case this buffer is thread-specific
 *
 * for cache coherency purposes this buffer is designed such that
 * each trace buffer element is aligned on an integer multiple of the
 * l1d cache line size
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL

typedef struct osi_TraceBuffer {
    /* ring buffer parameters */
    osi_register_uint last_idx;         /* monotonic index counter */
    osi_register_uint blocks;           /* number of event data buffers */
    osi_register_uint blocks_mask;      /* mask for valid buffer indices */
    osi_uint32 len;                     /* total size of ring buffer */
    osi_TracePoint_record * buf;        /* pointer to ring buffer */

    osi_mutex_t lock;                   /* only needed because of cv */
    osi_condvar_t cv;                   /* used to block consumer procs */
} osi_TraceBuffer;

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

typedef struct osi_TraceBuffer {
    /* ring buffer parameters */
    volatile osi_register_uint last_idx;         /* monotonic index counter */
    volatile osi_register_uint last_valid;       /* last valid entry */
    osi_register_uint blocks;                    /* number of event data buffers */
    osi_register_uint blocks_mask;               /* mask for valid buffer indices */
    osi_uint32 len;                              /* total size of ring buffer */
    osi_TracePoint_record * buf;                 /* pointer to ring buffer */

    osi_mutex_t lock;                            /* used to synchronize trace table access */
    osi_condvar_t cv;                            /* used to block consumer procs */
} osi_TraceBuffer;

osi_extern osi_TraceBuffer * _osi_tracebuf;
#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */


/* handle to a trace event that's being recorded */
typedef struct osi_Trace_EventHandle {
    osi_register_uint idx;
    osi_TracePoint_record_ptr_t data;
    osi_TraceBuffer * tracebuf;
} osi_Trace_EventHandle;

#define osi_Trace_EventHandle_Record(event) \
    ((event)->data.ptr.cur)
#define osi_Trace_EventHandle_RecordPtr(event) \
    (&((event)->data))
#define osi_Trace_EventHandle_Field_Set(event, field, val) \
    (event)->data.ptr.cur->field = val
#define osi_Trace_EventHandle_Field_Get(event, field, val) \
    val = (event)->data.ptr.cur->field


/*
 * initialize a trace ring buffer, 
 * and fill in the event handle before returning
 */
osi_extern osi_result osi_TraceBuffer_Init(osi_Trace_EventHandle *);


/*
 * deallocate a trace ring buffer
 */
osi_extern osi_result osi_TraceBuffer_Destroy(osi_TraceBuffer *);



/*
 * circular buffer new index acquisition
 *
 *  pull the tracebuffer control structure out of cpu-local or
 *  thread-local storage, and set the event pointer to the next
 *  available event structure
 *
 * void osi_TraceBuffer_Allocate(osi_Trace_EventHandle *)
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL

#define osi_TraceBuffer_Allocate(event) \
    osi_Macro_Begin \
        (event)->tracebuf = (osi_TraceBuffer *) osi_mem_local_get(_osi_tracepoint_config._tpbuf_key); \
        if (osi_compiler_expect_false((event)->tracebuf == osi_NULL)) { \
            osi_TraceBuffer_Init(event); \
        } \
        (event)->idx = (event)->tracebuf->last_idx; \
        (event)->data.ptr.cur = &((event)->tracebuf->buf[(event)->idx & (event)->tracebuf->blocks_mask]); \
        (event)->data.ptr.cur->nargs = 0; \
    osi_Macro_End

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

#define osi_TraceBuffer_Allocate(event) \
    osi_Macro_Begin \
        (event)->tracebuf = _osi_tracebuf; \
        osi_mutex_Lock(&(event)->tracebuf->lock); \
        (event)->idx = (event)->tracebuf->last_idx++; \
        (event)->data.ptr.cur = &((event)->tracebuf->buf[(event)->idx & (event)->tracebuf->blocks_mask]); \
        osi_mutex_Unlock(&(event)->tracebuf->lock); \
        (event)->data.ptr.cur->nargs = 0; \
    osi_Macro_End

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */


/*
 * circular buffer trace event commit
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL

#define osi_TraceBuffer_Commit(event) \
    osi_Macro_Begin \
        (event)->tracebuf->last_idx++; \
        osi_condvar_Broadcast(&(event)->tracebuf->cv); \
        osi_mem_local_put((event)->tracebuf); \
    osi_Macro_End

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

#define osi_TraceBuffer_Commit(event) \
    osi_Macro_Begin \
        osi_mutex_Lock(&(event)->tracebuf->lock); \
        if ((event)->idx > _osi_tracebuf->last_valid) { \
            _osi_tracebuf->last_valid = (event)->idx; \
            osi_condvar_Broadcast(&(event)->tracebuf->cv); \
        } \
        osi_mutex_Unlock(&(event)->tracebuf->lock); \
    osi_Macro_End

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */


/*
 * circular buffer trace event rollback
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL

#define osi_TraceBuffer_Rollback(event) \
    osi_Macro_Begin \
        osi_mem_local_put((event)->tracebuf); \
    osi_Macro_End

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

#define osi_TraceBuffer_Rollback(event) \
    osi_Macro_Begin \
        osi_mutex_Lock(&(event)->tracebuf->lock); \
        (event)->data.ptr.cur->probe = 0; \
        (event)->data.ptr.cur->tags[0] = (osi_uint8) osi_Trace_Event_AbortedEvent_Id; \
        if ((event)->idx > _osi_tracebuf->last_valid) { \
            _osi_tracebuf->last_valid = (event)->idx; \
            osi_condvar_Broadcast(&(event)->tracebuf->cv); \
        } \
        osi_mutex_Unlock(&(event)->tracebuf->lock); \
    osi_Macro_End

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */

/*
 * get/set the number of records in the local trace buffer
 */
osi_extern osi_result osi_TraceBuffer_size_set(osi_size_t);
osi_extern osi_result osi_TraceBuffer_size_get(osi_size_t *);

#endif /* _OSI_TRACE_GENERATOR_BUFFER_H */
