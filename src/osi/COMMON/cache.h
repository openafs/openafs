/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_CACHE_H
#define _OSI_COMMON_CACHE_H 1


struct osi_cache_info {
    osi_size_t l1i_size, l1i_line_size, l1i_associativity;
    osi_size_t l1d_size, l1d_line_size, l1d_associativity;
    osi_size_t l2_size,  l2_line_size,  l2_associativity;
    osi_size_t l3_size,  l3_line_size,  l3_associativity;
    osi_size_t max_alignment;
    int l1i_probed, l1d_probed, l2_probed, l3_probed;
};
osi_extern struct osi_cache_info osi_cache_info;


/*
 * for platforms where probing the cache size isn't possible,
 * (or we haven't implemented the necessary osi interfaces yet)
 * punt and return some made up but reasonable values
 */

osi_extern osi_result osi_cache_l1i_exists(void);
osi_extern osi_result osi_cache_l1d_exists(void);
osi_extern osi_result osi_cache_l2_exists(void);
osi_extern osi_result osi_cache_l3_exists(void);

osi_extern osi_result osi_cache_l1i_size(osi_size_t *);
osi_extern osi_result osi_cache_l1i_line_size(osi_size_t *);
osi_extern osi_result osi_cache_l1i_associativity(osi_size_t *);

osi_extern osi_result osi_cache_l1d_size(osi_size_t *);
osi_extern osi_result osi_cache_l1d_line_size(osi_size_t *);
osi_extern osi_result osi_cache_l1d_associativity(osi_size_t *);

osi_extern osi_result osi_cache_l2_size(osi_size_t *);
osi_extern osi_result osi_cache_l2_line_size(osi_size_t *);
osi_extern osi_result osi_cache_l2_associativity(osi_size_t *);

osi_extern osi_result osi_cache_l3_size(osi_size_t *);
osi_extern osi_result osi_cache_l3_line_size(osi_size_t *);
osi_extern osi_result osi_cache_l3_associativity(osi_size_t *);

osi_extern osi_result osi_cache_max_alignment(osi_size_t *);


#endif /* _OSI_COMMON_CACHE_H */
