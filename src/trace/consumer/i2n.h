/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_I2N_H
#define _OSI_TRACE_CONSUMER_I2N_H 1


/*
 * osi tracing framework
 * trace consumer library
 * primary i2n API
 */

#include <trace/gen_rgy.h>

osi_extern osi_result
osi_trace_consumer_i2n_lookup(osi_trace_gen_id_t gen_id,
			      osi_trace_probe_id_t probe_id,
			      char * probe_name,
			      size_t probe_name_len);
osi_extern osi_result
osi_trace_consumer_i2n_lookup_cached(osi_trace_gen_id_t gen_id,
				     osi_trace_probe_id_t probe_id,
				     char * probe_name,
				     size_t probe_name_len);

osi_extern osi_result
osi_trace_consumer_i2n_gen_new(osi_trace_gen_id_t);
osi_extern osi_result
osi_trace_consumer_i2n_gen_invalidate(osi_trace_gen_id_t);

osi_extern osi_result
osi_trace_consumer_i2n_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_i2n_PkgShutdown(void);


#endif /* _OSI_TRACE_CONSUMER_I2N_H */
