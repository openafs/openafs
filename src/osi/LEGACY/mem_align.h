/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MEM_ALIGN_H_
#define _OSI_LEGACY_MEM_ALIGN_H_



/*
 * aligned memory allocator
 *
 * this is an ugly hack to allow arbitrary-length aligned allocations
 * on platforms that don't otherwise support such things
 */

#define OSI_IMPLEMENTS_MEMORY_ALIGNED 1

typedef enum {
    OSI_MEM_ALLOC_SLEEP,
    OSI_MEM_ALLOC_NOSLEEP
} osi_mem_alloc_sleep_flag_t;


osi_extern void * osi_mem_aligned_alloc_internal(size_t, size_t, osi_mem_alloc_sleep_flag_t);

#define osi_mem_aligned_alloc(len, align) \
    osi_mem_aligned_alloc_internal((len), (align), OSI_MEM_ALLOC_SLEEP)

#define osi_mem_aligned_alloc_nosleep(len, align) \
    osi_mem_aligned_alloc_internal((len), (align), OSI_MEM_ALLOC_NOSLEEP)

osi_extern void osi_mem_aligned_free(void *, size_t);


#endif /* _OSI_LEGACY_MEM_ALIGN_H_ */
