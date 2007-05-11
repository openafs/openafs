/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_cpu.h>

/*
 * support for probing the cache hierarchy parameters
 */


#if defined(__sparc)


osi_static osi_result
osi_cache_probe(void)
{
    /*
     * these kernel symbols contain what we're interested in:
     */
    extern int ecache_size;
    extern int ecache_alignsize;
    extern int ecache_associativity;
    extern int dcache_size;
    extern int dcache_linesize;
    extern int icache_size;
    extern int icache_linesize;

    osi_cache_info.l2_size = ecache_size;
    osi_cache_info.l2_line_size = ecache_alignsize;
    osi_cache_info.l2_associativity = ecache_associativity;

    osi_cache_info.l1d_size = dcache_size;
    osi_cache_info.l1d_line_size = dcache_linesize;
    osi_cache_info.l1d_associativity = 1;

    osi_cache_info.l1i_size = icache_size;
    osi_cache_info.l1i_line_size = icache_linesize;
    osi_cache_info.l1i_associativity = 1;

    osi_cache_info.l1i_probed = osi_cache_info.l1d_probed = osi_cache_info.l2_probed = 1;

    return OSI_OK;
}

#elif defined(__x86)

/* uh, just punt for now.  getl2cacheinfo() from <sys/x86_archext.h> looks promising... */

osi_static osi_result
osi_cache_probe(void)
{
    return OSI_CACHE_RESULT_FAIL_PROBE;
}

#else /* !__sparc && !__x86 */
#error "this solaris port is not yet supported; please file a bug"
#endif /* !__sparc && !__x86 */


OSI_INIT_FUNC_DECL(osi_cache_PkgInit)
{
    return osi_cache_probe();
}

