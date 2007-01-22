/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_LOCAL_NAMESPACE_H
#define _OSI_TRACE_CONSUMER_LOCAL_NAMESPACE_H 1


/*
 * osi tracing framework
 * trace consumer library
 * consumer local probe namespace
 */

osi_extern osi_result osi_trace_consumer_local_namespace_alloc(osi_trace_probe_id_t *);

osi_extern osi_result osi_trace_consumer_local_namespace_PkgInit(void);
osi_extern osi_result osi_trace_consumer_local_namespace_PkgShutdown(void);


#endif /* _OSI_TRACE_CONSUMER_LOCAL_NAMESPACE_H */
