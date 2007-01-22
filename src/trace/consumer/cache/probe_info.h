/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_H
#define _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * executable binary metadata cache
 */

#include <trace/gen_rgy.h>
#include <trace/mail/msg.h>
#include <trace/consumer/cache/probe_info_types.h>

osi_extern osi_result
osi_trace_consumer_probe_info_cache_alloc(const char * probe_name,
					  size_t probe_name_len,
					  osi_trace_consumer_probe_info_cache_t ** probe_out);
osi_extern osi_result
osi_trace_consumer_probe_info_cache_get(osi_trace_consumer_probe_info_cache_t * probe);
osi_extern osi_result
osi_trace_consumer_probe_info_cache_put(osi_trace_consumer_probe_info_cache_t * probe);
osi_extern osi_result 
osi_trace_consumer_probe_info_cache_populate(osi_trace_consumer_probe_info_cache_t * probe,
					     osi_trace_mail_msg_probe_i2n_response_t * res);
osi_extern osi_result
osi_trace_consumer_probe_info_cache_lookup_arg(osi_trace_consumer_probe_info_cache_t * probe,
					       osi_uint32 arg,
					       osi_trace_consumer_probe_arg_info_cache_t * info_out);
osi_extern osi_result
osi_trace_consumer_probe_info_cache_lookup_name(osi_trace_consumer_probe_info_cache_t *,
						char * name_buf,
						size_t name_buf_len);

osi_extern osi_result
osi_trace_consumer_probe_info_cache_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_probe_info_cache_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_H */
