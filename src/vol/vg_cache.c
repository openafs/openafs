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

static int _VVGC_lookup(struct DiskPartition64 *,
                        VolumeId volid,
                        VVGCache_entry_t ** entry,
                        VVGCache_hash_entry_t ** hentry);
static int _VVGC_entry_alloc(VVGCache_entry_t ** entry);
static int _VVGC_entry_free(VVGCache_entry_t * entry);
static int _VVGC_entry_get(VVGCache_entry_t * entry);
static int _VVGC_entry_put(struct DiskPartition64 *,
			   VVGCache_entry_t * entry);
static int _VVGC_entry_add(struct DiskPartition64 *,
			   VolumeId volid,
			   VVGCache_entry_t **,
			   VVGCache_hash_entry_t **);
static int _VVGC_entry_cl_add(VVGCache_entry_t *, VolumeId);
static int _VVGC_entry_cl_del(struct DiskPartition64 *, VVGCache_entry_t *,
			      VolumeId);
static int _VVGC_entry_export(VVGCache_entry_t *, VVGCache_query_t *);
static int _VVGC_hash_entry_alloc(VVGCache_hash_entry_t ** entry);
static int _VVGC_hash_entry_free(VVGCache_hash_entry_t * entry);
static int _VVGC_hash_entry_add(struct DiskPartition64 *,
				VolumeId,
				VVGCache_entry_t *,
				VVGCache_hash_entry_t **);
static int _VVGC_hash_entry_del(VVGCache_hash_entry_t * entry);
static int _VVGC_hash_entry_unlink(VVGCache_hash_entry_t * entry);

VVGCache_hash_table_t VVGCache_hash_table;
VVGCache_t VVGCache;

/**
 * initialize volume group cache subsystem.
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_PkgInit(void)
{
    int code = 0;
    int i;

    /* allocate hash table */
    VVGCache_hash_table.hash_buckets =
	malloc(VolumeHashTable.Size * sizeof(struct rx_queue));
    if (VVGCache_hash_table.hash_buckets == NULL) {
	code = ENOMEM;
	goto error;
    }

    /* setup hash chain heads */
    for (i = 0; i < VolumeHashTable.Size; i++) {
	queue_Init(&VVGCache_hash_table.hash_buckets[i]);
    }

    /* initialize per-partition VVGC state */
    for (i = 0; i <= VOLMAXPARTS; i++) {
	VVGCache.part[i].state = VVGC_PART_STATE_INVALID;
	VVGCache.part[i].dlist_hash_buckets = NULL;
	CV_INIT(&VVGCache.part[i].cv, "cache part", CV_DEFAULT, 0);
	if (code) {
	    goto error;
	}
    }

 error:
    return code;
}

/**
 * shut down volume group cache subsystem.
 *
 * @return operation status
 *    @retval 0 success
 *
 * @todo implement
 */
int
VVGCache_PkgShutdown(void)
{
    int i;

    /* fix it later */

    /* free hash table */
    free(VVGCache_hash_table.hash_buckets);
    VVGCache_hash_table.hash_buckets = NULL;

    /* destroy per-partition VVGC state */
    for (i = 0; i <= VOLMAXPARTS; i++) {
	VVGCache.part[i].state = VVGC_PART_STATE_INVALID;
	CV_DESTROY(&VVGCache.part[i].cv);
    }

    return EOPNOTSUPP;
}

/**
 * allocate a cache entry.
 *
 * @param[out] entry_out  pointer to newly allocated entry
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_entry_alloc(VVGCache_entry_t ** entry_out)
{
    int code = 0;
    VVGCache_entry_t * ent;

    *entry_out = ent = malloc(sizeof(VVGCache_entry_t));
    if (ent == NULL) {
	code = ENOMEM;
	goto error;
    }

    memset(ent, 0, sizeof(*ent));

 error:
    return code;
}

/**
 * free a cache entry.
 *
 * @param[in] entry   cache entry
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_entry_free(VVGCache_entry_t * entry)
{
    int code = 0;

    osi_Assert(entry->refcnt == 0);
    free(entry);

    return code;
}

/**
 * allocate and register an entry for a volume group.
 *
 * @param[in]  dp         disk partition object
 * @param[in]  volid      volume id
 * @param[out] entry_out  vg cache object pointer
 * @param[out] hash_out   vg cache hash entry object pointer
 *
 * @pre - VOL_LOCK held
 *      - no such entry exists in hash table
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_entry_add(struct DiskPartition64 * dp,
		VolumeId volid,
		VVGCache_entry_t ** entry_out,
		VVGCache_hash_entry_t ** hash_out)
{
    int code = 0;
    VVGCache_entry_t * ent;

    code = _VVGC_entry_alloc(&ent);
    if (code) {
	goto error;
    }

    ent->rw = volid;
    /* refcnt will be inc'd when a child is added */
    ent->refcnt = 0;

    code = _VVGC_hash_entry_add(dp, volid, ent, hash_out);
    if (code) {
	goto error;
    }

    if (entry_out) {
	*entry_out = ent;
    }
    return code;

 error:
    if (ent) {
	_VVGC_entry_free(ent);
	ent = NULL;
    }
    return code;
}

/**
 * add a volid to the entry's child list.
 *
 * @param[in]   ent     volume group object
 * @param[in]   volid   volume id
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 child table is full
 *
 * @internal
 */
static int
_VVGC_entry_cl_add(VVGCache_entry_t * ent,
		   VolumeId volid)
{
    int code = 0, i;
    int empty_idx = -1;

    /* search table to avoid duplicates */
    for (i = 0; i < VOL_VG_MAX_VOLS; i++) {
	if (ent->children[i] == volid) {
	    ViceLog(1, ("VVGC_entry_cl_add: tried to add duplicate vol "
	               "%lu to VG %lu\n",
		       afs_printable_uint32_lu(volid),
		       afs_printable_uint32_lu(ent->rw)));
	    goto done;
	}
	if (empty_idx == -1 && !ent->children[i]) {
	    empty_idx = i;
	    /* don't break; make sure we go through all children so we don't
	     * add a duplicate entry */
	}
    }

    /* verify table isn't full */
    if (empty_idx == -1) {
	code = -1;
	ViceLog(0, ("VVGC_entry_cl_add: tried to add vol %lu to VG %lu, but VG "
	    "is full\n", afs_printable_uint32_lu(volid),
	    afs_printable_uint32_lu(ent->rw)));
	goto done;
    }

    /* add entry */
    ent->children[empty_idx] = volid;

    /* inc refcount */
    code = _VVGC_entry_get(ent);

 done:
    return code;
}

/**
 * delete a volid from the entry's child list.
 *
 * @param[in]   dp      disk partition object
 * @param[in]   ent     volume group object
 * @param[in]   volid   volume id
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 no such entry found
 *
 * @internal
 */
static int
_VVGC_entry_cl_del(struct DiskPartition64 *dp,
		   VVGCache_entry_t * ent,
		   VolumeId volid)
{
    int code = -1, i;

    for (i = 0; i < VOL_VG_MAX_VOLS; i++) {
	if (ent->children[i] == volid) {
	    ent->children[i] = 0;
	    code = 0;
	    goto done;
	}
    }

 done:
    if (!code) {
	code = _VVGC_entry_put(dp, ent);
    }

    return code;
}

/**
 * add a refcount to an entry.
 *
 * @param[in] entry   cache entry
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int _VVGC_entry_get(VVGCache_entry_t * entry)
{
    entry->refcnt++;
    return 0;
}

/**
 * put back a reference to an entry.
 *
 * @param[in] dp      disk partition object
 * @param[in] entry   cache entry
 *
 * @pre VOL_LOCK held
 *
 * @warning do not attempt to deref pointer after calling this interface
 *
 * @return operation status
 *    @retval 0 success
 *
 * @note dp is needed to lookup the RW hash entry to unlink, if we are
 *       putting back the final reference and freeing
 *
 * @internal
 */
static int
_VVGC_entry_put(struct DiskPartition64 * dp, VVGCache_entry_t * entry)
{
    int code = 0;

    osi_Assert(entry->refcnt > 0);

    if (--entry->refcnt == 0) {
	VVGCache_entry_t *nentry;
	VVGCache_hash_entry_t *hentry;

	/* first, try to delete the RW id hash entry pointing to this
	 * entry */
	code = _VVGC_lookup(dp, entry->rw, &nentry, &hentry);
	if (!code) {
	    if (nentry != entry) {
		/* looking up the rw of this entry points to a different
		 * entry; should not happen */
		ViceLog(0, ("VVGC_entry_put: error: entry lookup for entry %lu "
		    "found different entry than was passed",
		    afs_printable_uint32_lu(entry->rw)));
		code = -1;
	    } else {
		code = _VVGC_hash_entry_unlink(hentry);
		hentry = NULL;
	    }
	} else if (code == ENOENT) {
	    /* ignore ENOENT; this shouldn't happen, since the RW hash
	     * entry should always exist if the entry does... but we
	     * were going to delete it anyway, so try to continue */
	    ViceLog(0, ("VVGC_entry_put: warning: tried to unlink entry for "
	        "vol %lu, but RW hash entry doesn't exist; continuing "
		"anyway...\n", afs_printable_uint32_lu(entry->rw)));

	    code = 0;
	}

	/* now, just free the entry itself */
	if (!code) {
	    code = _VVGC_entry_free(entry);
	}
    }

    return code;
}

/**
 * export a volume group entry in the external object format.
 *
 * @param[in]  ent   internal-format volume group object
 * @param[out] qry   external-format volume group object
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_entry_export(VVGCache_entry_t * ent, VVGCache_query_t * qry)
{
    int i;

    qry->rw = ent->rw;
    for (i = 0; i < VOL_VG_MAX_VOLS; i++) {
	qry->children[i] = ent->children[i];
    }

    return 0;
}

/**
 * allocate a hash table entry structure.
 *
 * @param[out] entry_out  address in which to store newly allocated hash entry struct
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_hash_entry_alloc(VVGCache_hash_entry_t ** entry_out)
{
    int code = 0;
    VVGCache_hash_entry_t * ent;

    *entry_out = ent = malloc(sizeof(VVGCache_hash_entry_t));
    if (ent == NULL) {
	code = ENOMEM;
    }

    return code;
}

/**
 * free a hash table entry structure.
 *
 * @param[in] entry   hash table entry structure to be freed
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_hash_entry_free(VVGCache_hash_entry_t * entry)
{
    int code = 0;

    free(entry);

    return code;
}

/**
 * add an entry to the hash table.
 *
 * @param[in]  dp        disk partition object
 * @param[in]  volid     volume id
 * @param[in]  ent       volume group object
 * @param[out] hash_out  address in which to store pointer to hash entry
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *    @retval EEXIST hash entry for volid already exists, and it points to
 *                   a different VG entry
 *
 * @internal
 */
static int
_VVGC_hash_entry_add(struct DiskPartition64 * dp,
		     VolumeId volid,
		     VVGCache_entry_t * ent,
		     VVGCache_hash_entry_t ** hash_out)
{
    int code = 0;
    VVGCache_hash_entry_t * hent;
    int hash = VVGC_HASH(volid);
    VVGCache_entry_t *nent;

    code = _VVGC_lookup(dp, volid, &nent, hash_out);
    if (!code) {
	if (ent != nent) {
	    ViceLog(0, ("_VVGC_hash_entry_add: tried to add a duplicate "
	                " nonmatching entry for vol %lu: original "
	                "(%"AFS_PTR_FMT",%lu) new (%"AFS_PTR_FMT",%lu)\n",
	                afs_printable_uint32_lu(volid),
	                nent, afs_printable_uint32_lu(nent->rw),
	                ent, afs_printable_uint32_lu(ent->rw)));
	    return EEXIST;
	}
	ViceLog(1, ("_VVGC_hash_entry_add: tried to add duplicate "
	              "hash entry for vol %lu, VG %lu",
	              afs_printable_uint32_lu(volid),
	              afs_printable_uint32_lu(ent->rw)));
	/* accept attempts to add matching duplicate entries; just
	 * pretend we added it */
	return 0;
    }

    code = _VVGC_hash_entry_alloc(&hent);
    if (code) {
	goto done;
    }

    hent->entry = ent;
    hent->dp    = dp;
    hent->volid = volid;
    queue_Append(&VVGCache_hash_table.hash_buckets[hash],
		 hent);

 done:
    if (hash_out) {
	*hash_out = hent;
    }
    return code;
}

/**
 * remove an entry from the hash table.
 *
 * @param[in] hent   hash table entry
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_hash_entry_del(VVGCache_hash_entry_t * hent)
{
    int code = 0, res;
    int rw = 0;

    if (hent->entry->rw == hent->volid) {
	rw = 1;
    }

    code = _VVGC_entry_cl_del(hent->dp, hent->entry, hent->volid);
    /* note: hent->entry is possibly NULL after _VVGC_entry_cl_del, and
     * if hent->entry->rw == hent->volid, it is possible for hent to
     * have been freed */

    if (!rw) {
	/* If we are the RW id, don't unlink, since we still need the
	 * hash entry to exist, so when we lookup children, they can
	 * look up the RW id hash chain, and they will all go to the
	 * same object.
	 *
	 * If we are the last entry and the entry should be deleted,
	 * _VVGC_entry_cl_del will take care of unlinking the RW hash entry.
	 */
	res = _VVGC_hash_entry_unlink(hent);
	if (res) {
	    code = res;
	}
    }

    return code;
}

/**
 * low-level interface to remove an entry from the hash table.
 *
 * Does not alter the refcount or worry about the children lists or
 * anything like that; just removes the hash table entry, frees it, and
 * that's all. You probably want @see _VVGC_hash_entry_del instead.
 *
 * @param[in] hent   hash table entry
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_VVGC_hash_entry_unlink(VVGCache_hash_entry_t * hent)
{
    int code;

    queue_Remove(hent);
    hent->entry = NULL;
    hent->volid = 0;
    code = _VVGC_hash_entry_free(hent);

    return code;
}

/**
 * lookup a vg cache entry given any member volume id.
 *
 * @param[in]  dp           disk partition object
 * @param[in]  volid        vg member volume id
 * @param[out] entry_out    address in which to store volume group entry structure pointer
 * @param[out] hash_out     address in which to store hash entry pointer
 *
 * @pre VOL_LOCK held
 *
 * @warning - it is up to the caller to get a ref to entry_out, if needed
 *          - hash_out must not be referenced after dropping VOL_LOCK
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOENT volume id not found
 *    @retval EINVAL partition's VGC is invalid
 *
 * @internal
 */
static int
_VVGC_lookup(struct DiskPartition64 * dp,
               VolumeId volid,
               VVGCache_entry_t ** entry_out,
               VVGCache_hash_entry_t ** hash_out)
{
    int code = ENOENT;
    int bucket = VVGC_HASH(volid);
    struct VVGCache_hash_entry * ent, * nent;

    if (VVGCache.part[dp->index].state == VVGC_PART_STATE_INVALID) {
	return EINVAL;
    }

    *entry_out = NULL;

    for (queue_Scan(&VVGCache_hash_table.hash_buckets[bucket],
		    ent,
		    nent,
		    VVGCache_hash_entry)) {
	if (ent->volid == volid && ent->dp == dp) {
	    code = 0;
	    *entry_out = ent->entry;
	    if (hash_out) {
		*hash_out = ent;
	    }
	    break;
	}
    }

    return code;
}

/**
 * add an entry to the volume group cache.
 *
 * @param[in] dp       disk partition object
 * @param[in] parent   parent volume id
 * @param[in] child    child volume id
 * @param[out] newvg   if non-NULL, *newvg is 1 if adding this added a
 *                     new VG, 0 if we added to an existing VG
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 parent and child are already registered in
 *               different VGs
 */
int
VVGCache_entry_add_r(struct DiskPartition64 * dp,
		     VolumeId parent,
		     VolumeId child,
		     afs_int32 *newvg)
{
    int code = 0, res;
    VVGCache_entry_t * child_ent, * parent_ent;

    if (newvg) {
	*newvg = 0;
    }

    /* check for existing entries */
    res = _VVGC_lookup(dp, child, &child_ent, NULL);
    if (res && res != ENOENT) {
	code = res;
	goto done;
    }

    res = _VVGC_lookup(dp, parent, &parent_ent, NULL);
    if (res && res != ENOENT) {
	code = res;
	goto done;
    }

    /*
     * branch based upon existence of parent and child nodes
     */
    if (parent_ent && child_ent) {
	/* both exist.  we're done.
	 * if they point different places, then report the error. */
	if (child_ent != parent_ent) {
	    code = -1;
	}
	if (parent == child) {
	    /* if we're adding the RW entry as a child, the RW id may
	     * not be in the child array yet, so make sure not to skip
	     * over that */
	    goto cladd;
	}
	goto done;
    } else if (!parent_ent && child_ent) {
	/* child exists.
	 * update vg root volid, and add hash entry. */
	parent_ent = child_ent;
	parent_ent->rw = parent;

	code = _VVGC_hash_entry_add(dp,
				    parent,
				    parent_ent,
				    NULL);
	goto done;
    } else if (!child_ent && !parent_ent) {
	code = _VVGC_entry_add(dp,
			       parent,
			       &parent_ent,
			       NULL);
	if (code) {
	    goto done;
	}
	if (newvg) {
	    *newvg = 1;
	}
	if (child == parent) {
	    /* if we're the RW, skip over adding the child hash entry;
	     * we already added the hash entry when creating the entry */
	    child_ent = parent_ent;
	    goto cladd;
	}
    }

    osi_Assert(!child_ent);
    child_ent = parent_ent;
    code = _VVGC_hash_entry_add(dp,
				child,
				child_ent,
				NULL);
    if (code) {
	goto done;
    }

 cladd:
    code = _VVGC_entry_cl_add(child_ent, child);

 done:
    if (code && code != EINVAL) {
	ViceLog(0, ("VVGCache_entry_add: error %d trying to add vol %lu to VG"
	    " %lu on partition %s", code, afs_printable_uint32_lu(child),
	    afs_printable_uint32_lu(parent), VPartitionPath(dp)));
    }

    if (code == 0 && VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING) {
	/* we successfully added the entry; make sure it's not on the
	 * to-delete list, so it doesn't get deleted later */
	code = _VVGC_dlist_del_r(dp, parent, child);
	if (code && code != ENOENT) {
	    ViceLog(0, ("VVGCache_entry_add: error %d trying to remove vol "
	                "%lu (parent %lu) from the to-delete list for part "
	                "%s.\n", code, afs_printable_uint32_lu(child),
	                afs_printable_uint32_lu(parent),
	                VPartitionPath(dp)));
	} else {
	    code = 0;
	}
    }

    return code;
}

/**
 * add an entry to the volume group cache.
 *
 * @param[in] dp       disk partition object
 * @param[in] parent   parent volume id
 * @param[in] child    child volume id
 * @param[out] newvg   if non-NULL, *newvg is 1 if adding this added a
 *                     new VG, 0 if we added to an existing VG
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_entry_add(struct DiskPartition64 * dp,
		   VolumeId parent,
		   VolumeId child,
		   afs_int32 *newvg)
{
    int code = 0;

    VOL_LOCK;
    VVGCache_entry_add_r(dp, parent, child, newvg);
    VOL_UNLOCK;

    return code;
}

/**
 * delete an entry from the volume group cache.
 *
 * If partition is scanning, actually puts the entry on a list of entries
 * to delete when the scan is done.
 *
 * @param[in] dp       disk partition object
 * @param[in] parent   parent volume id
 * @param[in] child    child volume id
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_entry_del_r(struct DiskPartition64 * dp,
		     VolumeId parent, VolumeId child)
{
    if (VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING) {
	int code;
	code = _VVGC_dlist_add_r(dp, parent, child);
	if (code) {
	    return code;
	}
    }
    return _VVGC_entry_purge_r(dp, parent, child);
}

/**
 * delete an entry from the volume group cache.
 *
 * @param[in] dp       disk partition object
 * @param[in] parent   parent volume id
 * @param[in] child    child volume id
 *
 * @pre VOL_LOCK held
 *
 * @internal
 *
 * @return operation status
 *    @retval 0 success
 */
int
_VVGC_entry_purge_r(struct DiskPartition64 * dp,
                    VolumeId parent, VolumeId child)
{
    int code = 0, res;
    VVGCache_entry_t * parent_ent, * child_ent;
    VVGCache_hash_entry_t * child_hent;

    /* check mappings for each volid */
    res = _VVGC_lookup(dp, parent, &parent_ent, NULL);
    if (res) {
	code = res;
	goto done;
    }
    res = _VVGC_lookup(dp, child, &child_ent, &child_hent);
    if (res) {
	code = res;
	goto done;
    }

    /* if the mappings don't match, we have a serious error */
    if (parent_ent != child_ent) {
	ViceLog(0, ("VVGCache_entry_del: trying to delete vol %lu from VG %lu, "
	    "but vol %lu points to VGC entry %"AFS_PTR_FMT" and VG %lu "
	    "points to VGC entry %"AFS_PTR_FMT"\n",
	    afs_printable_uint32_lu(child),
	    afs_printable_uint32_lu(parent),
	    afs_printable_uint32_lu(child),
	    child_ent, afs_printable_uint32_lu(parent), parent_ent));
	code = -1;
	goto done;
    }

    code = _VVGC_hash_entry_del(child_hent);

 done:
    return code;
}

/**
 * delete an entry from the volume group cache.
 *
 * @param[in] dp       disk partition object
 * @param[in] parent   parent volume id
 * @param[in] child    child volume id
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_entry_del(struct DiskPartition64 * dp,
		   VolumeId parent, VolumeId child)
{
    int code;

    VOL_LOCK;
    code = VVGCache_entry_del_r(dp, parent, child);
    VOL_UNLOCK;

    return code;
}

/**
 * query a volume group by any member volume id.
 *
 * @param[in]  dp        disk partition object
 * @param[in]  volume    volume id of a member of VG
 * @param[out] res       vg membership data
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *    @retval EAGAIN partition needs to finish scanning
 */
int
VVGCache_query_r(struct DiskPartition64 * dp,
		 VolumeId volume,
		 VVGCache_query_t * res)
{
    int code = 0;
    VVGCache_entry_t * ent;

    /* If cache for this partition doesn't exist; start a scan */
    if (VVGCache.part[dp->index].state == VVGC_PART_STATE_INVALID) {
	code = VVGCache_scanStart_r(dp);
	if (code == 0 || code == -3) {
	    /* -3 means another thread already started scanning */
	    return EAGAIN;
	}
	return code;
    }
    if (VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING) {
	return EAGAIN;
    }

    code = _VVGC_lookup(dp, volume, &ent, NULL);
    if (!code) {
	code = _VVGC_entry_export(ent, res);
    }

    return code;
}

/**
 * query a volume group by any member volume id.
 *
 * @param[in]  dp        disk partition object
 * @param[in]  volume    volume id of a member of VG
 * @param[out] res       vg membership data
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_query(struct DiskPartition64 * dp,
	       VolumeId volume, VVGCache_query_t * res)
{
    int code;

    VOL_LOCK;
    code = VVGCache_query_r(dp, volume, res);
    VOL_UNLOCK;

    return code;
}

/**
 * begin asynchronous scan of on-disk volume group metadata.
 *
 * @param[in] dp       disk partition object
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_scanStart_r(struct DiskPartition64 * dp)
{
    int code = 0, res;

    if (dp) {
	code = _VVGC_scan_start(dp);
    } else {
	/* start a scanner thread on each partition */
	for (dp = DiskPartitionList; dp; dp = dp->next) {
	    res = _VVGC_scan_start(dp);
	    if (res) {
		code = res;
	    }
	}
    }

    return code;
}

/**
 * begin asynchronous scan of on-disk volume group metadata.
 *
 * @param[in] dp       disk partition object
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_scanStart(struct DiskPartition64 * dp)
{
    int code;

    VOL_LOCK;
    code = VVGCache_scanStart_r(dp);
    VOL_UNLOCK;

    return code;
}

/**
 * wait for async on-disk VG metadata scan to complete.
 *
 * @param[in] dp       disk partition object
 *
 * @pre VOL_LOCK held
 *
 * @warning this routine must drop VOL_LOCK internally
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_scanWait_r(struct DiskPartition64 * dp)
{
    int code = 0;

    while (VVGCache.part[dp->index].state == VVGC_PART_STATE_UPDATING) {
	VOL_CV_WAIT(&VVGCache.part[dp->index].cv);
    }

    return code;
}

/**
 * wait for async on-disk VG metadata scan to complete.
 *
 * @param[in] dp       disk partition object
 *
 * @return operation status
 *    @retval 0 success
 */
int
VVGCache_scanWait(struct DiskPartition64 * dp)
{
    int code;

    VOL_LOCK;
    code = VVGCache_scanWait_r(dp);
    VOL_UNLOCK;

    return code;
}

/**
 * flush all cache entries for a given disk partition.
 *
 * @param[in] part  disk partition object
 *
 * @pre VOL_LOCK held
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
int
_VVGC_flush_part_r(struct DiskPartition64 * part)
{
    int code = 0, res;
    int i;
    VVGCache_hash_entry_t * ent, * nent;

    for (i = 0; i < VolumeHashTable.Size; i++) {
	for (queue_Scan(&VVGCache_hash_table.hash_buckets[i],
			ent,
			nent,
			VVGCache_hash_entry)) {
	    if (ent->dp == part) {
		VolumeId volid = ent->volid;
		res = _VVGC_hash_entry_del(ent);
		if (res) {
		    ViceLog(0, ("_VVGC_flush_part_r: error %d deleting hash entry for %lu\n",
		        res, afs_printable_uint32_lu(volid)));
		    code = res;
		}
	    }
	}
    }

    return code;
}

/**
 * flush all cache entries for a given disk partition.
 *
 * @param[in] part  disk partition object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
int
_VVGC_flush_part(struct DiskPartition64 * part)
{
    int code;

    VOL_LOCK;
    code = _VVGC_flush_part_r(part);
    VOL_UNLOCK;

    return code;
}


/**
 * change VVGC partition state.
 *
 * @param[in]  part    disk partition object
 * @param[in]  state   new state
 *
 * @pre VOL_LOCK is held
 *
 * @return old state
 *
 * @internal
 */
int
_VVGC_state_change(struct DiskPartition64 * part,
		   VVGCache_part_state_t state)
{
    VVGCache_part_state_t old_state;

    old_state = VVGCache.part[part->index].state;
    VVGCache.part[part->index].state = state;

    if (old_state != state) {
	CV_BROADCAST(&VVGCache.part[part->index].cv);
    }

    return old_state;
}

#endif /* AFS_DEMAND_ATTACH_FS */
