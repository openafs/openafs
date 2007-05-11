/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_UMEM_H
#define _OSI_LEGACY_UMEM_H 1


#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * general purpose userspace memory allocator
 */

#define OSI_MEM_ALLOC_ALIGNMENT 8 /* MOST userspace allocators provide 8-byte 
				     aligned memory 
				     (DARWIN ppc being a notable exception) */

#define osi_mem_alloc(size) malloc(size)
#define osi_mem_alloc_nosleep(size) malloc(size)
#define osi_mem_free(buf, size) free(buf)

#define osi_mem_small_alloc osi_mem_alloc
#define osi_mem_small_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_small_free osi_mem_free
#define osi_mem_large_alloc osi_mem_alloc
#define osi_mem_large_alloc_nosleep osi_mem_alloc_nosleep
#define osi_mem_large_free osi_mem_free

#if defined(OSI_SUN5_ENV) || defined(OSI_LINUX_ENV) || defined(OSI_AIX_ENV) || defined(OSI_SGI_ENV)
#define osi_mem_page_size_default(s) (((*(s)) = sysconf(_SC_PAGESIZE)) >= 0 ? OSI_OK : OSI_FAIL)
#elif defined(OSI_HPUX_ENV)
#define osi_mem_page_size_default(s) (((*(s)) = sysconf(_SC_PAGE_SIZE)) >= 0 ? OSI_OK : OSI_FAIL)
#else
#define osi_mem_page_size_default(s) ((*(s)) = 4096, OSI_MEM_RESULT_FAKED)
#endif

#define osi_mem_zero(buf, len) memset((buf), 0, (len))

#define OSI_IMPLEMENTS_MEM_REALLOC 1

#define osi_mem_realloc(buf, len) realloc((buf), (len))
#define osi_mem_realloc_nosleep(buf, len) realloc((buf), (len))

/*
 * aligned memory allocator
 */

#define OSI_IMPLEMENTS_MEMORY_ALIGNED 1

#if defined(OSI_LINUX_ENV)
#include <malloc.h>
#endif

#define osi_mem_aligned_alloc(size, align) memalign((align), (size))
#define osi_mem_aligned_alloc_nosleep(size, align) memalign((align), (size))
#define osi_mem_aligned_free(buf, size) free(buf)


#define osi_mem_PkgInit       osi_null_init_func
#define osi_mem_PkgShutdown   osi_null_fini_func

#endif /* _OSI_LEGACY_UMEM_H */
