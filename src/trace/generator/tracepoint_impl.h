/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_TRACEPOINT_IMPL_H
#define _OSI_TRACE_TRACEPOINT_IMPL_H 1


/*
 * osi tracing framework
 * tracepoint implementation
 */


/* all trace args get casted to type osi_register_uint */
#define _osi_TraceCast(x)  ((osi_register_uint)(x))


/*
 * tracepoint argument macros
 *
 * to simplify things for version 1
 * we are going to require that all probe record parameters
 * be osi_register_int sized data blobs
 *
 * this requirement will probably be relaxed in future versions
 * of the osi_TracePoint_record encoding
 */

#define osi_Trace_Args0() 0

#define osi_Trace_Args1(v1) 1, \
    _osi_TraceCast(v1)

#define osi_Trace_Args2(v1, v2) 2, \
    _osi_TraceCast(v1), _osi_TraceCast(v2)

#define osi_Trace_Args3(v1, v2, v3) 3, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3)

#define osi_Trace_Args4(v1, v2, v3, v4) 4, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4)

#define osi_Trace_Args5(v1, v2, v3, v4, v5) 5, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5)

#define osi_Trace_Args6(v1, v2, v3, v4, v5, v6) 6, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6)

#define osi_Trace_Args7(v1, v2, v3, v4, v5, v6, v7) 7, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7)

#define osi_Trace_Args8(v1, v2, v3, v4, v5, v6, v7, v8) 8, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8)

#define osi_Trace_Args9(v1, v2, v3, v4, v5, v6, v7, v8, v9) 9, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8), \
    _osi_TraceCast(v9)

#define osi_Trace_Args10(v1, v2, v3, v4, v5, v6, v7, v8, v9, va) 10, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8), \
    _osi_TraceCast(v9), _osi_TraceCast(va)

#define osi_Trace_Args11(v1, v2, v3, v4, v5, v6, v7, v8, v9, va, vb) 11, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8), \
    _osi_TraceCast(v9), _osi_TraceCast(va), _osi_TraceCast(vb)

#define osi_Trace_Args12(v1, v2, v3, v4, v5, v6, v7, v8, v9, va, vb, vc) 12, \
    _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), _osi_TraceCast(v4), \
    _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8), \
    _osi_TraceCast(v9), _osi_TraceCast(va), _osi_TraceCast(vb), _osi_TraceCast(vc)


/*
 * calculation to determine whether
 * a given tracepoint is activated
 */
#define _osi_TracePoint_Activated(offset, bitnum) \
    (_osi_tracepoint_config._tp_activation_vec[offset] & (1 << (bitnum)))

/*
 * tracepoint definition
 */



/*
 * tracepoint support
 *
 * the following is a canonical example of an event tracepoint
 *
 *    osi_TracePoint(osi_Trace_Rx_ProbeId(packet_event_arrive),
 *                   osi_TraceFunc_Event,
 *                   osi_Trace_Event_RxEvent_Id,
 *                   _osi_TraceCast(packet));
 *
 */

#if defined(OSI_ENV_VARIADIC_MACROS)

#define osi_TracePoint(osi_probe_id, osi_tp_func, type, ...) \
    osi_Macro_Begin \
        osi_fast_uint _tp_bitnum, _tp_offset; \
        osi_Trace_Probe2Index(osi_probe_id, _tp_offset, _tp_bitnum); \
        if (osi_compiler_expect_false(_osi_TracePoint_Activated(_tp_offset, _tp_bitnum))) { \
            (*(osi_tp_func))(osi_probe_id, type, __VA_ARGS__); \
        } \
    osi_Macro_End

#else /* ! OSI_ENV_VARIADIC_MACROS */

#define osi_TracePoint(osi_probe_id, f, type, nf, args) \
    osi_Macro_Begin \
        osi_fast_uint _tp_bitnum, _tp_offset; \
        osi_Trace_Probe2Index(osi_probe_id, _tp_offset, _tp_bitnum); \
        if (osi_compiler_expect_false(_osi_TracePoint_Activated(_tp_offset, _tp_bitnum))) { \
	    (*f)(osi_probe_id, type, nf, args); \
        } \
    osi_Macro_End

#endif /* ! OSI_ENV_VARIADIC_MACROS */


/*
 * conditional tracepoint support
 *
 * in certain circumstances, tracepoint argument setup is very expensive
 * for these cases, conditional tracepoints allow us to execute certain code
 * only if the trace point is activated
 *
 * the following is a canonical example of how the stats package uses this feature
 * the *_nv atomic operations are significantly slower on many platforms, so we
 * avoid using them when the tracepoint is deactivated
 *
 *   osi_TracePoint_ConditionalBegin(some_probe_id) {
 *       osi_register_int x;
 *       x = osi_atomic_inc_register_nv(&some_global_counter);
 *       osi_TracePoint_Fire(some_probe_id, some_trace_func, some_trace_type, 1, x);
 *   } osi_TracePoint_ConditionalElse {
 *       osi_atomic_inc_register(&some_global_counter);
 *   } osi_TracePoint_ConditionalEnd;
 *       
 */
#define osi_TracePoint_ConditionalBegin(osi_probe_id) \
    osi_Macro_Begin \
        osi_fast_uint _tp_bitnum, _tp_offset; \
        osi_Trace_Probe2Index(osi_probe_id, _tp_offset, _tp_bitnum); \
        if (osi_compiler_expect_false(_osi_TracePoint_Activated(_tp_offset, _tp_bitnum)))

#define osi_TracePoint_ConditionalElse \
        else

#define osi_TracePoint_ConditionalEnd \
    osi_Macro_End

#if defined(OSI_ENV_VARIADIC_MACROS)

#define osi_TracePoint_Fire(osi_probe_id, osi_tp_func, type, ...) \
            (*(osi_tp_func))(osi_probe_id, type, __VA_ARGS__)

#else /* ! OSI_ENV_VARIADIC_MACROS */

#define osi_TracePoint_Fire(osi_probe_id, f, type, nf, args) \
            (*f)(osi_probe_id, type, nf, args)

#endif /* ! OSI_ENV_VARIADIC_MACROS */


#endif /* _OSI_TRACE_TRACEPOINT_IMPL_H */
