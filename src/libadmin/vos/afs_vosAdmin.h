/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_VOS_ADMIN_H
#define OPENAFS_VOS_ADMIN_H

#include <afs/param.h>
#include <afs/afs_Admin.h>
#include <sys/types.h>
#include <afs/volint.h>
#ifdef AFS_NT40_ENV
#ifndef _MFC_VER
#include <winsock2.h>
#endif /* _MFC_VER */
#else
#include <sys/socket.h>
#endif

#define VOS_MAX_PARTITION_NAME_LEN 32
#define VOS_MAX_VOLUME_NAME_LEN 32
#define VOS_MAX_VOLUME_TYPES 3
#define VOS_MAX_REPLICA_SITES 13
#define VOS_MAX_SERVER_ADDRESS 16

typedef enum {
    VOS_NORMAL,
    VOS_FORCE
} vos_force_t, *vos_force_p;

typedef enum {
    VOS_INCLUDE,
    VOS_EXCLUDE
} vos_exclude_t, *vos_exclude_p;

typedef enum {
    VOS_OK,
    VOS_SALVAGE,
    VOS_NO_VNODE,
    VOS_NO_VOL,
    VOS_VOL_EXISTS,
    VOS_NO_SERVICE,
    VOS_OFFLINE,
    VOS_ONLINE,
    VOS_DISK_FULL,
    VOS_OVER_QUOTA,
    VOS_BUSY,
    VOS_MOVED
} vos_volumeStatus_t, *vos_volumeStatus_p;

typedef enum {
    VOS_READ_WRITE_VOLUME,
    VOS_READ_ONLY_VOLUME,
    VOS_BACKUP_VOLUME
} vos_volumeType_t, *vos_volumeType_p;

typedef struct vos_fileServerEntry {
    int serverAddress[VOS_MAX_SERVER_ADDRESS];
    int count;
} vos_fileServerEntry_t, *vos_fileServerEntry_p;

typedef enum {
    VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK,
    VOS_VOLUME_READ_WRITE_STATS_SAME_NETWORK_AUTHENTICATED,
    VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK,
    VOS_VOLUME_READ_WRITE_STATS_DIFFERENT_NETWORK_AUTHENTICATED
} vos_volumeReadWriteStats_t, *vos_volumeReadWriteStats_p;

#define VOS_VOLUME_READ_WRITE_STATS_NUMBER 4

typedef enum {
    VOS_VOLUME_TIME_STATS_0_TO_60_SECONDS,
    VOS_VOLUME_TIME_STATS_1_TO_10_MINUTES,
    VOS_VOLUME_TIME_STATS_10_TO_60_MINUTES,
    VOS_VOLUME_TIME_STATS_1_TO_24_HOURS,
    VOS_VOLUME_TIME_STATS_1_TO_7_DAYS,
    VOS_VOLUME_TIME_STATS_GREATER_THAN_7_DAYS
} vos_volumeTimeStats_t, *vos_volumeTimeStats_p;

#define VOS_VOLUME_TIME_STATS_NUMBER 6

typedef struct vos_volumeEntry {
    unsigned int id;
    unsigned int readWriteId;
    unsigned int readOnlyId;
    unsigned int backupId;
    unsigned long creationDate;
    unsigned long lastAccessDate;
    unsigned long lastUpdateDate;
    unsigned long lastBackupDate;
    unsigned long copyCreationDate;
    int accessesSinceMidnight;
    int fileCount;
    int maxQuota;
    int currentSize;
    int readStats[VOS_VOLUME_READ_WRITE_STATS_NUMBER];
    int writeStats[VOS_VOLUME_READ_WRITE_STATS_NUMBER];
    int fileAuthorWriteSameNetwork[VOS_VOLUME_TIME_STATS_NUMBER];
    int fileAuthorWriteDifferentNetwork[VOS_VOLUME_TIME_STATS_NUMBER];
    int dirAuthorWriteSameNetwork[VOS_VOLUME_TIME_STATS_NUMBER];
    int dirAuthorWriteDifferentNetwork[VOS_VOLUME_TIME_STATS_NUMBER];
    vos_volumeStatus_t status;
    vos_volumeStatus_t volumeDisposition;
    vos_volumeType_t type;
    char name[VOS_MAX_VOLUME_NAME_LEN];
} vos_volumeEntry_t, *vos_volumeEntry_p;

typedef struct vos_partitionEntry {
    char name[VOS_MAX_PARTITION_NAME_LEN];
    char deviceName[VOS_MAX_PARTITION_NAME_LEN];
    int lockFileDescriptor;
    int totalSpace;
    int totalFreeSpace;
} vos_partitionEntry_t, *vos_partitionEntry_p;

typedef enum {
    VOS_VLDB_ENTRY_OK = 0x1,
    VOS_VLDB_ENTRY_MOVE = 0x2,
    VOS_VLDB_ENTRY_RELEASE = 0x4,
    VOS_VLDB_ENTRY_BACKUP = 0x8,
    VOS_VLDB_ENTRY_DELETE = 0x10,
    VOS_VLDB_ENTRY_DUMP = 0x20,
    VOS_VLDB_ENTRY_LOCKED = 0x40,
    VOS_VLDB_ENTRY_RWEXISTS = 0x1000,
    VOS_VLDB_ENTRY_ROEXISTS = 0x2000,
    VOS_VLDB_ENTRY_BACKEXISTS = 0x4000
} vos_vldbEntryStatus_t, *vos_vldbEntryStatus_p;

typedef enum {
    VOS_VLDB_NEW_REPSITE = 0x1,
    VOS_VLDB_READ_ONLY = 0x2,
    VOS_VLDB_READ_WRITE = 0x4,
    VOS_VLDB_BACKUP = 0x8,
    VOS_VLDB_DONT_USE = 0x10
} vos_vldbServerFlag_t, *vos_vldbServerFlag_p;

typedef struct vos_vldbEntry {
    int numServers;
    unsigned int volumeId[VOS_MAX_VOLUME_TYPES];
    unsigned int cloneId;
    vos_vldbEntryStatus_t status;
    struct {
	int serverAddress;
	int serverPartition;
	vos_vldbServerFlag_t serverFlags;
    } volumeSites[VOS_MAX_REPLICA_SITES];
    char name[VOS_MAX_VOLUME_NAME_LEN];
} vos_vldbEntry_t, *vos_vldbEntry_p;

#define VOS_PROCEDURE_NAME_LEN 30

typedef enum {
    VOS_VOLUME_ATTACH_MODE_OK,
    VOS_VOLUME_ATTACH_MODE_OFFLINE,
    VOS_VOLUME_ATTACH_MODE_BUSY,
    VOS_VOLUME_ATTACH_MODE_READONLY,
    VOS_VOLUME_ATTACH_MODE_CREATE,
    VOS_VOLUME_ATTACH_MODE_CREATE_VOLID
} vol_volumeAttachMode_t, *vol_volumeAttachMode_p;

typedef enum {
    VOS_VOLUME_ACTIVE_STATUS_OK,
    VOS_VOLUME_ACTIVE_STATUS_DELETE_ON_SALVAGE,
    VOS_VOLUME_ACTIVE_STATUS_OUT_OF_SERVICE,
    VOS_VOLUME_ACTIVE_STATUS_DELETED
} vos_volumeActiveStatus_t, *vos_volumeActiveStatus_p;

typedef enum {
    VOS_VOLUME_TRANSACTION_STATUS_OK,
    VOS_VOLUME_TRANSACTION_STATUS_DELETED
} vos_volumeTransactionStatus_t, *vos_volumeTransactionStatus_p;

typedef struct vos_serverTransactionStatus {
    int transactionId;
    int lastActiveTime;
    int creationTime;
    int errorCode;
    unsigned int volumeId;
    int partition;
    char lastProcedureName[VOS_PROCEDURE_NAME_LEN];
    int nextReceivePacketSequenceNumber;
    int nextSendPacketSequenceNumber;
    int lastReceiveTime;
    int lastSendTime;
    vol_volumeAttachMode_t volumeAttachMode;
    vos_volumeActiveStatus_t volumeActiveStatus;
    vos_volumeTransactionStatus_t volumeTransactionStatus;
} vos_serverTransactionStatus_t, *vos_serverTransactionStatus_p;

typedef enum {
    VOS_RESTORE_FULL,
    VOS_RESTORE_INCREMENTAL
} vos_volumeRestoreType_t, *vos_volumeRestoreType_p;

typedef enum {
    VOS_ONLINE_BUSY,
    VOS_ONLINE_OFFLINE
} vos_volumeOnlineType_t, *vos_volumeOnlineType_p;

typedef enum {
    VOS_DEBUG_MESSAGE = 0x1,
    VOS_ERROR_MESSAGE = 0x2,
    VOS_VERBOSE_MESSAGE = 0x4
} vos_messageType_t, *vos_messageType_p;

typedef void
  (ADMINAPI * vos_MessageCallBack_t) (vos_messageType_t type, char *message);

extern int ADMINAPI vos_BackupVolumeCreate(const void *cellHandle,
					   vos_MessageCallBack_t callBack,
					   unsigned int volumeId,
					   afs_status_p st);

extern int ADMINAPI vos_BackupVolumeCreateMultiple(const void *cellHandle,
						   const void *serverHandle,
						   vos_MessageCallBack_t
						   callBack,
						   const unsigned int
						   *partition,
						   const char *volumePrefix,
						   vos_exclude_t
						   excludePrefix,
						   afs_status_p st);

extern int ADMINAPI vos_PartitionGet(const void *cellHandle,
				     const void *serverHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int partition,
				     vos_partitionEntry_p partitionP,
				     afs_status_p st);

extern int ADMINAPI vos_PartitionGetBegin(const void *cellHandle,
					  const void *serverHandle,
					  vos_MessageCallBack_t callBack,
					  void **iterationIdP,
					  afs_status_p st);

extern int ADMINAPI vos_PartitionGetNext(const void *iterationId,
					 vos_partitionEntry_p partitionP,
					 afs_status_p st);

extern int ADMINAPI vos_PartitionGetDone(const void *iterationId,
					 afs_status_p st);

extern int ADMINAPI vos_ServerOpen(const void *cellHandle,
				   const char *serverName,
				   void **serverHandleP, afs_status_p st);

extern int ADMINAPI vos_ServerClose(const void *serverHandle,
				    afs_status_p st);

extern int ADMINAPI vos_ServerSync(const void *cellHandle,
				   const void *serverHandle,
				   vos_MessageCallBack_t callBack,
				   const unsigned int *partition,
				   afs_status_p st);

extern int ADMINAPI vos_FileServerAddressChange(const void *cellHandle,
						vos_MessageCallBack_t
						callBack, int oldAddress,
						int newAddress,
						afs_status_p st);

extern int ADMINAPI vos_FileServerAddressRemove(const void *cellHandle,
						vos_MessageCallBack_t
						callBack, int serverAddress,
						afs_status_p st);

extern int ADMINAPI vos_FileServerGetBegin(const void *cellHandle,
					   vos_MessageCallBack_t callBack,
					   void **iterationIdP,
					   afs_status_p st);

extern int ADMINAPI vos_FileServerGetNext(void *iterationId,
					  vos_fileServerEntry_p serverEntryP,
					  afs_status_p st);

extern int ADMINAPI vos_FileServerGetDone(void *iterationId, afs_status_p st);

extern int ADMINAPI vos_ServerTransactionStatusGetBegin(const void
							*cellHandle, const void
							*serverHandle,
							vos_MessageCallBack_t
							callBack,
							void **iterationIdP,
							afs_status_p st);

extern int ADMINAPI vos_ServerTransactionStatusGetNext(const void
						       *iterationId,
						       vos_serverTransactionStatus_p
						       serverTransactionStatusP,
						       afs_status_p st);

extern int ADMINAPI vos_ServerTransactionStatusGetDone(const void
						       *iterationId,
						       afs_status_p st);

extern int ADMINAPI vos_VLDBGet(const void *cellHandle,
				vos_MessageCallBack_t callBack,
				const unsigned int *volumeId,
				const char *volumeName,
				vos_vldbEntry_p vldbEntry, afs_status_p st);

extern int ADMINAPI vos_VLDBGetBegin(const void *cellHandle,
				     const void *serverHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int *partition,
				     void **iterationIdP, afs_status_p st);

extern int ADMINAPI vos_VLDBGetNext(const void *iterationId,
				    vos_vldbEntry_p vldbEntry,
				    afs_status_p st);

extern int ADMINAPI vos_VLDBGetDone(const void *iterationId, afs_status_p st);

extern int ADMINAPI vos_VLDBEntryRemove(const void *cellHandle,
					const void *serverHandle,
					vos_MessageCallBack_t callBack,
					const unsigned int *partition,
					unsigned int *volumeId,
					afs_status_p st);

extern int ADMINAPI vos_VLDBUnlock(const void *cellHandle,
				   const void *serverHandle,
				   vos_MessageCallBack_t callBack,
				   const unsigned int *partition,
				   afs_status_p st);

extern int ADMINAPI vos_VLDBEntryLock(const void *cellHandle,
				      vos_MessageCallBack_t callBack,
				      unsigned int volumeId, afs_status_p st);

extern int ADMINAPI vos_VLDBEntryUnlock(const void *cellHandle,
					vos_MessageCallBack_t callBack,
					unsigned int volumeId,
					afs_status_p st);

extern int ADMINAPI vos_VLDBReadOnlySiteCreate(const void *cellHandle,
					       const void *serverHandle,
					       vos_MessageCallBack_t callBack,
					       unsigned int partition,
					       unsigned int volumeId,
					       afs_status_p st);

extern int ADMINAPI vos_VLDBReadOnlySiteDelete(const void *cellHandle,
					       const void *serverHandle,
					       vos_MessageCallBack_t callBack,
					       unsigned int partition,
					       unsigned int volumeId,
					       afs_status_p st);

extern int ADMINAPI vos_VLDBSync(const void *cellHandle,
				 const void *serverHandle,
				 vos_MessageCallBack_t callBack,
				 const unsigned int *partition,
				 vos_force_t force, afs_status_p st);

extern int ADMINAPI vos_VolumeCreate(const void *cellHandle,
				     const void *serverHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int partition,
				     const char *volumeName,
				     unsigned int quota,
				     unsigned int *volumeId, afs_status_p st);

extern int ADMINAPI vos_VolumeDelete(const void *cellHandle,
				     const void *serverHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int partition,
				     unsigned int volumeId, afs_status_p st);

extern int ADMINAPI vos_VolumeRename(const void *cellHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int readWriteVolumeId,
				     const char *newVolumeName,
				     afs_status_p st);

extern int ADMINAPI vos_VolumeDump(const void *cellHandle,
				   const void *serverHandle,
				   vos_MessageCallBack_t callBack,
				   unsigned int *partition,
				   unsigned int volumeId,
				   unsigned int startTime,
				   const char *dumpFile, afs_status_p st);

extern int ADMINAPI vos_VolumeRestore(const void *cellHandle,
				      const void *serverHandle,
				      vos_MessageCallBack_t callBack,
				      unsigned int partition,
				      unsigned int *volumeId,
				      const char *volumeName,
				      const char *dumpFile,
				      vos_volumeRestoreType_t dumpType,
				      afs_status_p st);

extern int ADMINAPI vos_VolumeOnline(const void *serverHandle,
				     vos_MessageCallBack_t callBack,
				     unsigned int partition,
				     unsigned int volumeId,
				     unsigned int sleepTime,
				     vos_volumeOnlineType_t volumeStatus,
				     afs_status_p st);

extern int ADMINAPI vos_VolumeOffline(const void *serverHandle,
				      vos_MessageCallBack_t callBack,
				      unsigned int partition,
				      unsigned int volumeId, afs_status_p st);

extern int ADMINAPI vos_VolumeGet(const void *cellHandle,
				  const void *serverHandle,
				  vos_MessageCallBack_t callBack,
				  unsigned int partition,
				  unsigned int volumeId,
				  vos_volumeEntry_p volumeP, afs_status_p st);

extern int ADMINAPI vos_VolumeGetBegin(const void *cellHandle,
				       const void *serverHandle,
				       vos_MessageCallBack_t callBack,
				       unsigned int partition,
				       void **iterationIdP, afs_status_p st);

extern int ADMINAPI vos_VolumeGetNext(const void *iterationId,
				      vos_volumeEntry_p volumeP,
				      afs_status_p st);

extern int ADMINAPI vos_VolumeGetDone(const void *iterationId,
				      afs_status_p st);

extern int ADMINAPI vos_VolumeMove(const void *cellHandle,
				   vos_MessageCallBack_t callBack,
				   unsigned int volumeId,
				   const void *fromServer,
				   unsigned int fromPartition,
				   const void *toServer,
				   unsigned int toPartition, afs_status_p st);

extern int ADMINAPI vos_VolumeRelease(const void *cellHandle,
				      vos_MessageCallBack_t callBack,
				      unsigned int volumeId,
				      vos_force_t force, afs_status_p st);

extern int ADMINAPI vos_VolumeZap(const void *cellHandle,
				  const void *serverHandle,
				  vos_MessageCallBack_t callBack,
				  unsigned int partition,
				  unsigned int volumeId, vos_force_t force,
				  afs_status_p st);

extern int ADMINAPI vos_PartitionNameToId(const char *partitionName,
					  unsigned int *partitionId,
					  afs_status_p st);

extern int ADMINAPI vos_PartitionIdToName(unsigned int partitionId,
					  char *partitionName,
					  afs_status_p st);

extern int ADMINAPI vos_VolumeQuotaChange(const void *cellHandle,
					  const void *serverHandle,
					  vos_MessageCallBack_t callBack,
					  unsigned int partition,
					  unsigned int volumeId,
					  unsigned int volumeQuota,
					  afs_status_p st);

extern int ADMINAPI vos_VolumeGet2(const void *cellHandle,
				  const void *serverHandle,
				  vos_MessageCallBack_t callBack,
				  unsigned int partition,
				  unsigned int volumeId,
				  volintInfo* pinfo, afs_status_p st);

extern int ADMINAPI vos_ClearVolUpdateCounter(const void *cellHandle,
				  const void *serverHandle,
				  unsigned int partition,
				  unsigned int volumeId,
				  afs_status_p st);				  

#endif /* OPENAFS_VOS_ADMIN_H */
