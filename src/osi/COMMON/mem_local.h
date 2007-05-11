/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_MEM_LOCAL_H
#define _OSI_COMMON_MEM_LOCAL_H 1


/*
 * context-local memory
 * common support
 *
 * backend OS mem_local implementations must implement
 * the following interfaces:
 *
 *   #define OSI_MEM_LOCAL_CPU_ARRAY
 *     -- define on platforms which don't provide a cpu-specific memory 
 *        API for kernel modules.  When this is defined, compatability
 *        code is enabled which declares an array of osi_mem_local_ctx_data
 *        pointers, one for each cpu.
 *
 * the following two tunables are only meangingful if
 * !defined(OSI_MEM_LOCAL_CPU_ARRAY):
 *   #define OSI_MEM_LOCAL_PERCPU_ALLOC
 *     -- define on platforms which have a per-cpu memory allocation interface.
 *        (e.g. on linux 2.6 you can have the kernel automatically go out
 *         and do numa-aware allocations on every cpu when declaring the
 *         cpu-specific variable)
 *
 *   #define OSI_MEM_LOCAL_PREEMPT_INTERNAL
 *     -- define on platforms where the cpu-specific memory implementation 
 *        handles kernel preemption enable/disable internally 
 *        (e.g. the linux 2.6 cpu-specific memory api calls get_cpu() and 
 *         put_cpu() under the covers)
 *
 *   osi_result osi_mem_local_os_PkgInit(void);
 *     -- do os-specific initialization of context-local memory
 *
 *   osi_result osi_mem_local_os_PkgShutdown(void);
 *     -- do os-specific shutdown of context-local memory
 *
 * the following interfaces are only required if
 * !defined(OSI_MEM_LOCAL_CPU_ARRAY)
 *   struct osi_mem_local_ctx_data * osi_mem_local_ctx_get(void);
 *     -- get the osi_mem_local_ctx_data struct for this context
 *
 *   void osi_mem_local_ctx_put(void);
 *     -- put back the osi_mem_local_ctx_data struct for this context
 *
 * the following interface is only required if
 * !defined(OSI_MEM_LOCAL_CPU_ARRAY) && !defined(OSI_MEM_LOCAL_PERCPU_ALLOC)
 *   void osi_mem_local_ctx_set(struct osi_mem_local_ctx_data *);
 *     -- set the osi_mem_local_ctx_data struct for this context
 *
 * the following interface is only required if
 * !defined(OSI_MEM_LOCAL_CPU_ARRAY) && defined(OSI_ENV_KERNELSPACE)
 *   struct osi_mem_local_ctx_data * osi_mem_local_ctx_get_ctx(osi_mem_local_ctx_id_t);
 *     -- get the osi_mem_local_ctx_data struct for a specific context
 *
 * the following interface is only required if
 * defined(OSI_MEM_LOCAL_PERCPU_ALLOC)
 *   osi_result osi_mem_local_os_alloc_percpu_buffers(void);
 *     -- allocate the osi_mem_local_ctx_data structs for each cpu
 *        (for the !defined(OSI_MEM_LOCAL_PERCPU_ALLOC) case, we have an 
 *         os-indep implementation which walks the osi cpu list, tries to 
 *         bind to each cpu (in an attempt to be numa-friendly), and 
 *         performs osi_mem_aligned_alloc()s)
 */

#define OSI_IMPLEMENTS_MEM_LOCAL 1


#include <osi/COMMON/mem_local_types.h>


#if defined(OSI_ENV_KERNELSPACE) && defined(OSI_MEM_LOCAL_CPU_ARRAY)
/*
 * Many platforms don't provide a mechanism for cpu-local storage
 * for kernel modules, so we have to do it ourselves.  This
 * is a vector of pointers to local data structs, indexed by
 * cpuid
 */
osi_extern struct osi_mem_local_ctx_data ** osi_mem_local_ctx_data;

#define osi_mem_local_ctx_get()        (osi_mem_local_ctx_data[osi_cpu_current_id()])
#define osi_mem_local_ctx_put()
#define osi_mem_local_ctx_get_ctx(ctx) (osi_mem_local_ctx_data[ctx])
#define osi_mem_local_ctx_set(ctx)     (osi_mem_local_ctx_data[osi_cpu_current_id()] = (ctx))
#endif /* OSI_MEM_LOCAL_CPU_ARRAY */


OSI_INIT_FUNC_PROTOTYPE(osi_mem_local_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_mem_local_PkgShutdown);

osi_extern osi_result osi_mem_local_ctx_init(osi_mem_local_ctx_id_t ctx, 
					     struct osi_mem_local_ctx_data ** ret);



osi_extern osi_result osi_mem_local_key_create(osi_mem_local_key_t *,
					       osi_mem_local_destructor_t *);
osi_extern osi_result osi_mem_local_key_destroy(osi_mem_local_key_t);

osi_extern osi_result osi_mem_local_alloc(osi_mem_local_key_t,
					  osi_size_t len, osi_size_t align);

osi_extern osi_result osi_mem_local_set(osi_mem_local_key_t,
					void * data);

#if defined(OSI_ENV_USERSPACE)
/* get a specific thread's mem local context structure */
osi_extern struct osi_mem_local_ctx_data * osi_mem_local_ctx_get_ctx(osi_mem_local_ctx_id_t);
#endif


/*
 * context-local getters
 */

#if defined(OSI_ENV_KERNELSPACE)
#if defined(OSI_MEM_LOCAL_PREEMPT_INTERNAL)

#define osi_mem_local_get(key) \
    (osi_mem_local_ctx_get()->keys[key])
#define osi_mem_local_put(key) \
    (osi_mem_local_ctx_put())

#else /* !OSI_MEM_LOCAL_PREEMPT_INTERNAL */

/* getter is an inline function for this case */

#define osi_mem_local_put(key) \
    osi_Macro_Begin \
        osi_mem_local_ctx_put(); \
        osi_kernel_preemption_enable(); \
    osi_Macro_End

#endif /* !OSI_MEM_LOCAL_PREEMPT_INTERNAL */
#else /* !OSI_ENV_KERNELSPACE */

#define osi_mem_local_get(key) \
    (osi_mem_local_ctx_get()->keys[key])
#define osi_mem_local_put(key)

#endif /* !OSI_ENV_KERNELSPACE */


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/mem_local_inline.h>
#endif


#endif /* _OSI_COMMON_MEM_LOCAL_H */
