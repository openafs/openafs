/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_I2N_CACHE_H
#define _OSI_TRACE_CONSUMER_I2N_CACHE_H


/*
 * osi tracing framework
 * trace consumer process
 * probe I2N cache
 */

/*
 * lookup a probe name from its generator id and probe id
 *
 * [IN]    gen -- generator id
 * [IN]    id  -- probe id
 * [INOUT] name -- probe name buffer
 * [IN]    name_len -- probe name buffer len
 */
osi_extern osi_result osi_trace_consumer_i2n_cache_lookup(osi_trace_gen_id_t gen,
							  osi_trace_probe_id_t id,
							  char * name,
							  size_t name_len);


osi_extern osi_result osi_trace_consumer_i2n_cache_PkgInit(void);
osi_extern osi_result osi_trace_consumer_i2n_cache_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_I2N_CACHE_H */
