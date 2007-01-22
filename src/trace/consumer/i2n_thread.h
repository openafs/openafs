/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_I2N_THREAD_H
#define _OSI_TRACE_CONSUMER_I2N_THREAD_H 1

/*
 * osi tracing framework
 * trace consumer library
 * i2n resolution queue 
 * control thread / thread pool
 */

osi_extern osi_result 
osi_trace_consumer_i2n_thread_unblock(osi_trace_gen_id_t,
				      osi_trace_probe_id_t);

osi_extern osi_result osi_trace_consumer_i2n_thread_PkgInit(void);
osi_extern osi_result osi_trace_consumer_i2n_thread_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_I2N_THREAD_H */
