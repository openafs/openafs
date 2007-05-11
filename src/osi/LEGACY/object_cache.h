/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MEM_OBJECT_CACHE_H
#define _OSI_LEGACY_MEM_OBJECT_CACHE_H 1

/*
 * osi mem object cache interface
 * legacy backend
 * public interfaces
 */

#if defined(OSI_IMPLEMENTS_MEM_OBJECT_CACHE)
#if !defined(OSI_IMPLEMENTS_NATIVE_MEM_OBJECT_CACHE)
#define OSI_IMPLEMENTS_NATIVE_MEM_OBJECT_CACHE 1
#endif /* !OSI_IMPLEMENTS_NATIVE_MEM_OBJECT_CACHE */
#else /* !OSI_IMPLEMENTS_MEM_OBJECT_CACHE */

#define OSI_IMPLEMENTS_MEM_OBJECT_CACHE 1
#define OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE 1

#include <osi/LEGACY/object_cache_types.h>

/* piggyback on the solaris API */
#include <osi/SOLARIS/object_cache.h>

#endif /* !OSI_IMPLEMENTS_MEM_OBJECT_CACHE */

#endif /* _OSI_LEGACY_MEM_OBJECT_CACHE_H */
