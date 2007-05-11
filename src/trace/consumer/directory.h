/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_DIRECTORY_H
#define _OSI_TRACE_CONSUMER_DIRECTORY_H 1


/*
 * osi tracing framework
 * probe point directory
 */

#include <trace/directory.h>
#include <trace/gen_rgy.h>

/*
 * probe point interface
 */


osi_extern osi_result osi_trace_directory_N2I(osi_trace_gen_id_t gen,
					      const char * name, 
					      osi_trace_probe_id_t * id);

osi_extern osi_result osi_trace_directory_I2N(osi_trace_gen_id_t gen,
					      osi_trace_probe_id_t id, 
					      char * name, 
					      osi_size_t buflen,
					      osi_bool_t blocking);

#endif /* _OSI_TRACE_CONSUMER_DIRECTORY_H */
