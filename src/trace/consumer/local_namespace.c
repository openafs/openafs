/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <trace/gen_rgy.h>
#include <osi/osi_counter.h>
#include <trace/consumer/local_namespace.h>

/*
 * osi tracing framework
 * trace consumer library
 * consumer local probe namespace
 */


struct {
    osi_counter32_t max_id;
} osi_trace_consumer_local_namespace;

osi_result
osi_trace_consumer_local_namespace_alloc(osi_trace_probe_id_t * id_out)
{
    osi_counter32_val_t id;
    osi_counter32_inc_nv(&osi_trace_consumer_local_namespace.max_id, &id);
    *id_out = (osi_trace_probe_id_t) id;
    return (id != 0) ? OSI_OK : OSI_FAIL;
}

osi_result
osi_trace_consumer_local_namespace_PkgInit(void)
{
    osi_counter32_init(&osi_trace_consumer_local_namespace.max_id, 0);
    return OSI_OK;
}

osi_result
osi_trace_consumer_local_namespace_PkgShutdown(void)
{
    osi_counter32_destroy(&osi_trace_consumer_local_namespace.max_id);
    return OSI_OK;
}
