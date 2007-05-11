/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_PROBE_RGY_IMPL_H
#define _OSI_TRACE_GENERATOR_PROBE_RGY_IMPL_H 1

/*
 * osi tracing framework
 * trace data generator library
 * probe data registry
 */

#include <osi/osi_mutex.h>

struct osi_trace_probe {
    osi_trace_probe_id_t probe_id;
    volatile osi_uint32 activation_count;
    osi_mutex_t probe_lock;
};

osi_extern osi_init_func_t * osi_trace_probe_mail_init_fp;
osi_extern osi_fini_func_t * osi_trace_probe_mail_fini_fp;

#endif /* _OSI_TRACE_GENERATOR_PROBE_RGY_IMPL_H */
