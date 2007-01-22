/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KMEM_FLAGS_H
#define	_OSI_LINUX_KMEM_FLAGS_H


#include <linux/slab.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <linux/gfp.h>
#else
#include <linux/mm.h>
#endif

/*
 * general purpose memory allocator
 * mapping of linux kmem GFP flags
 * onto the osi interface layer
 */

#define OSI_MEM_ALLOC_ALIGNMENT 8

#ifndef __GFP_NORETRY
#define __GFP_NORETRY 0
#endif

#define OSI_LINUX_MEM_ALLOC_FLAG_SLEEP    (GFP_KERNEL)
#define OSI_LINUX_MEM_ALLOC_FLAG_NOSLEEP  (GFP_KERNEL | __GFP_NORETRY)


#define OSI_LINUX_MEM_PAGE_SIZE_DEFAULT   (PAGE_SIZE)


#endif /* _OSI_SOLARIS_KMEM_FLAGS_H */
