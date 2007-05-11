/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UCACHE_H
#define _OSI_SOLARIS_UCACHE_H 1

/*
 * Solaris cache hierarchy introspection
 * userspace implementation
 */

/* cpu cache probing is only supported on solaris 7 and beyond */
#if defined(OSI_SUN57_ENV)
OSI_INIT_FUNC_PROTOTYPE(osi_cache_PkgInit);
#else
#define osi_cache_PkgInit      osi_null_init_func
#endif

#define osi_cache_PkgShutdown  osi_null_fini_func

#endif /* _OSI_SOLARIS_UCACHE_H */
