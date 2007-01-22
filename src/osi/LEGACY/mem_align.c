/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mem.h>


/*
 * aligned memory allocator
 *
 * this is an ugly hack to allow arbitrary-length aligned allocations
 * on platforms that don't otherwise support such things
 */


/* this size should work until OSI_DATAMODEL_LP128 happens... */
#define OSI_MEM_ALIGNED_OFFSET 16

struct osi_mem_aligned_header {
    void * addr;
    size_t len;
};

void *
osi_mem_aligned_alloc_internal(size_t len, size_t align, osi_mem_alloc_sleep_flag_t flag)
{
    struct osi_mem_aligned_header * hdr;
    osi_uintptr_t addr, mask, base;
    size_t alloc_len = len;

    if (len < OSI_MEM_ALIGNED_OFFSET) {
	alloc_len = OSI_MEM_ALIGNED_OFFSET;
    }

    /* now add in the header length */
    alloc_len += OSI_MEM_ALIGNED_OFFSET;

    /* it wouldn't make any sense to call with align < 16 */
    if (align < OSI_MEM_ALIGNED_OFFSET) {
	align = OSI_MEM_ALIGNED_OFFSET;
    }

    /* alloc enough to deal with whatever random alignment we're given */
    alloc_len += align - OSI_MEM_ALLOC_ALIGNMENT;

    if (flag == OSI_MEM_ALLOC_SLEEP) {
	addr = (osi_uintptr_t) osi_mem_alloc(alloc_len);
    } else {
	addr = (osi_uintptr_t) osi_mem_alloc_nosleep(alloc_len);
	if (!addr) {
	    return osi_NULL;
	}
    }

    /* go to the first proper alignment */
    mask = (osi_uintptr_t) align;
    mask = ~(mask-1);
    base = addr & mask;
    base += align;

    /* if there's not enough room to stuff the header
     * structure beneath the first alignment, then move
     * the base out to the second alignment offset */
    if ((base - addr) < OSI_MEM_ALIGNED_OFFSET) {
	base += align;
    }

    hdr = (struct osi_mem_aligned_header *) (base - OSI_MEM_ALIGNED_OFFSET);
    hdr->addr = (void *) addr;
    hdr->len = alloc_len;
    return ((void *) base);
}

void
osi_mem_aligned_free(void * buf, size_t len)
{
    struct osi_mem_aligned_header * hdr;
    osi_uintptr_t addr;

    addr = (osi_uintptr_t) buf;
    addr -= OSI_MEM_ALIGNED_OFFSET;

    hdr = (struct osi_mem_aligned_header *) addr;

    osi_mem_free(hdr->addr, hdr->len);
}
