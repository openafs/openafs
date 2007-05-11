/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KMEM_H
#define _OSI_SOLARIS_KMEM_H 1


#include <sys/types.h>
#include <sys/kmem.h>


/*
 * general purpose memory allocator
 */

#define OSI_MEM_ALLOC_ALIGNMENT 8

osi_extern void * osi_mem_alloc(size_t);
osi_extern void * osi_mem_alloc_nosleep(size_t);
osi_extern void osi_mem_free(void *, size_t);

#define osi_mem_small_alloc osi_mem_alloc
#define osi_mem_small_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_small_free osi_mem_free
#define osi_mem_large_alloc osi_mem_alloc
#define osi_mem_large_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_large_free osi_mem_free


#define osi_mem_page_size_default(s) ((*(s)) = PAGESIZE, OSI_OK)

#define osi_mem_zero(buf, len) memset((buf), 0, (len))


#define osi_mem_PkgInit       osi_null_init_func
#define osi_mem_PkgShutdown   osi_null_fini_func


/*
 * aligned allocations are not supported, except via kmem_cache_*
 * using LEGACY implementation for now
 */
#include <osi/LEGACY/mem_align.h>

#endif /* _OSI_SOLARIS_KMEM_H */
