/*
 * Copyright 2009-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * volume group membership cache
 */

#ifndef _AFS_VOL_VG_CACHE_TYPES_H
#define _AFS_VOL_VG_CACHE_TYPES_H 1

#include "voldefs.h"

/**
 * volume group query response
 */
typedef struct VVGCache_query {
    afs_uint32 rw;                        /**< rw volume id */
    afs_uint32 children[VOL_VG_MAX_VOLS]; /**< vector of children */
} VVGCache_query_t;


#endif /* _AFS_VOL_VG_CACHE_TYPES_H */
