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
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mutex.h>
#include <sys/types.h>
#include <sys/kmem.h>


/*
 * general purpose memory allocator
 */

void *
osi_mem_alloc(size_t len)
{
    return kmem_alloc(len, KM_SLEEP);
}

void *
osi_mem_alloc_nosleep(size_t len)
{
    return kmem_alloc(len, KM_NOSLEEP);
}

void
osi_mem_free(void * buf, size_t len)
{
    kmem_free(buf, len);
}
