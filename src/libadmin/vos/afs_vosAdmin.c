/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/libadmin/vos/afs_vosAdmin.c,v 1.9.10.2 2005/10/13 18:29:41 shadow Exp $");

#include <afs/stds.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include "afs_vosAdmin.h"
#include "../adminutil/afs_AdminInternal.h"
#include <afs/afs_utilAdmin.h>
#include <afs/vlserver.h>
#include <afs/volser.h>
#include <afs/volint.h>
#include <afs/partition.h>
#include <rx/rx.h>
#include "vosutils.h"
#include "vsprocs.h"
#include "lockprocs.h"

typedef struct file_server {
    int begin_magic;
    int is_valid;
    struct rx_connection *serv;
    int end_magic;
} file_server_t, *file_server_p;

/*
 * IsValidServerHandle - test a server handle for validity.
 *
 * PARAMETERS
 *
 * IN serverHandle - the serverHandle to be validated.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidServerHandle(file_server_p serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (serverHandle == NULL) {
	tst = ADMVOSSERVERHANDLENULL;
	goto fail_IsValidServerHandle;
    }

    if (serverHandle->is_valid != 1) {
	tst = ADMVOSSERVERHANDLEINVALID;
	goto fail_IsValidServerHandle;
    }

    if ((serverHandle->begin_magic != BEGIN_MAGIC)
	|| (serverHandle->end_magic != END_MAGIC)) {
	tst = ADMVOSSERVERHANDLEBADMAGIC;
	goto fail_IsValidServerHandle;
    }
    rc = 1;

  fail_IsValidServerHandle:

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

/*
 * IsValidCellHandle - verify that a cell handle can be used to make vos 
 * requests.
 *
 * PARAMETERS
 *
 * IN cellHandle - the cellHandle to be validated.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidCellHandle(afs_cell_handle_p cellHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (!CellHandleIsValid((void *)cellHandle, &tst)) {
	goto fail_IsValidCellHandle;
    }

    if (cellHandle->vos_valid == 0) {
	tst = ADMVOSCELLHANDLEINVALIDVOS;
	goto fail_IsValidCellHandle;
    }
    rc = 1;

  fail_IsValidCellHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/* set <server> and <part> to the correct values depending on
 * <voltype> and <entry> */
static void
GetServerAndPart(struct nvldbentry *entry, int voltype, afs_int32 * server,
		 afs_int32 * part, int *previdx)
{
    int i, istart, vtype;

    *server = -1;
    *part = -1;

    /* Doesn't check for non-existance of backup volume */
    if ((voltype == RWVOL) || (voltype == BACKVOL)) {
	vtype = ITSRWVOL;
	istart = 0;		/* seach the entire entry */
    } else {
	vtype = ITSROVOL;
	/* Seach from beginning of entry or pick up where we left off */
	istart = ((*previdx < 0) ? 0 : *previdx + 1);
    }

    for (i = istart; i < entry->nServers; i++) {
	if (entry->serverFlags[i] & vtype) {
	    *server = entry->serverNumber[i];
	    *part = entry->serverPartition[i];
	    *previdx = i;
	    return;
	}
    }

    /* Didn't find any, return -1 */
    *previdx = -1;
    return;
}

/*
 * vos_BackupVolumeCreate - create a backup volume for a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume to create the back up for.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_BackupVolumeCreate(const void *cellHandle, vos_MessageCallBack_t callBack,
		       unsigned int volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    struct nvldbentry rw_vol_entry;
    afs_int32 rw_server;
    afs_int32 rw_partition;
    afs_int32 rw_vol_type;
    struct nvldbentry bk_vol_entry;
    afs_int32 bk_server;
    afs_int32 bk_partition;
    afs_int32 bk_vol_type;
    int equal;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_BackupVolumeCreate;
    }

    /*
     * Get the volume information and verify that we've been passed
     * a read write volume id
     */

    if (!GetVolumeInfo
	(c_handle, volumeId, &rw_vol_entry, &rw_server, &rw_partition,
	 &rw_vol_type, &tst)) {
	goto fail_vos_BackupVolumeCreate;
    }

    if (rw_vol_type != RWVOL) {
	tst = ADMVOSMUSTBERWVOL;
	goto fail_vos_BackupVolumeCreate;
    }

    /*
     * Check to see that if a backup volume exists, it exists on the
     * same server as volumeId
     */

    if (rw_vol_entry.flags & BACK_EXISTS) {
	if (!GetVolumeInfo
	    (c_handle, rw_vol_entry.volumeId[BACKVOL], &bk_vol_entry,
	     &bk_server, &bk_partition, &bk_vol_type, &tst)) {
	    goto fail_vos_BackupVolumeCreate;
	}
	if (!VLDB_IsSameAddrs(c_handle, bk_server, rw_server, &equal, &tst)) {
	    goto fail_vos_BackupVolumeCreate;
	}
	if (!equal) {
	    tst = ADMVOSBACKUPVOLWRONGSERVER;
	    goto fail_vos_BackupVolumeCreate;
	}
    }

    /*
     * Create the new backup volume
     */

    rc = UV_BackupVolume(c_handle, rw_server, rw_partition, volumeId, &tst);

  fail_vos_BackupVolumeCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_BackupVolumeCreateMultiple - create backup volumes en masse.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volumes exist.
 *
 * IN serverHandle - the server where the backups are to be created.  Can be
 * null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the backups are to be created.  Can be
 * null.
 *
 * IN volumePrefix - all volumes with this prefix will have backup volumes
 * created. Can be null.
 *
 * IN excludePrefix - exclude the volumes that match volumePrefix.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_BackupVolumeCreateMultiple(const void *cellHandle,
			       const void *serverHandle,
			       vos_MessageCallBack_t callBack,
			       const unsigned int *partition,
			       const char *volumePrefix,
			       vos_exclude_t excludePrefix, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    struct VldbListByAttributes attr;
    int exclude = 0;
    int prefix = 0;
    size_t prefix_len = 0;
    nbulkentries arrayEntries;
    afs_int32 nentries = 0;
    register struct nvldbentry *entry;
    int i;
    afs_int32 rw_volid, rw_server, rw_partition;
    int previdx;
    int equal = 0;
    char backbuf[1024];

    memset((void *)&attr, 0, sizeof(attr));

    /*
     * Validate arguments
     *
     * The only required argument to this function is the cellHandle.
     * If the excludePrefix is set to VOS_EXCLUDE, volumePrefix must
     * be non-null.
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_BackupVolumeCreateMultiple;
    }

    if ((excludePrefix == VOS_EXCLUDE)
	&& ((volumePrefix == NULL) || (*volumePrefix == 0))) {
	tst = ADMVOSEXCLUDEREQUIRESPREFIX;
	goto fail_vos_BackupVolumeCreateMultiple;
    }

    if (f_server != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_BackupVolumeCreateMultiple;
	}
	attr.server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));
	attr.Mask |= VLLIST_SERVER;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_BackupVolumeCreateMultiple;
	}
	attr.partition = *partition;
	attr.Mask |= VLLIST_PARTITION;
    }

    if (excludePrefix == VOS_EXCLUDE) {
	exclude = 1;
    }

    if ((volumePrefix != NULL) && (*volumePrefix != 0)) {
	prefix = 1;
	prefix_len = strlen(volumePrefix);
    }

    memset((void *)&arrayEntries, 0, sizeof(arrayEntries));

    /*
     * Get a list of all the volumes in the cell
     */

    if (!VLDB_ListAttributes(c_handle, &attr, &nentries, &arrayEntries, &tst)) {
	goto fail_vos_BackupVolumeCreateMultiple;
    }

    /*
     * Cycle through the list of volumes and see if we should create a backup
     * for each individual volume
     */

    for (i = 0; i < nentries; i++) {
	entry = &arrayEntries.nbulkentries_val[i];

	/*
	 * Skip entries that don't have a RW volume
	 */

	if (!(entry->flags & RW_EXISTS)) {
	    if (callBack != NULL) {
		const char *messageText;
		if (util_AdminErrorCodeTranslate
		    (ADMVOSVOLUMENOREADWRITE, 0, &messageText, &tst)) {
		    sprintf(backbuf, "%s %s", messageText, entry->name);
		    (**callBack) (VOS_VERBOSE_MESSAGE, backbuf);
		}
	    }
	    continue;
	}

	/*
	 * See if we should skip this entry because of the prefix/exclude
	 * combination we've been passed
	 */

	if (prefix) {
	    if (exclude) {
		if (!strncmp(entry->name, volumePrefix, prefix_len)) {
		    continue;
		}
	    } else {
		if (strncmp(entry->name, volumePrefix, prefix_len)) {
		    continue;
		}
	    }
	}

	rw_volid = entry->volumeId[RWVOL];
	GetServerAndPart(entry, RWVOL, &rw_server, &rw_partition, &previdx);

	if ((rw_server == -1) || (rw_partition == -1)) {
	    if (callBack != NULL) {
		const char *messageText;
		if (util_AdminErrorCodeTranslate
		    (ADMVOSVLDBBADENTRY, 0, &messageText, &tst)) {
		    sprintf(backbuf, "%s %s", messageText, entry->name);
		    (**callBack) (VOS_ERROR_MESSAGE, backbuf);
		}
	    }
	    continue;
	}

	/*
	 * Check that the RW volume is on the same server that we were
	 * passed
	 */

	if (serverHandle != NULL) {
	    if (!VLDB_IsSameAddrs
		(c_handle, ntohl(rx_HostOf(rx_PeerOf(f_server->serv))),
		 rw_server, &equal, &tst)) {
		if (callBack != NULL) {
		    const char *messageText;
		    if (util_AdminErrorCodeTranslate
			(ADMVOSVLDBBADSERVER, 0, &messageText, &tst)) {
			sprintf(backbuf, "%s %x %d", messageText,
				ntohl(rx_HostOf(rx_PeerOf(f_server->serv))),
				tst);
			(**callBack) (VOS_ERROR_MESSAGE, backbuf);
		    }
		}
		continue;
	    }
	    if (!equal) {
		if (callBack != NULL) {
		    const char *messageText;
		    if (util_AdminErrorCodeTranslate
			(ADMVOSVLDBDIFFERENTADDR, 0, &messageText, &tst)) {
			sprintf(backbuf, "%s %s", messageText, entry->name);
			(**callBack) (VOS_ERROR_MESSAGE, backbuf);
		    }
		}
		continue;
	    }
	}

	/*
	 * Check that the RW volume is on the same partition we were
	 * passed
	 */

	if (partition != NULL) {
	    if (*partition != rw_partition) {
		continue;
	    }
	}

	/*
	 * Backup the volume
	 */

	rc = UV_BackupVolume(c_handle, rw_server, rw_partition, rw_volid,
			     &tst);
    }

  fail_vos_BackupVolumeCreateMultiple:

    if (arrayEntries.nbulkentries_val) {
	free(arrayEntries.nbulkentries_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionGet - get information about a single partition.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server lives.
 *
 * IN serverHandle - a previously open vos server handle that holds
 * the partition of interest.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the integer that represents the partition of interest.
 *
 * OUT partitionP - a pointer to a vos_partitionEntry_t that upon successful 
 * completion contains information regarding the partition.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionGet(const void *cellHandle, const void *serverHandle,
		 vos_MessageCallBack_t callBack, unsigned int partition,
		 vos_partitionEntry_p partitionP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct diskPartition part_info;
    file_server_p f_server = (file_server_p) serverHandle;
    char partitionName[10];	/* this rpc requires a character partition name */

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_PartitionGet;
    }

    if (partitionP == NULL) {
	tst = ADMVOSPARTITIONPNULL;
	goto fail_vos_PartitionGet;
    }

    if (!vos_PartitionIdToName(partition, partitionName, &tst)) {
	goto fail_vos_PartitionGet;
    }

    tst = AFSVolPartitionInfo(f_server->serv, partitionName, &part_info);
    if (tst) {
	goto fail_vos_PartitionGet;
    }
    strcpy(partitionP->name, part_info.name);
    strcpy(partitionP->deviceName, part_info.devName);
    partitionP->lockFileDescriptor = part_info.lock_fd;
    partitionP->totalSpace = part_info.minFree;
    partitionP->totalFreeSpace = part_info.free;
    rc = 1;

  fail_vos_PartitionGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the partition retrieval functions.
 */

typedef struct partition_get {
    afs_int32 total_received;	/* the total number of valid partiions retrieved */
    int number_processed;	/* the number of valid paritions we've handed out */
    int index;			/* the current index into the part_list array */
    struct partList part_list;	/* the list of partitions */
    vos_partitionEntry_t partition[CACHED_ITEMS];	/* the cache of partitions */
    const void *server;		/* the server where the parititions exist */
} partition_get_t, *partition_get_p;

static int
GetPartitionInfoRPC(void *rpc_specific, int slot, int *last_item,
		    int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    partition_get_p part = (partition_get_p) rpc_specific;
    vos_partitionEntry_p ptr = (vos_partitionEntry_p) & part->partition[slot];

    /*
     * Skip partition entries that are not valid
     */

    while (!(part->part_list.partFlags[part->index] & PARTVALID)) {
	part->index++;
    }

    /*
     * Get information for the next partition
     */

    if (!vos_PartitionGet
	(0, part->server, 0,
	 (unsigned int)part->part_list.partId[part->index], ptr, &tst)) {
	goto fail_GetPartitionInfoRPC;
    }

    part->index++;
    part->number_processed++;

    if (part->number_processed == part->total_received) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

  fail_GetPartitionInfoRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetPartitionInfoFromCache(void *rpc_specific, int slot, void *dest,
			  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    partition_get_p part = (partition_get_p) rpc_specific;

    memcpy(dest, (const void *)&part->partition[slot],
	   sizeof(vos_partitionEntry_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionGetBegin - begin to iterate over the partitions at a 
 * particular server.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server exists.
 *
 * IN serverHandle - the server that houses the partitions of interest.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * OUT iterationIdP - upon successful completion, contains an iterator that can
 * be passed to vos_PartitionGetNext.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionGetBegin(const void *cellHandle, const void *serverHandle,
		      vos_MessageCallBack_t callBack, void **iterationIdP,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    partition_get_p part =
	(partition_get_p) calloc(1, sizeof(partition_get_t));

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_PartitionGetBegin;
    }

    if (iterationIdP == NULL) {
	goto fail_vos_PartitionGetBegin;
    }

    if ((iter == NULL) || (part == NULL)) {
	tst = ADMNOMEM;
	goto fail_vos_PartitionGetBegin;
    }

    /*
     * Fill in the part structure
     */

    part->server = serverHandle;
    if (!UV_ListPartitions
	(f_server->serv, &part->part_list, &part->total_received, &tst)) {
	goto fail_vos_PartitionGetBegin;
    }

    /*
     * If we didn't receive any partitions, don't spawn a background thread.
     * Mark the iterator complete.
     */

    if (part->total_received == 0) {
	if (!IteratorInit(iter, (void *)part, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_vos_PartitionGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	if (!IteratorInit
	    (iter, (void *)part, GetPartitionInfoRPC,
	     GetPartitionInfoFromCache, NULL, NULL, &tst)) {
	    goto fail_vos_PartitionGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_vos_PartitionGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (part != NULL) {
	    free(part);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionGetNext - get the next partition at a server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by vos_PartitionGetBegin
 *
 * OUT partitionP - a pointer to a vos_partitionEntry_t that upon successful
 * completion contains the next partition.
 *
 * LOCKS
 * 
 * The iterator is locked while the next parition is retrieved.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionGetNext(const void *iterationId, vos_partitionEntry_p partitionP,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_PartitionGetNext;
    }

    if (partitionP == NULL) {
	tst = ADMVOSPARTITIONPNULL;
	goto fail_vos_PartitionGetNext;
    }

    rc = IteratorNext(iter, (void *)partitionP, &tst);

  fail_vos_PartitionGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionGetDone - finish using a partition iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by vos_PartitionGetBegin
 *
 * LOCKS
 * 
 * The iterator is locked and then destroyed.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_PartitionGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_vos_PartitionGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerOpen - open a handle to an individual server for future
 * operations
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server lives.
 *
 * IN serverName - the machine name of the server
 *
 * OUT serverHandleP - a void pointer that upon successful completion 
 * contains a handle that is used in future operations upon the server.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerOpen(const void *cellHandle, const char *serverName,
	       void **serverHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) malloc(sizeof(file_server_t));
    int server_address;
    struct rx_securityClass *sc[3];
    int scIndex;

    if (f_server == NULL) {
	tst = ADMNOMEM;
	goto fail_vos_ServerOpen;
    }

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_ServerOpen;
    }

    if (!c_handle->tokens->afs_token_set) {
	tst = ADMVOSCELLHANDLENOAFSTOKENS;
	goto fail_vos_ServerOpen;
    }

    if (!util_AdminServerAddressGetFromName
	(serverName, &server_address, &tst)) {
	goto fail_vos_ServerOpen;
    }

    scIndex = c_handle->tokens->sc_index;
    sc[scIndex] = c_handle->tokens->afs_sc[scIndex];
    f_server->serv =
	rx_GetCachedConnection(htonl(server_address),
			       htons(AFSCONF_VOLUMEPORT), VOLSERVICE_ID,
			       sc[scIndex], scIndex);
    if (f_server->serv != NULL) {
	f_server->begin_magic = BEGIN_MAGIC;
	f_server->end_magic = END_MAGIC;
	f_server->is_valid = 1;
	*serverHandleP = (void *)f_server;
	rc = 1;
    } else {
	tst = ADMVOSSERVERNOCONNECTION;
	goto fail_vos_ServerOpen;
    }

  fail_vos_ServerOpen:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerClose - close a handle previously obtained from vos_ServerOpen
 *
 * PARAMETERS
 *
 * IN serverHandle - an existing server handle.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerClose(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_ServerClose;
    }

    rx_ReleaseCachedConnection(f_server->serv);
    f_server->is_valid = 0;
    free(f_server);
    rc = 1;

  fail_vos_ServerClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerSync - synchronize the vldb and the fileserver at a particular
 * server.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server lives.
 *
 * IN serverHandle - a handle to the server machine.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition to synchronize.  Can be NULL.
 *
 * IN force - force deletion of bad volumes.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerSync(const void *cellHandle, const void *serverHandle,
	       vos_MessageCallBack_t callBack, const unsigned int *partition,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_int32 part = 0;
    int flags = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_ServerSync;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_ServerSync;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_ServerSync;
	}
	part = (afs_int32) * partition;
	flags = 1;
    }

    /*
     * sync the server
     */

    rc = UV_SyncServer(c_handle, f_server->serv, part, flags, &tst);

  fail_vos_ServerSync:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_FileServerAddressChange - change an existing file server address.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the address should be changed.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN oldAddress - the old server address in host byte order
 *
 * IN newAddress - the new server address in host byte order
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_FileServerAddressChange(const void *cellHandle,
			    vos_MessageCallBack_t callBack, int oldAddress,
			    int newAddress, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_FileServerAddressChange;
    }

    tst =
	ubik_Call_New(VL_ChangeAddr, c_handle->vos, 0, oldAddress,
		      newAddress);
    if (tst) {
	goto fail_vos_FileServerAddressChange;
    }
    rc = 1;

  fail_vos_FileServerAddressChange:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_FileServerAddressRemove - remove an existing file server address.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the address should be removed.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN serverAddress - the server address to remove in host byte order.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_FileServerAddressRemove(const void *cellHandle,
			    vos_MessageCallBack_t callBack, int serverAddress,
			    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int dummyAddress = 0xffffffff;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_FileServerAddressRemove;
    }

    tst =
	ubik_Call_New(VL_ChangeAddr, c_handle->vos, 0, dummyAddress,
		      serverAddress);
    if (tst) {
	goto fail_vos_FileServerAddressRemove;
    }
    rc = 1;

  fail_vos_FileServerAddressRemove:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the server retrieval functions.
 *
 * These functions are very similar to the FileServerAddressGet
 * functions.  The main difference being that instead of returning
 * a single address at a time for a server, we fill an array with
 * all the addresses of a server.
 */

typedef struct server_get {
    struct ubik_client *vldb;	/* connection for future rpc's if neccessary */
    afs_int32 total_addresses;	/* total number of addresses */
    bulkaddrs addresses;	/* the list of addresses */
    int address_index;		/* current index into address list */
    vos_fileServerEntry_t server[CACHED_ITEMS];	/* the cache of servers */
} server_get_t, *server_get_p;

static int
GetServerRPC(void *rpc_specific, int slot, int *last_item,
	     int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    server_get_p serv = (server_get_p) rpc_specific;
    afs_uint32 *addrP = &serv->addresses.bulkaddrs_val[serv->address_index];
    afs_int32 base, index;
    afsUUID m_uuid;
    afs_int32 m_unique;
    ListAddrByAttributes m_attrs;
    afs_int32 total_multi;
    bulkaddrs addr_multi;
    int i;

    /*
     * Check to see if this is a multihomed address server
     */

    if (((*addrP & 0xff000000) == 0xff000000) && ((*addrP) & 0xffff)) {
	base = (*addrP >> 16) & 0xff;
	index = (*addrP) & 0xffff;

	if ((base >= 0) && (base <= VL_MAX_ADDREXTBLKS) && (index >= 1)
	    && (index <= VL_MHSRV_PERBLK)) {

	    /*
	     * This is a multihomed server.  Make an rpc to retrieve
	     * all its addresses.  Copy the addresses into the cache.
	     */

	    m_attrs.Mask = VLADDR_INDEX;
	    m_attrs.index = (base * VL_MHSRV_PERBLK) + index;
	    total_multi = 0;
	    addr_multi.bulkaddrs_val = 0;
	    addr_multi.bulkaddrs_len = 0;
	    tst =
		ubik_Call(VL_GetAddrsU, serv->vldb, 0, &m_attrs, &m_uuid,
			  &m_unique, &total_multi, &addr_multi);
	    if (tst) {
		goto fail_GetServerRPC;
	    }

	    /*
	     * Remove any bogus IP addresses which the user may have
	     * been unable to remove.
	     */

	    RemoveBadAddresses(&total_multi, &addr_multi);

	    /*
	     * Copy all the addresses into the cache
	     */

	    for (i = 0; i < total_multi; i++) {
		serv->server[slot].serverAddress[i] =
		    addr_multi.bulkaddrs_val[i];
	    }

	    serv->server[slot].count = total_multi;
	    serv->address_index++;
	    free(addr_multi.bulkaddrs_val);
	}

	/*
	 * The next address is just a plain old address
	 */

    } else {
	serv->server[slot].serverAddress[0] = *addrP;
	serv->server[slot].count = 1;
	serv->address_index++;
    }

    /*
     * See if we've processed all the entries
     */


    if (serv->address_index == serv->total_addresses) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

  fail_GetServerRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetServerFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    server_get_p serv = (server_get_p) rpc_specific;

    memcpy(dest, (const void *)&serv->server[slot],
	   sizeof(vos_fileServerEntry_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
DestroyServer(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    server_get_p serv = (server_get_p) rpc_specific;

    if (serv->addresses.bulkaddrs_val != NULL) {
	free(serv->addresses.bulkaddrs_val);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_FileServerGetBegin - begin to iterate over the file servers in a cell.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the file servers exist.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * OUT iterationIdP - upon successful completion, contains an iterator that
 * can be passed to vos_FileServerGetNext.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_FileServerGetBegin(const void *cellHandle, vos_MessageCallBack_t callBack,
		       void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    server_get_p serv = (server_get_p) calloc(1, sizeof(server_get_t));
    struct VLCallBack unused;


    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_FileServerGetBegin;
    }

    if (iterationIdP == NULL) {
	goto fail_vos_FileServerGetBegin;
    }

    if ((iter == NULL) || (serv == NULL)) {
	tst = ADMNOMEM;
	goto fail_vos_FileServerGetBegin;
    }

    /*
     * Fill in the serv structure
     */

    serv->vldb = c_handle->vos;
    tst =
	ubik_Call_New(VL_GetAddrs, c_handle->vos, 0, 0, 0, &unused,
		      &serv->total_addresses, &serv->addresses);

    if (tst) {
	goto fail_vos_FileServerGetBegin;
    }

    /*
     * Remove any bogus IP addresses which the user may have
     * been unable to remove.
     */

    RemoveBadAddresses(&serv->total_addresses, &serv->addresses);

    if (serv->total_addresses == 0) {
	if (!IteratorInit(iter, (void *)serv, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_vos_FileServerGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	if (!IteratorInit
	    (iter, (void *)serv, GetServerRPC, GetServerFromCache, NULL,
	     DestroyServer, &tst)) {
	    goto fail_vos_FileServerGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_vos_FileServerGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (serv != NULL) {
	    if (serv->addresses.bulkaddrs_val != NULL) {
		free(serv->addresses.bulkaddrs_val);
	    }
	    free(serv);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_FileServerGetNext - get information about the next fileserver in the cell.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * vos_FileServerGetBegin
 *
 * OUT serverEntryP - a pointer to a vos_fileServerEntry_t that upon successful
 * completion contains information about the next server in the cell.
 *
 * LOCKS
 * 
 * The iterator is locked while the next server is retrieved.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_FileServerGetNext(void *iterationId, vos_fileServerEntry_p serverEntryP,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_FileServerGetNext;
    }

    if (serverEntryP == NULL) {
	tst = ADMVOSSERVERENTRYPNULL;
	goto fail_vos_FileServerGetNext;
    }

    rc = IteratorNext(iter, (void *)serverEntryP, &tst);

  fail_vos_FileServerGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_FileServerGetDone - finish using a partition iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by vos_FileServerGetBegin
 *
 * LOCKS
 * 
 * The iterator is locked and then destroyed.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_FileServerGetDone(void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_FileServerGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_vos_FileServerGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the transation retrieval functions.
 */

typedef struct transaction_get {
    afs_int32 total;		/* total number of transactions */
    afs_int32 index;		/* index to the current transaction */
    transDebugInfo *cur;	/* the current transaction */
    vos_serverTransactionStatus_t tran[CACHED_ITEMS];	/* the cache of trans */
} transaction_get_t, *transaction_get_p;

static int
GetTransactionRPC(void *rpc_specific, int slot, int *last_item,
		  int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    transaction_get_p t = (transaction_get_p) rpc_specific;
    int index = t->index;

    /*
     * Copy the next transaction into the cache
     */

    t->tran[slot].transactionId = t->cur[index].tid;
    t->tran[slot].lastActiveTime = t->cur[index].time;
    t->tran[slot].creationTime = t->cur[index].creationTime;
    t->tran[slot].errorCode = t->cur[index].returnCode;
    t->tran[slot].volumeId = t->cur[index].volid;
    t->tran[slot].partition = t->cur[index].partition;
    strcpy(t->tran[slot].lastProcedureName, t->cur[index].lastProcName);
    t->tran[slot].nextReceivePacketSequenceNumber = t->cur[index].readNext;
    t->tran[slot].nextSendPacketSequenceNumber = t->cur[index].transmitNext;
    t->tran[slot].lastReceiveTime = t->cur[index].lastReceiveTime;
    t->tran[slot].lastSendTime = t->cur[index].lastSendTime;

    t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_OK;

    switch (t->cur[index].iflags) {
    case ITOffline:
	t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_OFFLINE;
	break;
    case ITBusy:
	t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_BUSY;
	break;
    case ITReadOnly:
	t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_READONLY;
	break;
    case ITCreate:
	t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_CREATE;
	break;
    case ITCreateVolID:
	t->tran[slot].volumeAttachMode = VOS_VOLUME_ATTACH_MODE_CREATE_VOLID;
	break;
    }

    t->tran[slot].volumeActiveStatus = VOS_VOLUME_ACTIVE_STATUS_OK;

    switch (t->cur[index].vflags) {
    case VTDeleteOnSalvage:
	t->tran[slot].volumeActiveStatus =
	    VOS_VOLUME_ACTIVE_STATUS_DELETE_ON_SALVAGE;
	break;
    case VTOutOfService:
	t->tran[slot].volumeActiveStatus =
	    VOS_VOLUME_ACTIVE_STATUS_OUT_OF_SERVICE;
	break;
    case VTDeleted:
	t->tran[slot].volumeActiveStatus = VOS_VOLUME_ACTIVE_STATUS_DELETED;
	break;
    }

    t->tran[slot].volumeTransactionStatus = VOS_VOLUME_TRANSACTION_STATUS_OK;

    if (t->cur[index].tflags) {
	t->tran[slot].volumeTransactionStatus =
	    VOS_VOLUME_TRANSACTION_STATUS_DELETED;
    }
    t->index++;


    /*
     * See if we've processed all the entries
     */


    if (t->index == t->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetTransactionFromCache(void *rpc_specific, int slot, void *dest,
			afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    transaction_get_p tran = (transaction_get_p) rpc_specific;

    memcpy(dest, (const void *)&tran->tran[slot],
	   sizeof(vos_serverTransactionStatus_p));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
DestroyTransaction(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    transaction_get_p tran = (transaction_get_p) rpc_specific;

    if (tran->cur != NULL) {
	free(tran->cur);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerTransactionStatusGetBegin - begin to iterate over the transactions
 * at a volume server.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume server exists.
 *
 * IN serverHandle - a handle to the server to query.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * OUT iterationIdP - upon successful completion, contains an iterator that
 * can be passed to vos_ServerTransactionStatusGetNext.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerTransactionStatusGetBegin(const void *cellHandle,
				    const void *serverHandle,
				    vos_MessageCallBack_t callBack,
				    void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    transaction_get_p tran =
	(transaction_get_p) calloc(1, sizeof(transaction_get_t));


    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_ServerTransactionStatusGetBegin;
    }

    if (iterationIdP == NULL) {
	goto fail_vos_ServerTransactionStatusGetBegin;
    }

    if ((iter == NULL) || (tran == NULL)) {
	tst = ADMNOMEM;
	goto fail_vos_ServerTransactionStatusGetBegin;
    }

    /*
     * Fill in the tran structure
     */

    if (!UV_VolserStatus(f_server->serv, &tran->cur, &tran->total, &tst)) {
	goto fail_vos_ServerTransactionStatusGetBegin;
    }

    if (tran->total == 0) {
	if (!IteratorInit(iter, (void *)tran, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_vos_ServerTransactionStatusGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	if (!IteratorInit
	    (iter, (void *)tran, GetTransactionRPC, GetTransactionFromCache,
	     NULL, DestroyTransaction, &tst)) {
	    goto fail_vos_ServerTransactionStatusGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_vos_ServerTransactionStatusGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (tran != NULL) {
	    if (tran->cur != NULL) {
		free(tran->cur);
	    }
	    free(tran);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerTransactionStatusGetNext - get information about the next 
 * active transaction.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * vos_ServerTransactionStatusGetBegin
 *
 * OUT serverTransactionStatusP - a pointer to a vos_serverTransactionStatus_p
 * that upon successful completion contains information about the
 * next transaction.
 *
 * LOCKS
 * 
 * The iterator is locked while the next item is retrieved.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerTransactionStatusGetNext(const void *iterationId,
				   vos_serverTransactionStatus_p
				   serverTransactionStatusP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_ServerTransactionStatusGetNext;
    }

    if (serverTransactionStatusP == NULL) {
	tst = ADMVOSSERVERTRANSACTIONSTATUSPNULL;
	goto fail_vos_ServerTransactionStatusGetNext;
    }

    rc = IteratorNext(iter, (void *)serverTransactionStatusP, &tst);

  fail_vos_ServerTransactionStatusGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_ServerTransactionStatusGetDone - finish using a transaction iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * vos_ServerTransactionStatusGetBegin
 *
 * LOCKS
 * 
 * The iterator is locked and then destroyed.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_ServerTransactionStatusGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_ServerTransactionStatusGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_vos_ServerTransactionStatusGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
copyVLDBEntry(struct nvldbentry *source, vos_vldbEntry_p dest,
	      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int i;

    dest->numServers = source->nServers;
    for (i = 0; i < VOS_MAX_VOLUME_TYPES; i++) {
	dest->volumeId[i] = source->volumeId[i];
    }
    dest->cloneId = source->cloneId;
    dest->status = VOS_VLDB_ENTRY_OK;
    if (source->flags & VLOP_ALLOPERS) {
	dest->status |= VOS_VLDB_ENTRY_LOCKED;
    } else {
	if (source->flags & VLOP_MOVE) {
	    dest->status |= VOS_VLDB_ENTRY_MOVE;
	}
	if (source->flags & VLOP_RELEASE) {
	    dest->status |= VOS_VLDB_ENTRY_RELEASE;
	}
	if (source->flags & VLOP_BACKUP) {
	    dest->status |= VOS_VLDB_ENTRY_BACKUP;
	}
	if (source->flags & VLOP_DELETE) {
	    dest->status |= VOS_VLDB_ENTRY_DELETE;
	}
	if (source->flags & VLOP_DUMP) {
	    dest->status |= VOS_VLDB_ENTRY_DUMP;
	}
    }
    if (source->flags & VLF_RWEXISTS) {
	dest->status |= VOS_VLDB_ENTRY_RWEXISTS;
    }
    if (source->flags & VLF_ROEXISTS) {
	dest->status |= VOS_VLDB_ENTRY_ROEXISTS;
    }
    if (source->flags & VLF_BACKEXISTS) {
	dest->status |= VOS_VLDB_ENTRY_BACKEXISTS;
    }

    strcpy(dest->name, source->name);
    for (i = 0; i < VOS_MAX_REPLICA_SITES; i++) {
	dest->volumeSites[i].serverAddress = source->serverNumber[i];
	dest->volumeSites[i].serverPartition = source->serverPartition[i];
	dest->volumeSites[i].serverFlags = 0;

	if (source->serverFlags[i] & NEW_REPSITE) {
	    dest->volumeSites[i].serverFlags |= VOS_VLDB_NEW_REPSITE;
	}
	if (source->serverFlags[i] & ITSROVOL) {
	    dest->volumeSites[i].serverFlags |= VOS_VLDB_READ_ONLY;
	}
	if (source->serverFlags[i] & ITSRWVOL) {
	    dest->volumeSites[i].serverFlags |= VOS_VLDB_READ_WRITE;
	}
	if (source->serverFlags[i] & ITSBACKVOL) {
	    dest->volumeSites[i].serverFlags |= VOS_VLDB_BACKUP;
	}
	if (source->serverFlags[i] & RO_DONTUSE) {
	    dest->volumeSites[i].serverFlags |= VOS_VLDB_DONT_USE;
	}

    }

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBGet- get a volume's vldb entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume entries exist.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the id of the volume to retrieve.
 *
 * IN volumeName - the name of the volume to retrieve.
 *
 * OUT vldbEntry - upon successful completion, contains the information regarding
 * the volume.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBGet(const void *cellHandle, vos_MessageCallBack_t callBack,
	    const unsigned int *volumeId, const char *volumeName,
	    vos_vldbEntry_p vldbEntry, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    struct nvldbentry entry;


    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBGet;
    }

    if (vldbEntry == NULL) {
	tst = ADMVOSVLDBENTRYNULL;
	goto fail_vos_VLDBGet;
    }

    if (((volumeName == NULL) || (*volumeName == 0)) && (volumeId == NULL)) {
	tst = ADMVOSVOLUMENAMEANDVOLUMEIDNULL;
	goto fail_vos_VLDBGet;
    }

    /*
     * Retrieve the entry
     */

    if (!((volumeName == NULL) || (*volumeName == 0))) {
	if (!ValidateVolumeName(volumeName, &tst)) {
	    goto fail_vos_VLDBGet;
	}
	if (!VLDB_GetEntryByName(c_handle, volumeName, &entry, &tst)) {
	    goto fail_vos_VLDBGet;
	}
    } else {
	if (!VLDB_GetEntryByID(c_handle, *volumeId, -1, &entry, &tst)) {
	    goto fail_vos_VLDBGet;
	}
    }

    /*
     * Copy the entry into our structure
     */

    if (!copyVLDBEntry(&entry, vldbEntry, &tst)) {
	goto fail_vos_VLDBGet;
    }
    rc = 1;

  fail_vos_VLDBGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the vldb entry retrieval functions.
 */

typedef struct vldb_entry_get {
    afs_int32 total;		/* total number of vldb entries */
    afs_int32 index;		/* index to the current vldb entry */
    nbulkentries entries;	/* the list of entries retrieved */
    vos_vldbEntry_t entry[CACHED_ITEMS];	/* the cache of entries */
} vldb_entry_get_t, *vldb_entry_get_p;

static int
GetVLDBEntryRPC(void *rpc_specific, int slot, int *last_item,
		int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    vldb_entry_get_p entry = (vldb_entry_get_p) rpc_specific;

    /*
     * Copy the next entry into the cache
     */

    if (!copyVLDBEntry
	(&entry->entries.nbulkentries_val[entry->index], &entry->entry[slot],
	 &tst)) {
	goto fail_GetVLDBEntryRPC;
    }
    entry->index++;

    /*
     * See if we've processed all the entries
     */


    if (entry->index == entry->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

  fail_GetVLDBEntryRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetVLDBEntryFromCache(void *rpc_specific, int slot, void *dest,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    vldb_entry_get_p entry = (vldb_entry_get_p) rpc_specific;

    memcpy(dest, (const void *)&entry->entry[slot], sizeof(vos_vldbEntry_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
DestroyVLDBEntry(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    vldb_entry_get_p entry = (vldb_entry_get_p) rpc_specific;

    if (entry->entries.nbulkentries_val != NULL) {
	free(entry->entries.nbulkentries_val);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * vos_VLDBGetBegin - begin to iterate over the VLDB.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume entries exist.
 *
 * IN serverHandle - a handle to the server whose entries should be listed.
 * Can be null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition whose entries should be listed.
 * Can be null.
 *
 * OUT iterationIdP - upon successful completion, contains an iterator that
 * can be passed to vos_VLDBGetNext.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBGetBegin(const void *cellHandle, const void *serverHandle,
		 vos_MessageCallBack_t callBack, unsigned int *partition,
		 void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    vldb_entry_get_p entry =
	(vldb_entry_get_p) calloc(1, sizeof(vldb_entry_get_t));
    struct VldbListByAttributes attr;

    attr.Mask = 0;
    memset(&attr, 0, sizeof(attr));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBGetBegin;
    }

    if ((iter == NULL) || (entry == NULL)) {
	tst = ADMNOMEM;
	goto fail_vos_VLDBGetBegin;
    }

    if (f_server != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_VLDBGetBegin;
	}
	attr.server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));
	attr.Mask |= VLLIST_SERVER;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_VLDBGetBegin;
	}
	attr.partition = *partition;
	attr.Mask |= VLLIST_PARTITION;
    }

    if (!VLDB_ListAttributes
	(c_handle, &attr, &entry->total, &entry->entries, &tst)) {
	goto fail_vos_VLDBGetBegin;
    }

    if (entry->total <= 0) {
	if (!IteratorInit(iter, (void *)entry, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_vos_VLDBGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	if (!IteratorInit
	    (iter, (void *)entry, GetVLDBEntryRPC, GetVLDBEntryFromCache,
	     NULL, DestroyVLDBEntry, &tst)) {
	    goto fail_vos_VLDBGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_vos_VLDBGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (entry->entries.nbulkentries_val != NULL) {
	    free(entry->entries.nbulkentries_val);
	}
	if (entry != NULL) {
	    free(entry);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBGetNext - get information about the next volume.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * vos_VLDBGetBegin
 *
 * OUT vldbEntry - a pointer to a vos_vldbEntry_t
 * that upon successful completion contains information about the
 * next volume.
 *
 * LOCKS
 * 
 * The iterator is locked while the next item is retrieved.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBGetNext(const void *iterationId, vos_vldbEntry_p vldbEntry,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_VLDBGetNext;
    }

    if (vldbEntry == NULL) {
	tst = ADMVOSVLDBENTRYNULL;
	goto fail_vos_VLDBGetNext;
    }

    rc = IteratorNext(iter, (void *)vldbEntry, &tst);

  fail_vos_VLDBGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBGetDone - finish using a volume iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by vos_VLDBGetBegin
 *
 * LOCKS
 * 
 * The iterator is locked and then destroyed.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_VLDBGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_vos_VLDBGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBEntryRemove - remove a vldb entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the vldb entry exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the vldb entry exists.  Can be null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the vldb entry exists.  Can be null.
 *
 * IN volumeId - the volume id of the vldb entry to be deleted. Can be null.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBEntryRemove(const void *cellHandle, const void *serverHandle,
		    vos_MessageCallBack_t callBack,
		    const unsigned int *partition, unsigned int *volumeId,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    struct VldbListByAttributes attr;
    nbulkentries entries;
    afs_int32 nentries;
    int i;

    memset(&attr, 0, sizeof(attr));
    memset(&entries, 0, sizeof(entries));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBEntryRemove;
    }

    /*
     * If the volume is specified, just delete it
     */

    if (volumeId != NULL) {
	tst = ubik_Call(VL_DeleteEntry, c_handle->vos, 0, *volumeId, -1);
	if (tst != 0) {
	    goto fail_vos_VLDBEntryRemove;
	}
    }

    if (f_server != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_VLDBEntryRemove;
	}
	attr.server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));
	attr.Mask |= VLLIST_SERVER;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_VLDBEntryRemove;
	}
	attr.partition = *partition;
	attr.Mask |= VLLIST_PARTITION;
    }

    if ((f_server == NULL) && (partition == NULL)) {
	tst = ADMVOSVLDBDELETEALLNULL;
	goto fail_vos_VLDBEntryRemove;
    }

    if (!VLDB_ListAttributes(c_handle, &attr, &nentries, &entries, &tst)) {
	goto fail_vos_VLDBEntryRemove;
    }

    if (nentries <= 0) {
	tst = ADMVOSVLDBNOENTRIES;
	goto fail_vos_VLDBEntryRemove;
    }

    for (i = 0; i < nentries; i++) {
	ubik_Call(VL_DeleteEntry, c_handle->vos, 0,
		  entries.nbulkentries_val[i].volumeId[RWVOL]);
    }
    rc = 1;

  fail_vos_VLDBEntryRemove:

    if (entries.nbulkentries_val) {
	free(entries.nbulkentries_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBUnlock - unlock vldb entries en masse.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the vldb entries exist.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the vldb entries exist.  Can be null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the vldb entries exist.  Can be null.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBUnlock(const void *cellHandle, const void *serverHandle,
	       vos_MessageCallBack_t callBack, const unsigned int *partition,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    struct VldbListByAttributes attr;
    nbulkentries entries;
    afs_int32 nentries;
    int i;

    memset(&attr, 0, sizeof(attr));
    memset(&entries, 0, sizeof(entries));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBUnlock;
    }

    if (f_server != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_VLDBUnlock;
	}
	attr.server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));
	attr.Mask |= VLLIST_SERVER;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_VLDBUnlock;
	}
	attr.partition = *partition;
	attr.Mask |= VLLIST_PARTITION;
    }
    attr.flag = VLOP_ALLOPERS;
    attr.Mask |= VLLIST_FLAG;


    if (!VLDB_ListAttributes(c_handle, &attr, &nentries, &entries, &tst)) {
	goto fail_vos_VLDBUnlock;
    }

    if (nentries <= 0) {
	tst = ADMVOSVLDBNOENTRIES;
	goto fail_vos_VLDBUnlock;
    }

    for (i = 0; i < nentries; i++) {
	vos_VLDBEntryUnlock(cellHandle, 0,
			    entries.nbulkentries_val[i].volumeId[RWVOL],
			    &tst);
    }
    rc = 1;

  fail_vos_VLDBUnlock:

    if (entries.nbulkentries_val) {
	free(entries.nbulkentries_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * vos_VLDBEntryLock - lock a vldb entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the vldb entry exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume id of the vldb entry to be deleted.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBEntryLock(const void *cellHandle, vos_MessageCallBack_t callBack,
		  unsigned int volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBEntryLock;
    }

    tst = ubik_Call(VL_SetLock, c_handle->vos, 0, volumeId, -1, VLOP_DELETE);
    if (tst != 0) {
	goto fail_vos_VLDBEntryLock;
    }
    rc = 1;

  fail_vos_VLDBEntryLock:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBEntryUnlock - unlock a vldb entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the vldb entry exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume id of the vldb entry to be unlocked.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBEntryUnlock(const void *cellHandle, vos_MessageCallBack_t callBack,
		    unsigned int volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBEntryUnlock;
    }


    tst =
	ubik_Call(VL_ReleaseLock, c_handle->vos, 0, volumeId, -1,
		  LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    if (tst != 0) {
	goto fail_vos_VLDBEntryUnlock;
    }
    rc = 1;

  fail_vos_VLDBEntryUnlock:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBReadOnlySiteCreate - create a readonly site for a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the new volume should be created.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where then new volume should be created.
 *
 * IN volumeId - the volume id of the volume to be replicated.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBReadOnlySiteCreate(const void *cellHandle, const void *serverHandle,
			   vos_MessageCallBack_t callBack,
			   unsigned int partition, unsigned int volumeId,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBReadOnlySiteCreate;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VLDBReadOnlySiteCreate;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONTOOLARGE;
	goto fail_vos_VLDBReadOnlySiteCreate;
    }

    if (!UV_AddSite
	(c_handle, ntohl(rx_HostOf(rx_PeerOf(f_server->serv))), partition,
	 volumeId, &tst)) {
	goto fail_vos_VLDBReadOnlySiteCreate;
    }
    rc = 1;

  fail_vos_VLDBReadOnlySiteCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBReadOnlySiteDelete - delete a replication site for a volume.
 *
 * PARAMETERS
 *
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume should be deleted.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where then volume should be deleted.
 *
 * IN volumeId - the volume id of the volume to be deleted.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBReadOnlySiteDelete(const void *cellHandle, const void *serverHandle,
			   vos_MessageCallBack_t callBack,
			   unsigned int partition, unsigned int volumeId,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBReadOnlySiteDelete;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VLDBReadOnlySiteDelete;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONTOOLARGE;
	goto fail_vos_VLDBReadOnlySiteDelete;
    }

    if (!UV_RemoveSite
	(c_handle, ntohl(rx_HostOf(rx_PeerOf(f_server->serv))), partition,
	 volumeId, &tst)) {
	goto fail_vos_VLDBReadOnlySiteDelete;
    }
    rc = 1;

  fail_vos_VLDBReadOnlySiteDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VLDBSync - synchronize the vldb with the fileserver.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the sync should occur.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the sync should occur.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the sync should occur.  Can be null.
 *
 * IN force - force deletion of bad volumes.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VLDBSync(const void *cellHandle, const void *serverHandle,
	     vos_MessageCallBack_t callBack, const unsigned int *partition,
	     vos_force_t force, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_int32 part = 0;
    int flags = 0;
    int force_flag = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VLDBSync;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VLDBSync;
    }

    if (partition != NULL) {
	if (*partition > VOLMAXPARTS) {
	    tst = ADMVOSPARTITIONTOOLARGE;
	    goto fail_vos_VLDBSync;
	}
	part = (afs_int32) * partition;
	flags = 1;
    }

    if (force == VOS_FORCE) {
	force_flag = 1;
    }

    /*
     * sync the vldb
     */

    rc = UV_SyncVldb(c_handle, f_server->serv, part, flags, force_flag, &tst);

  fail_vos_VLDBSync:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeCreate - create a new partition.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server lives.
 *
 * IN serverHandle - a previously open vos server handle that holds
 * the partition where the volume should be create.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the integer that represents the partition that will 
 * house the new volume.
 *
 * IN volumeName - the name of the new volume.
 *
 * IN quota - the quota of the new volume.
 *
 * OUT volumeId - the volume id of the newly created volume.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeCreate(const void *cellHandle, const void *serverHandle,
		 vos_MessageCallBack_t callBack, unsigned int partition,
		 const char *volumeName, unsigned int quota,
		 unsigned int *volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    vos_partitionEntry_t pinfo;
    struct nvldbentry vinfo;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeCreate;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeCreate;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONTOOLARGE;
	goto fail_vos_VolumeCreate;
    }

    if (!ValidateVolumeName(volumeName, &tst)) {
	goto fail_vos_VolumeCreate;
    }

    if (volumeId == NULL) {
	tst = ADMVOSVOLUMEID;
	goto fail_vos_VolumeCreate;
    }

    /*
     * Check that partition is valid at the server
     */

    if (!vos_PartitionGet
	(cellHandle, serverHandle, 0, partition, &pinfo, &tst)) {
	goto fail_vos_VolumeCreate;
    }

    /*
     * Check that the volume doesn't already exist
     */

    if (VLDB_GetEntryByName(c_handle, volumeName, &vinfo, &tst)) {
	tst = ADMVOSVOLUMENAMEDUP;
	goto fail_vos_VolumeCreate;
    }

    /*
     * Create the new volume
     */

    rc = UV_CreateVolume(c_handle, f_server->serv, partition, volumeName,
			 quota, volumeId, &tst);

  fail_vos_VolumeCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeDelete - remove a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be deleted.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeDelete(const void *cellHandle, const void *serverHandle,
		 vos_MessageCallBack_t callBack, unsigned int partition,
		 unsigned int volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    vos_partitionEntry_t pinfo;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeDelete;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeDelete;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONTOOLARGE;
	goto fail_vos_VolumeDelete;
    }

    /*
     * Check that partition is valid at the server
     */

    if (!vos_PartitionGet
	(cellHandle, serverHandle, 0, partition, &pinfo, &tst)) {
	goto fail_vos_VolumeDelete;
    }

    rc = UV_DeleteVolume(c_handle, f_server->serv, partition, volumeId, &tst);

  fail_vos_VolumeDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeRename - rename a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the vldb entry exists.  Can be null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN readWriteVolumeId - the volume id of the volume to be renamed.
 *
 * IN newVolumeName - the new name.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeRename(const void *cellHandle, vos_MessageCallBack_t callBack,
		 unsigned int readWriteVolumeId, const char *newVolumeName,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    struct nvldbentry entry;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeRename;
    }

    if ((newVolumeName == NULL) || (*newVolumeName == 0)) {
	tst = ADMVOSNEWVOLUMENAMENULL;
	goto fail_vos_VolumeRename;
    }

    /*
     * Retrieve the entry
     */

    if (!VLDB_GetEntryByID(c_handle, readWriteVolumeId, -1, &entry, &tst)) {
	goto fail_vos_VolumeRename;
    }

    rc = UV_RenameVolume(c_handle, &entry, newVolumeName, &tst);

  fail_vos_VolumeRename:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeDump - dump a volume
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.  Can be null.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume id of the volume to be dumped.
 *
 * IN startTime - files with modification times >= this time will be dumped.
 *
 * IN dumpFile - the file to dump the volume to.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeDump(const void *cellHandle, const void *serverHandle,
	       vos_MessageCallBack_t callBack, unsigned int *partition,
	       unsigned int volumeId, unsigned int startTime,
	       const char *dumpFile, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_int32 server, part, voltype;
    struct nvldbentry entry;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeDump;
    }

    if (serverHandle != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_VolumeDump;
	}
    }

    /*
     * You must specify both the serverHandle and the partition
     */

    if (serverHandle || partition) {
	if (!serverHandle || !partition) {
	    tst = ADMVOSSERVERANDPARTITION;
	    goto fail_vos_VolumeDump;
	} else {
	    if (*partition > VOLMAXPARTS) {
		tst = ADMVOSPARTITIONTOOLARGE;
		goto fail_vos_VolumeDump;
	    }
	    server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));
	    part = *partition;
	}
    } else {
	if (!GetVolumeInfo
	    (c_handle, volumeId, &entry, &server, &part, &voltype, &tst)) {
	    goto fail_vos_VolumeDump;
	}
    }

    if ((dumpFile == NULL) || (*dumpFile == 0)) {
	tst = ADMVOSDUMPFILENULL;
	goto fail_vos_VolumeDump;
    }

    rc = UV_DumpVolume(c_handle, volumeId, server, part, startTime, dumpFile,
		       &tst);

  fail_vos_VolumeDump:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeRestore - restore a volume from a dump
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be restored.
 *
 * IN volumeName - the volume name of the volume to be restored.
 *
 * IN dumpFile - the file from which to restore the volume.
 *
 * IN dumpType - the type of dump to perform.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeRestore(const void *cellHandle, const void *serverHandle,
		  vos_MessageCallBack_t callBack, unsigned int partition,
		  unsigned int *volumeId, const char *volumeName,
		  const char *dumpFile, vos_volumeRestoreType_t dumpType,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    struct nvldbentry entry;
    afs_int32 volid, server;
    int fd;
    struct stat status;
    int restoreflags = 0;
    afs_int32 Oserver, Opart, Otype;
    struct nvldbentry Oentry;
    int equal;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeRestore;
    }

    if (serverHandle != NULL) {
	if (!IsValidServerHandle(f_server, &tst)) {
	    goto fail_vos_VolumeRestore;
	}
    }

    /*
     * Must pass volumeName
     */

    if ((volumeName == NULL) || (*volumeName == 0)) {
	tst = ADMVOSVOLUMENAMENULL;
	goto fail_vos_VolumeRestore;
    }

    if (!ValidateVolumeName(volumeName, &tst)) {
	goto fail_vos_VolumeRestore;
    }

    /*
     * If volumeId is passed, it must be a valid volume id
     */

    if (volumeId != NULL) {
	if (!VLDB_GetEntryByID(c_handle, *volumeId, -1, &entry, &tst)) {
	    goto fail_vos_VolumeRestore;
	}
	volid = *volumeId;
    } else {
	volid = 0;
    }

    server = ntohl(rx_HostOf(rx_PeerOf(f_server->serv)));

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONTOOLARGE;
	goto fail_vos_VolumeRestore;
    }

    /*
     * Check that dumpFile exists and can be accessed
     */

    fd = open(dumpFile, 0);
    if ((fd < 0) || (fstat(fd, &status) < 0)) {
	close(fd);
	tst = ADMVOSDUMPFILEOPENFAIL;
	goto fail_vos_VolumeRestore;
    } else {
	close(fd);
    }

    if (!VLDB_GetEntryByName(c_handle, volumeName, &entry, &tst)) {
	restoreflags = RV_FULLRST;
    } else if (Lp_GetRwIndex(c_handle, &entry, 0) == -1) {
	restoreflags = RV_FULLRST;
	if (volid == 0) {
	    volid = entry.volumeId[RWVOL];
	} else if ((entry.volumeId[RWVOL] != 0)
		   && (entry.volumeId[RWVOL] != volid)) {
	    volid = entry.volumeId[RWVOL];
	}
    } else {

	if (volid == 0) {
	    volid = entry.volumeId[RWVOL];
	} else if ((entry.volumeId[RWVOL] != 0)
		   && (entry.volumeId[RWVOL] != volid)) {
	    volid = entry.volumeId[RWVOL];
	}

	/*
	 * If the vldb says the same volume exists somewhere else
	 * the caller must specify a full restore, not an incremental
	 */

	if (dumpType == VOS_RESTORE_FULL) {
	    restoreflags = RV_FULLRST;
	} else {

	    /*
	     * Check to see if the volume exists where the caller said
	     */
	    if (!GetVolumeInfo
		(c_handle, volid, &Oentry, &Oserver, &Opart, &Otype, &tst)) {
		goto fail_vos_VolumeRestore;
	    }
	    if (!VLDB_IsSameAddrs(c_handle, Oserver, server, &equal, &tst)) {
		goto fail_vos_VolumeRestore;
	    }

	    if (!equal) {
		tst = ADMVOSRESTOREVOLEXIST;
		goto fail_vos_VolumeRestore;
	    }
	}
    }

    rc = UV_RestoreVolume(c_handle, server, partition, volid, volumeName,
			  restoreflags, dumpFile, &tst);

  fail_vos_VolumeRestore:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeOnline - bring a volume online.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be brought online.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeOnline(const void *serverHandle, vos_MessageCallBack_t callBack,
		 unsigned int partition, unsigned int volumeId,
		 unsigned int sleepTime, vos_volumeOnlineType_t volumeStatus,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;
    int up = ITOffline;

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeOnline;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeOnline;
    }

    if (volumeStatus == VOS_ONLINE_BUSY) {
	up = ITBusy;
    }

    rc = UV_SetVolume(f_server->serv, partition, volumeId, up, 0, sleepTime,
		      &tst);

  fail_vos_VolumeOnline:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeOffline - take a volume offline.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be taken offline.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeOffline(const void *serverHandle, vos_MessageCallBack_t callBack,
		  unsigned int partition, unsigned int volumeId,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeOffline;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeOffline;
    }

    rc = UV_SetVolume(f_server->serv, partition, volumeId, ITOffline,
		      VTOutOfService, 0, &tst);

  fail_vos_VolumeOffline:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * copyvolintXInfo - copy a struct volintXInfo to a vos_volumeEntry_p.
 *
 * PARAMETERS
 *
 * IN source - the volintXInfo structure to copy.
 *
 * OUT dest - the vos_volumeEntry_t to fill
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
copyvolintXInfo(struct volintXInfo *source, vos_volumeEntry_p dest,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int i;

    /*
     * If the volume is not marked OK, all the other fields are invalid
     * We take the extra step of blanking out dest here to prevent the
     * user from seeing stale data from a previous call
     */

    memset(dest, 0, sizeof(dest));

    switch (source->status) {
    case VOK:
	dest->status = VOS_OK;
	break;
    case VSALVAGE:
	dest->status = VOS_SALVAGE;
	break;
    case VNOVNODE:
	dest->status = VOS_NO_VNODE;
	break;
    case VNOVOL:
	dest->status = VOS_NO_VOL;
	break;
    case VVOLEXISTS:
	dest->status = VOS_VOL_EXISTS;
	break;
    case VNOSERVICE:
	dest->status = VOS_NO_SERVICE;
	break;
    case VOFFLINE:
	dest->status = VOS_OFFLINE;
	break;
    case VONLINE:
	dest->status = VOS_ONLINE;
	break;
    case VDISKFULL:
	dest->status = VOS_DISK_FULL;
	break;
    case VOVERQUOTA:
	dest->status = VOS_OVER_QUOTA;
	break;
    case VBUSY:
	dest->status = VOS_BUSY;
	break;
    case VMOVED:
	dest->status = VOS_MOVED;
	break;
    }

    /*
     * Check to see if the entry is marked ok before copying all the
     * fields
     */

    if (dest->status == VOS_OK) {
	strcpy(dest->name, source->name);
	dest->id = source->volid;
	if (source->type == 0) {
	    dest->type = VOS_READ_WRITE_VOLUME;
	} else if (source->type == 1) {
	    dest->type = VOS_READ_ONLY_VOLUME;
	} else if (source->type == 2) {
	    dest->type = VOS_BACKUP_VOLUME;
	}
	dest->backupId = source->backupID;
	dest->readWriteId = source->parentID;
	dest->readOnlyId = source->cloneID;
	dest->copyCreationDate = source->copyDate;
	dest->creationDate = source->creationDate;
	dest->lastAccessDate = source->accessDate;
	dest->lastUpdateDate = source->updateDate;
	dest->lastBackupDate = source->backupDate;
	dest->accessesSinceMidnight = source->dayUse;
	dest->fileCount = source->filecount;
	dest->maxQuota = source->maxquota;
	dest->currentSize = source->size;
	if (source->inUse == 1) {
	    dest->volumeDisposition = VOS_ONLINE;
	} else {
	    dest->volumeDisposition = VOS_OFFLINE;
	}

	for (i = 0; i < VOS_VOLUME_READ_WRITE_STATS_NUMBER; i++) {
	    dest->readStats[i] = source->stat_reads[i];
	    dest->writeStats[i] = source->stat_writes[i];
	}

	for (i = 0; i < VOS_VOLUME_TIME_STATS_NUMBER; i++) {
	    dest->fileAuthorWriteSameNetwork[i] =
		source->stat_fileSameAuthor[i];
	    dest->fileAuthorWriteDifferentNetwork[i] =
		source->stat_fileDiffAuthor[i];
	    dest->dirAuthorWriteSameNetwork[i] =
		source->stat_dirSameAuthor[i];
	    dest->dirAuthorWriteDifferentNetwork[i] =
		source->stat_dirDiffAuthor[i];
	}
    }

    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeGet - get information about a particular volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be retrieved.
 *
 * OUT volumeP - upon successful completion, contains the information about the 
 * specified volume.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeGet(const void *cellHandle, const void *serverHandle,
	      vos_MessageCallBack_t callBack, unsigned int partition,
	      unsigned int volumeId, vos_volumeEntry_p volumeP,
	      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;
    struct volintXInfo *info = NULL;

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeGet;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeGet;
    }

    if (volumeP == NULL) {
	tst = ADMVOSVOLUMEPNULL;
	goto fail_vos_VolumeGet;
    }

    /*
     * Retrieve the information for the volume
     */

    if (!UV_XListOneVolume(f_server->serv, partition, volumeId, &info, &tst)) {
	goto fail_vos_VolumeGet;
    }

    /*
     * Copy the volume info to our structure
     */

    if (!copyvolintXInfo(info, volumeP, &tst)) {
	goto fail_vos_VolumeGet;
    }
    rc = 1;

  fail_vos_VolumeGet:

    if (info != NULL) {
	free(info);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the volume retrieval functions.
 */

typedef struct volume_get {
    struct volintXInfo *vollist;
    afs_int32 total;		/* total number of volumes at this partition */
    afs_int32 index;		/* index to the current volume */
    vos_volumeEntry_t entry[CACHED_ITEMS];	/* the cache of entries */
} volume_get_t, *volume_get_p;

static int
GetVolumeRPC(void *rpc_specific, int slot, int *last_item,
	     int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    volume_get_p entry = (volume_get_p) rpc_specific;

    /*
     * Copy the next entry into the cache
     */

    if (!copyvolintXInfo
	(&entry->vollist[entry->index], &entry->entry[slot], &tst)) {
	goto fail_GetVolumeRPC;
    }
    entry->index++;

    /*
     * See if we've processed all the entries
     */


    if (entry->index == entry->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

  fail_GetVolumeRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetVolumeFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    volume_get_p entry = (volume_get_p) rpc_specific;

    memcpy(dest, (const void *)&entry->entry[slot],
	   sizeof(vos_volumeEntry_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
DestroyVolume(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    volume_get_p entry = (volume_get_p) rpc_specific;

    if (entry->vollist != NULL) {
	free(entry->vollist);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * vos_VolumeGetBegin - begin to iterator over the list of volumes at a server.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volumes exist.
 *
 * IN serverHandle - a handle to the server where the volumes exist.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition whose volumes should be listed.  Can be null.
 *
 * OUT iterationIdP - upon successful completion, contains an iterator that
 * can be passed to vos_VolumeGetBegin.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeGetBegin(const void *cellHandle, const void *serverHandle,
		   vos_MessageCallBack_t callBack, unsigned int partition,
		   void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    file_server_p f_server = (file_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    volume_get_p entry = (volume_get_p) calloc(1, sizeof(volume_get_t));

    /*
     * Validate arguments
     */

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeGetBegin;
    }

    if (partition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeGetBegin;
    }

    if ((iter == NULL) || (entry == NULL)) {
	tst = ADMNOMEM;
	goto fail_vos_VolumeGetBegin;
    }

    /*
     * Get a list of all the volumes contained in the partition at the
     * server
     */

    if (!UV_XListVolumes
	(f_server->serv, partition, 1, &entry->vollist, &entry->total,
	 &tst)) {
	goto fail_vos_VolumeGetBegin;
    }

    if (entry->total == 0) {
	if (!IteratorInit(iter, (void *)entry, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_vos_VolumeGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	if (!IteratorInit
	    (iter, (void *)entry, GetVolumeRPC, GetVolumeFromCache, NULL,
	     DestroyVolume, &tst)) {
	    goto fail_vos_VolumeGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_vos_VolumeGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (entry != NULL) {
	    free(entry);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeGetNext - get information about the next volume.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * vos_VolumeGetBegin
 *
 * OUT volumeP - a pointer to a vos_volumeEntry_t
 * that upon successful completion contains information about the
 * next volume.
 *
 * LOCKS
 * 
 * The iterator is locked while the next item is retrieved.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeGetNext(const void *iterationId, vos_volumeEntry_p volumeP,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_VolumeGetNext;
    }

    if (volumeP == NULL) {
	tst = ADMVOSVOLUMEPNULL;
	goto fail_vos_VolumeGetNext;
    }

    rc = IteratorNext(iter, (void *)volumeP, &tst);

  fail_vos_VolumeGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeGetDone - finish using a volume iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by vos_VolumeGetBegin
 *
 * LOCKS
 * 
 * The iterator is locked and then destroyed.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_vos_VolumeGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_vos_VolumeGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeMove - move a volume from one server to another.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume id of the volume to be moved.
 *
 * IN fromServer - a previously opened serverHandle that corresponds
 * to the server where the volume currently resides.
 *
 * IN fromPartition - the partition where the volume currently resides.
 *
 * IN toServer - a previously opened serverHandle that corresponds
 * to the server where the volume will be moved.
 *
 * IN toPartition - the partition where the volume will be moved.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeMove(const void *cellHandle, vos_MessageCallBack_t callBack,
	       unsigned int volumeId, const void *fromServer,
	       unsigned int fromPartition, const void *toServer,
	       unsigned int toPartition, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p from_server = (file_server_p) fromServer;
    file_server_p to_server = (file_server_p) toServer;
    afs_int32 from_server_addr =
	ntohl(rx_HostOf(rx_PeerOf(from_server->serv)));
    afs_int32 to_server_addr = ntohl(rx_HostOf(rx_PeerOf(to_server->serv)));
    afs_int32 from_partition = fromPartition;
    afs_int32 to_partition = toPartition;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeMove;
    }

    if (!IsValidServerHandle(from_server, &tst)) {
	goto fail_vos_VolumeMove;
    }

    if (!IsValidServerHandle(to_server, &tst)) {
	goto fail_vos_VolumeMove;
    }

    if (fromPartition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeMove;
    }

    if (toPartition > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_VolumeMove;
    }

    /*
     * Move the volume
     */

    rc = UV_MoveVolume(c_handle, volumeId, from_server_addr, from_partition,
		       to_server_addr, to_partition, &tst);

  fail_vos_VolumeMove:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeRelease - release a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN volumeId - the volume to be released.
 *
 * IN force - force a complete release.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeRelease(const void *cellHandle, vos_MessageCallBack_t callBack,
		  unsigned int volumeId, vos_force_t force, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 server, part, forc = 0, voltype, volume;
    struct nvldbentry entry;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeRelease;
    }

    if (!GetVolumeInfo
	(c_handle, volumeId, &entry, &server, &part, &voltype, &tst)) {
	goto fail_vos_VolumeRelease;
    }

    if (force == VOS_FORCE) {
	forc = 1;
    }

    volume = volumeId;
    rc = UV_ReleaseVolume(c_handle, volume, server, part, forc, &tst);

  fail_vos_VolumeRelease:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeZap - forcibly delete a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the vldb entry to be deleted.
 *
 * IN force - force the deletion of bad volumes.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeZap(const void *cellHandle, const void *serverHandle,
	      vos_MessageCallBack_t callBack, unsigned int partition,
	      unsigned int volumeId, vos_force_t force, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;

    /*
     * Verify that the cellHandle is capable of making vos rpc's
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeZap;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeZap;
    }

    if (force == VOS_FORCE) {
	rc = UV_NukeVolume(c_handle, f_server->serv, partition, volumeId,
			   &tst);
    } else {
	rc = UV_VolumeZap(c_handle, f_server->serv, partition, volumeId,
			  &tst);
    }

  fail_vos_VolumeZap:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionNameToId - translate a string representing a partition
 * to a number.
 *
 * PARAMETERS
 *
 * IN partitionName - a string representing a partition.  Must be of
 * the form /vicep..
 *
 * OUT partitionId - a number containing the partition id upon successful 
 * completion.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionNameToId(const char *partitionName, unsigned int *partitionId,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    size_t partition_name_len;
    int i;

    /* 
     * Validate arguments
     */

    if (partitionName == NULL) {
	tst = ADMVOSPARTITIONNAMENULL;
	goto fail_vos_PartitionNameToId;
    }

    if (partitionId == NULL) {
	tst = ADMVOSPARTITIONIDNULL;
	goto fail_vos_PartitionNameToId;
    }

    /*
     * Check that string begins with /vicep
     */

    if (strncmp(partitionName, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
	tst = ADMVOSPARTITIONNAMEINVALID;
	goto fail_vos_PartitionNameToId;
    }

    /*
     * Check that the string is either one or two characters
     * longer than VICE_PREFIX_SIZE
     */

    partition_name_len = strlen(partitionName);

    if (partition_name_len == VICE_PREFIX_SIZE) {
	tst = ADMVOSPARTITIONNAMETOOSHORT;
	goto fail_vos_PartitionNameToId;
    }

    if (partition_name_len > (VICE_PREFIX_SIZE + 2)) {
	tst = ADMVOSPARTITIONNAMETOOLONG;
	goto fail_vos_PartitionNameToId;
    }

    /*
     * Check that all characters past the prefix are lower case
     */

    for (i = VICE_PREFIX_SIZE; i < partition_name_len; i++) {
	if (!islower(partitionName[i])) {
	    tst = ADMVOSPARTITIONNAMENOTLOWER;
	    goto fail_vos_PartitionNameToId;
	}
    }

    /*
     * Convert the name to a number
     */

    if (partitionName[VICE_PREFIX_SIZE + 1] == 0) {
	*partitionId = partitionName[VICE_PREFIX_SIZE] - 'a';
    } else {
	*partitionId =
	    (partitionName[VICE_PREFIX_SIZE] - 'a') * 26 +
	    (partitionName[VICE_PREFIX_SIZE + 1] - 'a') + 26;
    }

    if (*partitionId > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_PartitionNameToId;
    }
    rc = 1;

  fail_vos_PartitionNameToId:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_PartitionIdToName - translate a number representing a partition
 * to a character string.
 *
 * PARAMETERS
 *
 * IN partitionId - an integer representing the partition.
 *
 * OUT partitionName - a string containing the converted partition ID
 * upon successful completion.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_PartitionIdToName(unsigned int partitionId, char *partitionName,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (partitionId > VOLMAXPARTS) {
	tst = ADMVOSPARTITIONIDTOOLARGE;
	goto fail_vos_PartitionIdToName;
    }

    if (partitionName == NULL) {
	tst = ADMVOSPARTITIONNAMENULL;
	goto fail_vos_PartitionIdToName;
    }

    if (partitionId < 26) {
	strcpy(partitionName, VICE_PARTITION_PREFIX);
	partitionName[6] = partitionId + 'a';
	partitionName[7] = '\0';
    } else {
	strcpy(partitionName, VICE_PARTITION_PREFIX);
	partitionId -= 26;
	partitionName[6] = 'a' + (partitionId / 26);
	partitionName[7] = 'a' + (partitionId % 26);
	partitionName[8] = '\0';
    }
    rc = 1;

  fail_vos_PartitionIdToName:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * vos_VolumeQuotaChange - change the quota of a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the volume exists.
 *
 * IN serverHandle - a previously opened serverHandle that corresponds
 * to the server where the volume exists.
 *
 * IN callBack - a call back function pointer that may be called to report
 * status information.  Can be null.
 *
 * IN partition - the partition where the volume exists.
 *
 * IN volumeId - the volume id of the volume to be modified.
 *
 * IN volumeQuota - the new volume quota.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
vos_VolumeQuotaChange(const void *cellHandle, const void *serverHandle,
		      vos_MessageCallBack_t callBack, unsigned int partition,
		      unsigned int volumeId, unsigned int volumeQuota,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    file_server_p f_server = (file_server_p) serverHandle;
    int ttid = 0;
    int rcode = 0;
    struct volintInfo tstatus;
    int active_trans = 0;

    /*
     * Verify that the cellHandle is capable of making vos rpc's
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_vos_VolumeQuotaChange;
    }

    if (!IsValidServerHandle(f_server, &tst)) {
	goto fail_vos_VolumeQuotaChange;
    }

    memset((void *)&tstatus, 0, sizeof(tstatus));
    tstatus.dayUse = -1;
    tstatus.maxquota = volumeQuota;

    tst =
	AFSVolTransCreate(f_server->serv, volumeId, partition, ITBusy, &ttid);
    if (tst) {
	goto fail_vos_VolumeQuotaChange;
    }
    active_trans = 1;

    tst = AFSVolSetInfo(f_server->serv, ttid, &tstatus);
    if (tst) {
	goto fail_vos_VolumeQuotaChange;
    }
    rc = 1;

  fail_vos_VolumeQuotaChange:

    if (active_trans) {
	afs_status_t tst2 = 0;
	tst2 = AFSVolEndTrans(f_server->serv, ttid, &rcode);
	if (tst2) {
	    if (tst == 0) {
		tst = tst2;
		rc = 0;
	    }
	}
	if (rcode) {
	    if (tst == 0) {
		tst = rcode;
		rc = 0;
	    }
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
