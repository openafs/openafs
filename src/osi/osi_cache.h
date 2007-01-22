/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_CACHE_H
#define _OSI_OSI_CACHE_H 1


/*
 * platform-independent osi_cache API
 *
 *
 * the returns from these functions will be:
 *  OSI_CACHE_RESULT_EXISTS -- cache exists
 *  OSI_CACHE_RESULT_ABSENT -- cache does not exist
 *  OSI_FAIL -- failed to probe for cache
 *
 *  osi_result osi_cache_l1i_exists(void);
 *    -- probe for the existence of an l1 instruction cache
 *
 *  osi_result osi_cache_l1d_exists(void);
 *    -- probe for the existence of an l1 data cache
 *
 *  osi_result osi_cache_l2_exists(void);
 *    -- probe for the existence of an l2 cache
 *
 *  osi_result osi_cache_l3_exists(void);
 *    -- probe for the existence of an l3 cache
 *
 * please note that on certain platforms these interfaces
 * will simply return default values because real values
 * cannot be determined.  on such platforms the result
 * code will be the success code OSI_CACHE_RESULT_FAKED.
 *
 *  osi_result osi_cache_l1i_size(size_t *)
 *    -- returns the size of the l1 instruction cache
 *
 *  osi_result osi_cache_l1i_line_size(size_t *);
 *    -- returns the line size of the l1 instruction cache
 *
 *  osi_result osi_cache_l1i_associativity(size_t *);
 *    -- returns the set associativity of the l1 instruction cache
 *
 *  osi_result osi_cache_l1d_size(size_t *);
 *    -- returns the size of the l1 data cache
 *
 *  osi_result osi_cache_l1d_line_size(size_t *);
 *    -- returns the line size of the l1 data cache
 *
 *  osi_result osi_cache_l1d_associativity(size_t *);
 *    -- returns the set associativity of the l1 data cache
 *
 *  osi_result osi_cache_l2_size(size_t *);
 *    -- returns the size of the l2 cache
 *
 *  osi_result osi_cache_l2_line_size(size_t *);
 *    -- returns the line size of the l2 cache
 *
 *  osi_result osi_cache_l2_associativity(size_t *);
 *    -- returns the set associativity of the l2 cache
 *
 *  osi_result osi_cache_l3_size(size_t *);
 *    -- returns the size of the l3 cache
 *
 *  osi_result osi_cache_l3_line_size(size_t *);
 *    -- returns the line size of the l3 cache
 *
 *  osi_result osi_cache_l3_associativity(size_t *);
 *    -- returns the set associativity of the l3 cache
 *
 *  osi_result osi_cache_max_alignment(size_t *);
 *    -- returns the max alignment needed for the cache hierarchy
 */


#define OSI_CACHE_RESULT_FAKED       OSI_RESULT_SUCCESS_CODE(OSI_CACHE_ERROR_FAKED)
#define OSI_CACHE_RESULT_EXISTS      OSI_RESULT_SUCCESS_CODE(OSI_CACHE_ERROR_EXISTS)
#define OSI_CACHE_RESULT_ABSENT      OSI_RESULT_SUCCESS_CODE(OSI_CACHE_ERROR_ABSENT)
#define OSI_CACHE_RESULT_FAIL_PROBE  OSI_RESULT_SUCCESS_CODE(OSI_CACHE_ERROR_FAIL_PROBE)


osi_extern osi_result osi_cache_PkgInit(void);
osi_extern osi_result osi_cache_PkgShutdown(void);


/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#include <osi/COMMON/cache.h>

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kcache.h>
#else
#define osi_cache_PkgInit()         (OSI_CACHE_RESULT_FAKED)
#define osi_cache_PkgShutdown()     (OSI_CACHE_RESULT_FAKED)
#endif

#else /* !OSI_KERNELSPACE_ENV */

#include <osi/COMMON/cache.h>

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/ucache.h>
#else
#define osi_cache_PkgInit()         (OSI_CACHE_RESULT_FAKED)
#define osi_cache_PkgShutdown()     (OSI_CACHE_RESULT_FAKED)
#endif

#endif /* !OSI_KERNELSPACE_ENV */


#endif /* _OSI_OSI_CACHE_H */
