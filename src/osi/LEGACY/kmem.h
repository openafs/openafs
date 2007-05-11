/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_KMEM_H
#define _OSI_LEGACY_KMEM_H 1


/*
 * general purpose memory allocator
 */

#define OSI_MEM_ALLOC_ALIGNMENT 8

#define osi_mem_alloc(len) afs_osi_Alloc(len)
#define osi_mem_alloc_nosleep(len) afs_osi_Alloc_NoSleep(len)
#define osi_mem_free(buf, len) afs_osi_Free((buf), (len))

#define osi_mem_small_alloc osi_mem_alloc
#define osi_mem_small_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_small_free osi_mem_free
#define osi_mem_large_alloc osi_mem_alloc
#define osi_mem_large_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_large_free osi_mem_free


#define osi_mem_page_size_default(s) ((*(s)) = 4096, OSI_MEM_RESULT_FAKED)


#define osi_mem_zero(buf, len) memset((buf), 0, (len))


#define osi_mem_PkgInit       osi_null_init_func
#define osi_mem_PkgShutdown   osi_null_fini_func


/*
 * legacy interface does not support aligned allocations
 * use the osi internal aligned allocator from LEGACY
 */
#include <osi/LEGACY/mem_align.h>


#endif /* _OSI_LEGACY_KMEM_H */
