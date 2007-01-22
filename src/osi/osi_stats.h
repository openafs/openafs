/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_STATS_H
#define _OSI_OSI_STATS_H 1


/*
 * platform-independent osi_stats API
 *
 * statistics types
 *
 *  osi_stats_counter16_t
 *  osi_stats_counter32_t
 *  osi_stats_counter64_t
 *    -- unsigned monotonic counters
 *
 *  osi_stats_level16_t
 *  osi_stats_level32_t
 *  osi_stats_level64_t
 *    -- unsigned level statistics
 *
 *  osi_stats_max16_t
 *  osi_stats_max32_t
 *  osi_stats_max64_t
 *    -- unsigned max value statistics
 *
 *  osi_stats_min16_t
 *  osi_stats_min32_t
 *  osi_stats_min64_t
 *    -- unsigned min value statistics
 *
 *  osi_stats_average16_t
 *  osi_stats_average32_t
 *  osi_stats_average64_t
 *    -- unsigned mean value statistics
 *
 * 
 * basic statistics update interfaces
 *
 *  void osi_stats_level16_inc(probe,grp_mbr,event,table,var);
 *  void osi_stats_level32_inc(probe,grp_mbr,event,table,var);
 *  void osi_stats_level64_inc(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter16_inc(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter32_inc(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter64_inc(probe,grp_mbr,event,table,var);
 *    -- increment statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_dec(probe,grp_mbr,event,table,var);
 *  void osi_stats_level32_dec(probe,grp_mbr,event,table,var);
 *  void osi_stats_level64_dec(probe,grp_mbr,event,table,var);
 *    -- decrement statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_add(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level32_add(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level64_add(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter16_add(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter32_add(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter64_add(probe,grp_mbr,event,table,var,val);
 *    -- add $val$ to statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_sub(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level32_sub(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level64_sub(probe,grp_mbr,event,table,var,val);
 *    -- subtract $val$ from statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level64_add32(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter64_add32(probe,grp_mbr,event,table,var,val);
 *    -- add 32-bit unsigned quantity $val$ to 64-bit statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level64_sub32(probe,grp_mbr,event,table,var,val);
 *    -- subtract 32-bit unsigned quantity $val$ from 64-bit statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_max16_update(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_max32_update(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_max64_update(probe,grp_mbr,event,table,var,val);
 *    -- check whether $val$ is greater than member $var$ in table pointer $table$
 *       if it is, then store $val$ into $table$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_min16_update(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_min32_update(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_min64_update(probe,grp_mbr,event,table,var,val);
 *    -- check whether $val$ is less than member $var$ in table pointer $table$
 *       if it is, then store $val$ into $table$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_average16_sample(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_average32_sample(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_average64_sample(probe,grp_mbr,event,table,var,val);
 *    -- update mean statistic member $var$ in table pointer $table$ with new sample $val$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *
 * atomic statistics update interfaces
 *
 *  void osi_stats_level16_inc_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_level32_inc_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_level64_inc_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter16_inc_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter32_inc_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_counter64_inc_atomic(probe,grp_mbr,event,table,var);
 *    -- atomically increment statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_dec_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_level32_dec_atomic(probe,grp_mbr,event,table,var);
 *  void osi_stats_level64_dec_atomic(probe,grp_mbr,event,table,var);
 *    -- atomically decrement statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_add_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level32_add_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level64_add_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter16_add_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter32_add_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter64_add_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically add $val$ to statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level16_sub_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level32_sub_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_level64_sub_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically subtract $val$ from statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level64_add32_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_counter64_add32_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically add 32-bit unsigned quantity $val$ to 64-bit statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_level64_sub32_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically subtract 32-bit unsigned quantity $val$ from 64-bit statistic member $var$ in table pointer $table$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_max16_update_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_max32_update_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_max64_update_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically check whether $val$ is greater than member $var$ in table pointer $table$
 *       if it is, then store $val$ into $table$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_min16_update_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_min32_update_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_min64_update_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically check whether $val$ is less than member $var$ in table pointer $table$
 *       if it is, then store $val$ into $table$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_average16_sample_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_average32_sample_atomic(probe,grp_mbr,event,table,var,val);
 *  void osi_stats_average64_sample_atomic(probe,grp_mbr,event,table,var,val);
 *    -- atomically update mean statistic member $var$ in table pointer $table$ with new sample $val$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *
 * context-local statistics update interfaces
 *
 *  void osi_stats_local_level16_inc(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_level32_inc(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_level64_inc(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_counter16_inc(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_counter32_inc(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_counter64_inc(probe,grp_mbr,event,locl,globl,var);
 *    -- increment statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_level16_dec(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_level32_dec(probe,grp_mbr,event,locl,globl,var);
 *  void osi_stats_local_level64_dec(probe,grp_mbr,event,locl,globl,var);
 *    -- decrement statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_level16_add(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_level32_add(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_level64_add(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_counter16_add(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_counter32_add(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_counter64_add(probe,grp_mbr,event,locl,globl,var,val);
 *    -- add $val$ to statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_level16_sub(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_level32_sub(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_level64_sub(probe,grp_mbr,event,locl,globl,var,val);
 *    -- subtract $val$ from statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_level64_add32(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_counter64_add32(probe,grp_mbr,event,locl,globl,var,val);
 *    -- add 32-bit unsigned quantity $val$ to 64-bit statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_level64_sub32(probe,grp_mbr,event,locl,globl,var,val);
 *    -- subtract 32-bit unsigned quantity $val$ from 64-bit statistic member $var$ in local table pointer $locl$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_max16_update(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_max32_update(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_max64_update(probe,grp_mbr,event,locl,globl,var,val);
 *    -- check whether $val$ is greater than member $var$ in local table pointer $locl$
 *       if it is, then store $val$ into $locl$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_min16_update(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_min32_update(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_min64_update(probe,grp_mbr,event,locl,globl,var,val);
 *    -- check whether $val$ is less than member $var$ in local table pointer $locl$
 *       if it is, then store $val$ into $locl$->$var$
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 *  void osi_stats_local_average16_sample(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_average32_sample(probe,grp_mbr,event,locl,globl,var,val);
 *  void osi_stats_local_average64_sample(probe,grp_mbr,event,locl,globl,var,val);
 *    -- update mean statistic member $var$ in local table pointer $locl$ with new sample $val$
 *       roll up changes to global table pointer $globl$ if overflow
 *       check activation of probe id $probe$
 *       if probe fires, tag as osi_Trace_Event_StatsUpdate_Id and $event$
 *
 */


#include <osi/COMMON/stats_impl.h>


/* 
 * osi_stats_level* update interfaces
 */

/* level16 */
#define osi_stats_level16_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_inc(table,var),_osi_stats_trace_arg16(grp_mbr,(table)->var))
#define osi_stats_level16_dec(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_dec(table,var),_osi_stats_trace_arg16(grp_mbr,(table)->var))
#define osi_stats_level16_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_add(table,var,val),_osi_stats_trace_arg16_d(grp_mbr,(table)->var,val))
#define osi_stats_level16_sub(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_sub(table,var,val),_osi_stats_trace_arg16_d(grp_mbr,(table)->var,val))

#define osi_stats_local_level16_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_level16_inc(probe,event,locl,var)
#define osi_stats_local_level16_dec(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_level16_dec(probe,event,locl,var)
#define osi_stats_local_level16_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_level16_add(probe,event,locl,var,val)
#define osi_stats_local_level16_sub(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_level16_sub(probe,event,locl,var,val)

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL16)
#define osi_stats_level16_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_inc_atomic(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_level16_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_dec_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_dec_atomic(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_level16_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_16_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level16_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_sub_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_sub_atomic(table,var), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL16 */
#define osi_stats_level16_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_inc_sync(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_level16_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_dec_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_dec_sync(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_level16_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_16_add_sync(table,var,val), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level16_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_sub_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_sub_sync(table,var), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL16 */


/* level32 */
#define osi_stats_level32_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_inc(table,var),_osi_stats_trace_arg32(grp_mbr,(table)->var))
#define osi_stats_level32_dec(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_dec(table,var),_osi_stats_trace_arg32(grp_mbr,(table)->var))
#define osi_stats_level32_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_add(table,var,val),_osi_stats_trace_arg32_d(grp_mbr,(table)->var,val))
#define osi_stats_level32_sub(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_sub(table,var,val),_osi_stats_trace_arg32_d(grp_mbr,(table)->var,val))

#define osi_stats_local_level32_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_level32_inc(probe,event,locl,var)
#define osi_stats_local_level32_dec(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_level32_dec(probe,event,locl,var)
#define osi_stats_local_level32_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_level32_add(probe,event,locl,var,val)
#define osi_stats_local_level32_sub(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_level32_sub(probe,event,locl,var,val)

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL32)
#define osi_stats_level32_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_inc_atomic(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_level32_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_dec_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_dec_atomic(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_level32_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_32_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level32_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_sub_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_sub_atomic(table,var), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL32 */
#define osi_stats_level32_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_inc_sync(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_level32_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_dec_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_dec_sync(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_level32_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_32_add_sync(table,var,val), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level32_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_sub_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_sub_sync(table,var), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL32 */


/* level64 */
#define osi_stats_level64_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_inc(table,var), \
                          _osi_stats_trace_arg64(grp_mbr,(table)->var))
#define osi_stats_level64_dec(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_dec(table,var), \
                          _osi_stats_trace_arg64(grp_mbr,(table)->var))
#define osi_stats_level64_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_add(table,var,val), \
                          _osi_stats_trace_arg64_d(grp_mbr,(table)->var,val))
#define osi_stats_level64_sub(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_sub(table,var,val), \
                          _osi_stats_trace_arg64_d(grp_mbr,(table)->var,val))
#define osi_stats_level64_add32(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_add32(table,var,val), \
                          _osi_stats_trace_arg64_d32(grp_mbr,(table)->var,val))
#define osi_stats_level64_sub32(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_sub32(table,var,val), \
                          _osi_stats_trace_arg64_d32(grp_mbr,(table)->var,val))

#define osi_stats_local_level64_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_Macro_Begin \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_inc(locl,var), \
                              _osi_stats_trace_arg_fast((locl)->var)); \
        osi_stats_level64_inc_l2g(locl,globl,var); \
    osi_Macro_End
#define osi_stats_local_level64_dec(probe,grp_mbr,event,locl,globl,var) \
    osi_Macro_Begin \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_dec(locl,var), \
                              _osi_stats_trace_arg_fast((locl)->var)); \
        osi_stats_level64_dec_l2g(locl,globl,var); \
    osi_Macro_End
#define osi_stats_local_level64_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        if (_osi_stats_local_level64_arg64_fits(val)) { \
            osi_fast_uint __osi_st_oval = (locl)->var; \
            _osi_stats_tracepoint(probe,event,_osi_stats_fast_add(locl,var,val), \
                                  _osi_stats_trace_arg_fast_d((locl)->var,val)); \
            osi_stats_level64_add_l2g(locl,globl,var,__osi_st_oval); \
        } else { \
            osi_stats_level64_add_atomic(probe,grp_mbr,event,globl,var,val); \
        } \
    osi_Macro_End
#define osi_stats_local_level64_sub(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        if (_osi_stats_local_level64_arg64_fits(val)) { \
            osi_fast_uint __osi_st_oval = (locl)->var; \
            _osi_stats_tracepoint(probe,event,_osi_stats_fast_sub(locl,var,val), \
                                  _osi_stats_trace_arg_fast_d((locl)->var,val)); \
            osi_stats_level64_sub_l2g(locl,globl,var,__osi_st_oval); \
        } else { \
            osi_stats_level64_sub_atomic(probe,grp_mbr,event,globl,var,val); \
        } \
    osi_Macro_End
#define osi_stats_local_level64_add32(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        osi_fast_uint __osi_st_oval = (locl)->var; \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_add32(locl,var,val), \
                              _osi_stats_trace_arg_fast_d32((locl)->var,val)); \
        osi_stats_level64_add_l2g(locl,globl,var,__osi_st_oval); \
    osi_Macro_End
#define osi_stats_local_level64_sub32(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        osi_fast_uint __osi_st_oval = (locl)->var; \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_sub32(locl,var,val), \
                              _osi_stats_trace_arg_fast_d32((locl)->var,val)); \
        osi_stats_level64_sub_l2g(locl,globl,var,__osi_st_oval); \
    osi_Macro_End

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64)
#define osi_stats_level64_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_inc_atomic(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_level64_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_dec_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_dec_atomic(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_level64_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_sub_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_sub_atomic(table,var), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_add32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add32_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add32_atomic(table,var,val), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_sub32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_sub32_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_sub32_atomic(table,var), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64 */
#define osi_stats_level64_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_inc_sync(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_level64_dec_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_dec_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_dec_sync(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_level64_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add_sync(table,var,val), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_sub_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_sub_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_sub_sync(table,var), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_add32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add32_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add32_sync(table,var,val), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#define osi_stats_level64_sub32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_sub32_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_sub32_sync(table,var), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#endif /* OSI_IMPLEMENTS_STATS_ATOMIC_LEVEL64 */


/* 
 * osi_stats_counter* update interfaces
 */

/* counter16 */
#define osi_stats_counter16_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_inc(table,var),_osi_stats_trace_arg16(grp_mbr,(table)->var))
#define osi_stats_counter16_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_16_add(table,var,val),_osi_stats_trace_arg16_d(grp_mbr,(table)->var,val))

#define osi_stats_local_counter16_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_counter16_inc(probe,event,locl,var)
#define osi_stats_local_counter16_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_counter16_add(probe,event,locl,var,val)

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER16)
#define osi_stats_counter16_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_inc_atomic(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_counter16_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_16_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER16 */
#define osi_stats_counter16_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_16_inc_sync(table,var), \
                                 _osi_stats_trace_arg16(grp_mbr,__tpf_nv))
#define osi_stats_counter16_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint16, __tpf_nv, \
                                 _osi_stats_16_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_16_add_sync(table,var,val), \
                                 _osi_stats_trace_arg16_d(grp_mbr,__tpf_nv,val))
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER16 */


/* counter32 */
#define osi_stats_counter32_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_inc(table,var),_osi_stats_trace_arg32(grp_mbr,(table)->var))
#define osi_stats_counter32_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_32_add(table,var,val),_osi_stats_trace_arg32_d(grp_mbr,(table)->var,val))

#define osi_stats_local_counter32_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_stats_counter32_inc(probe,event,locl,var)
#define osi_stats_local_counter32_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_stats_counter32_add(probe,event,locl,var,val)

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER32)
#define osi_stats_counter32_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_inc_atomic(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_counter32_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_32_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER32 */
#define osi_stats_counter32_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_32_inc_sync(table,var), \
                                 _osi_stats_trace_arg32(grp_mbr,__tpf_nv))
#define osi_stats_counter32_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint32, __tpf_nv, \
                                 _osi_stats_32_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_32_add_sync(table,var,val), \
                                 _osi_stats_trace_arg32_d(grp_mbr,__tpf_nv,val))
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER32 */


/* counter64 */
#define osi_stats_counter64_inc(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_inc(table,var), \
                          _osi_stats_trace_arg64(grp_mbr,(table)->var))
#define osi_stats_counter64_add(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_add(table,var,val), \
                          _osi_stats_trace_arg64_d(grp_mbr,(table)->var,val))
#define osi_stats_counter64_add32(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint(probe,event,_osi_stats_64_add32(table,var,val), \
                          _osi_stats_trace_arg64_d32(grp_mbr,(table)->var,val))

#define osi_stats_local_counter64_inc(probe,grp_mbr,event,locl,globl,var) \
    osi_Macro_Begin \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_inc(locl,var), \
                              _osi_stats_trace_arg_fast((locl)->var)); \
        osi_stats_counter64_inc_l2g(locl,globl,var); \
    osi_Macro_End
#define osi_stats_local_counter64_add(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        if (_osi_stats_local_counter64_arg64_fits(val)) { \
            osi_fast_uint __osi_st_oval = (locl)->var; \
            _osi_stats_tracepoint(probe,event,_osi_stats_fast_add(locl,var,val), \
                                  _osi_stats_trace_arg_fast_d((locl)->var,val)); \
            osi_stats_counter64_add_l2g(locl,globl,var,__osi_st_oval); \
        } else { \
            osi_stats_counter64_add_atomic(probe,grp_mbr,event,globl,var,val); \
        } \
    osi_Macro_End
#define osi_stats_local_counter64_add32(probe,grp_mbr,event,locl,globl,var,val) \
    osi_Macro_Begin \
        osi_fast_uint __osi_st_oval = (locl)->var; \
        _osi_stats_tracepoint(probe,event,_osi_stats_fast_add32(locl,var,val), \
                              _osi_stats_trace_arg_fast_d32((locl)->var,val)); \
        osi_stats_counter64_add_l2g(locl,globl,var,__osi_st_oval); \
    osi_Macro_End

#if defined(OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64)
#define osi_stats_counter64_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_inc_atomic_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_inc_atomic(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_counter64_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add_atomic(table,var,val), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_counter64_add32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add32_atomic_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add32_atomic(table,var,val), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#else /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64 */
#define osi_stats_counter64_inc_atomic(probe,grp_mbr,event,table,var) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_inc_sync_nv(table,var,&__tpf_nv), \
                                 _osi_stats_64_inc_sync(table,var), \
                                 _osi_stats_trace_arg64(grp_mbr,__tpf_nv))
#define osi_stats_counter64_add_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add_sync(table,var,val), \
                                 _osi_stats_trace_arg64_d(grp_mbr,__tpf_nv,val))
#define osi_stats_counter64_add32_atomic(probe,grp_mbr,event,table,var,val) \
    _osi_stats_tracepoint_atomic(probe,event, osi_uint64, __tpf_nv, \
                                 _osi_stats_64_add32_sync_nv(table,var,&__tpf_nv,val), \
                                 _osi_stats_64_add32_sync(table,var,val), \
                                 _osi_stats_trace_arg64_d32(grp_mbr,__tpf_nv,val))
#endif /* !OSI_IMPLEMENTS_STATS_ATOMIC_COUNTER64 */

#endif /* _OSI_OSI_STATS_H */
