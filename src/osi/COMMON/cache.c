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

/*
 * cache hierarchy information interface
 */

struct osi_cache_info osi_cache_info = {
    /* provide sane defaults for each architecture */
#if defined(__osi_arch_ia32)      /* pulled from athlon MP */
      65536, 64, 2,
      65536, 64, 2,
      262144, 64, 16,
      0, 0, 0,
      64,
#elif defined(__osi_arch_amd64)   /* pulled from opteron 24x */
      65536, 64, 2,
      65536, 64, 2,
      1048576, 64, 16,
      0, 0, 0,
      64,
#elif defined(__osi_arch_sparc)   /* pulled from usIIIi+ */
      65536, 32, 4,
      65536, 32, 4,
      4194304, 64, 4,
      0, 0, 0,
      64,
#elif defined(__osi_arch_ppc64)   /* pulled from power5 */
      65536, 128, 4,
      32768, 128, 4,
      1966080, 128, 10,
      37748736, 256, 12,
      256,
#elif defined(__osi_arch_mips64)  /* pulled from R14000 */
      32768, 64, 2,
      32768, 32, 2,
      8388608, 128, 2,
      0, 0, 0,
      128,
#else
#warning "default cache parameters have not been provided for this architecture; please file a bug"
      65536, 64, 2,
      65536, 64, 2,
      262144, 64, 16,
      0, 0, 0,
      64,
#endif
      0, 0, 0, 0
};


osi_result
osi_cache_l1i_exists(void)
{
    if (!osi_cache_info.l1i_probed) {
	return OSI_FAIL;
    }
    return (osi_cache_info.l1i_size) ? OSI_CACHE_RESULT_EXISTS : OSI_CACHE_RESULT_ABSENT;
}

osi_result
osi_cache_l1d_exists(void)
{
    if (!osi_cache_info.l1d_probed) {
	return OSI_FAIL;
    }
    return (osi_cache_info.l1d_size) ? OSI_CACHE_RESULT_EXISTS : OSI_CACHE_RESULT_ABSENT;
}

osi_result
osi_cache_l2_exists(void)
{
    if (!osi_cache_info.l2_probed) {
	return OSI_FAIL;
    }
    return (osi_cache_info.l2_size) ? OSI_CACHE_RESULT_EXISTS : OSI_CACHE_RESULT_ABSENT;

}

osi_result
osi_cache_l3_exists(void)
{
    if (!osi_cache_info.l3_probed) {
	return OSI_FAIL;
    }
    return (osi_cache_info.l3_size) ? OSI_CACHE_RESULT_EXISTS : OSI_CACHE_RESULT_ABSENT;
}


osi_result
osi_cache_l1i_size(osi_size_t * val)
{
    *val = osi_cache_info.l1i_size;
    return (osi_cache_info.l1i_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l1i_line_size(osi_size_t * val)
{
    *val = osi_cache_info.l1i_line_size;
    return (osi_cache_info.l1i_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l1i_associativity(osi_size_t * val)
{
    *val = osi_cache_info.l1i_associativity;
    return (osi_cache_info.l1i_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l1d_size(osi_size_t * val)
{
    *val = osi_cache_info.l1d_size;
    return (osi_cache_info.l1d_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l1d_line_size(osi_size_t * val)
{
    *val = osi_cache_info.l1d_line_size;
    return (osi_cache_info.l1d_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l1d_associativity(osi_size_t * val)
{
    *val = osi_cache_info.l1d_associativity;
    return (osi_cache_info.l1d_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l2_size(osi_size_t * val)
{
    *val = osi_cache_info.l2_size;
    return (osi_cache_info.l2_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l2_line_size(osi_size_t * val)
{
    *val = osi_cache_info.l2_line_size;
    return (osi_cache_info.l2_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l2_associativity(osi_size_t * val)
{
    *val = osi_cache_info.l2_associativity;
    return (osi_cache_info.l2_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l3_size(osi_size_t * val)
{
    *val = osi_cache_info.l3_size;
    return (osi_cache_info.l3_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l3_line_size(osi_size_t * val)
{
    *val = osi_cache_info.l3_line_size;
    return (osi_cache_info.l3_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_l3_associativity(osi_size_t * val)
{
    *val = osi_cache_info.l3_associativity;
    return (osi_cache_info.l3_probed) ? OSI_OK : OSI_CACHE_RESULT_FAKED;
}

osi_result
osi_cache_max_alignment(osi_size_t * val)
{
    *val = osi_cache_info.max_alignment;
    return OSI_OK;
}



