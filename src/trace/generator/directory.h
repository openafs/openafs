/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_DIRECTORY_H
#define _OSI_TRACE_GENERATOR_DIRECTORY_H 1


/*
 * osi tracing framework
 * probe point directory
 */

#include <trace/directory.h>
#include <trace/generator/probe.h>

/* 
 * map a probe name to its probe handle
 *
 * [IN] name -- name of probe to lookup
 * [OUT] probe -- opaque handle to probe
 */
osi_extern osi_result osi_trace_directory_N2P(const char *, osi_trace_probe_t *);

/*
 * map a probe id to its primary name
 *
 * [IN]    id -- probe id
 * [INOUT] name -- pointer to buffer in which to store the probe name
 * [IN]    len -- length of name buffer
 */
osi_extern osi_result osi_trace_directory_I2N(osi_trace_probe_id_t, char * name, size_t len);

/*
 * map a probe id to its probe handle
 *
 * [IN] id -- probe id
 * [OUT] probe -- opaque handle to probe
 */
osi_extern osi_result osi_trace_directory_I2P(osi_trace_probe_id_t, osi_trace_probe_t *);

/* 
 * register primary name for a probe
 *
 * [IN] name -- name for probe
 * [IN] probe -- opaque handle to probe
 */
osi_extern osi_result osi_trace_directory_probe_register(const char * name, osi_trace_probe_t);

/*
 * register an alias for a probe
 *
 * [IN] name -- alias name for probe
 * [IN] probe -- opaque handle to probe
 */
osi_extern osi_result osi_trace_directory_probe_register_alias(const char * name, osi_trace_probe_t);

/*
 * perform some action for each matching probe
 *
 * [IN] name -- name of probe (possibly containing wildcards
 * [IN] function -- function pointer to call for each matching probe
 * [IN] data -- opaque pointer to pass to function
 */
typedef osi_result osi_trace_directory_foreach_action_t(osi_trace_probe_t, void *);
osi_extern osi_result osi_trace_directory_foreach(const char * match,
						  osi_trace_directory_foreach_action_t * fp,
						  void * data);

#endif /* _OSI_TRACE_GENERATOR_DIRECTORY_H */
