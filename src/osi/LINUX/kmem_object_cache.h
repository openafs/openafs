/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KMEM_OBJECT_CACHE_H
#define	_OSI_LINUX_KMEM_OBJECT_CACHE_H

#include <osi/LINUX/kmem_flags.h>

/*
 * XXX this is a work in progress
 *
 * the linux kmem_cache mechanism does not pass
 * an opaque pointer to the constructor and 
 * destructor routines for some odd reason.
 *
 * instead, they pass in a kmem_cache_t pointer,
 * which is NOT always known a priori --
 * kmem_cache_create creates slabs and calls the
 * constructor _before_ returning the kmem_cache_t
 * pointer to the user.
 *
 * as a result, we will have to do all kmem_cache_create
 * calls under a mutex.
 *
 * unfortunately, this means some form of synchronization
 * in the constructor and destructor to do mapping between
 * kmem_cache_t pointers and osi_mem_object_cache_t pointers
 *
 * for linux 2.6, I'm thinking about using seqlock's since
 * this is an extreme case of a read-mostly operation
 */

typedef struct {
    kmem_cache_t * cache;
    void * sdata;
    osi_mem_object_cache_constructor_t * ctor;
    osi_mem_object_cache_destructor_t * dtor;
} _osi_mem_object_cache_handle_t;

#include <osi/COMMON/object_cache_impl.h>

/* these functions are only called from one place. static inline
 * is perfectly acceptable */

osi_static osi_inline void
__osi_linux_object_cache2obj(kmem_cache_t * cache, osi_mem_object_cache_t ** obj_out)
{
    /* XXX writeme */
}

osi_static osi_inline void
__osi_linux_object_cache_ctor(void * buf, kmem_cache_t * cache, unsigned long flags)
{
    osi_mem_object_cache_t * obj;
    
    __osi_linux_object_cache2obj(cache, &obj);
    if (obj->handle.ctor) {
	(*obj->handle.ctor)(buf, obj->handle.sdata, flags);
    }
}

osi_static osi_inline void
__osi_linux_object_cache_dtor(void * buf, kmem_cache_t * cache, unsigned long flags)
{
    osi_mem_object_cache_t * obj;
    
    __osi_linux_object_cache2obj(cache, &obj);
    if (obj->handle.dtor) {
	(*obj->handle.dtor)(buf, obj->handle.sdata);
    }
}

osi_static osi_inline osi_result
_osi_mem_object_cache_create(osi_mem_object_cache_t * cache,
			     char * name,
			     osi_size_t size, 
			     osi_size_t align,
			     void * spec_data,
			     osi_mem_object_cache_constructor_t * ctor,
			     osi_mem_object_cache_destructor_t * dtor,
			     osi_mem_object_cache_reclaim_t * reclaim)
{
    osi_result res = OSI_OK;
    osi_size_t element_size;

    cache->handle.sdata = spec_data;
    cache->handle.ctor = ctor;
    cache->handle.dtor = dtor;

    /*
     * linux slab allocator requires sizeof(void *) to be minimum object size
     */
    element_size = MAX(sizeof(void *), size);

    /* slab allocator semantics are rather different on linux 
     * (compared to solaris)
     *
     * one major difference is that the align argument controls
     * alignment of the first element in a slab, not alignment of
     * every element in the slab
     *
     * thus, we increase object size to enforce alignment of all
     * elements within a slab
     */
    if (align && (element_size & (align-1))) {
	element_size &= ~(align-1);
	element_size += align;
    }

    cache->handle.cache = kmem_cache_create(name,
					    element_size,
					    align,
					    0,
					    &__osi_linux_object_cache_ctor,
					    &__osi_linux_object_cache_dtor);
    if (osi_compiler_expect_false(cache->handle.cache == osi_NULL)) {
	res = OSI_FAIL;
    }
    return res;
}

osi_static osi_inline osi_result
_osi_mem_object_cache_destroy(osi_mem_object_cache_t * cache)
{
    kmem_cache_destroy(cache->handle.cache);
    return OSI_OK;
}

#define _osi_mem_object_cache_alloc(obj) \
    kmem_cache_alloc((obj)->handle.cache, OSI_LINUX_MEM_ALLOC_FLAG_SLEEP)
#define _osi_mem_object_cache_alloc_nosleep(obj) \
    kmem_cache_alloc((obj)->handle.cache, OSI_LINUX_MEM_ALLOC_FLAG_NOSLEEP)
#define _osi_mem_object_cache_free(obj, buf) \
    kmem_cache_free((obj)->handle.cache, buf)

#endif /* _OSI_LINUX_KMEM_OBJECT_CACHE_H */
