/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_VAR_IMPL_H
#define _OSI_TRACE_ANALYZER_VAR_IMPL_H 1

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

#include <trace/analyzer/var_types.h>

typedef osi_result osi_trace_anly_var_update_func_t(/*IN*/osi_trace_anly_var_t *,
						    /*IN*/osi_trace_anly_var_update_t *,
						    /*IN*/osi_trace_anly_var_input_vec_t *,
						    /*IN*/void * sdata,
						    /*OUT*/osi_uint64 * result);

osi_extern osi_result __osi_trace_anly_var_type_register(osi_trace_anly_var_type_t,
							 osi_mem_object_cache_t *,
							 osi_trace_anly_var_update_func_t *,
							 void * sdata);
osi_extern osi_result __osi_trace_anly_var_type_unregister(osi_trace_anly_var_type_t);

osi_extern osi_result __osi_trace_anly_var_alloc(osi_trace_anly_var_type_t,
						 osi_trace_anly_var_t **,
						 osi_trace_anly_fan_in_vec_t *,
						 int lock_hold);
osi_extern osi_result __osi_trace_anly_var_free(osi_trace_anly_var_t *);

#endif /* _OSI_TRACE_ANALYZER_VAR_IMPL_H */
