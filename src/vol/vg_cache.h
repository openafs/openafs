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

#ifndef _AFS_VOL_VG_CACHE_H
#define _AFS_VOL_VG_CACHE_H 1

#include "vg_cache_types.h"
#include "partition.h"

extern int VVGCache_entry_add(struct DiskPartition64 *, VolumeId parent,
			      VolumeId child, afs_int32 *newvg);
extern int VVGCache_entry_add_r(struct DiskPartition64 *, VolumeId parent,
				VolumeId child, afs_int32 *newvg);
extern int VVGCache_entry_del(struct DiskPartition64 *, VolumeId parent, VolumeId child);
extern int VVGCache_entry_del_r(struct DiskPartition64 *, VolumeId parent, VolumeId child);
extern int VVGCache_query(struct DiskPartition64 *, VolumeId volume, VVGCache_query_t * res);
extern int VVGCache_query_r(struct DiskPartition64 *, VolumeId volume, VVGCache_query_t * res);

extern int VVGCache_scanStart(struct DiskPartition64 *);
extern int VVGCache_scanStart_r(struct DiskPartition64 *);
extern int VVGCache_scanWait(struct DiskPartition64 *);
extern int VVGCache_scanWait_r(struct DiskPartition64 *);
extern int VVGCache_checkPartition_r(struct DiskPartition64 *);

extern int VVGCache_PkgInit(void);
extern int VVGCache_PkgShutdown(void);


#endif /* _AFS_VOL_VG_CACHE_H */
