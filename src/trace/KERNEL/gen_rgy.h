/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_GEN_RGY_H
#define _OSI_TRACE_KERNEL_GEN_RGY_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 * public interfaces
 */

#include <trace/gen_rgy.h>
#include <trace/KERNEL/postmaster.h>

osi_extern osi_result osi_trace_gen_rgy_register(osi_trace_generator_address_t *, 
						 osi_trace_gen_id_t *);
osi_extern osi_result osi_trace_gen_rgy_unregister(osi_trace_gen_id_t);

osi_extern osi_result osi_trace_gen_rgy_lookup_I2A(osi_trace_gen_id_t,
						   osi_trace_generator_address_t *);
osi_extern osi_result osi_trace_gen_rgy_lookup_A2I(osi_trace_generator_address_t *,
						   osi_trace_gen_id_t *);

/* generator registry syscall interfaces */
osi_extern int osi_trace_gen_rgy_sys_register(void *,
					      void *);
osi_extern int osi_trace_gen_rgy_sys_unregister(osi_trace_gen_id_t);
osi_extern int osi_trace_gen_rgy_sys_lookup_I2A(osi_trace_gen_id_t,
						void *);
osi_extern int osi_trace_gen_rgy_sys_lookup_A2I(void *,
						void *);
osi_extern int osi_trace_gen_rgy_sys_get(osi_trace_gen_id_t);
osi_extern int osi_trace_gen_rgy_sys_put(osi_trace_gen_id_t);
osi_extern int osi_trace_gen_rgy_sys_get_by_addr(void *,
						 void *);

#define osi_trace_gen_id(id) \
    ((*(id))=0, OSI_OK)


osi_extern osi_result osi_trace_gen_rgy_PkgInit(void);
osi_extern osi_result osi_trace_gen_rgy_PkgShutdown(void);

#endif /* _OSI_TRACE_KERNEL_GEN_RGY_H */
