/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_CACHE_H
#define _OSI_COMMON_OBJECT_CACHE_H 1

/*
 * osi mem object cache
 * common support
 * public interfaces
 */

#define OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF buf
#define OSI_MEM_OBJECT_CACHE_FUNC_ARG_ROCK rock


#include <osi/COMMON/object_cache_types.h>

/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_object_cache.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_SUN59_ENV)
#include <osi/SOLARIS/umem_object_cache.h>
#endif

#endif /* !OSI_ENV_KERNELSPACE */

#include <osi/LEGACY/object_cache.h>


/* external interface */

osi_extern osi_mem_object_cache_t * 
osi_mem_object_cache_create(char * name, 
			    osi_size_t size, 
			    osi_size_t align,
			    void * rock,
			    osi_mem_object_cache_constructor_t * ctor,
			    osi_mem_object_cache_destructor_t * dtor,
			    osi_mem_object_cache_reclaim_t * reclaim,
			    osi_mem_object_cache_options_t * options);
osi_extern osi_result osi_mem_object_cache_destroy(osi_mem_object_cache_t *);

osi_extern void * osi_mem_object_cache_alloc(osi_mem_object_cache_t * cache);
osi_extern void * osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t * cache);
osi_extern void osi_mem_object_cache_free(osi_mem_object_cache_t * cache, void * buf);

OSI_INIT_FUNC_PROTOTYPE(osi_mem_object_cache_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_mem_object_cache_PkgShutdown);

#endif /* _OSI_COMMON_OBJECT_CACHE_H */
