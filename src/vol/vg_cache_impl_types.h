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

#ifndef _AFS_VOL_VG_CACHE_IMPL_TYPES_H
#define _AFS_VOL_VG_CACHE_IMPL_TYPES_H 1

#ifndef __VOL_VG_CACHE_IMPL
#error "do not include this file outside of the volume group cache implementation"
#endif

#include "volume.h"
#include <rx/rx_queue.h>
#include <signal.h>


/**
 * volume group cache node.
 */
typedef struct VVGCache_entry {
    VolumeId rw;                        /**< rw volume id */
    VolumeId children[VOL_VG_MAX_VOLS]; /**< vector of children */
    afs_uint32 refcnt;                  /**< hash chain refcount */
} VVGCache_entry_t;

/**
 * volume group hash table.
 */
typedef struct VVGCache_hash_table {
    struct rx_queue * hash_buckets;      /**< variable-length array of
					  *   hash buckets */
} VVGCache_hash_table_t;

/**
 * volume group hash bucket.
 *
 * @see VVGCache_hash_table_t
 */
typedef struct VVGCache_hash_entry {
    struct rx_queue hash_chain;          /**< hash chain pointers */
    VolumeId volid;             /**< volume id */
    struct DiskPartition64 * dp;         /**< associated disk partition */
    VVGCache_entry_t * entry;   /**< volume group cache entry */
} VVGCache_hash_entry_t;

/* scanner implementation details */

/**
 * scan element.
 */
typedef struct VVGCache_scan_entry {
    VolumeId volid;
    VolumeId parent;
} VVGCache_scan_entry_t;

/**
 * scan table.
 */
typedef struct VVGCache_scan_table {
    unsigned int idx;

    /* stats */
    unsigned long newvols;
    unsigned long newvgs;

    VVGCache_scan_entry_t entries[VVGC_SCAN_TBL_LEN];
} VVGCache_scan_table_t;

/**
 * VVGC partition state enumeration.
 */
typedef enum VVGCache_part_state {
    VVGC_PART_STATE_VALID,      /**< vvgc data for partition is valid */
    VVGC_PART_STATE_INVALID,    /**< vvgc data for partition is known to be invalid */
    VVGC_PART_STATE_UPDATING    /**< vvgc data for partition is currently updating */
} VVGCache_part_state_t;

/**
 * entry in the 'to-delete' list.
 *
 * @see _VVGC_dlist_add_r
 */
typedef struct VVGCache_dlist_entry {
    struct rx_queue hash_chain;  /**< hash chain pointers */
    VolumeId child;              /**< child volid of the VGC entry */
    VolumeId parent;             /**< parent volid of the VGC entry */
} VVGCache_dlist_entry_t;

/**
 * VVGC partition state.
 */
typedef struct VVGCache_part {
    VVGCache_part_state_t state;  /**< state of VVGC for this partition */
    pthread_cond_t cv;            /**< state change cv */
    struct rx_queue *dlist_hash_buckets; /**< variable-length array of hash
                                          *   buckets. Queues contain
					  *   VVGCache_dlist_entry_t's.
					  *   This is NULL when we are not
					  *   scanning. */
} VVGCache_part_t;

/**
 * VVGC global state.
 */
typedef struct VVGCache {
    VVGCache_part_t part[VOLMAXPARTS+1]; /**< state of VVGC for each partition */
} VVGCache_t;

#endif /* _AFS_VOL_VG_CACHE_IMPL_TYPES_H */
