/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KMEM_LOCAL_H
#define	_OSI_LINUX_KMEM_LOCAL_H


#ifdef AFS_LINUX26_ENV

#include <linux/percpu.h>


/*
 * cpu-local memory support
 */

/*
 * warning: this interface is a work in progress
 * it is not included in the build system yet
 */

/* linux has alloc_percpu */
#define OSI_MEM_LOCAL_PERCPU_ALLOC 1

#include <osi/COMMON/mem_local.h>

osi_extern void * osi_mem_local_linux_handle;

#define osi_mem_local_ctx_get() \
    ((struct osi_mem_local_ctx_data *)per_cpu_ptr(osi_mem_local_linux_handle, smp_procesor_id()))
#define osi_mem_local_ctx_put()


#endif /* AFS_LINUX26_ENV */


#endif /* _OSI_LINUX_KMEM_LOCAL_H */
