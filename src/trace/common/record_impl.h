/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_RECORD_IMPL_H
#define _OSI_TRACE_COMMON_RECORD_IMPL_H 1

/*
 * tracepoint record
 * IMPLEMENTATION-PRIVATE interfaces
 */

#include <trace/common/record_types.h>


/* compute arg offset into trap record payload array */
#if (OSI_TYPE_REGISTER_BITS == 32)
#define OSI_TRACEPOINT_RECORD_ARG(arg) (arg << 1)
#else /* OSI_TYPE_REGISTER_BITS != 32 */
#define OSI_TRACEPOINT_RECORD_ARG(arg) (arg)
#endif /* OSI_TYPE_REGISTER_BITS != 32 */


/*
 * INTERNAL
 * 64-bit value getter/setter macros
 *
 * __osi_TracePoint_record_arg_set64(record, arg, value)
 *   [IN] record  -- pointer to any type of record
 *   [IN] arg     -- arg value from OSI_TRACEPOINT_RECORD_ARG()
 *   [IN] value   -- osi_int64 value to store into record payload
 *
 * __osi_TracePoint_record_arg_get64(record, arg, value)
 *   [IN] record  -- pointer to any type of record
 *   [IN] arg     -- arg value from OSI_TRACEPOINT_RECORD_ARG()
 *   [OUT] value  -- pointer to osi_int64 in which to store value from record payload
 */
#if (OSI_TYPE_REGISTER_BITS == 32)
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
#if defined(OSI_ENV_BIG_ENDIAN)

#define __osi_TracePoint_record_arg_set64(record, arg, value) \
    osi_Macro_Begin \
        (record)->payload[arg] = ((value) >> 32); \
        (record)->payload[(arg)+1] = (value) & 0xffffffff; \
    osi_Macro_End
#define __osi_TracePoint_record_arg_get64(record, arg, value) \
    *(value) = (((osi_int64)((record)->payload[arg])) << 32) | \
	(record)->payload[(arg)+1]

#else /* !OSI_ENV_BIG_ENDIAN */

#define __osi_TracePoint_record_arg_set64(record, arg, value) \
    osi_Macro_Begin \
        (record)->payload[(arg)+1] = ((value) >> 32); \
        (record)->payload[arg] = (value) & 0xffffffff; \
    osi_Macro_End
#define __osi_TracePoint_record_arg_get64(record, arg, value) \
    *(value) = (((osi_int64)((record)->payload[(arg)+1])) << 32) | \
	(record)->payload[arg]

#endif /* !OSI_ENV_BIG_ENDIAN */
#else /* !OSI_ENV_NATIVE_INT64_TYPE */
#if defined(OSI_ENV_BIG_ENDIAN)

#define __osi_TracePoint_record_arg_set64(record, arg, value) \
    SplitInt64((value), \
	       (record)->payload[arg], \
	       (record)->payload[(arg)+1])
#define __osi_TracePoint_record_arg_get64(record, arg, value) \
    FillInt64(*(value), \
	      (record)->payload[arg], \
	      (record)->payload[(arg)+1])

#else /* !OSI_ENV_BIG_ENDIAN */

#define __osi_TracePoint_record_arg_set64(record, arg, value) \
    SplitInt64((value), \
	       (record)->payload[(arg)+1], \
	       (record)->payload[arg])
#define __osi_TracePoint_record_arg_get64(record, arg, value) \
    FillInt64(*(value), \
	      (record)->payload[(arg)+1], \
	      (record)->payload[arg])

#endif /* !OSI_ENV_BIG_ENDIAN */
#endif /* !OSI_ENV_NATIVE_INT64_TYPE */

#else /* (OSI_TYPE_REGISTER_BITS != 32) */

#define __osi_TracePoint_record_arg_set64(record, arg, value) \
    (record)->payload[arg] = (value)
#define __osi_TracePoint_record_arg_get64(record, arg, value) \
    *(value) = (record)->payload[arg]

#endif /* (OSI_TYPE_REGISTER_BITS != 32) */



/*
 * getter/setter methods which operate on
 * all known trace record versions
 */
osi_extern osi_result
osi_TracePoint_record_VX_arg_set64(osi_TracePoint_record_ptr_t * record,
				   osi_uint32 arg_in,
				   osi_int64 value);
osi_extern osi_result
osi_TracePoint_record_VX_arg_get64(osi_TracePoint_record_ptr_t * record,
				   osi_uint32 arg_in,
				   osi_int64 * value);
osi_extern osi_result
osi_TracePoint_record_VX_arg_add64(osi_TracePoint_record_ptr_t * record,
				   osi_int64 value);


#endif /* _OSI_TRACE_COMMON_RECORD_IMPL_H */
