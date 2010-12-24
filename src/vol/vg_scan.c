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
 * asynchronous partition scanner
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef AFS_DEMAND_ATTACH_FS

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <afs/afs_assert.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <sys/file.h>
#include <sys/param.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX_ENV)
#include <unistd.h>
#endif
#endif /* AFS_NT40_ENV */
#include <lock.h>
#include <afs/afsutil.h>
#include <lwp.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "voldefs.h"
#include "partition.h"
#include <afs/errors.h>

#define __VOL_VG_CACHE_IMPL 1

#include "vg_cache.h"
#include "vg_cache_impl.h"

#ifdef O_LARGEFILE
#define afs_open	open64
#else /* !O_LARGEFILE */
#define afs_open	open
#endif /* !O_LARGEFILE */

static int _VVGC_scan_table_init(VVGCache_scan_table_t * tbl);
static int _VVGC_scan_table_add(VVGCache_scan_table_t * tbl,
				struct DiskPartition64 * dp,
				VolumeId volid,
				VolumeId parent);
static int _VVGC_scan_table_flush(VVGCache_scan_table_t * tbl,
				  struct DiskPartition64 * dp);
static void * _VVGC_scanner_thread(void *);
static int _VVGC_scan_partition(struct DiskPartition64 * part);
static VVGCache_dlist_entry_t * _VVGC_dlist_lookup_r(struct DiskPartition64 *dp,
                                                     VolumeId parent,
                                                     VolumeId child);
static void _VVGC_flush_dlist(struct DiskPartition64 *dp);

/**
 * init a thread-local scan table.
 *
 * @param[in] tbl  scan table
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_scan_table_init(VVGCache_scan_table_t * tbl)
{
    memset(tbl, 0, sizeof(*tbl));

    return 0;
}

/**
 * add an entry to the thread-local scan table.
 *
 * @param[in] tbl     scan table
 * @param[in] dp      disk partition object
 * @param[in] volid   volume id
 * @param[in] parent  parent volume id
 *
 * @pre VOL_LOCK is NOT held
 *
 * @note if the table is full, this routine will acquire
 *       VOL_LOCK and flush the table to the global one.
 *
 * @return operation status
 *    @retval 0 success
 *    @retval nonzero a VVGCache_entry_add_r operation failed during a
 *                    flush of the thread-local table
 *
 * @internal
 */
static int
_VVGC_scan_table_add(VVGCache_scan_table_t * tbl,
		     struct DiskPartition64 * dp,
		     VolumeId volid,
		     VolumeId parent)
{
    int code = 0;

    if (tbl->idx == VVGC_SCAN_TBL_LEN) {
	code = _VVGC_scan_table_flush(tbl, dp);
    }

    tbl->entries[tbl->idx].volid = volid;
    tbl->entries[tbl->idx].parent = parent;
    tbl->idx++;

    return code;
}

/**
 * flush thread-local scan table to the global VG cache.
 *
 * @param[in] tbl     scan table
 * @param[in] dp      disk partition object
 *
 * @pre VOL_LOCK is NOT held
 *
 * @return operation status
 *    @retval 0 success
 *    @retval nonzero a VVGCache_entry_add_r operation failed during a
 *                    flush of the thread-local table
 *
 * @internal
 */
static int
_VVGC_scan_table_flush(VVGCache_scan_table_t * tbl,
		       struct DiskPartition64 * dp)
{
    int code = 0, res, i;
    afs_int32 newvg = 0;
    unsigned long newvols, newvgs;

    newvols = tbl->newvols;
    newvgs = tbl->newvgs;

    VOL_LOCK;

    for (i = 0; i < tbl->idx; i++) {
	/*
	 * We need to check the 'to-delete' list and prevent adding any entries
	 * that are on it. The volser could potentially create a volume in one
	 * VG, then delete it and put it on another VG. If we are doing a scan
	 * when that happens, tbl->entries could have the entries for trying to
	 * put the vol on both VGs, though at least one of them will also be on
	 * the dlist.  If we put everything in tbl->entries on the VGC then try
	 * to delete afterwards, putting one entry on the VGC cause an error,
	 * and we'll fail to add it. So instead, avoid adding any new VGC
	 * entries if it is on the dlist.
	 */
	if (_VVGC_dlist_lookup_r(dp, tbl->entries[i].parent,
			         tbl->entries[i].volid)) {
	    continue;
	}
	res = VVGCache_entry_add_r(dp,
				   tbl->entries[i].parent,
				   tbl->entries[i].volid,
				   &newvg);
	if (res) {
	    code = res;
	} else {
	    newvols++;
	    newvgs += newvg;
	}
    }

    /* flush the to-delete list while we're here. We don't need to preserve
     * the list across the entire scan, and flushing it each time we flush
     * a scan table will keep the size of the dlist down */
    _VVGC_flush_dlist(dp);

    VOL_UNLOCK;

    ViceLog(125, ("VVGC_scan_table_flush: flushed %d entries from "
                  "scan table to global VG cache\n", tbl->idx));
    ViceLog(125, ("VVGC_scan_table_flush: %s total: %lu vols, %lu groups\n",
                  VPartitionPath(dp), newvols, newvgs));

    res = _VVGC_scan_table_init(tbl);
    if (res) {
	code = res;
    }

    tbl->newvols = newvols;
    tbl->newvgs = newvgs;

    return code;
}

/**
 * record a volume header found by VWalkVolumeHeaders in a VGC scan table.
 *
 * @param[in] dp   the disk partition
 * @param[in] name full path to the .vol header (unused)
 * @param[in] hdr  the header data
 * @param[in] last whether this is the last try or not (unused)
 * @param[in] rock actually a VVGCache_scan_table_t* to add the volume to
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 fatal error adding vol to the scan table
 */
static int
_VVGC_RecordHeader(struct DiskPartition64 *dp, const char *name,
                   struct VolumeDiskHeader *hdr, int last, void *rock)
{
    int code;
    VVGCache_scan_table_t *tbl;
    tbl = (VVGCache_scan_table_t *)rock;

    code = _VVGC_scan_table_add(tbl, dp, hdr->id, hdr->parent);
    if (code) {
	ViceLog(0, ("VVGC_scan_partition: error %d adding volume %s to scan table\n",
	             code, name));
	return -1;
    }
    return 0;
}

/**
 * unlink a faulty volume header found by VWalkVolumeHeaders.
 *
 * @param[in] dp   the disk partition (unused)
 * @param[in] name the full path to the .vol header
 * @param[in] hdr  the header data (unused)
 * @param[in] rock unused
 */
static void
_VVGC_UnlinkHeader(struct DiskPartition64 *dp, const char *name,
                   struct VolumeDiskHeader *hdr, void *rock)
{
    ViceLog(0, ("%s is not a legitimate volume header file; deleted\n", name));
    if (unlink(name)) {
	ViceLog(0, ("Unable to unlink %s (errno = %d)\n",
	            name, errno));
    }
}

/**
 * scan a disk partition for .vol files
 *
 * @param[in] part   disk partition object
 *
 * @pre VOL_LOCK is NOT held
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 invalid disk partition object
 *    @retval -2 failed to flush stale entries for this partition
 *
 * @internal
 */
static int
_VVGC_scan_partition(struct DiskPartition64 * part)
{
    int code, res;
    DIR *dirp = NULL;
    VVGCache_scan_table_t tbl;
    char *part_path = NULL;

    code = _VVGC_scan_table_init(&tbl);
    if (code) {
	ViceLog(0, ("VVGC_scan_partition: could not init scan table; error = %d\n",
	    code));
	goto done;
    }
    part_path = VPartitionPath(part);
    if (part_path == NULL) {
	ViceLog(0, ("VVGC_scan_partition: invalid partition object given; aborting scan\n"));
	code = -1;
	goto done;
    }

    VOL_LOCK;
    res = _VVGC_flush_part_r(part);
    if (res) {
	ViceLog(0, ("VVGC_scan_partition: error flushing partition %s; error = %d\n",
	    VPartitionPath(part), res));
	code = -2;
    }
    VOL_UNLOCK;
    if (code) {
	goto done;
    }

    dirp = opendir(part_path);
    if (dirp == NULL) {
	ViceLog(0, ("VVGC_scan_partition: could not open %s, aborting scan; error = %d\n",
	    part_path, errno));
	code = -1;
	goto done;
    }

    ViceLog(5, ("VVGC_scan_partition: scanning partition %s for VG cache\n",
                 part_path));

    code = VWalkVolumeHeaders(part, part_path, _VVGC_RecordHeader,
                              _VVGC_UnlinkHeader, &tbl);
    if (code < 0) {
	goto done;
    }

    _VVGC_scan_table_flush(&tbl, part);

 done:
    if (dirp) {
	closedir(dirp);
	dirp = NULL;
    }
    if (code) {
	ViceLog(0, ("VVGC_scan_partition: error %d while scanning %s\n",
	            code, part_path));
    } else {
	ViceLog(0, ("VVGC_scan_partition: finished scanning %s: %lu volumes in %lu groups\n",
	             part_path, tbl.newvols, tbl.newvgs));
    }

    VOL_LOCK;

    _VVGC_flush_dlist(part);
    free(VVGCache.part[part->index].dlist_hash_buckets);
    VVGCache.part[part->index].dlist_hash_buckets = NULL;

    if (code) {
	_VVGC_state_change(part, VVGC_PART_STATE_INVALID);
    } else {
	_VVGC_state_change(part, VVGC_PART_STATE_VALID);
    }

    VOL_UNLOCK;

    return code;
}

/**
 * scanner thread.
 */
static void *
_VVGC_scanner_thread(void * args)
{
    struct DiskPartition64 *part = args;
    int code;

    code = _VVGC_scan_partition(part);
    if (code) {
	ViceLog(0, ("Error: _VVGC_scan_partition failed with code %d for partition %s\n",
	    code, VPartitionPath(part)));
    }

    return NULL;
}

/**
 * start a background scan.
 *
 * @param[in] dp  disk partition object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 internal error
 *    @retval -3 racing against another thread
 *
 * @internal
 */
int
_VVGC_scan_start(struct DiskPartition64 * dp)
{
    int code = 0;
    pthread_t tid;
    pthread_attr_t attrs;
    int i;

    if (_VVGC_state_change(dp,
			   VVGC_PART_STATE_UPDATING)
	== VVGC_PART_STATE_UPDATING) {
	/* race */
	ViceLog(0, ("VVGC_scan_partition: race detected; aborting scanning partition %s\n",
	            VPartitionPath(dp)));
	code = -3;
	goto error;
    }

    /* initialize partition's to-delete list */
    VVGCache.part[dp->index].dlist_hash_buckets =
	malloc(VolumeHashTable.Size * sizeof(struct rx_queue));
    if (!VVGCache.part[dp->index].dlist_hash_buckets) {
	code = -1;
	goto error;
    }
    for (i = 0; i < VolumeHashTable.Size; i++) {
	queue_Init(&VVGCache.part[dp->index].dlist_hash_buckets[i]);
    }

    code = pthread_attr_init(&attrs);
    if (code) {
	goto error;
    }

    code = pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    if (code) {
	goto error;
    }

    code = pthread_create(&tid, &attrs, &_VVGC_scanner_thread, dp);

    if (code) {
	VVGCache_part_state_t old_state;

	ViceLog(0, ("_VVGC_scan_start: pthread_create failed with %d\n", code));

	old_state = _VVGC_state_change(dp, VVGC_PART_STATE_INVALID);
	osi_Assert(old_state == VVGC_PART_STATE_UPDATING);
    }

 error:
    if (code) {
	ViceLog(0, ("_VVGC_scan_start failed with code %d for partition %s\n",
	        code, VPartitionPath(dp)));
	if (VVGCache.part[dp->index].dlist_hash_buckets) {
	    free(VVGCache.part[dp->index].dlist_hash_buckets);
	    VVGCache.part[dp->index].dlist_hash_buckets = NULL;
	}
    }

    return code;
}

/**
 * looks up an entry on the to-delete list, if it exists.
 *
 * @param[in] dp     the partition whose dlist we are looking at
 * @param[in] parent the parent volume ID we're looking for
 * @param[in] child  the child volume ID we're looking for
 *
 * @return a pointer to the entry in the dlist for that entry
 *  @retval NULL the requested entry does not exist in the dlist
 */
static VVGCache_dlist_entry_t *
_VVGC_dlist_lookup_r(struct DiskPartition64 *dp, VolumeId parent,
                     VolumeId child)
{
    int bucket = VVGC_HASH(child);
    VVGCache_dlist_entry_t *ent, *nent;

    for (queue_Scan(&VVGCache.part[dp->index].dlist_hash_buckets[bucket],
                    ent, nent,
		    VVGCache_dlist_entry)) {

	if (ent->child == child && ent->parent == parent) {
	    return ent;
	}
    }

    return NULL;
}

/**
 * delete all of the entries in the dlist from the VGC.
 *
 * Traverses the to-delete list for the specified partition, and deletes
 * the specified entries from the global VGC. Also deletes the entries from
 * the dlist itself as it goes along.
 *
 * @param[in] dp  the partition whose dlist we are flushing
 */
static void
_VVGC_flush_dlist(struct DiskPartition64 *dp)
{
    int i;
    VVGCache_dlist_entry_t *ent, *nent;

    for (i = 0; i < VolumeHashTable.Size; i++) {
	for (queue_Scan(&VVGCache.part[dp->index].dlist_hash_buckets[i],
	                ent, nent,
	                VVGCache_dlist_entry)) {

	    _VVGC_entry_purge_r(dp, ent->parent, ent->child);
	    queue_Remove(ent);
	    free(ent);
	}
    }
}

/**
 * add a VGC entry to the partition's to-delete list.
 *
 * This adds a VGC entry (a parent/child pair) to a list of VGC entries to
 * be deleted from the VGC at the end of a VGC scan. This is necessary,
 * while a VGC scan is ocurring, volumes may be deleted. Since a VGC scan
 * scans a partition in VVGC_SCAN_TBL_LEN chunks, a VGC delete operation
 * may delete a volume, only for it to be added again when the VGC scan's
 * table adds it to the VGC. So when a VGC entry is deleted and a VGC scan
 * is running, this function must be called to ensure it does not come
 * back onto the VGC.
 *
 * @param[in] dp      the partition to whose dlist we are adding
 * @param[in] parent  the parent volumeID of the VGC entry
 * @param[in] child   the child volumeID of the VGC entry
 *
 * @return operation status
 *  @retval 0 success
 *  @retval ENOMEM memory allocation error
 *
 * @pre VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING
 *
 * @internal VGC use only
 */
int
_VVGC_dlist_add_r(struct DiskPartition64 *dp, VolumeId parent,
                  VolumeId child)
{
    int bucket = VVGC_HASH(child);
    VVGCache_dlist_entry_t *entry;

    entry = malloc(sizeof(*entry));
    if (!entry) {
	return ENOMEM;
    }

    entry->child = child;
    entry->parent = parent;

    queue_Append(&VVGCache.part[dp->index].dlist_hash_buckets[bucket],
                 entry);
    return 0;
}

/**
 * delete a VGC entry from the partition's to-delete list.
 *
 * When a VGC scan is ocurring, and a volume is removed, but then created
 * again, we need to ensure that it does not get deleted from being on the
 * dlist. Call this function whenever adding a new entry to the VGC during
 * a VGC scan to ensure it doesn't get deleted later.
 *
 * @param[in] dp      the partition from whose dlist we are deleting
 * @param[in] parent  the parent volumeID of the VGC entry
 * @param[in] child   the child volumeID of the VGC entry
 *
 * @return operation status
 *  @retval 0 success
 *  @retval ENOENT the specified VGC entry is not on the dlist
 *
 * @pre VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING
 *
 * @internal VGC use only
 *
 * @see _VVGC_dlist_add_r
 */
int
_VVGC_dlist_del_r(struct DiskPartition64 *dp, VolumeId parent,
                  VolumeId child)
{
    VVGCache_dlist_entry_t *ent;

    ent = _VVGC_dlist_lookup_r(dp, parent, child);
    if (!ent) {
	return ENOENT;
    }

    queue_Remove(ent);
    free(ent);

    return 0;
}

#endif /* AFS_DEMAND_ATTACH_FS */
