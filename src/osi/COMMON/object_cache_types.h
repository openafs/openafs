/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_CACHE_TYPES_H
#define _OSI_COMMON_OBJECT_CACHE_TYPES_H 1

/*
 * osi memory object cache
 * type definitions
 */

/* 
 * constructor function prototype
 *
 * [IN] buf -- newly allocated object buffer
 * [IN] sdata -- opaque pointer
 * [IN] flags -- flags (should be ignored for now)
 *
 * return codes:
 *   zero on success
 *   non-zero on failure
 */
typedef int osi_mem_object_cache_constructor_t(void * buf, void * sdata, int flags);

/* 
 * destructor function prototype
 *
 * [IN] buf -- object about to be returned to main system memory pool
 * [IN] sdata -- opaque pointer
 *
 */
typedef void osi_mem_object_cache_destructor_t(void * buf, void * sdata);

/* 
 * reclaim function prototype
 * called on low-memory conditions
 *
 * [IN] sdata -- opaque pointer
 *
 */
typedef void osi_mem_object_cache_reclaim_t(void * sdata);


/*
 * now pull in definition of _osi_mem_object_cache_handle_t
 */

#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_object_cache_types.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#if defined(OSI_SUN59_ENV)
#include <osi/SOLARIS/umem_object_cache_types.h>
#endif

#endif /* !OSI_KERNELSPACE_ENV */

#include <osi/LEGACY/object_cache_types.h>


/*
 * finally, define osi_mem_object_cache_t
 */

typedef struct {
    _osi_mem_object_cache_handle_t handle;
    osi_mem_object_cache_options_t options;
} osi_mem_object_cache_t;

#endif /* _OSI_COMMON_OBJECT_CACHE_TYPES_H */
