/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_STATS_IMPL_H
#define _OSI_COMMON_STATS_IMPL_H 1


#include <osi/osi_atomic.h>
#include <osi/osi_mem_local.h>


/*
 * detect where we can do atomic ops
 */

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_16) && defined(OSI_IMPLEMENTS_ATOMIC_INC_16_NV) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_16) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_16_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER16 1
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_16) && defined(OSI_IMPLEMENTS_ATOMIC_DEC_16_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL16 1
#endif
#endif

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32) && defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_32) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_32_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER32 1
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_32) && defined(OSI_IMPLEMENTS_ATOMIC_DEC_32_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL32 1
#endif
#endif

#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64) && defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_64) && defined(OSI_IMPLEMENTS_ATOMIC_ADD_64_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64 1
#if defined(OSI_IMPLEMENTS_ATOMIC_DEC_64) && defined(OSI_IMPLEMENTS_ATOMIC_DEC_64_NV)
#define OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64 1
#endif
#endif


/*
 * primary stats type definitions
 */

typedef osi_uint16 osi_stats_counter16_t;
typedef osi_uint32 osi_stats_counter32_t;
typedef osi_uint64 osi_stats_counter64_t;
typedef osi_uint16 osi_stats_level16_t;
typedef osi_uint32 osi_stats_level32_t;
typedef osi_uint64 osi_stats_level64_t;
typedef osi_uint16 osi_stats_max16_t;
typedef osi_uint32 osi_stats_max32_t;
typedef osi_uint64 osi_stats_max64_t;
typedef osi_uint16 osi_stats_min16_t;
typedef osi_uint32 osi_stats_min32_t;
typedef osi_uint64 osi_stats_min64_t;
typedef struct {
    osi_uint32 sum;
    osi_uint32 samples;
} osi_stats_average16_t;
typedef struct {
    osi_uint64 sum;
    osi_uint32 samples;
} osi_stats_average32_t;
typedef struct {
    osi_uint64 sum;
    osi_uint64 samples;
} osi_stats_average64_t;
typedef osi_uint32 osi_stats_time32_t;


/*
 * context-local stats type definitions
 *
 * notice that in many cases the context-local
 * types for a given statistic are smaller than
 * the global analogues in order to increase
 * stats performance.
 *
 * The correct value of any statistic is the
 * summation of the global value, and all 
 * context-local values
 */

typedef osi_uint16 osi_stats_local_counter16_t;
typedef osi_uint32 osi_stats_local_counter32_t;
typedef osi_fast_uint osi_stats_local_counter64_t;
#define OSI_STATS_LOCAL_COUNTER64_BITS OSI_TYPE_FAST_BITS
typedef osi_uint16 osi_stats_local_level16_t;
typedef osi_uint32 osi_stats_local_level32_t;
typedef osi_fast_uint osi_stats_local_level64_t;
#define OSI_STATS_LOCAL_LEVEL64_BITS OSI_TYPE_FAST_BITS
typedef osi_uint16 osi_stats_local_max16_t;
typedef osi_uint32 osi_stats_local_max32_t;
typedef osi_uint64 osi_stats_local_max64_t;
typedef osi_uint16 osi_stats_local_min16_t;
typedef osi_uint32 osi_stats_local_min32_t;
typedef osi_uint64 osi_stats_local_min64_t;
typedef osi_stats_average16_t osi_stats_local_average16_t;
typedef struct {
    osi_fast_uint sum;
    osi_fast_uint samples;
} osi_stats_local_average32_t;
typedef struct {
    osi_fast_uint sum;
    osi_fast_uint samples;
} osi_stats_local_average64_t;


/*
 * statistics tracepoint interfaces
 */

#define _osi_stats_tracepoint(probe,event,op,var) \
    osi_Macro_Begin \
        op ; \
        osi_Trace_Stats_Event(probe, event, var); \
    osi_Macro_End

#define _osi_stats_tracepoint_atomic(probe,event,type,var,op_active,op_inactive) \
    osi_TracePoint_ConditionalBegin(probe) { \
        type var ; \
        op_active ; \
        osi_Trace_Stats_Event_Fire(probe, event, var); \
    } osi_TracePoint_ConditionalElse { \
        op_inactive ; \
    } osi_TracePoint_ConditionalEnd


/*
 * stats tracepoint argument interfaces
 */

#define _osi_stats_trace_arg16(grp_mbr,nv) osi_Trace_Args2(grp_mbr,nv)
#define _osi_stats_trace_arg16_d(grp_mbr,nv,delta) osi_Trace_Args3(grp_mbr,nv,delta)
#define _osi_stats_trace_arg32(grp_mbr,nv) osi_Trace_Args2(grp_mbr,nv)
#define _osi_stats_trace_arg32_d(grp_mbr,nv,delta) osi_Trace_Args3(grp_mbr,nv,delta)

#ifdef AFS_64BIT_ENV
#if (OSI_TYPE_REGISTER_BITS == 64)
#define _osi_stats_trace_arg64(grp_mbr, nv) osi_Trace_Args2(grp_mbr, nv)
#define _osi_stats_trace_arg64_d(grp_mbr, nv, delta) osi_Trace_Args3(grp_mbr, nv, delta)
#define _osi_stats_trace_arg64_d32(grp_mbr, nv, delta) osi_Trace_Args3(grp_mbr, nv, delta)
#else /* OSI_TYPE_REGISTER_BITS != 64 */
#define _osi_stats_trace_arg64(grp_mbr, nv) osi_Trace_Args3(grp_mbr, nv >> 32, nv & 0xffffffff)
#define _osi_stats_trace_arg64_d(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, nv >> 32, nv & 0xffffffff, \
                                                            delta >> 32, delta & 0xffffffff)
#define _osi_stats_trace_arg64_d32(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, nv >> 32, nv & 0xffffffff, \
                                                              0, delta)
#endif /* OSI_TYPE_REIGSTER_BITS != 64 */
#else /* !AFS_64BIT_ENV */
#define _osi_stats_trace_arg64(grp_mbr, nv) osi_Trace_Args3(grp_mbr, nv.high, nv.low)
#define _osi_stats_trace_arg64_d(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, nv.high, nv.low, delta.high, delta.low)
#define _osi_stats_trace_arg64_d32(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, nv.high, nv.low, 0, delta)
#endif /* !AFS_64BIT_ENV */

#if (OSI_TYPE_FAST_BITS == 64)
#define _osi_stats_trace_arg_fast(grp_mbr, nv) osi_Trace_Args2(grp_mbr, nv)
#define _osi_stats_trace_arg_fast_d(grp_mbr, nv, delta) osi_Trace_Args3(grp_mbr, nv, delta)
#define _osi_stats_trace_arg_fast_d32(grp_mbr, nv, delta) osi_Trace_Args3(grp_mbr, nv, delta)
#else /* OSI_TYPE_FAST_BITS != 64 */
#define _osi_stats_trace_arg_fast(grp_mbr, nv) osi_Trace_Args3(grp_mbr, 0, nv)
#ifdef AFS_64BIT_ENV
#define _osi_stats_trace_arg_fast_d(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, 0, nv, delta >> 32, delta & 0xffffffff)
#define _osi_stats_trace_arg_fast_d32(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, 0, nv, 0, delta)
#else /* !AFS_64BIT_ENV */
#define _osi_stats_trace_arg_fast_d(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, 0, nv, delta.high, delta.low)
#define _osi_stats_trace_arg_fast_d32(grp_mbr, nv, delta) osi_Trace_Args5(grp_mbr, 0, nv, 0, delta)
#endif /* !AFS_64BIT_ENV */
#endif /* OSI_TYPE_FAST_BITS != 64 */

#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_arg64_fits32(val) (((val) & 0xffffffff) == (val))
#else /* !OSI_DATAMODEL_IS(OSI_LP64_ENV) */
#define _osi_stats_arg64_fits32(val) ((val).high == 0)
#endif /* !OSI_DATAMODEL_IS(OSI_LP64_ENV) */


/*
 * context-local stats interfaces
 */

#if defined(OSI_IMPLEMENTS_MEM_LOCAL)
#define OSI_STATS_CTX_LOCAL_BUFFER 1
#endif

#if defined(OSI_STATS_CTX_LOCAL_BUFFER)
#define _osi_stats_ctx_local_op_dynamic(key, type, varname, op, initfunc) \
    osi_Macro_Begin \
        type * varname = (type *) osi_mem_local_get(key); \
        if (osi_compiler_expect_false(varname == osi_NULL)) { \
           varname = initfunc; \
        } \
        op ; \
        osi_mem_local_put(varname); \
    osi_Macro_End
#define _osi_stats_ctx_local_op_static(key, type, varname, op) \
    osi_Macro_Begin \
        type * varname = (type *) osi_mem_local_get(key); \
        op ; \
        osi_mem_local_put(varname); \
    osi_Macro_End
#endif


/*
 * raw statistics update interfaces
 */

#define _osi_stats_16_inc(table,var)  ((table)->var++)
#define _osi_stats_32_inc(table,var)  ((table)->var++)
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_inc(table,var)  ((table)->var++)
#else
#define _osi_stats_64_inc(table,var)  (IncUInt64(&(table)->var))
#endif

#define _osi_stats_16_dec(table,var)  ((table)->var--)
#define _osi_stats_32_dec(table,var)  ((table)->var--)
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_dec(table,var)  ((table)->var--)
#else
#define _osi_stats_64_dec(table,var)  (DecUInt64(&(table)->var))
#endif

#define _osi_stats_16_add(table,var,val)   ((table)->var += (val))
#define _osi_stats_32_add(table,var,val)   ((table)->var += (val))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_add(table,var,val)   ((table)->var += (val))
#define _osi_stats_64_add32(table,var,val) ((table)->var += (osi_uint64)(val))
#else
#define _osi_stats_64_add(table,var,val)   (AddUInt64((table)->var, val, &(table)->var))
#define _osi_stats_64_add32(table,var,val) (AddUInt64_32u((table)->var, val, &(table)->var))
#endif

#define _osi_stats_16_sub(table,var,val)   ((table)->var -= (val))
#define _osi_stats_32_sub(table,var,val)   ((table)->var -= (val))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_sub(table,var,val)   ((table)->var -= (val))
#define _osi_stats_64_sub32(table,var,val) ((table)->var -= (osi_uint64)(val))
#else
#define _osi_stats_64_sub(table,var,val)   (SubtractUInt64((table)->var, val, &(table)->var))
#define _osi_stats_64_sub32(table,var,val) (SubtractUInt64_32u((table)->var, val, &(table)->var))
#endif

#if (OSI_TYPE_FAST_BITS==32)
#define _osi_stats_fast_inc(table,var) _osi_stats_32_inc(table,var)
#define _osi_stats_fast_dec(table,var) _osi_stats_32_dec(table,var)
#define _osi_stats_fast_add(table,var,val) _osi_stats_32_add(table,var,val)
#define _osi_stats_fast_sub(table,var,val) _osi_stats_32_sub(table,var,val)
#define _osi_stats_fast_add32(table,var,val) _osi_stats_32_add(table,var,val)
#define _osi_stats_fast_sub32(table,var,val) _osi_stats_32_sub(table,var,val)
#elif (OSI_TYPE_FAST_BITS==64)
#define _osi_stats_fast_inc(table,var) _osi_stats_64_inc(table,var)
#define _osi_stats_fast_dec(table,var) _osi_stats_64_dec(table,var)
#define _osi_stats_fast_add(table,var,val) _osi_stats_64_add(table,var,val)
#define _osi_stats_fast_sub(table,var,val) _osi_stats_64_sub(table,var,val)
#define _osi_stats_fast_add32(table,var,val) _osi_stats_64_add32(table,var,val)
#define _osi_stats_fast_sub32(table,var,val) _osi_stats_64_sub32(table,var,val)
#else
#error "osi_fast_uint is neither 32 nor 64 bits wide; please file a bug"
#endif


/* atomic and synchronized update methods */
#define _osi_stats_16_inc_atomic(table,var)  osi_atomic_inc_16(&(table)->var)
#define _osi_stats_16_inc_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_inc((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_inc_atomic(table,var)  osi_atomic_inc_32(&(table)->var)
#define _osi_stats_32_inc_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_inc((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_inc_atomic(table,var)  osi_atomic_inc_64(&(table)->var)
#define _osi_stats_64_inc_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_inc((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_16_dec_atomic(table,var)  osi_atomic_dec_16(&(table)->var)
#define _osi_stats_16_dec_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_dec((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_dec_atomic(table,var)  osi_atomic_dec_32(&(table)->var)
#define _osi_stats_32_dec_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_dec((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_dec_atomic(table,var)  osi_atomic_dec_64(&(table)->var)
#define _osi_stats_64_dec_sync(table,var)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_dec((table),var); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_16_add_atomic(table,var,val)  osi_atomic_add_16(&(table)->var, (val))
#define _osi_stats_16_add_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_add((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_add_atomic(table,var,val)  osi_atomic_add_32(&(table)->var, (val))
#define _osi_stats_32_add_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_add((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_add_atomic(table,var,val)  osi_atomic_add_64(&(table)->var, (val))
#define _osi_stats_64_add_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_add((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_add32_atomic(table,var,val)  osi_atomic_add_64(&(table)->var, (osi_int64)((osi_uint64)(val)))
#define _osi_stats_64_add32_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_add32((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_macro_End

#define _osi_stats_16_sub_atomic(table,var,val)  osi_atomic_add_16(&(table)->var, -(val))
#define _osi_stats_32_sub_atomic(table,var,val)  osi_atomic_add_32(&(table)->var, -(val))

#define _osi_stats_64_sub_atomic(table,var,val)  osi_atomic_add_64(&(table)->var, -(val))
#define _osi_stats_64_sub_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_sub((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_sub32_atomic(table,var,val)  osi_atomic_add_64(&(table)->var, \
                                                                     (-((osi_int64)((osi_uint64)(val)))))
#define _osi_stats_64_sub32_sync(table,var,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_sub32((table),var,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End


/* 
 * return new value versions of stats update interfaces
 */
#define _osi_stats_16_inc_nv(table,var,nv)  (*(nv) = ++((table)->var))
#define _osi_stats_32_inc_nv(table,var,nv)  (*(nv) = ++((table)->var))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_inc_nv(table,var,nv)  (*(nv) = ++((table)->var))
#else
#define _osi_stats_64_inc_nv(table,var,nv) \
    ((IncUInt64(&(table)->var)),(AssignInt64((table)->var,nv)))
#endif

#define _osi_stats_16_dec_nv(table,var,nv)  (*(nv) = --((table)->var))
#define _osi_stats_32_dec_nv(table,var,nv)  (*(nv) = --((table)->var))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_dec_nv(table,var,nv)  (*(nv) = --((table)->var))
#else
#define _osi_stats_64_dec_nv(table,var,nv) \
    ((DecUInt64(&(table)->var)),(AssignInt64((table)->var,nv)))
#endif

#define _osi_stats_16_add_nv(table,var,nv,val)   (*(nv) = ((table)->var += (val)))
#define _osi_stats_32_add_nv(table,var,nv,val)   (*(nv) = ((table)->var += (val)))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_add_nv(table,var,nv,val)   (*(nv) = ((table)->var += (val)))
#define _osi_stats_64_add32_nv(table,var,nv,val) (*(nv) = ((table)->var += (osi_uint64)(val)))
#else
#define _osi_stats_64_add_nv(table,var,nv,val) \
    ((AddUInt64((table)->var, val, &(table)->var)), \
     (AssignInt64((table)->var,nv)))
#define _osi_stats_64_add32_nv(table,var,nv,val) \
    ((AddUInt64_32u((table)->var, val, &(table)->var)), \
     (AssignInt64((table)->var,nv)))
#endif

#define _osi_stats_16_sub_nv(table,var,nv,val)   (*(nv) = ((table)->var -= (val)))
#define _osi_stats_32_sub_nv(table,var,nv,val)   (*(nv) = ((table)->var -= (val)))
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_64_sub_nv(table,var,nv,val)   (*(nv) = ((table)->var -= (val)))
#define _osi_stats_64_sub32_nv(table,var,nv,val) (*(nv) = ((table)->var -= (osi_uint64)(val)))
#else
#define _osi_stats_64_sub_nv(table,var,nv,val) \
    ((SubtractUInt64((table)->var, val, &(table)->var)), \
     (AssignInt64((table)->var,nv)))
#define _osi_stats_64_sub32_nv(table,var,nv,val) \
    ((SubtractUInt64_32u((table)->var, val, &(table)->var)), \
     (AssignInt64((table)->var,nv)))
#endif


/* atomic and synchronized update methods */
#define _osi_stats_16_inc_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_inc_16_nv(&(table)->var))
#define _osi_stats_16_inc_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_inc_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_inc_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_inc_32_nv(&(table)->var))
#define _osi_stats_32_inc_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_inc_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_inc_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_inc_64_nv(&(table)->var))
#define _osi_stats_64_inc_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_inc_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_16_dec_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_dec_16_nv(&(table)->var))
#define _osi_stats_16_dec_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_dec_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_dec_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_dec_32_nv(&(table)->var))
#define _osi_stats_32_dec_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_dec_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_dec_atomic_nv(table,var,nv)  (*(nv) = osi_atomic_dec_64_nv(&(table)->var))
#define _osi_stats_64_dec_sync_nv(table,var,nv)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_dec_nv((table),var,nv); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_16_add_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_16_nv(&(table)->var, (val)))
#define _osi_stats_16_add_sync_nv(table,var,nv,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_16_add_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_32_add_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_32_nv(&(table)->var, (val)))
#define _osi_stats_32_add_sync_nv(table,var,nv,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_32_add_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_add_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_64_nv(&(table)->var, (val)))
#define _osi_stats_64_add_sync_nv(table,var,nv,val)  \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_add_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_add32_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_64_nv(&(table)->var, (osi_int64)((osi_uint64)(val))))
#define _osi_stats_64_add32_sync_nv(table,var,nv,val) \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_add32_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_16_sub_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_16_nv(&(table)->var, -(val)))
#define _osi_stats_32_sub_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_32_nv(&(table)->var, -(val)))

#define _osi_stats_64_sub_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_64_nv(&(table)->var, -(val)))
#define _osi_stats_64_sub_sync_nv(table,var,nv,val) \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_sub_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End

#define _osi_stats_64_sub32_atomic_nv(table,var,nv,val) \
    (*(nv) = osi_atomic_add_64_nv(&(table)->var, \
                                  (-((osi_int64)((osi_uint64)(val))))))
#define _osi_stats_64_sub32_sync_nv(table,var,nv,val) \
    osi_Macro_Begin \
        osi_mutex_Lock(&(table)->lock); \
        _osi_stats_64_sub32_nv((table),var,nv,(val)); \
        osi_mutex_Unlock(&(table)->lock); \
    osi_Macro_End


/*
 * context-local -> global update interactions
 */

#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define _osi_stats_l2g_decl_inc64(name) \
    osi_int64 name = 0x0000000100000000
#define _osi_stats_l2g_decl_dec64(name) \
    osi_int64 name = 0xFFFFFFFF00000000
#else /* !OSI_DATAMODEL_IS(OSI_LP64_ENV) */
#define _osi_stats_l2g_decl_inc64(name) \
    osi_int64 name; \
    FillInt64(name,1,0)
#define _osi_stats_l2g_decl_dec64(name) \
    osi_int64 name; \
    FillInt64(name,-1,0)
#endif /* !OSI_DATAMODEL_IS(OSI_LP64_ENV) */


/* level64 update */
#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64)
#define _osi_stats_level64_inc_l2g(globl,var,name) \
    _osi_stats_64_add_atomic(globl,var,name)
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64 */
#define _osi_stats_level64_inc_l2g(globl,var,name) \
    _osi_stats_64_add_sync(globl,var,name)
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64 */

#if (OSI_STATS_LOCAL_LEVEL64_BITS==32)
#define osi_stats_level64_inc_l2g(locl,globl,var) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(!((locl)->var))) { \
            _osi_stats_l2g_decl_inc64(__osi_st_incval); \
            _osi_stats_level64_inc_l2g(globl,var,__osi_st_incval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_LEVEL64_BITS==64)
#define osi_stats_level64_inc_l2g(locl,globl,var)
#else /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */
#error "osi_stats_local_level64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */

#if (OSI_STATS_LOCAL_LEVEL64_BITS==32)
#define osi_stats_level64_dec_l2g(locl,globl,var) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(((locl)->var) == 0xFFFFFFFF)) { \
            _osi_stats_l2g_decl_dec64(__osi_st_decval); \
            _osi_stats_level64_dec_l2g(globl,var,__osi_st_decval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_LEVEL64_BITS==64)
#define osi_stats_level64_dec_l2g(locl,globl,var)
#else /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */
#error "osi_stats_local_level64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */

#if (OSI_STATS_LOCAL_LEVEL64_BITS==32)
#define osi_stats_level64_add_l2g(locl,globl,var,oval) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(((locl)->var) < (oval))) { \
            _osi_stats_l2g_decl_inc64(__osi_st_incval); \
            _osi_stats_level64_inc_l2g(globl,var,__osi_st_incval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_LEVEL64_BITS==64)
#define osi_stats_level64_add_l2g(locl,globl,var,oval)
#else /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */
#error "osi_stats_local_level64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */

#if (OSI_STATS_LOCAL_LEVEL64_BITS==32)
#define osi_stats_level64_sub_l2g(locl,globl,var,oval) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(((locl)->var) > (oval))) { \
            _osi_stats_l2g_decl_dec64(__osi_st_decval); \
            _osi_stats_level64_dec_l2g(globl,var,__osi_st_decval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_LEVEL64_BITS==64)
#define osi_stats_level64_sub_l2g(locl,globl,var,oval)
#else /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */
#error "osi_stats_local_level64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_LEVEL64_BITS!=32 && OSI_STATS_LOCAL_LEVEL64_BITS!=64 */

#if (OSI_STATS_LOCAL_LEVEL64_BITS==32)
#define _osi_stats_local_level64_arg64_fits(val) _osi_stats_arg64_fits32(val)
#elif (OSI_STATS_LOCAL_LEVEL64_BITS==64)
#define _osi_stats_local_level64_arg64_fits(val) 1
#else
#error "osi_stats_local_level64_t is neither 32 nor 64 bits wide; please file a bug"
#endif


/* counter64 update */
#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64)
#define _osi_stats_counter64_inc_l2g(globl,var,name) \
    _osi_stats_64_add_atomic(globl,var,name)
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64 */
#define _osi_stats_counter64_inc_l2g(globl,var,name) \
    _osi_stats_64_add_sync(globl,var,name)
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64 */

#if (OSI_STATS_LOCAL_COUNTER64_BITS==32)
#define osi_stats_counter64_inc_l2g(locl,globl,var) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(!((locl)->var))) { \
            _osi_stats_l2g_decl_inc64(__osi_st_incval); \
            _osi_stats_counter64_inc_l2g(globl,var,__osi_st_incval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_COUNTER64_BITS==64)
#define osi_stats_counter64_inc_l2g(locl,globl,var)
#else /* OSI_STATS_LOCAL_COUNTER64_BITS!=32 && OSI_STATS_LOCAL_COUNTER64_BITS!=64 */
#error "osi_stats_local_counter64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_COUNTER64_BITS!=32 && OSI_STATS_LOCAL_COUNTER64_BITS!=64 */

#if (OSI_STATS_LOCAL_COUNTER64_BITS==32)
#define osi_stats_counter64_add_l2g(locl,globl,var,oval) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(((locl)->var) < (oval))) { \
            _osi_stats_l2g_decl_inc64(__osi_st_incval); \
            _osi_stats_counter64_inc_l2g(globl,var,__osi_st_incval); \
        } \
    osi_Macro_End
#elif (OSI_STATS_LOCAL_COUNTER64_BITS==64)
#define osi_stats_counter64_add_l2g(locl,globl,var,oval)
#else /* OSI_STATS_LOCAL_COUNTER64_BITS!=32 && OSI_STATS_LOCAL_COUNTER64_BITS!=64 */
#error "osi_stats_local_counter64_t is neither 32 nor 64 bits wide; please file a bug"
#endif /* OSI_STATS_LOCAL_COUNTER64_BITS!=32 && OSI_STATS_LOCAL_COUNTER64_BITS!=64 */

#if (OSI_STATS_LOCAL_COUNTER64_BITS==32)
#define _osi_stats_local_counter64_arg64_fits(val) _osi_stats_arg64_fits32(val)
#elif (OSI_STATS_LOCAL_COUNTER64_BITS==64)
#define _osi_stats_local_counter64_arg64_fits(val) 1
#else
#error "osi_stats_local_counter64_t is neither 32 nor 64 bits wide; please file a bug"
#endif

#endif /* _OSI_COMMON_STATS_IMPL_H */
