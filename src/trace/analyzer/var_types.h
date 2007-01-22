/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_VAR_TYPES_H
#define _OSI_TRACE_ANALYZER_VAR_TYPES_H 1

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

#include <osi/osi_list.h>
#include <osi/osi_mutex.h>
#include <osi/osi_refcnt.h>
#include <trace/gen_rgy.h>

typedef enum {
    OSI_TRACE_ANLY_VAR_COUNTER,
    OSI_TRACE_ANLY_VAR_SUM,
    OSI_TRACE_ANLY_VAR_LOGIC,
    OSI_TRACE_ANLY_VAR_INTEGER_MATH,
    OSI_TRACE_ANLY_VAR_COMPARITOR,
    OSI_TRACE_ANLY_VAR_CLOCK,
    OSI_TRACE_ANLY_VAR_EDGE_TRIGGER,
    OSI_TRACE_ANLY_VAR_LEVEL_TRIGGER,
    OSI_TRACE_ANLY_VAR_MAX_ID
} osi_trace_anly_var_type_t;

/* forward declare the various private data types */
struct osi_trace_anly_counter_data;
struct osi_trace_anly_sum_data;
struct osi_trace_anly_logic_data;
struct osi_trace_anly_integer_math_data;
struct osi_trace_anly_comparitor_data;
struct osi_trace_anly_clock_data;
struct osi_trace_anly_edge_trigger_data;
struct osi_trace_anly_level_trigger_data;

typedef union {
    void * osi_volatile raw;
    struct osi_trace_anly_counter_data * osi_volatile counter;
    struct osi_trace_anly_sum_data * osi_volatile sum;
    struct osi_trace_anly_logic_data * osi_volatile logic;
    struct osi_trace_anly_integer_math_data * osi_volatile integer_math;
    struct osi_trace_anly_comparitor_data * osi_volatile comparitor;
    struct osi_trace_anly_clock_data * osi_volatile clock;
    struct osi_trace_anly_edge_trigger_data * osi_volatile edge_trigger;
    struct osi_trace_anly_level_trigger_data * osi_volatile level_trigger;
} osi_trace_anly_var_data_ptr_t;

typedef struct {
    osi_volatile osi_trace_probe_id_t probe;
    osi_volatile osi_trace_gen_id_t gen;
    osi_volatile osi_uint8 arg;
    struct osi_trace_anly_var * osi_volatile var; /* optional fast-path linkage */
} osi_trace_anly_fan_in_t;

#define OSI_TRACE_ANLY_FAN_IN_LIST_RECORD_SIZE 2

typedef struct {
    osi_trace_anly_fan_in_t * vec;
    size_t vec_len;
} osi_trace_anly_fan_in_vec_t;

typedef struct {
    osi_list_element_volatile fan_in_list;
    osi_trace_anly_fan_in_t * osi_volatile fan_in[OSI_TRACE_ANLY_FAN_IN_LIST_RECORD_SIZE];
} osi_trace_anly_fan_in_list_t;

typedef struct osi_trace_anly_var {
    osi_list_element_volatile var_list;
    osi_list_head_volatile var_fan_in_list;
    osi_trace_probe_id_t osi_volatile var_id;
    osi_trace_anly_var_type_t osi_volatile var_type;
    osi_trace_anly_var_data_ptr_t var_data;
    osi_refcnt_t var_refcount;
    osi_mutex_t var_lock;
} osi_trace_anly_var_t;

typedef struct {
    osi_trace_anly_fan_in_t addr;
    osi_int64 osi_volatile val;
} osi_trace_anly_var_update_t;

typedef struct {
    osi_uint64 * vec;
    size_t vec_len;
} osi_trace_anly_var_input_vec_t;

#endif /* _OSI_TRACE_ANALYZER_VAR_TYPES_H */
