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

#ifndef _AFS_VOL_VG_CACHE_IMPL_H
#define _AFS_VOL_VG_CACHE_IMPL_H 1

#define VVGC_SCAN_TBL_LEN 4096  /**< thread-local partition scan table size */

#include "vg_cache_impl_types.h"

extern VVGCache_hash_table_t VVGCache_hash_table;
extern VVGCache_t VVGCache;

extern int _VVGC_flush_part(struct DiskPartition64 * part);
extern int _VVGC_flush_part_r(struct DiskPartition64 * part);
extern int _VVGC_scan_start(struct DiskPartition64 * dp);
extern int _VVGC_state_change(struct DiskPartition64 * part,
			      VVGCache_part_state_t state);
extern int _VVGC_entry_purge_r(struct DiskPartition64 * dp,
                               VolumeId parent, VolumeId child);
extern int _VVGC_dlist_add_r(struct DiskPartition64 *dp,
                             VolumeId parent, VolumeId child);
extern int _VVGC_dlist_del_r(struct DiskPartition64 *dp,
                             VolumeId parent, VolumeId child);

#define VVGC_HASH(volumeId) (volumeId&(VolumeHashTable.Mask))

#endif /* _AFS_VOL_VG_CACHE_H */
