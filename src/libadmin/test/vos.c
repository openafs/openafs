/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the vos related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>
#ifndef AFS_NT40_ENV
#include <netdb.h>
#endif

RCSID
    ("$Header$");

#include "vos.h"

/*
 * Utility functions
 */

/*
 * Generic fuction for converting input string to an integer.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static unsigned int
GetIntFromString(const char *int_str, const char *error_msg)
{
    unsigned int i;
    char *bad_char = NULL;

    i = strtoul(int_str, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return i;
    }

    ERR_EXT(error_msg);
}

/*
 * Generic fuction for converting input string to a volume number
 * It will accept integer strings as well as volume names.
 */

static unsigned int
GetVolumeIdFromString(const char *volume)
{
    unsigned int volume_id;
    char *bad_char = NULL;
    afs_status_t st = 0;
    vos_vldbEntry_t entry;

    volume_id = strtoul(volume, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return volume_id;
    }

    /*
     * We failed to convert the string to a number, so see if it
     * is a volume name
     */
    if (vos_VLDBGet
	(cellHandle, 0, (const unsigned int *)0, volume, &entry, &st)) {
	return entry.volumeId[VOS_READ_WRITE_VOLUME];
    } else {
	ERR_EXT("failed to convert specified volume to an id");
    }
}

/*
 * Generic fuction for converting input string to a partition number
 * It will accept integer strings as well as partition names.
 */

static unsigned int
GetPartitionIdFromString(const char *partition)
{
    unsigned int partition_id;
    char *bad_char = NULL;
    afs_status_t st = 0;
    char pname[20];
    int pname_len;

    partition_id = strtoul(partition, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return partition_id;
    }

    /*
     * We failed to convert the string to a number, so see if it
     * is a partition name
     */

    pname_len = strlen(partition);

    sprintf(pname, "%s", "/vicep");
    if (pname_len <= 2) {
	strcat(pname, partition);
    } else if (!strncmp(partition, "/vicep", 6)) {
	strcat(pname, partition + 6);
    } else if (!strncmp(partition, "vicep", 5)) {
	strcat(pname, partition + 5);
    } else {
	ERR_EXT("invalid partition");
    }

    if (!vos_PartitionNameToId((const char *)pname, &partition_id, &st)) {
	ERR_ST_EXT("invalid partition", st);
    }

    return partition_id;
}

/*
 * Generic fuction for converting input string to an address in host order. 
 * It will accept strings in the form "128.98.12.1"
 */

static int
GetAddressFromString(const char *addr_str)
{
    int addr = inet_addr(addr_str);

    if (addr == -1) {
	ERR_EXT("failed to convert specified address");
    }

    return ntohl(addr);
}

static void
PrintMessage(vos_messageType_t type, char *message)
{
    printf("%s\n", message);
}

int
DoVosBackupVolumeCreate(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME } DoVosBackupVolumeCreate_parm_t;
    afs_status_t st = 0;
    unsigned int volume_id;

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_BackupVolumeCreate(cellHandle, 0, volume_id, &st)) {
	ERR_ST_EXT("vos_BackupVolumeCreate", st);
    }
    return 0;
}

int
DoVosBackupVolumeCreateMultiple(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, PREFIX,
	EXCLUDE
    } DoVosBackupVolumeCreate_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    const unsigned int *part_ptr = NULL;
    const char *prefix = NULL;
    vos_exclude_t exclude = VOS_INCLUDE;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (as->parms[PREFIX].items) {
	prefix = as->parms[PREFIX].items->data;
    }

    if (as->parms[EXCLUDE].items) {
	exclude = VOS_EXCLUDE;
    }

    if (!vos_BackupVolumeCreateMultiple
	(cellHandle, vos_server, (vos_MessageCallBack_t) 0, part_ptr, prefix,
	 exclude, &st)) {
	ERR_ST_EXT("vos_BackupVolumeCreate", st);
    }
    return 0;
}

static void
Print_vos_partitionEntry_p(vos_partitionEntry_p part, const char *prefix)
{
    printf("%sInformation for partition %s\n", prefix, part->name);
    printf("%s\tDevice name: %s\n", prefix, part->deviceName);
    printf("%s\tlockFileDescriptor: %d\n", prefix, part->lockFileDescriptor);
    printf("%s\tTotal space: %d\n", prefix, part->totalSpace);
    printf("%s\tTotal Free space: %d\n", prefix, part->totalFreeSpace);
}

int
DoVosPartitionGet(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION } DoVosPartitionGet_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    vos_partitionEntry_t entry;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (!vos_PartitionGet
	(cellHandle, vos_server, 0, partition_id, &entry, &st)) {
	ERR_ST_EXT("vos_PartitionGet", st);
    }

    Print_vos_partitionEntry_p(&entry, "");

    return 0;
}

int
DoVosPartitionList(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER } DoVosPartitionGet_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    void *iter;
    vos_partitionEntry_t entry;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (!vos_PartitionGetBegin(cellHandle, vos_server, 0, &iter, &st)) {
	ERR_ST_EXT("vos_PartitionGetBegin", st);
    }

    printf("Listing partitions at server %s\n",
	   as->parms[SERVER].items->data);
    while (vos_PartitionGetNext(iter, &entry, &st)) {
	Print_vos_partitionEntry_p(&entry, "    ");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("vos_PartitionGetNext", st);
    }

    if (!vos_PartitionGetDone(iter, &st)) {
	ERR_ST_EXT("vos_PartitionGetDone", st);
    }
    return 0;
}

int
DoVosServerSync(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION } DoVosServerSync_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    const unsigned int *part_ptr = NULL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (!vos_ServerSync(cellHandle, vos_server, 0, part_ptr, &st)) {
	ERR_ST_EXT("vos_PartitionGetDone", st);
    }
    return 0;
}

int
DoVosFileServerAddressChange(struct cmd_syndesc *as, char *arock)
{
    typedef enum { OLDADDRESS,
	NEWADDRESS
    } DoVosFileServerAddressChange_parm_t;
    afs_status_t st = 0;
    int old_addr, new_addr;

    if (as->parms[OLDADDRESS].items) {
	const char *addr = as->parms[OLDADDRESS].items->data;
	old_addr = GetAddressFromString(addr);
    }

    if (as->parms[NEWADDRESS].items) {
	const char *addr = as->parms[OLDADDRESS].items->data;
	new_addr = GetAddressFromString(addr);
    }

    if (!vos_FileServerAddressChange(cellHandle, 0, old_addr, new_addr, &st)) {
	ERR_ST_EXT("vos_FileServerAddressChange", st);
    }
    return 0;
}

int
DoVosFileServerAddressRemove(struct cmd_syndesc *as, char *arock)
{
    typedef enum { ADDRESS } DoVosFileServerAddressRemove_parm_t;
    afs_status_t st = 0;
    int address;

    if (as->parms[ADDRESS].items) {
	const char *addr = as->parms[ADDRESS].items->data;
	address = GetAddressFromString(addr);
    }

    if (!vos_FileServerAddressRemove(cellHandle, 0, address, &st)) {
	ERR_ST_EXT("vos_FileServerAddressRemove", st);
    }
    return 0;
}

static void
Print_vos_fileServerEntry_p(vos_fileServerEntry_p serv, const char *prefix)
{
    int i;

    for (i = 0; i < serv->count; i++) {
	printf("%s%x ", prefix, serv->serverAddress[i]);
    }
    printf("\n");
}

int
DoVosFileServerList(struct cmd_syndesc *as, char *arock)
{
    afs_status_t st = 0;
    void *iter;
    vos_fileServerEntry_t entry;

    if (!vos_FileServerGetBegin(cellHandle, 0, &iter, &st)) {
	ERR_ST_EXT("vos_FileServerGetBegin", st);
    }

    while (vos_FileServerGetNext(iter, &entry, &st)) {
	Print_vos_fileServerEntry_p(&entry, "");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("vos_FileServerGetNext", st);
    }

    if (!vos_FileServerGetDone(iter, &st)) {
	ERR_ST_EXT("vos_FileServerGetDone", st);
    }

    return 0;
}

static void
Print_vos_serverTransactionStatus_p(vos_serverTransactionStatus_p tran,
				    const char *prefix)
{
    printf("%sTransaction id\t\t\t%d\n", prefix, tran->transactionId);
    printf("%sLast active time\t\t\t%d\n", prefix, tran->lastActiveTime);
    printf("%sCreation time\t\t\t%d\n", prefix, tran->creationTime);
    printf("%sError code\t\t\t%d\n", prefix, tran->errorCode);
    printf("%sVolume id\t\t\t\t%u\n", prefix, tran->volumeId);
    printf("%sPartition\t\t\t\t%d\n", prefix, tran->partition);
    printf("%sLast procedure name\t\t\t%s\n", prefix,
	   tran->lastProcedureName);
    printf("%sNext receive packet seq num\t\t\t%d\n", prefix,
	   tran->nextReceivePacketSequenceNumber);
    printf("%sNext send packet seq num\t\t\t%d\n", prefix,
	   tran->nextSendPacketSequenceNumber);
    printf("%sLast receive time\t\t\t%d\n", prefix, tran->lastReceiveTime);
    printf("%sLast send time\t\t\t%d\n", prefix, tran->lastSendTime);
    printf("%sVolume attach mode\t\t\t%d\n", prefix, tran->volumeAttachMode);
    printf("%sVolume active status\t\t\t%d\n", prefix,
	   tran->volumeActiveStatus);
    printf("%sVolume tran status\t\t\t%d\n", prefix,
	   tran->volumeTransactionStatus);
}

int
DoVosServerTransactionStatusList(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER } DoVosServerTransactionStatusList_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    void *iter = NULL;
    vos_serverTransactionStatus_t tran;


    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (!vos_ServerTransactionStatusGetBegin
	(cellHandle, vos_server, 0, &iter, &st)) {
	ERR_ST_EXT("vos_ServerTransactionStatusGetBegin", st);
    }

    while (vos_ServerTransactionStatusGetNext(iter, &tran, &st)) {
	Print_vos_serverTransactionStatus_p(&tran, "");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("vos_ServerTransactionStatusGetNext", st);
    }

    if (!vos_ServerTransactionStatusGetDone(iter, &st)) {
	ERR_ST_EXT("vos_ServerTransactionStatusGetDone", st);
    }
    return 0;
}

static void
Print_vos_vldbEntry_p(vos_vldbEntry_p entry, const char *prefix)
{
    int i;

    printf("%sVolume entry %s\n", prefix, entry->name);
    printf("%sNumber of servers %d\n", prefix, entry->numServers);
    printf("%sRead write volume %u\n", prefix,
	   entry->volumeId[VOS_READ_WRITE_VOLUME]);
    printf("%sRead only volume %u\n", prefix,
	   entry->volumeId[VOS_READ_ONLY_VOLUME]);
    printf("%sBackup volume %u\n", prefix,
	   entry->volumeId[VOS_BACKUP_VOLUME]);
    printf("%sClone volume %u\n", prefix, entry->cloneId);

    printf("%sVolume entry status:\n", prefix);
    if (entry->status & VOS_VLDB_ENTRY_OK) {
	printf("%s\tVOS_VLDB_ENTRY_OK\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_MOVE) {
	printf("%s\tVOS_VLDB_ENTRY_MOVE\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_RELEASE) {
	printf("%s\tVOS_VLDB_ENTRY_RELEASE\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_BACKUP) {
	printf("%s\tVOS_VLDB_ENTRY_BACKUP\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_DELETE) {
	printf("%s\tVOS_VLDB_ENTRY_DELETE\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_DUMP) {
	printf("%s\tVOS_VLDB_ENTRY_DUMP\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_LOCKED) {
	printf("%s\tVOS_VLDB_ENTRY_LOCKED\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_RWEXISTS) {
	printf("%s\tVOS_VLDB_ENTRY_RWEXISTS\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_ROEXISTS) {
	printf("%s\tVOS_VLDB_ENTRY_ROEXISTS\n", prefix);
    }
    if (entry->status & VOS_VLDB_ENTRY_BACKEXISTS) {
	printf("%s\tVOS_VLDB_ENTRY_BACKEXISTS\n", prefix);
    }

    printf("%sVolume location information for replicas:\n", prefix);
    for (i = 0; i < entry->numServers; i++) {
	printf("%s\tServer %x\n", prefix,
	       entry->volumeSites[i].serverAddress);
	printf("%s\tPartition %x\n", prefix,
	       entry->volumeSites[i].serverPartition);
	if (entry->volumeSites[i].serverFlags & VOS_VLDB_NEW_REPSITE) {
	    printf("%s\tVOS_VLDB_NEW_REPSITE\n", prefix);
	}
	if (entry->volumeSites[i].serverFlags & VOS_VLDB_READ_ONLY) {
	    printf("%s\tVOS_VLDB_READ_ONLY\n", prefix);
	}
	if (entry->volumeSites[i].serverFlags & VOS_VLDB_READ_WRITE) {
	    printf("%s\tVOS_VLDB_READ_WRITE\n", prefix);
	}
	if (entry->volumeSites[i].serverFlags & VOS_VLDB_BACKUP) {
	    printf("%s\tVOS_VLDB_BACKUP\n", prefix);
	}
	if (entry->volumeSites[i].serverFlags & VOS_VLDB_DONT_USE) {
	    printf("%s\tVOS_VLDB_DONT_USE\n", prefix);
	}
    }
    printf("\n");
}

int
DoVosVLDBGet(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME } DoVosVLDBGet_parm_t;
    afs_status_t st = 0;
    vos_vldbEntry_t entry;
    unsigned int volume_id;
    const char *volume_name = NULL;

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }


    if (!vos_VLDBGet(cellHandle, 0, &volume_id, volume_name, &entry, &st)) {
	ERR_ST_EXT("vos_VLDBGet", st);
    }

    Print_vos_vldbEntry_p(&entry, "");

    return 0;
}

int
DoVosVLDBList(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION } DoVosVLDBList_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int *part_ptr = NULL;
    int have_server = 0;
    vos_vldbEntry_t entry;
    void *iter = NULL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
	have_server = 1;
    }

    if (as->parms[PARTITION].items) {
	if (!have_server) {
	    ERR_EXT("must specify server when specifying partition");
	}
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (!vos_VLDBGetBegin(cellHandle, vos_server, 0, part_ptr, &iter, &st)) {
	ERR_ST_EXT("vos_VLDBGetBegin", st);
    }

    while (vos_VLDBGetNext(iter, &entry, &st)) {
	Print_vos_vldbEntry_p(&entry, "");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("vos_VLDBGetNext", st);
    }

    if (!vos_VLDBGetDone(iter, &st)) {
	ERR_ST_EXT("vos_VLDBGetDone", st);
    }

    return 0;
}

int
DoVosVLDBEntryRemove(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVLDBEntryRemove_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int *part_ptr = NULL;
    int have_server = 0;
    unsigned int volume_id;
    unsigned int *vol_ptr = NULL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
	have_server = 1;
    }

    if (as->parms[PARTITION].items) {
	if (!have_server) {
	    ERR_EXT("must specify server when specifying partition");
	}
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
	vol_ptr = &volume_id;
    }

    if (!vos_VLDBEntryRemove
	(cellHandle, vos_server, 0, part_ptr, vol_ptr, &st)) {
	ERR_ST_EXT("vos_VLDBEntryRemove", st);
    }

    return 0;
}

int
DoVosVLDBUnlock(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION } DoVosVLDBUnlock_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int *part_ptr = NULL;
    int have_server = 0;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
	have_server = 1;
    }

    if (as->parms[PARTITION].items) {
	if (!have_server) {
	    ERR_EXT("must specify server when specifying partition");
	}
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (!vos_VLDBUnlock(cellHandle, vos_server, 0, part_ptr, &st)) {
	ERR_ST_EXT("vos_VLDBUnlock", st);
    }

    return 0;
}

int
DoVosVLDBEntryLock(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME } DoVosVLDBEntryLoc_parm_tk;
    afs_status_t st = 0;
    unsigned int volume_id;

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VLDBEntryLock(cellHandle, 0, volume_id, &st)) {
	ERR_ST_EXT("vos_VLDBEntryLock", st);
    }

    return 0;
}

int
DoVosVLDBEntryUnlock(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME } DoVosVLDBEntryUnlock_parm_t;
    afs_status_t st = 0;
    unsigned int volume_id;

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VLDBEntryUnlock(cellHandle, 0, volume_id, &st)) {
	ERR_ST_EXT("vos_VLDBEntryUnlock", st);
    }

    return 0;
}

int
DoVosVLDBReadOnlySiteCreate(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION,
	VOLUME
    } DoVosVLDBReadOnlySiteCreate_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VLDBReadOnlySiteCreate
	(cellHandle, vos_server, 0, partition_id, volume_id, &st)) {
	ERR_ST_EXT("vos_VLDBReadOnlySiteCreate", st);
    }
    return 0;
}

int
DoVosVLDBReadOnlySiteDelete(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION,
	VOLUME
    } DoVosVLDBReadOnlySiteDelete_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VLDBReadOnlySiteDelete
	(cellHandle, vos_server, 0, partition_id, volume_id, &st)) {
	ERR_ST_EXT("vos_VLDBReadOnlySiteDelete", st);
    }

    return 0;
}

int
DoVosVLDBSync(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, FORCE } DoVosVLDBSync_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int *part_ptr = NULL;
    int have_server = 0;
    vos_force_t force = VOS_NORMAL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
	have_server = 1;
    }

    if (as->parms[PARTITION].items) {
	if (!have_server) {
	    ERR_EXT("must specify server when specifying partition");
	}
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (as->parms[FORCE].items) {
	force = VOS_FORCE;
    }

    if (!vos_VLDBSync(cellHandle, vos_server, 0, part_ptr, force, &st)) {
	ERR_ST_EXT("vos_VLDBSync", st);
    }

    return 0;
}

int
DoVosVolumeCreate(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME,
	QUOTA
    } DoVosVolumeCreate_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    const char *volume = NULL;
    unsigned int quota;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	volume = as->parms[VOLUME].items->data;
    }

    if (as->parms[QUOTA].items) {
	quota =
	    GetIntFromString(as->parms[QUOTA].items->data, "invalid quota");
    }

    if (!vos_VolumeCreate
	(cellHandle, vos_server, 0, partition_id, volume, quota, &volume_id,
	 &st)) {
	ERR_ST_EXT("vos_VolumeCreate", st);
    }

    printf("Created volume %u\n", volume_id);

    return 0;
}

int
DoVosVolumeDelete(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVolumeDelete_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VolumeDelete
	(cellHandle, vos_server, 0, partition_id, volume_id, &st)) {
	ERR_ST_EXT("vos_VolumeDelete", st);
    }

    return 0;
}

int
DoVosVolumeRename(struct cmd_syndesc *as, char *arock)
{
    typedef enum { OLDVOLUME, NEWVOLUME } DoVosVolumeRename_parm_t;
    afs_status_t st = 0;
    unsigned int old_volume;
    const char *new_volume;

    if (as->parms[OLDVOLUME].items) {
	const char *volume = as->parms[OLDVOLUME].items->data;
	old_volume = GetVolumeIdFromString(volume);
    }

    if (as->parms[NEWVOLUME].items) {
	new_volume = as->parms[NEWVOLUME].items->data;
    }

    if (!vos_VolumeRename(cellHandle, 0, old_volume, new_volume, &st)) {
	ERR_ST_EXT("vos_VolumeRename", st);
    }

    return 0;
}

int
DoVosVolumeDump(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME, STARTTIME,
	DUMPFILE
    } DoVosVolumeDump_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int *part_ptr = NULL;
    int have_server = 0;
    unsigned int volume_id;
    unsigned int start_time;
    const char *dumpfile;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
	have_server = 1;
    }

    if (as->parms[PARTITION].items) {
	if (!have_server) {
	    ERR_EXT("must specify server when specifying partition");
	}
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
	part_ptr = &partition_id;
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (as->parms[STARTTIME].items) {
	start_time =
	    GetIntFromString(as->parms[STARTTIME].items->data,
			     "invalid start time");
    }

    if (as->parms[DUMPFILE].items) {
	dumpfile = as->parms[DUMPFILE].items->data;
    }

    if (!vos_VolumeDump
	(cellHandle, vos_server, 0, part_ptr, volume_id, start_time, dumpfile,
	 &st)) {
	ERR_ST_EXT("vos_VolumeDump", st);
    }

    return 0;
}

int
DoVosVolumeRestore(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, ID, VOLUME, DUMPFILE,
	FULL
    } DoVosVolumeRestore_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    unsigned int *vol_ptr = NULL;
    const char *dumpfile;
    const char *volume_name;
    vos_volumeRestoreType_t restore = VOS_RESTORE_INCREMENTAL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	volume_name = as->parms[VOLUME].items->data;
    }

    if (as->parms[ID].items) {
	const char *volume = as->parms[ID].items->data;
	volume_id = GetVolumeIdFromString(volume);
	vol_ptr = &volume_id;
    }

    if (as->parms[DUMPFILE].items) {
	dumpfile = as->parms[DUMPFILE].items->data;
    }

    if (as->parms[FULL].items) {
	restore = VOS_RESTORE_FULL;
    }

    if (!vos_VolumeRestore
	(cellHandle, vos_server, 0, partition_id, vol_ptr, volume_name,
	 dumpfile, restore, &st)) {
	ERR_ST_EXT("vos_VolumeRestore", st);
    }

    return 0;
}

int
DoVosVolumeOnline(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME, SLEEP,
	BUSY
    } DoVosVolumeOnline_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    unsigned int sleep;
    vos_volumeOnlineType_t type = VOS_ONLINE_OFFLINE;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (as->parms[SLEEP].items) {
	sleep =
	    GetIntFromString(as->parms[SLEEP].items->data,
			     "invalid sleep time");
    }

    if (as->parms[BUSY].items) {
	type = VOS_ONLINE_BUSY;
    }

    if (!vos_VolumeOnline
	(vos_server, 0, partition_id, volume_id, sleep, type, &st)) {
	ERR_ST_EXT("vos_VolumeOnline", st);
    }

    return 0;
}

int
DoVosVolumeOffline(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVolumeOffline_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VolumeOffline(vos_server, 0, partition_id, volume_id, &st)) {
	ERR_ST_EXT("vos_VolumeOffline", st);
    }

    return 0;
}

static void
Print_vos_volumeEntry_p(vos_volumeEntry_p entry, const char *prefix)
{
    if (entry->status == VOS_OK) {
	printf("%sVolume name %s id %u\n", prefix, entry->name, entry->id);
	printf("%sRead write id %u\n", prefix, entry->readWriteId);
	printf("%sRead only id %u\n", prefix, entry->readOnlyId);
	printf("%sBackup id %u\n", prefix, entry->backupId);
	printf("%sCreation date %lu\n", prefix, entry->creationDate);
	printf("%sLast access date %lu\n", prefix, entry->lastAccessDate);
	printf("%sLast update date %lu\n", prefix, entry->lastUpdateDate);
	printf("%sLast backup date %lu\n", prefix, entry->lastBackupDate);
	printf("%sLast copy creation date %lu\n", prefix,
	       entry->copyCreationDate);
	printf("%sAccesses since midnight %d\n", prefix,
	       entry->accessesSinceMidnight);
	printf("%sFile count %d\n", prefix, entry->fileCount);
	printf("%sMax quota %d\n", prefix, entry->maxQuota);
	printf("%sCurrent size %d\n", prefix, entry->currentSize);

	printf("%sVolume status: VOS_OK\n", prefix);

	printf("%sVolume disposition:\n", prefix);

	switch (entry->volumeDisposition) {
	case VOS_OK:
	    printf("%s\tVOS_OK\n", prefix);
	    break;
	case VOS_SALVAGE:
	    printf("%s\tVOS_SALVAGE\n", prefix);
	    break;
	case VOS_NO_VNODE:
	    printf("%s\tVOS_NO_VNODE\n", prefix);
	    break;
	case VOS_NO_VOL:
	    printf("%s\tVOS_NO_VOL\n", prefix);
	    break;
	case VOS_VOL_EXISTS:
	    printf("%s\tVOS_VOL_EXISTS\n", prefix);
	    break;
	case VOS_NO_SERVICE:
	    printf("%s\tVOS_NO_SERVICE\n", prefix);
	    break;
	case VOS_OFFLINE:
	    printf("%s\tVOS_OFFLINE\n", prefix);
	    break;
	case VOS_ONLINE:
	    printf("%s\tVOS_ONLINE\n", prefix);
	    break;
	case VOS_DISK_FULL:
	    printf("%s\tVOS_DISK_FULL\n", prefix);
	    break;
	case VOS_OVER_QUOTA:
	    printf("%s\tVOS_OVER_QUOTA\n", prefix);
	    break;
	case VOS_BUSY:
	    printf("%s\tVOS_BUSY\n", prefix);
	    break;
	case VOS_MOVED:
	    printf("%s\tVOS_MOVED\n", prefix);
	    break;
	default:
	    printf("Unknown volume disposition %d\n",
		   entry->volumeDisposition);
	    break;
	}

	printf("%sVolume type: ", prefix);

	if (entry->type == VOS_READ_WRITE_VOLUME) {
	    printf("read write\n");
	} else if (entry->type == VOS_READ_ONLY_VOLUME) {
	    printf("read only\n");
	} else {
	    printf("backup\n");
	}

	printf("\n%s\tSame Network\tSame Network Authenticated"
	       "\tDifferent Network\tDifferent Network Authenticated\n\n",
	       prefix);
	printf("%sRead\t%d\t%d\t%d\t%d\n", prefix,
	       entry->readStats[VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK],
	       entry->
	       readStats
	       [VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK_AUTHENTICATED],
	       entry->
	       readStats[VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK],
	       entry->
	       readStats
	       [VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK_AUTHENTICATED]);
	printf("%sWrite\t%d\t%d\t%d\t%d\n", prefix,
	       entry->writeStats[VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK],
	       entry->
	       writeStats
	       [VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK_AUTHENTICATED],
	       entry->
	       writeStats[VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK],
	       entry->
	       writeStats
	       [VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK_AUTHENTICATED]);

	printf
	    ("\n%s\t0 to 60 secs\t1 to 10 mins\t10 to 60 mins\t1 to 24 hrs\t1 to 7 days\t more than 7 days\n",
	     prefix);
	printf("%sFile Author Write Same Network\t%d\t%d\t%d\t%d\t%d\t%d\n",
	       prefix,
	       entry->
	       fileAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_0_TO_60_SECONDS],
	       entry->
	       fileAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_10_MINUTES],
	       entry->
	       fileAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_10_TO_60_MINUTES],
	       entry->
	       fileAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_24_HOURS],
	       entry->
	       fileAuthorWriteSameNetwork[VOS_VOLUME_TIME_STATS_1_TO_7_DAYS],
	       entry->
	       fileAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_GREATER_THAN_7_DAYS]);
	printf("%sFile Author Write Diff Network\t%d\t%d\t%d\t%d\t%d\t%d\n",
	       prefix,
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_0_TO_60_SECONDS],
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_10_MINUTES],
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_10_TO_60_MINUTES],
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_24_HOURS],
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_7_DAYS],
	       entry->
	       fileAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_GREATER_THAN_7_DAYS]);
	printf("%sDir Author Write Same Network\t%d\t%d\t%d\t%d\t%d\t%d\n",
	       prefix,
	       entry->
	       dirAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_0_TO_60_SECONDS],
	       entry->
	       dirAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_10_MINUTES],
	       entry->
	       dirAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_10_TO_60_MINUTES],
	       entry->
	       dirAuthorWriteSameNetwork[VOS_VOLUME_TIME_STATS_1_TO_24_HOURS],
	       entry->
	       dirAuthorWriteSameNetwork[VOS_VOLUME_TIME_STATS_1_TO_7_DAYS],
	       entry->
	       dirAuthorWriteSameNetwork
	       [VOS_VOLUME_TIME_STATS_GREATER_THAN_7_DAYS]);
	printf("%sDir Author Write Diff Network\t%d\t%d\t%d\t%d\t%d\t%d\n",
	       prefix,
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_0_TO_60_SECONDS],
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_10_MINUTES],
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_10_TO_60_MINUTES],
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_24_HOURS],
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_1_TO_7_DAYS],
	       entry->
	       dirAuthorWriteDifferentNetwork
	       [VOS_VOLUME_TIME_STATS_GREATER_THAN_7_DAYS]);
    } else {
	printf("%sUnable to print volume because volume status:\n", prefix);
	switch (entry->status) {
	case VOS_SALVAGE:
	    printf("%s\tVOS_SALVAGE\n", prefix);
	    break;
	case VOS_NO_VNODE:
	    printf("%s\tVOS_NO_VNODE\n", prefix);
	    break;
	case VOS_NO_VOL:
	    printf("%s\tVOS_NO_VOL\n", prefix);
	    break;
	case VOS_VOL_EXISTS:
	    printf("%s\tVOS_VOL_EXISTS\n", prefix);
	    break;
	case VOS_NO_SERVICE:
	    printf("%s\tVOS_NO_SERVICE\n", prefix);
	    break;
	case VOS_OFFLINE:
	    printf("%s\tVOS_OFFLINE\n", prefix);
	    break;
	case VOS_ONLINE:
	    printf("%s\tVOS_ONLINE\n", prefix);
	    break;
	case VOS_DISK_FULL:
	    printf("%s\tVOS_DISK_FULL\n", prefix);
	    break;
	case VOS_OVER_QUOTA:
	    printf("%s\tVOS_OVER_QUOTA\n", prefix);
	    break;
	case VOS_BUSY:
	    printf("%s\tVOS_BUSY\n", prefix);
	    break;
	case VOS_MOVED:
	    printf("%s\tVOS_MOVED\n", prefix);
	    break;
	default:
	    printf("Unknown volume status %d\n", entry->status);
	    break;
	}
    }
}

int
DoVosVolumeGet(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVolumeGet_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    vos_volumeEntry_t entry;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VolumeGet
	(cellHandle, vos_server, 0, partition_id, volume_id, &entry, &st)) {
	ERR_ST_EXT("vos_VolumeGet", st);
    }

    Print_vos_volumeEntry_p(&entry, "");

    return 0;
}

int
DoVosVolumeList(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION } DoVosVolumeList_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    void *iter = NULL;
    unsigned int partition_id;
    vos_volumeEntry_t entry;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (!vos_VolumeGetBegin
	(cellHandle, vos_server, 0, partition_id, &iter, &st)) {
	ERR_ST_EXT("vos_VolumeGetBegin", st);
    }

    printf("Volumes located at %s partition %s\n",
	   as->parms[SERVER].items->data, as->parms[PARTITION].items->data);

    while (vos_VolumeGetNext(iter, &entry, &st)) {
	Print_vos_volumeEntry_p(&entry, "    ");
	printf("\n");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("vos_VolumeGetNext", st);
    }

    if (!vos_VolumeGetDone(iter, &st)) {
	ERR_ST_EXT("vos_VolumeGetDone", st);
    }

    return 0;
}

int
DoVosVolumeMove(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME, FROMSERVER, FROMPARTITION, TOSERVER,
	TOPARTITION
    } DoVosVolumeMove_parm_t;
    afs_status_t st = 0;
    void *from_server = NULL;
    void *to_server = NULL;
    unsigned int from_partition;
    unsigned int to_partition;
    unsigned int volume_id;

    if (as->parms[FROMSERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[FROMSERVER].items->data, &from_server,
	     &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[TOSERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[TOSERVER].items->data, &to_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[FROMPARTITION].items) {
	from_partition =
	    GetPartitionIdFromString(as->parms[FROMPARTITION].items->data);
    }

    if (as->parms[TOPARTITION].items) {
	to_partition =
	    GetPartitionIdFromString(as->parms[TOPARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_VolumeMove
	(cellHandle, 0, volume_id, from_server, from_partition, to_server,
	 to_partition, &st)) {
	ERR_ST_EXT("vos_VolumeMove", st);
    }

    return 0;
}

int
DoVosVolumeRelease(struct cmd_syndesc *as, char *arock)
{
    typedef enum { VOLUME, FORCE } DoVosVolumeRelease_parm_t;
    afs_status_t st = 0;
    unsigned int volume_id;
    vos_force_t force = VOS_NORMAL;

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (as->parms[FORCE].items) {
	force = VOS_FORCE;
    }

    if (!vos_VolumeRelease(cellHandle, 0, volume_id, force, &st)) {
	ERR_ST_EXT("vos_VolumeRelease", st);
    }
    return 0;
}

int
DoVosVolumeZap(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME, FORCE } DoVosVolumeZap_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    vos_force_t force = VOS_NORMAL;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (as->parms[FORCE].items) {
	force = VOS_FORCE;
    }

    if (!vos_VolumeZap
	(cellHandle, vos_server, 0, partition_id, volume_id, force, &st)) {
	ERR_ST_EXT("vos_VolumeZap", st);
    }

    return 0;
}

int
DoVosPartitionNameToId(struct cmd_syndesc *as, char *arock)
{
    typedef enum { PARTITION } DoVosPartitionNameToId_parm_t;
    afs_status_t st = 0;
    unsigned int partition_id;

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    printf("The id for partition %s is %u\n",
	   as->parms[PARTITION].items->data, partition_id);

    return 0;
}

int
DoVosPartitionIdToName(struct cmd_syndesc *as, char *arock)
{
    typedef enum { PARTITIONID } DoVosPartitionIdToName_parm_t;
    afs_status_t st = 0;
    unsigned int partition_id;
    char partition[VOS_MAX_PARTITION_NAME_LEN];

    if (as->parms[PARTITIONID].items) {
	partition_id =
	    GetIntFromString(as->parms[PARTITIONID].items->data,
			     "bad partition id");
    }

    if (!vos_PartitionIdToName(partition_id, partition, &st)) {
	ERR_ST_EXT("bad partition id", st);
    }

    printf("The partition for id %u is %s\n", partition_id, partition);

    return 0;
}

int
DoVosVolumeQuotaChange(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME,
	QUOTA
    } DoVosVolumeQuotaChange_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;
    unsigned int quota;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (as->parms[QUOTA].items) {
	quota =
	    GetIntFromString(as->parms[QUOTA].items->data, "invalid quota");
    }

    if (!vos_VolumeQuotaChange
	(cellHandle, vos_server, 0, partition_id, volume_id, quota, &st)) {
	ERR_ST_EXT("vos_VolumeQuotaChange", st);
    }

    return 0;
}

/*
 * Parse a server name/address and return the address in HOST BYTE order
 */
static afs_uint32
GetServer(char *aname)
{
    register struct hostent *th;
    afs_uint32 addr;
    int b1, b2, b3, b4;
    register afs_int32 code;
    char hostname[MAXHOSTCHARS];

    code = sscanf(aname, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	addr = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
	addr = ntohl(addr);	/* convert to host order */
    } else {
	th = gethostbyname(aname);
	if (!th)
	    return 0;
	memcpy(&addr, th->h_addr, sizeof(addr));
    }

    if (addr == htonl(0x7f000001)) {	/* local host */
	code = gethostname(hostname, MAXHOSTCHARS);
	if (code)
	    return 0;
	th = gethostbyname(hostname);	/* returns host byte order */
	if (!th)
	    return 0;
	memcpy(&addr, th->h_addr, sizeof(addr));
    }

    return (addr);
}

static void
Print_vos_volintInfo(afs_uint32 server, afs_uint32 partition, volintInfo* pinfo, const char *prefix)
{
    static afs_uint32 server_cache;
    static int cache_valid = 0;
    static char hostname[256], address[32];

    if (!cache_valid || server != server_cache) {
	struct in_addr s;

	s.s_addr = server;
	strcpy(hostname, hostutil_GetNameByINet(server));
	strcpy(address, inet_ntoa(s));
	server_cache = server;
	cache_valid = 1;
    }
    
    
    printf("%sname\t\t%s\n",prefix, pinfo->name);
    printf("%sid\t\t%lu\n",prefix, pinfo->volid);
    printf("%sserv\t\t%s\t%s\n",prefix, address,hostname);
    printf("%spart\t\t%u\n", prefix,partition);
    
    switch (pinfo->status) {
    case 2: /* VOK */
	printf("%sstatus\t\tOK\n",prefix);
	break;
    case 101: /* VBUSY */
	printf("%sstatus\t\tBUSY\n",prefix);
	return;
    default:
	printf("%sstatus\t\tUNATTACHABLE\n",prefix);
	return;
    }
    printf("%sbackupID\t%lu\n",prefix, pinfo->backupID);
    printf("%sparentID\t%lu\n",prefix, pinfo->parentID);
    printf("%scloneID\t%lu\n",prefix, pinfo->cloneID);
    printf("%sinUse\t\t%s\n",prefix, pinfo->inUse ? "Y" : "N");
    printf("%sneedsSalvaged\t%s\n",prefix, pinfo->needsSalvaged ? "Y" : "N");
    /* 0xD3 is from afs/volume.h since I had trouble including the file */
    printf("%sdestroyMe\t%s\n",prefix, pinfo->destroyMe == 0xD3 ? "Y" : "N");
    switch (pinfo->type) {
    case 0:
	printf("%stype\t\tRW\n",prefix);
	break;
    case 1:
	printf("%stype\t\tRO\n",prefix);
	break;
    case 2:
	printf("%stype\t\tBK\n",prefix);
	break;
    default:
	printf("%stype\t\t?\n",prefix);
	break;
    }
    printf("%screationDate\t%-9lu\n", prefix,pinfo->creationDate);
    printf("%saccessDate\t%-9lu\n", prefix,pinfo->accessDate);
    printf("%supdateDate\t%-9lu\n", prefix,pinfo->updateDate);
    printf("%sbackupDate\t%-9lu\n", prefix,pinfo->backupDate);
    printf("%scopyDate\t%-9lu\n", prefix,pinfo->copyDate);
	    
    printf("%sflags\t\t%#lx\t(Optional)\n",prefix, pinfo->flags);
    printf("%sdiskused\t%u\n",prefix, pinfo->size);
    printf("%smaxquota\t%u\n",prefix, pinfo->maxquota);
    printf("%sminquota\t%lu\t(Optional)\n",prefix, pinfo->spare0);
    printf("%sfilecount\t%u\n",prefix, pinfo->filecount);
    printf("%sdayUse\t\t%u\n",prefix, pinfo->dayUse);
    printf("%sweekUse\t%lu\t(Optional)\n",prefix, pinfo->spare1);
    printf("%svolUpdateCounter\t\t%lu\t(Optional)\n",prefix, pinfo->spare2);
    printf("%sspare3\t\t%lu\t(Optional)\n",prefix, pinfo->spare3);
}

int
DoVosVolumeGet2(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVolumeGet_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    afs_uint32 partition_id;
    afs_uint32 volume_id;

	volintInfo info;
	memset(&info, 0, sizeof(struct volintInfo));

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }
    

	if (!vos_VolumeGet2
	(cellHandle, vos_server, 0, partition_id, volume_id, &info, &st)) {
	ERR_ST_EXT("vos_VolumeGet2", st);
    }


    Print_vos_volintInfo(GetServer(as->parms[SERVER].items->data),partition_id,&info," ");

    return 0;
}


int
DoVos_ClearVolUpdateCounter(struct cmd_syndesc *as, char *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME } DoVosVolumeGet_parm_t;
    afs_status_t st = 0;
    void *vos_server = NULL;
    unsigned int partition_id;
    unsigned int volume_id;

    if (as->parms[SERVER].items) {
	if (!vos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &vos_server, &st)) {
	    ERR_ST_EXT("vos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition_id =
	    GetPartitionIdFromString(as->parms[PARTITION].items->data);
    }

    if (as->parms[VOLUME].items) {
	const char *volume = as->parms[VOLUME].items->data;
	volume_id = GetVolumeIdFromString(volume);
    }

    if (!vos_ClearVolUpdateCounter
	(cellHandle, vos_server,partition_id, volume_id, &st)) {
	ERR_ST_EXT("vos_ClearVolUpdateCounter", st);
    }

    return 0;
}

void
SetupVosAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("VosBackupVolumeCreate", DoVosBackupVolumeCreate, 0,
			  "create a backup volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to back up");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosBackupVolumeCreateMultiple",
			  DoVosBackupVolumeCreateMultiple, 0,
			  "create a backup volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"server housing volumes to back up");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"partition housing volumes to back up");
    cmd_AddParm(ts, "-prefix", CMD_SINGLE, CMD_OPTIONAL,
		"common prefix of volumes to back up");
    cmd_AddParm(ts, "-exclude", CMD_FLAG, CMD_OPTIONAL,
		"exclude volumes from backup that match prefix");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosPartitionGet", DoVosPartitionGet, 0,
			  "get information about a partition");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server housing partition of interest");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosPartitionList", DoVosPartitionList, 0,
			  "list information about all partitions at a server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server housing partitions of interest");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosServerSync", DoVosServerSync, 0,
			  "sync server with vldb");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to sync");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"partition to sync");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosFileServerAddressChange",
			  DoVosFileServerAddressChange, 0,
			  "change a server's address in the vldb");
    cmd_AddParm(ts, "-oldaddress", CMD_SINGLE, CMD_REQUIRED,
		"old address to change");
    cmd_AddParm(ts, "-newaddress", CMD_SINGLE, CMD_REQUIRED, "new address");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosFileServerAddressRemove",
			  DoVosFileServerAddressRemove, 0,
			  "remove a server's address from the vldb");
    cmd_AddParm(ts, "-address", CMD_SINGLE, CMD_REQUIRED,
		"address to remove");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosFileServerList", DoVosFileServerList, 0,
			  "list the file servers in a cell");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosServerTransactionStatusList",
			  DoVosServerTransactionStatusList, 0,
			  "list the active transactions at a server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBGet", DoVosVLDBGet, 0,
			  "get a vldb entry for a volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to retrieve");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBList", DoVosVLDBList, 0,
			  "list a group of vldb entries");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular partition");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBEntryRemove", DoVosVLDBEntryRemove, 0,
			  "remove vldb entries");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular partition");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_OPTIONAL, "volume to remove");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBUnlock", DoVosVLDBUnlock, 0,
			  "unlock a group of vldb entries");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"limit entries to a particular partition");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBEntryLock", DoVosVLDBList, 0,
			  "lock a single vldb entry");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to lock");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBEntryUnlock", DoVosVLDBEntryUnlock, 0,
			  "unlock a single vldb entry");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to unlock");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBReadOnlySiteCreate",
			  DoVosVLDBReadOnlySiteCreate, 0,
			  "create a read only site");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where read only will be created");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition where read only will be created");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to replicate");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBReadOnlySiteDelete",
			  DoVosVLDBReadOnlySiteDelete, 0,
			  "delete a read only site before initial replication");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"server where read only will be deleted");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"partition where read only will be deleted");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVLDBSync", DoVosVLDBSync, 0,
			  "sync vldb with server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to sync");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"limit sync to a particular partition");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "force sync to occur");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeCreate", DoVosVolumeCreate, 0,
			  "create a read write volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where volume will be created");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition where volume will be created");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"name of new volume");
    cmd_AddParm(ts, "-quota", CMD_SINGLE, CMD_REQUIRED,
		"size quota of new volume in 1kb units");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeDelete", DoVosVolumeDelete, 0,
			  "delete a volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where volume exists");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition where volume exists");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeRename", DoVosVolumeRename, 0,
			  "rename a volume");
    cmd_AddParm(ts, "-oldname", CMD_SINGLE, CMD_REQUIRED, "old volume name");
    cmd_AddParm(ts, "-newname", CMD_SINGLE, CMD_REQUIRED, "new volume name");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeDump", DoVosVolumeDump, 0,
			  "dump a volume to a file");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"dump volume at a particular server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"dump volume at a particular partition");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to dump");
    cmd_AddParm(ts, "-starttime", CMD_SINGLE, CMD_REQUIRED,
		"files modified after this time will be dumped");
    cmd_AddParm(ts, "-dumpfile", CMD_SINGLE, CMD_REQUIRED,
		"file to contain dump results");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeRestore", DoVosVolumeRestore, 0,
			  "restore a volume from a dumpfile");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume to restore");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume to restore");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_OPTIONAL, "id of volume restored");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"name of volume restored");
    cmd_AddParm(ts, "-dumpfile", CMD_SINGLE, CMD_REQUIRED,
		"file contained dump of volume");
    cmd_AddParm(ts, "-full", CMD_FLAG, CMD_OPTIONAL,
		"does a full restore of volume");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeOnline", DoVosVolumeOnline, 0,
			  "bring a volume online");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to bring online");
    cmd_AddParm(ts, "-sleep", CMD_SINGLE, CMD_REQUIRED, "seconds to sleep");
    cmd_AddParm(ts, "-busy", CMD_FLAG, CMD_OPTIONAL, "mark volume busy");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeOffline", DoVosVolumeOffline, 0,
			  "take a volume offline");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to bring offline");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeGet", DoVosVolumeGet, 0,
			  "get a volume entry");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to retrieve");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeList", DoVosVolumeList, 0,
			  "list a group of volumes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"limit volumes to a particular server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"limit volumes to a particular partition");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeMove", DoVosVolumeMove, 0,
			  "move a volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to move");
    cmd_AddParm(ts, "-fromserver", CMD_SINGLE, CMD_REQUIRED, "source server");
    cmd_AddParm(ts, "-frompartition", CMD_SINGLE, CMD_REQUIRED,
		"source partition");
    cmd_AddParm(ts, "-toserver", CMD_SINGLE, CMD_REQUIRED,
		"destination server");
    cmd_AddParm(ts, "-topartition", CMD_SINGLE, CMD_REQUIRED,
		"destination partition");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeRelease", DoVosVolumeRelease, 0,
			  "release updates to read only");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to replicate");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"force release to occur");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeZap", DoVosVolumeZap, 0, "zap a volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses the volume to zap");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses the volume to zap");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to zap");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "force zap");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosPartitionNameToId", DoVosPartitionNameToId, 0,
			  "convert a partition name to a number");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition to convert");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosPartitionIdToName", DoVosPartitionIdToName, 0,
			  "convert a number to a partition");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_REQUIRED, "number to convert");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeQuotaChange", DoVosVolumeQuotaChange, 0,
			  "change the quota for a partition");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses the volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses the volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume to change");
    cmd_AddParm(ts, "-quota", CMD_SINGLE, CMD_REQUIRED,
		"new quota in 1kb units");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("VosVolumeGet2", DoVosVolumeGet2, 0,
			  "get a volume entry");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume to retrieve");
    SetupCommonCmdArgs(ts);
    
    ts = cmd_CreateSyntax("ClearVolUpdateCounter", DoVos_ClearVolUpdateCounter, 0,
			  "clear volUpdateCounter");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server that houses volume");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition that houses volume");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"volume");
    SetupCommonCmdArgs(ts);

}

