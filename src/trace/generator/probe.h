/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_PROBE_RGY_H
#define _OSI_TRACE_GENERATOR_PROBE_RGY_H 1


/*
 * osi tracing framework
 * probe point control
 */

#include <trace/directory.h>


osi_extern osi_result osi_trace_probe_rgy_PkgInit(void);
osi_extern osi_result osi_trace_probe_rgy_PkgShutdown(void);


/* forward declare.
 * internal details of this structure are hidden
 * in trace/generator/probe_impl.h */
struct osi_trace_probe;
typedef struct osi_trace_probe * osi_trace_probe_t;

/*
 * allocate a probe
 *
 * [OUT] probe -- opaque handle to probe
 * [OUT] id -- new probe's id
 * [IN] mode -- initial probe mode (0 == disable, non-zero == enable)
 */
osi_extern osi_result osi_trace_probe_register(osi_trace_probe_t *,
					       osi_trace_probe_id_t *,
					       int mode);

/*
 * enable a probe
 *
 * [IN] probe -- opaque handle to a probe
 */
osi_extern osi_result osi_trace_probe_enable(osi_trace_probe_t);

/*
 * disable a probe
 *
 * [IN] probe -- opaque handle to a probe
 */
osi_extern osi_result osi_trace_probe_disable(osi_trace_probe_t);

/*
 * get probe id
 *
 * [IN] probe -- opaque handle to a probe
 * [OUT] id -- probe's id
 */
osi_extern osi_result osi_trace_probe_id(osi_trace_probe_t,
					 osi_trace_probe_id_t *);

/*
 * enable a probe by id
 *
 * [IN] id -- probe id
 */
osi_extern osi_result osi_trace_probe_enable_by_id(osi_trace_probe_id_t);

/*
 * disable a probe by id
 *
 * [IN] id -- probe id
 */
osi_extern osi_result osi_trace_probe_disable_by_id(osi_trace_probe_id_t);

/*
 * enable a group of probes
 *
 * [IN] filter -- probe filter
 */
osi_extern osi_result osi_trace_probe_enable_by_filter(const char * filter);

/*
 * disable a group of probes
 *
 * [IN] filter -- probe filter
 */
osi_extern osi_result osi_trace_probe_disable_by_filter(const char * filter);

#endif /* _OSI_TRACE_GENERATOR_PROBE_RGY_H */
