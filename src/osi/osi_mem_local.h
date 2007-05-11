/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_MEM_LOCAL_H
#define _OSI_OSI_MEM_LOCAL_H 1


/*
 * platform-independent osi_mem_local API
 * context-local memory support
 *
 * for userspace, osi_mem_local provides thread-local memory support
 * for kernelspace, osi_mem_local provides cpu-local memory support
 *
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_MEM_LOCAL)
 *
 *  osi_mem_local_key_t key;
 *  void (osi_mem_local_destructor *)(void *);
 *
 *  osi_result osi_mem_local_key_create(osi_mem_local_key_t *,
 *                                      osi_mem_local_destructor_t *);
 *    -- create a thread-local/cpu-local data handle
 *
 *  osi_result osi_mem_local_key_destroy(osi_mem_local_key_t);
 *    -- destroy a thread-local/cpu-local data handle
 *
 * The following two calls are mutually exclusive ways of managing
 * context-local memory.  Each cpu/thread has a primary local buffer
 * several KB in size.  osi_mem_local_alloc allocates part of this
 * buffer in all contexts for a specific purpose and associates an
 * already created key with it.  osi_mem_local_set is generally used
 * for very large allocations that are separate from the main buffer.
 * For these large buffers, each context must allocate a buffer and
 * call osi_mem_local_set.
 *
 *  osi_result osi_mem_local_alloc(osi_mem_local_key_t,
 *                                 osi_size_t len, osi_size_t align);
 *    -- allocate space in the main context-local buffer
 *
 *  osi_result osi_mem_local_set(osi_mem_local_key_t, void *);
 *    -- set the pointer for this key on this thread/cpu
 *
 *  void * osi_mem_local_get(osi_mem_local_key_t);
 *    -- get the pointer for this key on this thread/cpu
 *       (disable preemption for kernel)
 *
 *  void osi_mem_local_put(void *);
 *    -- put back a ref to this memory object
 *       (re-enable preemption for kernel)
 */


typedef void osi_mem_local_destructor_t(void *);


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_local.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/umem_local.h>
#endif

#endif /* !OSI_ENV_KERNELSPACE */


#if !defined(OSI_IMPLEMENTS_MEM_LOCAL)
#define osi_mem_local_PkgInit      osi_null_init_func
#define osi_mem_local_PkgShutdown  osi_null_fini_func
#endif /* !OSI_IMPLEMENTS_MEM_LOCAL */

#endif /* _OSI_OSI_MEM_LOCAL_H */
