/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_MEM_H
#define _OSI_OSI_MEM_H 1


/*
 * platform-independent osi_memory API
 * dynamic memory allocation/deallocation interface
 *
 *  OSI_MEM_ALLOC_ALIGNMENT
 *    -- the guaranteed alignment for this allocator
 *
 *  void * osi_mem_alloc(size_t len);
 *    -- allocate len bytes of memory
 *
 *  void * osi_mem_alloc_nosleep(size_t len);
 *    -- allocate len bytes of memory without sleeping
 *
 *  void osi_mem_free(void *, size_t len);
 *    -- free len bytes of memory
 *
 *  void * osi_mem_small_alloc(size_t len);
 *    -- alloc optimized for small blobs
 *
 *  void * osi_mem_small_alloc_nosleep(size_t len);
 *    -- alloc optimized for small blobs without sleeping
 *
 *  void osi_mem_small_free(void *, size_t len);
 *    -- free optimized for small blobs
 *
 *  void * osi_mem_large_alloc(size_t len);
 *    -- alloc optimized for large blobs
 *
 *  void * osi_mem_large_alloc_nosleep(size_t len);
 *    -- alloc optimized for large blobs without sleeping
 *
 *  void osi_mem_large_free(void *, size_t len);
 *    -- free optimized for large blobs
 *
 *  void * osi_mem_aligned_alloc(size_t len, size_t align);
 *    -- allocate len bytes aligned on alignment bytes
 *
 *  void * osi_mem_aligned_alloc_nosleep(size_t len, size_t align);
 *    -- allocate len bytes aligned on alignment bytes without sleeping
 *
 *  void osi_mem_aligned_free(void *, size_t len);
 *    -- free an aligned allocation
 *
 *  osi_result osi_mem_page_size_default(size_t *);
 *    -- returns the default page size
 *
 *  void osi_mem_zero(void * buf, size_t len);
 *    -- zero's a buffer
 *
 *  void osi_mem_copy(void * dst, const void * src, size_t len)
 *    -- copies a buffer (non-overlapping regions ONLY)
 *
 *  void osi_mem_move(void * dst, const void * src, size_t len)
 *    -- moves a buffer (safe for overlapping regions)
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_MEM_REALLOC)
 *
 *  void * osi_mem_realloc(void * buf, size_t len);
 *    -- change size of buf, or move buf to new allocation
 *
 *  void * osi_mem_realloc_nosleep(void * buf, size_t len);
 *    -- change size of buf, or move buf to new allocation
 *
 */


#define OSI_MEM_RESULT_FAKED  OSI_RESULT_SUCCESS_CODE(OSI_MEM_ERROR_FAKED)

#define osi_mem_copy(dst,src,len) memcpy(dst,src,len)
#define osi_mem_move(dst,src,len) bcopy(src,dst,len)


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem.h>
#elif defined(OSI_LINUX_ENV)
#include <osi/LINUX/kmem.h>
#else
#include <osi/LEGACY/kmem.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#include <osi/LEGACY/umem.h>

#endif /* !OSI_ENV_KERNELSPACE */


#endif /* _OSI_OSI_MEM_H */
