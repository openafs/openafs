/*
 * Copyright 2006, Sine Nomine Associates and others.
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
 * public interface
 */

#include <osi/COMMON/object_cache_types.h>

osi_extern osi_mem_object_cache_t * 
osi_mem_object_cache_create(char * name, 
			    osi_size_t size, 
			    osi_size_t align,
			    void * spec_data,
			    osi_mem_object_cache_constructor_t * ctor,
			    osi_mem_object_cache_destructor_t * dtor,
			    osi_mem_object_cache_reclaim_t * reclaim,
			    osi_mem_object_cache_options_t * options);
osi_extern osi_result osi_mem_object_cache_destroy(osi_mem_object_cache_t *);

osi_extern void * osi_mem_object_cache_alloc(osi_mem_object_cache_t * cache);
osi_extern void * osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t * cache);
osi_extern void osi_mem_object_cache_free(osi_mem_object_cache_t * cache, void * buf);


/* dummy functions for caches where NULL fp's aren't allowed */
osi_extern int osi_mem_object_cache_dummy_ctor(void * buf, void * sdata, int flags);
osi_extern void osi_mem_object_cache_dummy_dtor(void * buf, void * sdata);
osi_extern void osi_mem_object_cache_dummy_reclaim(void * sdata);


osi_extern osi_result osi_mem_object_cache_PkgInit(void);
osi_extern osi_result osi_mem_object_cache_PkgShutdown(void);

#endif /* _OSI_COMMON_OBJECT_CACHE_H */
