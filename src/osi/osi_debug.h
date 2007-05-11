/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_DEBUG_H
#define _OSI_OSI_DEBUG_H 1


/*
 * osi abstraction
 * debug build environment related defines
 *
 * defines:
 *
 *  OSI_DEBUG_ALL
 *    -- turn on all osi debug features
 *
 *  OSI_DEBUG_ASSERT
 *    -- make osi_AssertDebug() work
 *       (in optimized code it elides away to nothing)
 *
 *  OSI_DEBUG_INLINE
 *    -- turn all osi_inline_define() style inlines
 *       into real functions
 *
 *  OSI_DEBUG_MEM_OBJECT_CACHE
 *    -- turn on mem_object_cache leak detection
 *
 *  OSI_DEBUG_LIB_INIT
 *    -- turn on osi_lib_init debug messages
 *
 *  OSI_DEBUG_LIB_FINI
 *    -- turn on osi_lib_fini debug messages
 *
 *  OSI_DEBUG_VECTOR
 *    -- turn on osi_vector debugging
 *
 *  OSI_DEBUG_HEAP
 *    -- turn on osi_heap debugging
 *
 *  OSI_DEBUG_TRACE_ALL
 *    -- turn on all debug trace points
 *
 *  OSI_DEBUG_TRACE_OSI
 *    -- turn on all osi debug trace points
 *
 *  OSI_DEBUG_TRACE_MEM
 *    -- turn on debug trace points in osi_mem
 *
 */

#if defined(OSI_DEBUG_ALL)
#define OSI_DEBUG_ASSERT 1
#define OSI_DEBUG_INLINE 1
#define OSI_DEBUG_MEM_OBJECT_CACHE 1
#define OSI_DEBUG_LIB_INIT 1
#define OSI_DEBUG_LIB_FINI 1
#define OSI_DEBUG_VECTOR 1
#define OSI_DEBUG_HEAP 1
#define OSI_DEBUG_TRACE_ALL 1
#endif /* OSI_DEBUG_ALL */

#if defined(OSI_DEBUG_TRACE_ALL)
#define OSI_DEBUG_TRACE_OSI 1
#endif

#if defined(OSI_DEBUG_TRACE_OSI)
#define OSI_DEBUG_TRACE_MEM 1
#endif

/*
 * make sure OSI_ENV_DEBUG is defined when appropriate
 */
#if defined(OSI_DEBUG_ASSERT) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_INLINE) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_MEM_OBJECT_CACHE) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_LIB_INIT) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_LIB_FINI) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_VECTOR) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_HEAP) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif
#if defined(OSI_DEBUG_TRACE_MEM) && !defined(OSI_ENV_DEBUG)
#define OSI_ENV_DEBUG 1
#endif

/*
 * setup other deps
 */
#if defined(OSI_DEBUG_VECTOR) && !defined(OSI_DEBUG_ASSERT)
#define OSI_DEBUG_ASSERT 1
#endif

#endif /* _OSI_OSI_DEBUG_H */
