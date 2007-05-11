/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_ACTIVATION_IMPL_H
#define _OSI_TRACE_GENERATOR_ACTIVATION_IMPL_H 1


/*
 * osi tracing framework
 * trace generator library
 * tracepoint activation control implementation
 */

#include <trace/activation.h>
#include <osi/osi_mem_local.h>
#include <osi/osi_mutex.h>


/*
 * length of the bit vector that controls
 * trace point activation
 */
#define OSI_TRACE_ACTIVATION_VEC_LEN 256

/*
 * log2 of number of bits needed to control one probe
 * (we reserve the right to use more bits in the future)
 */
#define OSI_TRACE_ACTIVATION_VEC_LOG2_BITS_PER_PROBE 0

/*
 * max supported probes in one process space
 */
#define OSI_TRACE_PROBES_MAX (OSI_TRACE_ACTIVATION_VEC_LEN * (8 >> OSI_TRACE_ACTIVATION_VEC_LOG2_BITS_PER_PROBE))


/*
 * tracepoint activation control structure
 * every tracepoint has an activation bit in the vector
 */
struct osi_TracePoint_config {
#ifdef OSI_TRACE_BUFFER_CTX_LOCAL
    osi_mem_local_key_t _tpbuf_key;
#endif
    osi_volatile osi_fast_uint * _tp_activation_vec;
    osi_size_t _tp_activation_vec_len;
    osi_mutex_t lock;
};
osi_extern struct osi_TracePoint_config _osi_tracepoint_config;

/* activation vector arithmetic */

/* this macro computes a mask of probe id lsb bits which
 * address offsets  within a single activation array element */
#define osi_TracePoint_Mask \
    ((sizeof(osi_fast_uint)<<(OSI_LOG2_BITS_PER_BYTE>>OSI_TRACE_ACTIVATION_VEC_LOG2_BITS_PER_PROBE))-1)
#define osi_TracePoint_Shift \
    (OSI_TYPE_FAST_LOG2_BITS - OSI_TRACE_ACTIVATION_VEC_LOG2_BITS_PER_PROBE)


/*
 * convert a probe id into an offset 
 * and bitnum in the activation vector
 */
#define osi_Trace_Probe2Index(osi_probe_id, offset, bitnum) \
        ((bitnum) = ((offset) = (osi_probe_id)) & osi_TracePoint_Mask, \
         (offset) >>= osi_TracePoint_Shift)



/*
 * support atomic enable/disable ops where available
 */
#include <osi/osi_atomic.h>

#if defined(OSI_IMPLEMENTS_ATOMIC_OR_FAST) && defined(OSI_IMPLEMENTS_ATOMIC_AND_FAST)
#define OSI_TRACE_ACTIVATION_ATOMIC_OPS 1
#endif


#endif /* _OSI_TRACE_GENERATOR_ACTIVATION_IMPL_H */
