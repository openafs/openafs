/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KMEM_H
#define	_OSI_LINUX_KMEM_H


#include <osi/LINUX/kmem_flags.h>


/*
 * general purpose memory allocator
 */

#define OSI_MEM_ALLOC_ALIGNMENT 8

#define osi_mem_alloc(len) \
    kmalloc((len), OSI_LINUX_MEM_ALLOC_FLAG_SLEEP)
#define osi_mem_alloc_nosleep(len) \
    kmalloc((len), OSI_LINUX_MEM_ALLOC_FLAG_NOSLEEP)
#define osi_mem_free(buf, len) \
    kfree(buf)

#define osi_mem_small_alloc osi_mem_alloc
#define osi_mem_small_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_small_free osi_mem_free
#define osi_mem_large_alloc osi_mem_alloc
#define osi_mem_large_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_large_free osi_mem_free


#define osi_mem_page_size_default(s) ((*(s)) = PAGE_SIZE, OSI_OK)

#define osi_mem_zero(buf, len) memset((buf), 0, (len))


#define osi_mem_PkgInit()      (OSI_OK)
#define osi_mem_PkgShutdown()  (OSI_OK)


/*
 * aligned allocations are not supported, except via kmem_cache_*
 * using LEGACY implementation for now
 */
#include <osi/LEGACY/mem_align.h>


#endif /* _OSI_SOLARIS_KMEM_H */
